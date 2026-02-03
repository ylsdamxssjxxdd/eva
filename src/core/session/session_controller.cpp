#include "core/session/session_controller.h"

#include "widget/widget.h"
#include "ui_widget.h"
#include "prompt_builder.h"
#include "utils/flowtracer.h"

#include <doc2md/document_converter.h>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>

namespace
{
QString joinAttachmentNames(const QStringList &paths, int maxCount)
{
    if (paths.isEmpty()) return QString();
    const int limit = qMax(1, maxCount);
    QStringList names;
    names.reserve(qMin(paths.size(), limit) + 1);
    for (int i = 0; i < paths.size() && i < limit; ++i)
    {
        QFileInfo info(paths[i]);
        QString name = info.fileName();
        if (name.isEmpty()) name = paths[i];
        names.append(name);
    }
    if (paths.size() > limit)
    {
        names.append(QStringLiteral("...+%1").arg(paths.size() - limit));
    }
    return names.join(QStringLiteral(", "));
}
}

SessionController::SessionController(Widget *owner)
    : QObject(owner), w_(owner)
{
}

ENDPOINT_DATA SessionController::prepareEndpointData()
{
    ENDPOINT_DATA d;
    d.date_prompt = w_->ui_DATES.date_prompt;
    d.stopwords = w_->ui_DATES.extra_stop_words;
    d.is_complete_state = (w_->ui_state == COMPLETE_STATE);
    d.temp = w_->ui_SETTINGS.temp;
    d.repeat = w_->ui_SETTINGS.repeat;
    d.top_k = w_->ui_SETTINGS.top_k;
    d.top_p = w_->ui_SETTINGS.hid_top_p;
    d.n_predict = w_->ui_SETTINGS.hid_npredict;
    d.reasoning_effort = sanitizeReasoningEffort(w_->ui_SETTINGS.reasoning_effort);
    d.messagesArray = w_->ui_messagesArray;
    d.tool_call_mode = w_->ui_tool_call_mode;
    d.tools = (w_->ui_tool_call_mode == TOOL_CALL_FUNCTION) ? w_->buildFunctionTools() : QJsonArray();
    d.id_slot = w_->currentSlotId_;
    d.turn_id = w_->activeTurnId_;
    return d;
}

void SessionController::beginSessionIfNeeded()
{
    if (!(w_->history_ && w_->ui_state == CHAT_STATE && w_->history_->sessionId().isEmpty()))
        return;
    SessionMeta meta;
    meta.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    meta.title = "";
    meta.endpoint = (w_->ui_mode == LINK_MODE) ? (w_->apis.api_endpoint + ((w_->ui_state == CHAT_STATE) ? w_->apis.api_chat_endpoint : w_->apis.api_completion_endpoint))
                                               : w_->formatLocalEndpoint(w_->activeServerHost_, w_->activeServerPort_);
    meta.model = (w_->ui_mode == LINK_MODE) ? w_->apis.api_model : w_->ui_SETTINGS.modelpath;
    meta.system = w_->ui_DATES.date_prompt;
    meta.n_ctx = w_->ui_SETTINGS.nctx;
    meta.slot_id = w_->currentSlotId_;
    meta.startedAt = QDateTime::currentDateTime();
    w_->history_->begin(meta);
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", w_->ui_DATES.date_prompt);
    w_->history_->appendMessage(systemMessage);
}

bool SessionController::buildDocumentAttachment(const QString &path, DocumentAttachment &attachment)
{
    const QFileInfo info(path);
    const QString absolutePath = info.exists() ? info.absoluteFilePath() : path;
    const QByteArray encoded = QFile::encodeName(absolutePath);
    if (encoded.isEmpty())
    {
        w_->reflash_state(QStringLiteral("ui:invalid document path -> ") + absolutePath, WRONG_SIGNAL);
        return false;
    }
    const std::string pathStr(encoded.constData(), static_cast<size_t>(encoded.size()));
    const doc2md::ConversionResult result = doc2md::convertFile(pathStr);
    for (const std::string &warn : result.warnings)
    {
        w_->reflash_state(QStringLiteral("[doc2md] %1").arg(QString::fromStdString(warn)), USUAL_SIGNAL);
    }
    if (!result.success)
    {
        w_->reflash_state(QStringLiteral("ui:doc parse failed -> ") + absolutePath, WRONG_SIGNAL);
        return false;
    }
    attachment.path = absolutePath;
    attachment.displayName = info.fileName().isEmpty() ? absolutePath : info.fileName();
    attachment.markdown = QString::fromUtf8(result.markdown.data(), static_cast<int>(result.markdown.size()));
    return true;
}

