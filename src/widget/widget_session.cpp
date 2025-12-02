#include "widget.h"
#include "ui_widget.h"
#include "../utils/flowtracer.h"

#include <doc2md/document_converter.h>
#include <QFile>
#include <QFileInfo>
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
        is_load = false;
        monitor_timer.stop();
        firstAutoNglEvaluated_ = false; // 新模型：允许重新评估一次是否可全量 offload
        // 启动/重启本地llama-server（内部会根据是否需要重启来切换到“装载中”状态）
        ensureLocalServer();
    }
	else if (ret == 2)
	{
		FlowTracer::log(FlowChannel::UI, QStringLiteral("action: choose link mode"), activeTurnId_);
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

void Widget::recv_freeover_loadlater()
{
    gpu_wait_load = true;
    emit gpu_reflash(); // 强制刷新gpu信息
}

void Widget::preLoad()
{
	FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: preload start %1").arg(ui_SETTINGS.modelpath), activeTurnId_);
	is_load = false; // 重置is_load标签
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
    ENDPOINT_DATA d;
    d.date_prompt = ui_DATES.date_prompt;
    d.stopwords = ui_DATES.extra_stop_words;
    d.is_complete_state = (ui_state == COMPLETE_STATE);
    d.temp = ui_SETTINGS.temp;
    d.repeat = ui_SETTINGS.repeat;
    d.top_k = ui_SETTINGS.top_k;
    d.top_p = ui_SETTINGS.hid_top_p;
    d.n_predict = ui_SETTINGS.hid_npredict;
    d.reasoning_effort = sanitizeReasoningEffort(ui_SETTINGS.reasoning_effort);
    d.messagesArray = ui_messagesArray;
    d.id_slot = currentSlotId_;
    d.turn_id = activeTurnId_;
    return d;
}

void Widget::beginSessionIfNeeded()
{
    if (!(history_ && ui_state == CHAT_STATE && history_->sessionId().isEmpty())) return;
    SessionMeta meta;
    meta.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    meta.title = "";
    meta.endpoint = (ui_mode == LINK_MODE) ? (apis.api_endpoint + ((ui_state == CHAT_STATE) ? apis.api_chat_endpoint : apis.api_completion_endpoint))
                                           : formatLocalEndpoint(activeServerHost_, activeServerPort_);
    meta.model = (ui_mode == LINK_MODE) ? apis.api_model : ui_SETTINGS.modelpath;
    meta.system = ui_DATES.date_prompt;
    meta.n_ctx = ui_SETTINGS.nctx;
    meta.slot_id = currentSlotId_;
    meta.startedAt = QDateTime::currentDateTime();
    history_->begin(meta);
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    history_->appendMessage(systemMessage);
}

bool Widget::buildDocumentAttachment(const QString &path, DocumentAttachment &attachment)
{
    const QFileInfo info(path);
    const QString absolutePath = info.exists() ? info.absoluteFilePath() : path;
    const QByteArray encoded = QFile::encodeName(absolutePath);
    if (encoded.isEmpty())
    {
        reflash_state(QStringLiteral("ui:invalid document path -> ") + absolutePath, WRONG_SIGNAL);
        return false;
    }
    const std::string pathStr(encoded.constData(), static_cast<size_t>(encoded.size()));
    const doc2md::ConversionResult result = doc2md::convertFile(pathStr);
    for (const std::string &warn : result.warnings)
    {
        reflash_state(QStringLiteral("[doc2md] %1").arg(QString::fromStdString(warn)), USUAL_SIGNAL);
    }
    if (!result.success)
    {
        reflash_state(QStringLiteral("ui:doc parse failed -> ") + absolutePath, WRONG_SIGNAL);
        return false;
    }
    attachment.path = absolutePath;
    attachment.displayName = info.fileName().isEmpty() ? absolutePath : info.fileName();
    attachment.markdown = QString::fromUtf8(result.markdown.data(), static_cast<int>(result.markdown.size()));
    return true;
}

QString Widget::formatDocumentPayload(const DocumentAttachment &doc) const
{
    QString name = doc.displayName;
    if (name.isEmpty())
    {
        const QFileInfo info(doc.path);
        name = info.fileName().isEmpty() ? doc.path : info.fileName();
    }
    return QStringLiteral("### Document: %1\n%2").arg(name, doc.markdown);
}

QString Widget::describeDocumentList(const QVector<DocumentAttachment> &docs) const
{
    if (docs.isEmpty()) return QString();
    QStringList names;
    names.reserve(docs.size());
    for (const DocumentAttachment &doc : docs)
    {
        QString name = doc.displayName;
        if (name.isEmpty())
        {
            const QFileInfo info(doc.path);
            name = info.fileName().isEmpty() ? doc.path : info.fileName();
        }
        names.append(name);
    }
    return names.join(QStringLiteral(", "));
}

void Widget::collectUserInputs(InputPack &pack)
{
    pack.text.clear();
    // Only collect user text when we are NOT in a tool loop. The current task
    // is already logged by on_send_clicked(); do not log here to avoid
    // duplicate/misleading "current task" lines.
    if (tool_result.isEmpty())
    {
        pack.text = ui->input->textEdit->toPlainText().toUtf8().data();
        ui->input->textEdit->clear();
    }
    pack.images = ui->input->imageFilePaths();
    pack.documents.clear();
    const QStringList documentPaths = ui->input->documentFilePaths();
    if (!documentPaths.isEmpty())
    {
        pack.documents.reserve(documentPaths.size());
        for (const QString &docPath : documentPaths)
        {
            DocumentAttachment attachment;
            if (buildDocumentAttachment(docPath, attachment))
            {
                pack.documents.append(attachment);
            }
        }
    }
    if (ui_mode == LOCAL_MODE && ui_state == CHAT_STATE && !monitorFrames_.isEmpty())
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 cutoff = nowMs - qint64(kMonitorKeepSeconds_) * 1000;
        while (!monitorFrames_.isEmpty() && monitorFrames_.front().tsMs < cutoff)
        {
            const QString old = monitorFrames_.front().path;
            monitorFrames_.pop_front();
            QFile f(old);
            if (f.exists()) f.remove();
        }
        for (const auto &mf : monitorFrames_) pack.images.append(mf.path);
    }
    pack.wavs = ui->input->wavFilePaths();
    ui->input->clearThumbnails();
}

