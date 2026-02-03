#include "core/toolflow/tool_flow_controller.h"

#include "widget/widget.h"
#include "ui_widget.h"
#include "utils/startuplogger.h"
#include "utils/flowtracer.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>

ToolFlowController::ToolFlowController(Widget *owner)
    : QObject(owner), w_(owner)
{
}

void ToolFlowController::recvPushover()
{
    w_->flushPendingStream();
    w_->lastToolCallName_.clear();
    w_->lastToolPendingName_.clear();
    w_->pendingToolCallId_.clear();
    const QJsonArray toolCallsSnapshot = w_->pendingToolCallsPayload_;
    w_->pendingToolCallsPayload_ = QJsonArray();
    // Separate all reasoning (<think>...</think>) blocks from final content; capture both roles
    QString finalText = w_->temp_assistant_history;
    const QString tBegin = QString(DEFAULT_THINK_BEGIN);
    const QString tEnd = QString(DEFAULT_THINK_END);
    QStringList reasonings;
    int searchPos = 0;
    while (true)
    {
        int s = finalText.indexOf(tBegin, searchPos);
        if (s == -1) break;
        int e = finalText.indexOf(tEnd, s + tBegin.size());
        if (e == -1) break; // unmatched tail -> leave as is
        const int rStart = s + tBegin.size();
        reasonings << finalText.mid(rStart, e - rStart);
        // remove the whole <think>...</think> segment from finalText
        finalText.remove(s, (e + tEnd.size()) - s);
        searchPos = s; // continue scanning from removal point
    }
    const QString reasoningText = reasonings.join("");
    // 压缩请求回合：将输出作为摘要处理，避免走常规 assistant/tool 链路
    if (w_->compactionInFlight_)
    {
        w_->handleCompactionReply(finalText, reasoningText);
        w_->temp_assistant_history = "";
        w_->currentThinkIndex_ = -1;
        w_->currentAssistantIndex_ = -1;
        return;
    }
    // 重要：不要在这里打印 assistant_len/reasoning_len（字符长度），它很容易被误解为 token 数。
    // 链接模式下我们只关心 token 口径（来自 usage/timings 的校准结果，以及 UI 侧的 KV 汇总）。
    {
        const int promptTok = qMax(0, w_->kvPromptTokensTurn_);
        const int genTok = qMax(0, w_->kvStreamedTurn_);
        const int reasoningTok = qMax(0, w_->lastReasoningTokens_);
        const int usedTok = qMax(0, w_->kvUsed_);
        const int turnTok = qMax(0, w_->kvTokensTurn_);
        w_->logFlow(FlowPhase::NetDone,
                    QStringLiteral("tokens prompt=%1 gen=%2 reasoning=%3 turn=%4 used=%5")
                        .arg(promptTok)
                        .arg(genTok)
                        .arg(reasoningTok)
                        .arg(turnTok)
                        .arg(usedTok),
                    SIGNAL_SIGNAL);
    }
    // Persist assistant message (with optional reasoning) into UI/history
    QJsonObject roleMessage;
    roleMessage.insert("role", DEFAULT_MODEL_NAME);
    roleMessage.insert("content", finalText);
    if (!reasoningText.isEmpty())
    {
        roleMessage.insert("reasoning_content", reasoningText);
    }
    if (w_->ui_tool_call_mode == TOOL_CALL_FUNCTION && !toolCallsSnapshot.isEmpty())
    {
        roleMessage.insert(QStringLiteral("tool_calls"), toolCallsSnapshot);
    }
    if (w_->engineerProxyRuntime_.active)
    {
        w_->handleEngineerAssistantMessage(finalText, reasoningText);
        w_->temp_assistant_history = "";
        w_->currentThinkIndex_ = -1;
        w_->currentAssistantIndex_ = -1;
        return;
    }
    w_->ui_messagesArray.append(roleMessage);
    if (w_->history_ && w_->ui_state == CHAT_STATE)
    {
        w_->history_->appendMessage(roleMessage);
    }
    const int asstMsgIndex = w_->ui_messagesArray.size() - 1;
    if (!reasoningText.isEmpty() && w_->currentThinkIndex_ >= 0)
    {
        w_->recordEntries_[w_->currentThinkIndex_].msgIndex = asstMsgIndex;
    }
    if (w_->currentAssistantIndex_ >= 0)
    {
        w_->recordEntries_[w_->currentAssistantIndex_].msgIndex = asstMsgIndex;
    }

    // 输出区增强：若模型输出包含完整的 <tool_call>...</tool_call> 工具调用块，
    // 则在“流式输出结束”这一刻对该 assistant 记录块做一次重渲染：
    // - 让工具名/参数名/关键字段以更醒目的颜色显示（用户更容易看懂模型在调用什么）；
    // - 同时解决“<tool_call> JSON 跨 chunk 分片”导致的高亮不完整问题（这里用完整 finalText 重绘一次即可）。
    // 仅当包含 tool_call 标签时才触发，避免对普通对话输出产生额外开销。
    const bool hasToolCallBlock = finalText.contains(QStringLiteral("<tool_call>")) &&
                                  finalText.contains(QStringLiteral("</tool_call>"));
    if (hasToolCallBlock && w_->currentAssistantIndex_ >= 0)
    {
        w_->updateRecordEntryContent(w_->currentAssistantIndex_, finalText);
    }
    w_->currentThinkIndex_ = -1;
    w_->currentAssistantIndex_ = -1;
    w_->temp_assistant_history = "";

    if (w_->ui_state == COMPLETE_STATE) // 补完模式的回答只输出一次
    {
        w_->normal_finish_pushover();
        w_->on_reset_clicked(); // 自动重置
    }
    else
    {
        // 工具链开关开启时，尝试解析工具 JSON
        if (w_->is_load_tool)
        {
            if (w_->ui_tool_call_mode == TOOL_CALL_FUNCTION)
            {
                auto parseArguments = [&](const QJsonValue &value) -> mcp::json {
                    if (value.isObject())
                    {
                        const QJsonDocument doc(value.toObject());
                        const QByteArray bytes = doc.toJson(QJsonDocument::Compact);
                        try
                        {
                            mcp::json parsed = mcp::json::parse(bytes.constData(), bytes.constData() + bytes.size());
                            return parsed.is_object() ? parsed : mcp::json::object();
                        }
                        catch (const std::exception &)
                        {
                            return mcp::json::object();
                        }
                    }
                    if (value.isString())
                    {
                        const QByteArray bytes = value.toString().toUtf8();
                        try
                        {
                            mcp::json parsed = mcp::json::parse(bytes.constData(), bytes.constData() + bytes.size());
                            return parsed.is_object() ? parsed : mcp::json::object();
                        }
                        catch (const std::exception &)
                        {
                            return mcp::json::object();
                        }
                    }
                    return mcp::json::object();
                };

                if (toolCallsSnapshot.isEmpty())
                {
                    w_->normal_finish_pushover();
                }
                else
                {
                    QJsonObject callObj;
                    for (const auto &item : toolCallsSnapshot)
                    {
                        if (item.isObject())
                        {
                            callObj = item.toObject();
                            break;
                        }
                    }
                    if (callObj.isEmpty())
                    {
                        w_->normal_finish_pushover();
                    }
                    else
                    {
                        QString tools_name;
                        QJsonValue argsValue;
                        const QJsonValue functionVal = callObj.value(QStringLiteral("function"));
                        if (functionVal.isObject())
                        {
                            const QJsonObject functionObj = functionVal.toObject();
                            tools_name = functionObj.value(QStringLiteral("name")).toString();
                            argsValue = functionObj.value(QStringLiteral("arguments"));
                        }
                        else
                        {
                            tools_name = callObj.value(QStringLiteral("name")).toString();
                            argsValue = callObj.value(QStringLiteral("arguments"));
                        }
                        const QString toolCallId = callObj.value(QStringLiteral("id")).toString();

                        if (tools_name.isEmpty())
                        {
                            w_->normal_finish_pushover();
                        }
                        else
                        {
                            w_->tools_call = mcp::json::object();
                            w_->tools_call["name"] = tools_name.toStdString();
                            w_->tools_call["arguments"] = parseArguments(argsValue);
                            w_->pendingToolCallId_ = toolCallId;

                            w_->lastToolCallName_ = tools_name;
                            w_->lastToolPendingName_ = tools_name; // 保留工具名，供工具返回时附加截图等场景
                            w_->reflash_state("ui:" + w_->jtr("clicked") + " " + tools_name, SIGNAL_SIGNAL);

                            // 记录区：工具“触发即显示”，不必等工具执行完成再出现记录块。
                            // - 这里只创建记录块（图标/徽标），不输出内容；
                            // - 工具返回时在 handleToolLoop() 中复用该记录块写入 tool_result。
                            if (tools_name != QStringLiteral("answer") && tools_name != QStringLiteral("response"))
                            {
                                w_->currentToolRecordIndex_ = w_->recordCreate(Widget::RecordRole::Tool, tools_name);
                            }
                            // 工具层面指出结束
                            if (tools_name == QStringLiteral("system_engineer_proxy"))
                            {
                                w_->startEngineerProxyTool(w_->tools_call);
                                return;
                            }
                            if (tools_name == QStringLiteral("schedule_task"))
                            {
                                w_->handleScheduleToolCall(w_->tools_call);
                                return;
                            }
                            if (tools_name == QStringLiteral("answer") || tools_name == QStringLiteral("response"))
                            {
                                w_->pendingToolCallId_.clear();
                                w_->normal_finish_pushover();
                            }
                            else
                            {
                                w_->logFlow(FlowPhase::ToolParsed, QStringLiteral("name=%1").arg(tools_name), SIGNAL_SIGNAL);
                                w_->pendingAssistantHeaderReset_ = true;
                                w_->toolInvocationActive_ = true;
                                emit w_->ui2tool_turn(w_->activeTurnId_);
                                w_->logFlow(FlowPhase::ToolStart, QStringLiteral("name=%1").arg(tools_name), SIGNAL_SIGNAL);
                                emit w_->ui2tool_exec(w_->tools_call);
                                // use tool; decoding remains paused
                            }
                        }
                    }
                }
            }
            else
            {
                QString tool_str = w_->ui_messagesArray.last().toObject().value("content").toString();
                w_->tools_call = w_->XMLparser(tool_str);
                if (w_->tools_call.empty())
                {
                    w_->normal_finish_pushover();
                }
                else
                {
                    if (w_->tools_call.contains("name") && w_->tools_call.contains("arguments"))
                    {
                        QString tools_name = QString::fromStdString(w_->tools_call.value("name", ""));
                        w_->lastToolCallName_ = tools_name;
                        w_->lastToolPendingName_ = tools_name; // 保留工具名，供工具返回时附加截图等场景
                        w_->reflash_state("ui:" + w_->jtr("clicked") + " " + tools_name, SIGNAL_SIGNAL);

                        // 记录区：工具“触发即显示”，不必等工具执行完成再出现记录块。
                        // - 这里只创建记录块（图标/徽标），不输出内容；
                        // - 工具返回时在 handleToolLoop() 中复用该记录块写入 tool_result。
                        if (tools_name != QStringLiteral("answer") && tools_name != QStringLiteral("response"))
                        {
                            w_->currentToolRecordIndex_ = w_->recordCreate(Widget::RecordRole::Tool, tools_name);
                        }
                        // 工具层面指出结束
                        if (tools_name == "system_engineer_proxy")
                        {
                            w_->startEngineerProxyTool(w_->tools_call);
                            return;
                        }
                        if (tools_name == "schedule_task")
                        {
                            w_->handleScheduleToolCall(w_->tools_call);
                            return;
                        }
                        if (tools_name == "answer" || tools_name == "response")
                        {
                            w_->normal_finish_pushover();
                        }
                        else
                        {
                            w_->logFlow(FlowPhase::ToolParsed, QStringLiteral("name=%1").arg(tools_name), SIGNAL_SIGNAL);
                            w_->pendingAssistantHeaderReset_ = true;
                            w_->toolInvocationActive_ = true;
                            emit w_->ui2tool_turn(w_->activeTurnId_);
                            w_->logFlow(FlowPhase::ToolStart, QStringLiteral("name=%1").arg(tools_name), SIGNAL_SIGNAL);
                            emit w_->ui2tool_exec(w_->tools_call);
                            // use tool; decoding remains paused
                        }
                    }
                }
            }
        }
        else
        {
            w_->normal_finish_pushover();
        }
    }
    w_->markBackendActivity();
    w_->scheduleLazyUnload();
}

