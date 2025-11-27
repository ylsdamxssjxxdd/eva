#ifndef DOCKER_SANDBOX_H
#define DOCKER_SANDBOX_H

#include <QObject>
#include <QString>
#include <QMetaType>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

struct DockerSandboxStatus
{
    bool enabled = false;
    bool ready = false;
    bool usingExistingContainer = false;
    bool managedContainer = false;
    bool restartedExistingContainer = false;
    QString image;
    QString containerName;
    QString hostWorkdir;
    QString containerWorkdir;
    QString osPretty;
    QString kernelPretty;
    QString lastError;
    QString infoMessage;
};
Q_DECLARE_METATYPE(DockerSandboxStatus)

class DockerSandbox : public QObject
{
    Q_OBJECT
  public:
    enum class TargetType
    {
        Image,
        Container
    };
    Q_ENUM(TargetType)

    struct Config
    {
        bool enabled = false;
        TargetType target = TargetType::Image;
        QString image;
        QString containerName;
        QString hostWorkdir;
    };

    explicit DockerSandbox(QObject *parent = nullptr);

    static QString defaultContainerWorkdir();

    void applyConfig(const Config &config);
    bool prepare(QString *errorMessage);
    bool sandboxEnabled() const;
    QString containerWorkdir() const;
    QString effectiveImage() const;
    QString containerName() const { return status_.containerName; }
    bool recreateContainerWithRequiredMount(QString *errorMessage);

  signals:
    void statusChanged(const DockerSandboxStatus &status);

  private:
    struct ContainerLaunchSpec
    {
        QString name;
        QString image;
        QString workdir;
        QString user;
        QString entrypointProgram;
        QStringList entrypointArgs;
        QStringList cmd;
        QStringList env;
        QStringList binds;
        QStringList extraHosts;
        QStringList portMappings;
        QString networkMode;
        QString restartPolicy;
        QString gpusRequest;
        bool privileged = false;
    };

    struct DockerCommandResult
    {
        int exitCode = -1;
        bool timedOut = false;
        QString stdOut;
        QString stdErr;
        bool success() const { return !timedOut && exitCode == 0; }
    };

    QString dockerExecutable() const;
    QString desiredImage() const;
    QString desiredContainerName() const;
    QString formattedHostPath(const QString &path) const;
    QString normalizedPathPortable(const QString &path) const;

    DockerCommandResult runDocker(const QStringList &args, int timeoutMs = 120000) const;
    bool ensureDockerReachable(QString *errorMessage);
    bool ensureImageReady(const QString &image, QString *errorMessage);
    bool ensureContainer(QString *errorMessage);
    bool ensureImageContainer(QString *errorMessage);
    bool ensureExistingContainer(QString *errorMessage);
    bool containerExists(const QString &name, bool *running, QString *errorMessage);
    bool inspectContainerObject(const QString &name, QJsonObject *object, QString *errorMessage);
    bool extractMountSource(const QJsonArray &mounts, QString *source, QString *errorMessage) const;
    bool ensureExistingMountAligned(const QString &source, QString *errorMessage) const;
    bool createContainer(const QString &image, const QString &name, const QString &hostWorkdir, QString *errorMessage);
    bool startContainer(const QString &name, QString *errorMessage);
    bool removeContainer(const QString &name, QString *errorMessage);
    void stopContainer(const QString &name);
    void fetchMetadata();
    void updateStatusAndNotify();
    bool parseContainerLaunchSpec(const QJsonObject &inspect, ContainerLaunchSpec *spec, QString *errorMessage) const;
    bool runContainerFromSpec(const ContainerLaunchSpec &spec, QString *errorMessage);
    QString containerPathFromBind(const QString &bind) const;

    Config config_;
    DockerSandboxStatus status_;
    QString dockerVersion_;
};

Q_DECLARE_METATYPE(DockerSandbox::Config)

#endif // DOCKER_SANDBOX_H
