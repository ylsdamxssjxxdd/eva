#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QTimeZone>

#include "../xconfig.h"

// 轻量定时任务调度器：
// - 管理 jobs.json 持久化
// - 计算 next_run
// - 到期触发 jobDue 信号
class SchedulerService : public QObject
{
    Q_OBJECT
  public:
    explicit SchedulerService(QObject *parent = nullptr);

    void setBaseDir(const QString &baseDir);
    void setSettings(const SCHEDULER_SETTINGS &settings);

    void loadJobs();
    QJsonArray jobs() const;

    // 处理 tool_call 的 schedule_task 请求
    QJsonObject handleToolCall(const QJsonObject &args);

    // UI 手动操作（启用/禁用/删除/立即执行）
    QJsonObject handleUiAction(const QString &action, const QString &jobId);

  signals:
    void jobsUpdated(const QJsonArray &jobs);
    void jobDue(const QJsonObject &job, const QDateTime &fireTime);

  private slots:
    void onTimerTimeout();

  private:
    void ensureLoaded();
    void saveJobs();
    void scheduleNextTimer();
    QJsonObject findJobById(const QString &jobId, int *indexOut = nullptr) const;
    QString ensureJobId(const QString &preferred);
    QDateTime parseDateTime(const QString &text, const QString &tzId) const;
    QDateTime computeNextRun(const QJsonObject &job, const QDateTime &from, QString *error = nullptr) const;
    bool updateNextRun(QJsonObject &job, const QDateTime &from, QString *error = nullptr) const;
    QJsonObject applyAction(const QString &action, const QJsonObject &jobInput, QString *error);
    void emitJobsUpdated();
    void appendRunLog(const QString &jobId, const QJsonObject &record);
    bool parseCronField(const QString &field, int minValue, int maxValue, QSet<int> &out, bool allowSevenAsSunday) const;
    bool matchCron(const QDateTime &dt, const QJsonObject &schedule, QString *error) const;

  private:
    QString baseDir_;
    QString jobsPath_;
    QString runsDir_;
    QJsonArray jobs_;
    bool loaded_ = false;
    SCHEDULER_SETTINGS settings_;
    QTimer timer_;
};
