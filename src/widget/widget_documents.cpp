#include "widget.h"
#include "ui_widget.h"
#include <QTextDocument>
#include <QPlainTextDocumentLayout>
#include "../utils/textparse.h"

void Widget::recv_chat_format(EVA_CHATS_TEMPLATE chats)
{
    bot_chat = chats;
}

void Widget::initTextComponentsMemoryPolicy()
{
    // Output area（QPlainTextEdit）: no undo stack to avoid growth on streaming inserts
    ui->output->setUndoRedoEnabled(false);

    // State log (QPlainTextEdit): disable undo and cap block count
    ui->state->setUndoRedoEnabled(false);
    ui->state->setMaximumBlockCount(5000); // prevent unbounded log growth
}

void Widget::resetOutputDocument()
{
    flushPendingStream();
    is_stop_output_scroll = false;
    // Preserve tab stops before swapping the doc so layout stays consistent.
    const qreal prevTabStop = ui->output->tabStopDistance();

    // Create a fresh document and hand ownership to the widget.
    // Qt will destroy the previous document for us; avoid manual delete.
    QTextDocument *doc = new QTextDocument(ui->output);
    doc->setDocumentLayout(new QPlainTextDocumentLayout(doc)); // 确保使用纯文本布局以提升性能
    doc->setUndoRedoEnabled(false);
    const QFont font = currentOutputFont();
    doc->setDefaultFont(font); // keep the configured output font after reset
    ui->output->setDocument(doc);

    // Reapply widget-level settings that the new document doesn't carry.
    ui->output->setFont(font);
    ui->output->setTabStopDistance(prevTabStop);
}

void Widget::resetStateDocument()
{
    // Same as above: let Qt own and clean up the previous document.
    QTextDocument *doc = new QTextDocument(ui->state);
    doc->setDocumentLayout(new QPlainTextDocumentLayout(doc));
    doc->setUndoRedoEnabled(false);
    ui->state->setDocument(doc);
}

void Widget::updateKvBarUi()
{
    // Prefer server-reported n_ctx_slot; fallback to UI nctx
    int cap = slotCtxMax_ > 0 ? slotCtxMax_ : (ui_SETTINGS.nctx > 0 ? ui_SETTINGS.nctx : DEFAULT_NCTX);
    if (cap <= 0) cap = DEFAULT_NCTX;

    int used = qMax(0, kvUsed_);
    if (used > cap) used = cap;

    // Convert used/cap to percent in a single (orange) segment
    int percent = cap > 0 ? int(qRound(100.0 * double(used) / double(cap))) : 0;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    // Visual minimum: if any memory, show at least 1%
    if (used > 0 && percent == 0) percent = 1;

    // Force progress range 0..100; use second segment only (orange)
    if (ui->kv_bar->maximum() != 100 || ui->kv_bar->minimum() != 0) ui->kv_bar->setRange(0, 100);
    ui->kv_bar->setValue(0);
    ui->kv_bar->setSecondValue(percent);
    ui->kv_bar->setShowText(jtr("kv bar label"));
    ui->kv_bar->setCenterText("");
    ui->kv_bar->setToolTip(jtr("kv bar tooltip").arg(used).arg(cap));
    broadcastControlKv(used, cap, percent);
}

