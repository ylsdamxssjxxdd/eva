#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "utils/perf_metrics.h"
#include "xconfig.h"

namespace
{
QString metricsFilePath(const QString &rootPath)
{
    return QDir(rootPath).filePath(QStringLiteral(EVA_TEMP_DIR_RELATIVE) + QStringLiteral("/metrics/events.jsonl"));
}
}

TEST_CASE("PerfMetrics writes event records to EVA_TEMP")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QJsonObject fields;
    fields.insert(QStringLiteral("case"), QStringLiteral("event"));
    PerfMetrics::recordEvent(tempDir.path(), QStringLiteral("test.event"), fields);

    const QString filePath = metricsFilePath(tempDir.path());
    QFile file(filePath);
    REQUIRE(file.exists());
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> lines = file.readAll().split('\n');
    file.close();

    REQUIRE(lines.size() >= 2);
    const QJsonDocument lastDoc = QJsonDocument::fromJson(lines.at(lines.size() - 2));
    REQUIRE(lastDoc.isObject());
    const QJsonObject payload = lastDoc.object();
    CHECK(payload.value(QStringLiteral("event")).toString() == QStringLiteral("test.event"));
    CHECK(payload.value(QStringLiteral("fields")).toObject().value(QStringLiteral("case")).toString() == QStringLiteral("event"));
}

TEST_CASE("PerfMetrics duration records include duration_ms")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    QJsonObject fields;
    fields.insert(QStringLiteral("phase"), QStringLiteral("finish"));
    PerfMetrics::recordDuration(tempDir.path(), QStringLiteral("test.duration"), 123, fields);

    const QString filePath = metricsFilePath(tempDir.path());
    QFile file(filePath);
    REQUIRE(file.exists());
    REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> lines = file.readAll().split('\n');
    file.close();

    REQUIRE(lines.size() >= 2);
    const QJsonDocument lastDoc = QJsonDocument::fromJson(lines.at(lines.size() - 2));
    REQUIRE(lastDoc.isObject());
    const QJsonObject payload = lastDoc.object();
    const QJsonObject payloadFields = payload.value(QStringLiteral("fields")).toObject();
    CHECK(payload.value(QStringLiteral("event")).toString() == QStringLiteral("test.duration"));
    CHECK(payloadFields.value(QStringLiteral("duration_ms")).toInt() == 123);
    CHECK(payloadFields.value(QStringLiteral("phase")).toString() == QStringLiteral("finish"));
}
