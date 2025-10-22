#include "xnet.h"
#include "prompt_builder.h"
#if QT_CONFIG(ssl)
#include <QSslError>
#endif

xNet::xNet()
{
    // Defer creation of network objects until we are in worker thread
    qDebug() << "xNet init over";
}

xNet::~xNet()
{
    // Ensure cleanup happens only in the owning thread to avoid QWinEventNotifier warnings on Windows
    if (QThread::currentThread() == this->thread())
    {
        abortActiveReply();
        if (timeoutTimer_)
        {
            timeoutTimer_->stop();
            timeoutTimer_->deleteLater();
            timeoutTimer_ = nullptr;
        }
        if (nam_)
        {
            nam_->deleteLater();
            nam_ = nullptr;
        }
    }
    else
    {
        // Skip cross-thread cleanup; objects will be reclaimed on process exit
    }
}

void xNet::resetState()
{
    tokens_ = 0;
    thinkFlag = false;
    current_content.clear();
    sseBuffer_.clear();
    aborted_ = false;
    reasoningTokensTurn_ = 0;
    extThinkActive_ = false;
    sawToolStopword_ = false;
    // reset timing stats
    promptTokens_ = -1;
    promptMs_ = 0.0;
    predictedTokens_ = -1;
    predictedMs_ = 0.0;
    promptPerSec_ = -1.0;
    predictedPerSec_ = -1.0;
    timingsReceived_ = false;
    cacheTokens_ = -1;
    totalsEmitted_ = false;
}

void xNet::abortActiveReply()
{
    if (timeoutTimer_) timeoutTimer_->stop();
    if (reply_)
    {
        // Ensure we signal an abort only once
        if (!aborted_)
        {
            aborted_ = true;
        }

        // disconnect all our slots from this reply first to prevent late callbacks
        QObject::disconnect(connReadyRead_);
        QObject::disconnect(connFinished_);
        QObject::disconnect(connError_);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0) && QT_CONFIG(ssl)
        QObject::disconnect(connSslErrors_);
#endif
#endif
        connReadyRead_ = QMetaObject::Connection{};
        connFinished_ = QMetaObject::Connection{};
        connError_ = QMetaObject::Connection{};
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0) && QT_CONFIG(ssl)
        connSslErrors_ = QMetaObject::Connection{};
#endif
#endif
        // finally, abort and delete the reply safely
        reply_->abort();
        reply_->deleteLater();
        reply_ = nullptr;
        running_ = false;
        emit net2ui_pushover();
    }
}

QNetworkRequest xNet::buildRequest(const QUrl &url) const

{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Only attach Authorization when api_key is present
    if (!apis.api_key.trimmed().isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + apis.api_key.toUtf8());
    req.setRawHeader("Accept", "text/event-stream");
    req.setRawHeader("Connection", "keep-alive");
    req.setRawHeader("Cache-Control", "no-cache");
    // FollowRedirectsAttribute is deprecated; use RedirectPolicyAttribute
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(0); // disable per-transfer timeout for SSE; rely on our inactivity guard
#endif
    return req;
}

