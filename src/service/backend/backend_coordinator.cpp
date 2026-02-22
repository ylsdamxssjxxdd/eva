#include "service/backend/backend_coordinator.h"

#include "widget/widget.h"
#include "ui_widget.h"
#include "utils/devicemanager.h"
#include "utils/eva_error.h"
#include "utils/flowtracer.h"
#include "utils/recovery_guidance.h"
#include "utils/startuplogger.h"
#include "utils/textparse.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHostAddress>
#include <QLatin1Char>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

BackendCoordinator::BackendCoordinator(Widget *owner)
    : QObject(owner),
      w_(owner)
{
}

void BackendCoordinator::ensureLocalServer(bool lazyWake, bool forceReload)
{
    if (!w_) return;
    if (w_->isShuttingDown_)
    {
        FlowTracer::log(FlowChannel::Backend,
                        QStringLiteral("backend: ensureLocalServer skipped (shutting down)"),
                        w_->activeTurnId_);
        return;
    }

    if (!w_->serverManager) return;

    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: ensureLocalServer lazy=%1 force=%2 mode=%3")
                        .arg(lazyWake ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(forceReload ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(w_->ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local")),
                    w_->activeTurnId_);
    StartupLogger::log(QStringLiteral("ensureLocalServer start (lazy=%1, force=%2)")
                           .arg(lazyWake)
                           .arg(forceReload));
    {
        QJsonObject fields;
        fields.insert(QStringLiteral("lazy_wake"), lazyWake);
        fields.insert(QStringLiteral("force_reload"), forceReload);
        fields.insert(QStringLiteral("mode"), w_->ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local"));
        w_->recordPerfEvent(QStringLiteral("backend.ensure"), fields);
    }
    QElapsedTimer ensureTimer;
    ensureTimer.start();

    cancelLazyUnload(QStringLiteral("ensureLocalServer entry"));
    if (lazyWake)
    {
        // 懒唤醒不走 preLoad，但 onServerReady 仍会用 load_timer 计算装载耗时
        if (!w_->lazyWakeInFlight_)
        {
            w_->load_time = 0;
            w_->load_timer.start();
        }
        w_->lazyWakeInFlight_ = true;
        w_->applyWakeUiLock(true);
        w_->setBackendLifecycleState(BackendLifecycleState::Waking, QStringLiteral("lazy wake"), SIGNAL_SIGNAL, false);
    }

    if (!w_->firstAutoNglEvaluated_ && !w_->serverManager->isRunning())
    {
        w_->firstAutoNglEvaluated_ = true;
        // Only auto-evaluate ngl on first-run when there is no persisted positive value.
        if (w_->ui_SETTINGS.ngl <= 0)
        {
            QFileInfo fileInfo(w_->ui_SETTINGS.modelpath);
            QFileInfo fileInfo2(w_->ui_SETTINGS.mmprojpath);
            const int modelsize_MB = fileInfo.size() / 1024 / 1024 + fileInfo2.size() / 1024 / 1024;
            if (modelsize_MB > 0 && w_->vfree > 0)
            {
                const double limit = 0.95 * w_->vfree;
                if (modelsize_MB <= limit)
                {
                    w_->ui_SETTINGS.ngl = 999; // 初次装载：尽可能全量 offload
                    if (w_->settings_ui && w_->settings_ui->ngl_slider)
                    {
                        w_->settings_ui->ngl_slider->setValue(w_->ui_SETTINGS.ngl);
                        w_->settings_ui->ngl_label->setText("gpu " + w_->jtr("offload") + " " + QString::number(w_->ui_SETTINGS.ngl));
                    }
                }
            }
            else if (modelsize_MB > 0 && w_->vfree <= 0)
            {
                w_->gpu_wait_load = true;
            }
        }
    }

    const QString originalUserPort = w_->ui_port.trimmed();
    w_->ui_port = originalUserPort;
    w_->portConflictDetected_ = false;
    enum class PortFallbackReason
    {
        None,
        Invalid,
        Busy
    };
    PortFallbackReason fallbackReason = PortFallbackReason::None;

    QString frontendHost = QStringLiteral("0.0.0.0");
    QString chosenPort = originalUserPort;
    bool appliedFallbackPort = false;

    if (!w_->portFallbackInFlight_)
    {
        w_->forcedPortOverride_.clear();
    }
    if (!w_->forcedPortOverride_.isEmpty())
    {
        chosenPort = w_->forcedPortOverride_;
        appliedFallbackPort = true;
        fallbackReason = PortFallbackReason::Busy;
    }

    if (chosenPort.isEmpty())
    {
        frontendHost = QStringLiteral("127.0.0.1");
        QString fallback = pickFreeTcpPort();
        if (!fallback.isEmpty()) chosenPort = fallback;
        if (chosenPort.isEmpty()) chosenPort = QString(DEFAULT_SERVER_PORT);
        w_->ui_port.clear();
        w_->lastPortConflictPreferred_.clear();
        w_->lastPortConflictFallback_.clear();
        if (w_->settings_ui && w_->settings_ui->port_lineEdit)
        {
            w_->settings_ui->port_lineEdit->setPlaceholderText("blank = localhost only (random port)");
            w_->settings_ui->port_lineEdit->setToolTip(QString());
        }
        w_->reflash_state(QStringLiteral("ui:port cleared -> bind 127.0.0.1:%1").arg(chosenPort), SIGNAL_SIGNAL);
        appliedFallbackPort = true;
    }
    else
    {
        bool ok = false;
        const quint16 portNum = chosenPort.toUShort(&ok);
        if (!ok || portNum == 0)
        {
            frontendHost = QStringLiteral("127.0.0.1");
            QString fallback = pickFreeTcpPort();
            if (!fallback.isEmpty()) chosenPort = fallback;
            appliedFallbackPort = true;
            fallbackReason = PortFallbackReason::Invalid;
            w_->lastPortConflictPreferred_.clear();
            w_->lastPortConflictFallback_.clear();
            w_->reflash_state(QStringLiteral("ui:invalid port -> bind 127.0.0.1:%1").arg(chosenPort), SIGNAL_SIGNAL);
            if (w_->settings_ui && w_->settings_ui->port_lineEdit)
            {
                w_->settings_ui->port_lineEdit->setPlaceholderText(w_->jtr("port fallback placeholder").arg(chosenPort));
                w_->settings_ui->port_lineEdit->setToolTip(w_->jtr("invalid port fallback body").arg(chosenPort));
            }
        }
    }

    if (appliedFallbackPort)
    {
        w_->ui_port = originalUserPort;
        if (w_->settings_ui && w_->settings_ui->port_lineEdit)
        {
            QSignalBlocker blocker(w_->settings_ui->port_lineEdit);
            w_->settings_ui->port_lineEdit->setText(originalUserPort);
        }
        if (fallbackReason == PortFallbackReason::Invalid && w_->settings_ui && w_->settings_ui->port_lineEdit)
        {
            w_->settings_ui->port_lineEdit->setPlaceholderText(w_->jtr("port fallback placeholder").arg(chosenPort));
            w_->settings_ui->port_lineEdit->setToolTip(w_->jtr("invalid port fallback body").arg(chosenPort));
        }
        w_->portFallbackInFlight_ = false;
    }
    else if (!originalUserPort.isEmpty())
    {
        w_->ui_port = originalUserPort;
        if (w_->settings_ui && w_->settings_ui->port_lineEdit)
        {
            w_->settings_ui->port_lineEdit->setPlaceholderText(QString());
            w_->settings_ui->port_lineEdit->setToolTip(QString());
        }
        w_->lastPortConflictPreferred_.clear();
        w_->lastPortConflictFallback_.clear();
    }

    w_->forcedPortOverride_.clear();
    w_->portFallbackInFlight_ = false;

    QString proxyError;
    if (!ensureProxyListening(frontendHost, chosenPort, &proxyError))
    {
        if (!proxyError.isEmpty())
        {
            w_->reflash_state(formatEvaErrorWithHint(EvaErrorCode::BeProxyListenFailed,
                                                     QStringLiteral("proxy %1").arg(proxyError),
                                                     RecoveryHintAction::AdjustPort),
                              WRONG_SIGNAL);
            FlowTracer::log(FlowChannel::Backend,
                            QStringLiteral("backend: proxy listen fail %1:%2 (%3)")
                                .arg(frontendHost, chosenPort, proxyError),
                            w_->activeTurnId_);
        }
        if (!w_->portFallbackInFlight_)
        {
            w_->portConflictDetected_ = true;
            initiatePortFallback();
        }
        w_->lazyWakeInFlight_ = false;
        w_->applyWakeUiLock(false);
        w_->setBackendLifecycleState(BackendLifecycleState::Error,
                                     QStringLiteral("proxy listen failed"),
                                     WRONG_SIGNAL,
                                     false);
        return;
    }

    w_->activeServerHost_ = frontendHost;
    w_->activeServerPort_ = chosenPort;
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: proxy ready front %1:%2")
                        .arg(w_->activeServerHost_, w_->activeServerPort_),
                    w_->activeTurnId_);

    QString backendPort = w_->activeBackendPort_;
    const bool backendRunning = w_->serverManager->isRunning();
    if (backendPort.isEmpty() || backendPort == chosenPort || !backendRunning)
    {
        QString candidate;
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            candidate = pickFreeTcpPort(QHostAddress::LocalHost);
            if (!candidate.isEmpty() && candidate != chosenPort)
                break;
        }
        if (candidate.isEmpty() || candidate == chosenPort)
        {
            bool ok = false;
            const quint16 frontNum = chosenPort.toUShort(&ok);
            quint16 alt = ok ? quint16((frontNum + 1) % 65535) : 0;
            if (alt == 0 || alt == frontNum) alt = 9000;
            candidate = QString::number(alt);
        }
        backendPort = candidate;
    }
    w_->activeBackendPort_ = backendPort;
    updateProxyBackend(w_->backendListenHost_, w_->activeBackendPort_);
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: target %1:%2 (proxy %3:%4)")
                        .arg(w_->backendListenHost_, w_->activeBackendPort_, w_->activeServerHost_, w_->activeServerPort_),
                    w_->activeTurnId_);

    w_->serverManager->setSettings(w_->ui_SETTINGS);
    w_->serverManager->setHost(w_->backendListenHost_);
    w_->serverManager->setPort(w_->activeBackendPort_);
    w_->serverManager->setModelPath(w_->ui_SETTINGS.modelpath);
    w_->serverManager->setMmprojPath(w_->ui_SETTINGS.mmprojpath);
    w_->serverManager->setLoraPath(w_->ui_SETTINGS.lorapath);

    // 用户主动“重载模型”时，即使参数完全相同也要强制重启后端，
    // 否则会出现没有装载动画、按钮状态提前解锁的问题。
    const bool forceRestartActive = forceReload && backendRunning;
    w_->lastServerRestart_ = w_->serverManager->needsRestart() || forceRestartActive;
    if (w_->lastServerRestart_)
    {
        w_->win7CpuFallbackArmed_ = shouldArmWin7CpuFallback();
        w_->win7CpuFallbackTriggered_ = false;
    }
    else
    {
        w_->win7CpuFallbackArmed_ = false;
        w_->win7CpuFallbackTriggered_ = false;
    }
    const bool hadOld = backendRunning;
    w_->ignoreNextServerStopped_ = w_->lastServerRestart_ && hadOld;
    if (w_->lastServerRestart_)
    {
        const BackendLifecycleState targetState = hadOld ? BackendLifecycleState::Restarting : BackendLifecycleState::Starting;
        w_->setBackendLifecycleState(targetState, QStringLiteral("ensure local backend"), SIGNAL_SIGNAL, false);
    }
    else if (backendRunning)
    {
        w_->setBackendLifecycleState(BackendLifecycleState::Running, QStringLiteral("reuse running backend"), USUAL_SIGNAL, false);
    }
    if (w_->lastServerRestart_)
    {
        w_->backendOnline_ = false;
        if (w_->proxyServer_) w_->proxyServer_->setBackendAvailable(false);
        if (!lazyWake)
        {
            w_->preLoad();
            emit w_->ui2net_stop(true);
        }
    }

    if (forceRestartActive)
    {
        w_->serverManager->restart();
    }
    else
    {
        w_->serverManager->ensureRunning();
    }
    StartupLogger::log(QStringLiteral("ensureLocalServer launch done (%1 ms, restart=%2)")
                           .arg(ensureTimer.elapsed())
                           .arg(w_->lastServerRestart_));
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: ensureRunning issued restart=%1")
                        .arg(w_->lastServerRestart_ ? QStringLiteral("yes") : QStringLiteral("no")),
                    w_->activeTurnId_);

    w_->backendOnline_ = w_->serverManager->isRunning() && !w_->lastServerRestart_;
    if (w_->proxyServer_) w_->proxyServer_->setBackendAvailable(w_->backendOnline_);
    if (!lazyWake && w_->backendOnline_) markBackendActivity();
    if (w_->backendOnline_)
    {
        FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: online"), w_->activeTurnId_);
    }

    if (!w_->lastServerRestart_ && backendRunning)
    {
        w_->lazyWakeInFlight_ = false;
        w_->applyWakeUiLock(false);
        w_->setBackendLifecycleState(BackendLifecycleState::Running, QStringLiteral("already running"), USUAL_SIGNAL, false);
    }

    w_->apis.api_endpoint = formatLocalEndpoint(w_->activeServerHost_, w_->activeServerPort_);
    w_->apis.api_key = "";
    w_->apis.api_model = "default";
    w_->apis.is_local_backend = true;
    // 切回本地后端后，必须恢复 llama.cpp 的 /v1/... 路径
    w_->apis.api_chat_endpoint = QStringLiteral(CHAT_ENDPOINT);
    w_->apis.api_completion_endpoint = QStringLiteral(COMPLETION_ENDPOINT);
    emit w_->ui2expend_apis(w_->apis);
    emit w_->ui2expend_mode(w_->ui_mode);
    updateLazyCountdownLabel();

    if (w_->backendOnline_ && lazyUnloadEnabled())
    {
        scheduleLazyUnload();
    }
}

