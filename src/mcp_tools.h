#include <iostream>
#include <memory>
#include <unordered_map>
#include <stdexcept>
#include "mcp_message.h"
#include "mcp_sse_client.h"
#include "mcp_stdio_client.h"
#include <string>

struct ParsedURL {
    std::string protocol;
    std::string host;
    int port;
    std::string path;
};

inline ParsedURL parse_url(const std::string& url) {
    ParsedURL result;
    size_t protocol_end = url.find("://");
    bool has_protocol = (protocol_end != std::string::npos);
    size_t host_start = 0;

    if (has_protocol) {
        result.protocol = url.substr(0, protocol_end);
        host_start = protocol_end + 3;
    }

    size_t port_pos = url.find(':', host_start);
    size_t path_pos = url.find('/', host_start);

    // 处理端口和主机名
    if (port_pos != std::string::npos && (path_pos == std::string::npos || port_pos < path_pos)) {
        result.host = url.substr(host_start, port_pos - host_start);
        size_t port_end = (path_pos != std::string::npos) ? path_pos : url.length();
        try {
            result.port = std::stoi(url.substr(port_pos + 1, port_end - port_pos - 1));
        } catch (...) {
            // 无效端口处理，保留默认端口
            result.port = has_protocol ? (result.protocol == "https" ? 443 : 80) : 80;
        }
    } else {
        result.host = url.substr(host_start, path_pos - host_start);
        // 设置默认端口
        result.port = has_protocol ? (result.protocol == "https" ? 443 : 80) : 80;
    }

    // 处理路径
    if (path_pos != std::string::npos) {
        result.path = url.substr(path_pos);
    } else {
        result.path = "/";
    }

    return result;
}

// 客户端异常类型
class client_exception : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// 客户端包装器基类
class ClientWrapper {
public:
    virtual ~ClientWrapper() = default;
    virtual mcp::json get_server_capabilities() = 0;
    virtual std::vector<mcp::tool> get_tools() = 0;
    virtual mcp::json call_tool(const std::string& toolName, const mcp::json& params = {}) = 0;
};

// SSE客户端包装器
class SSEClientWrapper : public ClientWrapper {
    mcp::sse_client client_;
    
public:
    SSEClientWrapper(const std::string& host, int port, int timeout, const mcp::json& capabilities)
        : client_(host, port) 
    {
        // 设置客户端参数
        client_.set_timeout(timeout);
        client_.set_capabilities(capabilities);

        // 执行初始化
        if (!client_.initialize("ExampleClient", mcp::MCP_VERSION)) {
            throw client_exception("SSE client initialization failed: " + host + ":" + std::to_string(port));
        }

        // 验证连接
        if (!client_.ping()) {
            throw client_exception("SSE server ping failed: " + host + ":" + std::to_string(port));
        }
    }

    mcp::json get_server_capabilities() override {
        return client_.get_server_capabilities();
    }

    std::vector<mcp::tool> get_tools() override {
        return client_.get_tools();
    }

    mcp::json call_tool(const std::string& toolName, const mcp::json& params = {}) override {
        return client_.call_tool(toolName, params);
    }
};

// Stdio客户端包装器
class StdioClientWrapper : public ClientWrapper {
    mcp::stdio_client client_;
    
    static std::string join_command(const std::string& base, const std::vector<std::string>& args) {
        std::string full_cmd = base;
        for (const auto& arg : args) {
            full_cmd += " " + arg;
        }
        return full_cmd;
    }

public:
    StdioClientWrapper(const std::string& command,
                      const std::vector<std::string>& args,
                      const mcp::json& env)
        : client_(join_command(command, args), env)
    {
        // 初始化客户端
        if (!client_.initialize("StdioClient", mcp::MCP_VERSION)) {
            throw client_exception("Stdio client initialization failed: " + command);
        }

        // 验证连接
        if (!client_.ping()) {
            throw client_exception("Stdio server ping failed: " + command);
        }
    }

    mcp::json get_server_capabilities() override {
        return client_.get_server_capabilities();
    }

    std::vector<mcp::tool> get_tools() override {
        return client_.get_tools();
    }

    mcp::json call_tool(const std::string& toolName, const mcp::json& params = {}) override {
        return client_.call_tool(toolName, params);
    }
};


// 客户端工厂
class ClientFactory {
public:
    static std::unique_ptr<ClientWrapper> create(const mcp::json& config) {
        const std::string type = config["type"];
        
        if (type == "sse") {
            std::string url = config["url"];
            auto parsed = parse_url(url);
            int timeout = config.value("timeout", 10);
            mcp::json capabilities = {{"roots", {{"listChanged", true}}}};
            
            return std::make_unique<SSEClientWrapper>(parsed.host, parsed.port, timeout, capabilities);
        }
        else if (type == "stdio") {
            std::string command = config["command"];
            std::vector<std::string> args = config["args"];
            mcp::json env = config.value("env", mcp::json::object());
            
            return std::make_unique<StdioClientWrapper>(command, args, env);
        }
        
        throw client_exception("Unsupported client type: " + type);
    }
};

class McpToolManager {
public:
    // 添加服务到管理器
    void addServer(const std::string& name, const mcp::json& config) {
        try {
            auto client = ClientFactory::create(config);
            auto tools = client->get_tools();
            clients_.emplace(name, std::move(client));
            tools_cache_[name] = tools;
        } catch (const client_exception& e) {
            throw client_exception("Failed to add server '" + name + "': " + e.what());
        }
    }

