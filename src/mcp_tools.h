#ifndef MCP_TOOLS_H
#define MCP_TOOLS_H

#include "mcp_message.h"
#include "mcp_sse_client.h"
#include "mcp_stdio_client.h"
#include "xconfig.h"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
struct ParsedURL
{
    std::string protocol;
    std::string host;
    int port;
    std::string path;
};

inline ParsedURL parse_url(const std::string &url)
{
    ParsedURL result;
    size_t protocol_end = url.find("://");
    bool has_protocol = (protocol_end != std::string::npos);
    size_t host_start = 0;

    if (has_protocol)
    {
        result.protocol = url.substr(0, protocol_end);
        host_start = protocol_end + 3;
    }

    size_t port_pos = url.find(':', host_start);
    size_t path_pos = url.find('/', host_start);

    // 处理端口和主机名
    if (port_pos != std::string::npos && (path_pos == std::string::npos || port_pos < path_pos))
    {
        result.host = url.substr(host_start, port_pos - host_start);
        size_t port_end = (path_pos != std::string::npos) ? path_pos : url.length();
        try
        {
            result.port = std::stoi(url.substr(port_pos + 1, port_end - port_pos - 1));
        }
        catch (...)
        {
            // 无效端口处理，保留默认端口
            result.port = has_protocol ? (result.protocol == "https" ? 443 : 80) : 80;
        }
    }
    else
    {
        result.host = url.substr(host_start, path_pos - host_start);
        // 设置默认端口
        result.port = has_protocol ? (result.protocol == "https" ? 443 : 80) : 80;
    }

    // 处理路径
    if (path_pos != std::string::npos)
    {
        result.path = url.substr(path_pos);
    }
    else
    {
        result.path = "/";
    }

    return result;
}

// 客户端异常类型
class client_exception : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

// 客户端包装器基类
class ClientWrapper
{
  public:
    virtual ~ClientWrapper() = default;
    virtual mcp::json get_server_capabilities() = 0;
    virtual std::vector<mcp::tool> get_tools() = 0;
    virtual mcp::json call_tool(const std::string &toolName, const mcp::json &params = {}) = 0;
};

// SSE客户端包装器
class SSEClientWrapper : public ClientWrapper
{
    mcp::sse_client client_;

  public:
    SSEClientWrapper(const std::string &host, int port, int timeout, const mcp::json &capabilities)
        : client_(host, port)
    {
        // 设置客户端参数
        client_.set_timeout(timeout);
        client_.set_capabilities(capabilities);

        // 执行初始化
        if (!client_.initialize("ExampleClient", mcp::MCP_VERSION))
        {
            throw client_exception("SSE client initialization failed: " + host + ":" + std::to_string(port));
        }

        // 验证连接
        if (!client_.ping())
        {
            throw client_exception("SSE server ping failed: " + host + ":" + std::to_string(port));
        }
    }

    mcp::json get_server_capabilities() override
    {
        return client_.get_server_capabilities();
    }

    std::vector<mcp::tool> get_tools() override
    {
        return client_.get_tools();
    }

    mcp::json call_tool(const std::string &toolName, const mcp::json &params = {}) override
    {
        return client_.call_tool(toolName, params);
    }
};

// Stdio客户端包装器
class StdioClientWrapper : public ClientWrapper
{
    mcp::stdio_client client_;

    static std::string join_command(const std::string &base, const std::vector<std::string> &args)
    {
        std::string full_cmd = base;
        for (const auto &arg : args)
        {
            full_cmd += " " + arg;
        }
        return full_cmd;
    }

  public:
    StdioClientWrapper(const std::string &command,
                       const std::vector<std::string> &args,
                       const mcp::json &env)
        : client_(join_command(command, args), env)
    {
        // 初始化客户端
        if (!client_.initialize("StdioClient", mcp::MCP_VERSION))
        {
            throw client_exception("Stdio client initialization failed: " + command);
        }

        // 验证连接
        if (!client_.ping())
        {
            throw client_exception("Stdio server ping failed: " + command);
        }
    }

    mcp::json get_server_capabilities() override
    {
        return client_.get_server_capabilities();
    }

    std::vector<mcp::tool> get_tools() override
    {
        return client_.get_tools();
    }

    mcp::json call_tool(const std::string &toolName, const mcp::json &params = {}) override
    {
        return client_.call_tool(toolName, params);
    }
};

