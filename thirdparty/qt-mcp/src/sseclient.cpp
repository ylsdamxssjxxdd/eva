#include "qmcp/sseclient.h"

#include "qmcp/errors.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QTimer>
#include <QUuid>

namespace {

Q_LOGGING_CATEGORY(lcSseClient, "qmcp.sseclient");

QString idFromValue(const QJsonValue& value) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(static_cast<qint64>(value.toDouble()));
    }
    return QString();
}

QUrl normalizeEndpoint(const QUrl& origin, const QString& endpoint) {
    const QUrl base = [&origin]() {
        QUrl copy(origin);
        copy.setQuery(QString());
        copy.setFragment(QString());
        return copy;
    }();

    QUrl candidate(endpoint);
    if (!candidate.isValid()) {
        candidate = QUrl(QString::fromUtf8(endpoint.toUtf8()));
    }

    if (candidate.isRelative() || candidate.scheme().isEmpty()) {
        return base.resolved(candidate);
    }

    return candidate;
}

QString headerValueToString(const QJsonValue& value) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 16);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isNull()) {
        return QString();
    }
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
}

QJsonObject valueToObject(const QJsonValue& value) {
    return value.isObject() ? value.toObject() : QJsonObject{};
}

} // namespace

namespace qmcp {

SseClient::SseClient(const ServerConfig& config, QObject* parent)
    : McpClient(config, parent), m_sseUrl(config.baseUrl), m_origin(config.baseUrl) {
    m_origin.setQuery(QString());
    m_origin.setFragment(QString());
}

bool SseClient::initialize(const QString& clientName, const QString& clientVersion) {
    if (!ensureStream()) {
        qCWarning(lcSseClient) << "Failed to start SSE stream for" << config().name;
        return false;
    }

    if (!waitForEndpoint(60000)) {
        qCWarning(lcSseClient) << "Timeout waiting for message endpoint from" << config().name;
        return false;
    }

    try {
        const QJsonValue response = sendRequest(QStringLiteral("initialize"),
                                                buildInitializeParams(clientName, clientVersion));
        if (response.isObject()) {
            const QJsonObject obj = response.toObject();
            if (obj.contains(QStringLiteral("capabilities"))) {
                setServerCapabilities(obj.value(QStringLiteral("capabilities")).toObject());
            }
        }
        sendNotification(QStringLiteral("notifications/initialized"));
        return true;
    } catch (const std::exception& ex) {
        qCWarning(lcSseClient) << "Initialization failed:" << ex.what();
        return false;
    }
}

QJsonArray SseClient::listTools(const QJsonObject& pagination) {
    const QJsonValue response = sendRequest(QStringLiteral("tools/list"), pagination);
    if (response.isObject()) {
        const QJsonObject obj = response.toObject();
        if (obj.value(QStringLiteral("tools")).isArray()) {
            return obj.value(QStringLiteral("tools")).toArray();
        }
    } else if (response.isArray()) {
        return response.toArray();
    }
    return {};
}

QJsonObject SseClient::callTool(const QString& toolName, const QJsonObject& arguments) {
    QJsonObject params{
        {QStringLiteral("name"), toolName},
        {QStringLiteral("arguments"), arguments}
    };
    const QJsonValue response = sendRequest(QStringLiteral("tools/call"), params);
    return response.toObject();
}

QJsonObject SseClient::listResources(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("resources/list"), pagination));
}

QJsonObject SseClient::listResourceTemplates(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("resources/templates/list"), pagination));
}

QJsonObject SseClient::readResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/read"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject SseClient::subscribeResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/subscribe"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject SseClient::unsubscribeResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/unsubscribe"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject SseClient::listPrompts(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("prompts/list"), pagination));
}