QString BackendCoordinator::pickFreeTcpPort(const QHostAddress &addr) const
{
    QTcpServer server;
    if (server.listen(addr, 0))
    {
        const quint16 port = server.serverPort();
        server.close();
        return QString::number(port);
    }
    return QString();
}

void BackendCoordinator::announcePortBusy(const QString &requestedPort, const QString &alternativePort)
{
    if (!w_) return;
    if (alternativePort.isEmpty())
    {
        w_->reflash_state(w_->jtr("ui port busy none").arg(requestedPort), WRONG_SIGNAL);
        return;
    }

    const bool repeated = (w_->lastPortConflictPreferred_ == requestedPort && w_->lastPortConflictFallback_ == alternativePort);
    w_->lastPortConflictPreferred_ = requestedPort;
    w_->lastPortConflictFallback_ = alternativePort;

    const QString stateLine = formatEvaErrorWithHint(EvaErrorCode::BePortConflict,
                                                     w_->jtr("ui port busy switched").arg(requestedPort, alternativePort),
                                                     RecoveryHintAction::AdjustPort);
    w_->reflash_state(stateLine, WRONG_SIGNAL);

    const QString dialogTitle = w_->jtr("port conflict title");
    const QString dialogText = w_->jtr("port conflict body").arg(requestedPort, alternativePort);

    if (w_->settings_ui && w_->settings_ui->port_lineEdit)
    {
        w_->settings_ui->port_lineEdit->setPlaceholderText(w_->jtr("port fallback placeholder").arg(alternativePort));
        w_->settings_ui->port_lineEdit->setToolTip(dialogText);
    }

    if (!repeated)
    {
        QTimer::singleShot(0, w_, [this, dialogTitle, dialogText]()
                           { QMessageBox::warning(w_, dialogTitle, dialogText); });
    }
}

