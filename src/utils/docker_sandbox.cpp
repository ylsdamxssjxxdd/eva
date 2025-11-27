#include "docker_sandbox.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    status_.containerWorkdir = DockerSandbox::defaultContainerWorkdir();
}

QString DockerSandbox::defaultContainerWorkdir()
{
    return QStringLiteral("/eva_workspace");
}

void DockerSandbox::applyConfig(const Config &config)
{
    const QString previousContainer = status_.containerName;
    const bool previouslyManaged = status_.managedContainer;
    Config normalized = config;
    normalized.image = trimAndCollapse(normalized.image);
    normalized.containerName = trimAndCollapse(normalized.containerName);
    const QString hostRoot = trimAndCollapse(normalized.hostWorkdir);
    normalized.hostWorkdir = hostRoot.isEmpty() ? QString() : QDir::cleanPath(hostRoot);
    config_ = normalized;

    if (config_.target == TargetType::Image && config_.image.isEmpty())
    {
        config_.image = fallbackImage();
    }

    status_.enabled = config_.enabled;
    status_.managedContainer = (config_.target == TargetType::Image);
    status_.usingExistingContainer = (config_.target == TargetType::Container);
    status_.restartedExistingContainer = false;
    status_.infoMessage.clear();
    status_.image = status_.managedContainer ? config_.image : QString();
    status_.hostWorkdir = config_.hostWorkdir;
    status_.containerWorkdir = DockerSandbox::defaultContainerWorkdir();

    if (status_.managedContainer)
        status_.containerName = config_.enabled ? desiredContainerName() : QString();
    else
        status_.containerName = config_.enabled ? config_.containerName : QString();

    if (!config_.enabled)
    {
        if (previouslyManaged && !previousContainer.isEmpty())
        {
            stopContainer(previousContainer);
        }
        status_.ready = false;
        status_.lastError.clear();
        status_.osPretty.clear();
        status_.kernelPretty.clear();
        updateStatusAndNotify();
        return;
    }

    if (previouslyManaged && !previousContainer.isEmpty() && previousContainer != status_.containerName)
    {
        stopContainer(previousContainer);
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
    return status_.containerWorkdir.isEmpty() ? DockerSandbox::defaultContainerWorkdir() : status_.containerWorkdir;
}

QString DockerSandbox::effectiveImage() const
{
    if (!status_.image.isEmpty()) return status_.image;
    if (!status_.containerName.isEmpty()) return status_.containerName;
    return fallbackImage();
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

QString DockerSandbox::normalizedPathPortable(const QString &path) const
{
    if (path.isEmpty()) return QString();
    QString normalized = QDir::fromNativeSeparators(path);
    normalized = QDir::cleanPath(normalized);
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
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

bool DockerSandbox::inspectContainerObject(const QString &name, QJsonObject *object, QString *errorMessage)
{
    if (name.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Container name is empty.");
        return false;
    }
    const DockerCommandResult inspect = runDocker(QStringList() << QStringLiteral("inspect") << name, 20000);
    if (!inspect.success())
    {
        QString err = trimAndCollapse(inspect.stdErr);
        if (err.contains(QStringLiteral("No such object")))
        {
            if (errorMessage) *errorMessage = QStringLiteral("Container %1 not found.").arg(name);
        }
        else if (errorMessage)
        {
            if (err.isEmpty()) err = QStringLiteral("docker inspect failed for %1").arg(name);
            *errorMessage = err;
        }
        return false;
    }
    const QByteArray jsonBytes = inspect.stdOut.toUtf8();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray() || doc.array().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Failed to parse docker inspect output for %1").arg(name);
        }
        return false;
    }
    const QJsonValue first = doc.array().first();
    if (!first.isObject())
    {
        if (errorMessage) *errorMessage = QStringLiteral("docker inspect did not return an object for %1").arg(name);
        return false;
    }
    if (object) *object = first.toObject();
    if (errorMessage) errorMessage->clear();
    return true;
}

bool DockerSandbox::extractMountSource(const QJsonArray &mounts, QString *source, QString *errorMessage) const
{
    const QString target = DockerSandbox::defaultContainerWorkdir();
    for (const QJsonValue &item : mounts)
    {
        if (!item.isObject()) continue;
        const QJsonObject obj = item.toObject();
        const QString destination = obj.value(QStringLiteral("Destination")).toString().trimmed();
        if (destination == target)
        {
            if (source) *source = obj.value(QStringLiteral("Source")).toString().trimmed();
            if (errorMessage) errorMessage->clear();
            return true;
        }
    }
    if (errorMessage)
    {
        *errorMessage = QStringLiteral("Container %1 is missing a bind mount to %2").arg(config_.containerName, target);
    }
    return false;
}

bool DockerSandbox::ensureExistingMountAligned(const QString &source, QString *errorMessage) const
{
    const QString normalizedSource = normalizedPathPortable(source);
    const QString normalizedHost = normalizedPathPortable(config_.hostWorkdir);
    if (normalizedSource.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Container mount source for %1 is empty").arg(DockerSandbox::defaultContainerWorkdir());
        return false;
    }
    if (normalizedHost.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Host workspace is not configured.");
        return false;
    }
    if (normalizedSource != normalizedHost)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Container %1 mounts %2, but EVA workspace is %3.")
                                .arg(config_.containerName,
                                     QDir::toNativeSeparators(source),
                                     QDir::toNativeSeparators(config_.hostWorkdir));
        }
        return false;
    }
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
    const QString mount = QStringLiteral("%1:%2").arg(formattedHostPath(dir.absolutePath()), DockerSandbox::defaultContainerWorkdir());
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

