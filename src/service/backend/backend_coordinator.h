#ifndef BACKEND_COORDINATOR_H
#define BACKEND_COORDINATOR_H

#include <QObject>
#include <QHostAddress>
#include <QString>

class Widget;

// 后端协调器：集中本地后端生命周期、代理端口与惰性卸载逻辑
// 注意：当前仍通过 Widget 访问 UI/配置状态，后续可继续下沉为独立状态机。
class BackendCoordinator : public QObject
{
    Q_OBJECT

  public:
    explicit BackendCoordinator(Widget *owner);

    void ensureLocalServer(bool lazyWake = false, bool forceReload = false);
    QString pickFreeTcpPort(const QHostAddress &addr = QHostAddress::Any) const;
    void announcePortBusy(const QString &requestedPort, const QString &alternativePort);
    void initiatePortFallback();
    bool ensureProxyListening(const QString &host, const QString &port, QString *errorMessage);
    QString formatLocalEndpoint(const QString &host, const QString &port) const;
    void updateProxyBackend(const QString &backendHost, const QString &backendPort);
    void onProxyWakeRequested();
    void onProxyExternalActivity();
    void markBackendActivity();
    void scheduleLazyUnload();
    void cancelLazyUnload(const QString &reason);
    void performLazyUnload();
    void performLazyUnloadInternal(bool forced);
    bool lazyUnloadEnabled() const;
    void setLazyCountdownLabelDisplay(const QString &status);
    void updateLazyCountdownLabel();
    void onLazyUnloadNowClicked();
    void onServerReady(const QString &endpoint);
    void onServerOutput(const QString &chunk);
    bool processServerOutputLine(const QString &line);
    void onServerStartFailed(const QString &reason);
    bool shouldArmWin7CpuFallback() const;
    bool triggerWin7CpuFallback(const QString &reasonTag);
    void resetBackendFallbackState(const QString &reasonTag);
    QString pickNextBackendFallback(const QString &failedBackend) const;
    bool triggerBackendFallback(const QString &failedBackend, const QString &reasonTag);

  private:
    Widget *w_ = nullptr;
};

#endif // BACKEND_COORDINATOR_H
