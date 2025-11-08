#include "qmcp/mcpclient.h"

#include <QDir>
#include <QUrl>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

namespace qmcp {

QJsonObject McpClient::defaultRootObject() {
    const QUrl rootUrl = QUrl::fromLocalFile(QDir::currentPath());
    return QJsonObject{
        {QStringLiteral("name"), QStringLiteral("workspace")},
        {QStringLiteral("uri"), rootUrl.toString(QUrl::FullyEncoded)}
    };
}

QJsonObject McpClient::listResources(const QJsonObject& pagination) {
    Q_UNUSED(pagination);
    throw McpError(QStringLiteral("resources/list is not implemented for this client"));
}

QJsonObject McpClient::listResourceTemplates(const QJsonObject& pagination) {
    Q_UNUSED(pagination);
    throw McpError(QStringLiteral("resources/templates/list is not implemented for this client"));
}

QJsonObject McpClient::readResource(const QString& uri) {
    Q_UNUSED(uri);
    throw McpError(QStringLiteral("resources/read is not implemented for this client"));
}

QJsonObject McpClient::subscribeResource(const QString& uri) {
    Q_UNUSED(uri);
    throw McpError(QStringLiteral("resources/subscribe is not implemented for this client"));
}

QJsonObject McpClient::unsubscribeResource(const QString& uri) {
    Q_UNUSED(uri);
    throw McpError(QStringLiteral("resources/unsubscribe is not implemented for this client"));
}

QJsonObject McpClient::listPrompts(const QJsonObject& pagination) {
    Q_UNUSED(pagination);
    throw McpError(QStringLiteral("prompts/list is not implemented for this client"));
}

QJsonObject McpClient::getPrompt(const QString& name, const QJsonObject& arguments) {
    Q_UNUSED(name);
    Q_UNUSED(arguments);
    throw McpError(QStringLiteral("prompts/get is not implemented for this client"));
}

QJsonObject McpClient::complete(const QJsonObject& reference,
                                const QJsonObject& argument,
                                const QJsonObject& context) {
    Q_UNUSED(reference);
    Q_UNUSED(argument);
    Q_UNUSED(context);
    throw McpError(QStringLiteral("completion/complete is not implemented for this client"));
}

void McpClient::sendProgressNotification(const QString& token,
                                         double progress,
                                         double total,
                                         const QString& message) {
    Q_UNUSED(token);
    Q_UNUSED(progress);
    Q_UNUSED(total);
    Q_UNUSED(message);
    throw McpError(QStringLiteral("notifications/progress is not implemented for this client"));
}

void McpClient::setLoggingLevel(const QString& level) {
    Q_UNUSED(level);
    throw McpError(QStringLiteral("logging/setLevel is not implemented for this client"));
}

QJsonObject McpClient::ping() {
    throw McpError(QStringLiteral("ping is not implemented for this client"));
}

void McpClient::notifyRootsChanged() {
    throw McpError(QStringLiteral("roots/list_changed is not implemented for this client"));
}

void McpClient::handleServerNotification(const QString& method, const QJsonObject& params) {
    const QString serverKey = serverIdentifier();
    emit serverNotificationReceived(serverKey, method, params);

    auto stringifyJsonValue = [](const QJsonValue& value) -> QString {
        if (value.isString()) return value.toString();
        if (value.isDouble()) return QString::number(value.toDouble());
        if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        if (value.isNull() || value.isUndefined()) return {};
        QJsonDocument doc;
        if (value.isArray()) {
            doc = QJsonDocument(value.toArray());
        } else if (value.isObject()) {
            doc = QJsonDocument(value.toObject());
        }
        return doc.isNull() ? QString() : QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    };

    if (method == QLatin1String("notifications/message")) {
        const QString level = params.value(QStringLiteral("level")).toString(QStringLiteral("info"));
        QString message = params.value(QStringLiteral("message")).toString();
        if (message.isEmpty()) {
            message = stringifyJsonValue(params.value(QStringLiteral("data")));
        }
        if (message.isEmpty()) {
            message = stringifyJsonValue(params.value(QStringLiteral("text")));
        }
        emit serverMessageReceived(serverKey, level, message);
        if (!message.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[MCP %1] %2: %3").arg(serverKey, level, message);
        }
        return;
    }

    if (method == QLatin1String("notifications/progress")) {
        emit serverMessageReceived(
            serverKey,
            params.value(QStringLiteral("level")).toString(QStringLiteral("info")),
            QStringLiteral("progress %1/%2 %3")
                .arg(params.value(QStringLiteral("progress")).toVariant().toString(),
                     params.value(QStringLiteral("total")).toVariant().toString(),
                     params.value(QStringLiteral("message")).toString()));
        return;
    }

    qInfo() << "Unhandled server notification from" << serverKey << ":" << method;
}

QString McpClient::serverIdentifier() const {
    if (!m_config.key.isEmpty()) return m_config.key;
    if (!m_config.name.isEmpty()) return m_config.name;
    return QStringLiteral("mcp");
}

} // namespace qmcp
