#include "qmcp/mcpclient.h"

#include <QDir>
#include <QUrl>
#include <QDebug>

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
    Q_UNUSED(params);
    qInfo() << "Unhandled server notification from" << m_config.name << ":" << method;
}

} // namespace qmcp
