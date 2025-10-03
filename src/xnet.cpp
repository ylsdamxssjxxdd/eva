#include "xnet.h"
#include <QSslError>

xNet::xNet()
{
    // Defer creation of network objects until we are in worker thread
    qDebug() << "xNet initialized";
}

xNet::~xNet()
{
    abortActiveReply();
}

void xNet::resetState()
{
    tokens_ = 0;
    thinkFlag = false;
    current_content.clear();
    sseBuffer_.clear();
    aborted_ = false;
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
            reply_->abort();
        }
        reply_->deleteLater();
        reply_ = nullptr;
    }
}

QNetworkRequest xNet::buildRequest(const QUrl &url) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", QByteArray("Bearer ") + apis.api_key.toUtf8());
    req.setRawHeader("Accept", "text/event-stream");
    req.setRawHeader("Connection", "keep-alive");
    req.setRawHeader("Cache-Control", "no-cache");
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(60000); // 60s transfer timeout
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
    ensureNetObjects();
    emit net2ui_state("net:" + jtr("send message to api"));

    // Prepare request
    const bool isChat = !endpoint_data.is_complete_state;
    const QUrl url(isChat ? (apis.api_endpoint + apis.api_chat_endpoint)
                          : (apis.api_endpoint + apis.api_completion_endpoint));
    const QByteArray body = isChat ? createChatBody() : createCompleteBody();
    QNetworkRequest request = buildRequest(url);

    // Fire asynchronous request
    reply_ = nam_->post(request, body);

    // Timers
    t_all_.start();
    bool ttfbStarted = false;
    connect(reply_, &QNetworkReply::readyRead, this, [&, isChat]() {
        if (!ttfbStarted)
        {
            ttfbStarted = true;
            t_first_.start();
        }

        const QByteArray chunk = reply_->readAll();
        if (chunk.isEmpty()) return;
        sseBuffer_.append(chunk);

        // Normalize line endings and process complete SSE events (separated by blank line)
        int idx;
        while ((idx = sseBuffer_.indexOf("\n\n")) != -1)
        {
            QByteArray event = sseBuffer_.left(idx);
            sseBuffer_.remove(0, idx + 2);

            // Find a line starting with "data:"
            // Allow for CRLF and spaces after colon
            QList<QByteArray> lines = event.split('\n');
            for (QByteArray &ln : lines)
            {
                ln = ln.trimmed();
                if (!ln.startsWith("data:")) continue;

                QByteArray payload = ln.mid(5).trimmed();
                if (payload.isEmpty()) continue;
                if (payload == "[DONE]" || payload == "DONE")
                {
                    emit net2ui_state("net: DONE");
                    continue;
                }

                // Some servers may prepend junk before JSON; try to locate '{'
                int jpos = payload.indexOf('{');
                if (jpos > 0) payload.remove(0, jpos);

                QJsonParseError perr{};
                const QJsonDocument doc = QJsonDocument::fromJson(payload, &perr);
                if (perr.error != QJsonParseError::NoError || !doc.isObject())
                {
                    emit net2ui_state("net:resolve json fail", WRONG_SIGNAL);
                    continue;
                }

                QJsonObject obj = doc.object();

                // OpenAI chat format
                if (isChat)
                {
                    const QJsonArray choices = obj.value("choices").toArray();
                    if (choices.isEmpty()) continue;
                    const QJsonObject firstChoice = choices.at(0).toObject();
                    const QString finish = firstChoice.value("finish_reason").toString();
                    const QJsonObject delta = firstChoice.value("delta").toObject();
                    current_content = delta.value("content").toString();

                    QString content_flag = finish == "stop" ? jtr("<end>") : current_content;
                    if (!current_content.isEmpty())
                    {
                        tokens_++;
                        emit net2ui_state("net:" + jtr("recv reply") + " " + content_flag);

                        if (current_content.contains(DEFAULT_THINK_BEGIN)) thinkFlag = true;
                        if (thinkFlag)
                            emit net2ui_output(current_content, true, THINK_GRAY);
                        else
                            emit net2ui_output(current_content, true);
                        if (current_content.contains(DEFAULT_THINK_END)) thinkFlag = false;
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

                    if (!content.isEmpty())
                    {
                        tokens_++;
                        const QString content_flag = stop ? jtr("<end>") : content;
                        emit net2ui_state("net:" + jtr("recv reply") + " " + content_flag);
                        emit net2ui_output(content, true);
                    }
                }
            }

        }

        // Early stop: UI stop or tool stop-word outside of think block
        if (is_stop || (!thinkFlag && current_content.contains(DEFAULT_OBSERVATION_STOPWORD)))
        {
            is_stop = false;
            emit net2ui_state("net:abort by user", SIGNAL_SIGNAL);
            abortActiveReply();
        }
    });

    // Handle finish: stop timeout and finalize
    connect(reply_, &QNetworkReply::finished, this, [this]() {
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
            const double tokps = (tokens_ > 0 && t_first_.isValid()) ? (tokens_ / (t_first_.nsecsElapsed() / 1e9)) : 0.0;

            if (err == QNetworkReply::NoError)
            {
                if (endpoint_data.n_predict == 1)
                    emit net2ui_state("net:" + jtr("use time") + " " + QString::number(tAll, 'f', 2) + " s ", SUCCESS_SIGNAL);
                else
                    emit net2ui_state("net:" + jtr("use time") + " " + QString::number(tAll, 'f', 2) + " s " + jtr("single decode") + " " + QString::number(tokps, 'f', 2) + " token/s", SUCCESS_SIGNAL);

                if (httpCode)
                    emit net2ui_state("net:http " + QString::number(httpCode));
            }
            else
            {
                QString errStr = reply_ ? reply_->errorString() : QString("unknown error");
                emit net2ui_state("net:" + errStr, WRONG_SIGNAL);
                if (httpCode)
                    emit net2ui_state("net:http " + QString::number(httpCode), WRONG_SIGNAL);
            }
        }

        if (reply_)
        {
            reply_->deleteLater();
            reply_ = nullptr;
        }

        running_ = false;
        emit net2ui_pushover();
    });

    // Network errors should not hang the loop
    connect(reply_, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::errorOccurred), this, [this](QNetworkReply::NetworkError) {
        if (timeoutTimer_) timeoutTimer_->stop();
    });
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    connect(reply_, &QNetworkReply::sslErrors, this, [this](const QList<QSslError> &errors) {
        Q_UNUSED(errors);
        // Report but do not ignore by default
        emit net2ui_state("net: SSL error", WRONG_SIGNAL);
    });