void Widget::handleChatReply(ENDPOINT_DATA &data, const InputPack &in)
{
    markBackendActivity();
    cancelLazyUnload(QStringLiteral("handle chat reply"));

    // user message assembly
    const bool hasMixedContent = !in.images.isEmpty() || !in.documents.isEmpty();
    if (!hasMixedContent)
    {
        QJsonObject roleMessage;
        roleMessage.insert("role", DEFAULT_USER_NAME);
        roleMessage.insert("content", in.text);
        ui_messagesArray.append(roleMessage);
        if (history_) history_->appendMessage(roleMessage);
    }
    else
    {
        QJsonObject message;
        message["role"] = DEFAULT_USER_NAME;
        QJsonArray contentArray;
        if (!in.text.isEmpty())
        {
            QJsonObject textMessage;
            textMessage.insert("type", "text");
            textMessage.insert("text", in.text);
            contentArray.append(textMessage);
        }
        if (!in.documents.isEmpty())
        {
            for (const DocumentAttachment &doc : in.documents)
            {
                if (doc.markdown.isEmpty()) continue;
                QJsonObject docObject;
                docObject["type"] = "text";
                docObject["text"] = formatDocumentPayload(doc);
                contentArray.append(docObject);
            }
        }
        if (!in.images.isEmpty())
        {
            for (const QString &imagePath : in.images)
            {
                QFile imageFile(imagePath);
                if (!imageFile.open(QIODevice::ReadOnly))
                {
                    qDebug() << "Failed to open image file";
                    continue;
                }
                const QByteArray imageData = imageFile.readAll();
                const QByteArray base64Data = imageData.toBase64();
                const QString base64String = QStringLiteral("data:image/jpeg;base64,") + base64Data;
                QJsonObject imageObject;
                imageObject["type"] = QStringLiteral("image_url");
                QJsonObject imageUrlObject;
                imageUrlObject["url"] = base64String;
                imageObject["image_url"] = imageUrlObject;
                contentArray.append(imageObject);
                showImages({imagePath});
            }
        }
        message["content"] = contentArray;
        ui_messagesArray.append(message);
        if (history_)
        {
            QJsonObject histMsg = message; // copy
            if (!in.images.isEmpty())
            {
                QJsonArray locals;
                for (const QString &p : in.images)
                {
                    locals.append(QFileInfo(p).absoluteFilePath());
                }
                if (!locals.isEmpty()) histMsg.insert("local_images", locals);
            }
            history_->appendMessage(histMsg);
        }
    }
    if (!in.wavs.isEmpty())
    {
        QJsonObject message;
        message["role"] = DEFAULT_USER_NAME;
        QJsonArray contentArray;
        for (int i = 0; i < in.wavs.size(); ++i)
        {
            QString filePath = in.wavs[i];
            QFile audioFile(filePath);
            if (!audioFile.open(QIODevice::ReadOnly))
            {
                qDebug() << "Failed to open audio file:" << filePath;
                continue;
            }
            QByteArray audioData = audioFile.readAll();
            QByteArray base64Data = audioData.toBase64();
            QFileInfo fileInfo(filePath);
            QString extension = fileInfo.suffix().toLower();
            QString mimeType = "audio/mpeg";
            if (extension == "wav")
                mimeType = "audio/wav";
            else if (extension == "ogg")
                mimeType = "audio/ogg";
            else if (extension == "flac")
                mimeType = "audio/flac";
            QString base64String = QString("data:%1;base64,").arg(mimeType) + base64Data;
            QJsonObject audioObject;
            audioObject["type"] = "audio_url";
            QJsonObject audioUrlObject;
            audioUrlObject["url"] = base64String;
            audioObject["audio_url"] = audioUrlObject;
            contentArray.append(audioObject);
            showImages({":/logo/wav.png"});
        }
        if (!contentArray.isEmpty())
        {
            message["content"] = contentArray;
            ui_messagesArray.append(message);
            if (history_) history_->appendMessage(message);
        }
    }
    data.messagesArray = ui_messagesArray;
    logFlow(FlowPhase::Build,
            QStringLiteral("chat msgs=%1 img=%2 doc=%3 audio=%4")
                .arg(ui_messagesArray.size())
                .arg(in.images.size())
                .arg(in.documents.size())
                .arg(in.wavs.size()),
            SIGNAL_SIGNAL);
    // Create record BEFORE printing header/content so docFrom anchors at header line
    int __idx = recordCreate(RecordRole::User);
    appendRoleHeader(QStringLiteral("user"));
    QString userDisplayText = in.text;
    if (!in.documents.isEmpty())
    {
        for (const DocumentAttachment &doc : in.documents)
        {
            if (doc.markdown.isEmpty()) continue;
            if (!userDisplayText.isEmpty()) userDisplayText.append(QStringLiteral("\n\n"));
            userDisplayText.append(formatDocumentPayload(doc));
        }
    }
    reflash_output(userDisplayText, 0, themeTextPrimary());
    // After content is printed, update record's text and docTo, and link msgIndex
    recordAppendText(__idx, userDisplayText);
    if (!ui_messagesArray.isEmpty()) { recordEntries_[__idx].msgIndex = ui_messagesArray.size() - 1; }
    data.n_predict = ui_SETTINGS.hid_npredict;
    const QString endpointDisplay = (ui_mode == LINK_MODE)
                                        ? (apis.api_endpoint + apis.api_chat_endpoint)
                                        : formatLocalEndpoint(activeServerHost_, activeServerPort_);
    const QString modelDisplay = (ui_mode == LINK_MODE)
                                     ? apis.api_model
                                     : QFileInfo(ui_SETTINGS.modelpath).fileName();
    logFlow(FlowPhase::NetRequest,
            QStringLiteral("send chat -> %1 model=%2 n_predict=%3")
                .arg(endpointDisplay, modelDisplay, QString::number(data.n_predict)),
            SIGNAL_SIGNAL);
    emit ui2net_data(data);
    emit ui2net_push();
    if (ui_mode == LOCAL_MODE && ui_state == CHAT_STATE && !monitorFrames_.isEmpty()) monitorFrames_.clear();
}

