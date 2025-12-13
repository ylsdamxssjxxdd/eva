#include "widget.h"
#include "ui_widget.h"
#include "../utils/startuplogger.h"
#include "../utils/flowtracer.h"
#include <QTcpServer>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QSignalBlocker>

void Widget::recv_params(MODEL_PARAMS p)
{
    ui_n_ctx_train = p.n_ctx_train;
    settings_ui->nctx_slider->setMaximum(p.n_ctx_train); // 没有拓展4倍,因为批解码时还是会失败
    ui_maxngl = p.max_ngl;                               // gpu负载层数是n_layer+1
    if (settings_ui && settings_ui->ngl_slider)
    {
        settings_ui->ngl_slider->setMaximum(ui_maxngl);
    }
    const bool shouldAdoptMax = (ui_SETTINGS.ngl == 999 && ui_maxngl > 0);
    const bool shouldClamp = (ui_maxngl > 0 && ui_SETTINGS.ngl > ui_maxngl);
    if (shouldAdoptMax || shouldClamp)
    {
        ui_SETTINGS.ngl = ui_maxngl;
    }
    if (settings_ui && settings_ui->ngl_slider)
    {
        const int sliderValue = qBound(settings_ui->ngl_slider->minimum(), ui_SETTINGS.ngl, settings_ui->ngl_slider->maximum());
        if (sliderValue != settings_ui->ngl_slider->value()) settings_ui->ngl_slider->setValue(sliderValue);
        settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(sliderValue));
    } // 确保 UI 展示真实层数
}

void Widget::recv_kv(float percent, int ctx_size)
{
    Q_UNUSED(percent);
    Q_UNUSED(ctx_size);
}

void Widget::recv_gpu_status(float vmem, float vramp, float vcore, float vfree_)
{
    // Controller UI should mirror remote metrics; ignore local probes to prevent jitter.
    if (blockLocalMonitor_)
    {
        return;
    }
    Q_UNUSED(vmem);
    vfree = vfree_; // 剩余显存
    ui->vcore_bar->setValue(vcore);
    // 取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if (is_first_getvram)
    {
        is_first_getvram = false;
        first_vramp = vramp;
    ui->vram_bar->setValue(first_vramp);
    }
    ui->vram_bar->setSecondValue(vramp - first_vramp);

    if (gpu_wait_load)
    {
        gpu_wait_load = false;                       // 以文件体积近似估计显存占用：若模型大小低于当前可用显存的95%，则尝试全量 offload（ngl=999）
        QFileInfo fileInfo(ui_SETTINGS.modelpath);   // 模型文件大小
        QFileInfo fileInfo2(ui_SETTINGS.mmprojpath); // mmproj 文件大小（可为空）
        const int modelsize_MB = fileInfo.size() / 1024 / 1024 + fileInfo2.size() / 1024 / 1024;
        const double limit = 0.95 * vfree; // 95% 当前可用显存
        if (modelsize_MB > 0 && vfree > 0 && modelsize_MB <= limit)
        {
            ui_SETTINGS.ngl = 999; // 初次装载：尽可能全量 offload
        }
        else
        {
            ui_SETTINGS.ngl = 0; // 不足则先走纯CPU/少量 offload
        }
        // 应用新设置并按需重启本地服务
        if (ui_mode == LOCAL_MODE) ensureLocalServer();
    }
    broadcastControlMonitor();
}

void Widget::recv_cpu_status(double cpuload, double memload)
{
    // Controller UI should mirror remote metrics; ignore local probes to prevent jitter.
    if (blockLocalMonitor_)
    {
        return;
    }
    ui->cpu_bar->setValue(cpuload);
    // 取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if (is_first_getmem)
    {
        first_memp = memload;
        ui->mem_bar->setValue(first_memp);
        is_first_getmem = false;
    }
    ui->mem_bar->setSecondValue(memload - first_memp);
    // ui->mem_bar->setValue(physMemUsedPercent-(model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    // ui->mem_bar->setSecondValue((model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    broadcastControlMonitor();
}

