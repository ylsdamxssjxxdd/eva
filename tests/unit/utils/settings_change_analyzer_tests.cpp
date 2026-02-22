#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "utils/settings_change_analyzer.h"

TEST_CASE("settings analyzer reports no change when snapshots are equal")
{
    SETTINGS before;
    SETTINGS after = before;
    const SettingsChangeSummary summary = analyzeSettingsChanges(before,
                                                                 after,
                                                                 QStringLiteral("8080"),
                                                                 QStringLiteral("8080"),
                                                                 QStringLiteral("auto"),
                                                                 QStringLiteral("auto"),
                                                                 false,
                                                                 0);
    CHECK_FALSE(summary.hasAnyChange);
    CHECK_FALSE(summary.requiresBackendRestart);
    CHECK_FALSE(summary.requiresSessionReset);
    CHECK(summary.restartItems.isEmpty());
    CHECK(summary.resetItems.isEmpty());
}

TEST_CASE("settings analyzer separates session reset changes from restart changes")
{
    SETTINGS before;
    SETTINGS after = before;
    after.temp += 0.05;
    after.hid_top_p = 0.72;

    const SettingsChangeSummary summary = analyzeSettingsChanges(before,
                                                                 after,
                                                                 QStringLiteral("8080"),
                                                                 QStringLiteral("8080"),
                                                                 QStringLiteral("auto"),
                                                                 QStringLiteral("auto"),
                                                                 false,
                                                                 0);
    CHECK(summary.hasAnyChange);
    CHECK_FALSE(summary.requiresBackendRestart);
    CHECK(summary.requiresSessionReset);
    CHECK(summary.restartItems.isEmpty());
    CHECK(summary.resetItems.contains(QStringLiteral("temp")));
    CHECK(summary.resetItems.contains(QStringLiteral("top_p")));
}

TEST_CASE("settings analyzer marks backend restart fields and ngl equivalence")
{
    SETTINGS before;
    SETTINGS after = before;
    before.ngl = 999;
    after.ngl = 40;
    after.nctx = before.nctx + 256;

    const SettingsChangeSummary summary = analyzeSettingsChanges(before,
                                                                 after,
                                                                 QStringLiteral("8080"),
                                                                 QStringLiteral("8080"),
                                                                 QStringLiteral("auto"),
                                                                 QStringLiteral("auto"),
                                                                 false,
                                                                 40);
    CHECK(summary.hasAnyChange);
    CHECK(summary.requiresBackendRestart);
    CHECK(summary.requiresSessionReset);
    CHECK(summary.restartItems.contains(QStringLiteral("nctx")));
    CHECK_FALSE(summary.restartItems.contains(QStringLiteral("ngl")));
}

TEST_CASE("settings analyzer treats port/device/override as restart changes")
{
    SETTINGS before;
    SETTINGS after = before;

    const SettingsChangeSummary summary = analyzeSettingsChanges(before,
                                                                 after,
                                                                 QStringLiteral("8080"),
                                                                 QStringLiteral("9090"),
                                                                 QStringLiteral("auto"),
                                                                 QStringLiteral("cuda"),
                                                                 true,
                                                                 0);
    CHECK(summary.hasAnyChange);
    CHECK(summary.requiresBackendRestart);
    CHECK(summary.requiresSessionReset);
    CHECK(summary.restartItems.contains(QStringLiteral("port")));
    CHECK(summary.restartItems.contains(QStringLiteral("device")));
    CHECK(summary.restartItems.contains(QStringLiteral("backend_override")));
}

TEST_CASE("compact change items keeps output short and stable")
{
    const QStringList items = {
        QStringLiteral("model"),
        QStringLiteral("nctx"),
        QStringLiteral("port"),
        QStringLiteral("device"),
        QStringLiteral("model"), // duplicate
    };

    CHECK(compactChangeItems(QStringList(), 3) == QStringLiteral("none"));
    CHECK(compactChangeItems(items, 2) == QStringLiteral("model, nctx +2"));
    CHECK(compactChangeItems(items, 8) == QStringLiteral("model, nctx, port, device"));
}