void xNet::run()
{
    if (running_)
    {
        emit net2ui_state("net: busy, request ignored", SIGNAL_SIGNAL);
        return;
    }

    running_ = true;
    resetState();
    // Clear any stale stop flag from previous aborted turn
    is_stop = false;
    ensureNetObjects();
    // emit net2ui_state("net:" + jtr("send message to api"));

    // Prepare request
    const bool isChat = !endpoint_data.is_complete_state;
    const QUrl url(isChat ? (apis.api_endpoint + apis.api_chat_endpoint)
                          : (apis.api_endpoint + apis.api_completion_endpoint));
    const QByteArray body = isChat ? createChatBody() : createCompleteBody();
    QNetworkRequest request = buildRequest(url);

    // Fire asynchronous request
    reply_ = nam_->post(request, body);
    if (!reply_)
    {
        running_ = false;
        emit net2ui_state("net: failed to create request", WRONG_SIGNAL);
        emit net2ui_pushover();
        return;
    }

    // Timers
    t_all_.start();
    bool ttfbStarted = false;
    connReadyRead_ = connect(reply_, &QNetworkReply::readyRead, this, [&, isChat]()
                             {
        if (aborted_ || !reply_) return; // guard against late events after abort
        // Refresh inactivity guard on any activity
        if (timeoutTimer_) timeoutTimer_->start(120000);
        if (!ttfbStarted)
        {
            ttfbStarted = true;
            t_first_.start();
        }

        QByteArray chunk = reply_->readAll();
        if (chunk.isEmpty()) return;
        // Normalize CRLF to LF for the newly arrived bytes only
        chunk.replace("\r\n", "\n");
        sseBuffer_.append(chunk);

        // Guard against unbounded growth when server sends noisy/partial data
        static const int kMaxSseBuffer = 4 * 1024 * 1024; // 4MB
        if (sseBuffer_.size() > kMaxSseBuffer)
        {
            // Drop up to first newline or half of the buffer if no newline found
            int cut = sseBuffer_.indexOf('\n');
            if (cut < 0 || cut > sseBuffer_.size() / 2) cut = sseBuffer_.size() / 2;
            sseBuffer_.remove(0, cut);
            emit net2ui_state("net: sse overflow drop", SIGNAL_SIGNAL);
        }

        // Process complete SSE events separated by a blank line
        int idx;
        while ((idx = sseBuffer_.indexOf("\n\n")) != -1)
        {
            QByteArray event = sseBuffer_.left(idx);
            sseBuffer_.remove(0, idx + 2);

            // Accumulate all data: lines into one payload
            QList<QByteArray> lines = event.split('\n');
            QByteArray payloadAgg;
            for (QByteArray ln : lines)
            {
                ln = ln.trimmed();
                // qDebug() << "net: sse line" << ln;
                if (!ln.startsWith("data:")) continue;
                QByteArray part = ln.mid(5).trimmed();
                if (part.isEmpty()) continue;
                if (!payloadAgg.isEmpty()) payloadAgg.append('\n');
                payloadAgg.append(part);
            }
            if (payloadAgg.isEmpty()) continue;
            if (payloadAgg == "[DONE]" || payloadAgg == "DONE")
            {
                continue; // end-of-stream marker
            }

            // Some servers prepend junk before JSON; locate first '{' or '['
            int jposObj = payloadAgg.indexOf('{');
            int jposArr = payloadAgg.indexOf('[');
            int jpos = -1;
            if (jposObj >= 0 && jposArr >= 0) jpos = qMin(jposObj, jposArr);
            else jpos = qMax(jposObj, jposArr);
            if (jpos > 0) payloadAgg.remove(0, jpos);

            QJsonParseError perr{};
            const QJsonDocument doc = QJsonDocument::fromJson(payloadAgg, &perr);
            if (perr.error != QJsonParseError::NoError || (!doc.isObject() && !doc.isArray()))
            {
                // noisy providers may send partials; keep silent in UI
                qDebug() << "net: json parse fail" << perr.errorString();
                continue;
            }

            auto processObj = [&](const QJsonObject &obj) {
                    // capture server-assigned slot id for KV reuse
                    if (obj.contains("slot_id"))
                    {
                        const int sid = obj.value("slot_id").toInt(-1);
                        if (sid >= 0) emit net2ui_slot_id(sid);
                    }
                    // OpenAI chat format
                    if (isChat)
                    {
                        const QJsonArray choices = obj.value("choices").toArray();
                        if (!choices.isEmpty())
                        {
                            const QJsonObject firstChoice = choices.at(0).toObject();
                            const QString finish = firstChoice.value("finish_reason").toString();
                            const QJsonObject delta = firstChoice.value("delta").toObject();
                            // 1) xAI/OpenAI reasoning fields (no <think> markers)
                            QString reasoning = delta.value("reasoning_content").toString();
                            if (reasoning.isEmpty() && delta.contains("reasoning"))
                            {
                                // Some providers stream as string under `reasoning`
                                const QJsonValue rv = delta.value("reasoning");
                                if (rv.isString()) reasoning = rv.toString();
                            }

                            if (!reasoning.isEmpty())
                            {
                                // Open synthetic think block at first reasoning token
                                if (!extThinkActive_)
                                {
                                    extThinkActive_ = true;
                                    thinkFlag = true; // mark inside think for token counters
                                    // Emit a begin marker so UI opens a Think section
                                    emit net2ui_output(QString(DEFAULT_THINK_BEGIN), true);
                                }
                                // Count reasoning token and update KV indicator
                                tokens_++;
                                reasoningTokensTurn_++;
                                emit net2ui_kv_tokens(tokens_);
                                // Stream reasoning in gray
                                emit net2ui_output(reasoning, true, THINK_GRAY);
                            }

                            // 2) Normal assistant content
                            current_content = delta.value("content").toString();

                            // If reasoning section is open and normal content arrives, close think.
                            if (extThinkActive_ && !current_content.isEmpty())
                            {
                                current_content = QString(DEFAULT_THINK_END) + current_content;
                                extThinkActive_ = false;
                                thinkFlag = false;
                            }

                            if (!current_content.isEmpty())
                            {
                                tokens_++;
                                // notify UI to update approximate KV usage during streaming (LINK mode fallback)
                                emit net2ui_kv_tokens(tokens_);
                                // if this chunk is part of <think>, count it approximately
                                const bool isReasoningChunk = thinkFlag || current_content.contains(DEFAULT_THINK_BEGIN);
                                if (isReasoningChunk) reasoningTokensTurn_++;
                                // Detect tool-call end; do not abort here, just mark seen to allow tail meta
                                if (!thinkFlag && !sawToolStopword_ && current_content.contains(DEFAULT_OBSERVATION_STOPWORD))
                                {
                                    sawToolStopword_ = true;
                                }
                                // 不再在状态区流式输出内容；仅把内容流式发往输出区
                                if (current_content.contains(DEFAULT_THINK_BEGIN)) thinkFlag = true;
                                if (thinkFlag)
                                    emit net2ui_output(current_content, true, THINK_GRAY);
                                else
                                    emit net2ui_output(current_content, true);
                                if (current_content.contains(DEFAULT_THINK_END)) thinkFlag = false;
                            }
                        }
                    }
                    else // completion format (llama.cpp server style)
                    {
                        // try: top-level { content, stop }
                        QString content;
                        bool stop = false;
                        if (obj.contains("content"))
                            content = obj.value("content").toString();
                        if (obj.contains("stop"))
                            stop = obj.value("stop").toBool();

                        // fallback: nested { completion, tokens }
                        if (content.isEmpty() && obj.contains("completion"))
                            content = obj.value("completion").toString();
                        // OAI /v1/completions streaming: choices[0].text
                        if (content.isEmpty() && obj.contains("choices") && obj.value("choices").isArray()) {
                            QJsonArray chs = obj.value("choices").toArray();
                            if (!chs.isEmpty()) {
                                QJsonObject first = chs.at(0).toObject();
                                if (first.contains("text") && first.value("text").isString()) {
                                    content = first.value("text").toString();
                                }
                                if (first.contains("finish_reason") && first.value("finish_reason").isString()) {
                                    stop = (first.value("finish_reason").toString() == "stop");
                                }
                            }
                        }

                        if (!content.isEmpty())
                        {
                            tokens_++;
                            // notify UI of streamed token for fallback memory/speed in LINK mode
                            emit net2ui_kv_tokens(tokens_);
                            // completion style may also contain <think>
                            const bool isReasoningChunk = thinkFlag || content.contains(DEFAULT_THINK_BEGIN);
                            if (isReasoningChunk) reasoningTokensTurn_++;
                            // 不再在状态区流式输出内容；仅把内容流式发往输出区
                            emit net2ui_output(content, true);
                        }
                    }

                    // Parse optional timings from llama.cpp server and cache for final reporting
                                        // Parse OpenAI-style usage if provided (for LINK mode prompt baseline)
                    // Parse OpenAI-style usage if provided (LINK mode baseline should be prompt tokens only)
                    if (obj.contains("usage") && obj.value("usage").isObject())
                    {
                        const QJsonObject u = obj.value("usage").toObject();
                        int promptTokens = -1;
                        // Prefer explicit prompt/input tokens for baseline
                        if (u.contains("prompt_tokens")) promptTokens = u.value("prompt_tokens").toInt(-1);
                        if (promptTokens < 0 && u.contains("input_tokens")) promptTokens = u.value("input_tokens").toInt(-1);
                        // Fallback: derive prompt tokens from total - streamed (approx) when provider only exposes totals
                        if (promptTokens < 0 && u.contains("total_tokens"))
                        {
                            int totalTokens = -1;
                            const QJsonValue tv = u.value("total_tokens");
                            if (tv.isDouble() || tv.isString()) totalTokens = tv.toInt(-1);
                            if (tv.isObject())
                            {
                                const QJsonObject tt = tv.toObject();
                                int inTok = tt.value("input").toInt(-1);
                                int outTok = tt.value("output").toInt(-1);
                                if (inTok >= 0 && outTok >= 0) totalTokens = inTok + outTok;
                                else if (inTok >= 0) totalTokens = inTok; // rare partial info
                            }
                            if (totalTokens >= 0 && tokens_ >= 0)
                            {
                                int derived = totalTokens - tokens_;
                                if (derived >= 0) promptTokens = derived;
                            }
                        }
                        if (promptTokens >= 0) emit net2ui_prompt_baseline(promptTokens);
                    }
                    if (obj.contains("timings") && obj.value("timings").isObject())
                    {
                        const QJsonObject tobj = obj.value("timings").toObject();
                        // Values follow llama.cpp tools/server result_timings
                        promptTokens_ = tobj.value("prompt_n").toInt(promptTokens_);
                        promptMs_ = tobj.value("prompt_ms").toDouble(promptMs_);
                        predictedTokens_ = tobj.value("predicted_n").toInt(predictedTokens_);
                        predictedMs_ = tobj.value("predicted_ms").toDouble(predictedMs_);
                        cacheTokens_ = tobj.value("cache_n").toInt(cacheTokens_);
                        timingsReceived_ = true;
                        // optional direct speeds (tokens/sec) if provided by server
                        if (tobj.contains("prompt_per_second")) promptPerSec_ = tobj.value("prompt_per_second").toDouble(promptPerSec_);
                        if (tobj.contains("predicted_per_second")) predictedPerSec_ = tobj.value("predicted_per_second").toDouble(predictedPerSec_);
                        // Emit final cache/prompt/predicted totals once all fields are known
                        if (!totalsEmitted_)
                        {
                            const int cache = cacheTokens_;
                            const int prompt = promptTokens_;
                            const int gen = predictedTokens_;
                            if (cache >= 0 && prompt >= 0 && gen >= 0)
                            {
                                totalsEmitted_ = true;
                                emit net2ui_turn_counters(cache, prompt, gen);
                            }
                        }
                    }
                };

                if (doc.isObject())
                {
                    processObj(doc.object());
                }
                else if (doc.isArray())
                {
                    const QJsonArray arr = doc.array();
                    for (const auto &v : arr)
                    {
                        if (v.isObject()) processObj(v.toObject());
                    }
                }
            } });

    // Handle finish: stop timeout and finalize

    connFinished_ = connect(reply_, &QNetworkReply::finished, this, [this]()
                            {
        if (timeoutTimer_) timeoutTimer_->stop();

        // Determine if finish is due to user abort/cancel
        const auto err = reply_ ? reply_->error() : QNetworkReply::NoError;
        const bool canceled = aborted_ || (err == QNetworkReply::OperationCanceledError);

        if (!canceled)
        {
            // Normal finish -> report metrics and http code
            const QVariant codeVar = reply_ ? reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute) : QVariant();
            const int httpCode = codeVar.isValid() ? codeVar.toInt() : 0;
            const double tAll = t_all_.nsecsElapsed() / 1e9;
            Q_UNUSED(tAll);
            const double tokps = (tokens_ > 0 && t_first_.isValid()) ? (tokens_ / (t_first_.nsecsElapsed() / 1e9)) : 0.0;
            Q_UNUSED(tokps);

            if (err == QNetworkReply::NoError)
            {

            }
            else
            {
                QString errStr = reply_ ? reply_->errorString() : QString("unknown error");
                emit net2ui_state("net:" + errStr, WRONG_SIGNAL);
                if (httpCode)
                    emit net2ui_state("net:http " + QString::number(httpCode), WRONG_SIGNAL);
            }
            // If we streamed provider-specific reasoning without explicit </think>, close it now
            if (extThinkActive_)
            {
                emit net2ui_output(QString(DEFAULT_THINK_END), true);
                extThinkActive_ = false;
                thinkFlag = false;
            }
            // Report final speeds from timings if available
            if (timingsReceived_) {
                double promptPerSec = promptPerSec_ >= 0.0 ? promptPerSec_ : ((promptMs_ > 0.0 && promptTokens_ >= 0) ? (1000.0 * double(promptTokens_) / promptMs_) : -1.0);
                double genPerSec = predictedPerSec_ >= 0.0 ? predictedPerSec_ : ((predictedMs_ > 0.0 && predictedTokens_ >= 0) ? (1000.0 * double(predictedTokens_) / predictedMs_) : -1.0);
                emit net2ui_speeds(promptPerSec, genPerSec);
            }

        }

        if (reply_)
        {
            reply_->deleteLater();
            reply_ = nullptr;
        }

        running_ = false;
        // Report reasoning token count of this turn before finishing
        emit net2ui_reasoning_tokens(reasoningTokensTurn_);
        emit net2ui_pushover(); });

    // Network errors should not hang the loop
    connError_ = connect(reply_, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::errorOccurred), this, [this](QNetworkReply::NetworkError)
                         {
        if (timeoutTimer_) timeoutTimer_->stop(); });
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0) && QT_CONFIG(ssl)
    connSslErrors_ = connect(reply_, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &errors)
                             {
        Q_UNUSED(errors);
        // Report but do not ignore by default
        emit net2ui_state("net: SSL error", WRONG_SIGNAL); });
