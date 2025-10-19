// xmcp.cpp
#include "xmcp.h"
#include <QDebug>
#include <unordered_map>

namespace
{
bool syncSelectedMcpTools(const mcp::json &allTools)
{
    if (!allTools.is_array()) return false;

    std::unordered_map<std::string, const mcp::json *> available;
    available.reserve(allTools.size());
    for (const auto &tool : allTools)
    {
        if (!tool.is_object()) continue;
        const std::string service = get_string_safely(tool, "service");
        const std::string name = get_string_safely(tool, "name");
        if (service.empty() || name.empty()) continue;
        const std::string key = service + "@" + name;
        available.emplace(key, &tool);
    }

    bool changed = false;
    auto it = MCP_TOOLS_INFO_LIST.begin();
    while (it != MCP_TOOLS_INFO_LIST.end())
    {
        const std::string key = it->name.toStdString();
        auto found = available.find(key);
        if (found == available.end())
        {
            it = MCP_TOOLS_INFO_LIST.erase(it);
            changed = true;
            continue;
        }

        const mcp::json *tool = found->second;
        const QString newDescription = QString::fromStdString(get_string_safely(*tool, "description"));
        mcp::json schema = sanitize_schema(tool->value("inputSchema", mcp::json::object()));
        const QString newArguments = QString::fromStdString(schema.dump());
        if (it->description != newDescription || it->arguments != newArguments)
        {
            it->description = newDescription;
            it->arguments = newArguments;
            it->generateToolText();
            changed = true;
        }
        ++it;
    }
    return changed;
}

mcp::json sanitizeToolsInfo(mcp::json tools)
{
    if (!tools.is_array()) return tools;
    for (auto &tool : tools)
    {
        if (!tool.is_object()) continue;
        auto schemaIt = tool.find("inputSchema");
        if (schemaIt != tool.end())
        {
            *schemaIt = sanitize_schema(*schemaIt);
        }
    }
    return tools;
}
} // namespace

xMcp::xMcp(QObject *parent)
    : QObject(parent)
{
    qDebug() << "mcp init over";
}

void xMcp::addService(const QString mcp_json_str)
{
    toolManager.clear();         // 清空工具
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
            std::string res = toolManager.addServer(name, serverConfig);
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
    MCP_TOOLS_INFO_ALL = sanitizeToolsInfo(toolManager.getAllToolsInfo());
    syncSelectedMcpTools(MCP_TOOLS_INFO_ALL);
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
    auto result2 = toolManager.callTool(mcp_server_name, mcp_tool_name, params);
    result = QString::fromStdString(result2.dump());
    callTool_over(invocationId, result);
}

// 查询mcp可用工具
void xMcp::callList(quint64 invocationId)
{
    MCP_TOOLS_INFO_ALL = sanitizeToolsInfo(toolManager.getAllToolsInfo());
    syncSelectedMcpTools(MCP_TOOLS_INFO_ALL);
    emit toolsRefreshed();
    emit callList_over(invocationId);
}

void xMcp::refreshTools()
{
    MCP_TOOLS_INFO_ALL = sanitizeToolsInfo(toolManager.getAllToolsInfo());
    syncSelectedMcpTools(MCP_TOOLS_INFO_ALL);
    emit toolsRefreshed();
    emit mcp_message(QStringLiteral("tools refreshed"));
}

void xMcp::disconnectAll()
{
    toolManager.clear();
    MCP_TOOLS_INFO_ALL = mcp::json::array();
    if (!MCP_TOOLS_INFO_LIST.empty())
    {
        MCP_TOOLS_INFO_LIST.clear();
    }
    emit toolsRefreshed();
    emit addService_over(MCP_CONNECT_MISS);
    emit mcp_message(QStringLiteral("all services disconnected"));
}
