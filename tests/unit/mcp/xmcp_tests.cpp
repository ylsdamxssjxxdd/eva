#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <map>

#include "xmcp.h"
#include "xmcp_internal.h"

namespace
{
QCoreApplication *ensureQtApp()
{
    static int argc = 0;
    static char **argv = nullptr;
    static QCoreApplication app(argc, argv);
    static bool registered = []()
    {
        qRegisterMetaType<MCP_CONNECT_STATE>("MCP_CONNECT_STATE");
        return true;
    }();
    Q_UNUSED(registered);
    return &app;
}

void resetGlobalTools()
{
    MCP_TOOLS_INFO_LIST.clear();
    MCP_TOOLS_INFO_ALL = mcp::json::array();
}

class FakeMcpToolController : public IMcpToolController
{
  public:
    void setNotificationHandler(McpToolManager::NotificationHandler handler) override { handler_ = std::move(handler); }
    void clear() override
    {
        cleared = true;
        services_.clear();
    }
    std::string addServer(const std::string &name, const mcp::json &) override
    {
        services_.insert(QString::fromStdString(name));
        if (handler_) handler_(QString::fromStdString(name), QStringLiteral("info"), QStringLiteral("connected"));
        const auto it = addServerResults.find(name);
        return (it != addServerResults.end()) ? it->second : std::string();
    }
    mcp::json getAllToolsInfo() const override { return currentTools; }
    mcp::json callTool(const std::string &serviceName, const std::string &toolName, const mcp::json &params) override
    {
        lastCallService = QString::fromStdString(serviceName);
        lastCallTool = QString::fromStdString(toolName);
        lastCallParams = params;
        if (callResult.is_null()) return mcp::json::object({{"ok", true}});
        return callResult;
    }
    bool refreshAllTools(const QSet<QString> *serviceFilter) override
    {
        lastRefreshFilter = serviceFilter ? *serviceFilter : QSet<QString>();
        refreshCount++;
        return refreshReturn;
    }
    size_t getServiceCount() const override
    {
        if (serviceCountOverride >= 0) return static_cast<size_t>(serviceCountOverride);
        return static_cast<size_t>(services_.size());
    }

    std::map<std::string, std::string> addServerResults;
    mcp::json currentTools = mcp::json::array();
    mcp::json callResult;
    QString lastCallService;
    QString lastCallTool;
    mcp::json lastCallParams;
    bool cleared = false;
    int refreshCount = 0;
    bool refreshReturn = false;
    QSet<QString> lastRefreshFilter;
    int serviceCountOverride = -1;

  private:
    McpToolManager::NotificationHandler handler_;
    QSet<QString> services_;
};

xMcpOptions testOptions()
{
    xMcpOptions opts;
    opts.idleThresholdMs = 5;
    opts.autoRefreshIntervalMs = 5;
    opts.enableAutoRefreshTimer = false;
    return opts;
}

mcp::json makeTool(const std::string &service, const std::string &name, const std::string &description)
{
    mcp::json schema = mcp::json::object({{"type", "object"}, {"properties", mcp::json::object({{"q", {{"type", "string"}}}})}});
    return mcp::json::object({{"service", service}, {"name", name}, {"description", description}, {"inputSchema", schema}});
}

class TestableMcp : public xMcp
{
  public:
    using xMcp::xMcp;
    using xMcp::maybeAutoRefreshTools;
};
} // namespace

TEST_CASE("xMcp addService syncs tool inventory and status signals")
{
    resetGlobalTools();
    auto fake = std::make_unique<FakeMcpToolController>();
    auto *fakePtr = fake.get();
    fakePtr->currentTools = mcp::json::array({makeTool("alpha", "scan", "desc")});
    fakePtr->addServerResults["beta"] = "offline";

    ensureQtApp();
    TestableMcp mcp(nullptr, std::move(fake), testOptions());
    QSignalSpy singleSpy(&mcp, &xMcp::addService_single_over);
    QSignalSpy finalSpy(&mcp, &xMcp::addService_over);

    const QString payload = QStringLiteral(R"({"mcpServers":{
        "alpha":{"type":"sse","baseUrl":"https://alpha"},
        "beta":{"type":"sse","baseUrl":"https://beta"}
    }})");
    mcp.addService(payload);

    REQUIRE(singleSpy.count() == 2);
    CHECK(singleSpy.at(0).at(0).toString() == QStringLiteral("alpha"));
    CHECK(singleSpy.at(0).at(1).toInt() == MCP_CONNECT_LINK);
    CHECK(singleSpy.at(1).at(0).toString() == QStringLiteral("beta"));
    CHECK(singleSpy.at(1).at(1).toInt() == MCP_CONNECT_MISS);

    REQUIRE(finalSpy.count() == 1);
    CHECK(finalSpy.at(0).at(0).toInt() == MCP_CONNECT_WIP);
    CHECK(MCP_TOOLS_INFO_ALL.is_array());
    REQUIRE(MCP_TOOLS_INFO_LIST.size() == 1);
    CHECK(MCP_TOOLS_INFO_LIST.front().name == QStringLiteral("alpha@scan"));
    CHECK(fakePtr->cleared);
}

