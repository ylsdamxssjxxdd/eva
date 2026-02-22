#include "widget.h"
#include "ui_widget.h"
#include "service/backend/backend_coordinator.h"
#include "../utils/perf_metrics.h"
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

void Widget::setBackendLifecycleState(BackendLifecycleState state, const QString &reason, SIGNAL_STATE level, bool emitStateLine)
{
    const BackendLifecycleState oldState = backendLifecycleState_;
    if (oldState == state && reason.isEmpty()) return;

    const auto isTransitionState = [](BackendLifecycleState s)
    {
        return s == BackendLifecycleState::Starting ||
               s == BackendLifecycleState::Restarting ||
               s == BackendLifecycleState::Waking;
    };

    const bool transitionAllowed = isBackendLifecycleTransitionAllowed(oldState, state);
    backendLifecycleState_ = state;
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: lifecycle %1 -> %2 (%3)")
                        .arg(backendLifecycleStateName(oldState),
                             backendLifecycleStateName(state),
                             reason.isEmpty() ? QStringLiteral("-") : reason),
                    activeTurnId_);
    if (!transitionAllowed)
    {
        FlowTracer::log(FlowChannel::Backend,
                        QStringLiteral("backend: lifecycle warning invalid transition %1 -> %2")
                            .arg(backendLifecycleStateName(oldState), backendLifecycleStateName(state)),
                        activeTurnId_);
    }
    {
        QJsonObject fields;
        fields.insert(QStringLiteral("from"), backendLifecycleStateName(oldState));
        fields.insert(QStringLiteral("to"), backendLifecycleStateName(state));
        fields.insert(QStringLiteral("reason"), reason);
        fields.insert(QStringLiteral("allowed"), transitionAllowed);
        fields.insert(QStringLiteral("turn_id"), static_cast<qint64>(activeTurnId_));
        recordPerfEvent(QStringLiteral("backend.lifecycle"), fields);
    }

    // 生命周期阶段耗时：记录 starting/restarting/waking -> terminal(running/error/stopped/sleeping) 的持续时间。
    if (isTransitionState(state))
    {
        if (!backendLifecycleTimerActive_ || !backendLifecycleTimer_.isValid() ||
            !isTransitionState(oldState) || backendLifecycleTimerFrom_ != state)
        {
            backendLifecycleTimerFrom_ = state;
            backendLifecycleTimer_.start();
            backendLifecycleTimerActive_ = true;
        }
    }
    else if (backendLifecycleTimerActive_ && backendLifecycleTimer_.isValid())
    {
        const qint64 elapsedMs = qMax<qint64>(0, backendLifecycleTimer_.elapsed());
        QJsonObject fields;
        fields.insert(QStringLiteral("from"), backendLifecycleStateName(backendLifecycleTimerFrom_));
        fields.insert(QStringLiteral("to"), backendLifecycleStateName(state));
        fields.insert(QStringLiteral("reason"), reason);
        fields.insert(QStringLiteral("allowed"), transitionAllowed);
        fields.insert(QStringLiteral("turn_id"), static_cast<qint64>(activeTurnId_));
        PerfMetrics::recordDuration(applicationDirPath, QStringLiteral("backend.lifecycle.duration"), elapsedMs, fields);
        backendLifecycleTimerActive_ = false;
    }

    if (!emitStateLine) return;
    QString line = QStringLiteral("ui:backend state -> %1").arg(backendLifecycleStateName(state));
    if (!reason.isEmpty()) line += QStringLiteral(" (") + reason + QStringLiteral(")");
    reflash_state(line, level);
}

bool Widget::isBackendLifecycleTransitioning() const
{
    return backendLifecycleState_ == BackendLifecycleState::Starting ||
           backendLifecycleState_ == BackendLifecycleState::Restarting ||
           backendLifecycleState_ == BackendLifecycleState::Waking;
}

void Widget::recordPerfEvent(const QString &eventName, const QJsonObject &fields) const
{
    PerfMetrics::recordEvent(applicationDirPath, eventName, fields);
}