QJsonObject SseClient::getPrompt(const QString& name, const QJsonObject& arguments) {
    QJsonObject params{{QStringLiteral("name"), name}};
    if (!arguments.isEmpty()) {
        params.insert(QStringLiteral("arguments"), arguments);
    }
    return valueToObject(sendRequest(QStringLiteral("prompts/get"), params));
}

QJsonObject SseClient::complete(const QJsonObject& reference,
                                const QJsonObject& argument,
                                const QJsonObject& context) {
    QJsonObject params{
        {QStringLiteral("ref"), reference},
        {QStringLiteral("argument"), argument}
    };
    if (!context.isEmpty()) {
        params.insert(QStringLiteral("context"), context);
    }
    return valueToObject(sendRequest(QStringLiteral("completion/complete"), params));
}

void SseClient::sendProgressNotification(const QString& token,
                                         double progress,
                                         double total,
                                         const QString& message) {
    QJsonObject params{
        {QStringLiteral("progressToken"), token},
        {QStringLiteral("progress"), progress}
    };
    if (total > 0.0) {
        params.insert(QStringLiteral("total"), total);
    }
    if (!message.isEmpty()) {
        params.insert(QStringLiteral("message"), message);
    }
    sendNotification(QStringLiteral("notifications/progress"), params);
}

void SseClient::setLoggingLevel(const QString& level) {
    sendRequest(QStringLiteral("logging/setLevel"), QJsonObject{{QStringLiteral("level"), level}});
}

QJsonObject SseClient::ping() {
    return valueToObject(sendRequest(QStringLiteral("ping"), QJsonObject{}));
}

void SseClient::notifyRootsChanged() {
    sendNotification(QStringLiteral("roots/list_changed"));
}

bool SseClient::isConnected() const {
    return m_streamOpen && m_sseReply;
}

bool SseClient::ensureStream() {
    if (m_streamOpen && m_sseReply) {
        return true;
    }

    if (!m_sseUrl.isValid()) {
        return false;
    }

    QNetworkRequest request(m_sseUrl);
    request.setRawHeader("Accept", "text/event-stream");
    request.setRawHeader("Cache-Control", "no-cache");
    applyHeaders(request);
    qCInfo(lcSseClient) << "Opening SSE stream to" << m_sseUrl;

    if (m_sseReply) {
        m_sseReply->deleteLater();
        m_sseReply = nullptr;
    }

    m_sseReply = m_sseManager.get(request);
    if (!m_sseReply) {
        return false;
    }

    connect(m_sseReply, &QNetworkReply::readyRead, this, &SseClient::handleSseReadyRead);
    connect(m_sseReply, &QNetworkReply::finished, this, &SseClient::handleSseFinished);
    connect(m_sseReply, &QNetworkReply::errorOccurred, this, &SseClient::handleSseError);

    m_streamOpen = true;
    m_sseBuffer.clear();
    return true;
}

