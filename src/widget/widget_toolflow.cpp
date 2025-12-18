#include "widget.h"
#include "ui_widget.h"
#include "terminal_pane.h"
#include "../utils/startuplogger.h"
#include "../utils/flowtracer.h"

#include <QDir>
#include <QMessageBox>

void Widget::recv_pushover()
{
    flushPendingStream();
    lastToolCallName_.clear();
    lastToolPendingName_.clear();
    // Separate all reasoning (<think>...</think>) blocks from final content; capture both roles
    QString finalText = temp_assistant_history;
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
    // 重要：不要在这里打印 assistant_len/reasoning_len（字符长度），它很容易被误解为 token 数。
    // 链接模式下我们只关心 token 口径（来自 usage/timings 的校准结果，以及 UI 侧的 KV 汇总）。
    {
        const int promptTok = qMax(0, kvPromptTokensTurn_);
        const int genTok = qMax(0, kvStreamedTurn_);
        const int reasoningTok = qMax(0, lastReasoningTokens_);
        const int usedTok = qMax(0, kvUsed_);
        const int turnTok = qMax(0, kvTokensTurn_);
        logFlow(FlowPhase::NetDone,
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
    if (engineerProxyRuntime_.active)
    {
        handleEngineerAssistantMessage(finalText, reasoningText);
        temp_assistant_history = "";
        currentThinkIndex_ = -1;
        currentAssistantIndex_ = -1;
        return;
    }
    ui_messagesArray.append(roleMessage);
    if (history_ && ui_state == CHAT_STATE)
    {
        history_->appendMessage(roleMessage);
    }
    const int asstMsgIndex = ui_messagesArray.size() - 1;
    if (!reasoningText.isEmpty() && currentThinkIndex_ >= 0)
    {
        recordEntries_[currentThinkIndex_].msgIndex = asstMsgIndex;
    }
    if (currentAssistantIndex_ >= 0)
    {
        recordEntries_[currentAssistantIndex_].msgIndex = asstMsgIndex;
    }

    // 输出区增强：若模型输出包含完整的 <tool_call>...</tool_call> 工具调用块，
    // 则在“流式输出结束”这一刻对该 assistant 记录块做一次重渲染：
    // - 让工具名/参数名/关键字段以更醒目的颜色显示（用户更容易看懂模型在调用什么）；
    // - 同时解决“<tool_call> JSON 跨 chunk 分片”导致的高亮不完整问题（这里用完整 finalText 重绘一次即可）。
    // 仅当包含 tool_call 标签时才触发，避免对普通对话输出产生额外开销。
    const bool hasToolCallBlock = finalText.contains(QStringLiteral("<tool_call>")) &&
                                  finalText.contains(QStringLiteral("</tool_call>"));
    if (hasToolCallBlock && currentAssistantIndex_ >= 0)
    {
        updateRecordEntryContent(currentAssistantIndex_, finalText);
    }
    currentThinkIndex_ = -1;
    currentAssistantIndex_ = -1;
    temp_assistant_history = "";

    if (ui_state == COMPLETE_STATE) // 琛ュ畬妯″紡鐨勫洖绛斿彧杈撳嚭涓€娆?
    {
        normal_finish_pushover();
        on_reset_clicked(); // 鑷姩閲嶇疆
    }
    else
    {
        // 宸ュ叿閾惧紑鍏冲紑鍚椂锛屽皾璇曡В鏋愬伐鍏?JSON
        if (is_load_tool)
        {
            QString tool_str = ui_messagesArray.last().toObject().value("content").toString();
            tools_call = XMLparser(tool_str);
            if (tools_call.empty())
            {
                normal_finish_pushover();
            }
            else
            {
                if (tools_call.contains("name") && tools_call.contains("arguments"))
                {
                    QString tools_name = QString::fromStdString(tools_call.value("name", ""));
                    lastToolCallName_ = tools_name;
                    lastToolPendingName_ = tools_name; // 保留工具名，供工具返回时附加截图等场景
                    reflash_state("ui:" + jtr("clicked") + " " + tools_name, SIGNAL_SIGNAL);

                    // 记录区：工具“触发即显示”，不必等工具执行完成再出现记录块。
                    // - 这里只创建记录块（图标/徽标），不输出内容；
                    // - 工具返回时在 handleToolLoop() 中复用该记录块写入 tool_result。
                    if (tools_name != QStringLiteral("answer") && tools_name != QStringLiteral("response"))
                    {
                        currentToolRecordIndex_ = recordCreate(RecordRole::Tool, tools_name);
                    }
                    // 宸ュ叿灞傞潰鎸囧嚭缁撴潫
                    if (tools_name == "system_engineer_proxy")
                    {
                        startEngineerProxyTool(tools_call);
                        return;
                    }
                    if (tools_name == "answer" || tools_name == "response")
                    {
                        normal_finish_pushover();
                    }
                    else
                    {
                        logFlow(FlowPhase::ToolParsed, QStringLiteral("name=%1").arg(tools_name), SIGNAL_SIGNAL);
                        pendingAssistantHeaderReset_ = true;
                        toolInvocationActive_ = true;
                        emit ui2tool_turn(activeTurnId_);
                        logFlow(FlowPhase::ToolStart, QStringLiteral("name=%1").arg(tools_name), SIGNAL_SIGNAL);
                        emit ui2tool_exec(tools_call);
                        // use tool; decoding remains paused
                    }
                }
            }
        }
        else
        {
            normal_finish_pushover();
        }
    }
    markBackendActivity();
    scheduleLazyUnload();
}

void Widget::normal_finish_pushover()
{
    turnThinkActive_ = false;
    pendingAssistantHeaderReset_ = false;
    // Reset per-turn header flags
    turnActive_ = false;
    is_run = false;
    ui_state_normal(); // 寰呮満鐣岄潰鐘舵€?
    if (ui_mode == LINK_MODE)
    {
        kvTokensTurn_ = kvPromptTokensTurn_ + qMax(0, kvStreamedTurn_);
        // 调试：LINK 模式下 reasoning tokens（思考 token）也计入本轮 KV 占用，便于观察真实推理负载。
        // 此处输出一个“完结汇总”，用于和 net 层的 prompt/stream 统计对齐。
        if (lastReasoningTokens_ > 0)
        {
            FlowTracer::log(
                FlowChannel::Session,
                QStringLiteral("link:kv finish(incl reasoning) reasoning=%1 kvUsed=%2 kvStream=%3 kvTurn=%4 prompt=%5 usedBefore=%6")
                    .arg(lastReasoningTokens_)
                    .arg(kvUsed_)
                    .arg(kvStreamedTurn_)
                    .arg(kvTokensTurn_)
                    .arg(kvPromptTokensTurn_)
                    .arg(kvUsedBeforeTurn_),
                activeTurnId_);
        }
        // reflash_state(QStringLiteral("link:turn complete prompt=%1 stream=%2 turn=%3 used=%4 used_before=%5")
        //                   .arg(kvPromptTokensTurn_)
        //                   .arg(kvStreamedTurn_)
        //                   .arg(kvTokensTurn_)
        //                   .arg(kvUsed_)
        //                   .arg(kvUsedBeforeTurn_));
    }
    decode_finish();
    if (linkProfile_ == LinkProfile::Control && !isHostControlled())
    {
        controlThinkActive_ = false;
        controlStreamRole_.clear();
    }
    if (!wait_to_show_images_filepath.isEmpty())
    {
        showImages(wait_to_show_images_filepath);
        wait_to_show_images_filepath.clear();
    }
}

void Widget::recv_toolpushover(QString tool_result_)
{
    toolInvocationActive_ = false;
    if (tool_result_.contains("<ylsdamxssjxxdd:showdraw>")) // 鏈夊浘鍍忚鏄剧ず鐨勬儏鍐?
    {
        wait_to_show_images_filepath.append(tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1]); // 鏂囩敓鍥惧悗寰呮樉绀哄浘鍍忕殑鍥惧儚璺緞
        tool_result = "stablediffusion " + jtr("call successful, image save at") + " " + tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1];
    }
    else
    {
        tool_result = tool_result_;
        tool_result = truncateString(tool_result, DEFAULT_MAX_INPUT); // 瓒呭嚭鏈€澶ц緭鍏ョ殑閮ㄥ垎鎴柇
    }
    logFlow(FlowPhase::ToolResult,
            QStringLiteral("len=%1 images=%2").arg(tool_result.size()).arg(wait_to_show_images_filepath.size()),
            SIGNAL_SIGNAL);
    logFlow(FlowPhase::ContinueTurn, QStringLiteral("feed tool result to model"), SIGNAL_SIGNAL);

    if (engineerProxyRuntime_.active)
    {
        const QString observation = tool_result;
        handleEngineerToolResult(observation);
        tool_result.clear();
        return;
    }


    on_send_clicked(); // 瑙﹀彂鍙戦€佺户缁娴嬩笅涓€涓瘝
}

