#include "widget.h"
#include "ui_widget.h"
#include "core/session/session_controller.h"
#include "../utils/flowtracer.h"

#include <doc2md/document_converter.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QDateTime>
#include <QEventLoop>
#include <string>

void Widget::on_load_clicked()
{
    FlowTracer::log(FlowChannel::UI, QStringLiteral("action: load clicked"), activeTurnId_);
    // reflash_state("ui:" + jtr("clicked load"), SIGNAL_SIGNAL);

    // 弹出模式选择对话框：本地模式 或 链接模式（上下结构、紧凑、无“取消”按钮）
    // Build a minimal modal dialog with vertical buttons to satisfy the UI spec.
    QDialog modeDlg(this);
    modeDlg.setModal(true);
    modeDlg.setWindowTitle(jtr("load"));
    // Remove help button, keep close button; do not add a Cancel control.
    modeDlg.setWindowFlags(modeDlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QVBoxLayout *vbox = new QVBoxLayout(&modeDlg);
    vbox->setContentsMargins(12, 12, 12, 12); // compact
    vbox->setSpacing(6);
    QPushButton *localBtn = new QPushButton(jtr("local mode"), &modeDlg);
    QPushButton *linkBtn = new QPushButton(jtr("link mode"), &modeDlg);
    localBtn->setMinimumHeight(50);
    linkBtn->setMinimumHeight(50);
    // Make them expand horizontally but stack vertically
    localBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    linkBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    vbox->addWidget(localBtn);
    vbox->addWidget(linkBtn);
    // Clicking a button ends the dialog with distinct codes
    QObject::connect(localBtn, &QPushButton::clicked, &modeDlg, [&modeDlg]()
                     { modeDlg.done(1); });
    QObject::connect(linkBtn, &QPushButton::clicked, &modeDlg, [&modeDlg]()
                     { modeDlg.done(2); });
    const int ret = modeDlg.exec();

	if (ret == 1)
	{
		FlowTracer::log(FlowChannel::UI, QStringLiteral("action: choose local mode"), activeTurnId_);
        recordPerfEvent(QStringLiteral("ui.load.choose_local"));
		// 用户选择本地模式：选择模型并启动本地 llama-server
        currentpath = customOpenfile(currentpath, jtr("load_button_tooltip"), "(*.bin *.gguf)");
        // 允许选择与上次相同的模型路径以便重新装载（如服务器已停止或需要重试）
        if (currentpath == "")
        {
            return; // 路径未选择或与上次相同
        }
        ui_mode = LOCAL_MODE;      // 本地模式 -> 使用本地llama-server + xNet
        historypath = currentpath; // 记录这个路径
        ui_SETTINGS.modelpath = currentpath;
        ui_SETTINGS.mmprojpath = ""; // 清空mmproj模型路径
        ui_SETTINGS.lorapath = "";   // 清空lora模型路径
        // 自动在同级目录搜索带有 mmproj 关键字的 gguf 视觉模型，存在则设置路径
        const QString autoMmprojPath = autoDetectSiblingMmproj(currentpath);
        if (!autoMmprojPath.isEmpty())
        {
            ui_SETTINGS.mmprojpath = autoMmprojPath;
            reflash_state("ui:" + jtr("load mmproj") + " " + autoMmprojPath, USUAL_SIGNAL);
        }
        if (settings_ui && settings_ui->mmproj_LineEdit)
        {
            settings_ui->mmproj_LineEdit->setText(ui_SETTINGS.mmprojpath);
        }
        is_load = false;

        // 手动装载：用户在“装载→本地模式→选择模型”后，先一律把 ngl 设为 999（尽可能全量 offload）。
        // 目的：即使显存探测（vfree）没有正确获取，也能让用户直接获得 GPU 加速体验。
        // 说明：模型真正可卸载的层数会在装载完成后由后端回传（max_ngl = n_layer + 1），
        //      recv_params() 会把 999 自动修正为真实值，并同步更新滑条与显示（保持当前行为）。
        ui_SETTINGS.ngl = 999;
        gpu_wait_load = false;         // 清理“等待显存信息再重载”的路径，避免把 ngl 覆盖回 0
        firstAutoNglEvaluated_ = true; // 手动装载已明确选择“拉满”，无需再做 vfree 阈值判断

        if (settings_ui && settings_ui->ngl_slider && settings_ui->ngl_label)
        {
            // 装载前先把 UI 置为“拉满”意图；装载后会根据真实 max_ngl 自动收敛。
            settings_ui->ngl_slider->setMaximum(999);
            settings_ui->ngl_slider->setValue(999);
            settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl));
        }

        resetBackendFallbackState(QStringLiteral("manual load"));
        // 用户从“装载”按钮主动触发时，语义是“重新装载当前模型”：
        // 即使参数未变化，也应强制重启并进入装载动画，避免按钮提前解锁。
        ensureLocalServer(false, true);
    }
	else if (ret == 2)
	{
		FlowTracer::log(FlowChannel::UI, QStringLiteral("action: choose link mode"), activeTurnId_);
        recordPerfEvent(QStringLiteral("ui.load.choose_link"));
		// 用户选择链接模式：打开链接设置对话框
        ui_state_info = "ui:" + jtr("clicked") + jtr("link") + jtr("set");
        reflash_state(ui_state_info, SIGNAL_SIGNAL);
        // 预填：优先使用已持久化的链接配置，避免本地模式切换时被覆盖
        {
            QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
            settings.setIniCodec("utf-8");
            const QString ep = settings.value("api_endpoint", apis.api_endpoint).toString();
            const QString key = settings.value("api_key", apis.api_key).toString();
            const QString model = settings.value("api_model", apis.api_model).toString();
            api_endpoint_LineEdit->setText(ep);
            api_key_LineEdit->setText(key);
            api_model_LineEdit->setText(model);
        }
        api_dialog->exec(); // 确定后触发 set_api()
    }
    else
    {
        // 用户关闭对话框（未选择） -> 不做任何事
        return;
    }
}