void Widget::ensureLocalServer(bool lazyWake)
{
    if (isShuttingDown_)
    {
        FlowTracer::log(FlowChannel::Backend,
                        QStringLiteral("backend: ensureLocalServer skipped (shutting down)"),
                        activeTurnId_);
        return;
    }

    if (!serverManager) return;

    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: ensureLocalServer lazy=%1 mode=%2")
                        .arg(lazyWake ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local")),
                    activeTurnId_);
    StartupLogger::log(QStringLiteral("ensureLocalServer start (lazy=%1)").arg(lazyWake));
    QElapsedTimer ensureTimer;
    ensureTimer.start();

    cancelLazyUnload(QStringLiteral("ensureLocalServer entry"));
    if (lazyWake)
    {
        lazyWakeInFlight_ = true;
        applyWakeUiLock(true);
    }

    if (!firstAutoNglEvaluated_ && !serverManager->isRunning())
    {
        firstAutoNglEvaluated_ = true;
        // Only auto-evaluate ngl on first-run when there is no persisted positive value.
        // If the user had a saved ngl (>0) in the config, respect it and do not override.
        if (ui_SETTINGS.ngl <= 0)
        {
            QFileInfo fileInfo(ui_SETTINGS.modelpath);
            QFileInfo fileInfo2(ui_SETTINGS.mmprojpath);
            const int modelsize_MB = fileInfo.size() / 1024 / 1024 + fileInfo2.size() / 1024 / 1024;
            if (modelsize_MB > 0 && vfree > 0)
            {
                const double limit = 0.95 * vfree;
                if (modelsize_MB <= limit)
                {
                    ui_SETTINGS.ngl = 999; // 初次装载：尽可能全量 offload
                    if (settings_ui && settings_ui->ngl_slider)
                    {
                        settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
                        settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl));
                    }
                }
            }
            else if (modelsize_MB > 0 && vfree <= 0)
            {
                gpu_wait_load = true;
            }
        }
        else
        {
            // Persisted positive ngl present (e.g. saved by user). Respect it and skip auto-evaluation.
        }
    }

    const QString originalUserPort = ui_port.trimmed();
    ui_port = originalUserPort;
    portConflictDetected_ = false;
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

    if (!portFallbackInFlight_)
    {
        forcedPortOverride_.clear();
    }
    if (!forcedPortOverride_.isEmpty())
    {
        chosenPort = forcedPortOverride_;
        appliedFallbackPort = true;
        fallbackReason = PortFallbackReason::Busy;
    }

    if (chosenPort.isEmpty())
    {
        frontendHost = QStringLiteral("127.0.0.1");
        QString fallback = pickFreeTcpPort();
        if (!fallback.isEmpty()) chosenPort = fallback;
        if (chosenPort.isEmpty()) chosenPort = QString(DEFAULT_SERVER_PORT);
        ui_port.clear();
        lastPortConflictPreferred_.clear();
        lastPortConflictFallback_.clear();
        if (settings_ui && settings_ui->port_lineEdit)
        {
            settings_ui->port_lineEdit->setPlaceholderText("blank = localhost only (random port)");
            settings_ui->port_lineEdit->setToolTip(QString());
        }
        reflash_state(QStringLiteral("ui:port cleared -> bind 127.0.0.1:%1").arg(chosenPort), SIGNAL_SIGNAL);
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
            lastPortConflictPreferred_.clear();
            lastPortConflictFallback_.clear();
            reflash_state(QStringLiteral("ui:invalid port -> bind 127.0.0.1:%1").arg(chosenPort), SIGNAL_SIGNAL);
            if (settings_ui && settings_ui->port_lineEdit)
            {
                settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(chosenPort));
                settings_ui->port_lineEdit->setToolTip(jtr("invalid port fallback body").arg(chosenPort));
            }
        }
    }

    if (appliedFallbackPort)
    {
        ui_port = originalUserPort;
        if (settings_ui && settings_ui->port_lineEdit)
        {
            QSignalBlocker blocker(settings_ui->port_lineEdit);
            settings_ui->port_lineEdit->setText(originalUserPort);
        }
        if (fallbackReason == PortFallbackReason::Invalid && settings_ui && settings_ui->port_lineEdit)
        {
            settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(chosenPort));
            settings_ui->port_lineEdit->setToolTip(jtr("invalid port fallback body").arg(chosenPort));
        }
        portFallbackInFlight_ = false;
    }
    else if (!originalUserPort.isEmpty())
    {
        ui_port = originalUserPort;
        if (settings_ui && settings_ui->port_lineEdit)
        {
            settings_ui->port_lineEdit->setPlaceholderText(QString());
            settings_ui->port_lineEdit->setToolTip(QString());
        }
        lastPortConflictPreferred_.clear();
        lastPortConflictFallback_.clear();
    }

    forcedPortOverride_.clear();
    portFallbackInFlight_ = false;

    QString proxyError;
    if (!ensureProxyListening(frontendHost, chosenPort, &proxyError))
    {
        if (!proxyError.isEmpty())
        {
            reflash_state(QStringLiteral("ui:proxy %1").arg(proxyError), WRONG_SIGNAL);
            FlowTracer::log(FlowChannel::Backend,
                            QStringLiteral("backend: proxy listen fail %1:%2 (%3)")
                                .arg(frontendHost, chosenPort, proxyError),
                            activeTurnId_);
        }
        if (!portFallbackInFlight_)
        {
            portConflictDetected_ = true;
            initiatePortFallback();
        }
        lazyWakeInFlight_ = false;
        applyWakeUiLock(false);
        return;
    }

    activeServerHost_ = frontendHost;
    activeServerPort_ = chosenPort;
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: proxy ready front %1:%2")
                        .arg(activeServerHost_, activeServerPort_),
                    activeTurnId_);

    QString backendPort = activeBackendPort_;
    const bool backendRunning = serverManager->isRunning();
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
    activeBackendPort_ = backendPort;
    updateProxyBackend(backendListenHost_, activeBackendPort_);
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: target %1:%2 (proxy %3:%4)")
                        .arg(backendListenHost_, activeBackendPort_, activeServerHost_, activeServerPort_),
                    activeTurnId_);

    serverManager->setSettings(ui_SETTINGS);
    serverManager->setHost(backendListenHost_);
    serverManager->setPort(activeBackendPort_);
    serverManager->setModelPath(ui_SETTINGS.modelpath);
    serverManager->setMmprojPath(ui_SETTINGS.mmprojpath);
    serverManager->setLoraPath(ui_SETTINGS.lorapath);

    lastServerRestart_ = serverManager->needsRestart();
    if (lastServerRestart_)
    {
        win7CpuFallbackArmed_ = shouldArmWin7CpuFallback();
        win7CpuFallbackTriggered_ = false;
    }
    else
    {
        win7CpuFallbackArmed_ = false;
        win7CpuFallbackTriggered_ = false;
    }
    const bool hadOld = backendRunning;
    ignoreNextServerStopped_ = lastServerRestart_ && hadOld;
    if (lastServerRestart_)
    {
        backendOnline_ = false;
        if (proxyServer_) proxyServer_->setBackendAvailable(false);
        if (!lazyWake)
        {
            preLoad();
            emit ui2net_stop(true);
        }
    }

    serverManager->ensureRunning();
    StartupLogger::log(QStringLiteral("ensureLocalServer ensureRunning done (%1 ms)").arg(ensureTimer.elapsed()));
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: ensureRunning issued restart=%1")
                        .arg(lastServerRestart_ ? QStringLiteral("yes") : QStringLiteral("no")),
                    activeTurnId_);

    backendOnline_ = serverManager->isRunning() && !lastServerRestart_;
    if (proxyServer_) proxyServer_->setBackendAvailable(backendOnline_);
    if (!lazyWake && backendOnline_) markBackendActivity();
    if (backendOnline_)
    {
        FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: online"), activeTurnId_);
    }

    if (!lastServerRestart_ && backendRunning)
    {
        lazyWakeInFlight_ = false;
        applyWakeUiLock(false);
    }

    apis.api_endpoint = formatLocalEndpoint(activeServerHost_, activeServerPort_);
    apis.api_key = "";
    apis.api_model = "default";
    apis.is_local_backend = true;
    emit ui2net_apis(apis);
    emit ui2expend_apis(apis);
    emit ui2expend_mode(ui_mode);
    updateLazyCountdownLabel();
}