void Widget::recv_prompt_baseline(int tokens)
{
    if (engineerProxyRuntime_.active)
    {
        if (tokens >= 0) engineerProxyRuntime_.lastPromptTokens = qMax(0, tokens);
        return;
    }
    if (tokens < 0) return;
    const int promptTokens = qMax(0, tokens);
    // Always treat provider usage as the absolute prompt baseline for this turn.
    kvPromptTokensTurn_ = promptTokens;
    kvUsedBeforeTurn_ = promptTokens;
    if (!turnActive_) turnActive_ = true;
    kvTokensTurn_ = kvPromptTokensTurn_ + qMax(0, kvStreamedTurn_);
    kvUsed_ = kvPromptTokensTurn_ + qMax(0, kvStreamedTurn_);
    sawPromptPast_ = true;
    // if (ui_mode == LINK_MODE)
    // {
    //     const int deltaPrompt = kvPromptTokensTurn_ - previousPrompt;
    //     const int deltaContext = kvUsedBeforeTurn_ - previousBaseline;
    //     const QString tag = (deltaPrompt == 0) ? QStringLiteral("link:prompt usage (repeat)")
    //                                            : QStringLiteral("link:prompt usage");
    //     reflash_state(QStringLiteral("%1 prompt=%2 delta_prompt=%3 delta_ctx=%4 stream=%5 turn=%6 used=%7 used_before=%8")
    //                       .arg(tag)
    //                       .arg(kvPromptTokensTurn_)
    //                       .arg(deltaPrompt)
    //                       .arg(deltaContext)
    //                       .arg(kvStreamedTurn_)
    //                       .arg(kvTokensTurn_)
    //                       .arg(kvUsed_)
    //                       .arg(kvUsedBeforeTurn_));
    // }
    updateKvBarUi();
}

void Widget::recv_turn_counters(int cacheTokens, int promptTokens, int predictedTokens)
{
    if (engineerProxyRuntime_.active)
    {
        recordEngineerUsage(qMax(0, promptTokens), qMax(0, predictedTokens));
        return;
    }
    const int cache = qMax(0, cacheTokens);
    const int prompt = qMax(0, promptTokens);
    const int generated = qMax(0, predictedTokens);
    const int total = cache + prompt + generated;
    if (total <= 0) return;
    kvPromptTokensTurn_ = prompt;
    kvStreamedTurn_ = generated;
    kvTokensTurn_ = prompt + generated;
    kvUsedBeforeTurn_ = cache + prompt;
    kvUsed_ = total;
    if (!turnActive_) turnActive_ = true;
    sawPromptPast_ = true;
    // if (ui_mode == LINK_MODE)
    // {
    //     reflash_state(QStringLiteral("link:timings cache=%1 prompt=%2 generated=%3 total=%4")
    //                       .arg(cache)
    //                       .arg(prompt)
    //                       .arg(generated)
    //                       .arg(total));
    // }
    updateKvBarUi();
}

void Widget::recv_kv_from_net(int usedTokens)
{
    if (engineerProxyRuntime_.active)
    {
        engineerProxyRuntime_.lastGeneratedTokens = qMax(0, usedTokens);
        return;
    }
    const int newStream = qMax(0, usedTokens);
    // Approximate KV usage accumulation during streaming tokens from xNet.
    if (ui_mode == LINK_MODE)
    {
        // In LINK mode, we may not have server logs; accept updates regardless of turnActive_
        if (!turnActive_) turnActive_ = true;
        kvStreamedTurn_ = newStream;
        kvTokensTurn_ = kvPromptTokensTurn_ + kvStreamedTurn_;
        kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
        // if (delta != 0)
        // {
        //     reflash_state(QStringLiteral("link:stream update stream=%1 delta=%2 prompt=%3 turn=%4 used=%5")
        //                       .arg(kvStreamedTurn_)
        //                       .arg(delta)
        //                       .arg(kvPromptTokensTurn_)
        //                       .arg(kvTokensTurn_)
        //                       .arg(kvUsed_));
        // }
        updateKvBarUi();
        return;
    }
    // LOCAL mode
    if (!turnActive_) return;
    kvStreamedTurn_ = newStream;
    kvTokensTurn_ = kvPromptTokensTurn_ + kvStreamedTurn_;
    // apply only after server reported prompt baseline (prompt done)
    if (!sawPromptPast_) return;
    kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
    updateKvBarUi();
}

