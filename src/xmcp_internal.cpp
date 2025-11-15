#include "xmcp_internal.h"

#include <unordered_map>
#include <unordered_set>

namespace eva::mcp
{
bool syncSelectedMcpTools(const ::mcp::json &allTools, const QSet<QString> *enabledFilter)
{
    if (!allTools.is_array()) return false;

    std::unordered_map<std::string, const ::mcp::json *> available;
    available.reserve(allTools.size());
    for (const auto &tool : allTools)
    {
        if (!tool.is_object()) continue;
        const QString serviceName = QString::fromStdString(get_string_safely(tool, "service"));
        const QString toolName = QString::fromStdString(get_string_safely(tool, "name"));
        if (serviceName.isEmpty() || toolName.isEmpty()) continue;
        if (enabledFilter && !enabledFilter->contains(serviceName)) continue;
        const std::string key = (serviceName + "@" + toolName).toStdString();
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

        const ::mcp::json *tool = found->second;
        const QString newDescription = QString::fromStdString(get_string_safely(*tool, "description"));
        ::mcp::json schema = sanitize_schema(tool->value("inputSchema", ::mcp::json::object()));
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

    std::unordered_set<std::string> existingKeys;
    existingKeys.reserve(MCP_TOOLS_INFO_LIST.size());
    for (const auto &entry : MCP_TOOLS_INFO_LIST)
    {
        existingKeys.insert(entry.name.toStdString());
    }

    for (const auto &pair : available)
    {
        if (existingKeys.find(pair.first) != existingKeys.end()) continue;
        const ::mcp::json *tool = pair.second;
        if (!tool || !tool->is_object()) continue;
        const QString service = QString::fromStdString(get_string_safely(*tool, "service"));
        if (enabledFilter && !enabledFilter->contains(service)) continue;
        const QString toolName = QString::fromStdString(get_string_safely(*tool, "name"));
        if (service.isEmpty() || toolName.isEmpty()) continue;
        const QString description = QString::fromStdString(get_string_safely(*tool, "description"));
        ::mcp::json schema = sanitize_schema(tool->value("inputSchema", ::mcp::json::object()));
        const QString arguments = QString::fromStdString(schema.dump());
        MCP_TOOLS_INFO_LIST.emplace_back(service + "@" + toolName, description, arguments);
        existingKeys.insert(pair.first);
        changed = true;
    }
    return changed;
}

::mcp::json sanitizeToolsInfo(::mcp::json tools)
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
} // namespace eva::mcp
