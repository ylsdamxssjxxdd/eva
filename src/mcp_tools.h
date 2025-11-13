#ifndef MCP_TOOLS_H
#define MCP_TOOLS_H

#include "mcp_json.h"
#include "xconfig.h"

#include "qmcp/errors.h"
#include "qmcp/sseclient.h"
#include "qmcp/streamablehttpclient.h"
#include "qmcp/stdioclient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QObject>
#include <QUrl>

#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class client_exception : public std::runtime_error
{
  public:
    explicit client_exception(const std::string &message)
        : std::runtime_error(message)
    {
    }
};

namespace mcp_internal
{

inline std::string to_lower_copy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

inline QJsonValue to_qjson_value(const mcp::json &value)
{
    if (value.is_null()) return QJsonValue();
    if (value.is_boolean()) return QJsonValue(value.get<bool>());
    if (value.is_number_integer()) return QJsonValue(static_cast<double>(value.get<long long>()));
    if (value.is_number_unsigned()) return QJsonValue(static_cast<double>(value.get<unsigned long long>()));
    if (value.is_number_float()) return QJsonValue(value.get<double>());
    if (value.is_string()) return QJsonValue(QString::fromStdString(value.get<std::string>()));
    if (value.is_array())
    {
        QJsonArray arr;
        for (const auto &entry : value)
        {
            arr.append(to_qjson_value(entry));
        }
        return arr;
    }
    if (value.is_object())
    {
        QJsonObject obj;
        for (const auto &entry : value.items())
        {
            obj.insert(QString::fromStdString(entry.key()), to_qjson_value(entry.value()));
        }
        return obj;
    }
    return QJsonValue();
}

inline QJsonObject to_qjson_object(const mcp::json &value)
{
    const QJsonValue converted = to_qjson_value(value);
    return converted.isObject() ? converted.toObject() : QJsonObject{};
}

inline QJsonArray to_qjson_array(const mcp::json &value)
{
    const QJsonValue converted = to_qjson_value(value);
    return converted.isArray() ? converted.toArray() : QJsonArray{};
}

inline mcp::json to_mcp_json(const QJsonValue &value)
{
    if (value.isNull() || value.isUndefined()) return mcp::json();
    if (value.isBool()) return value.toBool();
    if (value.isDouble()) return value.toDouble();
    if (value.isString()) return value.toString().toStdString();
    if (value.isArray())
    {
        mcp::json array = mcp::json::array();
        for (const QJsonValue &entry : value.toArray())
        {
            array.push_back(to_mcp_json(entry));
        }
        return array;
    }
    if (value.isObject())
    {
        mcp::json object = mcp::json::object();
        const QJsonObject obj = value.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        {
            object[it.key().toStdString()] = to_mcp_json(it.value());
        }
        return object;
    }
    return mcp::json();
}

inline QUrl resolve_endpoint(const QString &baseValue, const QString &endpointValue, bool endpointExplicit)
{
    QUrl endpoint(endpointValue);
    if (endpoint.isValid() && !endpoint.isRelative())
    {
        return endpoint;
    }

    QUrl base(baseValue);
    if (!base.isValid() || base.scheme().isEmpty())
    {
        throw client_exception("SSE client configuration has invalid baseUrl");
    }

    QString normalizedEndpoint = endpointValue;
    if (normalizedEndpoint.isEmpty())
    {
        return base;
    }

    const QString basePath = base.path();
    const bool hasBasePath = !basePath.isEmpty() && basePath != QStringLiteral("/");

    if (!endpointExplicit && hasBasePath)
    {
        if (basePath.endsWith(normalizedEndpoint))
        {
            normalizedEndpoint = basePath;
        }
        else
        {
            QString path = basePath;
            if (!path.endsWith('/')) path.append('/');
            QString relative = normalizedEndpoint;
            if (relative.startsWith('/')) relative.remove(0, 1);
            normalizedEndpoint = path + relative;
        }
    }
    else if (endpointExplicit && hasBasePath && !normalizedEndpoint.startsWith('/'))
    {
        QString path = basePath;
        if (!path.endsWith('/')) path.append('/');
        normalizedEndpoint = path + normalizedEndpoint;
    }

    if (normalizedEndpoint.isEmpty())
    {
        return base;
    }
    QUrl relative(normalizedEndpoint);
    return base.resolved(relative);
}

} // namespace mcp_internal

class McpToolManager
{
  public:
    using NotificationHandler = std::function<void(const QString &, const QString &, const QString &)>;

    void setNotificationHandler(NotificationHandler handler) { notificationHandler_ = std::move(handler); }

