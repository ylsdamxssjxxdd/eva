#include "widget.h"
#include "ui_widget.h"
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
    // LocalServerManager 使用 readAllStandardOutput/StandardError 读取时，一次回调可能携带：
    // - 多行文本（包含多个 '\n'）
    // - 半行文本（末尾不带 '\n'）
    // 若直接按“整段文本”解析，会导致关键字跨行误命中、max_ngl 被覆盖等问题。
    // 这里统一做分行缓冲：只对“完整的一行”调用解析逻辑。
    if (chunk.isEmpty()) return;

    serverLogLineBuffer_.append(chunk);

    // 极端保护：若后端长时间不换行，缓冲可能无限增长；保留末尾更容易捕获最近错误行。
    const int kMaxBufferedChars = 256 * 1024;
    if (serverLogLineBuffer_.size() > kMaxBufferedChars)
    {
        serverLogLineBuffer_ = serverLogLineBuffer_.right(kMaxBufferedChars);
    }

    while (true)
    {
        const int newline = serverLogLineBuffer_.indexOf('\n');
        if (newline < 0) break;

        QString line = serverLogLineBuffer_.left(newline);
        serverLogLineBuffer_.remove(0, newline + 1);

        if (line.endsWith('\r')) line.chop(1);
        if (line.isEmpty()) continue;

        // processServerOutputLine 返回 true 表示触发了端口回退等“重启路径”，应立即停止继续解析旧日志。
        if (processServerOutputLine(line))
        {
            serverLogLineBuffer_.clear();
            return;
        }
    }
}

