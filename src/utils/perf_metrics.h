#ifndef PERF_METRICS_H
#define PERF_METRICS_H

#include <QJsonObject>
#include <QString>

// 轻量性能与稳定性基线记录器：
// - 写入 EVA_TEMP/metrics/events.jsonl（每行一个 JSON 对象）
// - 用于后续做 P50/P95、失败率、恢复率统计
class PerfMetrics
{
  public:
    static void recordEvent(const QString &applicationDirPath,
                            const QString &eventName,
                            const QJsonObject &fields = QJsonObject());
    static void recordDuration(const QString &applicationDirPath,
                               const QString &eventName,
                               qint64 durationMs,
                               const QJsonObject &fields = QJsonObject());

  private:
    static QString metricsFilePath(const QString &applicationDirPath);
};

#endif // PERF_METRICS_H