void BackendCoordinator::initiatePortFallback()
{
    if (!w_) return;
    if (w_->portFallbackInFlight_) return;

    const QString preferred = w_->ui_port.trimmed();
    if (preferred.isEmpty()) return;

    QString fallback = pickFreeTcpPort();
    if (fallback.isEmpty() || fallback == preferred)
    {
        w_->reflash_state(w_->jtr("ui port busy none").arg(preferred), WRONG_SIGNAL);
        return;
    }

    w_->portConflictDetected_ = false;
    w_->portFallbackInFlight_ = true;
    w_->forcedPortOverride_ = fallback;

    announcePortBusy(preferred, fallback);

    if (w_->settings_ui && w_->settings_ui->port_lineEdit)
    {
        w_->settings_ui->port_lineEdit->setPlaceholderText(w_->jtr("port fallback placeholder").arg(fallback));
        w_->settings_ui->port_lineEdit->setToolTip(w_->jtr("port conflict body").arg(preferred, fallback));
    }

    if (w_->proxyServer_) w_->proxyServer_->stop();
    if (w_->serverManager)
    {
        w_->ignoreNextServerStopped_ = true;
        w_->serverManager->stopAsync();
    }

    QTimer::singleShot(150, w_, [this]()
                       { ensureLocalServer(); });
}

bool BackendCoordinator::ensureProxyListening(const QString &host, const QString &port, QString *errorMessage)
{
    if (!w_) return false;
    if (!w_->proxyServer_) return true;
    bool ok = false;
    const quint16 value = port.toUShort(&ok);
    if (!ok || value == 0)
    {
        if (errorMessage) *errorMessage = QStringLiteral("invalid proxy port -> %1").arg(port);
        return false;
    }
    if (w_->proxyServer_->isListening() && w_->proxyServer_->listenHost() == host && w_->proxyServer_->listenPort() == value)
    {
        return true;
    }
    if (!w_->proxyServer_->start(host, value, errorMessage))
    {
        return false;
    }
    return true;
}

QString BackendCoordinator::formatLocalEndpoint(const QString &host, const QString &port) const
{
    QString displayHost = host.trimmed();
    if (displayHost.isEmpty() || displayHost == QStringLiteral("0.0.0.0"))
    {
        displayHost = QStringLiteral("127.0.0.1");
    }
    QString displayPort = port.trimmed();
    if (displayPort.isEmpty())
    {
        displayPort = QString(DEFAULT_SERVER_PORT);
    }
    return QStringLiteral("http://%1:%2").arg(displayHost, displayPort);
}

void BackendCoordinator::updateProxyBackend(const QString &backendHost, const QString &backendPort)
{
    if (!w_) return;
    if (!w_->proxyServer_) return;
    bool ok = false;
    const quint16 value = backendPort.toUShort(&ok);
    w_->proxyServer_->setBackendEndpoint(backendHost, ok ? value : 0);
    w_->proxyServer_->setBackendAvailable(ok && w_->backendOnline_);
}

void BackendCoordinator::onProxyWakeRequested()
{
    if (!w_) return;
    if (!lazyUnloadEnabled()) return;
    if (w_->lazyWakeInFlight_) return;
    if (w_->backendOnline_ && w_->serverManager && w_->serverManager->isRunning()) return;
    w_->reflash_state(QStringLiteral("ui:proxy wake -> local backend"), SIGNAL_SIGNAL);
    w_->lazyWakeInFlight_ = true;
    w_->setBackendLifecycleState(BackendLifecycleState::Waking, QStringLiteral("proxy wake request"), SIGNAL_SIGNAL, false);
    w_->pendingSendAfterWake_ = false;
    ensureLocalServer(true);
}

void BackendCoordinator::onProxyExternalActivity()
{
    if (!w_) return;
    if (!lazyUnloadEnabled()) return;

    markBackendActivity();

    const bool busy = w_->turnActive_ || w_->toolInvocationActive_ || w_->lazyWakeInFlight_;
    if (busy)
    {
        cancelLazyUnload(QStringLiteral("proxy activity"));
        return;
    }

    if (w_->backendOnline_ && !w_->lazyUnloaded_)
    {
        scheduleLazyUnload();
    }
    else
    {
        updateLazyCountdownLabel();
    }
}

void BackendCoordinator::markBackendActivity()
{
    if (!w_) return;
    if (!lazyUnloadEnabled()) return;
    if (!w_->idleSince_.isValid())
        w_->idleSince_.start();
    else
        w_->idleSince_.restart();
    updateLazyCountdownLabel();
}

void BackendCoordinator::scheduleLazyUnload()
{
    if (!w_) return;
    if (!lazyUnloadEnabled()) return;
    if (!w_->serverManager || !w_->serverManager->isRunning()) return;
    if (w_->turnActive_ || w_->toolInvocationActive_) return;
    if (!w_->lazyUnloadTimer_) return;
    w_->lazyUnloaded_ = false;
    if (!w_->idleSince_.isValid())
        w_->idleSince_.start();
    else
        w_->idleSince_.restart();
    w_->lazyUnloadTimer_->start(w_->lazyUnloadMs_);
    updateLazyCountdownLabel();
}

void BackendCoordinator::cancelLazyUnload(const QString &reason)
{
    Q_UNUSED(reason);
    if (!w_) return;
    if (!w_->lazyUnloadTimer_) return;
    if (w_->lazyUnloadTimer_->isActive()) w_->lazyUnloadTimer_->stop();
    if (w_->lazyCountdownTimer_ && w_->lazyCountdownTimer_->isActive()) w_->lazyCountdownTimer_->stop();
    w_->lazyUnloaded_ = false;
    w_->idleSince_ = QElapsedTimer();
    updateLazyCountdownLabel();
}

void BackendCoordinator::performLazyUnload()
{
    performLazyUnloadInternal(false);
}