void Widget::collapseTerminalPane()
{
    StartupLogger::log(QStringLiteral("[widget] collapseTerminalPane enter"));
    if (!ui->statusTerminalSplitter || !ui->terminalPane) return;
    QList<int> sizes;
    sizes << 1 << 0;
    ui->statusTerminalSplitter->setSizes(sizes);
    terminalCollapsed_ = true;
}

void Widget::ensureTerminalPaneVisible()
{
    if (!ui->statusTerminalSplitter || !ui->terminalPane) return;
    if (!terminalCollapsed_) return;
    const int available = ui->statusTerminalSplitter->size().width();
    int desired = qMax(terminalAutoExpandSize_, 240);
    if (available > 0)
    {
        const int maxAllowed = qMax(240, available - 160);
        desired = qMin(desired, maxAllowed);
        if (desired <= 0) desired = qMax(available / 2, 240);
    }
    else
    {
        desired = qMax(desired, 320);
    }
    int left = (available > 0) ? qMax(available - desired, 240) : desired * 2;
    if (left <= 0) left = desired;
    QList<int> sizes;
    sizes << left << desired;
    ui->statusTerminalSplitter->setSizes(sizes);
    terminalAutoExpandSize_ = qMax(240, desired);
    terminalCollapsed_ = false;
}

