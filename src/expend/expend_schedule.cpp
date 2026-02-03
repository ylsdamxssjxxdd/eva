#include "expend.h"
#include "ui_expend.h"

#include <QDateTime>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QJsonDocument>
#include <QTableWidgetItem>

void Expend::initScheduleUi()
{
    if (!ui || !ui->schedule_table) return;
    ui->schedule_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->schedule_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->schedule_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->schedule_table->setAlternatingRowColors(false);
    if (ui->schedule_table->horizontalHeader())
    {
        ui->schedule_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    }
    connect(ui->schedule_table, &QTableWidget::itemSelectionChanged,
            this, &Expend::on_schedule_table_itemSelectionChanged, Qt::UniqueConnection);

    if (!scheduleCountdownTimer_.isActive())
    {
        scheduleCountdownTimer_.setInterval(DEFAULT_SCHEDULER_PAGE_REFRESH_MS);
        connect(&scheduleCountdownTimer_, &QTimer::timeout,
                this, &Expend::refreshScheduleCountdown, Qt::UniqueConnection);
        scheduleCountdownTimer_.start();
    }

    updateScheduleTexts();
    rebuildScheduleTable();
}

void Expend::updateScheduleTexts()
{
    if (!ui) return;
    if (ui->schedule_enable_button) ui->schedule_enable_button->setText(jtr("schedule enable"));
    if (ui->schedule_disable_button) ui->schedule_disable_button->setText(jtr("schedule disable"));
    if (ui->schedule_run_button) ui->schedule_run_button->setText(jtr("schedule run now"));
    if (ui->schedule_remove_button) ui->schedule_remove_button->setText(jtr("schedule remove"));
    if (ui->schedule_detail_label) ui->schedule_detail_label->setText(jtr("schedule detail"));
    if (ui->schedule_table)
    {
        ui->schedule_table->setHorizontalHeaderLabels(
            {jtr("schedule name"), jtr("schedule status"), jtr("schedule next run"), jtr("schedule countdown")});
    }
}

void Expend::recv_schedule_jobs(QString payload)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &err);
    QJsonArray jobs;
    if (err.error == QJsonParseError::NoError)
    {
        if (doc.isArray())
        {
            jobs = doc.array();
        }
        else if (doc.isObject())
        {
            const QJsonObject obj = doc.object();
            const QJsonValue val = obj.value(QStringLiteral("jobs"));
            if (val.isArray()) jobs = val.toArray();
        }
    }
    scheduleJobs_ = jobs;
    rebuildScheduleTable();
}

QString Expend::currentScheduleJobId() const
{
    if (!ui || !ui->schedule_table) return QString();
    const QList<QTableWidgetItem *> items = ui->schedule_table->selectedItems();
    if (items.isEmpty()) return QString();
    QTableWidgetItem *first = items.first();
    if (!first) return QString();
    return first->data(Qt::UserRole).toString();
}

QString Expend::formatCountdown(qint64 ms) const
{
    if (ms < 0) return QStringLiteral("-");
    qint64 totalSec = ms / 1000;
    const qint64 days = totalSec / 86400;
    totalSec %= 86400;
    const qint64 hours = totalSec / 3600;
    totalSec %= 3600;
    const qint64 minutes = totalSec / 60;
    const qint64 seconds = totalSec % 60;
    QString time = QStringLiteral("%1:%2:%3")
                       .arg(hours, 2, 10, QLatin1Char('0'))
                       .arg(minutes, 2, 10, QLatin1Char('0'))
                       .arg(seconds, 2, 10, QLatin1Char('0'));
    if (days > 0) time = QStringLiteral("%1d ").arg(days) + time;
    return time;
}