void BackendCoordinator::performLazyUnloadInternal(bool forced)
{
    if (!w_) return;
    w_->lazyUnloadPreserveState_ = false;
    if (!forced && !lazyUnloadEnabled()) return;
    w_->pendingSendAfterWake_ = false;
    if (w_->lazyUnloadTimer_ && w_->lazyUnloadTimer_->isActive()) w_->lazyUnloadTimer_->stop();
    if (w_->lazyCountdownTimer_ && w_->lazyCountdownTimer_->isActive()) w_->lazyCountdownTimer_->stop();
    if (!forced && (w_->turnActive_ || w_->toolInvocationActive_))
    {
        scheduleLazyUnload();
        updateLazyCountdownLabel();
        return;
    }
    if (forced && w_->turnActive_)
    {
        emit w_->ui2net_stop(true);
        w_->turnActive_ = false;
        w_->is_run = false;
    }
    if (forced && w_->toolInvocationActive_)
    {
        emit w_->ui2tool_cancelActive();
        w_->toolInvocationActive_ = false;
    }
    const bool hasUi = w_->ui && w_->ui->output;
    const bool hasDocument = hasUi && w_->ui->output->document();
    const bool hasRenderedContent = hasDocument && !w_->ui->output->document()->isEmpty();
    const bool hasConversationHistory = !w_->ui_messagesArray.isEmpty();
    const bool preserveAfterWake = (w_->ui_state == CHAT_STATE) && (hasConversationHistory || hasRenderedContent);
    w_->lazyUnloaded_ = true;
    if (preserveAfterWake)
        w_->lazyUnloadPreserveState_ = true;
    w_->preserveConversationOnNextReady_ = preserveAfterWake;
    w_->lazyWakeInFlight_ = false;
    w_->applyWakeUiLock(false);
    w_->backendOnline_ = false;
    w_->setBackendLifecycleState(BackendLifecycleState::Sleeping, QStringLiteral("lazy unload"), SIGNAL_SIGNAL, false);
    {
        QJsonObject fields;
        fields.insert(QStringLiteral("forced"), forced);
        fields.insert(QStringLiteral("preserve_conversation"), preserveAfterWake);
        w_->recordPerfEvent(QStringLiteral("backend.lazy_unload"), fields);
    }
    if (w_->proxyServer_) w_->proxyServer_->setBackendAvailable(false);
    w_->reflash_state(QStringLiteral("ui:") + w_->jtr("auto eject stop backend"), SIGNAL_SIGNAL);
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: lazy unload forced=%1 preserve=%2")
                        .arg(forced ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(w_->lazyUnloadPreserveState_ ? QStringLiteral("yes") : QStringLiteral("no")),
                    w_->activeTurnId_);
    w_->suppressStateClearOnStop_ = !forced;
    if (w_->serverManager && w_->serverManager->isRunning())
    {
        w_->serverManager->stopAsync();
    }
    else if (forced)
    {
        w_->suppressStateClearOnStop_ = false;
    }
    w_->idleSince_ = QElapsedTimer();
    updateLazyCountdownLabel();
}

bool BackendCoordinator::lazyUnloadEnabled() const
{
    if (!w_) return false;
    return w_->proxyServer_ && w_->lazyUnloadTimer_ && w_->lazyUnloadMs_ > 0 && w_->ui_mode == LOCAL_MODE;
}

void BackendCoordinator::setLazyCountdownLabelDisplay(const QString &status)
{
    if (!w_) return;
    if (!w_->settings_ui || !w_->settings_ui->lazy_timeout_label) return;

    QString display = w_->jtr("pop timeout label");
    const QString trimmed = status.trimmed();
    if (!trimmed.isEmpty())
    {
        if (!display.isEmpty()) display += QStringLiteral(" ");
        display += trimmed;
    }
    else if (display.isEmpty())
    {
        display = trimmed;
    }
    w_->settings_ui->lazy_timeout_label->setText(display);
}

void BackendCoordinator::updateLazyCountdownLabel()
{
    if (!w_) return;
    if (!w_->settings_ui || !w_->settings_ui->lazy_timeout_label) return;

    QString status;
    if (w_->lazyUnloadMs_ <= 0)
    {
        status = w_->jtr("pop countdown disabled");
    }
    else if (!w_->backendOnline_ || w_->lazyUnloaded_)
    {
        status = w_->jtr("pop countdown popped");
    }
    else if (!w_->lazyUnloadTimer_ || !w_->lazyUnloadTimer_->isActive())
    {
        status = w_->jtr("pop countdown standby");
    }
    else
    {
        int remaining = w_->lazyUnloadTimer_->remainingTime();
        if (remaining < 0) remaining = 0;
        const int totalSeconds = (remaining + 999) / 1000;
        const int hours = totalSeconds / 3600;
        const int minutes = (totalSeconds % 3600) / 60;
        const int seconds = totalSeconds % 60;
        if (hours > 0)
        {
            status = QStringLiteral("%1:%2:%3")
                         .arg(hours, 2, 10, QLatin1Char('0'))
                         .arg(minutes, 2, 10, QLatin1Char('0'))
                         .arg(seconds, 2, 10, QLatin1Char('0'));
        }
        else
        {
            status = QStringLiteral("%1:%2")
                         .arg(minutes, 2, 10, QLatin1Char('0'))
                         .arg(seconds, 2, 10, QLatin1Char('0'));
        }
    }

    setLazyCountdownLabelDisplay(status);
    w_->settings_ui->lazy_timeout_label->setToolTip(w_->jtr("pop countdown tooltip"));

    if (w_->lazyCountdownTimer_)
    {
        if (w_->lazyUnloadTimer_ && w_->lazyUnloadTimer_->isActive() && w_->lazyUnloadMs_ > 0 && w_->backendOnline_)
        {
            if (!w_->lazyCountdownTimer_->isActive()) w_->lazyCountdownTimer_->start();
        }
        else
        {
            w_->lazyCountdownTimer_->stop();
        }
    }
}

void BackendCoordinator::onLazyUnloadNowClicked()
{
    if (!w_) return;
    if (w_->ui_mode != LOCAL_MODE)
    {
        w_->reflash_state(QStringLiteral("ui:惰性卸载仅在本地模式启用"), WRONG_SIGNAL);
        return;
    }
    if (!w_->serverManager || !w_->serverManager->isRunning())
    {
        w_->reflash_state(QStringLiteral("ui:本地后端已停止"), SIGNAL_SIGNAL);
        cancelLazyUnload(QStringLiteral("manual unload"));
        return;
    }
    w_->reflash_state(QStringLiteral("ui:") + w_->jtr("pop trigger"), SIGNAL_SIGNAL);
    FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: manual lazy unload"), w_->activeTurnId_);
    performLazyUnloadInternal(true);
}