#endif

    // Arm an overall timeout (no bytes + no finish)
    if (timeoutTimer_) timeoutTimer_->start(120000); // 120s guard 超时设置

    // Fully async: return immediately
}

void xNet::ensureNetObjects()
{
    // Ensure QNetworkAccessManager and QTimer live in our current thread
    if (!nam_)
    {
        nam_ = new QNetworkAccessManager(this);
    }
    if (!timeoutTimer_)
    {
        timeoutTimer_ = new QTimer(this);
        timeoutTimer_->setSingleShot(true);
        connect(timeoutTimer_, &QTimer::timeout, this, [this]()
                {
            emit net2ui_state("net: timeout", WRONG_SIGNAL);
            abortActiveReply(); });
    }
}

// 构造请求的数据体
QByteArray xNet::createChatBody()
{ // Build JSON body in OpenAI-compatible chat format with multimodal support
    QJsonObject json;
    if (apis.is_cache)
    {
        json.insert("cache_prompt", apis.is_cache);
    }
    json.insert("model", apis.api_model);
    json.insert("stream", true);
    json.insert("include_usage", true);
    {
        double t = qBound(0.0, 2.0 * double(endpoint_data.temp), 2.0);
        json.insert("temperature", t); // OpenAI range [0,2]
    }
    /* top_k only for local llama.cpp */
    // expose full sampling knobs in LINK/LOCAL request body
    {
        double p = qBound(0.0, double(endpoint_data.top_p), 1.0);
        json.insert("top_p", p);
    }
    /* repeat_penalty only for local llama.cpp */

    // stop words
    QJsonArray stopkeys;
    for (int i = 0; i < endpoint_data.stopwords.size(); ++i)
    {
        stopkeys.append(endpoint_data.stopwords.at(i));
    }
    json.insert("stop", stopkeys);

    // Heuristic: treat localhost/LAN as local llama.cpp endpoint
    QUrl __ep = QUrl::fromUserInput(apis.api_endpoint);
    QString __host = __ep.host().toLower();
    const bool __isLocal = __host.isEmpty() || __host == "localhost" || __host == "127.0.0.1" || __host.startsWith("192.") || __host.startsWith("10.") || __host.startsWith("172.");
    if (__isLocal)
    {
        json.insert("top_k", endpoint_data.top_k);
        json.insert("repeat_penalty", endpoint_data.repeat);
    }

    // Normalize UI messages into OpenAI-compatible messages
    QJsonArray oaiMessages = promptx::buildOaiChatMessages(endpoint_data.messagesArray, endpoint_data.date_prompt,
                                                           QStringLiteral(DEFAULT_SYSTEM_NAME),
                                                           QStringLiteral(DEFAULT_USER_NAME),
                                                           QStringLiteral(DEFAULT_MODEL_NAME));
    // Some remote providers (e.g., OpenRouter/xAI) do not accept role="tool" unless using
    // OpenAI-native tool_calls schema. We do not use tool_calls; instead we stream a plain
    // observation back to the model. To maximize compatibility, convert any historical
    // tool messages to a user message prefixed with DEFAULT_OBSERVATION_NAME.
    QJsonArray compatMsgs;
    for (const auto &v : oaiMessages)
    {
        if (!v.isObject())
        {
            compatMsgs.append(v);
            continue;
        }
        QJsonObject m = v.toObject();
        const QString role = m.value("role").toString();
        if (role == QStringLiteral("tool"))
        {
            QString content;
            const QJsonValue cv = m.value("content");
            if (cv.isString())
                content = cv.toString();
            else if (cv.isArray())
            {
                // flatten parts to text if needed
                QStringList parts;
                for (const auto &pv : cv.toArray())
                {
                    if (pv.isObject())
                    {
                        QJsonObject po = pv.toObject();
                        if (po.value("type").toString() == QStringLiteral("text"))
                            parts << po.value("text").toString();
                    }
                }
                content = parts.join(QString());
            }
            QJsonObject u;
            u.insert("role", QStringLiteral("user"));
            u.insert("content", QString(DEFAULT_OBSERVATION_NAME) + content);
            compatMsgs.append(u);
        }
        else
        {
            compatMsgs.append(m);
        }
    }
    json.insert("messages", compatMsgs);
    // Reuse llama.cpp server slot KV cache if available
    if (__isLocal && endpoint_data.id_slot >= 0) { json.insert("id_slot", endpoint_data.id_slot); }

    // debug summary: role and content kind/length
    QStringList dbgLines;
    for (const auto &v : oaiMessages)
    {
        if (!v.isObject()) continue;
        QJsonObject o = v.toObject();
        QString r = o.value("role").toString();
        QJsonValue c = o.value("content");
        if (c.isString())
            dbgLines << (r + ":" + QString::number(c.toString().size()));
        else if (c.isArray())
            dbgLines << (r + ":parts=" + QString::number(c.toArray().size()));
        else
            dbgLines << (r + ":0");
    }
    // emit net2ui_state("net:send messages -> " + dbgLines.join(", "));

    QJsonDocument doc(json);
    return doc.toJson();
}