    // 调用工具
    mcp::json callTool(const std::string& serviceName, 
                        const std::string& toolName, 
                        const mcp::json& params = {}) {
        if (clients_.empty()) {return mcp::json{{"error", "No clients available."}};}
        // 查找客户端
        auto client_it = clients_.find(serviceName);
        if (client_it == clients_.end()) {
            return mcp::json{{"error", "Service '" + serviceName + "' not registered."}};
        }

        // 检查工具是否存在
        auto& tools = tools_cache_[serviceName];
        auto tool_it = std::find_if(tools.begin(), tools.end(),
            [&toolName](const mcp::tool& t) { return t.name == toolName; });
        
        if (tool_it == tools.end()) {
            return mcp::json{{"error", "Tool '" + toolName + "' not found in service '" + serviceName + "'."}};
        }

        // 调用工具并捕获可能的异常
        try {
            return client_it->second->call_tool(toolName, params);
        } catch (const std::exception& e) {
            return mcp::json{{"error", std::string("Tool call failed: ") + e.what()}};
        } catch (...) {
            return mcp::json{{"error", "Unknown error occurred during tool call."}};
        }
    }

    // 获取所有服务名称
    std::vector<std::string> getServiceNames() const {
        std::vector<std::string> names;
        if(clients_.empty()){return names;}
        for (const auto& pair : clients_) {
            names.push_back(pair.first);
        }
        return names;
    }

    // 获取服务的工具列表
    const std::vector<mcp::tool>& getTools(const std::string& serviceName) const {
        auto it = tools_cache_.find(serviceName);
        // if (it == tools_cache_.end()) {
        //     throw client_exception("Service '" + serviceName + "' not found.");
        // }
        return it->second;
    }

    // 获取所有工具的信息，包括服务、名称、描述、输入模式
    mcp::json getAllToolsInfo() const {
        mcp::json result = mcp::json::array();
        for (const auto& service_entry : tools_cache_) {
            const std::string& service_name = service_entry.first;
            for (const mcp::tool& tool : service_entry.second) {
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
    void clear() noexcept {
        clients_.clear();
        tools_cache_.clear();
    }

    // 获取已添加服务的个数
    size_t getServiceCount() const noexcept {
        if(clients_.empty()){return 0;}
        return clients_.size();
    }
private:
    std::unordered_map<std::string, std::unique_ptr<ClientWrapper>> clients_;
    std::unordered_map<std::string, std::vector<mcp::tool>> tools_cache_;
};

// 使用示例

// #include <iostream>
// #include "mcp_tools.h"


// int main() {
//     // 示例配置（包含多个服务）
//     const std::string config_text = R"({
//         "mcpServers": {
//             "sse-server": {
//                 "type": "sse",
//                 "url": "http://192.168.104.67:3001",
//                 "timeout": 10
//             },
//             "stdio-server": {
//                 "type": "stdio",
//                 "command": "npx",
//                 "args": ["-y", "@modelcontextprotocol/server-everything"],
//                 "env": {"MCP_DEBUG": "1"}
//             }
//         }
//     })";

//     // 创建并初始化所有客户端, 并将客户端全部视作工具
//     mcp::json config = mcp::json::parse(config_text);
//     McpToolManager toolManager;// 初始化工具管理器
//     for (auto& [name, serverConfig] : config["mcpServers"].items()) {
//         try {
//             toolManager.addServer(name, serverConfig);
//             std::cout << "addServer success: " << name << std::endl;
//         } catch (const client_exception& e) {
//             std::cerr << "addServer fail (" << name << "): " << e.what() << std::endl;
//         }
//     }

//     // 获取所有工具信息
//     auto tools_info = toolManager.getAllToolsInfo();
//     // 展示某个工具信息
//     int k = 3;
//     std::cout << "tool_name: " << tools_info.at(k)["service"].get<std::string>() + "@" + tools_info.at(k)["name"].get<std::string>() << std::endl;
//     std::cout << "description: " << tools_info.at(k)["description"] << std::endl;
//     std::cout << "inputSchema: " << tools_info.at(k)["inputSchema"].dump() << std::endl;// 参数结构说明
//     std::cout << "---" << std::endl;

//     // 示例调用
//     try {
//         // 调用stdio服务的加法工具
//         mcp::json params;
//         params["a"] = 1;
//         params["b"] = 2;
//         auto result1 = toolManager.callTool("stdio-server", "add", params);
//         std::cout << result1 << std::endl;

//         // 调用sse服务的计算工具
//         std::string llm_tool_name = "sse-server@calculate";// 大模型输出的要调用的工具名
//         std::string mcp_server_name;
//         std::string mcp_tool_name;
//         size_t pos = llm_tool_name.find('@');// 如果找到@则视为mcp服务器提供的工具

//         if (pos != std::string::npos) {
//             mcp_server_name = llm_tool_name.substr(0, pos);
//             mcp_tool_name = llm_tool_name.substr(pos + 1);
//             // std::cout << "Name: " << mcp_server_name << "\nFunction: " << mcp_tool_name << std::endl;
//         } else {std::cout << "No '@' found!" << std::endl;}

//         std::string llm_argBody="{\"expression\":\"2^8\"}";// 大模型输出的要传入的参数
//         mcp::json params2;
//         try {params2 = mcp::json::parse(llm_argBody);} 
//         catch (const std::exception& e) 
//         {   
//             std::cerr << "JSON parse fail: " << e.what() << std::endl;
//             params2 = mcp::json::object(); // 可选：初始化为空对象
//         }
//         auto result2 = toolManager.callTool(mcp_server_name, mcp_tool_name, params2);
//         std::cout << result2 << std::endl;

//     } catch (const client_exception& e) {
//         std::cerr << "callTool fail: " << e.what() << std::endl;
//     }

//     return 0;
// }
