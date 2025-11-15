#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QSignalSpy>
#include <QVariant>

#include "xnet.h"

class TestableNet : public xNet
{
  public:
    using xNet::processSsePayload;
};

TEST_CASE("processSsePayload streams reasoning content and closes think blocks")
{
    TestableNet net;
    QSignalSpy outputSpy(&net, &xNet::net2ui_output);
    QSignalSpy kvSpy(&net, &xNet::net2ui_kv_tokens);

    const QByteArray payload = R"({"choices":[{"delta":{"reasoning_content":"Alpha","content":"Beta"}}]})";
    net.processSsePayload(true, payload);

    REQUIRE(outputSpy.count() == 3);
    CHECK(outputSpy.at(0).at(0).toString() == QStringLiteral(DEFAULT_THINK_BEGIN));
    CHECK(outputSpy.at(1).at(0).toString() == QStringLiteral("Alpha"));
    CHECK(outputSpy.at(2).at(0).toString() == QStringLiteral(DEFAULT_THINK_END) + QStringLiteral("Beta"));

    REQUIRE(kvSpy.count() == 2);
    CHECK(kvSpy.at(0).at(0).toInt() == 1);
    CHECK(kvSpy.at(1).at(0).toInt() == 2);
}

TEST_CASE("processSsePayload derives prompt baselines and timings for completion payloads")
{
    TestableNet net;
    QSignalSpy outputSpy(&net, &xNet::net2ui_output);
    QSignalSpy kvSpy(&net, &xNet::net2ui_kv_tokens);
    QSignalSpy promptSpy(&net, &xNet::net2ui_prompt_baseline);
    QSignalSpy countersSpy(&net, &xNet::net2ui_turn_counters);

    const QByteArray payload = R"({
        "content":"Gamma",
        "usage":{"total_tokens":6},
        "timings":{"cache_n":1,"prompt_n":2,"predicted_n":4,"prompt_ms":50.0,"predicted_ms":40.0}
    })";
    net.processSsePayload(false, payload);

    REQUIRE(outputSpy.count() == 1);
    CHECK(outputSpy.at(0).at(0).toString() == QStringLiteral("Gamma"));

    REQUIRE(kvSpy.count() == 1);
    CHECK(kvSpy.at(0).at(0).toInt() == 1);

    REQUIRE(promptSpy.count() == 1);
    CHECK(promptSpy.at(0).at(0).toInt() == 5);

    REQUIRE(countersSpy.count() == 1);
    const QList<QVariant> totals = countersSpy.takeFirst();
    REQUIRE(totals.size() == 3);
    CHECK(totals.at(0).toInt() == 1);
    CHECK(totals.at(1).toInt() == 2);
    CHECK(totals.at(2).toInt() == 4);
}
