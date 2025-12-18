#include "xnet.h"
#include "prompt_builder.h"
#include "utils/flowtracer.h"
#if QT_CONFIG(ssl)
#include <QSslError>
#endif
#include <functional>

namespace
{
void maybeAttachReasoningPayload(QJsonObject &json, const QString &effort, bool useLegacySchema)
{
    const QString normalized = sanitizeReasoningEffort(effort);
    if (!isReasoningEffortActive(normalized)) return;
    QString finalEffort = normalized;
    if (finalEffort == QStringLiteral("auto"))
    {
        finalEffort = QStringLiteral("medium");
    }
    if (useLegacySchema)
    {
        QJsonObject reasoning;
        reasoning.insert(QStringLiteral("effort"), finalEffort);
        json.insert(QStringLiteral("reasoning"), reasoning);
    }
    else
    {
        json.insert(QStringLiteral("reasoning_effort"), finalEffort);
        json.insert(QStringLiteral("include_reasoning"), true);
    }
}

// -------------------- OpenAI 兼容 usage 解析工具 --------------------
// 说明：
// - 许多“OpenAI 兼容”服务商的 usage 字段形态并不完全一致（int/string/object 混用、字段名变化等）。
// - LINK 模式下 UI 的“记忆量/KV 用量”无法从 llama.cpp 日志读取，需要依赖 usage 来校准：
//   - prompt/input tokens：作为本轮 prompt 基线
//   - completion/output/total tokens：作为本轮生成 token（包含思考/推理 token）
// - 这里的解析尽量“宽松”，避免因为字段形态差异导致统计直接失效。
static inline int toIntLoose(const QJsonValue &v, int fallback = -1)
{
    if (v.isDouble()) return int(v.toDouble());
    if (v.isString())
    {
        bool ok = false;
        const int x = v.toString().toInt(&ok);
        if (ok) return x;
    }
    return fallback;
}

static inline int usageInt(const QJsonObject &u, const QString &key, int fallback = -1)
{
    if (!u.contains(key)) return fallback;
    return toIntLoose(u.value(key), fallback);
}

static inline int parseTotalTokensLoose(const QJsonObject &u)
{
    if (!u.contains(QStringLiteral("total_tokens"))) return -1;
    const QJsonValue tv = u.value(QStringLiteral("total_tokens"));
    if (tv.isDouble() || tv.isString()) return toIntLoose(tv, -1);
    if (tv.isObject())
    {
        // 兼容：total_tokens: { input: x, output: y }
        const QJsonObject tt = tv.toObject();
        const int inTok = usageInt(tt, QStringLiteral("input"), -1);
        const int outTok = usageInt(tt, QStringLiteral("output"), -1);
        if (inTok >= 0 && outTok >= 0) return inTok + outTok;
        if (inTok >= 0) return inTok; // 罕见：只给 input
        if (outTok >= 0) return outTok;
    }
    return -1;
}

static inline int parseReasoningTokensLoose(const QJsonObject &u)
{
    // 1) OpenAI：completion_tokens_details.reasoning_tokens
    if (u.contains(QStringLiteral("completion_tokens_details")) && u.value(QStringLiteral("completion_tokens_details")).isObject())
    {
        const QJsonObject d = u.value(QStringLiteral("completion_tokens_details")).toObject();
        int r = usageInt(d, QStringLiteral("reasoning_tokens"), -1);
        // 兼容：部分实现用 thinking_tokens 命名
        if (r < 0) r = usageInt(d, QStringLiteral("thinking_tokens"), -1);
        if (r >= 0) return r;
    }
    // 2) 兼容：output_tokens_details.reasoning_tokens
    if (u.contains(QStringLiteral("output_tokens_details")) && u.value(QStringLiteral("output_tokens_details")).isObject())
    {
        const QJsonObject d = u.value(QStringLiteral("output_tokens_details")).toObject();
        int r = usageInt(d, QStringLiteral("reasoning_tokens"), -1);
        if (r < 0) r = usageInt(d, QStringLiteral("thinking_tokens"), -1);
        if (r >= 0) return r;
    }
    // 3) 兼容：顶层直接给 reasoning_tokens
    {
        const int r = usageInt(u, QStringLiteral("reasoning_tokens"), -1);
        if (r >= 0) return r;
    }
    return -1;
}
} // namespace