void Widget::onSlotAssigned(int slotId)
{
    if (slotId < 0) return;
    if (currentSlotId_ == slotId) return;
    currentSlotId_ = slotId;
    if (history_) history_->updateSlotId(slotId);
    reflash_state(QString("net:slot id=%1").arg(slotId), SIGNAL_SIGNAL);
}

void Widget::recv_reasoning_tokens(int tokens)
{
    if (engineerProxyRuntime_.active)
    {
        recordEngineerReasoning(qMax(0, tokens));
        return;
    }
    lastReasoningTokens_ = qMax(0, tokens);
}

void Widget::onServerOutput(const QString &line)
{
    const QString trimmedLine = line.trimmed();
    if (lazyUnloadEnabled())
    {
        markBackendActivity();
        const bool busy = turnActive_ || toolInvocationActive_;
        if (busy)
        {
            cancelLazyUnload(QStringLiteral("log activity"));
        }
        const QString lowerLine = trimmedLine.toLower();
        if (lowerLine.contains(QStringLiteral("all slots are idle")) || lowerLine.contains(QStringLiteral("no pending work")) || lowerLine.contains(QStringLiteral("all clients are idle")))
        {
            scheduleLazyUnload();
        }
        else if (!busy && backendOnline_)
        {
            if (!lazyUnloadTimer_ || !lazyUnloadTimer_->isActive())
            {
                scheduleLazyUnload();
            }
        }
    }


    // Detect fatal/failed patterns in llama.cpp server logs and unlock UI promptly
    {
        const QString l = line.toLower();
        if (!portFallbackInFlight_ && !ui_port.isEmpty() && activeServerPort_ == ui_port)
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
                    portConflictDetected_ = true;
                    initiatePortFallback();
                    return;
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
        // Filter out benign phrases like "no error" if present
        if (hit && !l.contains("no error"))
        {
            reflash_state(QString::fromUtf8("ui:后端异常输出，已解锁控件"), WRONG_SIGNAL);
            if (decode_pTimer && decode_pTimer->isActive()) decode_fail();
            is_run = false;
            unlockButtonsAfterError();
        }
    }
    // 0) Track turn lifecycle heuristics
    if (line.contains("new prompt") || line.contains("launch_slot_"))
    {
        markBackendActivity();
        cancelLazyUnload(QStringLiteral("turn begin"));
        turnActive_ = true;
        kvUsedBeforeTurn_ = kvUsed_;
        kvTokensTurn_ = 0;
        kvPromptTokensTurn_ = 0;
        kvStreamedTurn_ = 0;
        sawFinalPast_ = false;
    }

    int trainCtx = 0;
    if (TextParse::extractIntAfterKeyword(line, QStringLiteral("n_ctx_train"), trainCtx) && trainCtx > 0)
    {
        if (ui_n_ctx_train != trainCtx)
        {
            ui_n_ctx_train = trainCtx;
            if (settings_ui && settings_ui->nctx_slider)
            {
                const int curMax = settings_ui->nctx_slider->maximum();
                if (curMax != trainCtx) settings_ui->nctx_slider->setMaximum(trainCtx);
            }
        }
    }

    int ctxValue = 0;
    if (line.contains(QStringLiteral("llama_context")) && TextParse::extractIntAfterKeyword(line, QStringLiteral("n_ctx"), ctxValue) && ctxValue > 0)
    {
        server_nctx_ = ctxValue;
        const int slotCtx = ui_SETTINGS.nctx > 0 ? ui_SETTINGS.nctx : DEFAULT_NCTX;
        const int parallel = ui_SETTINGS.hid_parallel > 0 ? ui_SETTINGS.hid_parallel : 1;
        const int expectedTotal = slotCtx * parallel;
        if (server_nctx_ != expectedTotal)
        {
            reflash_state(QStringLiteral("ui:server n_ctx=%1, expected=%2 (slot=%3, parallel=%4)")
                              .arg(server_nctx_)
                              .arg(expectedTotal)
                              .arg(slotCtx)
                              .arg(parallel),
                          SIGNAL_SIGNAL);
        }
    }

    int slotCtx = 0;
    if (TextParse::extractIntAfterKeyword(line, QStringLiteral("n_ctx_slot"), slotCtx) && slotCtx > 0)
    {
        slotCtxMax_ = slotCtx;
        enforcePredictLimit();
        updateKvBarUi();
        SETTINGS snap = ui_SETTINGS;
        if (ui_mode == LINK_MODE && slotCtxMax_ > 0) snap.nctx = slotCtxMax_;
        emit ui2expend_settings(snap);
    }

    int kvHit = 0;
    if (TextParse::extractIntBetweenMarkers(line, QStringLiteral("kv cache rm"), QStringLiteral("]"), kvHit))
    {
        kvUsed_ = qMax(0, kvHit);
        updateKvBarUi();
    }

    int layers = 0;
    if (TextParse::extractIntAfterKeyword(line, QStringLiteral("n_layer"), layers) && layers > 0)
    {
        const int maxngl = layers + 1;
        const bool shouldAdoptMax = (maxngl > 0 && (ui_SETTINGS.ngl == 999 || ui_SETTINGS.ngl > maxngl));
        if (ui_maxngl != maxngl)
        {
            ui_maxngl = maxngl;
            if (settings_ui && settings_ui->ngl_slider)
            {
                const int curMax = settings_ui->ngl_slider->maximum();
                if (curMax != maxngl) settings_ui->ngl_slider->setMaximum(maxngl);
            }
        }
        if (shouldAdoptMax)
        {
            ui_SETTINGS.ngl = maxngl;
        }
        if (settings_ui && settings_ui->ngl_slider)
        {
            int curVal = settings_ui->ngl_slider->value();
            if (shouldAdoptMax)
            {
                curVal = ui_SETTINGS.ngl;
            }
            else if (maxngl > 0 && curVal > maxngl)
            {
                curVal = maxngl;
            }
            if (curVal != settings_ui->ngl_slider->value()) settings_ui->ngl_slider->setValue(curVal);
            settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(curVal));
        }
    }

    const QString chatFmt = TextParse::textAfterKeyword(line, QStringLiteral("Chat format:"));
    if (!chatFmt.isEmpty())
    {
        // Reserved for future UI log.
    }

    if (line.contains(QStringLiteral("total time")))
    {
        int totalTokens = 0;
        if (TextParse::extractLastIntBeforeSuffix(line, QStringLiteral("tokens"), totalTokens))
        {
            kvStreamedTurn_ = totalTokens;
            if (ui_mode == LINK_MODE)
            {
                kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
                updateKvBarUi();
            }
            else if (sawPromptPast_ && !sawFinalPast_)
            {
                kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
                updateKvBarUi();
            }
        }
    }

    if (line.contains(QStringLiteral("prompt done")))
    {
        int past = 0;
        if (TextParse::extractIntAfterKeyword(line, QStringLiteral("n_past"), past))
        {
            kvUsed_ = qMax(0, past);
            kvUsedBeforeTurn_ = qMax(0, past);
            kvStreamedTurn_ = 0;
            sawPromptPast_ = true;
            updateKvBarUi();
            markBackendActivity();
            cancelLazyUnload(QStringLiteral("prompt done"));
        }
    }
    if (line.contains(QStringLiteral("stop processing")))
    {
        int past = 0;
        if (TextParse::extractIntAfterKeyword(line, QStringLiteral("n_past"), past))
        {
            kvUsed_ = qMax(0, past);
            sawFinalPast_ = true;
            updateKvBarUi();
            markBackendActivity();
            scheduleLazyUnload();
        }
    }
    // qDebug()<< "Server log:" << line;
}