bool SseClient::waitForEndpoint(int timeoutMs) {
    if (m_messageEndpoint.isValid()) {
        return true;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    qCInfo(lcSseClient) << "Waiting for SSE endpoint for up to" << timeoutMs << "ms";
    QObject::connect(this, &SseClient::endpointReady, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    timer.stop();
    const bool ready = m_messageEndpoint.isValid();
    qCInfo(lcSseClient) << "Endpoint wait finished, valid?" << ready << "endpoint" << m_messageEndpoint;
    return ready;
}

void SseClient::applyHeaders(QNetworkRequest& request) const {
    const QJsonObject headers = config().headers;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        const QString value = headerValueToString(it.value());
        if (!value.isEmpty()) {
            request.setRawHeader(it.key().toUtf8(), value.toUtf8());
        }
    }
}

void SseClient::handleSseReadyRead() {
    if (!m_sseReply) {
        return;
    }
    m_sseBuffer.append(m_sseReply->readAll());
    while (true) {
        int delimiterIndex = m_sseBuffer.indexOf("\n\n");
        int delimiterLength = 2;
        const int crlfIndex = m_sseBuffer.indexOf("\r\n\r\n");
        if (crlfIndex != -1 && (delimiterIndex == -1 || crlfIndex < delimiterIndex)) {
            delimiterIndex = crlfIndex;
            delimiterLength = 4;
        }
        if (delimiterIndex == -1) {
            break;
        }
        QByteArray chunk = m_sseBuffer.left(delimiterIndex);
        m_sseBuffer.remove(0, delimiterIndex + delimiterLength);
        processEvent(chunk);
    }
}

void SseClient::handleSseFinished() {
    qCInfo(lcSseClient) << "SSE stream finished";
    m_streamOpen = false;
    m_messageEndpoint = QUrl();
}

void SseClient::handleSseError(QNetworkReply::NetworkError code) {
    qCWarning(lcSseClient) << "SSE stream error:" << code << (m_sseReply ? m_sseReply->errorString() : QString());
}

void SseClient::processEvent(const QByteArray& rawEvent) {
    QList<QByteArray> lines = rawEvent.split('\n');
    QString eventType = QStringLiteral("message");
    QByteArray dataPayload;

    for (QByteArray line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.startsWith("event:")) {
            eventType = QString::fromUtf8(line.mid(6)).trimmed();
        } else if (line.startsWith("data:")) {
            if (!dataPayload.isEmpty()) {
                dataPayload.append('\n');
            }
            dataPayload.append(line.mid(5).trimmed());
        }
    }

    if (dataPayload.isEmpty()) {
        return;
    }

    if (eventType == QStringLiteral("heartbeat")) {
        return;
    }

    if (eventType == QStringLiteral("endpoint")) {
        const QString endpoint = QString::fromUtf8(dataPayload).trimmed();
        m_messageEndpoint = normalizeEndpoint(m_origin, endpoint);
        emit endpointReady();
        qCInfo(lcSseClient) << "Updated message endpoint to" << m_messageEndpoint;
        return;
    }

    if (eventType != QStringLiteral("message")) {
        qCWarning(lcSseClient) << "Unknown SSE event type" << eventType;
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(dataPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcSseClient) << "Invalid SSE payload:" << parseError.errorString();
        return;
    }

    const QJsonObject message = doc.object();
    const bool hasMethod = message.contains(QStringLiteral("method"));
    const bool hasResultOrError = message.contains(QStringLiteral("result")) || message.contains(QStringLiteral("error"));
    const bool hasIdField = message.contains(QStringLiteral("id"));

    if (hasMethod && !hasResultOrError && !hasIdField) {
        const QString method = message.value(QStringLiteral("method")).toString();
        const QJsonObject params = message.value(QStringLiteral("params")).toObject();
        handleServerNotification(method, params);
        return;
    }

    if (hasMethod && !hasResultOrError && hasIdField) {
        if (handleServerRequest(message)) {
            return;
        }
    }

    if (!message.contains(QStringLiteral("jsonrpc")) || !hasIdField) {
        return;
    }

    const QString id = idFromValue(message.value(QStringLiteral("id")));
    if (id.isEmpty()) {
        return;
    }

    qCInfo(lcSseClient) << "Processing SSE message event for id" << id
                        << "hasError" << message.contains(QStringLiteral("error"));

    StoredResponse stored;
    if (message.contains(QStringLiteral("error"))) {
        stored.isError = true;
        stored.payload = message.value(QStringLiteral("error"));
    } else {
        stored.payload = message.value(QStringLiteral("result"));
    }

    {
        QMutexLocker locker(&m_responseMutex);
        m_pendingResponses.insert(id, stored);
    }
    emit pendingResponseArrived(id);
}

