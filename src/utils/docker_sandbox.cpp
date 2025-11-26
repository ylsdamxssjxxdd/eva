#include "docker_sandbox.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

namespace
{
QString trimAndCollapse(const QString &text)
{
    QString trimmed = text.trimmed();
    trimmed.replace(QStringLiteral("\r"), QString());
    return trimmed;
}

QString defaultContainerWorkdir()
{
    return QStringLiteral("/workspace");
}

QString fallbackImage()
{
    return QStringLiteral("ubuntu:latest");
}

QString constantPrefix()
{
    return QStringLiteral("eva-sandbox-");
}
} // namespace

DockerSandbox::DockerSandbox(QObject *parent)
    : QObject(parent)
{
    status_.containerWorkdir = defaultContainerWorkdir();
}

void DockerSandbox::applyConfig(const Config &config)
{
    Config normalized = config;
    normalized.image = trimAndCollapse(normalized.image);
    const QString hostRoot = trimAndCollapse(normalized.hostWorkdir);
    normalized.hostWorkdir = hostRoot.isEmpty() ? QString() : QDir::cleanPath(hostRoot);
    config_ = normalized;

    if (config_.image.isEmpty())
    {
        config_.image = fallbackImage();
    }

    status_.enabled = config_.enabled;
    status_.image = config_.image;
    status_.hostWorkdir = config_.hostWorkdir;
    status_.containerWorkdir = defaultContainerWorkdir();

    const QString newName = config_.enabled ? desiredContainerName() : QString();
    if (status_.containerName != newName && !status_.containerName.isEmpty())
    {
        stopContainer(status_.containerName);
    }
    status_.containerName = newName;

    if (!config_.enabled)
    {
        status_.ready = false;
        status_.lastError.clear();
        status_.osPretty.clear();
        status_.kernelPretty.clear();
        updateStatusAndNotify();
        return;
    }

    QString error;
    if (!ensureContainer(&error))
    {
        status_.ready = false;
        status_.lastError = error;
        updateStatusAndNotify();
        return;
    }

    status_.ready = true;
    status_.lastError.clear();
    fetchMetadata();
    updateStatusAndNotify();
}

bool DockerSandbox::prepare(QString *errorMessage)
{
    if (!config_.enabled)
    {
        if (errorMessage) *errorMessage = QStringLiteral("Docker sandbox not enabled.");
        return false;
    }
    if (status_.ready) return true;
    QString localError;
    if (!ensureContainer(&localError))
    {
        if (errorMessage) *errorMessage = localError;
        return false;
    }
    status_.ready = true;
    fetchMetadata();
    updateStatusAndNotify();
    return true;
}

bool DockerSandbox::sandboxEnabled() const
{
    return config_.enabled && !status_.containerName.isEmpty();
}

QString DockerSandbox::containerWorkdir() const
{
    return status_.containerWorkdir.isEmpty() ? defaultContainerWorkdir() : status_.containerWorkdir;
}

QString DockerSandbox::effectiveImage() const
{
    return status_.image.isEmpty() ? fallbackImage() : status_.image;
}

QString DockerSandbox::dockerExecutable() const
{
#ifdef Q_OS_WIN
    return QStringLiteral("docker.exe");
#else
    return QStringLiteral("docker");
#endif
}

QString DockerSandbox::desiredImage() const
{
    return config_.image.isEmpty() ? fallbackImage() : config_.image;
}

QString DockerSandbox::desiredContainerName() const
{
    if (config_.hostWorkdir.isEmpty() && config_.image.isEmpty())
    {
        return QString();
    }
    const QString key = config_.hostWorkdir + QLatin1Char('|') + config_.image;
    quint32 hash = qHash(key);
    return constantPrefix() + QString::number(hash, 16);
}

QString DockerSandbox::formattedHostPath(const QString &path) const
{
#ifdef Q_OS_WIN
    return QDir::toNativeSeparators(path);
#else
    return path;
#endif
}

DockerSandbox::DockerCommandResult DockerSandbox::runDocker(const QStringList &args, int timeoutMs) const
{
    DockerCommandResult result;
    QProcess process;
    process.setProgram(dockerExecutable());
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForFinished(timeoutMs))
    {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(3000);
    }
    result.exitCode = process.exitCode();
    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    return result;
}

bool DockerSandbox::ensureDockerReachable(QString *errorMessage)
{
    if (!dockerVersion_.isEmpty()) return true;
    const DockerCommandResult result = runDocker(QStringList() << QStringLiteral("version") << QStringLiteral("--format") << QStringLiteral("{{.Server.Version}}"), 15000);
    if (!result.success())
    {
        if (errorMessage)
        {
            const QString err = result.timedOut ? QStringLiteral("docker version timed out") : trimAndCollapse(result.stdErr);
            *errorMessage = err.isEmpty() ? QStringLiteral("Unable to execute docker. Please ensure Docker Desktop/daemon is running.") : err;
        }
        return false;
    }
    dockerVersion_ = trimAndCollapse(result.stdOut);
    return true;
}