// 客户端工厂
class ClientFactory
{
  public:
    static std::unique_ptr<ClientWrapper> create(const mcp::json &config)
    {
        // 优先连接url
        std::string url = get_string_safely(config, "url");
        if (url != "")
        {
            std::string url = config["url"];
            auto parsed = parse_url(url);
            int timeout = get_int_safely(config, "timeout", 10);
            mcp::json capabilities = {{"roots", {{"listChanged", true}}}};
            return std::make_unique<SSEClientWrapper>(parsed.host, parsed.port, timeout, capabilities);
        }
        else
        {
            std::string command = get_string_safely(config, "command");
            std::vector<std::string> args = get_string_list_safely(config, "args"); //取文本列表
            if (command != "" && args.size() != 0)
            {
                mcp::json env = get_json_object_safely(config, "env");
                return std::make_unique<StdioClientWrapper>(command, args, env);
            }

            throw client_exception("unsupported client"); // 通过这个强制返回
        }
    }
};

class McpToolManager
{
  public:
    // 添加服务到管理器
    std::string addServer(const std::string &name, const mcp::json &config)
    {
        std::string result = "";
        try
        {
            auto client = ClientFactory::create(config);
            auto tools = client->get_tools();
            clients_.emplace(name, std::move(client));
            tools_cache_[name] = tools;
        }
        catch (const client_exception &e)
        {
            // throw client_exception("Failed to add server '" + name + "': " + e.what());
            result = "Failed to add server '" + name + "': " + e.what();
            std::cout << result << std::endl;
        }
        return result;
    }

    // 调用工具
    mcp::json callTool(const std::string &serviceName,
                       const std::string &toolName,
                       const mcp::json &params = {})
    {
        if (clients_.empty()) { return mcp::json{{"error", "No clients available."}}; }
        // 查找客户端
        auto client_it = clients_.find(serviceName);
        if (client_it == clients_.end())
        {
            return mcp::json{{"error", "Service '" + serviceName + "' not registered."}};
        }

        // 检查工具是否存在
        auto &tools = tools_cache_[serviceName];
        auto tool_it = std::find_if(tools.begin(), tools.end(),
                                    [&toolName](const mcp::tool &t) { return t.name == toolName; });

        if (tool_it == tools.end())
        {
            return mcp::json{{"error", "Tool '" + toolName + "' not found in service '" + serviceName + "'."}};
        }

        // 调用工具并捕获可能的异常
        try
        {
            return client_it->second->call_tool(toolName, params);
        }
        catch (const std::exception &e)
        {
            return mcp::json{{"error", std::string("Tool call failed: ") + e.what()}};
        }
        catch (...)
        {
            return mcp::json{{"error", "Unknown error occurred during tool call."}};
        }
    }

    // 获取所有服务名称
    std::vector<std::string> getServiceNames() const
    {
        std::vector<std::string> names;
        if (clients_.empty()) { return names; }
        for (const auto &pair : clients_)
        {
            names.push_back(pair.first);
        }
        return names;
    }

    // 获取服务的工具列表
    const std::vector<mcp::tool> &getTools(const std::string &serviceName) const
    {
        auto it = tools_cache_.find(serviceName);
        // if (it == tools_cache_.end()) {
        //     throw client_exception("Service '" + serviceName + "' not found.");
        // }
        return it->second;
    }

    // 获取所有工具的信息，包括服务、名称、描述、输入模式
    mcp::json getAllToolsInfo() const
    {
        mcp::json result = mcp::json::array();
        for (const auto &service_entry : tools_cache_)
        {
            const std::string &service_name = service_entry.first;
            for (const mcp::tool &tool : service_entry.second)
            {
                // 使用工具的to_json方法获取基本信息
                mcp::json tool_info = tool.to_json();
                // 添加服务名称以便区分不同服务的工具
                tool_info["service"] = service_name;
                result.push_back(tool_info);
            }
        }
        return result;
    }

    // 清空所有已添加的服务
    void clear() noexcept
    {
        clients_.clear();
        tools_cache_.clear();
    }

    // 获取已添加服务的个数
    size_t getServiceCount() const noexcept
    {
        if (clients_.empty()) { return 0; }
        return clients_.size();
    }

  private:
    std::unordered_map<std::string, std::unique_ptr<ClientWrapper>> clients_;
    std::unordered_map<std::string, std::vector<mcp::tool>> tools_cache_;
};

#endif