// xmcp.cpp
#include "xmcp.h"
#include "xmcp_internal.h"
#include <QDateTime>
#include <QDebug>
#include <algorithm>
#include <QtGlobal>
#include <utility>

using eva::mcp::sanitizeToolsInfo;
using eva::mcp::syncSelectedMcpTools;

namespace
{
class DefaultMcpToolController : public IMcpToolController
{
  public:
    void setNotificationHandler(McpToolManager::NotificationHandler handler) override { manager_.setNotificationHandler(std::move(handler)); }
    void clear() override { manager_.clear(); }
    std::string addServer(const std::string &name, const mcp::json &config) override { return manager_.addServer(name, config); }
    mcp::json getAllToolsInfo() const override { return manager_.getAllToolsInfo(); }
    mcp::json callTool(const std::string &serviceName, const std::string &toolName, const mcp::json &params) override
    {
        return manager_.callTool(serviceName, toolName, params);
    }
    bool refreshAllTools(const QSet<QString> *serviceFilter) override { return manager_.refreshAllTools(serviceFilter); }
    size_t getServiceCount() const override { return manager_.getServiceCount(); }

  private:
    McpToolManager manager_;
};

xMcpOptions normalizeOptions(xMcpOptions opts)
{
    if (opts.idleThresholdMs <= 0) opts.idleThresholdMs = 3000;
    if (opts.autoRefreshIntervalMs <= 0) opts.autoRefreshIntervalMs = 10000;
    return opts;
}
} // namespace

xMcp::xMcp(QObject *parent, std::unique_ptr<IMcpToolController> controller, xMcpOptions options)
    : QObject(parent)
{
    options = normalizeOptions(options);
    controller_ = controller ? std::move(controller) : std::make_unique<DefaultMcpToolController>();
    idleThresholdMs_ = options.idleThresholdMs;
    autoRefreshIntervalMs_ = options.autoRefreshIntervalMs;
    timerRequested_ = options.enableAutoRefreshTimer;
    qDebug() << "mcp init over";
    controller_->setNotificationHandler([this](const QString &service, const QString &level, const QString &message)
                                         {
                                             QString prefix = service;
                                             if (!level.isEmpty())
                                             {
                                                 prefix += QStringLiteral(" [%1]").arg(level.toUpper());
                                             }
                                             const QString payload = message.isEmpty() ? tr("server notification received") : message;
                                             emit mcp_message(prefix + ": " + payload);
                                         });
    idleTimer_.start();
    autoRefreshTimer_ = new QTimer(this);
    const int pollInterval = std::max(100, autoRefreshIntervalMs_ / 5);
    autoRefreshTimer_->setInterval(qMax(100, pollInterval));
    QObject::connect(autoRefreshTimer_, &QTimer::timeout, this, &xMcp::maybeAutoRefreshTools);
    if (timerRequested_)
    {
        autoRefreshTimer_->start();
    }
}

void xMcp::addService(const QString mcp_json_str)
{
    markActivity();
    lastRefreshEpochMs_ = 0;
    controller_->clear();        // 清空工具
    mcp::json config;
    if (mcp_json_str.isEmpty())
    {
        emit addService_over(MCP_CONNECT_MISS);
        return;
    }
    try
    {
        config = mcp::json::parse(mcp_json_str.toStdString()); // JSON解析可能抛出异常
    }
    catch (const std::exception &e) // 如果json解析失败
    {
        qCritical() << "tool JSON parse error:" << e.what();
        emit mcp_message(QString("tool JSON parse error:") + e.what());
        emit addService_over(MCP_CONNECT_MISS);
        return;
    }
    int ok_num = 0; // 用来记录服务是否全部连接成功
    for (auto &[name, serverConfig] : config["mcpServers"].items())
    {
        try
        {
            std::string res = controller_->addServer(name, serverConfig);
            if (res == "")
            {
                emit addService_single_over(QString::fromStdString(name), MCP_CONNECT_LINK);
                emit mcp_message(QString::fromStdString(name) + " add success");
                ok_num++;
            }
            else
            {
                emit addService_single_over(QString::fromStdString(name), MCP_CONNECT_MISS);
                emit mcp_message(QString::fromStdString(name) + " add fail: " + QString::fromStdString(res));
            }
        }
        catch (const client_exception &e)
        {
            emit addService_single_over(QString::fromStdString(name), MCP_CONNECT_MISS);
            emit mcp_message(QString::fromStdString(name) + " add fail: " + e.what());
            qCritical() << "client exception error:" << e.what();
        }
    }
    // 获取所有可用工具信息
    MCP_TOOLS_INFO_ALL = sanitizeToolsInfo(controller_->getAllToolsInfo());
    const QSet<QString> *filter = serviceFilterActive_ ? &enabledServices_ : nullptr;
    syncSelectedMcpTools(MCP_TOOLS_INFO_ALL, filter);
    lastRefreshEpochMs_ = QDateTime::currentMSecsSinceEpoch();
    mcp::json servers = get_json_object_safely(config, "mcpServers");
    if (ok_num == static_cast<int>(servers.size())) { emit addService_over(MCP_CONNECT_LINK); }
    else if (ok_num == 0)
    {
        emit addService_over(MCP_CONNECT_MISS);
    }
    else
    {
        emit addService_over(MCP_CONNECT_WIP);
    }
}