QJsonValue SseClient::sendRequest(const QString& method, const QJsonObject& params, int timeoutMs) {
    if (!ensureStream() || !waitForEndpoint(timeoutMs)) {
        throw McpError(QStringLiteral("SSE stream is not ready"));
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params}
    };

    const QByteArray jsonPayload = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    int endpointRetries = 0;
    constexpr int kMaxEndpointRetries = 3;

    while (true) {
        QNetworkRequest request(m_messageEndpoint);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        applyHeaders(request);

        qCInfo(lcSseClient) << "Sending HTTP request" << method << "to" << m_messageEndpoint << "payload size"
                            << jsonPayload.size() << "attempt" << (endpointRetries + 1);
        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(m_http.post(request, jsonPayload));

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(reply.data(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        loop.exec();
        timer.stop();

        const QNetworkReply::NetworkError netError = reply->error();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray rawBody = reply->readAll();
        qCInfo(lcSseClient) << "HTTP status" << status << "body length" << rawBody.size();

        const bool successStatus = (status >= 200 && status < 300) || status == 0;
        QString bodyString;
        if (!rawBody.isEmpty()) {
            bodyString = QString::fromUtf8(rawBody).trimmed();
            if (!bodyString.isEmpty()) {
                qCInfo(lcSseClient) << "HTTP body from message endpoint:" << bodyString;
            }
        } else {
            qCInfo(lcSseClient) << "HTTP body empty for request" << id;
        }

        bool handled = false;
        if (!rawBody.isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument httpDoc = QJsonDocument::fromJson(rawBody, &parseError);
            if (parseError.error == QJsonParseError::NoError && httpDoc.isObject()) {
                const QJsonObject obj = httpDoc.object();
                if (obj.contains(QStringLiteral("error"))) {
                    const QJsonObject error = obj.value(QStringLiteral("error")).toObject();
                    const int code = error.value(QStringLiteral("code")).toInt(-32000);
                    const QString message =
                        error.value(QStringLiteral("message")).toString(QStringLiteral("Unknown error"));
                    throw McpError(message, code, error.value(QStringLiteral("data")).toObject());
                }
                if (obj.contains(QStringLiteral("result"))) {
                    return obj.value(QStringLiteral("result"));
                }
                handled = true;
            } else {
                qCWarning(lcSseClient) << "Unexpected non-JSON HTTP body from message endpoint" << rawBody.size()
                                       << "bytes";
            }
        }

        if (!handled && !bodyString.isEmpty()) {
            const bool looksLikeUrl = bodyString.startsWith(QStringLiteral("http://"))
                                      || bodyString.startsWith(QStringLiteral("https://"))
                                      || bodyString.startsWith(QStringLiteral("/"))
                                      || bodyString.startsWith(QStringLiteral("messages"))
                                      || bodyString.contains(QStringLiteral("session_id"));
            if (looksLikeUrl) {
                const QUrl suggested = normalizeEndpoint(m_origin, bodyString);
                if (suggested.isValid() && suggested != m_messageEndpoint) {
                    if (!successStatus && endpointRetries < kMaxEndpointRetries) {
                        qCInfo(lcSseClient) << "Retrying request at server-suggested endpoint" << suggested;
                        m_messageEndpoint = suggested;
                        ++endpointRetries;
                        continue;
                    }
                    if (successStatus) {
                        qCInfo(lcSseClient) << "Server hinted alternate endpoint; updating future requests to"
                                            << suggested;
                        m_messageEndpoint = suggested;
                    }
                } else if (!successStatus && endpointRetries >= kMaxEndpointRetries) {
                    qCWarning(lcSseClient) << "Server suggested endpoint change but retry limit reached; continuing with"
                                            << m_messageEndpoint;
                }
            }
        }

        const bool fatalNetworkError = (netError != QNetworkReply::NoError) && status == 0;

        if (fatalNetworkError) {
            const QString errorMessage = reply->errorString();
            qCWarning(lcSseClient) << "HTTP transport error for" << method << ":" << errorMessage;
            throw McpError(QStringLiteral("HTTP error: %1").arg(errorMessage));
        }

        if (!successStatus) {
            // If we already tried to adjust the endpoint and still failed, surface the error.
            QString errorMessage = reply->errorString();
            if (errorMessage.isEmpty()) {
                errorMessage = bodyString;
            }
            if (errorMessage.isEmpty()) {
                errorMessage = QStringLiteral("Status code %1").arg(status);
            }
            qCWarning(lcSseClient) << "HTTP request failed with status" << status << "message" << errorMessage;
            throw McpError(QStringLiteral("HTTP error (%1): %2").arg(status).arg(errorMessage));
        }

        qCInfo(lcSseClient) << "Awaiting SSE response for id" << id;
        const StoredResponse response = awaitResponse(id, timeoutMs);
        if (response.isError) {
            const QJsonObject error = response.payload.toObject();
            const int code = error.value(QStringLiteral("code")).toInt(-32000);
            const QString message = error.value(QStringLiteral("message")).toString(QStringLiteral("Unknown error"));
            throw McpError(message, code, error.value(QStringLiteral("data")).toObject());
        }
        qCInfo(lcSseClient) << "Received SSE response for id" << id;
        return response.payload;
    }
}

void SseClient::sendNotification(const QString& method, const QJsonObject& params) {
    if (!ensureStream() || !waitForEndpoint(3000)) {
        throw McpError(QStringLiteral("SSE stream is not ready"));
    }

    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params}
    };

    sendResponseMessage(payload, 3000);
}

