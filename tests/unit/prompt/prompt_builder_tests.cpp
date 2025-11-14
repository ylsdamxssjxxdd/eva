#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QStringLiteral>

#include "prompt_builder.h"

namespace
{
const QString kSystemRole = QStringLiteral("system");
const QString kUserRole = QStringLiteral("user");
const QString kAssistantRole = QStringLiteral("assistant");
const QString kToolRole = QStringLiteral("tool");

QJsonArray runBuilder(const QJsonArray &history, const QString &systemPrompt)
{
    return promptx::buildOaiChatMessages(history, systemPrompt, kSystemRole, kUserRole, kAssistantRole, kToolRole);
}

} // namespace

TEST_CASE("prompt builder injects system prompt when history is empty")
{
    const QJsonArray out = runBuilder(QJsonArray{}, QStringLiteral("awake"));
    REQUIRE(out.size() == 1);
    const QJsonObject first = out.first().toObject();
    CHECK(first.value("role").toString() == kSystemRole);
    CHECK(first.value("content").toString() == QStringLiteral("awake"));
}

TEST_CASE("prompt builder prepends system prompt when missing")
{
    QJsonArray history;
    QJsonObject user;
    user.insert("role", kUserRole);
    user.insert("content", QStringLiteral("hi"));
    history.append(user);

    const auto out = runBuilder(history, QStringLiteral("sys"));
    REQUIRE(out.size() == 2);
    CHECK(out.at(0).toObject().value("role").toString() == kSystemRole);
    CHECK(out.at(1).toObject().value("role").toString() == kUserRole);
}

TEST_CASE("system prompt remains untouched when already first")
{
    QJsonArray history;
    QJsonObject system;
    system.insert("role", kSystemRole);
    system.insert("content", QStringLiteral("legacy-sys"));
    history.append(system);

    QJsonObject user;
    user.insert("role", kUserRole);
    user.insert("content", QStringLiteral("hi"));
    history.append(user);

    const auto out = runBuilder(history, QStringLiteral("ignored"));
    REQUIRE(out.size() == 2);
    CHECK(out.at(0).toObject().value("content").toString() == QStringLiteral("legacy-sys"));
    CHECK(out.at(1).toObject().value("role").toString() == kUserRole);
}

TEST_CASE("assistant reasoning is split away from content")
{
    QJsonArray history;
    QJsonObject asst;
    asst.insert("role", kAssistantRole);
    asst.insert("content", QStringLiteral("<think>calc</think> final answer"));
    history.append(asst);

    const auto out = runBuilder(history, QStringLiteral("sys"));
    REQUIRE(out.size() == 2);
    const QJsonObject processed = out.at(1).toObject();
    CHECK(processed.value("content").toString() == QStringLiteral("final answer"));
    CHECK(processed.value("reasoning_content").toString() == QStringLiteral("calc"));
    CHECK_FALSE(processed.contains("thinking"));
}

TEST_CASE("existing reasoning_content is preserved")
{
    QJsonArray history;
    QJsonObject asst;
    asst.insert("role", kAssistantRole);
    asst.insert("content", QStringLiteral("<think>calc</think> final"));
    asst.insert("reasoning_content", QStringLiteral("keep-me"));
    history.append(asst);

    const auto out = runBuilder(history, QStringLiteral("sys"));
    REQUIRE(out.size() == 2);
    const auto processed = out.at(1).toObject();
    CHECK(processed.value("content").toString() == QStringLiteral("final"));
    CHECK(processed.value("reasoning_content").toString() == QStringLiteral("keep-me"));
}

TEST_CASE("thinking field becomes reasoning fallback when inline tags absent")
{
    QJsonArray history;
    QJsonObject asst;
    asst.insert("role", kAssistantRole);
    asst.insert("content", QStringLiteral("final only"));
    asst.insert("thinking", QStringLiteral("pre-plan"));
    history.append(asst);

    const auto out = runBuilder(history, QStringLiteral("sys"));
    REQUIRE(out.size() == 2);
    const auto processed = out.at(1).toObject();
    CHECK(processed.value("content").toString() == QStringLiteral("final only"));
    CHECK(processed.value("reasoning_content").toString() == QStringLiteral("pre-plan"));
}

