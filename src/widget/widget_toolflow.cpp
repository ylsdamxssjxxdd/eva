#include "widget.h"
#include "ui_widget.h"
#include "core/toolflow/tool_flow_controller.h"
#include "terminal_pane.h"
#include "../utils/startuplogger.h"
#include "../utils/flowtracer.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>

void Widget::recv_pushover()
{
    toolFlowController_->recvPushover();
}



void Widget::recv_tool_calls(const QString &payload)
{
    toolFlowController_->recvToolCalls(payload);
}



void Widget::normal_finish_pushover()
{
    finishTurnPerfSample(QStringLiteral("net_finish"), true);
    turnThinkActive_ = false;
    pendingAssistantHeaderReset_ = false;
    // Reset per-turn header flags
    turnActive_ = false;
    is_run = false;
    ui_state_normal(); // 待机界面状态
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
    // 推理完成后尝试派发定时任务
    tryDispatchScheduledJobs();
}

void Widget::recv_toolpushover(QString tool_result_)
{
    toolFlowController_->recvToolPushover(tool_result_);
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
    finishTurnPerfSample(QStringLiteral("net_stopover"), false);
    flushPendingStream();
    if (ui_state == COMPLETE_STATE)
    {
        ui->reset->click();
    } // 补完模式终止后需要重置
}

void Widget::recv_resetover()
{
    if (ui_SETTINGS.ngl == 0)
    {
        setBaseWindowIcon(QIcon(":/logo/eva.png"));
    } // 恢复图标
    else
    {
        setBaseWindowIcon(QIcon(":/logo/eva.png"));
    } // 恢复图标
    reflash_state("ui:" + jtr("reset ok"), SUCCESS_SIGNAL);
}

void Widget::recv_reload()
{
    preLoad(); // 装载前动作
}

void Widget::recv_datereset()
{
    // 打印约定中的系统指令
    ui_state_info = "-----------" + jtr("date") + "-----------";
    reflash_state(ui_state_info, USUAL_SIGNAL);
    if (ui_state == COMPLETE_STATE)
    {
        reflash_state("- " + jtr("complete mode") + jtr("on") + " ", USUAL_SIGNAL);
    }
    else
    {
        reflash_state("- " + jtr("system calling") + " " + date_ui->date_prompt_TextEdit->toPlainText() + ui_extra_prompt, USUAL_SIGNAL);
        // // 展示额外停止标记
        // QString stop_str;
        // stop_str = jtr("extra stop words") + " ";
        // // stop_str += bot_chat.input_prefix + " ";
        // for (int i = 0; i < ui_DATES.extra_stop_words.size(); ++i) {
        //     stop_str += ui_DATES.extra_stop_words.at(i) + " ";
        // }

        // reflash_state("- " + stop_str + " ", USUAL_SIGNAL);
    }
    reflash_state("-----------" + jtr("date") + "-----------", USUAL_SIGNAL);
    auto_save_user(); // 保存 UI 配置

    ui->reset->click();
}

void Widget::recv_setreset()
{
    // 打印设置内容
    reflash_state("-----------" + jtr("set") + "-----------", USUAL_SIGNAL);

    reflash_state("- " + jtr("temperature") + " " + QString::number(ui_SETTINGS.temp), USUAL_SIGNAL);
    reflash_state("- " + jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat), USUAL_SIGNAL);
    const QString npredictText = (ui_SETTINGS.hid_npredict <= 0) ? QStringLiteral("auto")
                                                                 : QString::number(ui_SETTINGS.hid_npredict);
    reflash_state("- " + jtr("npredict") + " " + npredictText, USUAL_SIGNAL);
    reflash_state("- gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl), USUAL_SIGNAL);
    reflash_state("- cpu" + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread), USUAL_SIGNAL);
    reflash_state("- " + jtr("ctx") + jtr("length") + " " + QString::number(ui_SETTINGS.nctx), USUAL_SIGNAL);
    reflash_state("- " + jtr("batch size") + " " + QString::number(ui_SETTINGS.hid_batch), USUAL_SIGNAL);

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
        reflash_state("- " + jtr("chat mode"), USUAL_SIGNAL);
    }
    else if (ui_state == COMPLETE_STATE)
    {
        reflash_state("- " + jtr("complete mode"), USUAL_SIGNAL);
    }

    // 展示额外停止标记
    //  if (ui_state == CHAT_STATE) {
    //      QString stop_str;
    //      stop_str = jtr("extra stop words") + " ";
    //      for (int i = 0; i < ui_DATES.extra_stop_words.size(); ++i) {
    //          stop_str += ui_DATES.extra_stop_words.at(i) + " ";
    //      }
    //      reflash_state("- " + stop_str + " ", USUAL_SIGNAL);
    //  }

    reflash_state("-----------" + jtr("set") + "-----------", USUAL_SIGNAL);
    auto_save_user(); // 保存 UI 配置

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
        finishTurnPerfSample(QStringLiteral("user_reset_tool"), false);
        reflash_state("ui:tool cancelled", SIGNAL_SIGNAL);
        return;
    }

    emit ui2tool_cancelActive();
    wait_to_show_images_filepath.clear(); // 清空待显示图片
    emit ui2expend_resettts();            // 清空待读队列
    tool_result = "";                     // 清空工具结果
    // 如果模型正在推理就改为停止流程
    if (is_run)
    {
        reflash_state("ui:" + jtr("clicked") + jtr("shut down"), SIGNAL_SIGNAL);
        finishTurnPerfSample(QStringLiteral("user_stop"), false);
        emit ui2net_stop(1);
        // 发送停止信号，模型停止后会再次触发 on_reset_clicked()
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
    // 重置压缩状态，避免残留影响后续对话
    compactionInFlight_ = false;
    compactionQueued_ = false;
    compactionHeaderPrinted_ = false;
    currentCompactIndex_ = -1;
    compactionFromIndex_ = -1;
    compactionToIndex_ = -1;
    compactionReason_.clear();
    compactionPendingHasInput_ = false;
    compactionPendingInput_ = InputPack();

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
    recordClear(); // 待机界面状态

    // 请求式统一处理（本地/远端）
    ui_messagesArray = QJsonArray(); // 清空
    // 构造系统指令
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
        // 远端模式：显示当前端点
        current_api = (ui_state == CHAT_STATE) ? (apis.api_endpoint + apis.api_chat_endpoint)
                                               : (apis.api_endpoint + apis.api_completion_endpoint);
        setBaseWindowIcon(QIcon(":/logo/eva.png"));
        EVA_title = jtr("current api") + " " + current_api;
        reflash_state(QString("ui:") + EVA_title, USUAL_SIGNAL);
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
    }
    else // LOCAL_MODE：显示当前模型，保持本地装载表现
    {
        QString modelName = ui_SETTINGS.modelpath.split("/").last();
        EVA_title = jtr("current model") + " " + modelName;
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        if (ui_SETTINGS.ngl == 0)
        {
            setBaseWindowIcon(QIcon(":/logo/eva.png"));
        }
        else
        {
            setBaseWindowIcon(QIcon(":/logo/eva.png"));
        }
    }
    finishTurnFlow(QStringLiteral("model reply finished"), true);
    finishTurnPerfSample(QStringLiteral("user_reset"), false);
    return;
}

void Widget::recv_net_speeds(double promptPerSec, double genPerSec)
{
    const bool haveGen = genPerSec > 0.0;
    const bool havePrompt = promptPerSec > 0.0;
    if (!haveGen && !havePrompt) return; // 没有速度数据就不打印
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
