#include "utils/perf_metrics.h"

#include "xconfig.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>

namespace
{
QMutex &perfMetricsMutex()
{
    static QMutex mutex;
    return mutex;
}

void appendJsonLine(const QString &filePath, const QJsonObject &payload)
{
    QFile out(filePath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        return;
    }
    const QByteArray line = QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n';
    out.write(line);
    out.close();
}
} // namespace

QString PerfMetrics::metricsFilePath(const QString &applicationDirPath)
{
    const QString dirPath = QDir(applicationDirPath).filePath(QStringLiteral(EVA_TEMP_DIR_RELATIVE) + QStringLiteral("/metrics"));
    QDir dir;
    dir.mkpath(dirPath);
    return QDir(dirPath).filePath(QStringLiteral("events.jsonl"));
}

void PerfMetrics::recordEvent(const QString &applicationDirPath, const QString &eventName, const QJsonObject &fields)
{
    if (applicationDirPath.trimmed().isEmpty() || eventName.trimmed().isEmpty()) return;

    QJsonObject payload;
    payload.insert(QStringLiteral("ts_ms"), QDateTime::currentMSecsSinceEpoch());
    payload.insert(QStringLiteral("event"), eventName);
    payload.insert(QStringLiteral("fields"), fields);

    QMutexLocker locker(&perfMetricsMutex());
    appendJsonLine(metricsFilePath(applicationDirPath), payload);
}

void PerfMetrics::recordDuration(const QString &applicationDirPath,
                                 const QString &eventName,
                                 qint64 durationMs,
                                 const QJsonObject &fields)
{
    QJsonObject payloadFields = fields;
    payloadFields.insert(QStringLiteral("duration_ms"), durationMs);
    recordEvent(applicationDirPath, eventName, payloadFields);
}

