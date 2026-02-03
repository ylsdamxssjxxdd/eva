#include "widget.h"
#include "ui_widget.h"
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
        // 启动/重启本地 llama-server（内部会根据是否需要重启来切换到“装载中”状态）
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
    d.tool_call_mode = ui_tool_call_mode;
    d.tools = (ui_tool_call_mode == TOOL_CALL_FUNCTION) ? buildFunctionTools() : QJsonArray();
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

void Widget::collectUserInputs(InputPack &pack, bool attachControllerFrame)
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
    if (attachControllerFrame && ui_controller_ischecked)
    {
        // 桌面控制器开启时：为模型附带最新截屏（仅原图，不再附带坐标叠加图）
        const ControllerFrame frame = captureControllerFrame();
        if (!frame.imagePath.isEmpty())
        {
            // 记录“最后一次发给模型的控制器截图”，用于后续将 bbox 等信息叠加后落盘（EVA_TEMP/overlay）
            lastControllerImagePathForModel_ = frame.imagePath;
            pack.images.append(frame.imagePath);
        }
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
            // 附带图片时：只发送图片本体，不再额外插入“图片文件名/尺寸”等元信息文本，避免干扰模型决策。
            QJsonArray locals;
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
                // 按文件后缀选择 MIME，避免固定写死导致部分后端/模型解析异常。
                const QString ext = QFileInfo(imagePath).suffix().toLower();
                QString mimeType = QStringLiteral("image/png");
                if (ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg"))
                    mimeType = QStringLiteral("image/jpeg");
                else if (ext == QStringLiteral("png"))
                    mimeType = QStringLiteral("image/png");
                else if (ext == QStringLiteral("webp"))
                    mimeType = QStringLiteral("image/webp");
                else if (ext == QStringLiteral("gif"))
                    mimeType = QStringLiteral("image/gif");
                const QString base64String = QStringLiteral("data:%1;base64,").arg(mimeType) + base64Data;
                QJsonObject imageObject;
                imageObject["type"] = QStringLiteral("image_url");
                QJsonObject imageUrlObject;
                imageUrlObject["url"] = base64String;
                imageObject["image_url"] = imageUrlObject;
                contentArray.append(imageObject);

                // 历史/本地恢复用：记录图片的本地路径，避免把 base64 落盘到 messages.jsonl 导致文件臃肿。
                // 注意：该字段属于 EVA 的本地扩展字段，发给模型前会在 prompt_builder 中被移除。
                locals.append(QFileInfo(imagePath).absoluteFilePath());
            }
            if (!locals.isEmpty())
            {
                message.insert(QStringLiteral("local_images"), locals);
            }
        }
        message["content"] = contentArray;
        ui_messagesArray.append(message);
        if (history_) history_->appendMessage(message);
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
    // 终端打印即将发送的图片路径，便于调试
    if (!in.images.isEmpty())
    {
        QStringList absPaths;
        for (const QString &p : in.images) absPaths << QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath());
        qInfo().noquote() << "[chat-images]" << absPaths.join(" | ");
    }
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
    if (!in.images.isEmpty())
    {
        // 将图片路径附在用户消息底部，避免与正文交叉输出
        showImages(in.images);
    }
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
    const quint64 guardTurnId = activeTurnId_;
    if (guardTurnId == 0 || tool_result.isEmpty())
    {
        // 若用户在进入 tool-loop 前已 reset（或 tool_result 被清空），则不继续发送。
        return;
    }
    toolInvocationActive_ = false;
    QJsonObject roleMessage;
    roleMessage.insert("role", QStringLiteral("tool"));
    if (ui_tool_call_mode == TOOL_CALL_FUNCTION && !pendingToolCallId_.isEmpty())
    {
        roleMessage.insert(QStringLiteral("tool_call_id"), pendingToolCallId_);
    }
    // controller/monitor 工具返回时附带最新截图：
    // - controller：动作执行后附带新图，便于模型定位下一步
    // - monitor：等待结束后附带新图，形成“监视->判断->再监视/去操作”的观测回路
    ControllerFrame controllerFrame;
    const QString pendingToolName = lastToolPendingName_.trimmed();
    const bool needControllerLikeFrame = (pendingToolName == QStringLiteral("controller") || pendingToolName == QStringLiteral("monitor"));
    if (ui_controller_ischecked && needControllerLikeFrame)
    {
        // 给系统一点时间：
        // 1) controller：让用户看清“即将执行的动作”叠加提示（tool 侧会提前 emit 提示）；
        // 2) monitor：让“等待中倒计时提示”有机会完成一次绘制/隐藏，并让 UI 变化落到屏幕；
        // 3) 等待菜单/弹窗真正渲染到屏幕，再截取屏幕，避免抢在绘制前。
        // 不能用 QThread::msleep() 阻塞 UI 线程，否则叠加层/系统菜单可能根本来不及绘制。
        const int holdMs = (pendingToolName == QStringLiteral("controller")) ? 650 : 80;
        if (holdMs > 0)
        {
            QEventLoop loop;
            QTimer::singleShot(holdMs, &loop, &QEventLoop::quit);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
        controllerFrame = captureControllerFrame();
        if (activeTurnId_ != guardTurnId || activeTurnId_ == 0 || tool_result.isEmpty())
        {
            // capture 期间允许用户 reset；若 turn 已变化/被清空，则放弃本轮 tool-loop。
            return;
        }
        if (!controllerFrame.imagePath.isEmpty())
        {
            // 该截图会在下一轮继续发送给模型：记录下来，便于 controller 工具调用时回溯标注。
            lastControllerImagePathForModel_ = controllerFrame.imagePath;
            // 单独插入一条用户消息携带截图，保持 tool 消息仍为纯文本，避免 role 不兼容导致被丢弃
            QJsonArray screenshotContent;
            QJsonObject textPart;
            textPart["type"] = QStringLiteral("text");
            QString textLabel;
            if (pendingToolName == QStringLiteral("monitor"))
            {
                textLabel = QStringLiteral("monitor screenshot");
            }
            else
            {
                textLabel = jtr("controller") + QStringLiteral(" screenshot");
            }
            if (controllerFrame.cursorX >= 0 && controllerFrame.cursorY >= 0)
            {
                textLabel += QStringLiteral(" (cursor: %1,%2)").arg(controllerFrame.cursorX).arg(controllerFrame.cursorY);
            }
            textPart["text"] = textLabel;
            screenshotContent.append(textPart);

            QFile imageFile(controllerFrame.imagePath);
            if (imageFile.open(QIODevice::ReadOnly))
            {
                const QByteArray imageData = imageFile.readAll();
                const QByteArray base64Data = imageData.toBase64();
                const QString base64String = QStringLiteral("data:image/png;base64,") + base64Data;
                QJsonObject imageObject;
                imageObject["type"] = QStringLiteral("image_url");
                QJsonObject imageUrlObject;
                imageUrlObject["url"] = base64String;
                imageObject["image_url"] = imageUrlObject;
                screenshotContent.append(imageObject);
            }

            QJsonObject screenshotMessage;
            screenshotMessage.insert("role", DEFAULT_USER_NAME);
            screenshotMessage.insert("content", screenshotContent);
            // 历史/本地恢复用：记录截图本地路径，避免把 base64 落盘到 messages.jsonl 导致文件臃肿。
            screenshotMessage.insert(QStringLiteral("local_images"),
                                     QJsonArray{QFileInfo(controllerFrame.imagePath).absoluteFilePath()});
            ui_messagesArray.append(screenshotMessage);
            // reflash_state(QStringLiteral("ui:%1 screenshot attached").arg(pendingToolName.isEmpty() ? QStringLiteral("controller") : pendingToolName),
            //               SIGNAL_SIGNAL);
            if (history_ && ui_state == CHAT_STATE)
            {
                history_->appendMessage(screenshotMessage);
            }
            // 终端调试输出截图路径，便于排查
            const QString tag = (pendingToolName == QStringLiteral("monitor")) ? QStringLiteral("[monitor-screenshot]")
                                                                              : QStringLiteral("[controller-screenshot]");
            qInfo().noquote() << tag << QDir::toNativeSeparators(controllerFrame.imagePath);
        }
    }

    roleMessage.insert("content", tool_result);
    // 本地扩展字段：tool 名称（仅用于历史/回放恢复记录条图标）；发给模型前会被移除。
    const QString toolNameForHistory = (pendingToolName.isEmpty() ? lastToolCallName_ : pendingToolName).trimmed();
    if (!toolNameForHistory.isEmpty())
    {
        roleMessage.insert(QStringLiteral("tool"), toolNameForHistory);
    }
    // 本地扩展字段：记录工具产物图片路径，历史恢复时用于回显；发给模型前会被移除。
    QJsonArray toolLocals;
    if (!wait_to_show_images_filepath.isEmpty())
    {
        for (const QString &p : wait_to_show_images_filepath)
            toolLocals.append(QFileInfo(p).absoluteFilePath());
    }
    if (!controllerFrame.imagePath.isEmpty())
    {
        toolLocals.append(QFileInfo(controllerFrame.imagePath).absoluteFilePath());
    }
    if (!toolLocals.isEmpty())
    {
        roleMessage.insert(QStringLiteral("local_images"), toolLocals);
    }
    ui_messagesArray.append(roleMessage);
    if (history_ && ui_state == CHAT_STATE) history_->appendMessage(roleMessage);
    if (ui_tool_call_mode == TOOL_CALL_FUNCTION) pendingToolCallId_.clear();

    // 记录区：工具触发时就创建记录块（见 recv_pushover()），这里复用该记录块写入 tool_result；
    // 若没有可复用的记录（兼容旧流程/异常分支），则退回到“收到结果才创建”的旧逻辑。
    int __idx = currentToolRecordIndex_;
    const QString expectedToolName = pendingToolName.isEmpty() ? lastToolCallName_ : pendingToolName;
    const bool canReuse = (__idx >= 0 && __idx < recordEntries_.size() && recordEntries_[__idx].role == RecordRole::Tool &&
                           (expectedToolName.isEmpty() || recordEntries_[__idx].toolName == expectedToolName));
    if (!canReuse)
    {
        // Create record BEFORE printing header/content so docFrom anchors at header area
        __idx = recordCreate(RecordRole::Tool, pendingToolName);
    }
    appendRoleHeader(QStringLiteral("tool"));
    reflash_output(tool_result, 0, themeStateColor(TOOL_SIGNAL));
    recordAppendText(__idx, tool_result);
    if (!ui_messagesArray.isEmpty()) { recordEntries_[__idx].msgIndex = ui_messagesArray.size() - 1; }

    logFlow(FlowPhase::ToolResult,
            QStringLiteral("observation len=%1 pending_images=%2").arg(tool_result.size()).arg(wait_to_show_images_filepath.size()),
            SIGNAL_SIGNAL);
    pendingAssistantHeaderReset_ = true;

    tool_result = "";
    currentToolRecordIndex_ = -1;
    lastToolPendingName_.clear();
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
                            : (task == ConversationTask::Compaction) ? QStringLiteral("compaction")
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
        reflash_output_tool_highlight(systemText, themeTextPrimary());
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

//------------------------------------------------------------------------------
// 上下文压缩（Compaction）：触发判定 + 压缩请求 + 摘要落盘
//------------------------------------------------------------------------------
bool Widget::shouldTriggerCompaction() const
{
    if (!compactionSettings_.enabled) return false;
    if (ui_state != CHAT_STATE) return false;
    if (compactionInFlight_ || compactionQueued_) return false;
    if (engineerProxyRuntime_.active) return false;
    if (ui_messagesArray.isEmpty()) return false;

    const int cap = resolvedContextLimitForUi();
    if (cap <= 0) return false; // 未知上限时不自动压缩
    const int used = qMax(0, kvUsed_);
    if (used <= 0) return false;

    const double ratio = static_cast<double>(used) / static_cast<double>(cap);
    if (ratio >= compactionSettings_.trigger_ratio) return true;
    if (used >= (cap - compactionSettings_.reserve_tokens)) return true;
    return false;
}

bool Widget::startCompactionIfNeeded(const InputPack &pendingInput)
{
    if (!shouldTriggerCompaction()) return false;
    compactionPendingInput_ = pendingInput;
    compactionPendingHasInput_ = true;
    compactionQueued_ = true;
    const int cap = resolvedContextLimitForUi();
    compactionReason_ = QStringLiteral("auto kvUsed=%1 cap=%2").arg(kvUsed_).arg(cap);
    return true;
}

void Widget::startCompactionRun(const QString &reason)
{
    if (compactionInFlight_) return;
    if (ui_messagesArray.isEmpty())
    {
        compactionQueued_ = false;
        resumeSendAfterCompaction();
        return;
    }

    // 计算压缩范围：保留 system + 最后 N 条，其余做摘要
    int startIdx = 0;
    if (!ui_messagesArray.isEmpty())
    {
        const QJsonObject first = ui_messagesArray.first().toObject();
        if (first.value(QStringLiteral("role")).toString() == QStringLiteral(DEFAULT_SYSTEM_NAME))
        {
            startIdx = 1;
        }
    }
    const int keepTail = qMax(1, compactionSettings_.keep_last_messages);
    const int total = ui_messagesArray.size();
    const int toIdx = total - keepTail;
    if (toIdx <= startIdx)
    {
        // 无可压缩内容，直接继续发送
        compactionQueued_ = false;
        resumeSendAfterCompaction();
        return;
    }

    compactionFromIndex_ = startIdx;
    compactionToIndex_ = toIdx;
    const QString sourceText = buildCompactionSourceText(startIdx, toIdx);
    if (sourceText.trimmed().isEmpty())
    {
        compactionQueued_ = false;
        resumeSendAfterCompaction();
        return;
    }

    // 准备压缩请求（不启用工具调用，避免进入工具链）
    compactionInFlight_ = true;
    compactionQueued_ = false;
    compactionHeaderPrinted_ = false;
    currentCompactIndex_ = -1;
    temp_assistant_history.clear();
    pendingAssistantHeaderReset_ = true;
    currentThinkIndex_ = -1;
    currentAssistantIndex_ = -1;

    ENDPOINT_DATA data = prepareEndpointData();
    data.date_prompt = QStringLiteral("你是上下文压缩器。请将用户与助手的历史对话压缩成可用于继续对话的摘要。"
                                      "必须保留：重要事实、关键决策、待办事项、角色/项目/时间/数值信息。"
                                      "不要输出多余解释，不要虚构内容。只输出摘要正文。");
    const QString userPrompt = QStringLiteral("以下是待压缩对话片段（role: content）。请输出不超过 %1 字符的摘要：\n\n%2")
                                   .arg(compactionSettings_.max_summary_chars)
                                   .arg(sourceText);
    QJsonArray messages;
    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral(DEFAULT_USER_NAME));
    userMsg.insert(QStringLiteral("content"), userPrompt);
    messages.append(userMsg);
    data.messagesArray = messages;
    data.tool_call_mode = TOOL_CALL_TEXT;
    data.tools = QJsonArray();
    data.temp = compactionSettings_.temp;
    data.n_predict = compactionSettings_.n_predict;
    data.id_slot = -1; // 压缩请求不复用主会话 slot

    reflash_state(QStringLiteral("ui:上下文压缩中... (%1)").arg(reason), EVA_SIGNAL);
    emit ui2net_data(data);
    emit ui2net_push();
}