QString xNet::turnTag() const
{
    if (turn_id_ == 0) return QString();
    return QStringLiteral("[turn%1]").arg(turn_id_);
}

void xNet::emitFlowLog(const QString &msg, SIGNAL_STATE state)
{
    FlowTracer::log(FlowChannel::Net, msg, turn_id_);
    Q_UNUSED(state);
}

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
        abortActiveReply(AbortReason::Other);
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
    firstByteSeen_ = false;
    t_first_.invalidate();
    aborted_ = false;
    abortReason_ = AbortReason::None;
    speedsEmitted_ = false;
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

void xNet::abortActiveReply(AbortReason reason)
{
    // 仅记录第一次中断原因，避免多次 stop 覆盖工具中断等信息
    if (abortReason_ == AbortReason::None) abortReason_ = reason;
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

void xNet::emitSpeedsIfAvailable(bool allowFallback)
{
    // 统一封装速度上报：优先使用服务器 timings，缺失时可在工具中断场景下回退为本地估算
    if (speedsEmitted_) return;

    double promptPerSec = -1.0;
    double genPerSec = -1.0;

    if (timingsReceived_)
    {
        // 服务器侧测速：优先直接给出的 per_second，其次根据 tokens/ms 计算
        promptPerSec = (promptPerSec_ >= 0.0)
                           ? promptPerSec_
                           : ((promptMs_ > 0.0 && promptTokens_ >= 0) ? (1000.0 * double(promptTokens_) / promptMs_) : -1.0);
        genPerSec = (predictedPerSec_ >= 0.0)
                        ? predictedPerSec_
                        : ((predictedMs_ > 0.0 && predictedTokens_ >= 0) ? (1000.0 * double(predictedTokens_) / predictedMs_) : -1.0);
    }
    else if (allowFallback)
    {
        // 工具调用命中停符被提前中断时，服务器不会回传 timings；使用本地计数做近似提示
        if (tokens_ > 0 && t_first_.isValid())
        {
            const double elapsed = t_first_.nsecsElapsed() / 1e9;
            if (elapsed > 0.0) genPerSec = double(tokens_) / elapsed;
        }
    }

    if (promptPerSec > 0.0 || genPerSec > 0.0)
    {
        speedsEmitted_ = true;
        emit net2ui_speeds(promptPerSec, genPerSec);
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
    const bool allowHttp2 = (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, allowHttp2);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(0); // disable per-transfer timeout for SSE; rely on our inactivity guard
#endif
    return req;
}

void xNet::run()
{
    if (running_)
    {
        emitFlowLog("net: busy, request ignored", SIGNAL_SIGNAL);
        return;
    }

    running_ = true;
    turn_id_ = endpoint_data.turn_id;
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
    logRequestPayload(isChat ? "chat" : "complete", body);
    QNetworkRequest request = buildRequest(url);
    emitFlowLog(QStringLiteral("net:req %1 url=%2 model=%3 npredict=%4")
                    .arg(isChat ? QStringLiteral("chat") : QStringLiteral("complete"),
                         url.toString(),
                         apis.api_model,
                         QString::number(endpoint_data.n_predict)),
                SIGNAL_SIGNAL);

    // Fire asynchronous request
    reply_ = nam_->post(request, body);
    if (!reply_)
    {
        running_ = false;
        emitFlowLog("net: failed to create request", WRONG_SIGNAL);
        emit net2ui_pushover();
        return;
    }

    // Timers
    t_all_.start();
    connReadyRead_ = connect(reply_, &QNetworkReply::readyRead, this, [this, isChat]()
                              {
        if (aborted_ || !reply_) return; // guard against late events after abort
        // Refresh inactivity guard on any activity
        if (timeoutTimer_) timeoutTimer_->start(120000);
        if (!firstByteSeen_)
        {
            firstByteSeen_ = true;
            t_first_.start();
            emitFlowLog("net:stream begin", SIGNAL_SIGNAL);
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

            processSsePayload(isChat, payloadAgg);
            if (aborted_ || !reply_) return; // 流被主动中止时立即退出解析循环
        }
    });

    // Handle finish: stop timeout and finalize

    connFinished_ = connect(reply_, &QNetworkReply::finished, this, [this]()
                            {
        if (timeoutTimer_) timeoutTimer_->stop();

        // Determine if finish is due to user abort/cancel
        const auto err = reply_ ? reply_->error() : QNetworkReply::NoError;
        const bool canceled = aborted_ || (err == QNetworkReply::OperationCanceledError);
        const AbortReason finishReason = abortReason_;
        const bool toolInterrupted = (finishReason == AbortReason::ToolStop);

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
                emitFlowLog(QStringLiteral("net:done http=%1 tokens=%2 promptTok=%3 genTok=%4")
                                .arg(httpCode)
                                .arg(tokens_)
                                .arg(promptTokens_)
                                .arg(predictedTokens_),
                            SIGNAL_SIGNAL);
            }
            else
            {
                QString errStr = reply_ ? reply_->errorString() : QString("unknown error");
                emitFlowLog(QStringLiteral("net:error http=%1 %2").arg(httpCode).arg(errStr), WRONG_SIGNAL);
            }
            // If we streamed provider-specific reasoning without explicit </think>, close it now
            if (extThinkActive_)
            {
                emit net2ui_output(QString(DEFAULT_THINK_END), true);
                extThinkActive_ = false;
                thinkFlag = false;
            }
        }

        // 工具链中断（需要继续发送 observation）仍然希望看到速度提示；用户手动终止则跳过
        if (!canceled || toolInterrupted)
        {
            emitSpeedsIfAvailable(toolInterrupted);
        }

        if (reply_)
        {
            reply_->deleteLater();
            reply_ = nullptr;
        }

        abortReason_ = AbortReason::None;
        running_ = false;
        // Report reasoning token count of this turn before finishing
        emit net2ui_reasoning_tokens(reasoningTokensTurn_);
        emit net2ui_pushover(); });

    // Network errors should not hang the loop
    connError_ = connect(reply_, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::errorOccurred), this, [this](QNetworkReply::NetworkError)
                         {
        if (timeoutTimer_) timeoutTimer_->stop();
        FlowTracer::log(FlowChannel::Net,
                        QStringLiteral("net: network error reply"),
                        turn_id_); });
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0) && QT_CONFIG(ssl)
    connSslErrors_ = connect(reply_, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &errors)
                             {
        Q_UNUSED(errors);
        // Report but do not ignore by default
        emit net2ui_state("net: SSL error", WRONG_SIGNAL);
        FlowTracer::log(FlowChannel::Net, QStringLiteral("net: ssl error"), turn_id_); });
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
            emitFlowLog("net: timeout", WRONG_SIGNAL);
            abortActiveReply(AbortReason::Timeout); });
    }
}

