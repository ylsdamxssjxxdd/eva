#include "scheduler_service.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSet>
#include <limits>

namespace
{
QStringList splitWhitespace(const QString &text)
{
    QStringList parts;
    QString current;
    for (const QChar ch : text)
    {
        if (ch.isSpace())
        {
            if (!current.isEmpty())
            {
                parts << current;
                current.clear();
            }
        }
        else
        {
            current.append(ch);
        }
    }
    if (!current.isEmpty()) parts << current;
    return parts;
}

qint64 toMs(const QJsonObject &obj, const char *key, qint64 fallback = 0)
{
    if (!obj.contains(QLatin1String(key))) return fallback;
    const QJsonValue v = obj.value(QLatin1String(key));
    if (v.isDouble()) return static_cast<qint64>(v.toDouble());
    return fallback;
}

QString toStringSafe(const QJsonObject &obj, const char *key)
{
    if (!obj.contains(QLatin1String(key))) return QString();
    return obj.value(QLatin1String(key)).toString();
}
} // namespace

SchedulerService::SchedulerService(QObject *parent)
    : QObject(parent)
{
    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, &SchedulerService::onTimerTimeout);
}

void SchedulerService::setBaseDir(const QString &baseDir)
{
    baseDir_ = QDir::cleanPath(baseDir);
    jobsPath_ = QDir(baseDir_).filePath(QStringLiteral(EVA_TEMP_CRON_JOBS_FILE_RELATIVE));
    runsDir_ = QDir(baseDir_).filePath(QStringLiteral(EVA_TEMP_CRON_RUNS_DIR_RELATIVE));
}

void SchedulerService::setSettings(const SCHEDULER_SETTINGS &settings)
{
    settings_ = settings;
    if (loaded_) scheduleNextTimer();
}

void SchedulerService::ensureLoaded()
{
    if (!loaded_) loadJobs();
}

void SchedulerService::loadJobs()
{
    if (baseDir_.isEmpty()) return;
    QDir().mkpath(QDir(baseDir_).filePath(QStringLiteral(EVA_TEMP_CRON_DIR_RELATIVE)));
    QDir().mkpath(runsDir_);
    jobs_ = QJsonArray();

    QFile file(jobsPath_);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QByteArray data = file.readAll();
        file.close();
        if (!data.isEmpty())
        {
            QJsonParseError err{};
            const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
            if (err.error == QJsonParseError::NoError)
            {
                if (doc.isObject())
                {
                    const QJsonObject root = doc.object();
                    const QJsonValue jobsVal = root.value(QStringLiteral("jobs"));
                    if (jobsVal.isArray()) jobs_ = jobsVal.toArray();
                }
                else if (doc.isArray())
                {
                    jobs_ = doc.array();
                }
            }
        }
    }

    // 兜底校验：补 job_id / next_run
    for (int i = 0; i < jobs_.size(); ++i)
    {
        QJsonObject job = jobs_.at(i).toObject();
        const QString id = toStringSafe(job, "job_id");
        if (id.isEmpty())
        {
            job.insert(QStringLiteral("job_id"), ensureJobId(QString()));
        }
        if (!job.contains(QStringLiteral("enabled")))
        {
            job.insert(QStringLiteral("enabled"), true);
        }
        if (toMs(job, "next_run_ms", 0) <= 0 && job.value(QStringLiteral("enabled")).toBool())
        {
            QString error;
            updateNextRun(job, QDateTime::currentDateTime(), &error);
        }
        jobs_[i] = job;
    }

    loaded_ = true;
    emitJobsUpdated();
    scheduleNextTimer();
}

QJsonArray SchedulerService::jobs() const
{
    return jobs_;
}

QString SchedulerService::ensureJobId(const QString &preferred)
{
    auto exists = [&](const QString &id) -> bool {
        for (const auto &val : jobs_)
        {
            const QJsonObject obj = val.toObject();
            if (obj.value(QStringLiteral("job_id")).toString() == id) return true;
        }
        return false;
    };
    if (!preferred.trimmed().isEmpty() && !exists(preferred.trimmed()))
    {
        return preferred.trimmed();
    }
    const qint64 stamp = QDateTime::currentMSecsSinceEpoch();
    for (int i = 0; i < 50; ++i)
    {
        const QString candidate =
            QStringLiteral("%1-%2").arg(stamp).arg(QRandomGenerator::global()->bounded(1000, 9999));
        if (!exists(candidate)) return candidate;
    }
    return QStringLiteral("%1").arg(stamp);
}