QString SessionController::formatDocumentPayload(const DocumentAttachment &doc) const
{
    QString name = doc.displayName;
    if (name.isEmpty())
    {
        const QFileInfo info(doc.path);
        name = info.fileName().isEmpty() ? doc.path : info.fileName();
    }
    return QStringLiteral("### Document: %1\n%2").arg(name, doc.markdown);
}

QString SessionController::describeDocumentList(const QVector<DocumentAttachment> &docs) const
{
    if (docs.isEmpty())
        return QString();
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

void SessionController::collectUserInputs(InputPack &pack, bool attachControllerFrame)
{
    auto *ui = w_->ui;
    pack.text.clear();
    // Only collect user text when we are NOT in a tool loop. The current task
    // is already logged by on_send_clicked(); do not log here to avoid
    // duplicate/misleading "current task" lines.
    if (w_->tool_result.isEmpty())
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
    if (attachControllerFrame && w_->ui_controller_ischecked)
    {
        // 桌面控制器开启时：为模型附带最新截屏（仅原图，不再附带坐标叠加图）
        const Widget::ControllerFrame frame = w_->captureControllerFrame();
        if (!frame.imagePath.isEmpty())
        {
            // 记录“最后一次发给模型的控制器截图”，用于后续将 bbox 等信息叠加后落盘（EVA_TEMP/overlay）
            w_->lastControllerImagePathForModel_ = frame.imagePath;
            pack.images.append(frame.imagePath);
        }
    }
    pack.wavs = ui->input->wavFilePaths();
    ui->input->clearThumbnails();
}

void SessionController::handleChatReply(ENDPOINT_DATA &data, const InputPack &in)
{
    w_->markBackendActivity();
    w_->cancelLazyUnload(QStringLiteral("handle chat reply"));

    // 记录本轮用户输入对应的第一条消息索引，便于记录条锚点定位
    int firstUserMsgIndex = -1;
    const auto markUserIndex = [&](int idx) {
        if (firstUserMsgIndex < 0) firstUserMsgIndex = idx;
    };

    // 输出区展示文本（用户输入 + 附件摘要）
    QString displayText = in.text;
    QStringList attachmentLines;
    if (!in.documents.isEmpty())
    {
        QString docLabel = w_->describeDocumentList(in.documents);
        if (docLabel.isEmpty()) docLabel = QString::number(in.documents.size());
        attachmentLines.append(QStringLiteral("[DOC] ") + docLabel);
    }
    if (!in.images.isEmpty())
    {
        QString imageLabel = joinAttachmentNames(in.images, 6);
        if (imageLabel.isEmpty()) imageLabel = QString::number(in.images.size());
        attachmentLines.append(QStringLiteral("[IMG] ") + imageLabel);
    }
    if (!in.wavs.isEmpty())
    {
        QString audioLabel = joinAttachmentNames(in.wavs, 6);
        if (audioLabel.isEmpty()) audioLabel = QString::number(in.wavs.size());
        attachmentLines.append(QStringLiteral("[AUDIO] ") + audioLabel);
    }
    if (!attachmentLines.isEmpty())
    {
        if (!displayText.isEmpty()) displayText.append('\n');
        displayText.append(attachmentLines.join(QStringLiteral("\n")));
    }

    // user message assembly
    const bool hasMixedContent = !in.images.isEmpty() || !in.documents.isEmpty();
    if (!hasMixedContent)
    {
        QJsonObject roleMessage;
        roleMessage.insert("role", DEFAULT_USER_NAME);
        roleMessage.insert("content", in.text);
        w_->ui_messagesArray.append(roleMessage);
        markUserIndex(w_->ui_messagesArray.size() - 1);
        if (w_->history_)
            w_->history_->appendMessage(roleMessage);
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
                if (doc.markdown.isEmpty())
                    continue;
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
        w_->ui_messagesArray.append(message);
        markUserIndex(w_->ui_messagesArray.size() - 1);
        if (w_->history_)
            w_->history_->appendMessage(message);
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
        }
        message["content"] = contentArray;
        w_->ui_messagesArray.append(message);
        markUserIndex(w_->ui_messagesArray.size() - 1);
        if (w_->history_)
            w_->history_->appendMessage(message);
    }

    // 输出区：显示用户输入（包含附件摘要），避免只看到模型输出
    if (!displayText.isEmpty() && !w_->engineerProxyRuntime_.active)
    {
        w_->flushPendingStream();
        const int recIdx = w_->recordCreate(Widget::RecordRole::User);
        w_->appendRoleHeader(QStringLiteral("user"));
        w_->reflash_output(displayText, false, w_->textColorForRole(Widget::RecordRole::User));
        w_->recordAppendText(recIdx, displayText);
        if (firstUserMsgIndex >= 0 && firstUserMsgIndex < w_->ui_messagesArray.size())
        {
            w_->recordEntries_[recIdx].msgIndex = firstUserMsgIndex;
        }
    }

    if (!w_->tool_result.isEmpty())
    {
        QJsonObject toolMessage;
        toolMessage["role"] = QStringLiteral("tool");
        toolMessage["content"] = w_->tool_result;
        toolMessage["tool"] = w_->lastToolPendingName_;
        if (!w_->pendingToolCallId_.isEmpty())
        {
            toolMessage.insert(QStringLiteral("tool_call_id"), w_->pendingToolCallId_);
        }
        w_->ui_messagesArray.append(toolMessage);
        if (w_->history_)
            w_->history_->appendMessage(toolMessage);
        w_->tool_result = "";
    }

    // 把 message array 统一按照模型格式重新打包（去掉 UI-only 字段）
    data.messagesArray = promptx::buildOaiChatMessages(w_->ui_messagesArray,
                                                       w_->ui_DATES.date_prompt,
                                                       DEFAULT_SYSTEM_NAME,
                                                       DEFAULT_USER_NAME,
                                                       DEFAULT_MODEL_NAME);

    // 发送
    w_->emit_send(data);
}

