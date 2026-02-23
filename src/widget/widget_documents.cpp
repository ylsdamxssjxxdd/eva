#include "widget.h"
#include "ui_widget.h"
#include "service/backend/backend_coordinator.h"
#include <QDateTime>
#include <QTextDocument>
#include <QPlainTextDocumentLayout>
#include "../utils/textparse.h"
#include "../utils/startuplogger.h"
#include "../utils/flowtracer.h"

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
    // 优先展示真实探测到的上下文上限，LINK 模式下未探测成功则用“未知”而非默认值
    const int cap = resolvedContextLimitForUi();
    const bool capKnown = cap > 0;

    int used = qMax(0, kvUsed_);
    if (capKnown && used > cap) used = cap;

    // 统一换算为百分比：KV 条只用第二段（橙色）表示占用
    int percent = (capKnown && cap > 0) ? int(qRound(100.0 * double(used) / double(cap))) : 0;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    // 视觉最小值：只要 used>0，就至少显示 1%，避免“明明有记忆但条是空的”
    if (capKnown && used > 0 && percent == 0) percent = 1;

    // 强制进度范围为 0..100，避免 UI 组件被外部 setRange 影响
    if (ui->kv_bar->maximum() != 100 || ui->kv_bar->minimum() != 0) ui->kv_bar->setRange(0, 100);
    ui->kv_bar->setValue(0);
    ui->kv_bar->setSecondValue(percent);
    ui->kv_bar->setShowText(jtr("kv bar label"));
    // 中心文本与其它进度条保持一致：显示百分比（由 DoubleQProgressBar 默认绘制）
    const QString capLabel = resolvedContextLabelForUi();
    ui->kv_bar->setCenterText(QString());
    ui->kv_bar->setToolTip(jtr("kv bar tooltip").arg(used).arg(capLabel));
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
    // 若已从 llama-server 日志拿到 stop processing 的最终 n_tokens/n_past，则以其为准，避免流式近似覆盖最终值
    if (sawFinalPast_) return;
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
    const int sanitized = qMax(0, tokens);
    const int previous = lastReasoningTokens_;
    lastReasoningTokens_ = sanitized;

    // 调试：LINK 模式下 provider 可能会额外上报 reasoning tokens（思考 token）。
    // 这些 token 通常不会回写到最终对话内容，但会计入本轮推理/计费 token。
    // 当前 KV 统计选择“包含思考 token”，用来观察实际推理负载。
    // 这里打印“收到上报”的时机与数值，便于你对齐本轮 KV 汇总日志。
    if (ui_mode == LINK_MODE && sanitized > 0 && sanitized != previous)
    {
        FlowTracer::log(
            FlowChannel::Session,
            QStringLiteral("link:reasoning tokens update=%1 prev=%2 kvUsed=%3 usedBefore=%4 prompt=%5 stream=%6 turn=%7")
                .arg(sanitized)
                .arg(previous)
                .arg(kvUsed_)
                .arg(kvUsedBeforeTurn_)
                .arg(kvPromptTokensTurn_)
                .arg(kvStreamedTurn_)
                .arg(kvTokensTurn_),
            activeTurnId_);
    }
}

void Widget::onServerOutput(const QString &chunk)
{
    if (backendCoordinator_)
        backendCoordinator_->onServerOutput(chunk);
}



bool Widget::processServerOutputLine(const QString &line)
{
    return backendCoordinator_ ? backendCoordinator_->processServerOutputLine(line) : false;
}



void Widget::onServerStartFailed(const QString &reason)
{
    if (backendCoordinator_)
        backendCoordinator_->onServerStartFailed(reason);
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
        setBaseWindowIcon(QIcon(":/logo/eva.png"));
    }
    else
    {
        setBaseWindowIcon(QIcon(":/logo/eva.png"));
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
    reflash_output_tool_highlight(bot_predecode_content, themeTextPrimary());
    recordAppendText(__idx, bot_predecode_content);
    // Track the injected system record so later prompt refreshes reuse it instead of duplicating blocks
    if (!ui_messagesArray.isEmpty())
    {
        recordEntries_[__idx].msgIndex = 0;
    }
    lastSystemRecordIndex_ = __idx;
}