TEST_CASE("xMcp callTool validates inputs and forwards results")
{
    resetGlobalTools();
    auto fake = std::make_unique<FakeMcpToolController>();
    auto *fakePtr = fake.get();
    fakePtr->callResult = mcp::json::object({{"value", 42}});
    ensureQtApp();
    TestableMcp mcp(nullptr, std::move(fake), testOptions());
    QSignalSpy toolSpy(&mcp, &xMcp::callTool_over);

    mcp.callTool(1, QStringLiteral("invalid"), QString());
    REQUIRE(toolSpy.count() == 1);
    CHECK(toolSpy.takeFirst().at(1).toString().isEmpty());

    mcp.callTool(2, QStringLiteral("svc@tool"), QStringLiteral("{bad json"));
    REQUIRE(toolSpy.count() == 1);
    CHECK(toolSpy.takeFirst().at(1).toString().contains(QStringLiteral("JSON parse fail")));

    mcp.callTool(3, QStringLiteral("alpha@nav"), QStringLiteral("{\"x\":1}"));
    REQUIRE(toolSpy.count() == 1);
    CHECK(toolSpy.takeFirst().at(1).toString().contains(QStringLiteral("\"value\"")));
    CHECK(fakePtr->lastCallService == QStringLiteral("alpha"));
    CHECK(fakePtr->lastCallTool == QStringLiteral("nav"));
    CHECK(fakePtr->lastCallParams.contains("x"));
    CHECK(fakePtr->lastCallParams["x"] == 1);
}

TEST_CASE("xMcp callList refreshes caches and supports filtering")
{
    resetGlobalTools();
    auto fake = std::make_unique<FakeMcpToolController>();
    auto *fakePtr = fake.get();
    fakePtr->currentTools = mcp::json::array(
        {makeTool("alpha", "scan", "desc"), makeTool("beta", "search", "desc2")});
    ensureQtApp();
    TestableMcp mcp(nullptr, std::move(fake), testOptions());
    QSignalSpy listSpy(&mcp, &xMcp::callList_over);
    QSignalSpy toolsSpy(&mcp, &xMcp::toolsRefreshed);

    mcp.callList(77);
    REQUIRE(listSpy.count() == 1);
    REQUIRE(toolsSpy.count() == 1);
    CHECK(MCP_TOOLS_INFO_LIST.size() == 2);

    mcp.setEnabledServices(QStringList() << QStringLiteral("beta"));
    CHECK(MCP_TOOLS_INFO_LIST.size() == 1);
    CHECK(MCP_TOOLS_INFO_LIST.front().name == QStringLiteral("beta@search"));
}

TEST_CASE("xMcp refreshTools skips when service filter blocks all services")
{
    resetGlobalTools();
    auto fake = std::make_unique<FakeMcpToolController>();
    auto *fakePtr = fake.get();
    ensureQtApp();
    TestableMcp mcp(nullptr, std::move(fake), testOptions());
    QSignalSpy messageSpy(&mcp, &xMcp::mcp_message);

    mcp.setEnabledServices(QStringList()); // activate filter but empty
    mcp.refreshTools();
    CHECK(fakePtr->refreshCount == 0);
    REQUIRE(messageSpy.count() == 1);
    CHECK(messageSpy.at(0).at(0).toString().contains(QStringLiteral("no enabled MCP services")));
}

TEST_CASE("xMcp disconnectAll clears global tool caches")
{
    resetGlobalTools();
    auto fake = std::make_unique<FakeMcpToolController>();
    auto *fakePtr = fake.get();
    fakePtr->currentTools = mcp::json::array({makeTool("alpha", "scan", "desc")});
    ensureQtApp();
    TestableMcp mcp(nullptr, std::move(fake), testOptions());
    mcp.callList(1);
    REQUIRE(!MCP_TOOLS_INFO_LIST.empty());

    QSignalSpy toolsSpy(&mcp, &xMcp::toolsRefreshed);
    mcp.disconnectAll();
    CHECK(toolsSpy.count() == 1);
    CHECK(MCP_TOOLS_INFO_LIST.empty());
    CHECK(MCP_TOOLS_INFO_ALL.empty());
    CHECK(fakePtr->cleared);
}

TEST_CASE("xMcp maybeAutoRefreshTools enforces idle and interval guards")
{
    resetGlobalTools();
    auto fake = std::make_unique<FakeMcpToolController>();
    auto *fakePtr = fake.get();
    fakePtr->serviceCountOverride = 1;
    fakePtr->refreshReturn = true;
    ensureQtApp();
    TestableMcp mcp(nullptr, std::move(fake), testOptions());

    QTest::qSleep(10);
    mcp.maybeAutoRefreshTools();
    CHECK(fakePtr->refreshCount == 1);

    mcp.maybeAutoRefreshTools();
    CHECK(fakePtr->refreshCount == 1);

    QTest::qSleep(10);
    mcp.maybeAutoRefreshTools();
    CHECK(fakePtr->refreshCount == 2);
}