void SessionController::handleCompletion(ENDPOINT_DATA &data)
{
    w_->markBackendActivity();
    w_->cancelLazyUnload(QStringLiteral("handle completion"));

    QJsonObject roleMessage;
    roleMessage.insert("role", DEFAULT_USER_NAME);
    roleMessage.insert("content", w_->ui->input->textEdit->toPlainText().toUtf8().data());
    w_->ui->input->textEdit->clear();
    w_->ui_messagesArray.append(roleMessage);

    data.messagesArray = w_->ui_messagesArray;

    w_->emit_send(data);
}

void SessionController::handleToolLoop(ENDPOINT_DATA &data)
{
    w_->markBackendActivity();
    w_->cancelLazyUnload(QStringLiteral("handle tool loop"));

    // 插入 tool 结果作为 tool 消息（文本模式下会在 net 层兼容为 user 前缀）
    QJsonObject toolMessage;
    toolMessage.insert("role", QStringLiteral("tool"));
    toolMessage.insert("content", w_->tool_result);
    const QString toolName = w_->lastToolPendingName_.isEmpty() ? w_->lastToolCallName_ : w_->lastToolPendingName_;
    if (!toolName.isEmpty())
    {
        toolMessage.insert(QStringLiteral("tool"), toolName);
    }
    if (!w_->pendingToolCallId_.isEmpty())
    {
        toolMessage.insert(QStringLiteral("tool_call_id"), w_->pendingToolCallId_);
    }
    w_->ui_messagesArray.append(toolMessage);
    if (w_->history_)
        w_->history_->appendMessage(toolMessage);

    // 输出区：补齐 tool 结果显示（与记录条绑定）
    if (!w_->tool_result.isEmpty() && !w_->engineerProxyRuntime_.active)
    {
        w_->flushPendingStream();
        int recIdx = w_->currentToolRecordIndex_;
        if (recIdx < 0)
        {
            recIdx = w_->recordCreate(Widget::RecordRole::Tool, toolName);
        }
        w_->appendRoleHeader(QStringLiteral("tool"));
        w_->reflash_output(w_->tool_result, false, w_->textColorForRole(Widget::RecordRole::Tool));
        w_->recordAppendText(recIdx, w_->tool_result);
        if (recIdx >= 0)
        {
            w_->recordEntries_[recIdx].msgIndex = w_->ui_messagesArray.size() - 1;
        }
        if (recIdx == w_->currentToolRecordIndex_) w_->currentToolRecordIndex_ = -1;
    }
    w_->tool_result.clear();

    data.messagesArray = promptx::buildOaiChatMessages(w_->ui_messagesArray,
                                                       w_->ui_DATES.date_prompt,
                                                       DEFAULT_SYSTEM_NAME,
                                                       DEFAULT_USER_NAME,
                                                       DEFAULT_MODEL_NAME);

    w_->emit_send(data);
}

