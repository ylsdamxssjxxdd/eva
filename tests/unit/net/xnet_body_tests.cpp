#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringLiteral>

#include "xnet.h"

namespace
{
QJsonObject parseBody(const QByteArray &body)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    REQUIRE(err.error == QJsonParseError::NoError);
    return doc.object();
}

void appendMessage(QJsonArray &history, const QString &role, const QString &content)
{
    QJsonObject msg;
    msg.insert(QStringLiteral("role"), role);
    msg.insert(QStringLiteral("content"), content);
    history.append(msg);
}
} // namespace

TEST_CASE("createChatBody injects local sampling controls and tool translations")
{
    xNet net;
    net.apis.api_endpoint = QStringLiteral("http://127.0.0.1:9000");
    net.apis.api_model = QStringLiteral("eva-unit-chat");
    net.endpoint_data.date_prompt = QStringLiteral("Pilot sync prompt");
    net.endpoint_data.temp = 0.8F;
    net.endpoint_data.top_p = 0.55;
    net.endpoint_data.top_k = 33;
    net.endpoint_data.repeat = 1.25;
    net.endpoint_data.n_predict = 512;
    net.endpoint_data.reasoning_effort = QStringLiteral("AUTO");
    net.endpoint_data.stopwords = QStringList{QStringLiteral("END"), QStringLiteral("</stop>")};
    net.endpoint_data.id_slot = 7;

    QJsonArray history;
    appendMessage(history, QStringLiteral("user"), QStringLiteral("Launch status?"));
    appendMessage(history, QStringLiteral("tool"), QStringLiteral("all green"));
    net.endpoint_data.messagesArray = history;

    const QJsonObject payload = parseBody(net.createChatBody());
    CHECK(payload.value(QStringLiteral("cache_prompt")).toBool());
    CHECK(payload.value(QStringLiteral("model")).toString() == QStringLiteral("eva-unit-chat"));
    CHECK(payload.value(QStringLiteral("include_usage")).toBool());
    CHECK(payload.value(QStringLiteral("temperature")).toDouble() == doctest::Approx(1.6));
    CHECK(payload.value(QStringLiteral("top_p")).toDouble() == doctest::Approx(0.55));
    const QJsonArray stop = payload.value(QStringLiteral("stop")).toArray();
    REQUIRE(stop.size() == 2);
    CHECK(stop.at(0).toString() == QStringLiteral("END"));
    CHECK(stop.at(1).toString() == QStringLiteral("</stop>"));
    CHECK(payload.value(QStringLiteral("top_k")).toInt() == 33);
    CHECK(payload.value(QStringLiteral("repeat_penalty")).toDouble() == doctest::Approx(1.25));
    CHECK(payload.value(QStringLiteral("n_predict")).toInt() == 512);
    CHECK(payload.value(QStringLiteral("id_slot")).toInt() == 7);

    REQUIRE(payload.contains(QStringLiteral("reasoning")));
    CHECK(payload.value(QStringLiteral("reasoning")).toObject().value(QStringLiteral("effort")).toString() ==
          QStringLiteral("medium"));

    const QJsonArray messages = payload.value(QStringLiteral("messages")).toArray();
    REQUIRE(messages.size() == 3);
    const QJsonObject first = messages.at(0).toObject();
    CHECK(first.value(QStringLiteral("role")).toString() == QStringLiteral(DEFAULT_SYSTEM_NAME));
    CHECK(first.value(QStringLiteral("content")).toString() == QStringLiteral("Pilot sync prompt"));
    CHECK(messages.at(1).toObject().value(QStringLiteral("content")).toString() == QStringLiteral("Launch status?"));
    const QJsonObject convertedTool = messages.at(2).toObject();
    CHECK(convertedTool.value(QStringLiteral("role")).toString() == QStringLiteral("user"));
    CHECK(convertedTool.value(QStringLiteral("content")).toString() ==
          QStringLiteral(DEFAULT_OBSERVATION_NAME) + QStringLiteral("all green"));
}