void Widget::toolCommandStarted(const QString &command, const QString &workingDir)
{
    ensureTerminalPaneVisible();
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalStart(command, workingDir);
    }
}

void Widget::toolCommandStdout(const QString &chunk)
{
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalStdout(chunk);
    }
}

void Widget::toolCommandStderr(const QString &chunk)
{
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalStderr(chunk);
    }
}

void Widget::toolCommandFinished(int exitCode, bool interrupted)
{
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalFinished(exitCode, interrupted);
    }
}

void Widget::onTerminalInterruptRequested()
{
    emit ui2tool_interruptCommand();
}

void Widget::recv_stopover()
{
    flushPendingStream();
    if (ui_state == COMPLETE_STATE)
    {
        ui->reset->click();
    } // 琛ュ畬妯″紡缁堟鍚庨渶瑕侀噸缃?
}

void Widget::recv_resetover()
{
    if (ui_SETTINGS.ngl == 0)
    {
        setBaseWindowIcon(QIcon(":/logo/blue_logo.png"));
    } // 鎭㈠
    else
    {
        setBaseWindowIcon(QIcon(":/logo/green_logo.png"));
    } // 鎭㈠
    reflash_state("ui:" + jtr("reset ok"), SUCCESS_SIGNAL);
}

void Widget::recv_reload()
{
    preLoad(); // 瑁呰浇鍓嶅姩浣?
}

void Widget::recv_datereset()
{
    // 鎵撳嵃绾﹀畾鐨勭郴缁熸寚浠?
    ui_state_info = "路路路路路路路路路路路" + jtr("date") + "路路路路路路路路路路路";
    reflash_state(ui_state_info, USUAL_SIGNAL);
    if (ui_state == COMPLETE_STATE)
    {
        reflash_state("路 " + jtr("complete mode") + jtr("on") + " ", USUAL_SIGNAL);
    }
    else
    {
        reflash_state("路 " + jtr("system calling") + " " + date_ui->date_prompt_TextEdit->toPlainText() + ui_extra_prompt, USUAL_SIGNAL);
        // //灞曠ず棰濆鍋滄鏍囧織
        // QString stop_str;
        // stop_str = jtr("extra stop words") + " ";
        // // stop_str += bot_chat.input_prefix + " ";
        // for (int i = 0; i < ui_DATES.extra_stop_words.size(); ++i) {
        //     stop_str += ui_DATES.extra_stop_words.at(i) + " ";
        // }

        // reflash_state("路 " + stop_str + " ", USUAL_SIGNAL);
    }
    reflash_state("路路路路路路路路路路路" + jtr("date") + "路路路路路路路路路路路", USUAL_SIGNAL);
    auto_save_user(); // 淇濆瓨ui閰嶇疆

    ui->reset->click();
}