void BackendCoordinator::onServerReady(const QString &endpoint)
{
    if (!w_) return;
    w_->win7CpuFallbackArmed_ = false;
    w_->win7CpuFallbackTriggered_ = false;
    w_->backendOnline_ = true;
    w_->setBackendLifecycleState(BackendLifecycleState::Running, QStringLiteral("server ready"), SUCCESS_SIGNAL, false);
    resetBackendFallbackState(QStringLiteral("backend ready"));
    w_->lazyUnloaded_ = false;
    w_->lazyWakeInFlight_ = false;
    w_->applyWakeUiLock(false);
    cancelLazyUnload(QStringLiteral("backend ready"));
    markBackendActivity();
    updateProxyBackend(w_->backendListenHost_, w_->activeBackendPort_);
    if (w_->proxyServer_) w_->proxyServer_->setBackendAvailable(true);

    scheduleLazyUnload();
    updateLazyCountdownLabel();
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: ready %1 (front %2:%3 backend %4)")
                        .arg(endpoint, w_->activeServerHost_, w_->activeServerPort_, w_->activeBackendPort_),
                    w_->activeTurnId_);

    if (w_->pendingSendAfterWake_)
    {
        w_->pendingSendAfterWake_ = false;
        QTimer::singleShot(0, w_, [this]() { w_->on_send_clicked(); });
    }
    // 后端就绪后尝试派发定时任务（若有队列）
    w_->tryDispatchScheduledJobs();

    // 配置本地端点；统一由动效收尾逻辑 unlockLoad() 设置标题/图标/状态
    const QUrl backendUrl = QUrl::fromUserInput(endpoint);
    if (backendUrl.isValid() && backendUrl.port() > 0)
    {
        w_->activeBackendPort_ = QString::number(backendUrl.port());
        updateProxyBackend(w_->backendListenHost_, w_->activeBackendPort_);
    }
    const QString frontendEndpoint = formatLocalEndpoint(w_->activeServerHost_, w_->activeServerPort_);
    w_->apis.api_endpoint = frontendEndpoint;
    w_->apis.api_key = "";
    w_->apis.api_model = "default";
    w_->apis.is_local_backend = true;
    // 同上：本地后端固定走 /v1/...，避免 LINK 模式端点残留
    w_->apis.api_chat_endpoint = QStringLiteral(CHAT_ENDPOINT);
    w_->apis.api_completion_endpoint = QStringLiteral(COMPLETION_ENDPOINT);
    emit w_->ui2expend_apis(w_->apis);
    emit w_->ui2expend_mode(w_->ui_mode);

    // 完成装载：记录耗时，统一用简单转轮动效收尾，然后解锁 UI
    w_->load_time = w_->load_timer.isValid() ? (w_->load_timer.nsecsElapsed() / 1e9) : 0.0;
    {
        QJsonObject fields;
        fields.insert(QStringLiteral("mode"), QStringLiteral("local"));
        fields.insert(QStringLiteral("load_seconds"), w_->load_time);
        fields.insert(QStringLiteral("restart"), w_->lastServerRestart_);
        fields.insert(QStringLiteral("port"), w_->activeServerPort_);
        w_->recordPerfEvent(QStringLiteral("backend.ready"), fields);
    }
    w_->ui_mode = LOCAL_MODE;

    const bool preserveConversation = w_->preserveConversationOnNextReady_;
    w_->preserveConversationOnNextReady_ = false;
    w_->skipUnlockLoadIntro_ = preserveConversation;

    w_->flushPendingStream();
    if (!preserveConversation)
    {
        w_->ui->output->clear();
        w_->recordClear(); // drop any pre-load records before starting a new session
        w_->ui_messagesArray = QJsonArray();
        {
            QJsonObject systemMessage;
            systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
            systemMessage.insert("content", w_->ui_DATES.date_prompt);
            w_->ui_messagesArray.append(systemMessage);
            if (w_->history_ && w_->ui_state == CHAT_STATE)
            {
                SessionMeta meta;
                meta.id = QString::number(QDateTime::currentMSecsSinceEpoch());
                meta.title = "";
                meta.endpoint = frontendEndpoint;
                meta.model = w_->ui_SETTINGS.modelpath;
                meta.system = w_->ui_DATES.date_prompt;
                meta.n_ctx = w_->ui_SETTINGS.nctx;
                meta.slot_id = -1;
                meta.startedAt = QDateTime::currentDateTime();
                w_->history_->begin(meta);
                w_->history_->appendMessage(systemMessage);
                w_->currentSlotId_ = -1;
            }
        }
        w_->bot_predecode_content = w_->ui_DATES.date_prompt; // 系统指令作为预解码内容展示
    }
    w_->is_load = true;
    // After fresh load, the first "all slots are idle" is an idle baseline -> ignore once
    w_->lastServerRestart_ = false;
    // Track the backend that actually came up and align UI hints/fallback logic.
    const QString resolvedBackend = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main"));
    const QString previousRuntime = w_->runtimeDeviceBackend_;
    if (!resolvedBackend.isEmpty())
    {
        w_->runtimeDeviceBackend_ = resolvedBackend;
    }
    const bool runtimeChanged = (!resolvedBackend.isEmpty() && resolvedBackend != previousRuntime);
    // Sync settings device combobox with actually resolved backend if user chose an explicit device.
    const QString userSel = DeviceManager::userChoice();
    if (!userSel.isEmpty() && userSel != QLatin1String("auto"))
    {
        if (!resolvedBackend.isEmpty() && resolvedBackend != userSel)
        {
            if (w_->settings_ui && w_->settings_ui->device_comboBox)
            {
                int idx = w_->settings_ui->device_comboBox->findText(resolvedBackend);
                if (idx < 0)
                {
                    w_->settings_ui->device_comboBox->addItem(resolvedBackend);
                    idx = w_->settings_ui->device_comboBox->findText(resolvedBackend);
                }
                if (idx >= 0)
                {
                    w_->settings_ui->device_comboBox->setCurrentIndex(idx);
                }
            }
            w_->ui_device_backend = resolvedBackend;
            DeviceManager::setUserChoice(resolvedBackend);
            w_->auto_save_user();
            w_->reflash_state(QStringLiteral("ui:device fallback -> ") + resolvedBackend, SIGNAL_SIGNAL);
        }
    }
    else if (userSel == QLatin1String("auto") && runtimeChanged)
    {
        w_->reflash_state(QStringLiteral("ui:device resolved -> %1").arg(resolvedBackend), SIGNAL_SIGNAL);
    }
    w_->refreshDeviceBackendUI();
    // Complete load animation and finalize spinner state.
    w_->decode_finish();
    if (!w_->activeServerPort_.isEmpty())
    {
        QString displayHost = w_->activeServerHost_;
        if (displayHost.isEmpty() || displayHost == QStringLiteral("0.0.0.0"))
        {
            displayHost = QStringLiteral("127.0.0.1");
        }
        const QString url = QStringLiteral("http://%1:%2").arg(displayHost, w_->activeServerPort_);
        QString lanHint;
        if (w_->activeServerHost_ == QStringLiteral("0.0.0.0"))
        {
            const QString lanIp = w_->getFirstNonLoopbackIPv4Address();
            if (!lanIp.isEmpty())
            {
                lanHint = QStringLiteral(" / http://%1:%2").arg(lanIp, w_->activeServerPort_);
            }
        }
        w_->reflash_state(QStringLiteral("ui:") + w_->jtr("local endpoint ready") + QStringLiteral(" -> %1%2").arg(url, lanHint), SUCCESS_SIGNAL);
    }
    // 直接解锁界面（不再补帧播放复载动画）
    w_->unlockLoad();
}

void BackendCoordinator::onServerOutput(const QString &chunk)
{
    if (!w_) return;
    if (chunk.isEmpty()) return;

    w_->serverLogLineBuffer_.append(chunk);

    // 极端保护：若后端长时间不换行，缓冲可能无限增长；保留末尾更易捕获最近错误行
    const int kMaxBufferedChars = 256 * 1024;
    if (w_->serverLogLineBuffer_.size() > kMaxBufferedChars)
    {
        w_->serverLogLineBuffer_ = w_->serverLogLineBuffer_.right(kMaxBufferedChars);
    }

    while (true)
    {
        const int newline = w_->serverLogLineBuffer_.indexOf('\n');
        if (newline < 0) break;

        QString line = w_->serverLogLineBuffer_.left(newline);
        w_->serverLogLineBuffer_.remove(0, newline + 1);

        if (line.endsWith('\r')) line.chop(1);
        if (line.isEmpty()) continue;

        // processServerOutputLine 返回 true 表示触发了端口回退等重启路径
        if (processServerOutputLine(line))
        {
            w_->serverLogLineBuffer_.clear();
            return;
        }
    }
}

