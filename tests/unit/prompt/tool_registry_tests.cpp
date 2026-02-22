#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "service/tools/tool_registry.h"

TEST_CASE("ToolRegistry exposes stable capability metadata")
{
    ToolRegistry::setLanguage(EVA_LANG_EN);

    const auto &defs = ToolRegistry::entries();
    REQUIRE(defs.size() >= 10);

    const QJsonObject manifest = ToolRegistry::capabilityManifest();
    CHECK(manifest.value(QStringLiteral("manifest_version")).toInt() == 1);
    CHECK(manifest.value(QStringLiteral("tool_count")).toInt() == defs.size());

    const QJsonArray tools = manifest.value(QStringLiteral("tools")).toArray();
    CHECK(tools.size() == defs.size());
}

TEST_CASE("ToolRegistry capabilityByName is case-insensitive and includes risk metadata")
{
    ToolRegistry::setLanguage(EVA_LANG_EN);

    const QJsonObject executeCapability = ToolRegistry::capabilityByName(QStringLiteral("EXECUTE_COMMAND"));
    REQUIRE_FALSE(executeCapability.isEmpty());
    CHECK(executeCapability.value(QStringLiteral("name")).toString() == QStringLiteral("execute_command"));
    CHECK(executeCapability.value(QStringLiteral("schema_version")).toInt() >= 1);
    CHECK(executeCapability.value(QStringLiteral("timeout_ms")).toInt() >= 120000);
    CHECK(executeCapability.value(QStringLiteral("high_risk")).toBool());
    CHECK_FALSE(executeCapability.value(QStringLiteral("description")).toString().isEmpty());

    const QJsonObject calculatorCapability = ToolRegistry::capabilityByName(QStringLiteral("calculator"));
    REQUIRE_FALSE(calculatorCapability.isEmpty());
    CHECK(calculatorCapability.value(QStringLiteral("timeout_ms")).toInt() <= executeCapability.value(QStringLiteral("timeout_ms")).toInt());
    CHECK_FALSE(calculatorCapability.value(QStringLiteral("high_risk")).toBool());

    const QJsonObject missingCapability = ToolRegistry::capabilityByName(QStringLiteral("unknown_tool"));
    CHECK(missingCapability.isEmpty());
}