// 构造请求的数据体,补完模式
QByteArray xNet::createCompleteBody()
{
    // 创建 JSON 数据
    QJsonObject json;
    if (apis.is_cache)
    {
        json.insert("cache_prompt", apis.is_cache);
    } // 缓存上文
    json.insert("model", apis.api_model);
    json.insert("prompt", endpoint_data.input_prompt);
    json.insert("max_tokens", endpoint_data.n_predict);
    json.insert("stream", true);
    json.insert("include_usage", true);
    {
        double t = qBound(0.0, 2.0 * double(endpoint_data.temp), 2.0);
        json.insert("temperature", t); // OpenAI range [0,2]
    }
    /* top_k only for local llama.cpp */
    {
        double p = qBound(0.0, double(endpoint_data.top_p), 1.0);
        json.insert("top_p", p);
    }
    /* repeat_penalty only for local llama.cpp */
    // optional stop sequences
    QJsonArray stopkeys2;
    for (int i = 0; i < endpoint_data.stopwords.size(); ++i)
    {
        stopkeys2.append(endpoint_data.stopwords.at(i));
    }
    json.insert("stop", stopkeys2);
    QUrl __ep2 = QUrl::fromUserInput(apis.api_endpoint);
    QString __host2 = __ep2.host().toLower();
    const bool __isLocal2 = __host2.isEmpty() || __host2 == "localhost" || __host2 == "127.0.0.1" || __host2.startsWith("192.") || __host2.startsWith("10.") || __host2.startsWith("172.");
    if (__isLocal2)
    {
        json.insert("top_k", endpoint_data.top_k);
        json.insert("repeat_penalty", endpoint_data.repeat);
    }
    if (__isLocal2 && endpoint_data.id_slot >= 0) { json.insert("id_slot", endpoint_data.id_slot); }

    // 将 JSON 对象转换为字节序列
    QJsonDocument doc(json);
    return doc.toJson();
}
// 传递端点参数
void xNet::recv_data(ENDPOINT_DATA data)
{
    endpoint_data = data;
}
// 传递api设置参数
void xNet::recv_apis(APIS apis_)
{
    // If API parameters changed, abort current reply and reset state so next run uses new settings
    const bool changed = (apis.api_endpoint != apis_.api_endpoint) ||
                         (apis.api_key != apis_.api_key) ||
                         (apis.api_model != apis_.api_model) ||
                         (apis.api_chat_endpoint != apis_.api_chat_endpoint) ||
                         (apis.api_completion_endpoint != apis_.api_completion_endpoint) ||
                         (apis.is_cache != apis_.is_cache);
    apis = apis_;
    if (changed)
    {
        abortActiveReply();
        resetState();
        // emit net2ui_state("net: apis updated", SIGNAL_SIGNAL);
    }
}
void xNet::recv_stop(bool stop)
{
    // Immediate, deterministic stop: abort active network reply in-place.
    // Do not rely on periodic checks; user expects instant termination.
    is_stop = stop;
    if (stop)
    {
        // emit net2ui_state("net:abort by user", SIGNAL_SIGNAL);
        abortActiveReply(); // cancels reply_, disconnects signals, emits pushover
    }
}

// Legacy JSON extract helpers removed; SSE parser now handles deltas incrementally.

void xNet::recv_language(int language_flag_)
{
    language_flag = language_flag_;
}

// 根据language.json和language_flag中找到对应的文字
QString xNet::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}