bool DockerSandbox::ensureImageReady(const QString &image, QString *errorMessage)
{
    const DockerCommandResult inspect = runDocker(QStringList() << QStringLiteral("image") << QStringLiteral("inspect") << image, 20000);
    if (inspect.success()) return true;
    const DockerCommandResult pull = runDocker(QStringList() << QStringLiteral("pull") << image, 300000);
    if (!pull.success())
    {
        if (errorMessage)
        {
            QString err = trimAndCollapse(pull.stdErr);
            if (err.isEmpty()) err = QStringLiteral("docker pull failed for %1").arg(image);
            *errorMessage = err;
        }
        return false;
    }
    return true;
}

bool DockerSandbox::containerExists(const QString &name, bool *running, QString *errorMessage)
{
    if (name.isEmpty()) return false;
    const DockerCommandResult inspect = runDocker(QStringList() << QStringLiteral("inspect") << QStringLiteral("-f") << QStringLiteral("{{.State.Status}}") << name, 15000);
    if (!inspect.success())
    {
        const QString stderrText = trimAndCollapse(inspect.stdErr);
        if (stderrText.contains(QStringLiteral("No such object")))
        {
            if (running) *running = false;
            if (errorMessage) errorMessage->clear();
            return false;
        }
        if (errorMessage) *errorMessage = stderrText.isEmpty() ? QStringLiteral("docker inspect failed for %1").arg(name) : stderrText;
        return false;
    }
    const QString status = trimAndCollapse(inspect.stdOut);
    if (running) *running = (status == QStringLiteral("running"));
    if (errorMessage) errorMessage->clear();
    return true;
}

bool DockerSandbox::createContainer(const QString &image, const QString &name, const QString &hostWorkdir, QString *errorMessage)
{
    QString host = hostWorkdir;
    if (host.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Host workspace is empty.");
        return false;
    }
    QDir dir(host);
    if (!dir.exists())
    {
        dir.mkpath(QStringLiteral("."));
    }
    const QString mount = QStringLiteral("%1:%2").arg(formattedHostPath(dir.absolutePath()), containerWorkdir());
    QStringList args;
    args << QStringLiteral("run")
         << QStringLiteral("-d")
         << QStringLiteral("--name")
         << name
         << QStringLiteral("-w")
         << containerWorkdir()
         << QStringLiteral("-v")
         << mount
         << image
         << QStringLiteral("/bin/sh")
         << QStringLiteral("-c")
         << QStringLiteral("while true; do sleep 3600; done");
    const DockerCommandResult result = runDocker(args, 60000);
    if (!result.success())
    {
        if (errorMessage)
        {
            QString err = trimAndCollapse(result.stdErr);
            if (err.isEmpty()) err = QStringLiteral("Failed to create docker sandbox container.");
            *errorMessage = err;
        }
        return false;
    }
    return true;
}

bool DockerSandbox::startContainer(const QString &name, QString *errorMessage)
{
    const DockerCommandResult result = runDocker(QStringList() << QStringLiteral("start") << name, 20000);
    if (!result.success())
    {
        if (errorMessage)
        {
            QString err = trimAndCollapse(result.stdErr);
            if (err.isEmpty()) err = QStringLiteral("docker start failed for %1").arg(name);
            *errorMessage = err;
        }
        return false;
    }
    return true;
}

void DockerSandbox::stopContainer(const QString &name)
{
    if (name.isEmpty()) return;
    runDocker(QStringList() << QStringLiteral("stop") << name, 20000);
}

bool DockerSandbox::ensureContainer(QString *errorMessage)
{
    if (!ensureDockerReachable(errorMessage)) return false;
    if (!ensureImageReady(desiredImage(), errorMessage)) return false;
    bool running = false;
    QString inspectError;
    const bool existed = containerExists(status_.containerName, &running, &inspectError);
    if (!existed && !inspectError.isEmpty())
    {
        if (errorMessage) *errorMessage = inspectError;
        return false;
    }
    if (!existed)
    {
        if (!createContainer(desiredImage(), status_.containerName, config_.hostWorkdir, errorMessage))
        {
            return false;
        }
        running = true;
    }
    else if (!running)
    {
        if (!startContainer(status_.containerName, errorMessage))
        {
            return false;
        }
        running = true;
    }
    status_.ready = running;
    if (!status_.ready)
    {
        if (errorMessage) *errorMessage = QStringLiteral("Docker sandbox container is not running.");
        return false;
    }
    return true;
}

void DockerSandbox::fetchMetadata()
{
    if (!status_.ready || status_.containerName.isEmpty()) return;
    const QString script = QStringLiteral("uname -sr && (cat /etc/os-release 2>/dev/null | head -n 1)");
    const DockerCommandResult exec = runDocker(QStringList()
                                               << QStringLiteral("exec")
                                               << QStringLiteral("-i")
                                               << status_.containerName
                                               << QStringLiteral("/bin/sh")
                                               << QStringLiteral("-c")
                                               << script,
                                               20000);
    if (!exec.success()) return;
    const QStringList lines = exec.stdOut.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!lines.isEmpty())
    {
        status_.kernelPretty = lines.first().trimmed();
    }
    if (lines.size() > 1)
    {
        status_.osPretty = lines.at(1).trimmed();
    }
}

void DockerSandbox::updateStatusAndNotify()
{
    emit statusChanged(status_);
}