QString Widget::pickFreeTcpPort(const QHostAddress &addr) const
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

void Widget::announcePortBusy(const QString &requestedPort, const QString &alternativePort)
{
    if (alternativePort.isEmpty())
    {
        reflash_state(jtr("ui port busy none").arg(requestedPort), WRONG_SIGNAL);
        return;
    }

    const bool repeated = (lastPortConflictPreferred_ == requestedPort && lastPortConflictFallback_ == alternativePort);
    lastPortConflictPreferred_ = requestedPort;
    lastPortConflictFallback_ = alternativePort;

    const QString stateLine = jtr("ui port busy switched").arg(requestedPort, alternativePort);
    reflash_state(stateLine, WRONG_SIGNAL);

    const QString dialogTitle = jtr("port conflict title");
    const QString dialogText = jtr("port conflict body").arg(requestedPort, alternativePort);

    if (settings_ui && settings_ui->port_lineEdit)
    {
        settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(alternativePort));
        settings_ui->port_lineEdit->setToolTip(dialogText);
    }

    if (!repeated)
    {
        QTimer::singleShot(0, this, [this, dialogTitle, dialogText]()
                           { QMessageBox::warning(this, dialogTitle, dialogText); });
    }
}

void Widget::initiatePortFallback()
{
    if (portFallbackInFlight_) return;

    const QString preferred = ui_port.trimmed();
    if (preferred.isEmpty()) return;

    QString fallback = pickFreeTcpPort();
    if (fallback.isEmpty() || fallback == preferred)
    {
        reflash_state(jtr("ui port busy none").arg(preferred), WRONG_SIGNAL);
        return;
    }

    portConflictDetected_ = false;
    portFallbackInFlight_ = true;
    forcedPortOverride_ = fallback;

    announcePortBusy(preferred, fallback);

    if (settings_ui && settings_ui->port_lineEdit)
    {
        settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(fallback));
        settings_ui->port_lineEdit->setToolTip(jtr("port conflict body").arg(preferred, fallback));
    }

    if (proxyServer_) proxyServer_->stop();
    if (serverManager)
    {
        ignoreNextServerStopped_ = true;
        serverManager->stopAsync();
    }

    QTimer::singleShot(150, this, [this]()
                       { ensureLocalServer(); });
}