void Widget::onServerStartFailed(const QString &reason)
{
    backendOnline_ = false;
    lazyWakeInFlight_ = false;
    applyWakeUiLock(false);
    if (proxyServer_) proxyServer_->setBackendAvailable(false);
    cancelLazyUnload(QStringLiteral("backend start failed"));
    pendingSendAfterWake_ = false;

    Q_UNUSED(reason);
    if (!portFallbackInFlight_ && portConflictDetected_ && !ui_port.isEmpty() && activeServerPort_ == ui_port)
    {
        initiatePortFallback();
        return;
    }
    portConflictDetected_ = false;
    const QString selectedBackend = DeviceManager::userChoice();
    const QString resolvedBackend = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main"));
    const QString attemptedBackend = resolvedBackend.isEmpty() ? selectedBackend : resolvedBackend;
    if (!attemptedBackend.isEmpty())
    {
        QString statusLine;
        if (!selectedBackend.isEmpty() && selectedBackend != attemptedBackend)
        {
            statusLine = QStringLiteral("ui:backend start failure (%1 -> %2)").arg(selectedBackend, attemptedBackend);
        }
        else
        {
            statusLine = QStringLiteral("ui:backend start failure -> %1").arg(attemptedBackend);
        }
        reflash_state(statusLine, WRONG_SIGNAL);
    }
    refreshDeviceBackendUI();
    // 停止任何进行中的动画/计时
    if (decode_pTimer) decode_pTimer->stop();
    // 用失败标志收尾“装载中”转轮行
    decode_fail();

    // 清理装载状态，避免后续 serverStopped 被忽略
    lastServerRestart_ = false;
    ignoreNextServerStopped_ = true; // 紧随其后的 serverStopped 属于同一次失败，忽略之
    is_load = false;

    // 解锁界面，允许用户立即调整设置或重新装载
    is_run = false;
    unlockButtonsAfterError();
    if (triggerWin7CpuFallback(QStringLiteral("start failure")))
    {
        return;
    }
    // 明确允许打开“设置/约定”以便更换后端/设备/提示词
    if (ui && ui->set) ui->set->setEnabled(true);
    if (ui && ui->date) ui->date->setEnabled(true);
}