#endif

    // Arm an overall timeout (no bytes + no finish)
    if (timeoutTimer_) timeoutTimer_->start(120000); // 120s guard

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
        connect(timeoutTimer_, &QTimer::timeout, this, [this]() {
            emit net2ui_state("net: timeout", WRONG_SIGNAL);
            abortActiveReply();
        });
    }
}

//构造请求的数据体
QByteArray xNet::createChatBody()
{
    // build JSON body (OpenAI-compatible)
    QJsonObject json;
    if (apis.is_cache)
    {
        json.insert("cache_prompt", apis.is_cache);
    }
    json.insert("model", apis.api_model);
    json.insert("stream", true);
    json.insert("temperature", 2 * endpoint_data.temp); // OpenAI temperature 0-2; ours 0-1
    json.insert("max_tokens", endpoint_data.n_predict);

    // stop words
    QJsonArray stopkeys;
    for (int i = 0; i < endpoint_data.stopwords.size(); ++i)
    {
        stopkeys.append(endpoint_data.stopwords.at(i));
    }
    json.insert("stop", stopkeys);

    // sanitize messages so every message.content is a string
        auto toStringOnlyMessages = [&](const QJsonArray &in) -> QJsonArray {
        QJsonArray out;
        auto asString = [](const QJsonValue &c) -> QString {
            if (c.isString()) return c.toString();
            if (c.isArray()) {
                QString s;
                for (const auto &pv : c.toArray()) {
                    if (!pv.isObject()) continue;
                    QJsonObject p = pv.toObject();
                    const QString type = p.value("type").toString();
                    if (type == "text") s += p.value("text").toString();
                    // ignore non-text (image_url/audio_url) to keep server template happy
                }
                return s; // may be empty
            }
            if (c.isNull() || c.isUndefined()) return QString("");
            return c.toVariant().toString();
        };

        // Collect only known roles; ensure content present as string
        for (const auto &v : in) {
            if (!v.isObject()) continue;
            QJsonObject m = v.toObject();
            const QString role = m.value("role").toString();
            if (!(role == DEFAULT_USER_NAME || role == DEFAULT_MODEL_NAME || role == DEFAULT_SYSTEM_NAME)) {
                // unknown role -> skip
                continue;
            }
            QString s = asString(m.value("content"));
            m.insert("role", role);
                        // populate reasoning_content for assistant if think tags present
            if (role == DEFAULT_MODEL_NAME && s.contains(DEFAULT_THINK_END)) {
                QString contentPart = s;
                QString reasoningPart = "";
                // split by </think>
                const int endIdx = s.indexOf(DEFAULT_THINK_END);
                if (endIdx != -1) {
                    QString before = s.left(endIdx);
                    QString after = s.mid(endIdx + QString(DEFAULT_THINK_END).size());
                    // strip <think> from before
                    int startIdx = before.indexOf(DEFAULT_THINK_BEGIN);
                    if (startIdx != -1) before = before.mid(startIdx + QString(DEFAULT_THINK_BEGIN).size());
                    reasoningPart = before.trimmed();
                    contentPart = after.trimmed();
                }
                m.insert("reasoning_content", reasoningPart);
                m.insert("content", contentPart);
            } else {
                m.insert("content", s);
            }
            out.append(m);
        }

        // Ensure first item is system. If not, prepend it from date_prompt
        if (out.size() > 0) {
            const QJsonObject first = out.at(0).toObject();
            if (first.value("role").toString() != DEFAULT_SYSTEM_NAME) {
                QJsonObject systemMessage;
                systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
                systemMessage.insert("content", endpoint_data.date_prompt);
                QJsonArray fixed;
                fixed.append(systemMessage);
                for (const auto &v2 : out) fixed.append(v2);
                return fixed;
            }
        } else {
            // no messages at all -> create system message
            QJsonObject systemMessage;
            systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
            systemMessage.insert("content", endpoint_data.date_prompt);
            out.append(systemMessage);
        }
        return out;
    };

    json.insert("messages", toStringOnlyMessages(endpoint_data.messagesArray));
    {
        // debug summary: roles and content lengths
        QJsonArray dbg = toStringOnlyMessages(endpoint_data.messagesArray);
        QStringList dbgLines;
        for (const auto &v : dbg) {
            if (!v.isObject()) continue;
            auto o = v.toObject();
            auto r = o.value("role").toString();
            auto c = o.value("content").toString();
            dbgLines << (r + ":" + QString::number(c.size()));
        }
        emit net2ui_state("net:send messages -> " + dbgLines.join(", "));
    }
    // qDebug() << QJsonDocument(json).toJson(QJsonDocument::Indented);

    QJsonDocument doc(json);
    return doc.toJson();
}