bool Widget::ensureProxyListening(const QString &host, const QString &port, QString *errorMessage)
{
    if (!proxyServer_) return true;
    bool ok = false;
    const quint16 value = port.toUShort(&ok);
    if (!ok || value == 0)
    {
        if (errorMessage) *errorMessage = QStringLiteral("invalid proxy port -> %1").arg(port);
        return false;
    }
    if (proxyServer_->isListening() && proxyServer_->listenHost() == host && proxyServer_->listenPort() == value)
    {
        return true;
    }
    if (!proxyServer_->start(host, value, errorMessage))
    {
        return false;
    }
    return true;
}

QString Widget::formatLocalEndpoint(const QString &host, const QString &port) const
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

void Widget::updateProxyBackend(const QString &backendHost, const QString &backendPort)
{
    if (!proxyServer_) return;
    bool ok = false;
    const quint16 value = backendPort.toUShort(&ok);
    proxyServer_->setBackendEndpoint(backendHost, ok ? value : 0);
    proxyServer_->setBackendAvailable(ok && backendOnline_);
}

void Widget::onProxyWakeRequested()
{
    if (isShuttingDown_) return;
    if (!lazyUnloadEnabled()) return;
    if (lazyWakeInFlight_) return;
    if (backendOnline_ && serverManager && serverManager->isRunning()) return;
    reflash_state(QStringLiteral("ui:proxy wake request"), SIGNAL_SIGNAL);
    ensureLocalServer(true);
}

