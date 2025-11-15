#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

#include "mcp_tools.h"
#include "xmcp_internal.h"

namespace
{
struct ToolsListGuard
{
    ToolsListGuard()
        : snapshot(MCP_TOOLS_INFO_LIST)
    {
        MCP_TOOLS_INFO_LIST.clear();
    }
    ~ToolsListGuard() { MCP_TOOLS_INFO_LIST = snapshot; }
    std::vector<TOOLS_INFO> snapshot;
};

mcp::json toolEntry(const char *service, const char *name, const char *description, const mcp::json &schema)
{
    mcp::json obj = mcp::json::object();
    obj["service"] = service;
    obj["name"] = name;
    obj["description"] = description;
    obj["inputSchema"] = schema;
    return obj;
}
} // namespace

TEST_CASE("resolve_endpoint composes base paths and honors absolute overrides")
{
    const QUrl merged =
        mcp_internal::resolve_endpoint(QStringLiteral("https://eva.test/api"), QStringLiteral("events"), false);
    CHECK(merged.toString() == QStringLiteral("https://eva.test/api/events"));

    const QUrl explicitPath =
        mcp_internal::resolve_endpoint(QStringLiteral("https://eva.test/gateway"), QStringLiteral("jobs"), true);
    CHECK(explicitPath.toString() == QStringLiteral("https://eva.test/gateway/jobs"));

    const QUrl absolute = mcp_internal::resolve_endpoint(QStringLiteral("https://eva.test/api"),
                                                         QStringLiteral("https://remote.other/tools"), false);
    CHECK(absolute.toString() == QStringLiteral("https://remote.other/tools"));

    const QUrl fallback = mcp_internal::resolve_endpoint(QStringLiteral("https://eva.test/base"), QString(), false);
    CHECK(fallback.toString() == QStringLiteral("https://eva.test/base"));
}

TEST_CASE("sanitize_schema removes noisy metadata recursively")
{
    mcp::json schema = mcp::json::object({
        {"$schema", "http://example"},
        {"type", "object"},
        {"properties", mcp::json::object(
                           {{"command", mcp::json::object({{"type", "string"}, {"additionalProperties", true}})}})},
    });

    mcp::json sanitized = sanitize_schema(schema);
    CHECK_FALSE(sanitized.contains("$schema"));
    CHECK(sanitized["properties"]["command"].contains("type"));
    CHECK_FALSE(sanitized["properties"]["command"].contains("additionalProperties"));
}

TEST_CASE("mcp_internal conversions keep structure round-trip")
{
    mcp::json payload = mcp::json::object({
        {"active", true},
        {"count", 7},
        {"data", mcp::json::array({1, "two", mcp::json::object({{"inner", "value"}})})},
    });

    const QJsonValue qvalue = mcp_internal::to_qjson_value(payload);
    REQUIRE(qvalue.isObject());
    const QJsonObject object = qvalue.toObject();
    CHECK(object.value(QStringLiteral("active")).toBool());
    CHECK(object.value(QStringLiteral("count")).toInt() == 7);
    const QJsonArray arr = object.value(QStringLiteral("data")).toArray();
    REQUIRE(arr.size() == 3);
    CHECK(arr.at(1).toString() == QStringLiteral("two"));

    const mcp::json roundtrip = mcp_internal::to_mcp_json(qvalue);
    CHECK(roundtrip["data"][2]["inner"] == "value");
}

TEST_CASE("eva::mcp::syncSelectedMcpTools updates registry and filters services")
{
    ToolsListGuard guard;
    mcp::json schema = mcp::json::object({{"type", "object"}, {"$schema", "http://noise"}});
    mcp::json tools = mcp::json::array(
        {toolEntry("alpha", "inspect", "Original alpha", schema), toolEntry("beta", "draw", "Beta tool", schema)});

    CHECK(eva::mcp::syncSelectedMcpTools(tools, nullptr));
    REQUIRE(MCP_TOOLS_INFO_LIST.size() == 2);

    tools[0]["description"] = "Updated alpha";
    CHECK(eva::mcp::syncSelectedMcpTools(tools, nullptr));
    const auto findTool = [](const QString &name) -> const TOOLS_INFO *
    {
        for (const auto &info : MCP_TOOLS_INFO_LIST)
        {
            if (info.name == name) return &info;
        }
        return nullptr;
    };
    const TOOLS_INFO *alpha = findTool(QStringLiteral("alpha@inspect"));
    REQUIRE(alpha != nullptr);
    CHECK(alpha->description == QStringLiteral("Updated alpha"));
    CHECK(alpha->arguments.contains(QStringLiteral("type")));
    CHECK_FALSE(alpha->arguments.contains(QStringLiteral("$schema")));

    const QSet<QString> onlyAlpha = {QStringLiteral("alpha")};
    CHECK(eva::mcp::syncSelectedMcpTools(tools, &onlyAlpha));
    REQUIRE(MCP_TOOLS_INFO_LIST.size() == 1);
    const TOOLS_INFO *filtered = findTool(QStringLiteral("alpha@inspect"));
    REQUIRE(filtered != nullptr);
}

TEST_CASE("eva::mcp::sanitizeToolsInfo strips schemas in nested arrays")
{
    const mcp::json tools = mcp::json::array(
        {mcp::json::object({{"service", "alpha"},
                            {"name", "outer"},
                            {"inputSchema",
                             mcp::json::array({mcp::json::object({{"$schema", "http://example"}, {"type", "string"}})})}})});

    const mcp::json sanitized = eva::mcp::sanitizeToolsInfo(tools);
    REQUIRE(sanitized.is_array());
    REQUIRE(sanitized.size() == 1);
    CHECK_FALSE(sanitized[0]["inputSchema"][0].contains("$schema"));
}