void Widget::recv_setreset()
{
    // 鎵撳嵃璁剧疆鍐呭
    reflash_state("路路路路路路路路路路路" + jtr("set") + "路路路路路路路路路路路", USUAL_SIGNAL);

    reflash_state("路 " + jtr("temperature") + " " + QString::number(ui_SETTINGS.temp), USUAL_SIGNAL);
    reflash_state("路 " + jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat), USUAL_SIGNAL);
    const QString npredictText = (ui_SETTINGS.hid_npredict <= 0) ? QStringLiteral("auto")
                                                                 : QString::number(ui_SETTINGS.hid_npredict);
    reflash_state("路 " + jtr("npredict") + " " + npredictText, USUAL_SIGNAL);
    reflash_state("路 gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl), USUAL_SIGNAL);
    reflash_state("路 cpu" + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread), USUAL_SIGNAL);
    reflash_state("路 " + jtr("ctx") + jtr("length") + " " + QString::number(ui_SETTINGS.nctx), USUAL_SIGNAL);
    reflash_state("路 " + jtr("batch size") + " " + QString::number(ui_SETTINGS.hid_batch), USUAL_SIGNAL);

    if (ui_SETTINGS.lorapath != "")
    {
        reflash_state("ui:" + jtr("load lora") + " " + ui_SETTINGS.lorapath, USUAL_SIGNAL);
    }
    if (ui_SETTINGS.mmprojpath != "")
    {
        reflash_state("ui:" + jtr("load mmproj") + " " + ui_SETTINGS.mmprojpath, USUAL_SIGNAL);
    }
    if (ui_state == CHAT_STATE)
    {
        reflash_state("路 " + jtr("chat mode"), USUAL_SIGNAL);
    }
    else if (ui_state == COMPLETE_STATE)
    {
        reflash_state("路 " + jtr("complete mode"), USUAL_SIGNAL);
    }

    // 灞曠ず棰濆鍋滄鏍囧織
    //  if (ui_state == CHAT_STATE) {
    //      QString stop_str;
    //      stop_str = jtr("extra stop words") + " ";
    //      for (int i = 0; i < ui_DATES.extra_stop_words.size(); ++i) {
    //          stop_str += ui_DATES.extra_stop_words.at(i) + " ";
    //      }
    //      reflash_state("路 " + stop_str + " ", USUAL_SIGNAL);
    //  }

    reflash_state("路路路路路路路路路路路" + jtr("set") + "路路路路路路路路路路路", USUAL_SIGNAL);
    auto_save_user(); // 淇濆瓨ui閰嶇疆

    ui->reset->click();
}