bool Widget::processServerOutputLine(const QString &line)
{
    static int s_firstLogs = 0;
    if (s_firstLogs < 6)
    {
        ++s_firstLogs;
        StartupLogger::log(QStringLiteral("[server log %1] %2").arg(s_firstLogs).arg(line.left(200)));
    }
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
            // 外部客户端（通过本地代理直连 API）发起的请求不会走 UI 的 finishTurnFlow()，
            // turnActive_ 可能只会在日志里被置为 true，却没有机会归零，导致倒计时永远“待命”。
            // 当后端明确输出“空闲”提示且当前没有 UI turn 绑定（activeTurnId_==0）时，主动清理 turnActive_。
            if (activeTurnId_ == 0)
            {
                turnActive_ = false;
            }
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

    // -------------------- 视觉输入能力提示（mmproj） --------------------
    // 典型触发场景：用户发送 image_url/input_image 等多模态输入，但当前模型不是视觉模型，
    // 或者未为该模型挂载匹配的 mmproj（视觉模块）。
    // llama-server 往往会输出类似日志并返回 500：
    //   image input is not supported - hint: ... you may need to provide the mmproj
    // 该错误属于“能力不匹配”，用户如果只看到 500 容易误判为网络/后端崩溃，因此在状态区给出明确引导。
    {
        const QString lower = trimmedLine.toLower();
        const bool hitVisionNotSupported =
            lower.contains(QStringLiteral("image input is not supported")) ||
            lower.contains(QStringLiteral("you may need to provide the mmproj")) ||
            (lower.contains(QStringLiteral("mmproj")) && lower.contains(QStringLiteral("image")) && lower.contains(QStringLiteral("not supported")));
        if (hitVisionNotSupported)
        {
            // 去重：同一次请求可能会输出多行相关日志，避免状态区刷屏。
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 kDedupWindowMs = 2500;
            if (nowMs - lastVisionNotSupportedHintMs_ > kDedupWindowMs)
            {
                lastVisionNotSupportedHintMs_ = nowMs;
                reflash_state(QStringLiteral("ui:") + jtr(QStringLiteral("vision not supported hint")), WRONG_SIGNAL);
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
                    return true;
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
        if (ui_mode == LINK_MODE) snap.nctx = (slotCtxMax_ > 0 ? slotCtxMax_ : 0);
        emit ui2expend_settings(snap);
    }

    int kvHit = 0;
    if (TextParse::extractIntBetweenMarkers(line, QStringLiteral("kv cache rm"), QStringLiteral("]"), kvHit))
    {
        kvUsed_ = qMax(0, kvHit);
        updateKvBarUi();
    }

    // -------------------- ngl / max ngl 识别（兼容新版 llama-server 日志） --------------------
    // 说明：
    // - EVA 的 ngl 对应 llama.cpp 的 -ngl/--n-gpu-layers
    // - 旧版常从 "print_info: n_layer = X" 推导 max_ngl = X + 1（+1 为 output layer）
    // - 新版会直接输出 "load_tensors: offloaded A/B layers to GPU"，其中 B 更接近真实“总层数”
    auto applyDetectedMaxNgl = [&](int maxngl)
    {
        // 过滤明显不合理的 max_ngl：
        // - 999 在 EVA 里代表“尽可能拉满”（启动前占位），不应被当成模型真实层数写回
        // - 多模态/外挂组件（mmproj/clip）也可能输出相似日志；这里给一个足够宽松但合理的上限保护
        constexpr int kMaxReasonableMaxNgl = 512;
        if (maxngl <= 0 || maxngl > kMaxReasonableMaxNgl) return;

        // 当同一次装载过程中识别到多个候选值时：优先保留更大的那个，避免被 mmproj/clip 的较小 n_layer 覆盖。
        // 注意：preLoad() 会把 ui_maxngl 清零，因此跨模型装载不会“只增不减”。
        if (ui_maxngl > 0 && maxngl < ui_maxngl) return;

        const int oldMax = ui_maxngl;
        const bool keepAtMaxIntent = (ui_SETTINGS.ngl == 999 || (oldMax > 0 && ui_SETTINGS.ngl == oldMax));
        const bool shouldClamp = (ui_SETTINGS.ngl > maxngl);

        if (ui_maxngl != maxngl)
        {
            ui_maxngl = maxngl;
            if (settings_ui && settings_ui->ngl_slider)
            {
                const int curMax = settings_ui->ngl_slider->maximum();
                if (curMax != maxngl) settings_ui->ngl_slider->setMaximum(maxngl);
            }
        }

        if (keepAtMaxIntent || shouldClamp)
        {
            ui_SETTINGS.ngl = maxngl;
        }

        if (settings_ui && settings_ui->ngl_slider && settings_ui->ngl_label)
        {
            const int sliderValue = qBound(settings_ui->ngl_slider->minimum(), ui_SETTINGS.ngl, settings_ui->ngl_slider->maximum());
            if (sliderValue != settings_ui->ngl_slider->value()) settings_ui->ngl_slider->setValue(sliderValue);
            settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(sliderValue));
        }
    };

    // 新版：优先从 offloaded A/B 解析 max_ngl（B 为总层数，含 output layer）
    {
        const QString lower = trimmedLine.toLower();
        int offloaded = 0;
        int total = 0;
        if (lower.contains(QStringLiteral("load_tensors")) && lower.contains(QStringLiteral("offloaded")) &&
            lower.contains(QStringLiteral("layers")) && lower.contains(QStringLiteral("gpu")) &&
            TextParse::extractFractionAfterKeyword(lower, QStringLiteral("offloaded"), offloaded, total))
        {
            Q_UNUSED(offloaded);

            // 兼容 bug：部分 llama.cpp/llama-server 版本的该行日志中，分母 B 可能是“请求的 n_gpu_layers”
            //（例如 EVA 启动前把 ngl=999 作为“尽可能拉满”的占位，会出现 "... offloaded A/999 ..."），
            // 这不是模型真实总层数，不能用来更新 max_ngl。
            // 真实 max_ngl 会在后续的 n_layer 行里出现（如 llm_load_print_meta / llama_model_load）。
            applyDetectedMaxNgl(total);
        }
    }

    // 旧版/兜底：解析 n_layer 推导 max_ngl = n_layer + 1。
    // 兼容多种 llama.cpp 日志前缀（新版不一定再输出 print_info:）。
    int layers = 0;
    {
        const QString lower = trimmedLine.toLower();
        const bool looksLikeMetaLine =
            lower.startsWith(QStringLiteral("print_info:")) ||
            lower.startsWith(QStringLiteral("llm_load_print_meta:")) ||
            lower.startsWith(QStringLiteral("llama_model_load:")) ||
            lower.startsWith(QStringLiteral("llama_model_loader:"));

        if (looksLikeMetaLine &&
            TextParse::extractIntAfterKeyword(trimmedLine, QStringLiteral("n_layer"), layers) && layers > 0)
        {
            applyDetectedMaxNgl(layers + 1);
        }
    }

    // 若行前缀不匹配，则不解析 n_layer，避免误读其它模块（如 vision hparams）的同名字段。

    const QString chatFmt = TextParse::textAfterKeyword(line, QStringLiteral("Chat format:"));
    if (!chatFmt.isEmpty())
    {
        // Reserved for future UI log.
    }

    // llama-server 日志在不同版本里字段名会变化：
    // - 旧版常见：n_past=xxx
    // - 新版常见：n_tokens = xxx
    // 这里统一做兼容解析，避免 UI 的“记忆容量(KV)”统计失真或只在最后一刻跳变。
    auto extractPastLikeTokens = [&](const QString &text, int &out) -> bool
    {
        // 先尝试更“语义化”的 n_past，再回退到 n_tokens
        if (TextParse::extractIntAfterKeyword(text, QStringLiteral("n_past"), out)) return true;
        if (TextParse::extractIntAfterKeyword(text, QStringLiteral("n_tokens"), out)) return true;
        return false;
    };

    if (line.contains(QStringLiteral("total time")))
    {
        int totalTokens = 0;
        if (TextParse::extractLastIntBeforeSuffix(line, QStringLiteral("tokens"), totalTokens))
        {
            // 该行的 token 统计是“本轮总计”(prompt+output)的近似值，不是“流式输出token数”。
            // 仅当 stop processing 尚未给出最终 n_tokens/n_past 时，用它作为兜底刷新 UI。
            if (!sawFinalPast_)
            {
                const int totalUsed = qMax(0, totalTokens);
                if (sawPromptPast_)
                {
                    kvStreamedTurn_ = qMax(0, totalUsed - kvUsedBeforeTurn_);
                    kvTokensTurn_ = kvPromptTokensTurn_ + kvStreamedTurn_;
                    kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
                }
                else
                {
                    kvUsed_ = totalUsed;
                }
                updateKvBarUi();
            }
        }
    }

    if (line.contains(QStringLiteral("prompt done")))
    {
        int past = 0;
        if (extractPastLikeTokens(line, past))
        {
            const int baseline = qMax(0, past);
            kvUsed_ = baseline;
            kvUsedBeforeTurn_ = baseline;
            // 在本地模式下，把“prompt done 的 tokens”作为本轮 prompt 基线（便于 UI 与后续流式累加保持一致）
            kvPromptTokensTurn_ = baseline;
            kvStreamedTurn_ = 0;
            kvTokensTurn_ = kvPromptTokensTurn_ + kvStreamedTurn_;
            sawPromptPast_ = true;
            updateKvBarUi();
            markBackendActivity();
            cancelLazyUnload(QStringLiteral("prompt done"));
        }
    }
    if (line.contains(QStringLiteral("stop processing")))
    {
        int past = 0;
        if (extractPastLikeTokens(line, past))
        {
            const int finalUsed = qMax(0, past);
            kvUsed_ = finalUsed;
            if (sawPromptPast_)
            {
                kvStreamedTurn_ = qMax(0, finalUsed - kvUsedBeforeTurn_);
                kvTokensTurn_ = kvPromptTokensTurn_ + kvStreamedTurn_;
            }
            sawFinalPast_ = true;
            updateKvBarUi();
            markBackendActivity();
            scheduleLazyUnload();
        }
    }
    // qDebug()<< "Server log:" << line;
    return false;
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
