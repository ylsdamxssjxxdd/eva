#ifndef MCP_TOOLS_H
#define MCP_TOOLS_H

#include "mcp_message.h"
#include "mcp_sse_client.h"
#include "mcp_stdio_client.h"
#include "xconfig.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <utility>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
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

    size_t query_pos = result.path.find_first_of("?#");
    if (query_pos != std::string::npos)
    {
        result.path = result.path.substr(0, query_pos);
    }
    if (result.path.empty())
    {
        result.path = "/";
    }

    return result;
}

inline std::string trim_trailing_slashes_keep_root(std::string path)
{
    if (path.empty()) return "/";
    while (path.size() > 1 && path.back() == '/')
    {
        path.pop_back();
    }
    if (path.empty()) return "/";
    return path;
}

inline bool path_ends_with_segment(const std::string &path, const std::string &segment)
{
    if (segment.empty() || segment == "/") return false;
    if (path.size() < segment.size()) return false;
    if (path.compare(path.size() - segment.size(), segment.size(), segment) != 0) return false;
    if (path.size() == segment.size()) return true;
    return path[path.size() - segment.size() - 1] == '/';
}

inline std::string combine_paths(const std::string &base_path, const std::string &endpoint)
{
    if (endpoint.empty())
    {
        return trim_trailing_slashes_keep_root(base_path.empty() ? std::string("/") : base_path);
    }

    if (base_path.empty() || base_path == "/")
    {
        if (!endpoint.empty() && endpoint.front() == '/')
        {
            return trim_trailing_slashes_keep_root(endpoint);
        }
        std::string result = "/" + endpoint;
        return trim_trailing_slashes_keep_root(result);
    }

    std::string result = base_path;
    if (result.front() != '/')
    {
        result.insert(result.begin(), '/');
    }
    result = trim_trailing_slashes_keep_root(result);

    if (!endpoint.empty() && endpoint.front() == '/')
    {
        result += endpoint;
    }
    else
    {
        result.push_back('/');
        result += endpoint;
    }

    return trim_trailing_slashes_keep_root(result);
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
    std::unique_ptr<mcp::sse_client> client_;

    static void apply_headers(mcp::sse_client &client, const mcp::json &headers)
    {
        if (!headers.is_object()) return;
        for (auto it = headers.begin(); it != headers.end(); ++it)
        {
            const std::string headerKey = it.key();
            if (headerKey.empty()) continue;
            const mcp::json &headerValue = it.value();
            if (headerValue.is_null()) continue;
            std::string valueText = headerValue.is_string() ? headerValue.get<std::string>() : headerValue.dump();
            client.set_header(headerKey, valueText);
        }
    }

  public:
    SSEClientWrapper(std::unique_ptr<mcp::sse_client> client,
                     int timeout,
                     const mcp::json &capabilities,
                     const mcp::json &headers,
                     const std::string &clientName)
        : client_(std::move(client))
    {
        if (!client_)
        {
            throw client_exception("Invalid SSE client instance");
        }

        client_->set_timeout(timeout);
        client_->set_capabilities(capabilities);
        apply_headers(*client_, headers);

        if (!client_->initialize(clientName, mcp::MCP_VERSION))
        {
            throw client_exception("SSE client initialization failed");
        }

        if (!client_->ping())
        {
            throw client_exception("SSE server ping failed");
        }
    }

    mcp::json get_server_capabilities() override
    {
        return client_->get_server_capabilities();
    }

    std::vector<mcp::tool> get_tools() override
    {
        return client_->get_tools();
    }

    mcp::json call_tool(const std::string &toolName, const mcp::json &params = {}) override
    {
        return client_->call_tool(toolName, params);
    }
};

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
                       const mcp::json &env,
                       const std::string &clientName)
        : client_(join_command(command, args), env)
    {
        if (!client_.initialize(clientName, mcp::MCP_VERSION))
        {
            throw client_exception("Stdio client initialization failed: " + command);
        }

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

class ClientFactory
{
    static std::string to_lower_copy(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char ch)
                       { return static_cast<char>(std::tolower(ch)); });
        return text;
    }

    static std::string normalize_endpoint(std::string endpoint)
    {
        if (endpoint.empty()) endpoint = "/sse";
        if (endpoint.front() != '/') endpoint.insert(endpoint.begin(), '/');
        return endpoint;
    }

    static mcp::json resolve_capabilities(const mcp::json &config)
    {
        mcp::json capabilities = get_json_object_safely(config, "capabilities");
        if (!capabilities.is_object()) capabilities = mcp::json::object();
        if (!capabilities.contains("roots") || !capabilities["roots"].is_object())
        {
            capabilities["roots"] = mcp::json::object();
        }
        capabilities["roots"]["listChanged"] = true;
        return capabilities;
    }

  public:
    static std::unique_ptr<ClientWrapper> create(const mcp::json &config)
    {
        std::string type = to_lower_copy(get_string_safely(config, "type"));
        if (type.empty())
        {
            if (config.contains("baseUrl") || config.contains("url"))
            {
                type = "sse";
            }
            else if (config.contains("command"))
            {
                type = "stdio";
            }
        }

        const int timeout = get_int_safely(config, "timeout", 10);
        mcp::json capabilities = resolve_capabilities(config);
        const std::string clientName = get_string_safely(config, "clientName",
                                                         type == "stdio" ? "EvaMcpStdioClient" : "EvaMcpSseClient");

        if (type == "sse")
        {
            const std::string endpoint_raw = get_string_safely(config, "sseEndpoint", "/sse");
            std::string endpoint = normalize_endpoint(endpoint_raw);
            const bool endpoint_explicit = config.contains("sseEndpoint");
            const bool endpoint_is_relative = endpoint_explicit && !endpoint_raw.empty() && endpoint_raw.front() != '/';
            const mcp::json headers = get_json_object_safely(config, "headers");
            std::unique_ptr<mcp::sse_client> client;

            std::string baseUrl = get_string_safely(config, "baseUrl");
            if (!baseUrl.empty())
            {
                ParsedURL parsed = parse_url(baseUrl);
                if (parsed.host.empty())
                {
                    throw client_exception("SSE client configuration has empty host");
                }
                std::string scheme = parsed.protocol.empty() ? "http" : parsed.protocol;
                if (scheme != "http" && scheme != "https")
                {
                    throw client_exception("Unsupported URL scheme for SSE client: " + scheme);
                }
                const bool is_default_port = (scheme == "http" && parsed.port == 80) ||
                                             (scheme == "https" && parsed.port == 443);
                std::string scheme_host_port = scheme + "://" + parsed.host;
                if (!is_default_port && parsed.port > 0)
                {
                    scheme_host_port += ":" + std::to_string(parsed.port);
                }

                std::string base_path = parsed.path.empty() ? "/" : parsed.path;
                base_path = trim_trailing_slashes_keep_root(base_path);

                if (!endpoint_explicit)
                {
                    if (!base_path.empty() && base_path != "/")
                    {
                        if (path_ends_with_segment(base_path, endpoint))
                        {
                            endpoint = base_path;
                        }
                        else
                        {
                            endpoint = combine_paths(base_path, endpoint);
                        }
                    }
                }
                else if (endpoint_is_relative && !base_path.empty() && base_path != "/")
                {
                    endpoint = combine_paths(base_path, endpoint);
                }

                endpoint = normalize_endpoint(endpoint);
                client = std::make_unique<mcp::sse_client>(scheme_host_port, endpoint);
            }
            else
            {
                std::string url = get_string_safely(config, "url");
                if (url.empty())
                {
                    throw client_exception("SSE client configuration requires baseUrl or url");
                }
                ParsedURL parsed = parse_url(url);
                if (parsed.host.empty())
                {
                    throw client_exception("SSE client configuration has empty host");
                }
                std::string scheme = parsed.protocol.empty() ? "http" : parsed.protocol;
                if (scheme != "http" && scheme != "https")
                {
                    throw client_exception("Unsupported URL scheme for SSE client: " + scheme);
                }
                const bool is_default_port = (scheme == "http" && parsed.port == 80) ||
                                             (scheme == "https" && parsed.port == 443);
                std::string scheme_host_port = scheme + "://" + parsed.host;
                if (!is_default_port && parsed.port > 0)
                {
                    scheme_host_port += ":" + std::to_string(parsed.port);
                }
                if (!parsed.path.empty() && parsed.path != "/" && !endpoint_explicit && endpoint == "/sse")
                {
                    endpoint = trim_trailing_slashes_keep_root(parsed.path);
                }
                endpoint = normalize_endpoint(endpoint);
                client = std::make_unique<mcp::sse_client>(scheme_host_port, endpoint);
            }

            return std::make_unique<SSEClientWrapper>(std::move(client), timeout, capabilities, headers, clientName);
        }

        if (type == "stdio")
        {
            std::string command = get_string_safely(config, "command");
            if (command.empty())
            {
                throw client_exception("Stdio client configuration requires command");
            }
            std::vector<std::string> args = get_string_list_safely(config, "args");
            mcp::json env = get_json_object_safely(config, "env");

            return std::make_unique<StdioClientWrapper>(command, args, env, clientName);
        }

        throw client_exception("Unsupported MCP client type: " + type);
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
                                    [&toolName](const mcp::tool &t)
                                    { return t.name == toolName; });

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
