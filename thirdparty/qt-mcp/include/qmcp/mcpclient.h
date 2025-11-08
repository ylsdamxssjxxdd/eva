#pragma once

#include "qmcp/config.h"
#include "qmcp/errors.h"

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>

namespace qmcp {

class McpClient : public QObject {
    Q_OBJECT
public:
    explicit McpClient(const ServerConfig& config, QObject* parent = nullptr)
        : QObject(parent), m_config(config) {
        m_clientCapabilities = QJsonObject{
            {QStringLiteral("roots"), QJsonObject{{QStringLiteral("listChanged"), false}}}
        };
        m_clientRoots = QJsonArray{defaultRootObject()};
    }

    ~McpClient() override = default;

    virtual bool initialize(const QString& clientName, const QString& clientVersion) = 0;
    virtual QJsonArray listTools(const QJsonObject& pagination = {}) = 0;
    virtual QJsonObject callTool(const QString& toolName, const QJsonObject& arguments = {}) = 0;
    virtual QJsonObject listResources(const QJsonObject& pagination = {});
    virtual QJsonObject listResourceTemplates(const QJsonObject& pagination = {});
    virtual QJsonObject readResource(const QString& uri);
    virtual QJsonObject subscribeResource(const QString& uri);
    virtual QJsonObject unsubscribeResource(const QString& uri);
    virtual QJsonObject listPrompts(const QJsonObject& pagination = {});
    virtual QJsonObject getPrompt(const QString& name, const QJsonObject& arguments = {});
    virtual QJsonObject complete(const QJsonObject& reference,
                                 const QJsonObject& argument,
                                 const QJsonObject& context = {});
    virtual void sendProgressNotification(const QString& token,
                                          double progress,
                                          double total = 0.0,
                                          const QString& message = {});
    virtual void setLoggingLevel(const QString& level);
    virtual QJsonObject ping();
    virtual void notifyRootsChanged();
    virtual bool isConnected() const = 0;

    const ServerConfig& config() const noexcept { return m_config; }
    const QJsonObject& clientCapabilities() const noexcept { return m_clientCapabilities; }
    const QJsonArray& clientRoots() const noexcept { return m_clientRoots; }
    const QJsonObject& serverCapabilities() const noexcept { return m_serverCapabilities; }

    void setClientCapabilities(const QJsonObject& caps) { m_clientCapabilities = caps; }
    void setClientRoots(const QJsonArray& roots) { m_clientRoots = roots; }

protected:
    QJsonObject buildInitializeParams(const QString& clientName, const QString& clientVersion) const {
        QJsonObject params;
        params.insert(QStringLiteral("protocolVersion"), QStringLiteral("2024-11-05"));
        QJsonObject caps = m_clientCapabilities;
        if (!caps.contains(QStringLiteral("roots")) || !caps.value(QStringLiteral("roots")).isObject()) {
            caps.insert(QStringLiteral("roots"), QJsonObject{{QStringLiteral("listChanged"), false}});
        }
        params.insert(QStringLiteral("capabilities"), caps);
        QJsonArray roots = m_clientRoots;
        if (roots.isEmpty()) {
            roots.append(defaultRootObject());
        }
        params.insert(QStringLiteral("roots"), roots);
        params.insert(QStringLiteral("clientInfo"),
                      QJsonObject{{"name", clientName}, {"version", clientVersion}});
        return params;
    }

    void setServerCapabilities(const QJsonObject& caps) { m_serverCapabilities = caps; }
    virtual void handleServerNotification(const QString& method, const QJsonObject& params);
    static QJsonObject defaultRootObject();

private:
    ServerConfig m_config;
    QJsonObject m_clientCapabilities;
    QJsonArray m_clientRoots;
    QJsonObject m_serverCapabilities;
};

} // namespace qmcp
