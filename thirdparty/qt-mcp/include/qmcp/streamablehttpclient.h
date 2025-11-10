#pragma once

#include "qmcp/mcpclient.h"

#include <QHash>
#include <QJsonValue>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>

namespace qmcp {

class StreamableHttpClient : public McpClient {
    Q_OBJECT
public:
    explicit StreamableHttpClient(const ServerConfig& config, QObject* parent = nullptr);

    bool initialize(const QString& clientName, const QString& clientVersion) override;
    QJsonArray listTools(const QJsonObject& pagination = {}) override;
    QJsonObject callTool(const QString& toolName, const QJsonObject& arguments = {}) override;
    QJsonObject listResources(const QJsonObject& pagination = {}) override;
    QJsonObject listResourceTemplates(const QJsonObject& pagination = {}) override;
    QJsonObject readResource(const QString& uri) override;
    QJsonObject subscribeResource(const QString& uri) override;
    QJsonObject unsubscribeResource(const QString& uri) override;
    QJsonObject listPrompts(const QJsonObject& pagination = {}) override;
    QJsonObject getPrompt(const QString& name, const QJsonObject& arguments = {}) override;
    QJsonObject complete(const QJsonObject& reference,
                         const QJsonObject& argument,
                         const QJsonObject& context = {}) override;
    void sendProgressNotification(const QString& token,
                                  double progress,
                                  double total = 0.0,
                                  const QString& message = {}) override;
    void setLoggingLevel(const QString& level) override;
    QJsonObject ping() override;
    void notifyRootsChanged() override;
    bool isConnected() const override;

signals:
    void pendingResponseArrived(const QString& id);

private slots:
    void handleStreamReadyRead();
    void handleStreamFinished();
    void handleStreamError(QNetworkReply::NetworkError code);

private:
    struct StoredResponse {
        QJsonValue payload;
        bool isError = false;
    };

    bool ensureStreamStarted();
    void applyHeaders(QNetworkRequest& request) const;
    void appendSessionHeaders(QNetworkRequest& request) const;
    QJsonValue sendRequest(const QString& method, const QJsonObject& params, int timeoutMs = 15000);
    void sendNotification(const QString& method, const QJsonObject& params = {});
    bool handleServerRequest(const QJsonObject& request);
    void sendResponseMessage(const QJsonObject& payload, int timeoutMs = 5000);
    void sendErrorResponse(const QString& id, int code, const QString& message);
    StoredResponse awaitResponse(const QString& id, int timeoutMs);
    void processEvent(const QByteArray& rawEvent);
    void handleJsonMessage(const QJsonObject& message);
    void parseSsePayload(const QByteArray& payload);

    QNetworkAccessManager m_http;
    QNetworkAccessManager m_streamManager;
    QPointer<QNetworkReply> m_streamReply;
    QByteArray m_streamBuffer;
    QMutex m_responseMutex;
    QHash<QString, StoredResponse> m_pendingResponses;
    QUrl m_url;
    QString m_sessionId;
    bool m_initialized = false;
    bool m_streamUnsupported = false;
};

} // namespace qmcp