//构造请求的数据体,补完模式
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
    json.insert("n_predict", endpoint_data.n_predict);
    json.insert("stream", true);
    json.insert("temperature", endpoint_data.temp);

    // 将 JSON 对象转换为字节序列
    QJsonDocument doc(json);
    return doc.toJson();
}
// 传递端点参数
void xNet::recv_data(ENDPOINT_DATA data)
{
    endpoint_data = data;
}
//传递api设置参数
void xNet::recv_apis(APIS apis_)
{
    apis = apis_;
}
void xNet::recv_stop(bool stop)
{
    is_stop = stop;
}

QString xNet::extractContentFromJson(const QString &jsonString)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) return QString();

    QJsonObject jsonObject = doc.object();
    QJsonArray choicesArray = jsonObject["choices"].toArray();
    if (choicesArray.isEmpty()) return QString();

    QJsonObject firstChoice = choicesArray.first().toObject();
    QJsonObject delta = firstChoice["delta"].toObject();
    return delta["content"].toString();
}
// 处理整个原始数据字符串，提取所有 content 字段
QStringList xNet::extractAllContent(const QString &data)
{
    QStringList contentList;

    // 使用正则表达式匹配 JSON 对象
    QRegularExpression re("(\\{.*?\\})");
    QRegularExpressionMatchIterator i = re.globalMatch(data);

    while (i.hasNext())
    {
        QRegularExpressionMatch match = i.next();
        QString json = match.captured(1); // 捕获单个 JSON 对象字符串
        QString content = extractContentFromJson(json);
        if (!content.isEmpty())
        {
            contentList << content;
        }
    }

    return contentList;
}

void xNet::recv_language(int language_flag_)
{
    language_flag = language_flag_;
}

// 根据language.json和language_flag中找到对应的文字
QString xNet::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}