bool SseClient::handleServerRequest(const QJsonObject& request) {
    const QString id = idFromValue(request.value(QStringLiteral("id")));
    const QString method = request.value(QStringLiteral("method")).toString();
    if (id.isEmpty() || method.isEmpty()) {
        return false;
    }

    if (method == QStringLiteral("roots/list")) {
        QJsonObject payload{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), id},
            {QStringLiteral("result"), QJsonObject{{QStringLiteral("roots"), clientRoots()}}}
        };
        sendResponseMessage(payload, 5000);
        return true;
    }

    if (method == QStringLiteral("ping")) {
        QJsonObject payload{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), id},
            {QStringLiteral("result"), QJsonObject{}}
        };
        sendResponseMessage(payload, 5000);
        return true;
    }

    sendErrorResponse(id, -32601, QStringLiteral("Method %1 not implemented").arg(method));
    return true;
}

void SseClient::sendResponseMessage(const QJsonObject& payload, int timeoutMs) {
    if (!ensureStream() || !waitForEndpoint(timeoutMs)) {
        qCWarning(lcSseClient) << "SSE stream is not ready to send response";
        return;
    }

    QNetworkRequest request(m_messageEndpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    applyHeaders(request);

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
        m_http.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact)));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply.data(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
}

void SseClient::sendErrorResponse(const QString& id, int code, const QString& message) {
    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("error"),
         QJsonObject{
             {QStringLiteral("code"), code},
             {QStringLiteral("message"), message}
         }}
    };
    sendResponseMessage(payload, 5000);
}

SseClient::StoredResponse SseClient::awaitResponse(const QString& id, int timeoutMs) {
    {
        QMutexLocker locker(&m_responseMutex);
        if (m_pendingResponses.contains(id)) {
            return m_pendingResponses.take(id);
        }
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    qCInfo(lcSseClient) << "Starting event loop wait for id" << id << "timeout" << timeoutMs << "ms";
    QObject::connect(this, &SseClient::pendingResponseArrived, &loop, [&](const QString& readyId) {
        if (readyId == id) {
            qCInfo(lcSseClient) << "Event loop notified for id" << id;
            loop.quit();
        }
    });
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    timer.stop();

    QMutexLocker locker(&m_responseMutex);
    if (m_pendingResponses.contains(id)) {
        return m_pendingResponses.take(id);
    }
    qCWarning(lcSseClient) << "Event loop timeout for id" << id;
    throw McpError(QStringLiteral("Timeout waiting for SSE response for id %1").arg(id));
}

} // namespace qmcp