void xMcp::callTool(quint64 invocationId, QString tool_name, QString tool_args)
{
    markActivity();
    QString result;
    // 拆分出服务名和工具名
    std::string llm_tool_name = tool_name.toStdString(); // 大模型输出的要调用的工具名
    std::string mcp_server_name;
    std::string mcp_tool_name;
    size_t pos = llm_tool_name.find('@'); // 如果找到_则视为mcp服务器提供的工具

    if (pos != std::string::npos)
    {
        mcp_server_name = llm_tool_name.substr(0, pos);
        mcp_tool_name = llm_tool_name.substr(pos + 1);
        // std::cout << "Name: " << mcp_server_name << "\nFunction: " << mcp_tool_name << std::endl;
    }
    else
    {
        std::cout << "No '@' found!" << std::endl;
        callTool_over(invocationId, result);
        return;
    }

    mcp::json params;
    if (tool_args == "") { tool_args = "{}"; } // 处理tool_args为空的情况
    try
    {
        params = mcp::json::parse(tool_args.toStdString());
    }
    catch (const std::exception &e)
    {
        params = mcp::json::object(); // 可选：初始化为空对象
        result = "JSON parse fail: " + QString::fromStdString(e.what());
        callTool_over(invocationId, result);
        return;
    }
    auto result2 = controller_->callTool(mcp_server_name, mcp_tool_name, params);
    result = QString::fromStdString(result2.dump());
    callTool_over(invocationId, result);
}

// 查询mcp可用工具
void xMcp::callList(quint64 invocationId)
{
    markActivity();
    MCP_TOOLS_INFO_ALL = sanitizeToolsInfo(controller_->getAllToolsInfo());
    const QSet<QString> *filter = serviceFilterActive_ ? &enabledServices_ : nullptr;
    syncSelectedMcpTools(MCP_TOOLS_INFO_ALL, filter);
    lastRefreshEpochMs_ = QDateTime::currentMSecsSinceEpoch();
    emit toolsRefreshed();
    emit callList_over(invocationId);
}

void xMcp::refreshTools()
{
    markActivity();
    if (serviceFilterActive_ && enabledServices_.isEmpty())
    {
        emit mcp_message(QStringLiteral("no enabled MCP services; refresh skipped"));
        return;
    }
    const QSet<QString> *filter = serviceFilterActive_ ? &enabledServices_ : nullptr;
    const bool updated = controller_->refreshAllTools(filter);
    MCP_TOOLS_INFO_ALL = sanitizeToolsInfo(controller_->getAllToolsInfo());
    syncSelectedMcpTools(MCP_TOOLS_INFO_ALL, filter);
    lastRefreshEpochMs_ = QDateTime::currentMSecsSinceEpoch();
    emit toolsRefreshed();
    if (updated)
    {
        emit mcp_message(QStringLiteral("tools refreshed"));
    }
}

void xMcp::disconnectAll()
{
    markActivity();
    lastRefreshEpochMs_ = 0;
    controller_->clear();
    MCP_TOOLS_INFO_ALL = mcp::json::array();
    if (!MCP_TOOLS_INFO_LIST.empty())
    {
        MCP_TOOLS_INFO_LIST.clear();
    }
    emit toolsRefreshed();
    emit addService_over(MCP_CONNECT_MISS);
    emit mcp_message(QStringLiteral("all services disconnected"));
}

void xMcp::setEnabledServices(const QStringList &services)
{
    serviceFilterActive_ = true;
    QSet<QString> normalized;
    normalized.reserve(services.size());
    for (const QString &service : services)
    {
        if (service.isEmpty()) continue;
        normalized.insert(service);
    }
    const bool allow = !normalized.isEmpty();
    enabledServices_ = std::move(normalized);
    if (autoRefreshAllowed_ != allow)
    {
        autoRefreshAllowed_ = allow;
        if (!allow)
        {
            idleTimer_.restart();
        }
    }

    const QSet<QString> *filter = serviceFilterActive_ ? &enabledServices_ : nullptr;
    if (syncSelectedMcpTools(MCP_TOOLS_INFO_ALL, filter))
    {
        emit toolsRefreshed();
    }
}

void xMcp::markActivity()
{
    if (!idleTimer_.isValid())
    {
        idleTimer_.start();
        return;
    }
    idleTimer_.restart();
}

void xMcp::maybeAutoRefreshTools()
{
    if (!autoRefreshAllowed_) return;
    if (controller_->getServiceCount() == 0) return;
    if (!idleTimer_.isValid())
    {
        idleTimer_.start();
        return;
    }
    if (idleTimer_.elapsed() < idleThresholdMs_) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (lastRefreshEpochMs_ != 0 && (now - lastRefreshEpochMs_) < autoRefreshIntervalMs_) return;
    qInfo() << "mcp auto refresh tools executed";
    refreshTools();
}
