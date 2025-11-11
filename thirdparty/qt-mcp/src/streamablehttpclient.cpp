#include "qmcp/streamablehttpclient.h"

#include "qmcp/errors.h"

#include <QByteArray>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QTimer>
#include <QUuid>

namespace {

Q_LOGGING_CATEGORY(lcStreamableHttpClient, "qmcp.streamablehttpclient");

QString idFromValue(const QJsonValue& value) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(static_cast<qint64>(value.toDouble()));
    }
    return QString();
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

StreamableHttpClient::StreamableHttpClient(const ServerConfig& config, QObject* parent)
    : McpClient(config, parent), m_url(config.baseUrl) {}

bool StreamableHttpClient::initialize(const QString& clientName, const QString& clientVersion) {
    qCInfo(lcStreamableHttpClient) << "Initializing Streamable HTTP client for" << config().name;
    ensureStreamStarted();

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
        m_initialized = true;
        return true;
    } catch (const std::exception& ex) {
        qCWarning(lcStreamableHttpClient) << "Initialization failed:" << ex.what();
        m_initialized = false;
        return false;
    }
}

QJsonArray StreamableHttpClient::listTools(const QJsonObject& pagination) {
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

QJsonObject StreamableHttpClient::callTool(const QString& toolName, const QJsonObject& arguments) {
    QJsonObject params{
        {QStringLiteral("name"), toolName},
        {QStringLiteral("arguments"), arguments}
    };
    const QJsonValue response = sendRequest(QStringLiteral("tools/call"), params);
    return response.toObject();
}

QJsonObject StreamableHttpClient::listResources(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("resources/list"), pagination));
}

QJsonObject StreamableHttpClient::listResourceTemplates(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("resources/templates/list"), pagination));
}

QJsonObject StreamableHttpClient::readResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/read"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject StreamableHttpClient::subscribeResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/subscribe"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject StreamableHttpClient::unsubscribeResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/unsubscribe"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject StreamableHttpClient::listPrompts(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("prompts/list"), pagination));
}

QJsonObject StreamableHttpClient::getPrompt(const QString& name, const QJsonObject& arguments) {
    QJsonObject params{{QStringLiteral("name"), name}};
    if (!arguments.isEmpty()) {
        params.insert(QStringLiteral("arguments"), arguments);
    }
    return valueToObject(sendRequest(QStringLiteral("prompts/get"), params));
}

QJsonObject StreamableHttpClient::complete(const QJsonObject& reference,
                                           const QJsonObject& argument,
                                           const QJsonObject& context) {
    QJsonObject params{
        {QStringLiteral("ref"), reference},
        {QStringLiteral("argument"), argument}};
    if (!context.isEmpty()) {
        params.insert(QStringLiteral("context"), context);
    }
    return valueToObject(sendRequest(QStringLiteral("completion/complete"), params));
}

void StreamableHttpClient::sendProgressNotification(const QString& token,
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

void StreamableHttpClient::setLoggingLevel(const QString& level) {
    sendRequest(QStringLiteral("logging/setLevel"), QJsonObject{{QStringLiteral("level"), level}});
}

QJsonObject StreamableHttpClient::ping() {
    return valueToObject(sendRequest(QStringLiteral("ping"), QJsonObject{}));
}

void StreamableHttpClient::notifyRootsChanged() {
    sendNotification(QStringLiteral("roots/list_changed"));
}

bool StreamableHttpClient::isConnected() const {
    return m_initialized;
}

bool StreamableHttpClient::ensureStreamStarted() {
    if (m_streamUnsupported || !m_url.isValid()) {
        return false;
    }

    if (m_streamRetryAfterSession && m_sessionId.isEmpty()) {
        qCInfo(lcStreamableHttpClient) << "Deferring stream start until session id is available";
        return false;
    }

    if (m_streamReply) {
        return true;
    }

    m_streamLastAttemptHadSession = !m_sessionId.isEmpty();
    m_streamRetryAfterSession = false;

    QNetworkRequest request(m_url);
    request.setRawHeader("Accept", "text/event-stream");
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    applyHeaders(request);
    appendSessionHeaders(request);

    qCInfo(lcStreamableHttpClient) << "Opening optional message stream for" << m_url;
    m_streamReply = m_streamManager.get(request);
    if (!m_streamReply) {
        m_streamUnsupported = true;
        return false;
    }

    QObject::connect(m_streamReply, &QNetworkReply::readyRead, this, &StreamableHttpClient::handleStreamReadyRead);
    QObject::connect(m_streamReply, &QNetworkReply::finished, this, &StreamableHttpClient::handleStreamFinished);
    QObject::connect(m_streamReply, &QNetworkReply::errorOccurred, this, &StreamableHttpClient::handleStreamError);
    return true;
}

void StreamableHttpClient::applyHeaders(QNetworkRequest& request) const {
    const QJsonObject headers = config().headers;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        const QString value = headerValueToString(it.value());
        if (!value.isEmpty()) {
            request.setRawHeader(it.key().toUtf8(), value.toUtf8());
        }
    }
}