void Widget::handleCompletion(ENDPOINT_DATA &data)
{
    markBackendActivity();
    cancelLazyUnload(QStringLiteral("handle completion"));

    data.input_prompt = ui->output->toPlainText();
    data.n_predict = ui_SETTINGS.hid_npredict;
    logFlow(FlowPhase::Build,
            QStringLiteral("completion prompt_len=%1").arg(data.input_prompt.size()),
            SIGNAL_SIGNAL);
    const QString endpointDisplay = (ui_mode == LINK_MODE)
                                        ? (apis.api_endpoint + apis.api_completion_endpoint)
                                        : formatLocalEndpoint(activeServerHost_, activeServerPort_);
    const QString modelDisplay = (ui_mode == LINK_MODE)
                                     ? apis.api_model
                                     : QFileInfo(ui_SETTINGS.modelpath).fileName();
    logFlow(FlowPhase::NetRequest,
            QStringLiteral("send completion -> %1 model=%2 n_predict=%3")
                .arg(endpointDisplay, modelDisplay, QString::number(data.n_predict)),
            SIGNAL_SIGNAL);
    emit ui2net_data(data);
    emit ui2net_push();
}

void Widget::handleToolLoop(ENDPOINT_DATA &data)
{
    markBackendActivity();
    cancelLazyUnload(QStringLiteral("handle tool loop"));

    Q_UNUSED(data);
    toolInvocationActive_ = false;
    QJsonObject roleMessage;
    roleMessage.insert("role", QStringLiteral("tool"));
    roleMessage.insert("content", tool_result);
    ui_messagesArray.append(roleMessage);
    if (history_ && ui_state == CHAT_STATE)
    {
        // Include any pending image paths produced by tools so we can restore them later
        QJsonObject histMsg = roleMessage;
        if (!wait_to_show_images_filepath.isEmpty())
        {
            QJsonArray locals;
            for (const QString &p : wait_to_show_images_filepath)
                locals.append(QFileInfo(p).absoluteFilePath());
            histMsg.insert("local_images", locals);
        }
        history_->appendMessage(histMsg);
    }

    // Create record BEFORE printing header/content so docFrom anchors at header area
    int __idx = recordCreate(RecordRole::Tool);
    appendRoleHeader(QStringLiteral("tool"));
    reflash_output(tool_result, 0, themeStateColor(TOOL_SIGNAL));
    recordAppendText(__idx, tool_result);
    if (!ui_messagesArray.isEmpty()) { recordEntries_[__idx].msgIndex = ui_messagesArray.size() - 1; }

    logFlow(FlowPhase::ToolResult,
            QStringLiteral("observation len=%1 pending_images=%2").arg(tool_result.size()).arg(wait_to_show_images_filepath.size()),
            SIGNAL_SIGNAL);
    pendingAssistantHeaderReset_ = true;

    tool_result = "";
    QTimer::singleShot(100, this, SLOT(tool_testhandleTimeout()));
    is_run = true;
    ui_state_pushing();
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
    if (activeTurnId_ == 0 || !turnActive_)
    {
        activeTurnId_ = nextTurnId_++;
    }
    turnActive_ = true;
    const QString taskName = (task == ConversationTask::ChatReply)  ? QStringLiteral("chat")
                            : (task == ConversationTask::Completion) ? QStringLiteral("completion")
                                                                     : QStringLiteral("tool_loop");
    const QString modeName = (ui_mode == LINK_MODE) ? QStringLiteral("link") : QStringLiteral("local");
    const QString detail = QStringLiteral("task=%1 mode=%2 tool_cont=%3").arg(taskName, modeName, continuingTool ? QStringLiteral("yes") : QStringLiteral("no"));
    logFlow(FlowPhase::Start, detail, SIGNAL_SIGNAL);
    emit ui2tool_turn(activeTurnId_);
}