// 构造请求的数据体
QByteArray xNet::createChatBody()
{ // Build JSON body in OpenAI-compatible chat format with multimodal support
    QJsonObject json;
    const bool isLocal = apis.is_local_backend;
    if (isLocal && apis.is_cache)
    {
        json.insert("cache_prompt", apis.is_cache);
    }
    json.insert("model", apis.api_model);
    json.insert("stream", true);
    if (isLocal)
    {
        json.insert("include_usage", true);
    }
    else
    {
        QJsonObject streamOptions;
        streamOptions.insert(QStringLiteral("include_usage"), true);
        json.insert(QStringLiteral("stream_options"), streamOptions);
    }
    const int requestedPredict = endpoint_data.n_predict;
    const bool hasManualPredict = (requestedPredict > 0);
    if (hasManualPredict)
    {
        const int cappedPredict = qBound(1, requestedPredict, 99999);
        json.insert("n_predict", cappedPredict);
        if (!isLocal)
        {
            json.insert("max_completion_tokens", cappedPredict);
        }
    }
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

    // Only local llama.cpp accepts these sampling knobs
    if (isLocal)
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
    if (isLocal && endpoint_data.id_slot >= 0) { json.insert("id_slot", endpoint_data.id_slot); }
    maybeAttachReasoningPayload(json, endpoint_data.reasoning_effort, isLocal);

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
    const bool isLocal = apis.is_local_backend;
    if (isLocal && apis.is_cache)
    {
        json.insert("cache_prompt", apis.is_cache);
    } // 缓存上文
    json.insert("model", apis.api_model);
    json.insert("prompt", endpoint_data.input_prompt);
    const int requestedPredict2 = endpoint_data.n_predict;
    const bool hasManualPredict2 = (requestedPredict2 > 0);
    if (hasManualPredict2)
    {
        const int cappedPredict2 = qBound(1, requestedPredict2, 99999);
        json.insert("n_predict", cappedPredict2);
        if (!isLocal)
        {
            json.insert("max_completion_tokens", cappedPredict2);
        }
    }
    json.insert("stream", true);
    if (isLocal)
    {
        json.insert("include_usage", true);
    }
    else
    {
        QJsonObject streamOptions;
        streamOptions.insert(QStringLiteral("include_usage"), true);
        json.insert(QStringLiteral("stream_options"), streamOptions);
    }
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
    if (isLocal)
    {
        json.insert("top_k", endpoint_data.top_k);
        json.insert("repeat_penalty", endpoint_data.repeat);
    }
    if (isLocal && endpoint_data.id_slot >= 0) { json.insert("id_slot", endpoint_data.id_slot); }
    maybeAttachReasoningPayload(json, endpoint_data.reasoning_effort, isLocal);

    // 将 JSON 对象转换为字节序列
    QJsonDocument doc(json);
    return doc.toJson();
}
// 传递端点参数