void StreamableHttpClient::appendSessionHeaders(QNetworkRequest& request) const {
    if (!m_sessionId.isEmpty()) {
        request.setRawHeader("Mcp-Session-Id", m_sessionId.toUtf8());
    }
    request.setRawHeader("Mcp-Protocol-Version", QByteArrayLiteral("2024-11-05"));
}

QJsonValue StreamableHttpClient::sendRequest(const QString& method, const QJsonObject& params, int timeoutMs) {
    if (!m_url.isValid()) {
        throw McpError(QStringLiteral("Streamable HTTP baseUrl is invalid"));
    }
    ensureStreamStarted();

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params}
    };

    const QByteArray jsonPayload = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkRequest request(m_url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json, text/event-stream");
    applyHeaders(request);
    appendSessionHeaders(request);

    qCInfo(lcStreamableHttpClient) << "Sending Streamable HTTP request" << method << "payload size" << jsonPayload.size();
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
    QByteArray rawBody = reply->readAll();
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().toLower();
    const QByteArray sessionIdHeader = reply->rawHeader("Mcp-Session-Id");
    const QByteArray sessionIdLower = reply->rawHeader("mcp-session-id");
    if (!sessionIdHeader.isEmpty()) {
        m_sessionId = QString::fromUtf8(sessionIdHeader);
    } else if (!sessionIdLower.isEmpty()) {
        m_sessionId = QString::fromUtf8(sessionIdLower);
    }

    if (m_streamRetryAfterSession && !m_sessionId.isEmpty()) {
        ensureStreamStarted();
    }

    if (netError != QNetworkReply::NoError && status == 0) {
        const QString errorMessage = reply->errorString();
        qCWarning(lcStreamableHttpClient) << "Network error for" << method << errorMessage;
        throw McpError(QStringLiteral("HTTP error: %1").arg(errorMessage));
    }

    if (status == 401) {
        throw McpError(QStringLiteral("Unauthorized"), status);
    }

    if (status >= 400) {
        const QString errorMessage = rawBody.isEmpty() ? reply->errorString()
                                                       : QString::fromUtf8(rawBody).trimmed();
        throw McpError(QStringLiteral("HTTP error (%1): %2").arg(status).arg(errorMessage));
    }

    bool handled = false;
    if (!rawBody.isEmpty()) {
        if (contentType.contains(QStringLiteral("text/event-stream"))) {
            parseSsePayload(rawBody);
            handled = true;
        } else {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(rawBody, &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                if (doc.isObject()) {
                    const QJsonObject obj = doc.object();
                    if (obj.contains(QStringLiteral("result")) || obj.contains(QStringLiteral("error"))) {
                        handled = true;
                        if (obj.contains(QStringLiteral("error"))) {
                            const QJsonObject error = obj.value(QStringLiteral("error")).toObject();
                            const int code = error.value(QStringLiteral("code")).toInt(-32000);
                            const QString message =
                                error.value(QStringLiteral("message")).toString(QStringLiteral("Unknown error"));
                            throw McpError(message, code, error.value(QStringLiteral("data")).toObject());
                        }
                        return obj.value(QStringLiteral("result"));
                    }
                    if (obj.contains(QStringLiteral("jsonrpc"))) {
                        handleJsonMessage(obj);
                        handled = true;
                    }
                } else if (doc.isArray()) {
                    handled = true;
                    const QJsonArray array = doc.array();
                    for (const QJsonValue& value : array) {
                        if (value.isObject()) {
                            handleJsonMessage(value.toObject());
                        }
                    }
                }
            }
        }
    }

    if (!handled) {
        qCInfo(lcStreamableHttpClient) << "Awaiting streamed response for id" << id;
    }

    const StoredResponse response = awaitResponse(id, timeoutMs);
    if (response.isError) {
        const QJsonObject error = response.payload.toObject();
        const int code = error.value(QStringLiteral("code")).toInt(-32000);
        const QString message = error.value(QStringLiteral("message")).toString(QStringLiteral("Unknown error"));
        throw McpError(message, code, error.value(QStringLiteral("data")).toObject());
    }

    return response.payload;
}

void StreamableHttpClient::sendNotification(const QString& method, const QJsonObject& params) {
    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params}
    };
    sendResponseMessage(payload, 3000);
}

