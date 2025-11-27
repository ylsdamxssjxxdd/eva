#include "docker_sandbox.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QSet>
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

QStringList jsonArrayToStringList(const QJsonArray &array)
{
    QStringList out;
    for (const QJsonValue &value : array)
    {
        if (value.isString())
        {
            const QString str = value.toString().trimmed();
            if (!str.isEmpty()) out << str;
        }
    }
    return out;
}

QStringList jsonToStringListRecursive(const QJsonValue &value)
{
    if (value.isArray()) return jsonArrayToStringList(value.toArray());
    if (value.isString())
    {
        const QString str = value.toString().trimmed();
        return str.isEmpty() ? QStringList() : QStringList(str);
    }
    return {};
}
} // namespace

DockerSandbox::DockerSandbox(QObject *parent)
    : QObject(parent)
{
    status_.containerWorkdir = DockerSandbox::defaultContainerWorkdir();
    status_.skillsMountPoint = DockerSandbox::skillsMountPoint();
}

QString DockerSandbox::defaultContainerWorkdir()
{
    return QStringLiteral("/eva_workspace");
}

QString DockerSandbox::skillsMountPoint()
{
    return QStringLiteral("/eva_skills");
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
    const QString skillsRoot = trimAndCollapse(normalized.hostSkillsDir);
    normalized.hostSkillsDir = skillsRoot.isEmpty() ? QString() : QDir::cleanPath(skillsRoot);
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
    status_.hostSkillsDir = config_.hostSkillsDir;
    status_.skillsMountPoint = DockerSandbox::skillsMountPoint();
    status_.missingMountTarget.clear();

    if (status_.managedContainer)
        status_.containerName = config_.enabled ? desiredContainerName() : QString();
    else
        status_.containerName = config_.enabled ? config_.containerName : QString();

    if (!config_.enabled)
    {
        if (!previousContainer.isEmpty())
        {
            stopContainer(previousContainer);
            if (previouslyManaged)
            {
                QString removeError;
                removeContainer(previousContainer, &removeError);
            }
        }
        status_.ready = false;
        status_.lastError.clear();
        status_.osPretty.clear();
        status_.kernelPretty.clear();
        updateStatusAndNotify();
        return;
    }

    if (!previousContainer.isEmpty() && previousContainer != status_.containerName)
    {
        stopContainer(previousContainer);
        if (previouslyManaged)
        {
            QString removeError;
            removeContainer(previousContainer, &removeError);
        }
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

bool DockerSandbox::extractMountSource(const QJsonArray &mounts, const QString &target, QString *source, QString *errorMessage) const
{
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

bool DockerSandbox::ensureExistingMountAligned(const QString &source, const QString &expectedHost, const QString &target, QString *errorMessage) const
{
    const QString normalizedSource = normalizedPathPortable(source);
    const QString normalizedHost = normalizedPathPortable(expectedHost);
    if (normalizedSource.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Container mount source for %1 is empty").arg(target);
        }
        return false;
    }
    if (normalizedHost.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("Host directory for %1 is not configured.").arg(target);
        }
        return false;
    }
    if (normalizedSource != normalizedHost)
    {
        if (errorMessage)
        {
            if (target == DockerSandbox::defaultContainerWorkdir())
            {
                *errorMessage = QStringLiteral("Container %1 mounts %2, but EVA workspace is %3.")
                                    .arg(config_.containerName,
                                         QDir::toNativeSeparators(source),
                                         QDir::toNativeSeparators(expectedHost));
            }
            else if (target == DockerSandbox::skillsMountPoint())
            {
                *errorMessage = QStringLiteral("Container %1 mounts %2, but EVA skills directory is %3.")
                                    .arg(config_.containerName,
                                         QDir::toNativeSeparators(source),
                                         QDir::toNativeSeparators(expectedHost));
            }
            else
            {
                *errorMessage = QStringLiteral("Container %1 mounts %2, but host directory for %3 is %4.")
                                    .arg(config_.containerName,
                                         QDir::toNativeSeparators(source),
                                         target,
                                         QDir::toNativeSeparators(expectedHost));
            }
        }
        return false;
    }
    if (errorMessage) errorMessage->clear();
    return true;
}

bool DockerSandbox::createContainer(const QString &image,
                                    const QString &name,
                                    const QString &hostWorkdir,
                                    const QString &hostSkillsDir,
                                    QString *errorMessage)
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
    QString skillsHost = hostSkillsDir;
    QString skillsMount;
    if (!skillsHost.isEmpty())
    {
        QDir skillsDir(skillsHost);
        if (!skillsDir.exists())
        {
            skillsDir.mkpath(QStringLiteral("."));
        }
        skillsMount = QStringLiteral("%1:%2").arg(formattedHostPath(skillsDir.absolutePath()), DockerSandbox::skillsMountPoint());
    }
    QStringList args;
    args << QStringLiteral("run")
         << QStringLiteral("-d")
         << QStringLiteral("--name")
         << name
         << QStringLiteral("-w")
         << containerWorkdir()
         << QStringLiteral("-v")
         << mount;
    if (!skillsMount.isEmpty())
    {
        args << QStringLiteral("-v") << skillsMount;
    }
    args << image
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
    status_.missingMountTarget.clear();
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
        if (!createContainer(desiredImage(), status_.containerName, config_.hostWorkdir, config_.hostSkillsDir, errorMessage))
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
    const QJsonArray mountsArray = inspect.value(QStringLiteral("Mounts")).toArray();
    const QString workTarget = DockerSandbox::defaultContainerWorkdir();
    if (!extractMountSource(mountsArray, workTarget, &mountSource, errorMessage))
    {
        status_.missingMountTarget = workTarget;
        return false;
    }
    if (!ensureExistingMountAligned(mountSource, config_.hostWorkdir, workTarget, errorMessage)) return false;
    if (!config_.hostSkillsDir.isEmpty())
    {
        QString skillsSource;
        const QString skillsTarget = DockerSandbox::skillsMountPoint();
        if (!extractMountSource(mountsArray, skillsTarget, &skillsSource, errorMessage))
        {
            status_.missingMountTarget = skillsTarget;
            return false;
        }
        if (!ensureExistingMountAligned(skillsSource, config_.hostSkillsDir, skillsTarget, errorMessage)) return false;
    }

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

    QJsonObject inspect;
    if (!inspectContainerObject(config_.containerName, &inspect, errorMessage)) return false;

    ContainerLaunchSpec spec;
    if (!parseContainerLaunchSpec(inspect, &spec, errorMessage)) return false;

    if (spec.image.isEmpty())
    {
        spec.image = status_.image.isEmpty() ? fallbackImage() : status_.image;
    }
    spec.name = config_.containerName;

    QDir hostDir(config_.hostWorkdir);
    if (!hostDir.exists()) hostDir.mkpath(QStringLiteral("."));
    const QString hostPath = formattedHostPath(hostDir.absolutePath());
    QString skillsBind;
    if (!config_.hostSkillsDir.isEmpty())
    {
        QDir skillsDir(config_.hostSkillsDir);
        if (!skillsDir.exists()) skillsDir.mkpath(QStringLiteral("."));
        const QString hostSkillsPath = formattedHostPath(skillsDir.absolutePath());
        skillsBind = QStringLiteral("%1:%2:%3")
                         .arg(hostSkillsPath, DockerSandbox::skillsMountPoint(), QStringLiteral("rw"));
    }
    QStringList filteredBinds;
    for (const QString &bind : std::as_const(spec.binds))
    {
        const QString destination = containerPathFromBind(bind);
        if (destination == DockerSandbox::defaultContainerWorkdir()) continue;
        if (!skillsBind.isEmpty() && destination == DockerSandbox::skillsMountPoint()) continue;
        filteredBinds << bind;
    }
    QString evaBind = QStringLiteral("%1:%2:%3")
                          .arg(hostPath,
                               DockerSandbox::defaultContainerWorkdir(),
                               QStringLiteral("rw"));
    filteredBinds << evaBind;
    if (!skillsBind.isEmpty()) filteredBinds << skillsBind;
    spec.binds = filteredBinds;

    if (!removeContainer(config_.containerName, errorMessage)) return false;
    if (!runContainerFromSpec(spec, errorMessage)) return false;

    status_.image = spec.image;
    status_.lastError.clear();
    if (skillsBind.isEmpty())
    {
        status_.infoMessage = QStringLiteral("docker sandbox recreated %1 with mount %2")
                                  .arg(config_.containerName, DockerSandbox::defaultContainerWorkdir());
    }
    else
    {
        status_.infoMessage = QStringLiteral("docker sandbox recreated %1 with mounts %2 and %3")
                                  .arg(config_.containerName,
                                       DockerSandbox::defaultContainerWorkdir(),
                                       DockerSandbox::skillsMountPoint());
    }
    return true;
}