void xNet::logRequestPayload(const char *modeTag, const QByteArray &body)
{
    // Pretty print outgoing payloads to help diagnose bad parameters
    if (!modeTag) modeTag = "request";

    QJsonParseError parseErr{};
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);

    // 避免日志被 data:image/base64 占满：截断 url 保留长度信息
    auto truncateDataUrl = [](const QString &url) -> QString {
        if (!url.startsWith(QStringLiteral("data:image"), Qt::CaseInsensitive)) return url;
        const int maxLen = 96;
        if (url.size() <= maxLen) return url;
        return url.left(maxLen) + QStringLiteral("... (len=%1)").arg(url.size());
    };
    std::function<QJsonObject(const QJsonObject &)> sanitizeObject;
    std::function<QJsonArray(const QJsonArray &)> sanitizeArray;
    sanitizeArray = [&](const QJsonArray &arr) -> QJsonArray {
        QJsonArray out = arr;
        for (int i = 0; i < out.size(); ++i)
        {
            const QJsonValue v = out.at(i);
            if (v.isObject())
                out[i] = sanitizeObject(v.toObject());
            else if (v.isArray())
                out[i] = sanitizeArray(v.toArray());
        }
        return out;
    };
    sanitizeObject = [&](const QJsonObject &obj) -> QJsonObject {
        QJsonObject out = obj;
        for (auto it = out.begin(); it != out.end(); ++it)
        {
            const QString key = it.key();
            QJsonValue val = it.value();
            if (val.isObject())
            {
                val = sanitizeObject(val.toObject());
            }
            else if (val.isArray())
            {
                val = sanitizeArray(val.toArray());
            }
            else if (val.isString() && key.compare(QStringLiteral("url"), Qt::CaseInsensitive) == 0)
            {
                val = truncateDataUrl(val.toString());
            }
            it.value() = val;
        }
        return out;
    };

    QByteArray pretty = body;
    if (parseErr.error == QJsonParseError::NoError && !doc.isNull())
    {
        if (doc.isObject())
            doc = QJsonDocument(sanitizeObject(doc.object()));
        else if (doc.isArray())
            doc = QJsonDocument(sanitizeArray(doc.array()));
        pretty = doc.toJson(QJsonDocument::Indented);
    }

    const QString prefix = QStringLiteral("net:%1 payload -> ").arg(QString::fromLatin1(modeTag));
    const QString combined = prefix + QString::fromUtf8(pretty);

    qDebug().noquote() << combined;
    // emit net2ui_state(combined, SIGNAL_SIGNAL);
}