QDateTime SchedulerService::parseDateTime(const QString &text, const QString &tzId) const
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return QDateTime();
    QDateTime dt = QDateTime::fromString(trimmed, Qt::ISODate);
    if (!dt.isValid())
    {
        dt = QDateTime::fromString(trimmed, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    QTimeZone tz = QTimeZone::systemTimeZone();
    if (!tzId.trimmed().isEmpty())
    {
        const QTimeZone requested(tzId.toUtf8());
        if (requested.isValid()) tz = requested;
    }
    if (dt.isValid())
    {
        if (dt.timeSpec() == Qt::LocalTime)
        {
            dt = QDateTime(dt.date(), dt.time(), tz);
        }
        else if (dt.timeSpec() == Qt::OffsetFromUTC || dt.timeSpec() == Qt::UTC)
        {
            dt = dt.toTimeZone(tz);
        }
    }
    return dt;
}

bool SchedulerService::parseCronField(const QString &field, int minValue, int maxValue, QSet<int> &out, bool allowSevenAsSunday) const
{
    out.clear();
    const QString trimmed = field.trimmed();
    if (trimmed.isEmpty()) return false;
    if (trimmed == QStringLiteral("*"))
    {
        for (int v = minValue; v <= maxValue; ++v) out.insert(v);
        return true;
    }

    const QStringList tokens = trimmed.split(',', Qt::SkipEmptyParts);
    if (tokens.isEmpty()) return false;

    auto clamp = [&](int v) -> int { return std::min(maxValue, std::max(minValue, v)); };
    auto addRange = [&](int start, int end, int step) {
        if (step <= 0) step = 1;
        int s = clamp(start);
        int e = clamp(end);
        if (s > e) std::swap(s, e);
        for (int v = s; v <= e; v += step) out.insert(v);
    };

    for (const QString &tokenRaw : tokens)
    {
        QString token = tokenRaw.trimmed();
        if (token.isEmpty()) continue;
        int step = 1;
        QString base = token;
        if (token.contains('/'))
        {
            const QStringList pair = token.split('/');
            if (pair.size() != 2) return false;
            base = pair.at(0).trimmed();
            bool ok = false;
            step = pair.at(1).trimmed().toInt(&ok);
            if (!ok || step <= 0) return false;
        }
        if (base.isEmpty() || base == QStringLiteral("*"))
        {
            addRange(minValue, maxValue, step);
            continue;
        }
        if (base.contains('-'))
        {
            const QStringList range = base.split('-');
            if (range.size() != 2) return false;
            bool ok1 = false, ok2 = false;
            int start = range.at(0).trimmed().toInt(&ok1);
            int end = range.at(1).trimmed().toInt(&ok2);
            if (!ok1 || !ok2) return false;
            if (allowSevenAsSunday)
            {
                if (start == 7) start = 0;
                if (end == 7) end = 0;
            }
            addRange(start, end, step);
            continue;
        }
        bool ok = false;
        int value = base.toInt(&ok);
        if (!ok) return false;
        if (allowSevenAsSunday && value == 7) value = 0;
        out.insert(clamp(value));
    }
    return !out.isEmpty();
}

bool SchedulerService::matchCron(const QDateTime &dt, const QJsonObject &schedule, QString *error) const
{
    const QString cronExpr = schedule.value(QStringLiteral("cron")).toString().trimmed();
    if (cronExpr.isEmpty())
    {
        if (error) *error = QStringLiteral("cron expression missing");
        return false;
    }
    const QStringList fields = splitWhitespace(cronExpr);
    if (fields.size() < 5)
    {
        if (error) *error = QStringLiteral("cron expression must have 5 fields");
        return false;
    }

    QSet<int> minutes;
    QSet<int> hours;
    QSet<int> days;
    QSet<int> months;
    QSet<int> weekdays;
    if (!parseCronField(fields.at(0), 0, 59, minutes, false)) { if (error) *error = QStringLiteral("cron minute parse failed"); return false; }
    if (!parseCronField(fields.at(1), 0, 23, hours, false)) { if (error) *error = QStringLiteral("cron hour parse failed"); return false; }
    if (!parseCronField(fields.at(2), 1, 31, days, false)) { if (error) *error = QStringLiteral("cron day parse failed"); return false; }
    if (!parseCronField(fields.at(3), 1, 12, months, false)) { if (error) *error = QStringLiteral("cron month parse failed"); return false; }
    if (!parseCronField(fields.at(4), 0, 6, weekdays, true)) { if (error) *error = QStringLiteral("cron weekday parse failed"); return false; }

    const QDate date = dt.date();
    const int minute = dt.time().minute();
    const int hour = dt.time().hour();
    const int day = date.day();
    const int month = date.month();
    const int weekday = date.dayOfWeek() % 7; // Qt: 1=Mon..7=Sun -> 0=Sun

    return minutes.contains(minute) && hours.contains(hour) && days.contains(day) && months.contains(month) && weekdays.contains(weekday);
}

QDateTime SchedulerService::computeNextRun(const QJsonObject &job, const QDateTime &from, QString *error) const
{
    const QJsonObject schedule = job.value(QStringLiteral("schedule")).toObject();
    const QString kind = schedule.value(QStringLiteral("kind")).toString().trimmed().toLower();
    if (kind.isEmpty())
    {
        if (error) *error = QStringLiteral("schedule.kind missing");
        return QDateTime();
    }
    const QString tzId = schedule.value(QStringLiteral("tz")).toString().trimmed();
    QTimeZone tz = QTimeZone::systemTimeZone();
    if (!tzId.isEmpty())
    {
        const QTimeZone requested(tzId.toUtf8());
        if (requested.isValid()) tz = requested;
    }
    const QDateTime base = from.toTimeZone(tz);

    if (kind == QStringLiteral("at"))
    {
        const QDateTime atTime = parseDateTime(schedule.value(QStringLiteral("at")).toString(), tzId);
        if (!atTime.isValid())
        {
            if (error) *error = QStringLiteral("schedule.at invalid");
            return QDateTime();
        }
        if (atTime <= base)
        {
            if (error) *error = QStringLiteral("schedule.at is in the past");
            return QDateTime();
        }
        return atTime;
    }
    if (kind == QStringLiteral("every"))
    {
        const int requested = schedule.value(QStringLiteral("every_ms")).toInt(0);
        const int interval = std::max(settings_.min_interval_ms, requested);
        if (interval <= 0)
        {
            if (error) *error = QStringLiteral("schedule.every_ms invalid");
            return QDateTime();
        }
        const qint64 lastRunMs = toMs(job, "last_run_ms", 0);
        const qint64 baseMs = (lastRunMs > 0) ? lastRunMs : base.toMSecsSinceEpoch();
        qint64 nextMs = baseMs + interval;
        if (nextMs <= base.toMSecsSinceEpoch())
        {
            const qint64 delta = base.toMSecsSinceEpoch() - baseMs;
            const qint64 steps = delta / interval + 1;
            nextMs = baseMs + steps * interval;
        }
        return QDateTime::fromMSecsSinceEpoch(nextMs, tz);
    }
    if (kind == QStringLiteral("cron"))
    {
        const int lookaheadDays = std::max(1, settings_.cron_lookahead_days);
        QDateTime cursor = base;
        cursor = cursor.addSecs(60 - cursor.time().second());
        cursor.setTime(QTime(cursor.time().hour(), cursor.time().minute(), 0));
        const int maxMinutes = lookaheadDays * 24 * 60;
        for (int i = 0; i < maxMinutes; ++i)
        {
            QString cronErr;
            if (matchCron(cursor, schedule, &cronErr))
            {
                return cursor;
            }
            cursor = cursor.addSecs(60);
        }
        if (error) *error = QStringLiteral("cron next run not found");
        return QDateTime();
    }

    if (error) *error = QStringLiteral("schedule.kind unsupported");
    return QDateTime();
}

bool SchedulerService::updateNextRun(QJsonObject &job, const QDateTime &from, QString *error) const
{
    const QDateTime next = computeNextRun(job, from, error);
    if (!next.isValid())
    {
        job.remove(QStringLiteral("next_run_ms"));
        job.remove(QStringLiteral("next_run_at"));
        return false;
    }
    job.insert(QStringLiteral("next_run_ms"), static_cast<qint64>(next.toMSecsSinceEpoch()));
    job.insert(QStringLiteral("next_run_at"), next.toString(Qt::ISODate));
    return true;
}

QJsonObject SchedulerService::findJobById(const QString &jobId, int *indexOut) const
{
    if (indexOut) *indexOut = -1;
    const QString id = jobId.trimmed();
    if (id.isEmpty()) return QJsonObject();
    for (int i = 0; i < jobs_.size(); ++i)
    {
        const QJsonObject job = jobs_.at(i).toObject();
        if (job.value(QStringLiteral("job_id")).toString() == id)
        {
            if (indexOut) *indexOut = i;
            return job;
        }
    }
    return QJsonObject();
}

QJsonObject SchedulerService::applyAction(const QString &action, const QJsonObject &jobInput, QString *error)
{
    const QString normalized = action.trimmed().toLower();
    if (normalized == QStringLiteral("list") || normalized == QStringLiteral("get"))
    {
        return QJsonObject();
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QString jobId = jobInput.value(QStringLiteral("job_id")).toString().trimmed();

    if (normalized == QStringLiteral("add"))
    {
        QJsonObject job = jobInput;
        if (job.value(QStringLiteral("schedule")).toObject().isEmpty())
        {
            if (error) *error = QStringLiteral("job.schedule required");
            return QJsonObject();
        }
        const QString newId = ensureJobId(jobId);
        job.insert(QStringLiteral("job_id"), newId);
        if (job.value(QStringLiteral("name")).toString().trimmed().isEmpty())
        {
            job.insert(QStringLiteral("name"), newId);
        }
        if (!job.contains(QStringLiteral("enabled"))) job.insert(QStringLiteral("enabled"), true);
        if (!job.contains(QStringLiteral("delete_after_run"))) job.insert(QStringLiteral("delete_after_run"), false);
        if (!job.contains(QStringLiteral("session"))) job.insert(QStringLiteral("session"), QStringLiteral("main"));
        if (!job.contains(QStringLiteral("payload")))
        {
            QJsonObject payload;
            const QString msg = jobInput.value(QStringLiteral("message")).toString();
            if (!msg.isEmpty()) payload.insert(QStringLiteral("message"), msg);
            job.insert(QStringLiteral("payload"), payload);
        }
        job.insert(QStringLiteral("created_at"), now.toString(Qt::ISODate));
        job.insert(QStringLiteral("updated_at"), now.toString(Qt::ISODate));
        job.insert(QStringLiteral("run_count"), 0);

        QString scheduleErr;
        if (job.value(QStringLiteral("enabled")).toBool())
        {
            if (!updateNextRun(job, now, &scheduleErr))
            {
                if (error) *error = scheduleErr;
                return QJsonObject();
            }
        }
        jobs_.append(job);
        saveJobs();
        emitJobsUpdated();
        scheduleNextTimer();
        return job;
    }

    if (normalized == QStringLiteral("update"))
    {
        int idx = -1;
        QJsonObject job = findJobById(jobId, &idx);
        if (idx < 0)
        {
            if (error) *error = QStringLiteral("job not found");
            return QJsonObject();
        }
        QJsonObject updated = job;
        for (auto it = jobInput.begin(); it != jobInput.end(); ++it)
        {
            if (it.key() == QStringLiteral("job_id")) continue;
            updated.insert(it.key(), it.value());
        }
        updated.insert(QStringLiteral("updated_at"), now.toString(Qt::ISODate));
        if (updated.value(QStringLiteral("enabled")).toBool())
        {
            QString scheduleErr;
            if (!updateNextRun(updated, now, &scheduleErr))
            {
                if (error) *error = scheduleErr;
                return QJsonObject();
            }
        }
        jobs_[idx] = updated;
        saveJobs();
        emitJobsUpdated();
        scheduleNextTimer();
        return updated;
    }

    if (normalized == QStringLiteral("remove"))
    {
        int idx = -1;
        QJsonObject job = findJobById(jobId, &idx);
        if (idx < 0)
        {
            if (error) *error = QStringLiteral("job not found");
            return QJsonObject();
        }
        jobs_.removeAt(idx);
        saveJobs();
        emitJobsUpdated();
        scheduleNextTimer();
        return job;
    }

    if (normalized == QStringLiteral("enable") || normalized == QStringLiteral("disable"))
    {
        int idx = -1;
        QJsonObject job = findJobById(jobId, &idx);
        if (idx < 0)
        {
            if (error) *error = QStringLiteral("job not found");
            return QJsonObject();
        }
        const bool enable = (normalized == QStringLiteral("enable"));
        job.insert(QStringLiteral("enabled"), enable);
        job.insert(QStringLiteral("updated_at"), now.toString(Qt::ISODate));
        if (enable)
        {
            QString scheduleErr;
            updateNextRun(job, now, &scheduleErr);
        }
        else
        {
            job.remove(QStringLiteral("next_run_ms"));
            job.remove(QStringLiteral("next_run_at"));
        }
        jobs_[idx] = job;
        saveJobs();
        emitJobsUpdated();
        scheduleNextTimer();
        return job;
    }

    if (normalized == QStringLiteral("run"))
    {
        int idx = -1;
        QJsonObject job = findJobById(jobId, &idx);
        if (idx < 0)
        {
            if (error) *error = QStringLiteral("job not found");
            return QJsonObject();
        }
        // 立即执行（手动触发）
        QJsonObject record;
        record.insert(QStringLiteral("job_id"), jobId);
        record.insert(QStringLiteral("trigger"), QStringLiteral("manual"));
        record.insert(QStringLiteral("ts"), now.toString(Qt::ISODate));
        appendRunLog(jobId, record);

        const bool deleteAfter = job.value(QStringLiteral("delete_after_run")).toBool(false);
        job.insert(QStringLiteral("last_run_ms"), now.toMSecsSinceEpoch());
        job.insert(QStringLiteral("last_run_at"), now.toString(Qt::ISODate));
        job.insert(QStringLiteral("run_count"), job.value(QStringLiteral("run_count")).toInt(0) + 1);
        QString scheduleErr;
        updateNextRun(job, now, &scheduleErr);
        const QString kind = job.value(QStringLiteral("schedule")).toObject()
                                  .value(QStringLiteral("kind"))
                                  .toString()
                                  .trimmed()
                                  .toLower();
        if (kind == QStringLiteral("at"))
        {
            job.insert(QStringLiteral("enabled"), false);
            job.remove(QStringLiteral("next_run_ms"));
            job.remove(QStringLiteral("next_run_at"));
        }
        if (deleteAfter)
        {
            jobs_.removeAt(idx);
        }
        else
        {
            jobs_[idx] = job;
        }
        saveJobs();
        emitJobsUpdated();
        scheduleNextTimer();
        emit jobDue(job, now);
        return job;
    }

    if (error) *error = QStringLiteral("unsupported action");
    return QJsonObject();
}

QJsonObject SchedulerService::handleToolCall(const QJsonObject &args)
{
    ensureLoaded();
    const QString action = args.value(QStringLiteral("action")).toString().trimmed().toLower();
    const QString normalized = action.isEmpty() ? QStringLiteral("add") : action;

    QJsonObject result;
    result.insert(QStringLiteral("action"), normalized);

    if (normalized == QStringLiteral("list"))
    {
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("jobs"), jobs_);
        return result;
    }

    if (normalized == QStringLiteral("get"))
    {
        const QJsonObject jobInput = args.value(QStringLiteral("job")).toObject();
        const QString jobId = jobInput.value(QStringLiteral("job_id")).toString().trimmed();
        QJsonObject job = findJobById(jobId);
        if (job.isEmpty())
        {
            result.insert(QStringLiteral("ok"), false);
            result.insert(QStringLiteral("error"), QStringLiteral("job not found"));
            return result;
        }
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("job"), job);
        return result;
    }

    QJsonObject jobInput = args.value(QStringLiteral("job")).toObject();
    if (jobInput.isEmpty())
    {
        jobInput = args;
        jobInput.remove(QStringLiteral("action"));
    }

    QString error;
    const QJsonObject job = applyAction(normalized, jobInput, &error);
    if (!error.isEmpty())
    {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), error);
        return result;
    }
    result.insert(QStringLiteral("ok"), true);
    if (!job.isEmpty()) result.insert(QStringLiteral("job"), job);
    return result;
}