bool BackendCoordinator::processServerOutputLine(const QString &line)
{
    if (!w_) return false;
    static int s_firstLogs = 0;
    if (s_firstLogs < 6)
    {
        ++s_firstLogs;
        StartupLogger::log(QStringLiteral("[server log %1] %2").arg(s_firstLogs).arg(line.left(200)));
    }
    const QString trimmedLine = line.trimmed();
    if (lazyUnloadEnabled())
    {
        markBackendActivity();
        const bool busy = w_->turnActive_ || w_->toolInvocationActive_;
        if (busy)
        {
            cancelLazyUnload(QStringLiteral("log activity"));
        }
        const QString lowerLine = trimmedLine.toLower();
        if (lowerLine.contains(QStringLiteral("all slots are idle")) || lowerLine.contains(QStringLiteral("no pending work")) || lowerLine.contains(QStringLiteral("all clients are idle")))
        {
            if (w_->activeTurnId_ == 0)
            {
                w_->turnActive_ = false;
            }
            scheduleLazyUnload();
        }
        else if (!busy && w_->backendOnline_)
        {
            if (!w_->lazyUnloadTimer_ || !w_->lazyUnloadTimer_->isActive())
            {
                scheduleLazyUnload();
            }
        }
    }

    // 视觉输入能力提示（mmproj）
    {
        const QString lower = trimmedLine.toLower();
        const bool hitVisionNotSupported =
            lower.contains(QStringLiteral("image input is not supported")) ||
            lower.contains(QStringLiteral("you may need to provide the mmproj")) ||
            (lower.contains(QStringLiteral("mmproj")) && lower.contains(QStringLiteral("image")) && lower.contains(QStringLiteral("not supported")));
        if (hitVisionNotSupported)
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 kDedupWindowMs = 2500;
            if (nowMs - w_->lastVisionNotSupportedHintMs_ > kDedupWindowMs)
            {
                w_->lastVisionNotSupportedHintMs_ = nowMs;
                w_->reflash_state(QStringLiteral("ui:") + w_->jtr(QStringLiteral("vision not supported hint")), WRONG_SIGNAL);
            }
        }
    }

    // Detect fatal/failed patterns in llama.cpp server logs and unlock UI promptly
    {
        const QString l = line.toLower();
        if (!w_->portFallbackInFlight_ && !w_->ui_port.isEmpty() && w_->activeServerPort_ == w_->ui_port)
        {
            static const QStringList conflictKeys = {
                QStringLiteral("address already in use"),
                QStringLiteral("eaddrinuse"),
                QStringLiteral("failed to bind"),
                QStringLiteral("could not bind"),
                QStringLiteral("bind error"),
                QStringLiteral("listen(): address in use"),
                QStringLiteral("bind(): address")
            };
            for (const QString &key : conflictKeys)
            {
                if (l.contains(key))
                {
                    w_->portConflictDetected_ = true;
                    initiatePortFallback();
                    return true;
                }
            }
        }
        static const QStringList badKeys = {
            QStringLiteral(" failed"), QStringLiteral("fatal"), QStringLiteral("segmentation fault"),
            QStringLiteral("assertion failed"), QStringLiteral("error:")};
        bool hit = false;
        for (const QString &k : badKeys)
        {
            if (l.contains(k))
            {
                hit = true;
                break;
            }
        }
        if (hit && !l.contains("no error"))
        {
            w_->reflash_state(QString::fromUtf8("ui:后端异常输出，已解锁控件"), WRONG_SIGNAL);
            if (w_->decode_pTimer && w_->decode_pTimer->isActive()) w_->decode_fail();
            w_->is_run = false;
            w_->unlockButtonsAfterError();
        }
    }
    // 0) Track turn lifecycle heuristics
    if (line.contains("new prompt") || line.contains("launch_slot_"))
    {
        markBackendActivity();
        cancelLazyUnload(QStringLiteral("turn begin"));
        w_->turnActive_ = true;
        w_->kvUsedBeforeTurn_ = w_->kvUsed_;
        w_->kvTokensTurn_ = 0;
        w_->kvPromptTokensTurn_ = 0;
        w_->kvStreamedTurn_ = 0;
        w_->sawFinalPast_ = false;
    }

    int trainCtx = 0;
    if (TextParse::extractIntAfterKeyword(line, QStringLiteral("n_ctx_train"), trainCtx) && trainCtx > 0)
    {
        if (w_->ui_n_ctx_train != trainCtx)
        {
            w_->ui_n_ctx_train = trainCtx;
            if (w_->settings_ui && w_->settings_ui->nctx_slider)
            {
                const int curMax = w_->settings_ui->nctx_slider->maximum();
                if (curMax != trainCtx) w_->settings_ui->nctx_slider->setMaximum(trainCtx);
            }
        }
    }

    int ctxValue = 0;
    if (line.contains(QStringLiteral("llama_context")) && TextParse::extractIntAfterKeyword(line, QStringLiteral("n_ctx"), ctxValue) && ctxValue > 0)
    {
        w_->server_nctx_ = ctxValue;
        const int slotCtx = w_->ui_SETTINGS.nctx > 0 ? w_->ui_SETTINGS.nctx : DEFAULT_NCTX;
        const int parallel = w_->ui_SETTINGS.hid_parallel > 0 ? w_->ui_SETTINGS.hid_parallel : 1;
        const int expectedTotal = slotCtx * parallel;
        if (w_->server_nctx_ != expectedTotal)
        {
            w_->reflash_state(QStringLiteral("ui:server n_ctx=%1, expected=%2 (slot=%3, parallel=%4)")
                                  .arg(w_->server_nctx_)
                                  .arg(expectedTotal)
                                  .arg(slotCtx)
                                  .arg(parallel),
                              SIGNAL_SIGNAL);
        }
    }

    int slotCtx = 0;
    if (TextParse::extractIntAfterKeyword(line, QStringLiteral("n_ctx_slot"), slotCtx) && slotCtx > 0)
    {
        w_->slotCtxMax_ = slotCtx;
        w_->enforcePredictLimit();
        w_->updateKvBarUi();
        SETTINGS snap = w_->ui_SETTINGS;
        if (w_->ui_mode == LINK_MODE) snap.nctx = (w_->slotCtxMax_ > 0 ? w_->slotCtxMax_ : 0);
        emit w_->ui2expend_settings(snap);
    }

    int kvHit = 0;
    if (TextParse::extractIntBetweenMarkers(line, QStringLiteral("kv cache rm"), QStringLiteral("]"), kvHit))
    {
        w_->kvUsed_ = qMax(0, kvHit);
        w_->updateKvBarUi();
    }

    // ngl / max ngl 识别
    auto applyDetectedMaxNgl = [&](int maxngl)
    {
        constexpr int kMaxReasonableMaxNgl = 512;
        if (maxngl <= 0 || maxngl > kMaxReasonableMaxNgl) return;
        if (w_->ui_maxngl > 0 && maxngl < w_->ui_maxngl) return;

        const int oldMax = w_->ui_maxngl;
        const bool keepAtMaxIntent = (w_->ui_SETTINGS.ngl == 999 || (oldMax > 0 && w_->ui_SETTINGS.ngl == oldMax));
        const bool shouldClamp = (w_->ui_SETTINGS.ngl > maxngl);

        if (w_->ui_maxngl != maxngl)
        {
            w_->ui_maxngl = maxngl;
            if (w_->settings_ui && w_->settings_ui->ngl_slider)
            {
                const int curMax = w_->settings_ui->ngl_slider->maximum();
                if (curMax != maxngl) w_->settings_ui->ngl_slider->setMaximum(maxngl);
            }
        }

        if (keepAtMaxIntent || shouldClamp)
        {
            w_->ui_SETTINGS.ngl = maxngl;
        }

        if (w_->settings_ui && w_->settings_ui->ngl_slider && w_->settings_ui->ngl_label)
        {
            const int sliderValue = qBound(w_->settings_ui->ngl_slider->minimum(), w_->ui_SETTINGS.ngl, w_->settings_ui->ngl_slider->maximum());
            if (sliderValue != w_->settings_ui->ngl_slider->value()) w_->settings_ui->ngl_slider->setValue(sliderValue);
            w_->settings_ui->ngl_label->setText("gpu " + w_->jtr("offload") + " " + QString::number(sliderValue));
        }
    };

    // 新版：优先从 offloaded A/B 解析 max_ngl
    {
        const QString lower = trimmedLine.toLower();
        int offloaded = 0;
        int total = 0;
        if (lower.contains(QStringLiteral("load_tensors")) && lower.contains(QStringLiteral("offloaded")) &&
            lower.contains(QStringLiteral("layers")) && lower.contains(QStringLiteral("gpu")) &&
            TextParse::extractFractionAfterKeyword(lower, QStringLiteral("offloaded"), offloaded, total))
        {
            Q_UNUSED(offloaded);
            applyDetectedMaxNgl(total);
        }
    }

    // 旧版/兜底：解析 n_layer 推导 max_ngl = n_layer + 1
    int layers = 0;
    {
        const QString lower = trimmedLine.toLower();
        const bool looksLikeMetaLine =
            lower.startsWith(QStringLiteral("print_info:")) ||
            lower.startsWith(QStringLiteral("llm_load_print_meta:")) ||
            lower.startsWith(QStringLiteral("llama_model_load:")) ||
            lower.startsWith(QStringLiteral("llama_model_loader:"));

        if (looksLikeMetaLine &&
            TextParse::extractIntAfterKeyword(trimmedLine, QStringLiteral("n_layer"), layers) && layers > 0)
        {
            applyDetectedMaxNgl(layers + 1);
        }
    }

    const QString chatFmt = TextParse::textAfterKeyword(line, QStringLiteral("Chat format:"));
    if (!chatFmt.isEmpty())
    {
        // Reserved for future UI log.
    }

    auto extractPastLikeTokens = [&](const QString &text, int &out) -> bool
    {
        if (TextParse::extractIntAfterKeyword(text, QStringLiteral("n_past"), out)) return true;
        if (TextParse::extractIntAfterKeyword(text, QStringLiteral("n_tokens"), out)) return true;
        return false;
    };

    if (line.contains(QStringLiteral("total time")))
    {
        int totalTokens = 0;
        if (TextParse::extractLastIntBeforeSuffix(line, QStringLiteral("tokens"), totalTokens))
        {
            if (!w_->sawFinalPast_)
            {
                const int totalUsed = qMax(0, totalTokens);
                if (w_->sawPromptPast_)
                {
                    w_->kvStreamedTurn_ = qMax(0, totalUsed - w_->kvUsedBeforeTurn_);
                    w_->kvTokensTurn_ = w_->kvPromptTokensTurn_ + w_->kvStreamedTurn_;
                    w_->kvUsed_ = qMax(0, w_->kvUsedBeforeTurn_ + w_->kvStreamedTurn_);
                }
                else
                {
                    w_->kvUsed_ = totalUsed;
                }
                w_->updateKvBarUi();
            }
        }
    }

    if (line.contains(QStringLiteral("prompt done")))
    {
        int past = 0;
        if (extractPastLikeTokens(line, past))
        {
            const int baseline = qMax(0, past);
            w_->kvUsed_ = baseline;
            w_->kvUsedBeforeTurn_ = baseline;
            w_->kvPromptTokensTurn_ = baseline;
            w_->kvStreamedTurn_ = 0;
            w_->kvTokensTurn_ = w_->kvPromptTokensTurn_ + w_->kvStreamedTurn_;
            w_->sawPromptPast_ = true;
            w_->updateKvBarUi();
            markBackendActivity();
            cancelLazyUnload(QStringLiteral("prompt done"));
        }
    }
    if (line.contains(QStringLiteral("stop processing")))
    {
        int past = 0;
        if (extractPastLikeTokens(line, past))
        {
            const int finalUsed = qMax(0, past);
            w_->kvUsed_ = finalUsed;
            if (w_->sawPromptPast_)
            {
                w_->kvStreamedTurn_ = qMax(0, finalUsed - w_->kvUsedBeforeTurn_);
                w_->kvTokensTurn_ = w_->kvPromptTokensTurn_ + w_->kvStreamedTurn_;
            }
            w_->sawFinalPast_ = true;
            w_->updateKvBarUi();
            markBackendActivity();
            scheduleLazyUnload();
        }
    }
    return false;
}