void Widget::unlockLoad()
{
    if (ui_SETTINGS.ngl < ui_maxngl)
    {
        reflash_state("ui:" + jtr("ngl tips"), USUAL_SIGNAL);
    }

    reflash_state("ui:" + jtr("load model") + jtr("over") + " " + QString::number(load_time, 'f', 2) + " s " + jtr("right click and check model log"), SUCCESS_SIGNAL);
    if (ui_SETTINGS.ngl > 0)
    {
        setBaseWindowIcon(QIcon(":/logo/green_logo.png"));
    }
    else
    {
        setBaseWindowIcon(QIcon(":/logo/blue_logo.png"));
    }
    EVA_title = jtr("current model") + " " + ui_SETTINGS.modelpath.split("/").last();
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(max_thread));
    auto_save_user();
    ui_state_normal();
    if (skipUnlockLoadIntro_)
    {
        skipUnlockLoadIntro_ = false; // Already showing prior chat, do not inject system prompt
        return;
    }
    const bool canReuseSystemRecord = (lastSystemRecordIndex_ >= 0 && ui && ui->output && ui->output->document() && !ui->output->document()->isEmpty());
    if (canReuseSystemRecord)
    {
        updateRecordEntryContent(lastSystemRecordIndex_, bot_predecode_content);
        ensureOutputAtBottom();
        return;
    }
    // Record a system header and show system prompt as pre-decode content
    int __idx = recordCreate(RecordRole::System);
    appendRoleHeader(QStringLiteral("system"));
    reflash_output(bot_predecode_content, 0, themeTextPrimary());
    recordAppendText(__idx, bot_predecode_content);
    // Track the injected system record so later prompt refreshes reuse it instead of duplicating blocks
    if (!ui_messagesArray.isEmpty())
    {
        recordEntries_[__idx].msgIndex = 0;
    }
    lastSystemRecordIndex_ = __idx;
}
