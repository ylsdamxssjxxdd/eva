// xmcp.h
#ifndef XMCP_H
#define XMCP_H

#include "mcp_tools.h"
#include "xconfig.h"
#include <QObject>
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

  signals:
    void callList_over(quint64 invocationId);
    void callTool_over(quint64 invocationId, QString result);
    void addService_single_over(QString name, MCP_CONNECT_STATE state); // 添加某个mcp服务完成
    void addService_over(MCP_CONNECT_STATE state);                      // 添加全部mcp服务完成
    void toolResult(const QString &serviceName, const QString &toolName, const QVariantMap &result);
    void mcp_message(QString message);

  private:
    McpToolManager toolManager;
};

#endif // XMCP_H