void BackendCoordinator::onServerStartFailed(const QString &reason)
{
    if (!w_) return;
    w_->backendOnline_ = false;
    w_->setBackendLifecycleState(BackendLifecycleState::Error, reason, WRONG_SIGNAL, false);
    {
        QJsonObject fields;
        fields.insert(QStringLiteral("reason"), reason);
        fields.insert(QStringLiteral("mode"), QStringLiteral("local"));
        w_->recordPerfEvent(QStringLiteral("backend.start_failed"), fields);
    }
    w_->lazyWakeInFlight_ = false;
    w_->applyWakeUiLock(false);
    if (w_->proxyServer_) w_->proxyServer_->setBackendAvailable(false);
    cancelLazyUnload(QStringLiteral("backend start failed"));
    w_->pendingSendAfterWake_ = false;

    Q_UNUSED(reason);
    if (!w_->portFallbackInFlight_ && w_->portConflictDetected_ && !w_->ui_port.isEmpty() && w_->activeServerPort_ == w_->ui_port)
    {
        initiatePortFallback();
        return;
    }
    w_->portConflictDetected_ = false;
    const QString selectedBackend = DeviceManager::userChoice();
    const QString resolvedBackend = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main"));
    const QString attemptedBackend = resolvedBackend.isEmpty() ? selectedBackend : resolvedBackend;
    // 在给出启动失败文案前，先判断是否仍有自动恢复空间，帮助用户理解“系统接下来会自动做什么”。
    const bool win7FallbackPossible = (w_->win7CpuFallbackArmed_ && !w_->win7CpuFallbackTriggered_);
    const bool backendFallbackPossible = !pickNextBackendFallback(attemptedBackend).isEmpty();
    const RecoveryHintAction startFailHint = (win7FallbackPossible || backendFallbackPossible)
                                                 ? RecoveryHintAction::AutoFallback
                                                 : RecoveryHintAction::AdjustDevice;
    if (!attemptedBackend.isEmpty())
    {
        QString statusLine;
        if (!selectedBackend.isEmpty() && selectedBackend != attemptedBackend)
        {
            statusLine = formatEvaErrorWithHint(EvaErrorCode::BeStartFailed,
                                                QStringLiteral("backend start failure (%1 -> %2)")
                                                    .arg(selectedBackend, attemptedBackend),
                                                startFailHint);
        }
        else
        {
            statusLine = formatEvaErrorWithHint(EvaErrorCode::BeStartFailed,
                                                QStringLiteral("backend start failure -> %1").arg(attemptedBackend),
                                                startFailHint);
        }
        w_->reflash_state(statusLine, WRONG_SIGNAL);
    }
    else
    {
        const QString statusLine = formatEvaErrorWithHint(EvaErrorCode::BeStartFailed,
                                                          QStringLiteral("backend start failure"),
                                                          startFailHint);
        w_->reflash_state(statusLine, WRONG_SIGNAL);
    }
    w_->refreshDeviceBackendUI();
    if (w_->decode_pTimer) w_->decode_pTimer->stop();
    w_->decode_fail();

    w_->lastServerRestart_ = false;
    w_->ignoreNextServerStopped_ = true;
    w_->is_load = false;

    w_->is_run = false;
    w_->unlockButtonsAfterError();
    if (triggerWin7CpuFallback(QStringLiteral("start failure")))
    {
        return;
    }
    if (triggerBackendFallback(attemptedBackend, QStringLiteral("start failure")))
    {
        return;
    }
    resetBackendFallbackState(QStringLiteral("start failure"));
    if (w_->ui && w_->ui->set) w_->ui->set->setEnabled(true);
    if (w_->ui && w_->ui->date) w_->ui->date->setEnabled(true);
}

