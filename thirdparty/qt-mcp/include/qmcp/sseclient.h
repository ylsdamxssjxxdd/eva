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

class SseClient : public McpClient {
    Q_OBJECT
public:
    explicit SseClient(const ServerConfig& config, QObject* parent = nullptr);

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
    void endpointReady();

private slots:
    void handleSseReadyRead();
    void handleSseFinished();
    void handleSseError(QNetworkReply::NetworkError code);

private:
    struct StoredResponse {
        QJsonValue payload;
        bool isError = false;
    };

    bool ensureStream();
    bool waitForEndpoint(int timeoutMs);
    void applyHeaders(QNetworkRequest& request) const;
    void processEvent(const QByteArray& rawEvent);
    QJsonValue sendRequest(const QString& method, const QJsonObject& params, int timeoutMs = 600000);
    void sendNotification(const QString& method, const QJsonObject& params = {});
    bool handleServerRequest(const QJsonObject& request);
    void sendResponseMessage(const QJsonObject& payload, int timeoutMs = 5000);
    void sendErrorResponse(const QString& id, int code, const QString& message);
    StoredResponse awaitResponse(const QString& id, int timeoutMs);

    QNetworkAccessManager m_http;
    QNetworkAccessManager m_sseManager;
    QPointer<QNetworkReply> m_sseReply;
    QByteArray m_sseBuffer;
    QUrl m_sseUrl;
    QUrl m_origin;
    QUrl m_messageEndpoint;
    bool m_streamOpen = false;

    QMutex m_responseMutex;
    QHash<QString, StoredResponse> m_pendingResponses;
};

} // namespace qmcp