void Widget::beginTurnPerfSample()
{
    turnPerfTimer_.start();
    turnPerfTimerActive_ = true;
    QJsonObject fields;
    fields.insert(QStringLiteral("mode"), ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local"));
    fields.insert(QStringLiteral("state"), ui_state == CHAT_STATE ? QStringLiteral("chat") : QStringLiteral("complete"));
    fields.insert(QStringLiteral("turn_id"), static_cast<qint64>(activeTurnId_));
    recordPerfEvent(QStringLiteral("turn.begin"), fields);
}

void Widget::finishTurnPerfSample(const QString &reason, bool success)
{
    if (!turnPerfTimerActive_ || !turnPerfTimer_.isValid()) return;
    turnPerfTimerActive_ = false;
    const qint64 elapsedMs = qMax<qint64>(0, turnPerfTimer_.elapsed());

    QJsonObject fields;
    fields.insert(QStringLiteral("reason"), reason);
    fields.insert(QStringLiteral("success"), success);
    fields.insert(QStringLiteral("mode"), ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local"));
    fields.insert(QStringLiteral("turn_id"), static_cast<qint64>(activeTurnId_));
    fields.insert(QStringLiteral("kv_used"), kvUsed_);
    fields.insert(QStringLiteral("prompt_tokens"), kvPromptTokensTurn_);
    fields.insert(QStringLiteral("stream_tokens"), kvStreamedTurn_);
    fields.insert(QStringLiteral("reasoning_tokens"), lastReasoningTokens_);
    PerfMetrics::recordDuration(applicationDirPath, QStringLiteral("turn.finish"), elapsedMs, fields);
}

void Widget::ensureLocalServer(bool lazyWake, bool forceReload)
{
    if (backendCoordinator_)
        backendCoordinator_->ensureLocalServer(lazyWake, forceReload);
}



QString Widget::pickFreeTcpPort(const QHostAddress &addr) const
{
    return backendCoordinator_ ? backendCoordinator_->pickFreeTcpPort(addr) : QString();
}



void Widget::announcePortBusy(const QString &requestedPort, const QString &alternativePort)
{
    if (backendCoordinator_)
        backendCoordinator_->announcePortBusy(requestedPort, alternativePort);
}



void Widget::initiatePortFallback()
{
    if (backendCoordinator_)
        backendCoordinator_->initiatePortFallback();
}



bool Widget::ensureProxyListening(const QString &host, const QString &port, QString *errorMessage)
{
    return backendCoordinator_ ? backendCoordinator_->ensureProxyListening(host, port, errorMessage) : false;
}



QString Widget::formatLocalEndpoint(const QString &host, const QString &port) const
{
    return backendCoordinator_ ? backendCoordinator_->formatLocalEndpoint(host, port) : QString();
}



void Widget::updateProxyBackend(const QString &backendHost, const QString &backendPort)
{
    if (backendCoordinator_)
        backendCoordinator_->updateProxyBackend(backendHost, backendPort);
}



void Widget::onProxyWakeRequested()
{
    if (backendCoordinator_)
        backendCoordinator_->onProxyWakeRequested();
}



void Widget::onProxyExternalActivity()
{
    if (backendCoordinator_)
        backendCoordinator_->onProxyExternalActivity();
}



void Widget::markBackendActivity()
{
    if (backendCoordinator_)
        backendCoordinator_->markBackendActivity();
}



void Widget::scheduleLazyUnload()
{
    if (backendCoordinator_)
        backendCoordinator_->scheduleLazyUnload();
}



void Widget::cancelLazyUnload(const QString &reason)
{
    if (backendCoordinator_)
        backendCoordinator_->cancelLazyUnload(reason);
}



void Widget::performLazyUnload()
{
    if (backendCoordinator_)
        backendCoordinator_->performLazyUnload();
}



void Widget::performLazyUnloadInternal(bool forced)
{
    if (backendCoordinator_)
        backendCoordinator_->performLazyUnloadInternal(forced);
}



bool Widget::lazyUnloadEnabled() const
{
    return backendCoordinator_ ? backendCoordinator_->lazyUnloadEnabled() : false;
}



void Widget::setLazyCountdownLabelDisplay(const QString &status)
{
    if (backendCoordinator_)
        backendCoordinator_->setLazyCountdownLabelDisplay(status);
}



void Widget::updateLazyCountdownLabel()
{
    if (backendCoordinator_)
        backendCoordinator_->updateLazyCountdownLabel();
}



void Widget::onLazyUnloadNowClicked()
{
    if (backendCoordinator_)
        backendCoordinator_->onLazyUnloadNowClicked();
}



void Widget::onServerReady(const QString &endpoint)
{
    if (backendCoordinator_)
        backendCoordinator_->onServerReady(endpoint);
}



void Widget::recv_embeddingdb_describe(QString describe)
{
    embeddingdb_describe = describe;
}

bool Widget::shouldArmWin7CpuFallback() const
{
    return backendCoordinator_ ? backendCoordinator_->shouldArmWin7CpuFallback() : false;
}



bool Widget::triggerWin7CpuFallback(const QString &reasonTag)
{
    return backendCoordinator_ ? backendCoordinator_->triggerWin7CpuFallback(reasonTag) : false;
}



void Widget::resetBackendFallbackState(const QString &reasonTag)
{
    if (backendCoordinator_)
        backendCoordinator_->resetBackendFallbackState(reasonTag);
}



QString Widget::pickNextBackendFallback(const QString &failedBackend) const
{
    return backendCoordinator_ ? backendCoordinator_->pickNextBackendFallback(failedBackend) : QString();
}



bool Widget::triggerBackendFallback(const QString &failedBackend, const QString &reasonTag)
{
    return backendCoordinator_ ? backendCoordinator_->triggerBackendFallback(failedBackend, reasonTag) : false;
}