bool BackendCoordinator::shouldArmWin7CpuFallback() const
{
    if (!w_) return false;
    if (DeviceManager::currentOsId() != QStringLiteral("win7")) return false;
    const QStringList available = DeviceManager::availableBackends();
    if (!available.contains(QStringLiteral("cpu-noavx"))) return false;
    const QString choice = DeviceManager::userChoice();
    if (choice == QStringLiteral("cpu-noavx") || choice == QStringLiteral("custom")) return false;
    if (choice == QStringLiteral("cpu")) return true;
    if (choice == QStringLiteral("auto")) return DeviceManager::effectiveBackend() == QStringLiteral("cpu");
    return false;
}

bool BackendCoordinator::triggerWin7CpuFallback(const QString &reasonTag)
{
    if (!w_) return false;
    if (!w_->win7CpuFallbackArmed_ || w_->win7CpuFallbackTriggered_) return false;
    if (DeviceManager::currentOsId() != QStringLiteral("win7")) return false;
    const QStringList available = DeviceManager::availableBackends();
    if (!available.contains(QStringLiteral("cpu-noavx"))) return false;

    w_->win7CpuFallbackArmed_ = false;
    w_->win7CpuFallbackTriggered_ = true;

    const QString fallbackDevice = QStringLiteral("cpu-noavx");
    DeviceManager::setUserChoice(fallbackDevice);
    w_->ui_device_backend = fallbackDevice;
    w_->lastDeviceBeforeCustom_ = fallbackDevice;
    if (w_->settings_ui && w_->settings_ui->device_comboBox)
    {
        int idx = w_->settings_ui->device_comboBox->findText(fallbackDevice);
        if (idx < 0)
        {
            w_->settings_ui->device_comboBox->addItem(fallbackDevice);
            idx = w_->settings_ui->device_comboBox->findText(fallbackDevice);
        }
        if (idx >= 0)
        {
            QSignalBlocker blocker(w_->settings_ui->device_comboBox);
            w_->settings_ui->device_comboBox->setCurrentIndex(idx);
        }
    }
    w_->refreshDeviceBackendUI();
    w_->reflash_state(formatEvaErrorWithHint(EvaErrorCode::BeStartFailed,
                                             QStringLiteral("Win7 cpu backend failed (%1) -> retrying cpu-noavx").arg(reasonTag),
                                             RecoveryHintAction::AutoFallback),
                      WRONG_SIGNAL);
    QTimer::singleShot(0, w_, [this]()
                       { ensureLocalServer(); });
    return true;
}

void BackendCoordinator::resetBackendFallbackState(const QString &reasonTag)
{
    Q_UNUSED(reasonTag);
    if (!w_) return;
    w_->backendFallbackActive_ = false;
    w_->backendFallbackTried_.clear();
}

QString BackendCoordinator::pickNextBackendFallback(const QString &failedBackend) const
{
    if (!w_) return QString();
    const QString failed = failedBackend.trimmed().toLower();
    QStringList order = DeviceManager::preferredOrder();
    const QStringList available = DeviceManager::availableBackends();

    if (available.contains(QStringLiteral("cpu-noavx")) && !order.contains(QStringLiteral("cpu-noavx")))
    {
        order << QStringLiteral("cpu-noavx");
    }

    int startIndex = order.indexOf(failed);
    if (startIndex < 0) startIndex = -1;
    for (int i = startIndex + 1; i < order.size(); ++i)
    {
        const QString candidate = order.at(i);
        if (candidate.isEmpty()) continue;
        if (w_->backendFallbackTried_.contains(candidate)) continue;
        if (!available.contains(candidate)) continue;
        if (DeviceManager::effectiveBackendFor(candidate) != candidate) continue;
        return candidate;
    }
    return QString();
}

bool BackendCoordinator::triggerBackendFallback(const QString &failedBackend, const QString &reasonTag)
{
    if (!w_) return false;
    const QString failed = failedBackend.trimmed().toLower();
    if (failed.isEmpty()) return false;

    const QString choice = DeviceManager::userChoice();
    if (choice == QStringLiteral("custom")) return false;
    if (!DeviceManager::programOverride(QStringLiteral("llama-server-main")).isEmpty()) return false;

    const bool autoMode = (choice == QStringLiteral("auto"));
    const bool failedIsCpu = (failed == QStringLiteral("cpu") || failed == QStringLiteral("cpu-noavx"));
    if (!autoMode && failedIsCpu) return false;

    if (!w_->backendFallbackActive_) w_->backendFallbackTried_.clear();
    w_->backendFallbackActive_ = true;
    if (!w_->backendFallbackTried_.contains(failed)) w_->backendFallbackTried_.append(failed);

    const QString next = pickNextBackendFallback(failed);
    if (next.isEmpty())
    {
        resetBackendFallbackState(QStringLiteral("fallback exhausted"));
        return false;
    }

    DeviceManager::setUserChoice(next);
    w_->ui_device_backend = next;
    w_->lastDeviceBeforeCustom_ = next;
    if (w_->settings_ui && w_->settings_ui->device_comboBox)
    {
        int idx = w_->settings_ui->device_comboBox->findText(next);
        if (idx < 0)
        {
            w_->settings_ui->device_comboBox->addItem(next);
            idx = w_->settings_ui->device_comboBox->findText(next);
        }
        if (idx >= 0)
        {
            QSignalBlocker blocker(w_->settings_ui->device_comboBox);
            w_->settings_ui->device_comboBox->setCurrentIndex(idx);
        }
    }
    w_->refreshDeviceBackendUI();
    w_->reflash_state(formatEvaErrorWithHint(EvaErrorCode::BeStartFailed,
                                             QStringLiteral("backend fallback -> %1 (%2)").arg(next, reasonTag),
                                             RecoveryHintAction::AutoFallback),
                      WRONG_SIGNAL);

    QTimer::singleShot(0, w_, [this]()
                       { ensureLocalServer(); });
    return true;
}