void SessionController::logCurrentTask(ConversationTask task)
{
    Q_UNUSED(task);
    // 如需可视化当前任务，可在此处扩展统一输出
}

void SessionController::startTurnFlow(ConversationTask task, bool continuingTool)
{
    if (w_->activeTurnId_ == 0 || !w_->turnActive_)
    {
        w_->activeTurnId_ = w_->nextTurnId_++;
        w_->turnActive_ = true;
    }
    w_->toolInvocationActive_ = false;
    const QString taskName = (task == ConversationTask::ChatReply) ? QStringLiteral("chat")
                               : (task == ConversationTask::Completion) ? QStringLiteral("completion")
                               : (task == ConversationTask::ToolLoop) ? QStringLiteral("tool-loop")
                                                                       : QStringLiteral("compaction");
    const QString modeName = (w_->ui_mode == LINK_MODE) ? QStringLiteral("link") : QStringLiteral("local");
    const QString detail = QStringLiteral("task=%1 mode=%2 tool_cont=%3").arg(taskName, modeName, continuingTool ? QStringLiteral("yes") : QStringLiteral("no"));
    w_->logFlow(FlowPhase::Start, detail, SIGNAL_SIGNAL);
    emit w_->ui2tool_turn(w_->activeTurnId_);
}

void SessionController::finishTurnFlow(const QString &reason, bool success)
{
    if (w_->activeTurnId_ == 0)
        return;
    const QString detail = QStringLiteral("%1 kvUsed=%2").arg(reason).arg(w_->kvUsed_);
    w_->logFlow(FlowPhase::Finish, detail, success ? SIGNAL_SIGNAL : WRONG_SIGNAL);
    w_->turnActive_ = false;
    w_->activeTurnId_ = 0;
}

void SessionController::ensureSystemHeader(const QString &systemText)
{
    const bool needRecord = (w_->ui_state == CHAT_STATE);
    const bool engineerProxyWasActive = w_->engineerProxyRuntime_.active;
    // force UI path when printing system header
    w_->engineerProxyRuntime_.active = false;
    // Ensure first message is system
    if (w_->ui_messagesArray.isEmpty() || w_->ui_messagesArray.first().toObject().value(QStringLiteral("role")).toString() != QStringLiteral(DEFAULT_SYSTEM_NAME))
    {
        QJsonObject systemMessage;
        systemMessage.insert(QStringLiteral("role"), DEFAULT_SYSTEM_NAME);
        systemMessage.insert(QStringLiteral("content"), systemText);
        if (w_->ui_messagesArray.isEmpty())
            w_->ui_messagesArray.append(systemMessage);
        else
            w_->ui_messagesArray.replace(0, systemMessage);
    }

    const bool docEmpty = !w_->ui->output->document() || w_->ui->output->document()->isEmpty();
    if (needRecord && (w_->lastSystemRecordIndex_ < 0 || docEmpty))
    {
        const int idx = w_->recordCreate(Widget::RecordRole::System);
        w_->appendRoleHeader(QStringLiteral("system"));
        w_->reflash_output_tool_highlight(systemText, w_->themeTextPrimary());
        w_->recordAppendText(idx, systemText);
        w_->lastSystemRecordIndex_ = idx;
        if (!w_->ui_messagesArray.isEmpty())
        {
            w_->recordEntries_[idx].msgIndex = 0;
        }
        w_->logFlow(FlowPhase::Build, QStringLiteral("system header inserted"), SIGNAL_SIGNAL);
    }
    w_->engineerProxyRuntime_.active = engineerProxyWasActive;
}