bool StreamableHttpClient::handleServerRequest(const QJsonObject& request) {
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

void StreamableHttpClient::sendResponseMessage(const QJsonObject& payload, int timeoutMs) {
    if (!m_url.isValid()) {
        qCWarning(lcStreamableHttpClient) << "Cannot send payload without valid baseUrl";
        return;
    }

    QNetworkRequest request(m_url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    applyHeaders(request);
    appendSessionHeaders(request);

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

void StreamableHttpClient::sendErrorResponse(const QString& id, int code, const QString& message) {
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

StreamableHttpClient::StoredResponse StreamableHttpClient::awaitResponse(const QString& id, int timeoutMs) {
    {
        QMutexLocker locker(&m_responseMutex);
        if (m_pendingResponses.contains(id)) {
            return m_pendingResponses.take(id);
        }
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(this, &StreamableHttpClient::pendingResponseArrived, &loop, [&](const QString& readyId) {
        if (readyId == id) {
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
    throw McpError(QStringLiteral("Timeout waiting for Streamable HTTP response for id %1").arg(id));
}

void StreamableHttpClient::handleStreamReadyRead() {
    if (!m_streamReply) {
        return;
    }
    m_streamBuffer.append(m_streamReply->readAll());
    while (true) {
        int delimiterIndex = m_streamBuffer.indexOf("\n\n");
        int delimiterLength = 2;
        const int crlfIndex = m_streamBuffer.indexOf("\r\n\r\n");
        if (crlfIndex != -1 && (delimiterIndex == -1 || crlfIndex < delimiterIndex)) {
            delimiterIndex = crlfIndex;
            delimiterLength = 4;
        }
        if (delimiterIndex == -1) {
            break;
        }
        QByteArray chunk = m_streamBuffer.left(delimiterIndex);
        m_streamBuffer.remove(0, delimiterIndex + delimiterLength);
        processEvent(chunk);
    }
}

void StreamableHttpClient::handleStreamFinished() {
    if (!m_streamReply) {
        return;
    }
    const int status = m_streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qCInfo(lcStreamableHttpClient) << "Stream finished with status" << status;
    if (status == 405) {
        if (!m_streamLastAttemptHadSession) {
            qCInfo(lcStreamableHttpClient)
                << "Server requires session id before exposing GET stream endpoint, will retry after initialization";
            m_streamRetryAfterSession = true;
        } else {
            qCInfo(lcStreamableHttpClient) << "Server does not expose GET stream endpoint, disabling stream listener";
            m_streamUnsupported = true;
        }
    }
    m_streamReply->deleteLater();
    m_streamReply = nullptr;
    m_streamBuffer.clear();
}

void StreamableHttpClient::handleStreamError(QNetworkReply::NetworkError code) {
    if (code == QNetworkReply::OperationCanceledError) {
        return;
    }
    const int status = m_streamReply ? m_streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() : 0;
    if (code == QNetworkReply::ContentOperationNotPermittedError && status == 405) {
        if (!m_streamLastAttemptHadSession) {
            qCInfo(lcStreamableHttpClient)
                << "Stream request rejected before session id, waiting for initialization to retry";
            m_streamRetryAfterSession = true;
        } else {
            qCInfo(lcStreamableHttpClient) << "Server rejected stream even with session id, disabling listener";
            m_streamUnsupported = true;
        }
        return;
    }
    qCWarning(lcStreamableHttpClient) << "Stream error" << code
                                      << (m_streamReply ? m_streamReply->errorString() : QString());
}

void StreamableHttpClient::processEvent(const QByteArray& rawEvent) {
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
        qCInfo(lcStreamableHttpClient) << "Endpoint hint ignored for streamable HTTP";
        return;
    }

    if (eventType != QStringLiteral("message")) {
        qCWarning(lcStreamableHttpClient) << "Unknown event type" << eventType;
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(dataPayload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcStreamableHttpClient) << "Invalid SSE payload:" << parseError.errorString();
        return;
    }

    handleJsonMessage(doc.object());
}

void StreamableHttpClient::handleJsonMessage(const QJsonObject& message) {
    const bool hasMethod = message.contains(QStringLiteral("method"));
    const bool hasResultOrError = message.contains(QStringLiteral("result"))
                                  || message.contains(QStringLiteral("error"));
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

void StreamableHttpClient::parseSsePayload(const QByteArray& payload) {
    QByteArray buffer = payload;
    while (true) {
        int delimiterIndex = buffer.indexOf("\n\n");
        int delimiterLength = 2;
        const int crlfIndex = buffer.indexOf("\r\n\r\n");
        if (crlfIndex != -1 && (delimiterIndex == -1 || crlfIndex < delimiterIndex)) {
            delimiterIndex = crlfIndex;
            delimiterLength = 4;
        }
        if (delimiterIndex == -1) {
            break;
        }
        QByteArray chunk = buffer.left(delimiterIndex);
        buffer.remove(0, delimiterIndex + delimiterLength);
        processEvent(chunk);
    }
    if (!buffer.isEmpty()) {
        processEvent(buffer);
    }
}

} // namespace qmcp