QString DockerSandbox::containerPathFromBind(const QString &bind) const
{
    const QString trimmed = bind.trimmed();
    const int firstColon = trimmed.indexOf(QLatin1Char(':'));
    if (firstColon == -1) return QString();
    const QString rest = trimmed.mid(firstColon + 1);
    const int secondColon = rest.indexOf(QLatin1Char(':'));
    if (secondColon == -1) return rest;
    return rest.left(secondColon);
}

bool DockerSandbox::parseContainerLaunchSpec(const QJsonObject &inspect, ContainerLaunchSpec *spec, QString *errorMessage) const
{
    if (!spec)
    {
        if (errorMessage) *errorMessage = QStringLiteral("Invalid container spec pointer.");
        return false;
    }
    ContainerLaunchSpec parsed;
    QString name = inspect.value(QStringLiteral("Name")).toString().trimmed();
    if (name.startsWith(QLatin1Char('/'))) name.remove(0, 1);
    parsed.name = name;

    const QJsonObject configObj = inspect.value(QStringLiteral("Config")).toObject();
    parsed.image = configObj.value(QStringLiteral("Image")).toString().trimmed();
    parsed.workdir = configObj.value(QStringLiteral("WorkingDir")).toString().trimmed();
    parsed.user = configObj.value(QStringLiteral("User")).toString().trimmed();
    parsed.env = jsonArrayToStringList(configObj.value(QStringLiteral("Env")).toArray());

    QStringList entrypointParts = jsonArrayToStringList(configObj.value(QStringLiteral("Entrypoint")).toArray());
    if (!entrypointParts.isEmpty())
    {
        parsed.entrypointProgram = entrypointParts.takeFirst();
        parsed.entrypointArgs = entrypointParts;
    }
    parsed.cmd = jsonArrayToStringList(configObj.value(QStringLiteral("Cmd")).toArray());

    const QJsonArray mountsArray = inspect.value(QStringLiteral("Mounts")).toArray();
    QSet<QString> seenBinds;
    for (const QJsonValue &mountValue : mountsArray)
    {
        const QJsonObject mountObj = mountValue.toObject();
        QString destination = mountObj.value(QStringLiteral("Destination")).toString().trimmed();
        QString type = mountObj.value(QStringLiteral("Type")).toString().trimmed();
        if (destination.isEmpty()) continue;
        QString source;
        if (type == QStringLiteral("bind"))
        {
            source = mountObj.value(QStringLiteral("Source")).toString().trimmed();
        }
        else if (type == QStringLiteral("volume"))
        {
            source = mountObj.value(QStringLiteral("Name")).toString().trimmed();
            if (source.isEmpty()) source = mountObj.value(QStringLiteral("Source")).toString().trimmed();
        }
        if (source.isEmpty()) continue;
        QString mode = mountObj.value(QStringLiteral("Mode")).toString().trimmed();
        if (mode.isEmpty())
        {
            const bool rw = mountObj.value(QStringLiteral("RW")).toBool(true);
            mode = rw ? QStringLiteral("rw") : QStringLiteral("ro");
        }
        QString bind = QStringLiteral("%1:%2").arg(source, destination);
        if (!mode.isEmpty()) bind += QStringLiteral(":%1").arg(mode);
        if (seenBinds.contains(bind)) continue;
        parsed.binds << bind;
        seenBinds.insert(bind);
    }

    const QJsonObject hostConfig = inspect.value(QStringLiteral("HostConfig")).toObject();
    parsed.networkMode = hostConfig.value(QStringLiteral("NetworkMode")).toString().trimmed();
    parsed.privileged = hostConfig.value(QStringLiteral("Privileged")).toBool(false);
    const QJsonObject restartObj = hostConfig.value(QStringLiteral("RestartPolicy")).toObject();
    parsed.restartPolicy = restartObj.value(QStringLiteral("Name")).toString().trimmed();

    const QJsonObject portBindings = hostConfig.value(QStringLiteral("PortBindings")).toObject();
    for (auto it = portBindings.begin(); it != portBindings.end(); ++it)
    {
        const QString key = it.key().trimmed(); // e.g. 8001/tcp
        if (key.isEmpty()) continue;
        QString containerPort = key;
        const QStringList parts = key.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (!parts.isEmpty())
        {
            containerPort = parts.at(0);
        }
        QString proto = parts.size() > 1 ? parts.at(1) : QStringLiteral("tcp");
        proto = proto.trimmed().toLower();
        QString containerPortToken = containerPort.trimmed();
        if (proto != QStringLiteral("tcp") && !proto.isEmpty())
        {
            containerPortToken += QStringLiteral("/%1").arg(proto);
        }

        const QJsonArray mappingArray = it.value().toArray();
        for (const QJsonValue &mappingValue : mappingArray)
        {
            const QJsonObject mapping = mappingValue.toObject();
            QString hostPort = mapping.value(QStringLiteral("HostPort")).toString().trimmed();
            if (hostPort.isEmpty()) continue;
            QString hostIp = mapping.value(QStringLiteral("HostIp")).toString().trimmed();
            QString mapString;
            if (!hostIp.isEmpty())
            {
                mapString = QStringLiteral("%1:%2:%3").arg(hostIp, hostPort, containerPortToken);
            }
            else
            {
                mapString = QStringLiteral("%1:%2").arg(hostPort, containerPortToken);
            }
            parsed.portMappings << mapString;
        }
    }

    const QJsonArray extraHostsArray = hostConfig.value(QStringLiteral("ExtraHosts")).toArray();
    parsed.extraHosts = jsonArrayToStringList(extraHostsArray);

    const QJsonArray deviceRequests = hostConfig.value(QStringLiteral("DeviceRequests")).toArray();
    for (const QJsonValue &requestValue : deviceRequests)
    {
        const QJsonObject request = requestValue.toObject();
        const QJsonArray capabilitiesArray = request.value(QStringLiteral("Capabilities")).toArray();
        for (const QJsonValue &capVal : capabilitiesArray)
        {
            const QStringList caps = jsonArrayToStringList(capVal.toArray());
            if (caps.contains(QStringLiteral("gpu"), Qt::CaseInsensitive))
            {
                QString gpus;
                const QStringList deviceIds = jsonArrayToStringList(request.value(QStringLiteral("DeviceIDs")).toArray());
                if (!deviceIds.isEmpty())
                {
                    gpus = QStringLiteral("device=%1").arg(deviceIds.join(QLatin1Char(',')));
                }
                else
                {
                    const int count = request.value(QStringLiteral("Count")).toInt(-1);
                    if (count <= 0)
                        gpus = QStringLiteral("all");
                    else
                        gpus = QString::number(count);
                }
                parsed.gpusRequest = gpus;
                break;
            }
        }
        if (!parsed.gpusRequest.isEmpty()) break;
    }

    *spec = parsed;
    if (errorMessage) errorMessage->clear();
    return true;
}