void Widget::onProxyExternalActivity()
{
    markBackendActivity();
    cancelLazyUnload(QStringLiteral("proxy activity"));
}

void Widget::markBackendActivity()
{
    if (!lazyUnloadEnabled()) return;
    if (!idleSince_.isValid())
        idleSince_.start();
    else
        idleSince_.restart();
    updateLazyCountdownLabel();
}

void Widget::scheduleLazyUnload()
{
    if (!lazyUnloadEnabled()) return;
    if (!serverManager || !serverManager->isRunning()) return;
    if (turnActive_ || toolInvocationActive_) return;
    if (!lazyUnloadTimer_) return;
    lazyUnloaded_ = false;
    if (!idleSince_.isValid())
        idleSince_.start();
    else
        idleSince_.restart();
    // FlowTracer::log(FlowChannel::Backend,
    //                 QStringLiteral("backend: lazy schedule %1ms").arg(lazyUnloadMs_),
    //                 activeTurnId_);
    lazyUnloadTimer_->start(lazyUnloadMs_);
    updateLazyCountdownLabel();
}

void Widget::cancelLazyUnload(const QString &reason)
{
    Q_UNUSED(reason);
    if (!lazyUnloadTimer_) return;
    if (lazyUnloadTimer_->isActive()) lazyUnloadTimer_->stop();
    if (lazyCountdownTimer_ && lazyCountdownTimer_->isActive()) lazyCountdownTimer_->stop();
    lazyUnloaded_ = false;
    idleSince_ = QElapsedTimer();
    // FlowTracer::log(FlowChannel::Backend,
    //                 QStringLiteral("backend: lazy cancel (%1)").arg(reason),
    //                 activeTurnId_);
    updateLazyCountdownLabel();
}

void Widget::performLazyUnload()
{
    performLazyUnloadInternal(false);
}

void Widget::performLazyUnloadInternal(bool forced)
{
    lazyUnloadPreserveState_ = false;
    if (!forced && !lazyUnloadEnabled()) return;
    pendingSendAfterWake_ = false;
    if (lazyUnloadTimer_ && lazyUnloadTimer_->isActive()) lazyUnloadTimer_->stop();
    if (lazyCountdownTimer_ && lazyCountdownTimer_->isActive()) lazyCountdownTimer_->stop();
    if (!forced && (turnActive_ || toolInvocationActive_))
    {
        scheduleLazyUnload();
        updateLazyCountdownLabel();
        return;
    }
    if (forced && turnActive_)
    {
        emit ui2net_stop(true);
        turnActive_ = false;
        is_run = false;
    }
    if (forced && toolInvocationActive_)
    {
        emit ui2tool_cancelActive();
        toolInvocationActive_ = false;
    }
    const bool hasUi = ui && ui->output;
    const bool hasDocument = hasUi && ui->output->document();
    const bool hasRenderedContent = hasDocument && !ui->output->document()->isEmpty();
    const bool hasConversationHistory = !ui_messagesArray.isEmpty();
    const bool preserveAfterWake = (ui_state == CHAT_STATE) && (hasConversationHistory || hasRenderedContent);
    lazyUnloaded_ = true;
    if (preserveAfterWake)
        lazyUnloadPreserveState_ = true;
    preserveConversationOnNextReady_ = preserveAfterWake; // Resume chat log after lazy wake
    lazyWakeInFlight_ = false;
    applyWakeUiLock(false);
    backendOnline_ = false;
    if (proxyServer_) proxyServer_->setBackendAvailable(false);
    reflash_state("ui:" + jtr("auto eject stop backend"), SIGNAL_SIGNAL);
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: lazy unload forced=%1 preserve=%2")
                        .arg(forced ? QStringLiteral("yes") : QStringLiteral("no"))
                        .arg(lazyUnloadPreserveState_ ? QStringLiteral("yes") : QStringLiteral("no")),
                    activeTurnId_);
    suppressStateClearOnStop_ = !forced;
    if (serverManager && serverManager->isRunning())
    {
        serverManager->stopAsync();
    }
    else if (forced)
    {
        suppressStateClearOnStop_ = false;
    }
    idleSince_ = QElapsedTimer();
    updateLazyCountdownLabel();
}

