// xmcp.h
#ifndef XMCP_H
#define XMCP_H

#include "mcp_tools.h"
#include "xconfig.h"
#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

class IMcpToolController
{
  public:
    virtual ~IMcpToolController() = default;
    virtual void setNotificationHandler(McpToolManager::NotificationHandler handler) = 0;
    virtual void clear() = 0;
    virtual std::string addServer(const std::string &name, const mcp::json &config) = 0;
    virtual mcp::json getAllToolsInfo() const = 0;
    virtual mcp::json callTool(const std::string &serviceName, const std::string &toolName, const mcp::json &params) = 0;
    virtual bool refreshAllTools(const QSet<QString> *serviceFilter) = 0;
    virtual size_t getServiceCount() const = 0;
};

struct xMcpOptions
{
    int idleThresholdMs = 3000;
    int autoRefreshIntervalMs = 10000;
    bool enableAutoRefreshTimer = true;
};

class xMcp : public QObject
{
    Q_OBJECT
  public:
    explicit xMcp(QObject *parent = nullptr,
                  std::unique_ptr<IMcpToolController> controller = nullptr,
                  xMcpOptions options = {});

  public slots:
    void addService(const QString mcp_json_str);
    void callTool(quint64 invocationId, QString tool_name, QString tool_args);
    void callList(quint64 invocationId);
    void refreshTools();
    void disconnectAll();
    void setEnabledServices(const QStringList &services);

  signals:
    void callList_over(quint64 invocationId);
    void callTool_over(quint64 invocationId, QString result);
    void addService_single_over(QString name, MCP_CONNECT_STATE state); // 添加某个mcp服务完成
    void addService_over(MCP_CONNECT_STATE state);                      // 添加全部mcp服务完成
    void toolResult(const QString &serviceName, const QString &toolName, const QVariantMap &result);
    void mcp_message(QString message);
    void toolsRefreshed();

  protected:
    void maybeAutoRefreshTools();

  private:
    void markActivity();
    std::unique_ptr<IMcpToolController> controller_;
    QTimer *autoRefreshTimer_ = nullptr;
    QElapsedTimer idleTimer_;
    qint64 lastRefreshEpochMs_ = 0;
    QSet<QString> enabledServices_;
    bool serviceFilterActive_ = false;
    bool autoRefreshAllowed_ = true;
    int idleThresholdMs_ = 3000;
    int autoRefreshIntervalMs_ = 10000;
    bool timerRequested_ = true;
};

#endif // XMCP_H
