// Lightweight local backend manager for llama.cpp server
#ifndef XBACKEND_H
#define XBACKEND_H

#include "xconfig.h"
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>

// Manages the lifecycle of a local llama.cpp server and exposes a simple API
// to (re)start it from SETTINGS. All UI/model calls go through xNet; this
// class only ensures a local server exists when in LOCAL_MODE.
class LocalServerManager : public QObject
{
    Q_OBJECT
  public:
    explicit LocalServerManager(QObject *parent, const QString &appDirPath);
    ~LocalServerManager() override;

    // Configure inputs used to build server arguments
    void setSettings(const SETTINGS &s);
    void setPort(const QString &port);
    void setHost(const QString &host);
    void setModelPath(const QString &path);
    void setMmprojPath(const QString &path);
    void setLoraPath(const QString &path);

    // Ensure the server is running with the current args. Restart if args changed.
    void ensureRunning();
    // Force restart regardless of args (used when user insists).
    void restart();
    // Stop server if running.
    void stop();
    // Asynchronously stop server without blocking the UI thread.
    // This schedules terminate/kill on the owning thread and emits
    // serverStopped() when the process actually exits.
    void stopAsync();

    bool isRunning() const;
    QString endpointBase() const; // e.g. http://127.0.0.1:8080
    // Whether current program/args differ from last started ones
    bool needsRestart() const;

  signals:
    void serverOutput(const QString &line);
    void serverState(const QString &line, SIGNAL_STATE type);
    void serverReady(const QString &endpoint); // emitted when server is listening
    void serverStopped();
    // Emitted when a start/restart attempt fails:
    // - executable missing / failed to start
    // - process exits/crashes before becoming "ready"
    // UI can stop loading animation and unlock, and optionally trigger fallbacks.
    void serverStartFailed(const QString &reason);

  private:
    QString programPath() const;   // resolve llama-server path per platform
    QStringList buildArgs() const; // build args from SETTINGS and paths
    void startProcess(const QStringList &args);
    void hookProcessSignals();
    void emitServerStoppedOnce();

    QString appDirPath_;
    SETTINGS settings_;
    QString port_ = DEFAULT_SERVER_PORT;
    QString host_ = "0.0.0.0";
    QString modelpath_;
    QString mmproj_;
    QString lora_;

    QPointer<QProcess> proc_;
    QString lastProgram_;
    QStringList lastArgs_;

    // 启动阶段状态：用于把“启动后立刻崩溃/退出”识别为启动失败，避免 UI 一直等待 serverReady 卡死。
    bool readyEmitted_ = false;
    bool startFailedEmitted_ = false;
    bool stoppedEmitted_ = false;
};

#endif // XBACKEND_H