bool Widget::lazyUnloadEnabled() const
{
    return proxyServer_ && lazyUnloadTimer_ && lazyUnloadMs_ > 0 && ui_mode == LOCAL_MODE;
}

void Widget::setLazyCountdownLabelDisplay(const QString &status)
{
    if (!settings_ui || !settings_ui->lazy_timeout_label) return;

    QString display = jtr("pop timeout label");
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
    settings_ui->lazy_timeout_label->setText(display);
}

void Widget::updateLazyCountdownLabel()
{
    if (!settings_ui || !settings_ui->lazy_timeout_label) return;

    QString status;
    if (lazyUnloadMs_ <= 0)
    {
        status = jtr("pop countdown disabled");
    }
    else if (!backendOnline_ || lazyUnloaded_)
    {
        status = jtr("pop countdown popped");
    }
    else if (!lazyUnloadTimer_ || !lazyUnloadTimer_->isActive())
    {
        status = jtr("pop countdown standby");
    }
    else
    {
        int remaining = lazyUnloadTimer_->remainingTime();
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
    settings_ui->lazy_timeout_label->setToolTip(jtr("pop countdown tooltip"));

    if (lazyCountdownTimer_)
    {
        if (lazyUnloadTimer_ && lazyUnloadTimer_->isActive() && lazyUnloadMs_ > 0 && backendOnline_)
        {
            if (!lazyCountdownTimer_->isActive()) lazyCountdownTimer_->start();
        }
        else
        {
            lazyCountdownTimer_->stop();
        }
    }
}

void Widget::onLazyUnloadNowClicked()
{
    if (ui_mode != LOCAL_MODE)
    {
        reflash_state(QStringLiteral("ui:惰性卸载仅在本地模式启用"), WRONG_SIGNAL);
        return;
    }
    if (!serverManager || !serverManager->isRunning())
    {
        reflash_state(QStringLiteral("ui:本地后端已停止"), SIGNAL_SIGNAL);
        cancelLazyUnload(QStringLiteral("manual unload"));
        return;
    }
    reflash_state("ui:" + jtr("pop trigger"), SIGNAL_SIGNAL);
    FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: manual lazy unload"), activeTurnId_);
    performLazyUnloadInternal(true);
}

void Widget::onServerReady(const QString &endpoint)
{
    win7CpuFallbackArmed_ = false;
    win7CpuFallbackTriggered_ = false;
    backendOnline_ = true;
    lazyUnloaded_ = false;
    lazyWakeInFlight_ = false;
    applyWakeUiLock(false);
    cancelLazyUnload(QStringLiteral("backend ready"));
    markBackendActivity();
    updateProxyBackend(backendListenHost_, activeBackendPort_);
    if (proxyServer_) proxyServer_->setBackendAvailable(true);

    scheduleLazyUnload();
    updateLazyCountdownLabel();
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: ready %1 (front %2:%3 backend %4)")
                        .arg(endpoint, activeServerHost_, activeServerPort_, activeBackendPort_),
                    activeTurnId_);

    if (pendingSendAfterWake_)
    {
        pendingSendAfterWake_ = false;
        QTimer::singleShot(0, this, [this]() { on_send_clicked(); });
    }

    // 配置本地端点；统一由动画收尾逻辑 unlockLoad() 设置标题/图标/状态
    const QUrl backendUrl = QUrl::fromUserInput(endpoint);
    if (backendUrl.isValid() && backendUrl.port() > 0)
    {
        activeBackendPort_ = QString::number(backendUrl.port());
        updateProxyBackend(backendListenHost_, activeBackendPort_);
    }
    const QString frontendEndpoint = formatLocalEndpoint(activeServerHost_, activeServerPort_);
    apis.api_endpoint = frontendEndpoint;
    apis.api_key = "";
    apis.api_model = "default";
    apis.is_local_backend = true;
    emit ui2net_apis(apis);
    emit ui2expend_apis(apis);
    emit ui2expend_mode(ui_mode);

    // 完成装载：记录耗时，统一用简单转轮动画（decode_*）收尾，然后解锁 UI
    load_time = load_timer.isValid() ? (load_timer.nsecsElapsed() / 1e9) : 0.0;
    ui_mode = LOCAL_MODE;

    const bool preserveConversation = preserveConversationOnNextReady_;
    preserveConversationOnNextReady_ = false;
    skipUnlockLoadIntro_ = preserveConversation;

    flushPendingStream();
    if (!preserveConversation)
    {
        ui->output->clear();
        recordClear(); // drop any pre-load records (e.g., engineer probe output) before starting a new session
        ui_messagesArray = QJsonArray();
        {
            QJsonObject systemMessage;
            systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
            systemMessage.insert("content", ui_DATES.date_prompt);
            ui_messagesArray.append(systemMessage);
            if (history_ && ui_state == CHAT_STATE)
            {
                SessionMeta meta;
                meta.id = QString::number(QDateTime::currentMSecsSinceEpoch());
                meta.title = "";
                meta.endpoint = frontendEndpoint;
                meta.model = ui_SETTINGS.modelpath;
                meta.system = ui_DATES.date_prompt;
                meta.n_ctx = ui_SETTINGS.nctx;
                meta.slot_id = -1;
                meta.startedAt = QDateTime::currentDateTime();
                history_->begin(meta);
                history_->appendMessage(systemMessage);
                currentSlotId_ = -1;
            }
        }
        bot_predecode_content = ui_DATES.date_prompt; // 使用系统指令作为“预解码内容”展示
    }
    is_load = true;
    // After fresh load, the first "all slots are idle" is an idle baseline -> ignore once
    lastServerRestart_ = false; // 一次重启流程结束
    // Track the backend that actually came up and align UI hints/fallback logic.
    const QString resolvedBackend = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main"));
    const QString previousRuntime = runtimeDeviceBackend_;
    if (!resolvedBackend.isEmpty())
    {
        runtimeDeviceBackend_ = resolvedBackend;
    }
    const bool runtimeChanged = (!resolvedBackend.isEmpty() && resolvedBackend != previousRuntime);
    // Sync settings device combobox with actually resolved backend if user chose an explicit device.
    const QString userSel = DeviceManager::userChoice();
    if (!userSel.isEmpty() && userSel != QLatin1String("auto"))
    {
        if (!resolvedBackend.isEmpty() && resolvedBackend != userSel)
        {
            if (settings_ui && settings_ui->device_comboBox)
            {
                int idx = settings_ui->device_comboBox->findText(resolvedBackend);
                if (idx < 0)
                {
                    settings_ui->device_comboBox->addItem(resolvedBackend);
                    idx = settings_ui->device_comboBox->findText(resolvedBackend);
                }
                if (idx >= 0)
                {
                    settings_ui->device_comboBox->setCurrentIndex(idx);
                }
            }
            ui_device_backend = resolvedBackend;
            DeviceManager::setUserChoice(resolvedBackend);
            auto_save_user(); // persist corrected device selection
            reflash_state(QStringLiteral("ui:device fallback -> ") + resolvedBackend, SIGNAL_SIGNAL);
        }
    }
    else if (userSel == QLatin1String("auto") && runtimeChanged)
    {
        reflash_state(QStringLiteral("ui:device resolved -> %1").arg(resolvedBackend), SIGNAL_SIGNAL);
    }
    refreshDeviceBackendUI();
    // Complete load animation and finalize spinner state.
    decode_finish();
    if (!activeServerPort_.isEmpty())
    {
        QString displayHost = activeServerHost_;
        if (displayHost.isEmpty() || displayHost == QStringLiteral("0.0.0.0"))
        {
            displayHost = QStringLiteral("127.0.0.1");
        }
        const QString url = QStringLiteral("http://%1:%2").arg(displayHost, activeServerPort_);
        QString lanHint;
        if (activeServerHost_ == QStringLiteral("0.0.0.0"))
        {
            const QString lanIp = getFirstNonLoopbackIPv4Address();
            if (!lanIp.isEmpty())
            {
                lanHint = QStringLiteral(" / http://%1:%2").arg(lanIp, activeServerPort_);
            }
        }
        reflash_state("ui:" + jtr("local endpoint ready") + QStringLiteral(" -> %1%2").arg(url, lanHint), SUCCESS_SIGNAL);
    }
    // 直接解锁界面（不再补帧播放复杂装载动画）
    unlockLoad();
    // 刚装载完成：若已设置监视帧率，则启动监视
    updateMonitorTimer();
}