void ToolFlowController::recvToolCalls(const QString &payload)
{
    if (w_->ui_tool_call_mode != TOOL_CALL_FUNCTION)
    {
        return;
    }

    w_->pendingToolCallsPayload_ = QJsonArray();
    w_->pendingToolCallId_.clear();

    const QString trimmed = payload.trimmed();
    if (trimmed.isEmpty())
        return;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
    QJsonArray calls;
    if (err.error == QJsonParseError::NoError)
    {
        if (doc.isArray())
        {
            calls = doc.array();
        }
        else if (doc.isObject())
        {
            const QJsonObject obj = doc.object();
            const QJsonValue toolCallsVal = obj.value(QStringLiteral("tool_calls"));
            const QJsonValue functionCallVal = obj.value(QStringLiteral("function_call"));
            if (toolCallsVal.isArray())
            {
                calls = toolCallsVal.toArray();
            }
            else if (functionCallVal.isObject())
            {
                calls.append(functionCallVal.toObject());
            }
            else if (!obj.isEmpty())
            {
                calls.append(obj);
            }
        }
    }

    if (!calls.isEmpty())
    {
        w_->pendingToolCallsPayload_ = calls;
    }
    else
    {
        qWarning() << "function_call tool_calls payload parse failed:" << err.errorString();
    }

    // 让工具调用信息作为模型输出展示
    w_->flushPendingStream();
    QString displayText;
    if (!calls.isEmpty())
    {
        QJsonDocument outDoc(calls);
        displayText = QString::fromUtf8(outDoc.toJson(QJsonDocument::Compact));
    }
    else
    {
        displayText = trimmed;
    }
    if (displayText.isEmpty())
        return;
    if (!w_->temp_assistant_history.isEmpty() && !w_->temp_assistant_history.endsWith('\n'))
    {
        displayText.prepend('\n');
    }
    w_->reflash_output(displayText, true, w_->themeTextPrimary());
}