QString Widget::extractMessageTextForCompaction(const QJsonObject &msg) const
{
    const QJsonValue contentVal = msg.value(QStringLiteral("content"));
    QString text;
    if (contentVal.isString())
    {
        text = contentVal.toString();
    }
    else if (contentVal.isArray())
    {
        const QJsonArray parts = contentVal.toArray();
        for (const QJsonValue &pv : parts)
        {
            if (!pv.isObject()) continue;
            const QJsonObject po = pv.toObject();
            const QString type = po.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("text"))
            {
                text.append(po.value(QStringLiteral("text")).toString());
            }
        }
    }
    if (text.isEmpty()) return QString();

    QString trimmed = text.trimmed();
    if (trimmed.size() > compactionSettings_.max_message_chars)
    {
        trimmed = trimmed.left(compactionSettings_.max_message_chars);
        trimmed.append(QStringLiteral("..."));
    }
    return trimmed;
}

QString Widget::buildCompactionSourceText(int fromIndex, int toIndex) const
{
    if (fromIndex < 0) fromIndex = 0;
    if (toIndex > ui_messagesArray.size()) toIndex = ui_messagesArray.size();
    if (fromIndex >= toIndex) return QString();

    QStringList lines;
    int totalChars = 0;
    for (int i = fromIndex; i < toIndex; ++i)
    {
        const QJsonObject msg = ui_messagesArray.at(i).toObject();
        if (msg.isEmpty()) continue;
        QString role = msg.value(QStringLiteral("role")).toString();
        if (role == QStringLiteral("model")) role = QStringLiteral("assistant");
        if (role == QStringLiteral("assistant")) role = QStringLiteral("assistant");
        if (role == QStringLiteral("tool"))
        {
            const QString toolName = msg.value(QStringLiteral("tool")).toString().trimmed();
            if (!toolName.isEmpty()) role = QStringLiteral("tool:%1").arg(toolName);
        }
        if (role == QStringLiteral("compact")) role = QStringLiteral("summary");

        const QString content = extractMessageTextForCompaction(msg);
        if (content.isEmpty()) continue;
        QString line = QStringLiteral("%1: %2").arg(role, content);

        const int nextTotal = totalChars + line.size();
        if (compactionSettings_.max_source_chars > 0 && nextTotal > compactionSettings_.max_source_chars)
        {
            const int remain = compactionSettings_.max_source_chars - totalChars;
            if (remain <= 0) break;
            line = line.left(remain);
            lines << line;
            totalChars = compactionSettings_.max_source_chars;
            break;
        }
        lines << line;
        totalChars += line.size() + 1;
    }
    return lines.join(QStringLiteral("\n"));
}