bool SessionController::shouldTriggerCompaction() const
{
    if (!w_->compactionSettings_.enabled)
        return false;
    if (w_->ui_state != CHAT_STATE)
        return false;
    if (w_->compactionInFlight_ || w_->compactionQueued_)
        return false;
    if (w_->engineerProxyRuntime_.active)
        return false;
    if (w_->ui_messagesArray.isEmpty())
        return false;

    const int cap = w_->resolvedContextLimitForUi();
    if (cap <= 0)
        return false; // 未知上限时不自动压缩
    const int used = qMax(0, w_->kvUsed_);
    if (used <= 0)
        return false;

    const double ratio = static_cast<double>(used) / static_cast<double>(cap);
    if (ratio >= w_->compactionSettings_.trigger_ratio)
        return true;
    if (used >= (cap - w_->compactionSettings_.reserve_tokens))
        return true;
    return false;
}

bool SessionController::startCompactionIfNeeded(const InputPack &pendingInput)
{
    if (!shouldTriggerCompaction())
        return false;
    w_->compactionPendingInput_ = pendingInput;
    w_->compactionPendingHasInput_ = true;
    w_->compactionQueued_ = true;
    const int cap = w_->resolvedContextLimitForUi();
    w_->compactionReason_ = QStringLiteral("auto kvUsed=%1 cap=%2").arg(w_->kvUsed_).arg(cap);
    return true;
}

void SessionController::startCompactionRun(const QString &reason)
{
    if (w_->compactionInFlight_)
        return;
    if (w_->ui_messagesArray.isEmpty())
    {
        w_->compactionQueued_ = false;
        resumeSendAfterCompaction();
        return;
    }

    // 计算压缩范围：保留 system + 最后 N 条，其余做摘要
    int startIdx = 0;
    if (!w_->ui_messagesArray.isEmpty())
    {
        const QJsonObject first = w_->ui_messagesArray.first().toObject();
        if (first.value(QStringLiteral("role")).toString() == QStringLiteral(DEFAULT_SYSTEM_NAME))
        {
            startIdx = 1;
        }
    }
    const int keepTail = qMax(1, w_->compactionSettings_.keep_last_messages);
    const int total = w_->ui_messagesArray.size();
    const int toIdx = total - keepTail;
    if (toIdx <= startIdx)
    {
        // 无可压缩内容，直接继续发送
        w_->compactionQueued_ = false;
        resumeSendAfterCompaction();
        return;
    }

    w_->compactionFromIndex_ = startIdx;
    w_->compactionToIndex_ = toIdx;
    const QString sourceText = buildCompactionSourceText(startIdx, toIdx);
    if (sourceText.trimmed().isEmpty())
    {
        w_->compactionQueued_ = false;
        resumeSendAfterCompaction();
        return;
    }

    // 准备压缩请求（不启用工具调用，避免进入工具链）
    w_->compactionInFlight_ = true;
    w_->compactionQueued_ = false;
    w_->compactionHeaderPrinted_ = false;
    w_->currentCompactIndex_ = -1;
    w_->temp_assistant_history.clear();
    w_->pendingAssistantHeaderReset_ = true;
    w_->currentThinkIndex_ = -1;
    w_->currentAssistantIndex_ = -1;

    ENDPOINT_DATA data = prepareEndpointData();
    data.date_prompt = QStringLiteral("你是上下文压缩器。请将用户与助手的历史对话压缩成可用于继续对话的摘要。"
                                      "必须保留：重要事实、关键决策、待办事项、角色/项目/时间/数值信息。"
                                      "不要输出多余解释，不要虚构内容。只输出摘要正文。");
    const QString userPrompt = QStringLiteral("以下是待压缩对话片段（role: content）。请输出不超过 %1 字符的摘要：\n\n%2")
                                   .arg(w_->compactionSettings_.max_summary_chars)
                                   .arg(sourceText);
    QJsonArray messages;
    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral(DEFAULT_USER_NAME));
    userMsg.insert(QStringLiteral("content"), userPrompt);
    messages.append(userMsg);
    data.messagesArray = messages;
    data.tool_call_mode = TOOL_CALL_TEXT;
    data.tools = QJsonArray();
    data.temp = w_->compactionSettings_.temp;
    data.n_predict = w_->compactionSettings_.n_predict;
    data.id_slot = -1; // 压缩请求不复用主会话 slot

    w_->reflash_state(QStringLiteral("ui:上下文压缩中... (%1)").arg(reason), EVA_SIGNAL);
    w_->emit_send(data);
}