void Widget::on_reset_clicked()
{
    if (linkProfile_ == LinkProfile::Control && !isControllerActive())
    {
        reflash_state(jtr("control disconnected"), WRONG_SIGNAL);
        return;
    }
    if (isControllerActive())
    {
        if (controlChannel_)
        {
            QJsonObject cmd;
            cmd.insert(QStringLiteral("type"), QStringLiteral("command"));
            const bool stopOnly = controlClient_.remoteRunning;
            cmd.insert(QStringLiteral("name"), stopOnly ? QStringLiteral("stop") : QStringLiteral("reset"));
            controlChannel_->sendToHost(cmd);
            reflash_state(stopOnly ? jtr("control stop") : jtr("control reset"), SIGNAL_SIGNAL);
        }
        return;
    }
    if (engineerProxyOuterActive_)
    {
        cancelEngineerProxy(QStringLiteral("reset"));
        engineerProxyOuterActive_ = false;
        toolInvocationActive_ = false;
    }
    if (toolInvocationActive_)
    {
        emit ui2tool_cancelActive();
        // 重要：取消工具时也要立即终止文转声，否则可能出现“对话已重置但仍在朗读/播放”的体验问题
        emit ui2expend_resettts();
        toolInvocationActive_ = false;
        tool_result.clear();
        turnActive_ = false;
        is_run = false;
        decode_finish();
        ui_state_normal();
        reflash_state("ui:tool cancelled", SIGNAL_SIGNAL);
        return;
    }

    emit ui2tool_cancelActive();
    wait_to_show_images_filepath.clear(); // 娓呯┖寰呮樉绀哄浘鍍?
    emit ui2expend_resettts();            // 娓呯┖寰呰鍒楄〃
    tool_result = "";                     // 娓呯┖宸ュ叿缁撴灉
    // 濡傛灉妯″瀷姝ｅ湪鎺ㄧ悊灏辨敼鍙樻ā鍨嬬殑鍋滄鏍囩
    if (is_run)
    {
        reflash_state("ui:" + jtr("clicked") + jtr("shut down"), SIGNAL_SIGNAL);
        emit ui2net_stop(1);
        // 浼犻€掓帹鐞嗗仠姝俊鍙?妯″瀷鍋滄鍚庝細鍐嶆瑙﹀彂on_reset_clicked()
        return;
    }

    flushPendingStream();
    temp_assistant_history.clear();
    turnThinkActive_ = false;
    turnThinkHeaderPrinted_ = false;
    turnAssistantHeaderPrinted_ = false;
    currentThinkIndex_ = -1;
    currentAssistantIndex_ = -1;
    pendingAssistantHeaderReset_ = false;

    const bool engineerActive = date_ui && date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked();

    if (date_ui)
    {
        // Rebuild system prompt so workspace snapshot reflects current workspace state.
        ui_extra_prompt = create_extra_prompt();
        get_date(shouldApplySandboxNow());
    }

    // reflash_state("ui:" + jtr("clicked reset"), SIGNAL_SIGNAL);
    // Reset all per-turn token counters now that the session is cleared
    kvTokensTurn_ = 0;
    kvPromptTokensTurn_ = 0;
    // reset KV/speed tracking and progress bar
    kvUsed_ = 0;
    kvUsedBeforeTurn_ = 0;
    kvStreamedTurn_ = 0;
    turnActive_ = false;
    activeTurnId_ = 0;
    nextTurnId_ = 1;
    engineerProxyRuntime_.active = false; // reset engineer proxy session
    emit ui2tool_turn(0);
    updateKvBarUi();
    currentSlotId_ = -1; // new conversation -> no slot yet
    // Reset output safely. Replacing the QTextDocument drops any cached
    // resources/undo stack without risking double-deletes.
    // Note: QTextEdit takes ownership of the previous document and will
    // delete it; do not manually delete the old one here.
    if (ui_state == CHAT_STATE) resetOutputDocument();
    ui_state_normal();
    recordClear(); // 寰呮満鐣岄潰鐘舵€?

    // 璇锋眰寮忕粺涓€澶勭悊锛堟湰鍦?杩滅锛?
    ui_messagesArray = QJsonArray(); // 娓呯┖
    // 鏋勯€犵郴缁熸寚浠?
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    ui_messagesArray.append(systemMessage);
    ensureSystemHeader(ui_DATES.date_prompt);
    ensureOutputAtBottom();

    if (engineerActive)
    {
        engineerRestoreOutputAfterEngineerRefresh_ = true;
        if (!engineerRefreshAfterResetScheduled_)
        {
            engineerRefreshAfterResetScheduled_ = true;
            if (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked())
            {
                engineerRefreshAfterResetScheduled_ = false;
                engineerRestoreOutputAfterEngineerRefresh_ = false;
            }
            else
            {
                triggerEngineerEnvRefresh(true);
                engineerRefreshAfterResetScheduled_ = false;
            }
        }
    }

    // Do not record reset into history; clear current session only
    if (history_) history_->clearCurrent();

    if (ui_mode == LINK_MODE)
    {
        // 杩滅妯″紡锛氭樉绀哄綋鍓嶇鐐?
        current_api = (ui_state == CHAT_STATE) ? (apis.api_endpoint + apis.api_chat_endpoint)
                                               : (apis.api_endpoint + apis.api_completion_endpoint);
        setBaseWindowIcon(QIcon(":/logo/dark_logo.png"));
        EVA_title = jtr("current api") + " " + current_api;
        reflash_state(QString("ui:") + EVA_title, USUAL_SIGNAL);
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
    }
    else // LOCAL_MODE锛氭樉绀哄綋鍓嶆ā鍨嬶紝淇濇寔鏈湴瑁呰浇琛ㄧ幇
    {
        QString modelName = ui_SETTINGS.modelpath.split("/").last();
        EVA_title = jtr("current model") + " " + modelName;
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        if (ui_SETTINGS.ngl == 0)
        {
            setBaseWindowIcon(QIcon(":/logo/blue_logo.png"));
        }
        else
        {
            setBaseWindowIcon(QIcon(":/logo/green_logo.png"));
        }
    }
    finishTurnFlow(QStringLiteral("model reply finished"), true);
    return;
}