bool Widget::appendCompactionSummaryFile(const QJsonObject &summaryObj) const
{
    if (!history_) return false;
    const QString sessionId = history_->sessionId();
    if (sessionId.isEmpty()) return false;
    const QString baseDir = QDir(applicationDirPath).filePath(QStringLiteral("EVA_TEMP/compaction/%1").arg(sessionId));
    QDir().mkpath(baseDir);
    QFile f(QDir(baseDir).filePath(QStringLiteral("summary.jsonl")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return false;
    QJsonDocument doc(summaryObj);
    QByteArray line = doc.toJson(QJsonDocument::Compact);
    line.append('\n');
    f.write(line);
    f.close();
    return true;
}

void Widget::applyCompactionSummary(const QString &summaryText)
{
    // 生成 compact 消息并重建 messagesArray（保留 system + compact + 尾部消息）
    QJsonArray newMessages;
    bool hasSystem = false;
    if (!ui_messagesArray.isEmpty())
    {
        const QJsonObject first = ui_messagesArray.first().toObject();
        if (first.value(QStringLiteral("role")).toString() == QStringLiteral(DEFAULT_SYSTEM_NAME))
        {
            newMessages.append(first);
            hasSystem = true;
        }
    }
    QJsonObject compactMsg;
    compactMsg.insert(QStringLiteral("role"), QStringLiteral("compact"));
    compactMsg.insert(QStringLiteral("content"), summaryText);
    compactMsg.insert(QStringLiteral("range_from"), compactionFromIndex_);
    compactMsg.insert(QStringLiteral("range_to"), compactionToIndex_);
    compactMsg.insert(QStringLiteral("ts"), QDateTime::currentDateTime().toString(Qt::ISODate));
    newMessages.append(compactMsg);

    for (int i = qMax(0, compactionToIndex_); i < ui_messagesArray.size(); ++i)
    {
        newMessages.append(ui_messagesArray.at(i));
    }
    ui_messagesArray = newMessages;

    // 记录条与消息索引已失配，统一清空 msgIndex（避免误编辑）
    for (RecordEntry &entry : recordEntries_)
    {
        entry.msgIndex = -1;
    }
    // 记录 compact 记录块的 msgIndex（若已输出）
    if (currentCompactIndex_ >= 0)
    {
        const int compactMsgIndex = hasSystem ? 1 : 0;
        if (compactMsgIndex >= 0 && compactMsgIndex < ui_messagesArray.size())
        {
            recordEntries_[currentCompactIndex_].msgIndex = compactMsgIndex;
        }
    }

    if (history_ && ui_state == CHAT_STATE)
    {
        history_->rewriteAllMessages(ui_messagesArray);
    }
}

void Widget::handleCompactionReply(const QString &summaryText, const QString &reasoningText)
{
    Q_UNUSED(reasoningText);
    compactionInFlight_ = false;
    compactionHeaderPrinted_ = false;

    QString summary = summaryText;
    summary.replace(QString(DEFAULT_THINK_BEGIN), QString());
    summary.replace(QString(DEFAULT_THINK_END), QString());
    summary = summary.trimmed();
    if (summary.isEmpty())
    {
        summary = QStringLiteral("（压缩结果为空）");
    }
    if (compactionSettings_.max_summary_chars > 0 && summary.size() > compactionSettings_.max_summary_chars)
    {
        summary = summary.left(compactionSettings_.max_summary_chars);
        summary.append(QStringLiteral("..."));
    }

    // 如果压缩过程没有流式输出（或被静默），则此处补一个紫色记录块
    if (currentCompactIndex_ < 0)
    {
        currentCompactIndex_ = appendCompactRecord(summary);
    }
    else
    {
        updateRecordEntryContent(currentCompactIndex_, summary);
    }

    // 写入 compaction 摘要文件（JSONL）
    QJsonObject summaryObj;
    summaryObj.insert(QStringLiteral("role"), QStringLiteral("compact"));
    summaryObj.insert(QStringLiteral("summary"), summary);
    summaryObj.insert(QStringLiteral("range_from"), compactionFromIndex_);
    summaryObj.insert(QStringLiteral("range_to"), compactionToIndex_);
    summaryObj.insert(QStringLiteral("kv_used"), kvUsed_);
    summaryObj.insert(QStringLiteral("ctx_cap"), resolvedContextLimitForUi());
    summaryObj.insert(QStringLiteral("ts"), QDateTime::currentDateTime().toString(Qt::ISODate));
    appendCompactionSummaryFile(summaryObj);

    // 应用压缩结果到会话历史
    applyCompactionSummary(summary);

    // 压缩后建议新 slot 开启，避免 KV 历史残留
    currentSlotId_ = -1;
    kvUsed_ = 0;
    kvUsedBeforeTurn_ = 0;
    kvStreamedTurn_ = 0;
    updateKvBarUi();

    // 清理本轮压缩状态
    compactionFromIndex_ = -1;
    compactionToIndex_ = -1;
    compactionReason_.clear();

    // 继续发送原始用户请求（若存在）
    resumeSendAfterCompaction();
}

void Widget::resumeSendAfterCompaction()
{
    if (!compactionPendingHasInput_)
    {
        normal_finish_pushover();
        return;
    }

    const InputPack input = compactionPendingInput_;
    compactionPendingHasInput_ = false;
    compactionPendingInput_ = InputPack();

    currentTask_ = ConversationTask::ChatReply;
    startTurnFlow(currentTask_, false);
    logCurrentTask(currentTask_);
    ENDPOINT_DATA data = prepareEndpointData();
    handleChatReply(data, input);
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
