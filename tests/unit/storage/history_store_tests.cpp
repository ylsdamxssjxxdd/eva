#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTime>
#include <QVector>

#include "storage/history_store.h"

namespace
{
SessionMeta makeMeta(const QString &id, const QDateTime &startedAt)
{
    SessionMeta meta;
    meta.id = id;
    meta.endpoint = QStringLiteral("local-endpoint");
    meta.model = QStringLiteral("eva-tester");
    meta.system = QStringLiteral("pilot");
    meta.n_ctx = 4096;
    meta.startedAt = startedAt;
    return meta;
}
} // namespace

TEST_CASE("HistoryStore begin/append/resume persists metadata and messages")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    HistoryStore store(dir.path());
    const SessionMeta meta = makeMeta(QStringLiteral("session-alpha"), QDateTime::currentDateTimeUtc());
    REQUIRE(store.begin(meta));

    store.appendMessage(QJsonObject{{"role", QStringLiteral("system")}, {"content", QStringLiteral("boot sequence")}});
    store.appendMessage(
        QJsonObject{{"role", QStringLiteral("user")}, {"content", QStringLiteral("Launch EVA immediately, commander.")}});
    store.updateSlotId(7);

    SessionMeta loadedMeta;
    QJsonArray loadedMessages;
    REQUIRE(store.loadSession(meta.id, loadedMeta, loadedMessages));
    CHECK(loadedMeta.slot_id == 7);
    CHECK(loadedMeta.title.startsWith(QStringLiteral("Launch EVA immediately")));
    CHECK(loadedMessages.size() == 2);
    CHECK(loadedMessages.at(0).toObject().value("role").toString() == QStringLiteral("system"));

    REQUIRE(store.resume(meta.id));
    store.appendMessage(QJsonObject{{"role", QStringLiteral("assistant")}, {"content", QStringLiteral("Acknowledged.")}});
    REQUIRE(store.loadSession(meta.id, loadedMeta, loadedMessages));
    CHECK(loadedMessages.size() == 3);
    CHECK(loadedMessages.last().toObject().value("role").toString() == QStringLiteral("assistant"));
    CHECK(loadedMeta.title.startsWith(QStringLiteral("Launch EVA immediately")));
}

TEST_CASE("HistoryStore rewrite, list, rename and purge sessions")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    HistoryStore store(dir.path());
    const QDateTime baseTime(QDate(2025, 1, 1), QTime(0, 0), Qt::UTC);
    SessionMeta metaOne = makeMeta(QStringLiteral("session-one"), baseTime);
    SessionMeta metaTwo = makeMeta(QStringLiteral("session-two"), baseTime.addSecs(90));

    REQUIRE(store.begin(metaOne));
    store.appendMessage(QJsonObject{{"role", QStringLiteral("user")}, {"content", QStringLiteral("Alpha line")}});

    REQUIRE(store.begin(metaTwo));
    store.appendMessage(QJsonObject{{"role", QStringLiteral("user")}, {"content", QStringLiteral("Bravo line")}});

    REQUIRE(store.resume(metaOne.id));
    QJsonArray rewritten;
    rewritten.append(QJsonObject{{"role", QStringLiteral("assistant")}, {"content", QStringLiteral("Rewritten payload")}});
    REQUIRE(store.rewriteAllMessages(rewritten));

    SessionMeta loadedMeta;
    QJsonArray loadedMessages;
    REQUIRE(store.loadSession(metaOne.id, loadedMeta, loadedMessages));
    CHECK(loadedMessages.size() == 1);
    CHECK(loadedMessages.first().toObject().value("content").toString() == QStringLiteral("Rewritten payload"));

    REQUIRE(store.renameSession(metaOne.id, QStringLiteral("Commander Log")));
    REQUIRE(store.loadSession(metaOne.id, loadedMeta, loadedMessages));
    CHECK(loadedMeta.title == QStringLiteral("Commander Log"));

    QVector<HistoryStore::ListItem> recent = store.listRecent(0);
    REQUIRE(recent.size() == 2);
    CHECK(recent.first().id == metaTwo.id);

    recent = store.listRecent(1);
    REQUIRE(recent.size() == 1);
    CHECK(recent.first().id == metaTwo.id);

    REQUIRE(store.deleteSession(metaTwo.id));
    recent = store.listRecent(0);
    REQUIRE(recent.size() == 1);
    CHECK(recent.first().id == metaOne.id);

    REQUIRE(store.purgeAll());
    recent = store.listRecent(0);
    CHECK(recent.isEmpty());
}
