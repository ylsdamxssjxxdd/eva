#ifndef DOCKER_SANDBOX_H
#define DOCKER_SANDBOX_H

#include <QObject>
#include <QString>
#include <QMetaType>

struct DockerSandboxStatus
{
    bool enabled = false;
    bool ready = false;
    QString image;
    QString containerName;
    QString hostWorkdir;
    QString containerWorkdir;
    QString osPretty;
    QString kernelPretty;
    QString lastError;
};
Q_DECLARE_METATYPE(DockerSandboxStatus)

class DockerSandbox : public QObject
{
    Q_OBJECT
  public:
    struct Config
    {
        bool enabled = false;
        QString image;
        QString hostWorkdir;
    };

    explicit DockerSandbox(QObject *parent = nullptr);

    void applyConfig(const Config &config);
    bool prepare(QString *errorMessage);
    bool sandboxEnabled() const;
    QString containerWorkdir() const;
    QString effectiveImage() const;
    QString containerName() const { return status_.containerName; }

  signals:
    void statusChanged(const DockerSandboxStatus &status);

  private:
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

    DockerCommandResult runDocker(const QStringList &args, int timeoutMs = 120000) const;
    bool ensureDockerReachable(QString *errorMessage);
    bool ensureImageReady(const QString &image, QString *errorMessage);
    bool ensureContainer(QString *errorMessage);
    bool containerExists(const QString &name, bool *running, QString *errorMessage);
    bool createContainer(const QString &image, const QString &name, const QString &hostWorkdir, QString *errorMessage);
    bool startContainer(const QString &name, QString *errorMessage);
    void stopContainer(const QString &name);
    void fetchMetadata();
    void updateStatusAndNotify();

    Config config_;
    DockerSandboxStatus status_;
    QString dockerVersion_;
};

#endif // DOCKER_SANDBOX_H