void Widget::finishTurnFlow(const QString &reason, bool success)
{
    if (activeTurnId_ == 0) return;
    const QString detail = QStringLiteral("%1 kvUsed=%2").arg(reason).arg(kvUsed_);
    logFlow(FlowPhase::Finish, detail, success ? SIGNAL_SIGNAL : WRONG_SIGNAL);
    turnActive_ = false;
    activeTurnId_ = 0;
}

void Widget::ensureSystemHeader(const QString &systemText)
{
    const bool needRecord = (ui_state == CHAT_STATE);
    const bool engineerProxyWasActive = engineerProxyRuntime_.active;
    // force UI path when printing system header
    engineerProxyRuntime_.active = false;
    // Ensure first message is system
    if (ui_messagesArray.isEmpty() || ui_messagesArray.first().toObject().value(QStringLiteral("role")).toString() != QStringLiteral(DEFAULT_SYSTEM_NAME))
    {
        QJsonObject systemMessage;
        systemMessage.insert(QStringLiteral("role"), DEFAULT_SYSTEM_NAME);
        systemMessage.insert(QStringLiteral("content"), systemText);
        if (ui_messagesArray.isEmpty())
            ui_messagesArray.append(systemMessage);
        else
            ui_messagesArray.replace(0, systemMessage);
    }

    const bool docEmpty = !ui->output->document() || ui->output->document()->isEmpty();
    if (needRecord && (lastSystemRecordIndex_ < 0 || docEmpty))
    {
        const int idx = recordCreate(RecordRole::System);
        appendRoleHeader(QStringLiteral("system"));
        reflash_output(systemText, 0, themeTextPrimary());
        recordAppendText(idx, systemText);
        lastSystemRecordIndex_ = idx;
        if (!ui_messagesArray.isEmpty())
        {
            recordEntries_[idx].msgIndex = 0;
        }
        logFlow(FlowPhase::Build, QStringLiteral("system header inserted"), SIGNAL_SIGNAL);
    }
    engineerProxyRuntime_.active = engineerProxyWasActive;
}
void Widget::logCurrentTask(ConversationTask task)
{
    Q_UNUSED(task);
    // QString name;
    // switch (task)
    // {
    // case ConversationTask::ChatReply: name = QStringLiteral("chat-reply"); break;
    // case ConversationTask::Completion: name = QStringLiteral("completion"); break;
    // case ConversationTask::ToolLoop: name = QStringLiteral("tool-loop"); break;
    // }
    // // Prefer i18n key if present
    // const QString label = jtr("current task");
    // if (!label.isEmpty())
    //     reflash_state(QStringLiteral("ui:") + label + QStringLiteral(" ") + name, SIGNAL_SIGNAL);
    // else
    //     reflash_state(QStringLiteral("ui:current task ") + name, SIGNAL_SIGNAL);
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
        currentTask_ = ConversationTask::ChatReply;
        startTurnFlow(currentTask_, continuingTool);
        logCurrentTask(currentTask_);
        InputPack in;
        collectUserInputs(in);
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