void Widget::recv_net_speeds(double promptPerSec, double genPerSec)
{
    const bool haveGen = genPerSec > 0.0;
    const bool havePrompt = promptPerSec > 0.0;
    if (!haveGen && !havePrompt) return; // 娌℃湁灏变笉鎵撳嵃
    const QString genStr = haveGen ? (QString::number(genPerSec, 'f', 1) + " tokens/s") : QString::fromUtf8("--");
    const QString promptStr = havePrompt ? (QString::number(promptPerSec, 'f', 1) + " tokens/s") : QString::fromUtf8("--");
    reflash_state(QString::fromUtf8("ui:") + jtr("single decode") + " " + genStr + " " + jtr("batch decode") + " " + promptStr, SUCCESS_SIGNAL);
}
void Widget::recv_docker_status(const DockerSandboxStatus &status)
{
    dockerSandboxStatus_ = status;
    dockerSandboxStatusValid_ = true;
    if (ui_engineer_ischecked && ui_dockerSandboxEnabled)
    {
        refreshEngineerPromptBlock();
    }

    const bool needPrompt = (dockerTargetMode_ == DockerTargetMode::Container) && ui_engineer_ischecked && ui_dockerSandboxEnabled &&
                            !status.containerName.isEmpty() && !status.lastError.isEmpty() &&
                            status.lastError.contains(QStringLiteral("missing a bind mount"), Qt::CaseInsensitive);
    if (needPrompt)
    {
        if (!dockerMountPromptedContainers_.contains(status.containerName))
        {
            dockerMountPromptedContainers_.insert(status.containerName);
            QString missingTarget = status.missingMountTarget;
            if (missingTarget.isEmpty()) missingTarget = DockerSandbox::defaultContainerWorkdir();
            QString selectedHost = status.hostWorkdir.isEmpty() ? engineerWorkDir : status.hostWorkdir;
            if (missingTarget == DockerSandbox::skillsMountPoint() && !status.hostSkillsDir.isEmpty())
            {
                selectedHost = status.hostSkillsDir;
            }
            const QString hostPath = QDir::toNativeSeparators(selectedHost);
            const QString hostDisplay = hostPath.isEmpty() ? jtr("engineer workdir") : hostPath;
            const QString body = jtr("docker fix mount body")
                                     .arg(status.containerName,
                                          missingTarget.isEmpty() ? DockerSandbox::defaultContainerWorkdir() : missingTarget,
                                          hostDisplay);
            const auto reply = QMessageBox::question(
                this,
                jtr("docker fix mount title"),
                body,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (reply == QMessageBox::Yes)
            {
                dockerMountPromptedContainers_.remove(status.containerName);
                emit ui2tool_fixDockerContainerMount(status.containerName);
            }
        }
    }

    const bool sandboxExpected = ui_engineer_ischecked && ui_dockerSandboxEnabled;
    if (sandboxExpected)
    {
        engineerDockerReady_ = status.ready;
        if (status.ready)
        {
            engineerDockerLaunchPending_ = false;
            if (engineerEnvSummaryPending_)
            {
                engineerEnvSummaryPending_ = false;
                logEngineerEnvSummary();
            }
        }
    }
    else
    {
        engineerDockerReady_ = true;
        engineerDockerLaunchPending_ = false;
    }
    if (ui_engineer_ischecked) drainEngineerGateQueue();
}