QString SessionController::extractMessageTextForCompaction(const QJsonObject &msg) const
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
            if (!pv.isObject())
                continue;
            const QJsonObject po = pv.toObject();
            const QString type = po.value(QStringLiteral("type")).toString();
            if (type == QLatin1String("text"))
            {
                text.append(po.value(QStringLiteral("text")).toString());
            }
        }
    }
    if (text.isEmpty())
        return QString();

    QString trimmed = text.trimmed();
    if (trimmed.size() > w_->compactionSettings_.max_message_chars)
    {
        trimmed = trimmed.left(w_->compactionSettings_.max_message_chars);
        trimmed.append(QStringLiteral("..."));
    }
    return trimmed;
}

QString SessionController::buildCompactionSourceText(int fromIndex, int toIndex) const
{
    if (fromIndex < 0)
        fromIndex = 0;
    if (toIndex > w_->ui_messagesArray.size())
        toIndex = w_->ui_messagesArray.size();
    if (fromIndex >= toIndex)
        return QString();

    QStringList lines;
    int totalChars = 0;
    for (int i = fromIndex; i < toIndex; ++i)
    {
        const QJsonObject msg = w_->ui_messagesArray.at(i).toObject();
        if (msg.isEmpty())
            continue;
        QString role = msg.value(QStringLiteral("role")).toString();
        if (role == QStringLiteral("model"))
            role = QStringLiteral("assistant");
        if (role == QStringLiteral("assistant"))
            role = QStringLiteral("assistant");
        if (role == QStringLiteral("tool"))
        {
            const QString toolName = msg.value(QStringLiteral("tool")).toString().trimmed();
            if (!toolName.isEmpty())
                role = QStringLiteral("tool:%1").arg(toolName);
        }
        if (role == QStringLiteral("compact"))
            role = QStringLiteral("summary");

        const QString content = extractMessageTextForCompaction(msg);
        if (content.isEmpty())
            continue;
        QString line = QStringLiteral("%1: %2").arg(role, content);

        const int nextTotal = totalChars + line.size();
        if (w_->compactionSettings_.max_source_chars > 0 && nextTotal > w_->compactionSettings_.max_source_chars)
        {
            const int remain = w_->compactionSettings_.max_source_chars - totalChars;
            if (remain <= 0)
                break;
            line = line.left(remain);
            lines << line;
            totalChars = w_->compactionSettings_.max_source_chars;
            break;
        }
        lines << line;
        totalChars += line.size() + 1;
    }
    return lines.join(QStringLiteral("\n"));
}