QJsonObject SchedulerService::handleUiAction(const QString &action, const QString &jobId)
{
    QJsonObject args;
    args.insert(QStringLiteral("action"), action);
    QJsonObject job;
    job.insert(QStringLiteral("job_id"), jobId);
    args.insert(QStringLiteral("job"), job);
    return handleToolCall(args);
}

void SchedulerService::emitJobsUpdated()
{
    emit jobsUpdated(jobs_);
}

void SchedulerService::saveJobs()
{
    if (jobsPath_.isEmpty()) return;
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("jobs"), jobs_);
    QSaveFile file(jobsPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    file.write(data);
    file.commit();
}

void SchedulerService::appendRunLog(const QString &jobId, const QJsonObject &record)
{
    if (jobId.trimmed().isEmpty() || runsDir_.isEmpty()) return;
    QDir().mkpath(runsDir_);
    const QString path = QDir(runsDir_).filePath(jobId + QStringLiteral(".jsonl"));
    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;
    QByteArray line = QJsonDocument(record).toJson(QJsonDocument::Compact);
    line.append('\n');
    file.write(line);
    file.close();
}

void SchedulerService::scheduleNextTimer()
{
    timer_.stop();
    if (!settings_.enabled) return;
    if (jobs_.isEmpty()) return;

    const QDateTime now = QDateTime::currentDateTime();
    qint64 nextMs = 0;
    for (int i = 0; i < jobs_.size(); ++i)
    {
        QJsonObject job = jobs_.at(i).toObject();
        if (!job.value(QStringLiteral("enabled")).toBool()) continue;
        qint64 jobNext = toMs(job, "next_run_ms", 0);
        if (jobNext <= 0)
        {
            QString err;
            updateNextRun(job, now, &err);
            jobs_[i] = job;
            jobNext = toMs(job, "next_run_ms", 0);
        }
        if (jobNext <= 0) continue;
        if (nextMs == 0 || jobNext < nextMs) nextMs = jobNext;
    }
    if (nextMs <= 0) return;
    const qint64 delay = std::max<qint64>(0, nextMs - now.toMSecsSinceEpoch());
    timer_.start(static_cast<int>(std::min<qint64>(delay, std::numeric_limits<int>::max())));
}