TEST_CASE("createChatBody clamps sampling for remote endpoints and omits local extras")
{
    xNet net;
    net.apis.api_endpoint = QStringLiteral("https://eva.remote");
    net.apis.api_model = QStringLiteral("eva-link");
    net.apis.is_cache = false;
    net.endpoint_data.date_prompt = QStringLiteral("Remote sync");
    net.endpoint_data.temp = 1.5F;
    net.endpoint_data.top_p = 2.0;
    net.endpoint_data.top_k = 99;
    net.endpoint_data.repeat = 0.9;
    net.endpoint_data.n_predict = 150000;
    net.endpoint_data.stopwords = QStringList{QStringLiteral("END")};
    net.endpoint_data.reasoning_effort = QStringLiteral("off");
    net.endpoint_data.id_slot = 42;

    QJsonArray history;
    appendMessage(history, QStringLiteral("user"), QStringLiteral("Status check"));
    net.endpoint_data.messagesArray = history;

    const QJsonObject payload = parseBody(net.createChatBody());
    CHECK_FALSE(payload.contains(QStringLiteral("cache_prompt")));
    CHECK(payload.value(QStringLiteral("temperature")).toDouble() == doctest::Approx(2.0));
    CHECK(payload.value(QStringLiteral("top_p")).toDouble() == doctest::Approx(1.0));
    CHECK_FALSE(payload.contains(QStringLiteral("top_k")));
    CHECK_FALSE(payload.contains(QStringLiteral("repeat_penalty")));
    CHECK(payload.value(QStringLiteral("n_predict")).toInt() == 99999);
    CHECK_FALSE(payload.contains(QStringLiteral("id_slot")));
    CHECK_FALSE(payload.contains(QStringLiteral("reasoning")));
    const QJsonArray stop = payload.value(QStringLiteral("stop")).toArray();
    REQUIRE(stop.size() == 1);
    CHECK(stop.at(0).toString() == QStringLiteral("END"));
}

TEST_CASE("createCompleteBody honors local extras and reasoning effort")
{
    xNet net;
    net.apis.api_endpoint = QStringLiteral("http://10.0.0.5:8080");
    net.apis.api_model = QStringLiteral("eva-complete");
    net.apis.is_cache = true;
    net.endpoint_data.input_prompt = QStringLiteral("Complete this entry");
    net.endpoint_data.n_predict = 256;
    net.endpoint_data.temp = 0.3F;
    net.endpoint_data.top_p = 0.75;
    net.endpoint_data.top_k = 21;
    net.endpoint_data.repeat = 1.05;
    net.endpoint_data.stopwords = QStringList{QStringLiteral("###"), QStringLiteral("END")};
    net.endpoint_data.reasoning_effort = QStringLiteral("High");
    net.endpoint_data.id_slot = 12;

    const QJsonObject payload = parseBody(net.createCompleteBody());
    CHECK(payload.value(QStringLiteral("cache_prompt")).toBool());
    CHECK(payload.value(QStringLiteral("model")).toString() == QStringLiteral("eva-complete"));
    CHECK(payload.value(QStringLiteral("prompt")).toString() == QStringLiteral("Complete this entry"));
    CHECK(payload.value(QStringLiteral("stream")).toBool());
    CHECK(payload.value(QStringLiteral("include_usage")).toBool());
    CHECK(payload.value(QStringLiteral("temperature")).toDouble() == doctest::Approx(0.6));
    CHECK(payload.value(QStringLiteral("top_p")).toDouble() == doctest::Approx(0.75));
    const QJsonArray stop = payload.value(QStringLiteral("stop")).toArray();
    REQUIRE(stop.size() == 2);
    CHECK(stop.at(0).toString() == QStringLiteral("###"));
    CHECK(stop.at(1).toString() == QStringLiteral("END"));
    CHECK(payload.value(QStringLiteral("top_k")).toInt() == 21);
    CHECK(payload.value(QStringLiteral("repeat_penalty")).toDouble() == doctest::Approx(1.05));
    CHECK(payload.value(QStringLiteral("n_predict")).toInt() == 256);
    CHECK(payload.value(QStringLiteral("id_slot")).toInt() == 12);
    REQUIRE(payload.contains(QStringLiteral("reasoning")));
    CHECK(payload.value(QStringLiteral("reasoning")).toObject().value(QStringLiteral("effort")).toString() ==
          QStringLiteral("high"));
}
