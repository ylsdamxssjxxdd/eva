// xmcp.h
#ifndef XMCP_H
#define XMCP_H

#include "mcp_tools.h"
#include "xconfig.h"
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class xMcp : public QObject
{
    Q_OBJECT
  public:
    explicit xMcp(QObject *parent = nullptr);

  public slots:
    void addService(const QString mcp_json_str);
    void callTool(quint64 invocationId, QString tool_name, QString tool_args);
    void callList(quint64 invocationId);
    void refreshTools();
    void disconnectAll();

  signals:
    void callList_over(quint64 invocationId);
    void callTool_over(quint64 invocationId, QString result);
    void addService_single_over(QString name, MCP_CONNECT_STATE state); // 添加某个mcp服务完成
    void addService_over(MCP_CONNECT_STATE state);                      // 添加全部mcp服务完成
    void toolResult(const QString &serviceName, const QString &toolName, const QVariantMap &result);
    void mcp_message(QString message);
    void toolsRefreshed();

  private:
    void markActivity();
    void maybeAutoRefreshTools();
    McpToolManager toolManager;
    QTimer *autoRefreshTimer_ = nullptr;
    QElapsedTimer idleTimer_;
    qint64 lastRefreshEpochMs_ = 0;
    static constexpr int kIdleThresholdMs = 3000;
    static constexpr int kRefreshCooldownMs = 10000; // 10 seconds 重新刷新间隔
};

#endif // XMCP_H