bool DockerSandbox::runContainerFromSpec(const ContainerLaunchSpec &spec, QString *errorMessage)
{
    if (spec.image.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Container image is empty.");
        return false;
    }
    if (spec.name.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("Container name is empty.");
        return false;
    }

    QStringList args;
    args << QStringLiteral("run") << QStringLiteral("-d") << QStringLiteral("--name") << spec.name;
    if (spec.privileged) args << QStringLiteral("--privileged");
    if (!spec.networkMode.isEmpty() && spec.networkMode != QStringLiteral("default"))
    {
        args << QStringLiteral("--network") << spec.networkMode;
    }
    if (!spec.restartPolicy.isEmpty() && spec.restartPolicy != QStringLiteral("no"))
    {
        args << QStringLiteral("--restart") << spec.restartPolicy;
    }
    if (!spec.gpusRequest.isEmpty())
    {
        args << QStringLiteral("--gpus") << spec.gpusRequest;
    }
    if (!spec.user.isEmpty()) args << QStringLiteral("-u") << spec.user;
    if (!spec.workdir.isEmpty()) args << QStringLiteral("-w") << spec.workdir;

    for (const QString &env : spec.env)
    {
        if (env.isEmpty()) continue;
        args << QStringLiteral("-e") << env;
    }
    for (const QString &bind : spec.binds)
    {
        if (bind.isEmpty()) continue;
        args << QStringLiteral("-v") << bind;
    }
    for (const QString &mapping : spec.portMappings)
    {
        if (mapping.isEmpty()) continue;
        args << QStringLiteral("-p") << mapping;
    }
    for (const QString &host : spec.extraHosts)
    {
        if (host.isEmpty()) continue;
        args << QStringLiteral("--add-host") << host;
    }
    QStringList finalCmd = spec.entrypointArgs;
    finalCmd.append(spec.cmd);
    if (!spec.entrypointProgram.isEmpty())
    {
        args << QStringLiteral("--entrypoint") << spec.entrypointProgram;
    }
    args << spec.image;
    for (const QString &cmdArg : finalCmd)
    {
        if (cmdArg.isEmpty()) continue;
        args << cmdArg;
    }

    const DockerCommandResult result = runDocker(args, 120000);
    if (!result.success())
    {
        if (errorMessage)
        {
            QString err = trimAndCollapse(result.stdErr);
            if (err.isEmpty()) err = QStringLiteral("Failed to recreate docker container %1").arg(spec.name);
            *errorMessage = err;
        }
        return false;
    }
    if (errorMessage) errorMessage->clear();
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
