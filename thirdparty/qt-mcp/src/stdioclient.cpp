#include "qmcp/stdioclient.h"

#include "qmcp/errors.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

#include <memory>

namespace {

Q_LOGGING_CATEGORY(lcStdioClient, "qmcp.stdioclient");

QString idFromValue(const QJsonValue& value) {
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(static_cast<qint64>(value.toDouble()));
    }
    return QString();
}

QString convertToString(const QJsonValue& value) {
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
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

QJsonObject valueToObject(const QJsonValue& value) {
    return value.isObject() ? value.toObject() : QJsonObject{};
}

} // namespace

namespace qmcp {

StdioClient::StdioClient(const ServerConfig& config, QObject* parent)
    : McpClient(config, parent) {
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &StdioClient::handleStdout);
    connect(&m_process, &QProcess::readyReadStandardError, this, &StdioClient::handleStderr);
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &StdioClient::handleProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &StdioClient::handleProcessError);
}

StdioClient::~StdioClient() {
    stopProcess();
}

bool StdioClient::initialize(const QString& clientName, const QString& clientVersion) {
    if (!startProcess()) {
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
        qCWarning(lcStdioClient) << "initialize failed:" << ex.what();
        stopProcess();
        return false;
    }
}

QJsonArray StdioClient::listTools(const QJsonObject& pagination) {
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

QJsonObject StdioClient::callTool(const QString& toolName, const QJsonObject& arguments) {
    const QJsonValue response = sendRequest(QStringLiteral("tools/call"),
                                            QJsonObject{
                                                {QStringLiteral("name"), toolName},
                                                {QStringLiteral("arguments"), arguments}
                                            });
    return response.toObject();
}

QJsonObject StdioClient::listResources(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("resources/list"), pagination));
}

QJsonObject StdioClient::listResourceTemplates(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("resources/templates/list"), pagination));
}

QJsonObject StdioClient::readResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/read"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject StdioClient::subscribeResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/subscribe"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject StdioClient::unsubscribeResource(const QString& uri) {
    return valueToObject(sendRequest(QStringLiteral("resources/unsubscribe"),
                                     QJsonObject{{QStringLiteral("uri"), uri}}));
}

QJsonObject StdioClient::listPrompts(const QJsonObject& pagination) {
    return valueToObject(sendRequest(QStringLiteral("prompts/list"), pagination));
}

QJsonObject StdioClient::getPrompt(const QString& name, const QJsonObject& arguments) {
    QJsonObject params{{QStringLiteral("name"), name}};
    if (!arguments.isEmpty()) {
        params.insert(QStringLiteral("arguments"), arguments);
    }
    return valueToObject(sendRequest(QStringLiteral("prompts/get"), params));
}

QJsonObject StdioClient::complete(const QJsonObject& reference,
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

void StdioClient::sendProgressNotification(const QString& token,
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

void StdioClient::setLoggingLevel(const QString& level) {
    sendRequest(QStringLiteral("logging/setLevel"), QJsonObject{{QStringLiteral("level"), level}});
}

QJsonObject StdioClient::ping() {
    return valueToObject(sendRequest(QStringLiteral("ping"), QJsonObject{}));
}

void StdioClient::notifyRootsChanged() {
    sendNotification(QStringLiteral("roots/list_changed"));
}

bool StdioClient::isConnected() const {
    return m_process.state() == QProcess::Running;
}

bool StdioClient::startProcess() {
    if (m_process.state() == QProcess::Running) {
        return true;
    }

    if (config().command.isEmpty()) {
        qCWarning(lcStdioClient) << "stdio command is missing";
        return false;
    }

    QString program = config().command;
    QStringList arguments = config().args;

#if defined(Q_OS_WIN)
    const QFileInfo programInfo(program);
    const QString suffix = programInfo.suffix().toLower();
    const bool hasPathSeparator = program.contains('/') || program.contains('\\');
    const bool hasExecutableSuffix = suffix == QStringLiteral("exe") || suffix == QStringLiteral("cmd")
                                     || suffix == QStringLiteral("bat") || suffix == QStringLiteral("com");
    if (!hasExecutableSuffix && !hasPathSeparator) {
        arguments.prepend(program);
        arguments.prepend(QStringLiteral("/c"));
        program = QStringLiteral("cmd.exe");
        qCInfo(lcStdioClient) << "Wrapping stdio command with cmd.exe for Windows shell resolution";
    }
#endif

    qCInfo(lcStdioClient) << "Starting stdio server:" << program << arguments;

    m_process.setProgram(program);
    m_process.setArguments(arguments);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = config().env.constBegin(); it != config().env.constEnd(); ++it) {
        env.insert(it.key(), convertToString(it.value()));
    }
    m_process.setProcessEnvironment(env);

    m_process.start();
    if (!m_process.waitForStarted(10000)) {
        qCWarning(lcStdioClient) << "Failed to start stdio server" << m_process.errorString();
        return false;
    }

    m_stdoutBuffer.clear();
    return true;
}

void StdioClient::stopProcess() {
    if (m_process.state() == QProcess::NotRunning) {
        return;
    }

    m_process.terminate();
    if (!m_process.waitForFinished(3000)) {
        m_process.kill();
        m_process.waitForFinished(2000);
    }
}

void StdioClient::handleStdout() {
    m_stdoutBuffer.append(m_process.readAllStandardOutput());
    int newlineIndex = -1;
    while ((newlineIndex = m_stdoutBuffer.indexOf('\n')) != -1) {
        QByteArray line = m_stdoutBuffer.left(newlineIndex);
        m_stdoutBuffer.remove(0, newlineIndex + 1);
        processLine(line);
    }
}

void StdioClient::handleStderr() {
    const QByteArray data = m_process.readAllStandardError();
    if (!data.isEmpty()) {
        qCWarning(lcStdioClient) << "STDERR:" << QString::fromUtf8(data.trimmed());
    }
}

void StdioClient::handleProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status);
    qCInfo(lcStdioClient) << "stdio server exited with code" << exitCode;
}