void SchedulerService::onTimerTimeout()
{
    if (!settings_.enabled) return;
    ensureLoaded();
    const QDateTime now = QDateTime::currentDateTime();

    QJsonArray updated;
    QVector<QJsonObject> dueJobs;

    for (int i = 0; i < jobs_.size(); ++i)
    {
        QJsonObject job = jobs_.at(i).toObject();
        if (!job.value(QStringLiteral("enabled")).toBool())
        {
            updated.append(job);
            continue;
        }
        const qint64 nextMs = toMs(job, "next_run_ms", 0);
        if (nextMs <= 0 || nextMs > now.toMSecsSinceEpoch())
        {
            updated.append(job);
            continue;
        }

        const QString jobId = job.value(QStringLiteral("job_id")).toString();
        QJsonObject record;
        record.insert(QStringLiteral("job_id"), jobId);
        record.insert(QStringLiteral("trigger"), QStringLiteral("timer"));
        record.insert(QStringLiteral("ts"), now.toString(Qt::ISODate));
        appendRunLog(jobId, record);

        job.insert(QStringLiteral("last_run_ms"), now.toMSecsSinceEpoch());
        job.insert(QStringLiteral("last_run_at"), now.toString(Qt::ISODate));
        job.insert(QStringLiteral("run_count"), job.value(QStringLiteral("run_count")).toInt(0) + 1);

        const QJsonObject schedule = job.value(QStringLiteral("schedule")).toObject();
        const QString kind = schedule.value(QStringLiteral("kind")).toString().trimmed().toLower();
        const bool deleteAfter = job.value(QStringLiteral("delete_after_run")).toBool(false);

        QString scheduleErr;
        updateNextRun(job, now, &scheduleErr);
        if (kind == QStringLiteral("at") || deleteAfter)
        {
            job.insert(QStringLiteral("enabled"), false);
            job.remove(QStringLiteral("next_run_ms"));
            job.remove(QStringLiteral("next_run_at"));
        }

        dueJobs.push_back(job);
        if (!deleteAfter)
        {
            updated.append(job);
        }
    }

    jobs_ = updated;
    saveJobs();
    emitJobsUpdated();
    scheduleNextTimer();

    for (const QJsonObject &job : dueJobs)
    {
        emit jobDue(job, now);
    }
}