void ToolFlowController::recvToolPushover(QString tool_result_)
{
    w_->toolInvocationActive_ = false;
    if (tool_result_.contains("<ylsdamxssjxxdd:showdraw>")) // 有图像要显示的情况
    {
        w_->wait_to_show_images_filepath.append(tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1]); // 文生图后待显示图像的图像路径
        w_->tool_result = "stablediffusion " + w_->jtr("call successful, image save at") + " " + tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1];
    }
    else
    {
        w_->tool_result = tool_result_;
        w_->tool_result = w_->truncateString(w_->tool_result, DEFAULT_MAX_INPUT); // 超出最大输入的部分截断
    }
    w_->logFlow(FlowPhase::ToolResult,
                QStringLiteral("len=%1 images=%2").arg(w_->tool_result.size()).arg(w_->wait_to_show_images_filepath.size()),
                SIGNAL_SIGNAL);
    w_->logFlow(FlowPhase::ContinueTurn, QStringLiteral("feed tool result to model"), SIGNAL_SIGNAL);

    if (w_->engineerProxyRuntime_.active)
    {
        const QString observation = w_->tool_result;
        w_->handleEngineerToolResult(observation);
        w_->tool_result.clear();
        return;
    }

    w_->on_send_clicked(); // 触发发送继续预测下一个词
}