void Expend::rebuildScheduleTable()
{
    if (!ui || !ui->schedule_table) return;
    scheduleJobMap_.clear();
    const int rowCount = scheduleJobs_.size();
    ui->schedule_table->setRowCount(rowCount);
    const QDateTime now = QDateTime::currentDateTime();
    const QColor purple(160, 84, 255);

    for (int row = 0; row < rowCount; ++row)
    {
        const QJsonObject job = scheduleJobs_.at(row).toObject();
        const QString jobId = job.value(QStringLiteral("job_id")).toString();
        scheduleJobMap_.insert(jobId, job);

        QString name = job.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty()) name = jobId;
        const bool enabled = job.value(QStringLiteral("enabled")).toBool(true);
        const QString status = enabled ? jtr("schedule enabled") : jtr("schedule disabled");

        const qint64 nextMs = job.value(QStringLiteral("next_run_ms")).toVariant().toLongLong();
        QString nextText = QStringLiteral("-");
        if (nextMs > 0)
        {
            nextText = QDateTime::fromMSecsSinceEpoch(nextMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }
        const qint64 deltaMs = nextMs > 0 ? (nextMs - now.toMSecsSinceEpoch()) : -1;
        const QString countdown = enabled ? formatCountdown(deltaMs) : QStringLiteral("-");

        auto *itemName = new QTableWidgetItem(name);
        itemName->setData(Qt::UserRole, jobId);
        auto *itemStatus = new QTableWidgetItem(status);
        auto *itemNext = new QTableWidgetItem(nextText);
        auto *itemCountdown = new QTableWidgetItem(countdown);
        itemCountdown->setForeground(purple);

        ui->schedule_table->setItem(row, 0, itemName);
        ui->schedule_table->setItem(row, 1, itemStatus);
        ui->schedule_table->setItem(row, 2, itemNext);
        ui->schedule_table->setItem(row, 3, itemCountdown);
    }
    updateScheduleDetail();
}

void Expend::updateScheduleDetail()
{
    if (!ui || !ui->schedule_detail_text) return;
    const QString jobId = currentScheduleJobId();
    if (jobId.isEmpty())
    {
        ui->schedule_detail_text->clear();
        return;
    }
    const QJsonObject job = scheduleJobMap_.value(jobId);
    const QString detail = QString::fromUtf8(QJsonDocument(job).toJson(QJsonDocument::Indented));
    ui->schedule_detail_text->setPlainText(detail);
}

void Expend::refreshScheduleCountdown()
{
    if (!ui || !ui->schedule_table || !ui->tabWidget) return;
    if (ui->tabWidget->currentIndex() != window_map[SCHEDULE_WINDOW]) return;
    const QDateTime now = QDateTime::currentDateTime();
    const QColor purple(160, 84, 255);
    for (int row = 0; row < ui->schedule_table->rowCount(); ++row)
    {
        QTableWidgetItem *item = ui->schedule_table->item(row, 0);
        if (!item) continue;
        const QString jobId = item->data(Qt::UserRole).toString();
        const QJsonObject job = scheduleJobMap_.value(jobId);
        const bool enabled = job.value(QStringLiteral("enabled")).toBool(true);
        const qint64 nextMs = job.value(QStringLiteral("next_run_ms")).toVariant().toLongLong();
        const qint64 deltaMs = nextMs > 0 ? (nextMs - now.toMSecsSinceEpoch()) : -1;
        const QString countdown = enabled ? formatCountdown(deltaMs) : QStringLiteral("-");
        QTableWidgetItem *countItem = ui->schedule_table->item(row, 3);
        if (countItem)
        {
            countItem->setText(countdown);
            countItem->setForeground(purple);
        }
    }
}

void Expend::on_schedule_enable_button_clicked()
{
    const QString jobId = currentScheduleJobId();
    if (jobId.isEmpty()) return;
    emit expend2ui_scheduleAction(QStringLiteral("enable"), jobId);
}

void Expend::on_schedule_disable_button_clicked()
{
    const QString jobId = currentScheduleJobId();
    if (jobId.isEmpty()) return;
    emit expend2ui_scheduleAction(QStringLiteral("disable"), jobId);
}

void Expend::on_schedule_run_button_clicked()
{
    const QString jobId = currentScheduleJobId();
    if (jobId.isEmpty()) return;
    emit expend2ui_scheduleAction(QStringLiteral("run"), jobId);
}

void Expend::on_schedule_remove_button_clicked()
{
    const QString jobId = currentScheduleJobId();
    if (jobId.isEmpty()) return;
    emit expend2ui_scheduleAction(QStringLiteral("remove"), jobId);
}

void Expend::on_schedule_table_itemSelectionChanged()
{
    updateScheduleDetail();
}