TEST_CASE("multimodal payload keeps supported parts and converts audio_url")
{
    QJsonArray contentArray;

    QJsonObject text;
    text.insert("type", QStringLiteral("text"));
    text.insert("text", QStringLiteral("hello"));
    contentArray.append(text);

    QJsonObject image;
    image.insert("type", QStringLiteral("image_url"));
    QJsonObject imageUrl;
    imageUrl.insert("url", QStringLiteral("http://example/image.png"));
    image.insert("image_url", imageUrl);
    contentArray.append(image);

    QJsonObject audio;
    audio.insert("type", QStringLiteral("audio_url"));
    QJsonObject audioUrl;
    audioUrl.insert("url", QStringLiteral("data:audio/wav;base64,QUFB"));
    audio.insert("audio_url", audioUrl);
    contentArray.append(audio);

    QJsonObject user;
    user.insert("role", kUserRole);
    user.insert("content", contentArray);

    QJsonArray history;
    history.append(user);

    const auto out = runBuilder(history, QStringLiteral("sys"));
    REQUIRE(out.size() == 2); // system + user
    const QJsonArray processedContent = out.at(1).toObject().value("content").toArray();
    REQUIRE(processedContent.size() == 3);
    CHECK(processedContent.at(0).toObject().value("text").toString() == QStringLiteral("hello"));
    CHECK(processedContent.at(1).toObject().value("image_url").toObject().value("url").toString() ==
          QStringLiteral("http://example/image.png"));

    const QJsonObject audioPart = processedContent.at(2).toObject();
    CHECK(audioPart.value("type").toString() == QStringLiteral("input_audio"));
    const QJsonObject ia = audioPart.value("input_audio").toObject();
    CHECK(ia.value("format").toString() == QStringLiteral("wav"));
    CHECK(ia.value("data").toString() == QStringLiteral("QUFB"));
}

TEST_CASE("string entries in multimodal payload are normalized and unsupported parts drop")
{
    QJsonArray contentArray;
    contentArray.append(QStringLiteral("loose text"));

    QJsonObject audio;
    audio.insert("type", QStringLiteral("audio_url"));
    QJsonObject audioUrl;
    audioUrl.insert("url", QStringLiteral("data:audio/ogg;base64,AAAA"));
    audio.insert("audio_url", audioUrl);
    contentArray.append(audio);

    QJsonObject unsupported;
    unsupported.insert("type", QStringLiteral("matrix"));
    unsupported.insert("data", QStringLiteral("skip"));
    contentArray.append(unsupported);

    QJsonObject explicitText;
    explicitText.insert("type", QStringLiteral("text"));
    explicitText.insert("text", QStringLiteral("kept"));
    contentArray.append(explicitText);

    QJsonObject user;
    user.insert("role", kUserRole);
    user.insert("content", contentArray);

    QJsonArray history;
    history.append(user);

    const auto out = runBuilder(history, QStringLiteral("sys"));
    REQUIRE(out.size() == 2);
    const QJsonArray processedContent = out.at(1).toObject().value("content").toArray();
    REQUIRE(processedContent.size() == 3);

    CHECK(processedContent.at(0).toObject().value("text").toString() == QStringLiteral("loose text"));

    const QJsonObject audioPart = processedContent.at(1).toObject();
    CHECK(audioPart.value("type").toString() == QStringLiteral("input_audio"));
    const QJsonObject ia = audioPart.value("input_audio").toObject();
    CHECK(ia.value("format").toString() == QStringLiteral("mp3")); // ogg falls back to mp3 container
    CHECK(ia.value("data").toString() == QStringLiteral("AAAA"));

    CHECK(processedContent.at(2).toObject().value("text").toString() == QStringLiteral("kept"));
}

TEST_CASE("tool role is preserved while unknown roles are dropped")
{
    QJsonArray history;

    QJsonObject user;
    user.insert("role", kUserRole);
    user.insert("content", QStringLiteral("hi"));
    history.append(user);

    QJsonObject tool;
    tool.insert("role", kToolRole);
    tool.insert("content", QStringLiteral("{\"ok\":true}"));
    history.append(tool);

    QJsonObject ignored;
    ignored.insert("role", QStringLiteral("debug"));
    ignored.insert("content", QStringLiteral("ignore me"));
    history.append(ignored);

    const auto out = runBuilder(history, QStringLiteral("sys"));
    REQUIRE(out.size() == 3); // system + user + tool
    CHECK(out.at(2).toObject().value("role").toString() == kToolRole);
}

TEST_CASE("think role entries are skipped entirely")
{
    QJsonArray history;

    QJsonObject system;
    system.insert("role", kSystemRole);
    system.insert("content", QStringLiteral("sys"));
    history.append(system);

    QJsonObject think;
    think.insert("role", QStringLiteral("think"));
    think.insert("content", QStringLiteral("ponder"));
    history.append(think);

    QJsonObject user;
    user.insert("role", kUserRole);
    user.insert("content", QStringLiteral("hi"));
    history.append(user);

    const auto out = runBuilder(history, QStringLiteral("ignored"));
    REQUIRE(out.size() == 2);
    for (const auto &item : out)
    {
        CHECK(item.toObject().value("role").toString() != QStringLiteral("think"));
    }
}