bool SessionController::appendCompactionSummaryFile(const QJsonObject &summaryObj) const
{
    if (!w_->history_)
        return false;
    const QString sessionId = w_->history_->sessionId();
    if (sessionId.isEmpty())
        return false;
    const QString baseDir = QDir(w_->applicationDirPath).filePath(QStringLiteral("EVA_TEMP/compaction/%1").arg(sessionId));
    QDir().mkpath(baseDir);
    QFile f(QDir(baseDir).filePath(QStringLiteral("summary.jsonl")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return false;
    QJsonDocument doc(summaryObj);
    QByteArray line = doc.toJson(QJsonDocument::Compact);
    line.append('\n');
    f.write(line);
    f.close();
    return true;
}

void SessionController::applyCompactionSummary(const QString &summaryText)
{
    // 生成 compact 消息并重建 messagesArray（保留 system + compact + 尾部消息）
    QJsonArray newMessages;
    bool hasSystem = false;
    if (!w_->ui_messagesArray.isEmpty())
    {
        const QJsonObject first = w_->ui_messagesArray.first().toObject();
        if (first.value(QStringLiteral("role")).toString() == QStringLiteral(DEFAULT_SYSTEM_NAME))
        {
            newMessages.append(first);
            hasSystem = true;
        }
    }
    QJsonObject compactMsg;
    compactMsg.insert(QStringLiteral("role"), QStringLiteral("compact"));
    compactMsg.insert(QStringLiteral("content"), summaryText);
    compactMsg.insert(QStringLiteral("range_from"), w_->compactionFromIndex_);
    compactMsg.insert(QStringLiteral("range_to"), w_->compactionToIndex_);
    compactMsg.insert(QStringLiteral("ts"), QDateTime::currentDateTime().toString(Qt::ISODate));
    newMessages.append(compactMsg);

    for (int i = qMax(0, w_->compactionToIndex_); i < w_->ui_messagesArray.size(); ++i)
    {
        newMessages.append(w_->ui_messagesArray.at(i));
    }
    w_->ui_messagesArray = newMessages;

    // 记录条与消息索引已失配，统一清空 msgIndex（避免误编辑）
    for (Widget::RecordEntry &entry : w_->recordEntries_)
    {
        entry.msgIndex = -1;
    }
    // 记录 compact 记录块的 msgIndex（若已输出）
    if (w_->currentCompactIndex_ >= 0)
    {
        const int compactMsgIndex = hasSystem ? 1 : 0;
        if (compactMsgIndex >= 0 && compactMsgIndex < w_->ui_messagesArray.size())
        {
            w_->recordEntries_[w_->currentCompactIndex_].msgIndex = compactMsgIndex;
        }
    }

    if (w_->history_ && w_->ui_state == CHAT_STATE)
    {
        w_->history_->rewriteAllMessages(w_->ui_messagesArray);
    }
}

void SessionController::handleCompactionReply(const QString &summaryText, const QString &reasoningText)
{
    Q_UNUSED(reasoningText);
    w_->compactionInFlight_ = false;
    w_->compactionHeaderPrinted_ = false;

    QString summary = summaryText;
    summary.replace(QString(DEFAULT_THINK_BEGIN), QString());
    summary.replace(QString(DEFAULT_THINK_END), QString());
    summary = summary.trimmed();
    if (summary.isEmpty())
    {
        summary = QStringLiteral("（压缩结果为空）");
    }
    if (w_->compactionSettings_.max_summary_chars > 0 && summary.size() > w_->compactionSettings_.max_summary_chars)
    {
        summary = summary.left(w_->compactionSettings_.max_summary_chars);
        summary.append(QStringLiteral("..."));
    }

    // 如果压缩过程没有流式输出（或被静默），则此处补一个紫色记录块
    if (w_->currentCompactIndex_ < 0)
    {
        w_->currentCompactIndex_ = w_->appendCompactRecord(summary);
    }
    else
    {
        w_->updateRecordEntryContent(w_->currentCompactIndex_, summary);
    }

    // 写入 compaction 摘要文件（JSONL）
    QJsonObject summaryObj;
    summaryObj.insert(QStringLiteral("role"), QStringLiteral("compact"));
    summaryObj.insert(QStringLiteral("summary"), summary);
    summaryObj.insert(QStringLiteral("range_from"), w_->compactionFromIndex_);
    summaryObj.insert(QStringLiteral("range_to"), w_->compactionToIndex_);
    summaryObj.insert(QStringLiteral("kv_used"), w_->kvUsed_);
    summaryObj.insert(QStringLiteral("ctx_cap"), w_->resolvedContextLimitForUi());
    summaryObj.insert(QStringLiteral("ts"), QDateTime::currentDateTime().toString(Qt::ISODate));
    appendCompactionSummaryFile(summaryObj);

    // 应用压缩结果到会话历史
    applyCompactionSummary(summary);

    // 压缩后建议新 slot 开启，避免 KV 历史残留
    w_->currentSlotId_ = -1;
    w_->kvUsed_ = 0;
    w_->kvUsedBeforeTurn_ = 0;
    w_->kvStreamedTurn_ = 0;
    w_->updateKvBarUi();

    // 清理本轮压缩状态
    w_->compactionFromIndex_ = -1;
    w_->compactionToIndex_ = -1;
    w_->compactionReason_.clear();

    // 继续发送原始用户请求（若存在）
    resumeSendAfterCompaction();
}

void SessionController::resumeSendAfterCompaction()
{
    if (!w_->compactionPendingHasInput_)
    {
        w_->normal_finish_pushover();
        return;
    }

    const InputPack input = w_->compactionPendingInput_;
    w_->compactionPendingHasInput_ = false;
    w_->compactionPendingInput_ = InputPack();

    w_->currentTask_ = ConversationTask::ChatReply;
    startTurnFlow(w_->currentTask_, false);
    logCurrentTask(w_->currentTask_);
    ENDPOINT_DATA data = prepareEndpointData();
    handleChatReply(data, input);
}
