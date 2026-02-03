#include "widget.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>

void Widget::initScheduler()
{
    if (scheduler_) return;
    scheduler_ = new SchedulerService(this);
    scheduler_->setBaseDir(applicationDirPath);
    scheduler_->setSettings(schedulerSettings_);
    connect(scheduler_, &SchedulerService::jobsUpdated, this, &Widget::onSchedulerJobsUpdated);
    connect(scheduler_, &SchedulerService::jobDue, this, &Widget::onSchedulerJobDue);
    scheduler_->loadJobs();
}

void Widget::refreshSchedulerSettings()
{
    if (!scheduler_) initScheduler();
    if (!scheduler_) return;
    scheduler_->setSettings(schedulerSettings_);
    onSchedulerJobsUpdated(scheduler_->jobs());
}

void Widget::handleScheduleToolCall(const mcp::json &toolsCall)
{
    if (!scheduler_) initScheduler();
    if (!scheduler_) return;

    QJsonObject argsObj;
    if (toolsCall.contains("arguments"))
    {
        const std::string raw = toolsCall.at("arguments").dump();
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(raw), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
        {
            argsObj = doc.object();
        }
    }

    const QJsonObject resultObj = scheduler_->handleToolCall(argsObj);
    const QString resultJson = QString::fromUtf8(QJsonDocument(resultObj).toJson(QJsonDocument::Compact));
    const QString toolResult = QStringLiteral("schedule_task return\n") + resultJson;
    reflash_state(QStringLiteral("tool:schedule_task return\n") + resultJson, TOOL_SIGNAL);
    recv_toolpushover(toolResult);
}

void Widget::recv_schedule_action(QString action, QString jobId)
{
    handleScheduleUiAction(action, jobId);
}

void Widget::handleScheduleUiAction(const QString &action, const QString &jobId)
{
    if (!scheduler_) initScheduler();
    if (!scheduler_) return;
    const QJsonObject result = scheduler_->handleUiAction(action, jobId);
    const bool ok = result.value(QStringLiteral("ok")).toBool(true);
    const QString err = result.value(QStringLiteral("error")).toString();
    if (!ok && !err.isEmpty())
    {
        reflash_state(QStringLiteral("ui:schedule_task ") + err, WRONG_SIGNAL);
    }
}

void Widget::onSchedulerJobsUpdated(const QJsonArray &jobs)
{
    const QString payload = QString::fromUtf8(QJsonDocument(jobs).toJson(QJsonDocument::Compact));
    emit ui2expend_schedule_jobs(payload);
}

void Widget::onSchedulerJobDue(const QJsonObject &job, const QDateTime &fireTime)
{
    enqueueScheduledDispatch(job, fireTime);
    tryDispatchScheduledJobs();
}

void Widget::enqueueScheduledDispatch(const QJsonObject &job, const QDateTime &fireTime)
{
    const QString jobId = job.value(QStringLiteral("job_id")).toString().trimmed();
    for (const auto &item : scheduledDispatchQueue_)
    {
        const QString existingId = item.job.value(QStringLiteral("job_id")).toString().trimmed();
        if (!jobId.isEmpty() && jobId == existingId) return;
    }
    ScheduledDispatch item;
    item.job = job;
    item.fireTime = fireTime;
    scheduledDispatchQueue_.enqueue(item);
}

QString Widget::buildScheduleMessage(const QJsonObject &job, const QDateTime &fireTime) const
{
    const QString jobId = job.value(QStringLiteral("job_id")).toString().trimmed();
    QString name = job.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) name = jobId;
    QString payload;
    const QJsonValue payloadVal = job.value(QStringLiteral("payload"));
    if (payloadVal.isObject())
    {
        payload = payloadVal.toObject().value(QStringLiteral("message")).toString().trimmed();
    }
    if (payload.isEmpty())
    {
        payload = job.value(QStringLiteral("message")).toString().trimmed();
    }
    const QString timeText = fireTime.isValid() ? fireTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    const QString tag = jtr("schedule task");

    QString text = QStringLiteral("\u3010%1\u3011%2").arg(tag, name);
    if (!timeText.isEmpty())
    {
        text += QStringLiteral("\n") + jtr("schedule fired at") + QStringLiteral(": ") + timeText;
    }
    if (!payload.isEmpty())
    {
        text += QStringLiteral("\n") + payload;
    }
    return text;
}

bool Widget::dispatchScheduledText(const QString &text, const QJsonObject &job)
{
    Q_UNUSED(job);
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return false;

    if (ui_state != CHAT_STATE)
    {
        reflash_state(QStringLiteral("ui:schedule ignored (not in chat state)"), WRONG_SIGNAL);
        return false;
    }

    if (linkProfile_ == LinkProfile::Control && !isControllerActive())
    {
        reflash_state(jtr("control disconnected"), WRONG_SIGNAL);
        return false;
    }
    if (isControllerActive())
    {
        reflash_state(jtr("control send missing"), WRONG_SIGNAL);
        return false;
    }

    if (ui_mode == LOCAL_MODE)
    {
        const bool serverRunning = serverManager && serverManager->isRunning();
        const bool backendReady = serverRunning && backendOnline_ && !lazyUnloaded_ && !lazyWakeInFlight_;
        if (!backendReady)
        {
            if (!pendingSendAfterWake_)
            {
                pendingSendAfterWake_ = true;
                reflash_state("ui:" + jtr("pop wake hint"), SIGNAL_SIGNAL);
            }
            if (serverManager && !lazyWakeInFlight_)
            {
                ensureLocalServer(true);
            }
            return false;
        }
    }
    pendingSendAfterWake_ = false;

    pendingAssistantHeaderReset_ = false;
    flushPendingStream();
    turnThinkActive_ = false;
    turnThinkHeaderPrinted_ = false;
    turnAssistantHeaderPrinted_ = false;
    sawPromptPast_ = false;
    sawFinalPast_ = false;
    kvUsedBeforeTurn_ = kvUsed_;
    cancelLazyUnload(QStringLiteral("schedule dispatch"));
    markBackendActivity();

    kvTokensTurn_ = 0;
    kvPromptTokensTurn_ = 0;
    kvStreamedTurn_ = 0;
    emit ui2net_stop(0);
    beginSessionIfNeeded();

    InputPack in;
    in.text = trimmed;
    in.images.clear();
    in.wavs.clear();
    in.documents.clear();

    if (startCompactionIfNeeded(in))
    {
        currentTask_ = ConversationTask::Compaction;
        startTurnFlow(currentTask_, false);
        logCurrentTask(currentTask_);
        startCompactionRun(compactionReason_);
        is_run = true;
        ui_state_pushing();
        return true;
    }

    currentTask_ = ConversationTask::ChatReply;
    startTurnFlow(currentTask_, false);
    logCurrentTask(currentTask_);
    ENDPOINT_DATA data = prepareEndpointData();
    handleChatReply(data, in);
    is_run = true;
    ui_state_pushing();
    return true;
}

void Widget::tryDispatchScheduledJobs()
{
    if (scheduledDispatchQueue_.isEmpty()) return;
    if (is_run || toolInvocationActive_ || engineerProxyRuntime_.active || compactionInFlight_ || compactionQueued_)
    {
        return;
    }

    ScheduledDispatch item = scheduledDispatchQueue_.head();
    const QString message = buildScheduleMessage(item.job, item.fireTime);
    if (!dispatchScheduledText(message, item.job))
    {
        return;
    }
    scheduledDispatchQueue_.dequeue();
}