void StdioClient::handleProcessError(QProcess::ProcessError error) {
    qCWarning(lcStdioClient) << "stdio server error:" << error << m_process.errorString();
}

void StdioClient::processLine(const QByteArray& line) {
    QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (qEnvironmentVariableIsSet("QT_MCP_TRACE_STDIO")) {
        QTextStream(stderr) << "STDIO recv " << QString::fromUtf8(trimmed) << Qt::endl;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qCWarning(lcStdioClient) << "Invalid JSON from server:" << parseError.errorString()
                                 << QString::fromUtf8(trimmed);
        return;
    }

    const QJsonObject obj = doc.object();
    const bool hasMethod = obj.contains(QStringLiteral("method"));
    const bool hasResultOrError = obj.contains(QStringLiteral("result")) || obj.contains(QStringLiteral("error"));
    const bool hasIdField = obj.contains(QStringLiteral("id"));

    if (hasMethod && !hasResultOrError && !hasIdField) {
        const QString method = obj.value(QStringLiteral("method")).toString();
        const QJsonObject params = obj.value(QStringLiteral("params")).toObject();
        handleServerNotification(method, params);
        return;
    }

    if (hasMethod && !hasResultOrError && hasIdField) {
        if (handleServerRequest(obj)) {
            return;
        }
    }

    if (!hasIdField) {
        return;
    }

    const QString id = idFromValue(obj.value(QStringLiteral("id")));
    if (id.isEmpty()) {
        return;
    }

    StoredResponse stored;
    if (obj.contains(QStringLiteral("error"))) {
        stored.isError = true;
        stored.payload = obj.value(QStringLiteral("error"));
    } else {
        stored.payload = obj.value(QStringLiteral("result"));
    }

    {
        QMutexLocker locker(&m_responseMutex);
        m_pendingResponses.insert(id, stored);
    }
    emit pendingResponseArrived(id);
}

QJsonValue StdioClient::sendRequest(const QString& method, const QJsonObject& params, int timeoutMs) {
    if (!startProcess()) {
        throw McpError(QStringLiteral("stdio server is not running"));
    }

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params}
    };

    QByteArray requestBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    requestBytes.append('\n');
    qCInfo(lcStdioClient) << "STDIO send" << method << QString::fromUtf8(requestBytes.constData(), requestBytes.size() - 1);

    const qint64 written = m_process.write(requestBytes);
    if (written != requestBytes.size()) {
        throw McpError(QStringLiteral("Failed to write complete request to stdio server"));
    }
    if (!m_process.waitForBytesWritten(timeoutMs)) {
        throw McpError(QStringLiteral("Failed to flush request to stdio server"));
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

void StdioClient::sendNotification(const QString& method, const QJsonObject& params) {
    if (!startProcess()) {
        throw McpError(QStringLiteral("stdio server is not running"));
    }

    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params}
    };

    QByteArray requestBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    requestBytes.append('\n');
    const qint64 written = m_process.write(requestBytes);
    if (written != requestBytes.size()) {
        throw McpError(QStringLiteral("Failed to write notification to stdio server"));
    }
    if (!m_process.waitForBytesWritten(3000)) {
        throw McpError(QStringLiteral("Failed to flush notification to stdio server"));
    }
}

bool StdioClient::handleServerRequest(const QJsonObject& request) {
    const QString id = idFromValue(request.value(QStringLiteral("id")));
    const QString method = request.value(QStringLiteral("method")).toString();
    if (id.isEmpty() || method.isEmpty()) {
        return false;
    }

    if (method == QStringLiteral("roots/list")) {
        QJsonObject message{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), id},
            {QStringLiteral("result"), QJsonObject{{QStringLiteral("roots"), clientRoots()}}}
        };
        sendResponseMessage(message);
        return true;
    }

    if (method == QStringLiteral("ping")) {
        QJsonObject message{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), id},
            {QStringLiteral("result"), QJsonObject{}}
        };
        sendResponseMessage(message);
        return true;
    }

    sendErrorResponse(id, -32601, QStringLiteral("Method %1 not implemented").arg(method));
    return true;
}

void StdioClient::sendResponseMessage(const QJsonObject& message) {
    if (!startProcess()) {
        throw McpError(QStringLiteral("stdio server is not running"));
    }
    QByteArray responseBytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
    responseBytes.append('\n');
    const qint64 written = m_process.write(responseBytes);
    if (written != responseBytes.size()) {
        qCWarning(lcStdioClient) << "Failed to write response to stdio server";
        return;
    }
    if (!m_process.waitForBytesWritten(10000)) {
        qCWarning(lcStdioClient) << "Timeout flushing response to stdio server";
    }
}

void StdioClient::sendErrorResponse(const QString& id, int code, const QString& message) {
    QJsonObject payload{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("id"), id},
        {QStringLiteral("error"),
         QJsonObject{
             {QStringLiteral("code"), code},
             {QStringLiteral("message"), message}
         }}
    };
    sendResponseMessage(payload);
}

StdioClient::StoredResponse StdioClient::awaitResponse(const QString& id, int timeoutMs) {
    {
        QMutexLocker locker(&m_responseMutex);
        if (m_pendingResponses.contains(id)) {
            return m_pendingResponses.take(id);
        }
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(this, &StdioClient::pendingResponseArrived, &loop, [&](const QString& readyId) {
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
    throw McpError(QStringLiteral("Timeout waiting for stdio response for id %1").arg(id));
}

} // namespace qmcp