bool DockerSandbox::removeContainer(const QString &name, QString *errorMessage)
{
    if (name.isEmpty()) return true;
    const DockerCommandResult result = runDocker(QStringList() << QStringLiteral("rm") << QStringLiteral("-f") << name, 20000);
    if (!result.success())
    {
        if (errorMessage)
        {
            QString err = trimAndCollapse(result.stdErr);
            if (err.isEmpty()) err = QStringLiteral("docker rm failed for %1").arg(name);
            *errorMessage = err;
        }
        return false;
    }
    if (errorMessage) errorMessage->clear();
    return true;
}

bool DockerSandbox::ensureContainer(QString *errorMessage)
{
    if (config_.target == TargetType::Container)
    {
        return ensureExistingContainer(errorMessage);
    }
    return ensureImageContainer(errorMessage);
}

bool DockerSandbox::ensureImageContainer(QString *errorMessage)
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

bool DockerSandbox::ensureExistingContainer(QString *errorMessage)
{
    if (config_.containerName.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Container name is empty.");
        return false;
    }
    if (config_.hostWorkdir.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Host workspace is empty.");
        return false;
    }
    if (!ensureDockerReachable(errorMessage)) return false;

    QJsonObject inspect;
    if (!inspectContainerObject(config_.containerName, &inspect, errorMessage)) return false;

    const QJsonObject configObj = inspect.value(QStringLiteral("Config")).toObject();
    const QString imageName = configObj.value(QStringLiteral("Image")).toString().trimmed();
    if (!imageName.isEmpty()) status_.image = imageName;

    const QJsonObject stateObj = inspect.value(QStringLiteral("State")).toObject();
    QString stateStatus = stateObj.value(QStringLiteral("Status")).toString();
    stateStatus = stateStatus.trimmed().toLower();

    QString mountSource;
    if (!extractMountSource(inspect.value(QStringLiteral("Mounts")).toArray(), &mountSource, errorMessage))
    {
        return false;
    }
    if (!ensureExistingMountAligned(mountSource, errorMessage)) return false;

    const bool wasRunning = (stateStatus == QStringLiteral("running"));
    if (wasRunning)
    {
        status_.restartedExistingContainer = true;
        status_.infoMessage = QStringLiteral("Container %1 is running; restarting to refresh %2.")
                                  .arg(config_.containerName, DockerSandbox::defaultContainerWorkdir());
        const DockerCommandResult stopRes = runDocker(QStringList() << QStringLiteral("stop") << config_.containerName, 20000);
        if (!stopRes.success())
        {
            if (errorMessage)
            {
                QString err = trimAndCollapse(stopRes.stdErr);
                if (err.isEmpty()) err = QStringLiteral("docker stop failed for %1").arg(config_.containerName);
                *errorMessage = err;
            }
            return false;
        }
    }
    if (!startContainer(config_.containerName, errorMessage)) return false;
    status_.ready = true;
    status_.lastError.clear();
    return true;
}

bool DockerSandbox::recreateContainerWithRequiredMount(QString *errorMessage)
{
    if (config_.target != TargetType::Container)
    {
        if (errorMessage) *errorMessage = QStringLiteral("docker sandbox is not in container mode.");
        return false;
    }
    if (config_.containerName.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Container name is empty.");
        return false;
    }
    if (config_.hostWorkdir.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Host workspace is empty.");
        return false;
    }

    QString image = status_.image;
    if (image.isEmpty())
    {
        QJsonObject inspect;
        if (!inspectContainerObject(config_.containerName, &inspect, errorMessage)) return false;
        image = inspect.value(QStringLiteral("Config")).toObject().value(QStringLiteral("Image")).toString().trimmed();
        if (image.isEmpty()) image = fallbackImage();
    }

    if (!removeContainer(config_.containerName, errorMessage)) return false;
    if (!createContainer(image, config_.containerName, config_.hostWorkdir, errorMessage)) return false;

    status_.image = image;
    status_.lastError.clear();
    status_.infoMessage = QStringLiteral("docker sandbox recreated %1 with mount %2")
                              .arg(config_.containerName, DockerSandbox::defaultContainerWorkdir());
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