QString Widget::autoDetectSiblingMmproj(const QString &modelPath) const
{
    // 在选中 gguf 模型后，尝试在同级目录中寻找名称包含 mmproj（不区分大小写）的视觉模型
    const QString trimmed = modelPath.trimmed();
    if (trimmed.isEmpty()) return QString();
    const QFileInfo modelInfo(trimmed);
    if (!modelInfo.exists()) return QString();
    const QDir modelDir = modelInfo.dir();
    if (!modelDir.exists()) return QString();

    const QString modelBaseLower = modelInfo.completeBaseName().toLower();
    const QFileInfoList entries = modelDir.entryInfoList(QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    QFileInfoList candidates;
    for (const QFileInfo &entry : entries)
    {
        if (!entry.isFile()) continue;
        if (entry.suffix().compare(QStringLiteral("gguf"), Qt::CaseInsensitive) != 0) continue;
        if (!entry.fileName().toLower().contains(QStringLiteral("mmproj"))) continue;
        candidates.append(entry);
    }
    if (candidates.isEmpty()) return QString();

    const auto pickBest = [&](const QFileInfo &candidate) -> bool
    {
        const QString baseLower = candidate.completeBaseName().toLower();
        if (baseLower == modelBaseLower) return true;
        if (!modelBaseLower.isEmpty())
        {
            if (baseLower.startsWith(modelBaseLower) || modelBaseLower.startsWith(baseLower)) return true;
            if (baseLower.contains(modelBaseLower)) return true;
        }
        return false;
    };
    for (const QFileInfo &candidate : candidates)
    {
        if (pickBest(candidate)) return candidate.absoluteFilePath();
    }
    return candidates.first().absoluteFilePath();
}

void Widget::recv_freeover_loadlater()
{
    gpu_wait_load = true;
    emit gpu_reflash(); // 强制刷新gpu信息
}

void Widget::preLoad()
{
 	FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: preload start %1").arg(ui_SETTINGS.modelpath), activeTurnId_);
    {
        QJsonObject fields;
        fields.insert(QStringLiteral("model_path"), ui_SETTINGS.modelpath);
        fields.insert(QStringLiteral("mode"), ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local"));
        recordPerfEvent(QStringLiteral("backend.preload"), fields);
    }
 	is_load = false; // 重置is_load标签

    // 重新装载前清空 max_ngl：
    // - 多模态模型可能会在加载主模型与 mmproj/clip 组件时输出多段 n_layer/offload 日志；
    // - 若不清空，旧的 max_ngl 可能与新模型不一致，或在解析过程中被错误值覆盖；
    // - 由 onServerOutput()->processServerOutputLine() 重新识别并回填真实上限。
    ui_maxngl = 0;

    preserveConversationOnNextReady_ = false; // Fresh load starts a new chat
    skipUnlockLoadIntro_ = false;            // Ensure unlockLoad prints system prompt
    flushPendingStream();
    if (ui_state == CHAT_STATE)
    {
        ui->output->clear(); // 清空输出区
    }
    ui->state->clear(); // 清空状态区
    // 清空记录条与记录锚点，避免重新装载后残留旧节点
    recordClear();
    ui_state_loading(); // 装载中界面状态
    // 开始“装载中”转轮动画并计时（复用解码动画作为统一的简单动画）
    wait_play("load model");
    load_timer.start();
    if (is_config)
    {
        QString relativePath = applicationDirPath + "/EVA_TEMP/eva_config.ini";
        QFileInfo fileInfo(relativePath);
        QString absolutePath = fileInfo.absoluteFilePath();
        is_config = false;
        reflash_state("ui:" + jtr("apply_config_mess") + " " + absolutePath, USUAL_SIGNAL);
    }
    reflash_state("ui:" + jtr("model location") + " " + ui_SETTINGS.modelpath, USUAL_SIGNAL);
}

ENDPOINT_DATA Widget::prepareEndpointData()
{
    return sessionController_->prepareEndpointData();
}

void Widget::beginSessionIfNeeded()
{
    sessionController_->beginSessionIfNeeded();
}

bool Widget::buildDocumentAttachment(const QString &path, DocumentAttachment &attachment)
{
    return sessionController_->buildDocumentAttachment(path, attachment);
}

QString Widget::formatDocumentPayload(const DocumentAttachment &doc) const
{
    return sessionController_->formatDocumentPayload(doc);
}

QString Widget::describeDocumentList(const QVector<DocumentAttachment> &docs) const
{
    return sessionController_->describeDocumentList(docs);
}

void Widget::collectUserInputs(InputPack &pack, bool attachControllerFrame)
{
    sessionController_->collectUserInputs(pack, attachControllerFrame);
}

void Widget::handleChatReply(ENDPOINT_DATA &data, const InputPack &in)
{
    sessionController_->handleChatReply(data, in);
}

void Widget::handleCompletion(ENDPOINT_DATA &data)
{
    sessionController_->handleCompletion(data);
}

void Widget::handleToolLoop(ENDPOINT_DATA &data)
{
    sessionController_->handleToolLoop(data);
}


QString Widget::flowPhaseName(FlowPhase phase) const
{
    switch (phase)
    {
    case FlowPhase::Start: return QStringLiteral("start");
    case FlowPhase::Build: return QStringLiteral("build");
    case FlowPhase::NetRequest: return QStringLiteral("net_req");
    case FlowPhase::Streaming: return QStringLiteral("stream");
    case FlowPhase::NetDone: return QStringLiteral("net_done");
    case FlowPhase::ToolParsed: return QStringLiteral("tool_parse");
    case FlowPhase::ToolStart: return QStringLiteral("tool_start");
    case FlowPhase::ToolResult: return QStringLiteral("tool_result");
    case FlowPhase::ContinueTurn: return QStringLiteral("continue");
    case FlowPhase::Finish: return QStringLiteral("finish");
    case FlowPhase::Cancel: return QStringLiteral("cancel");
    }
    return QStringLiteral("unknown");
}

QString Widget::flowTag(quint64 turnId) const
{
    if (turnId == 0) return QStringLiteral("[turn-]");
    return QStringLiteral("[turn%1]").arg(turnId);
}

void Widget::logFlow(FlowPhase phase, const QString &detail, SIGNAL_STATE state)
{
    const QString line = QStringLiteral("[%1] %2").arg(flowPhaseName(phase), detail);
    FlowTracer::log(FlowChannel::Session, line, activeTurnId_);
    Q_UNUSED(state);
}

void Widget::startTurnFlow(ConversationTask task, bool continuingTool)
{
    sessionController_->startTurnFlow(task, continuingTool);
}



void Widget::finishTurnFlow(const QString &reason, bool success)
{
    sessionController_->finishTurnFlow(reason, success);
}



void Widget::ensureSystemHeader(const QString &systemText)
{
    sessionController_->ensureSystemHeader(systemText);
}



//------------------------------------------------------------------------------
// 上下文压缩（Compaction）：触发判定 + 压缩请求 + 摘要落盘
//------------------------------------------------------------------------------
bool Widget::shouldTriggerCompaction() const
{
    return sessionController_->shouldTriggerCompaction();
}



bool Widget::startCompactionIfNeeded(const InputPack &pendingInput)
{
    return sessionController_->startCompactionIfNeeded(pendingInput);
}



void Widget::startCompactionRun(const QString &reason)
{
    sessionController_->startCompactionRun(reason);
}



QString Widget::extractMessageTextForCompaction(const QJsonObject &msg) const
{
    return sessionController_->extractMessageTextForCompaction(msg);
}



QString Widget::buildCompactionSourceText(int fromIndex, int toIndex) const
{
    return sessionController_->buildCompactionSourceText(fromIndex, toIndex);
}



bool Widget::appendCompactionSummaryFile(const QJsonObject &summaryObj) const
{
    return sessionController_->appendCompactionSummaryFile(summaryObj);
}



void Widget::applyCompactionSummary(const QString &summaryText)
{
    sessionController_->applyCompactionSummary(summaryText);
}



void Widget::handleCompactionReply(const QString &summaryText, const QString &reasoningText)
{
    sessionController_->handleCompactionReply(summaryText, reasoningText);
}



void Widget::resumeSendAfterCompaction()
{
    sessionController_->resumeSendAfterCompaction();
}


void Widget::logCurrentTask(ConversationTask task)
{
    sessionController_->logCurrentTask(task);
}



void Widget::on_send_clicked()
{
	FlowTracer::log(FlowChannel::Session,
	                QStringLiteral("action: send clicked mode=%1 state=%2 tool_cont=%3")
	                    .arg(ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local"))
	                    .arg(ui_state == CHAT_STATE ? QStringLiteral("chat") : QStringLiteral("complete"))
	                    .arg(tool_result.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")),
	                activeTurnId_);
    if (linkProfile_ == LinkProfile::Control && !isControllerActive())
    {
        reflash_state(jtr("control disconnected"), WRONG_SIGNAL);
        return;
    }
    if (isControllerActive())
    {
        const QString text = ui && ui->input && ui->input->textEdit ? ui->input->textEdit->toPlainText() : QString();
        if (text.trimmed().isEmpty())
        {
            reflash_state(jtr("control send missing"), WRONG_SIGNAL);
            return;
        }
        if (controlChannel_)
        {
            QJsonObject cmd;
            cmd.insert(QStringLiteral("type"), QStringLiteral("command"));
            cmd.insert(QStringLiteral("name"), QStringLiteral("send"));
            cmd.insert(QStringLiteral("text"), text);
            controlChannel_->sendToHost(cmd);
        }
        if (ui && ui->input && ui->input->textEdit) ui->input->textEdit->clear();
        return;
    }
	const bool continuingTool = !tool_result.isEmpty();
    if (ui_mode == LOCAL_MODE)
    {
        const bool serverRunning = serverManager && serverManager->isRunning();
        const bool backendReady = serverRunning && backendOnline_ && !lazyUnloaded_ && !lazyWakeInFlight_;
        if (!backendReady)
        {
            if (!pendingSendAfterWake_)
            {
                pendingSendAfterWake_ = true;
                reflash_state("ui:" + jtr("pop wake hint"), SIGNAL_SIGNAL);
            }
            if (serverManager && !lazyWakeInFlight_)
            {
                ensureLocalServer(true);
            }
            return;
        }
    }
    pendingSendAfterWake_ = false;

    // Reset headers and kv tracker
    pendingAssistantHeaderReset_ = false;
    flushPendingStream();
    turnThinkActive_ = false;
    turnThinkHeaderPrinted_ = false;
    turnAssistantHeaderPrinted_ = false;
    sawPromptPast_ = false;
    sawFinalPast_ = false;
    // reflash_state("ui:" + jtr("clicked send"), SIGNAL_SIGNAL);
    kvUsedBeforeTurn_ = kvUsed_;
    cancelLazyUnload(QStringLiteral("user send"));
    markBackendActivity();
    // Fresh turn: clear prompt/output trackers before new network call
    kvTokensTurn_ = 0;
    kvPromptTokensTurn_ = 0;
    kvStreamedTurn_ = 0;
    // if (ui_mode == LINK_MODE)
    // {
    //     reflash_state(QStringLiteral("link:turn begin used_before=%1 total_used=%2")
    //                       .arg(kvUsedBeforeTurn_)
    //                       .arg(kvUsed_));
    // }

    beginTurnPerfSample();
    emit ui2net_stop(0);
    if (ui_state == CHAT_STATE) beginSessionIfNeeded();

    if (!tool_result.isEmpty())
    {
        currentTask_ = ConversationTask::ToolLoop;
        startTurnFlow(currentTask_, true);
        logCurrentTask(currentTask_);
        ENDPOINT_DATA data = prepareEndpointData();
        handleToolLoop(data);
        return;
    }

    if (ui_state == CHAT_STATE)
    {
        const bool controllerToolPending = continuingTool && lastToolCallName_ == QStringLiteral("controller");
        const bool attachControllerFrame = ui_controller_ischecked && (!continuingTool || controllerToolPending);
        InputPack in;
        collectUserInputs(in, attachControllerFrame);
        if (startCompactionIfNeeded(in))
        {
            currentTask_ = ConversationTask::Compaction;
            startTurnFlow(currentTask_, false);
            logCurrentTask(currentTask_);
            startCompactionRun(compactionReason_);
            is_run = true;
            ui_state_pushing();
            return;
        }
        currentTask_ = ConversationTask::ChatReply;
        startTurnFlow(currentTask_, continuingTool);
        logCurrentTask(currentTask_);
        ENDPOINT_DATA data = prepareEndpointData();
        handleChatReply(data, in);
    }
    else
    {
        currentTask_ = ConversationTask::Completion;
        startTurnFlow(currentTask_, continuingTool);
        logCurrentTask(currentTask_);
        ENDPOINT_DATA data = prepareEndpointData();
        handleCompletion(data);
    }

    is_run = true;
    ui_state_pushing();
}