void Widget::recv_embeddingdb_describe(QString describe)
{
    embeddingdb_describe = describe;
}

bool Widget::shouldArmWin7CpuFallback() const
{
    if (DeviceManager::currentOsId() != QStringLiteral("win7")) return false;
    const QStringList available = DeviceManager::availableBackends();
    if (!available.contains(QStringLiteral("cpu-noavx"))) return false;
    const QString choice = DeviceManager::userChoice();
    if (choice == QStringLiteral("cpu-noavx") || choice == QStringLiteral("custom")) return false;
    if (choice == QStringLiteral("cpu")) return true;
    if (choice == QStringLiteral("auto")) return DeviceManager::effectiveBackend() == QStringLiteral("cpu");
    return false;
}

bool Widget::triggerWin7CpuFallback(const QString &reasonTag)
{
    if (!win7CpuFallbackArmed_ || win7CpuFallbackTriggered_) return false;
    if (DeviceManager::currentOsId() != QStringLiteral("win7")) return false;
    const QStringList available = DeviceManager::availableBackends();
    if (!available.contains(QStringLiteral("cpu-noavx"))) return false;

    win7CpuFallbackArmed_ = false;
    win7CpuFallbackTriggered_ = true;

    const QString fallbackDevice = QStringLiteral("cpu-noavx");
    DeviceManager::setUserChoice(fallbackDevice);
    ui_device_backend = fallbackDevice;
    lastDeviceBeforeCustom_ = fallbackDevice;
    if (settings_ui && settings_ui->device_comboBox)
    {
        int idx = settings_ui->device_comboBox->findText(fallbackDevice);
        if (idx < 0)
        {
            settings_ui->device_comboBox->addItem(fallbackDevice);
            idx = settings_ui->device_comboBox->findText(fallbackDevice);
        }
        if (idx >= 0)
        {
            QSignalBlocker blocker(settings_ui->device_comboBox);
            settings_ui->device_comboBox->setCurrentIndex(idx);
        }
    }
    refreshDeviceBackendUI();
    reflash_state(QStringLiteral("ui:Win7 cpu backend failed (%1) -> retrying cpu-noavx").arg(reasonTag), WRONG_SIGNAL);
    QTimer::singleShot(0, this, [this]()
                       { ensureLocalServer(); });
    return true;
}