void xNet::processSsePayload(bool isChat, const QByteArray &payload)
{
    QByteArray data = payload;
    bool toolStopTriggered = false; // 检测到工具调用停符后用于打断后续解析
    // Some servers prepend junk before JSON; locate first '{' or '['
    int jposObj = data.indexOf('{');
    int jposArr = data.indexOf('[');
    int jpos = -1;
    if (jposObj >= 0 && jposArr >= 0) jpos = qMin(jposObj, jposArr);
    else jpos = qMax(jposObj, jposArr);
    if (jpos > 0) data.remove(0, jpos);

    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
    if (perr.error != QJsonParseError::NoError || (!doc.isObject() && !doc.isArray()))
    {
        // noisy providers may send partials; keep silent in UI
        FlowTracer::log(FlowChannel::Net,
                        QStringLiteral("net: json parse fail %1").arg(perr.errorString()),
                        turn_id_);
        return;
    }

    auto processObj = [&](const QJsonObject &obj)
    {
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
                Q_UNUSED(finish);
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
                // qDebug() << current_content;
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
                    const bool hitToolStopword = !thinkFlag && !sawToolStopword_ &&
                                                 current_content.contains(DEFAULT_OBSERVATION_STOPWORD);
                    if (hitToolStopword) sawToolStopword_ = true;
                    // 不再在状态区流式输出内容；仅把内容流式发往输出区
                    if (current_content.contains(DEFAULT_THINK_BEGIN)) thinkFlag = true;
                    if (thinkFlag)
                        emit net2ui_output(current_content, true, THINK_GRAY);
                    else
                        emit net2ui_output(current_content, true);
                    if (current_content.contains(DEFAULT_THINK_END)) thinkFlag = false;
                    if (hitToolStopword && !aborted_)
                    {
                        FlowTracer::log(FlowChannel::Net,
                                        QStringLiteral("net: tool stopword hit, abort stream"),
                                        turn_id_);
                        toolStopTriggered = true;
                        abortActiveReply(AbortReason::ToolStop); // 立即中止流式请求，避免模型继续输出干扰工具判定
                        return;
                    }
                }
            }
        }
        else // completion format (llama.cpp server style)
        {
            // try: top-level { content, stop }
            QString content;
            if (obj.contains("content"))
                content = obj.value("content").toString();

            // fallback: nested { completion, tokens }
            if (content.isEmpty() && obj.contains("completion"))
                content = obj.value("completion").toString();
            // OAI /v1/completions streaming: choices[0].text
            if (content.isEmpty() && obj.contains("choices") && obj.value("choices").isArray())
            {
                QJsonArray chs = obj.value("choices").toArray();
                if (!chs.isEmpty())
                {
                    QJsonObject first = chs.at(0).toObject();
                    if (first.contains("text") && first.value("text").isString())
                    {
                        content = first.value("text").toString();
                    }
                    // finish_reason may still be useful for future ABIs, ignore for now
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

        // Parse OpenAI-style usage if provided (LINK mode baseline should be prompt tokens only)
        if (obj.contains("usage") && obj.value("usage").isObject())
        {
            const QJsonObject u = obj.value("usage").toObject();
            // ---------- 1) 抽取 usage 的关键字段 ----------
            // prompt/input tokens：本轮 prompt 基线
            int promptTokens = usageInt(u, QStringLiteral("prompt_tokens"), -1);
            if (promptTokens < 0) promptTokens = usageInt(u, QStringLiteral("input_tokens"), -1);

            // completion/output tokens：本轮生成 token（多数实现会把“思考/推理 token”算在这里）
            int completionTokens = usageInt(u, QStringLiteral("completion_tokens"), -1);
            if (completionTokens < 0) completionTokens = usageInt(u, QStringLiteral("output_tokens"), -1);

            // total tokens：本轮总 token（最可靠的“包含思考 token”的口径）
            const int totalTokens = parseTotalTokensLoose(u);

            // reasoning/thinking tokens：若提供单独字段，用于 UI 侧展示与对齐调试
            const int reasoningTokens = parseReasoningTokensLoose(u);

            // ---------- 2) 推导 prompt 基线 ----------
            // 优先使用显式 prompt/input tokens；缺失时再用 total - completion / total - streamed(近似) 推导。
            if (promptTokens < 0 && totalTokens >= 0 && completionTokens >= 0)
            {
                const int derived = totalTokens - completionTokens;
                if (derived >= 0) promptTokens = derived;
            }
            if (promptTokens < 0 && totalTokens >= 0 && tokens_ >= 0)
            {
                // 兜底：当服务商只返回 total_tokens，且没有 prompt/completion 字段时，只能用“已流式输出的 chunk 数”近似。
                // 注意：tokens_ 是 1 chunk ~= 1 token 的近似计数，误差取决于服务商的分片策略。
                const int derived = totalTokens - tokens_;
                if (derived >= 0) promptTokens = derived;
            }
            if (promptTokens >= 0) emit net2ui_prompt_baseline(promptTokens);

            // ---------- 3) LINK 模式：用 usage 校准 UI 的 KV/记忆量统计 ----------
            // 目标：
            // - 本轮“记忆用量” = promptTokens + completionTokensEffective
            // - completionTokensEffective 要尽量包含 reasoning/thinking token（有些模型不会把它流式输出到 content）
            //
            // 实现策略：
            // - 若 totalTokens 可用：completion = total - prompt（最可靠，天然包含思考 token）
            // - 否则：退回使用 completion/output tokens（可能已包含思考 token）
            // - 若能得到 prompt 与 completion：发 net2ui_turn_counters(0, prompt, completion) 让 UI 一次性对齐
            //
            // 注意：
            // - 若服务器同时提供 llama.cpp timings，则优先使用 timings（包含 cache_n/prompt_n/predicted_n），避免重复上报。
            if (!apis.is_local_backend && !timingsReceived_)
            {
                int completionEffective = completionTokens;
                if (totalTokens >= 0 && promptTokens >= 0)
                {
                    const int derived = totalTokens - promptTokens;
                    if (derived >= 0) completionEffective = derived;
                }
                else if (completionEffective >= 0 && reasoningTokens > 0)
                {
                    // 兼容：少数实现会把 reasoning/thinking token 作为“额外字段”单独上报，
                    // 且 completion/output tokens 仅包含可见输出（不含思考）。
                    // 当缺失 total_tokens 时，我们宁愿把 reasoningTokens 加进来，以免 LINK 模式下 KV 用量明显偏小。
                    completionEffective += reasoningTokens;
                }

                if (promptTokens >= 0 && completionEffective >= 0)
                {
                    emit net2ui_turn_counters(0, promptTokens, completionEffective);
                }
            }

            // ---------- 4) 记录 reasoning token（用于 UI 调试/展示） ----------
            // 某些推理模型（或服务商）不会把思考内容写入 content，而是只在 usage.details 里提供 reasoning_tokens。
            // 这里用 provider 的上报值覆盖/修正本轮 reasoningTokensTurn_，确保 UI 能拿到“包含思考”的统计量。
            if (reasoningTokens >= 0)
            {
                reasoningTokensTurn_ = qMax(0, reasoningTokens);
            }
        }
    };

    if (doc.isObject())
    {
        processObj(doc.object());
        if (toolStopTriggered) return;
    }
    else if (doc.isArray())
    {
        const QJsonArray arr = doc.array();
        for (const auto &v : arr)
        {
            if (toolStopTriggered) break;
            if (v.isObject()) processObj(v.toObject());
            if (toolStopTriggered) break;
        }
    }
}
void xNet::recv_data(ENDPOINT_DATA data)
{
    endpoint_data = data;
    turn_id_ = data.turn_id;
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
        abortActiveReply(AbortReason::ApiChange);
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
        abortActiveReply(AbortReason::UserStop); // cancels reply_, disconnects signals, emits pushover
    }
}

// Legacy JSON extract helpers removed; SSE parser now handles deltas incrementally.

void xNet::recv_language(int language_flag_)
{
    language_flag = language_flag_;
}

void xNet::recv_turn(quint64 turnId)
{
    turn_id_ = turnId;
}

// 根据language.json和language_flag中找到对应的文字
QString xNet::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}