    std::string addServer(const std::string &name, const mcp::json &config)
    {
        if (!config.is_object())
        {
            return "Server config must be a JSON object.";
        }

        try
        {
            qmcp::ServerConfig serverConfig = buildServerConfig(name, config);
            std::unique_ptr<qmcp::McpClient> client = createClient(serverConfig);
            std::string defaultClientName = "EvaQtMcpSseClient";
            if (serverConfig.transport == qmcp::TransportType::Stdio)
            {
                defaultClientName = "EvaQtMcpStdioClient";
            }
            else if (serverConfig.transport == qmcp::TransportType::StreamableHttp)
            {
                defaultClientName = "EvaQtMcpStreamableHttpClient";
            }
            const std::string clientName = get_string_safely(config, "clientName", defaultClientName);
            const std::string clientVersion = get_string_safely(config, "clientVersion", "1.0.0");

            if (!client->initialize(QString::fromStdString(clientName), QString::fromStdString(clientVersion)))
            {
                std::string transport;
                switch (serverConfig.transport)
                {
                case qmcp::TransportType::Stdio:
                    transport = "stdio";
                    break;
                case qmcp::TransportType::StreamableHttp:
                    transport = "streamableHttp";
                    break;
                case qmcp::TransportType::Sse:
                default:
                    transport = "sse";
                    break;
                }
                return "Failed to initialize " + transport + " server '" + name + "'";
            }

            const QJsonArray tools = client->listTools();

            if (notificationHandler_)
            {
                QObject::connect(client.get(),
                                 &qmcp::McpClient::serverMessageReceived,
                                 client.get(),
                                 [this](const QString &serviceKey, const QString &level, const QString &message)
                                 {
                                     if (notificationHandler_)
                                     {
                                         notificationHandler_(serviceKey, level, message);
                                     }
                                 });
            }

            ClientEntry entry;
            entry.client = std::move(client);
            entry.tools.reserve(static_cast<size_t>(tools.size()));
            for (const QJsonValue &tool : tools)
            {
                entry.tools.push_back(mcp_internal::to_mcp_json(tool));
            }

            clients_[name] = std::move(entry);
            return {};
        }
        catch (const client_exception &ex)
        {
            return ex.what();
        }
        catch (const qmcp::McpError &ex)
        {
            return ex.what();
        }
        catch (const std::exception &ex)
        {
            return ex.what();
        }
    }

    mcp::json callTool(const std::string &serviceName, const std::string &toolName, const mcp::json &params = {})
    {
        if (clients_.empty()) return mcp::json{{"error", "No clients available."}};
        auto clientIt = clients_.find(serviceName);
        if (clientIt == clients_.end())
        {
            return mcp::json{{"error", "Service '" + serviceName + "' not registered."}};
        }

        auto &tools = clientIt->second.tools;
        auto toolIt = std::find_if(tools.begin(), tools.end(), [&](const mcp::json &tool) {
            return tool.is_object() && tool.value("name", "") == toolName;
        });

        if (toolIt == tools.end())
        {
            return mcp::json{{"error", "Tool '" + toolName + "' not found in service '" + serviceName + "'."}};
        }

        try
        {
            const QJsonValue paramValue = mcp_internal::to_qjson_value(params);
            const QJsonObject arguments = paramValue.isObject() ? paramValue.toObject() : QJsonObject{};
            QJsonObject result = clientIt->second.client->callTool(QString::fromStdString(toolName), arguments);
            return mcp_internal::to_mcp_json(result);
        }
        catch (const qmcp::McpError &ex)
        {
            return mcp::json{{"error", ex.what()}};
        }
        catch (const std::exception &ex)
        {
            return mcp::json{{"error", ex.what()}};
        }
    }

    std::vector<std::string> getServiceNames() const
    {
        std::vector<std::string> names;
        names.reserve(clients_.size());
        for (const auto &pair : clients_)
        {
            names.push_back(pair.first);
        }
        return names;
    }

    const std::vector<mcp::json> &getTools(const std::string &serviceName) const
    {
        static const std::vector<mcp::json> kEmpty;
        auto it = clients_.find(serviceName);
        return it == clients_.end() ? kEmpty : it->second.tools;
    }

    bool refreshAllTools(const QSet<QString> *allowedServices = nullptr)
    {
        bool changed = false;
        for (auto &service : clients_)
        {
            if (allowedServices && !allowedServices->contains(QString::fromStdString(service.first)))
            {
                continue;
            }
            try
            {
                const QJsonArray tools = service.second.client->listTools();
                std::vector<mcp::json> updated;
                updated.reserve(static_cast<size_t>(tools.size()));
                for (const QJsonValue &tool : tools)
                {
                    updated.push_back(mcp_internal::to_mcp_json(tool));
                }
                if (updated != service.second.tools)
                {
                    service.second.tools = std::move(updated);
                    changed = true;
                }
            }
            catch (const qmcp::McpError &ex)
            {
                qWarning() << "Failed to refresh tools for" << QString::fromStdString(service.first) << ":" << ex.what();
            }
            catch (const std::exception &ex)
            {
                qWarning() << "Failed to refresh tools for" << QString::fromStdString(service.first) << ":" << ex.what();
            }
        }
        return changed;
    }

