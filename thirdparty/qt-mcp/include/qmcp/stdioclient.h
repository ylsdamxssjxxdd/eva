#pragma once

#include "qmcp/mcpclient.h"

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QMutex>
#include <QProcess>

namespace qmcp {

class StdioClient : public McpClient {
    Q_OBJECT
public:
    explicit StdioClient(const ServerConfig& config, QObject* parent = nullptr);
    ~StdioClient() override;

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
    void handleStdout();
    void handleStderr();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);

private:
    struct StoredResponse {
        QJsonValue payload;
        bool isError = false;
    };

    bool startProcess();
    void stopProcess();
    QJsonValue sendRequest(const QString& method, const QJsonObject& params, int timeoutMs = 60000);
    void sendNotification(const QString& method, const QJsonObject& params = {});
    StoredResponse awaitResponse(const QString& id, int timeoutMs);
    bool handleServerRequest(const QJsonObject& request);
    void sendResponseMessage(const QJsonObject& message);
    void sendErrorResponse(const QString& id, int code, const QString& message);
    void processLine(const QByteArray& line);
    static QString jsonValueToString(const QJsonValue& value);

    QProcess m_process;
    QByteArray m_stdoutBuffer;

    QMutex m_responseMutex;
    QHash<QString, StoredResponse> m_pendingResponses;
};

} // namespace qmcp