    mcp::json getAllToolsInfo() const
    {
        mcp::json result = mcp::json::array();
        for (const auto &service : clients_)
        {
            for (const mcp::json &tool : service.second.tools)
            {
                mcp::json toolInfo = tool;
                toolInfo["service"] = service.first;
                result.push_back(toolInfo);
            }
        }
        return result;
    }

    void clear() noexcept
    {
        clients_.clear();
    }

    size_t getServiceCount() const noexcept { return clients_.size(); }

  private:
    struct ClientEntry
    {
        std::unique_ptr<qmcp::McpClient> client;
        std::vector<mcp::json> tools;
    };

    static qmcp::ServerConfig buildServerConfig(const std::string &name, const mcp::json &config)
    {
        qmcp::ServerConfig serverConfig;
        serverConfig.key = QString::fromStdString(name);
        serverConfig.name = QString::fromStdString(get_string_safely(config, "name", name));
        serverConfig.description = QString::fromStdString(get_string_safely(config, "description"));
        serverConfig.isActive = get_bool_safely(config, "isActive", true);

        const std::string type = mcp_internal::to_lower_copy(get_string_safely(config, "type", "sse"));
        if (type == "stdio")
        {
            serverConfig.transport = qmcp::TransportType::Stdio;
            const std::string command = get_string_safely(config, "command");
            if (command.empty())
            {
                throw client_exception("Stdio client configuration requires command");
            }
            serverConfig.command = QString::fromStdString(command);
            const std::vector<std::string> args = get_string_list_safely(config, "args");
            for (const std::string &arg : args)
            {
                serverConfig.args.append(QString::fromStdString(arg));
            }
        }
        else if (type == "streamablehttp" || type == "http")
        {
            serverConfig.transport = qmcp::TransportType::StreamableHttp;
            const std::string baseUrl = get_string_safely(config, "baseUrl", get_string_safely(config, "url"));
            if (baseUrl.empty())
            {
                throw client_exception("Streamable HTTP client configuration requires baseUrl");
            }
            serverConfig.baseUrl = QUrl(QString::fromStdString(baseUrl));
            if (!serverConfig.baseUrl.isValid() || serverConfig.baseUrl.scheme().isEmpty())
            {
                throw client_exception("Streamable HTTP baseUrl must be an absolute URL");
            }
        }
        else
        {
            serverConfig.transport = qmcp::TransportType::Sse;
            const std::string baseUrl = get_string_safely(config, "baseUrl", get_string_safely(config, "url"));
            const std::string endpoint = get_string_safely(config, "sseEndpoint", "");
            if (baseUrl.empty() && endpoint.empty())
            {
                throw client_exception("SSE client configuration requires baseUrl or sseEndpoint");
            }
            if (baseUrl.empty())
            {
                QUrl endpointUrl(QString::fromStdString(endpoint));
                if (!endpointUrl.isValid() || endpointUrl.scheme().isEmpty())
                {
                    throw client_exception("SSE endpoint must be absolute when baseUrl is missing");
                }
                serverConfig.baseUrl = endpointUrl;
            }
            else
            {
                const bool endpointExplicit = config.contains("sseEndpoint");
                serverConfig.baseUrl = mcp_internal::resolve_endpoint(QString::fromStdString(baseUrl),
                                                                      QString::fromStdString(endpoint),
                                                                      endpointExplicit);
            }
        }

        serverConfig.env = mcp_internal::to_qjson_object(get_json_object_safely(config, "env"));
        serverConfig.headers = mcp_internal::to_qjson_object(get_json_object_safely(config, "headers"));

        return serverConfig;
    }

    static std::unique_ptr<qmcp::McpClient> createClient(const qmcp::ServerConfig &config)
    {
        if (config.transport == qmcp::TransportType::Stdio)
        {
            return std::make_unique<qmcp::StdioClient>(config);
        }
        if (config.transport == qmcp::TransportType::StreamableHttp)
        {
            return std::make_unique<qmcp::StreamableHttpClient>(config);
        }
        return std::make_unique<qmcp::SseClient>(config);
    }

    std::unordered_map<std::string, ClientEntry> clients_;
    NotificationHandler notificationHandler_;
};

#endif // MCP_TOOLS_H
