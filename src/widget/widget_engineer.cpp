#include "widget.h"
#include "ui_widget.h"
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QScreen>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>

namespace
{
QString canonicalOrAbsolutePath(const QDir &dir)
{
    const QString canonical = dir.canonicalPath();
    if (!canonical.isEmpty()) return canonical;
    return dir.absolutePath();
}

void appendWorkspaceListing(const QDir &dir, int depth, int maxDepth, int maxEntriesPerDir, QStringList &lines)
{
    QStringList entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                        QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);
    if (entries.isEmpty())
    {
        const QString indent(depth * 2 + 2, ' ');
        lines << QString("%1- <empty>").arg(indent);
        return;
    }

    const qsizetype total = entries.size();
    const int displayed = std::min(maxEntriesPerDir, static_cast<int>(total));
    for (int i = 0; i < displayed; ++i)
    {
        const QString &name = entries.at(i);
        const QString indent(depth * 2 + 2, ' ');
        const QString absolutePath = dir.absoluteFilePath(name);
        QFileInfo info(absolutePath);
        const bool isDir = info.isDir();
        QString line = QString("%1- %2").arg(indent, name);
        if (isDir) line.append('/');
        lines << line;
        if (isDir && depth + 1 < maxDepth)
        {
            appendWorkspaceListing(QDir(absolutePath), depth + 1, maxDepth, maxEntriesPerDir, lines);
        }
    }
    if (total > displayed)
    {
        const QString indent(depth * 2 + 2, ' ');
        lines << QString("%1- ... (%2 more entries)").arg(indent).arg(total - displayed);
    }
}
} // namespace

QString Widget::buildWorkspaceSnapshot(const QString &root) const
{
    QDir rootDir(root);
    if (!rootDir.exists())
    {
        return QStringLiteral("Workspace directory not found.");
    }

    QStringList lines;
    lines << QString("Root: %1").arg(QDir::toNativeSeparators(canonicalOrAbsolutePath(rootDir)));

    constexpr int kMaxDepth = 2;
    constexpr int kMaxEntriesPerDir = 60;
    appendWorkspaceListing(rootDir, 0, kMaxDepth, kMaxEntriesPerDir, lines);

    return lines.join(QChar('\n'));
}

QString Widget::create_engineer_info()
{
    QString engineer_info_ = promptx::engineerInfo();
    QString engineer_system_info_ = promptx::engineerSystemInfo();
    QDate currentDate = QDate::currentDate(); // 今天日期
    QString dateString = currentDate.toString("yyyy" + QString(" ") + jtr("year") + QString(" ") + "M" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    engineer_system_info_.replace("{OS}", USEROS);
    engineer_system_info_.replace("{DATE}", dateString);
    engineer_system_info_.replace("{SHELL}", shell);
    engineer_system_info_.replace("{COMPILE_ENV}", compile_env);
    engineer_system_info_.replace("{PYTHON_ENV}", python_env);
    engineer_system_info_.replace("{NODE_ENV}", node_env);
    // Use selected engineer working directory (fallback to default)
    const QString dir = engineerWorkDir.isEmpty() ? (applicationDirPath + "/EVA_WORK") : engineerWorkDir;
    engineer_system_info_.replace("{DIR}", QDir::toNativeSeparators(dir));
    engineer_system_info_.replace("{WORKSPACE_TREE}", buildWorkspaceSnapshot(dir));
    engineer_info_.replace("{engineer_system_info}", engineer_system_info_);
    return engineer_info_;
}

Widget::EngineerEnvSnapshot Widget::collectEngineerEnvSnapshot()
{
    EngineerEnvSnapshot snapshot;
    snapshot.python = checkPython();
    snapshot.compile = checkCompile();
    snapshot.node = checkNode();
    return snapshot;
}

void Widget::triggerEngineerEnvRefresh(bool updatePrompt)
{
    if (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked()) return;
    if (engineerEnvWatcher_.isRunning())
    {
        engineerEnvRefreshQueued_ = true;
        engineerEnvPendingPromptUpdate_ = engineerEnvPendingPromptUpdate_ || updatePrompt;
        return;
    }
    engineerEnvApplyPromptOnCompletion_ = updatePrompt;
    engineerEnvRefreshQueued_ = false;
    engineerEnvPendingPromptUpdate_ = false;
    auto future = QtConcurrent::run([this]() -> EngineerEnvSnapshot { return collectEngineerEnvSnapshot(); });
    engineerEnvWatcher_.setFuture(future);
}

void Widget::onEngineerEnvProbeFinished()
{
    if (!engineerEnvWatcher_.isFinished()) return;
    const EngineerEnvSnapshot snapshot = engineerEnvWatcher_.result();
    const bool updatePrompt = engineerEnvApplyPromptOnCompletion_;
    engineerEnvApplyPromptOnCompletion_ = false;
    applyEngineerEnvSnapshot(snapshot, updatePrompt);
    if (engineerEnvRefreshQueued_)
    {
        const bool queuedPrompt = engineerEnvPendingPromptUpdate_;
        engineerEnvPendingPromptUpdate_ = false;
        engineerEnvRefreshQueued_ = false;
        triggerEngineerEnvRefresh(queuedPrompt);
    }
}

void Widget::applyEngineerEnvSnapshot(const EngineerEnvSnapshot &snapshot, bool updatePrompt)
{
    python_env = snapshot.python;
    compile_env = snapshot.compile;
    node_env = snapshot.node;
    if (updatePrompt && !date_ui) updatePrompt = false;
    if (updatePrompt && (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked()))
    {
        updatePrompt = false;
    }
    if (!updatePrompt) return;

    ui_extra_prompt = create_extra_prompt();
    get_date();

    if (!ui_messagesArray.isEmpty())
    {
        QJsonObject system = ui_messagesArray.first().toObject();
        system.insert(QStringLiteral("content"), ui_DATES.date_prompt);
        ui_messagesArray.replace(0, system);
    }
    if (history_ && !history_->sessionId().isEmpty())
    {
        history_->rewriteAllMessages(ui_messagesArray);
    }
    if (lastSystemRecordIndex_ >= 0)
    {
        updateRecordEntryContent(lastSystemRecordIndex_, ui_DATES.date_prompt);
    }
    if (engineerRestoreOutputAfterEngineerRefresh_)
    {
        engineerRestoreOutputAfterEngineerRefresh_ = false;
        ensureOutputAtBottom();
    }
}

void Widget::setEngineerWorkDirSilently(const QString &dir)
{
    if (dir.isEmpty()) return;
    engineerWorkDir = QDir::cleanPath(dir);
    if (ui->terminalPane)
    {
        ui->terminalPane->setManualWorkingDirectory(engineerWorkDir);
    }
    if (date_ui && date_ui->date_engineer_workdir_LineEdit)
    {
        date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
    }
    // Note: no emit here; caller decides when to notify tools
    // Also refresh extra prompt so UI shows updated path in system message
    ui_extra_prompt = create_extra_prompt();
}

void Widget::setEngineerWorkDir(const QString &dir)
{
    if (dir.isEmpty()) return;
    engineerWorkDir = QDir::cleanPath(dir);
    if (ui->terminalPane)
    {
        ui->terminalPane->setManualWorkingDirectory(engineerWorkDir);
    }
    emit ui2tool_workdir(engineerWorkDir);
    reflash_state(QString::fromUtf8("ui:工程师工作目录 -> ") + engineerWorkDir, SIGNAL_SIGNAL);
    if (date_ui && date_ui->date_engineer_workdir_LineEdit)
    {
        date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
    }
    // Workdir affects the engineer info injected into extra prompt; rebuild so
    // that the main UI system message shows the latest path immediately.
    ui_extra_prompt = create_extra_prompt();
}

void Widget::monitorTime()
{
    // 不在本地聊天模式 / 推理中 / 未装载 等情况下不处理
    if (!is_load || is_run || ui_state != CHAT_STATE || ui_mode != LOCAL_MODE || ui_monitor_frame <= 0 || ui_SETTINGS.mmprojpath.isEmpty())
    {
        return;
    }
    if (is_monitor) return; // 防抖：上一帧尚未结束
    is_monitor = true;

    // 捕获一帧并写入滚动缓冲（最多保留最近 kMonitorKeepSeconds_ 秒）
    const QString filePath = saveScreen();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    monitorFrames_.append(MonitorFrame{filePath, nowMs});
    // 修剪超过时间窗的旧帧，并清理对应文件（尽量不积累垃圾）
    const qint64 cutoff = nowMs - qint64(kMonitorKeepSeconds_) * 1000;
    while (!monitorFrames_.isEmpty() && monitorFrames_.front().tsMs < cutoff)
    {
        const QString old = monitorFrames_.front().path;
        monitorFrames_.pop_front();
        QFile f(old);
        if (f.exists()) f.remove(); // 只清理我们缓存产生的文件
    }

    is_monitor = false; // 解锁，允许下一帧
}

QString Widget::create_screen_info()
{
    // 屏幕左上角坐标为(0,0) 右下角坐标为(x,y)
    QString info;
    QScreen *screen = QApplication::primaryScreen();
    // 使用物理像素尺寸，而不是逻辑像素
    QRect screenGeometry = screen->geometry();
    qreal devicePixelRatio = screen->devicePixelRatio();
    // 计算实际的物理像素尺寸
    int physicalWidth = screenGeometry.width() * devicePixelRatio;
    int physicalHeight = screenGeometry.height() * devicePixelRatio;
    info = QString("The coordinates of the top left corner of the screen are (0,0) and the coordinates of the bottom right corner are (%1, %2)")
               .arg(physicalWidth)
               .arg(physicalHeight);
    return info;
}

void Widget::updateMonitorTimer()
{
    // 仅在“本地 + 对话模式 + 已装载模型 + 设置了帧率>0”时启用
    if (ui_mode == LOCAL_MODE && ui_state == CHAT_STATE && is_load && ui_monitor_frame > 0 && !ui_SETTINGS.mmprojpath.isEmpty())
    {
        // 计算间隔（毫秒）；向最近整数取整，至少 1ms
        const int intervalMs = qMax(1, int(1000.0 / ui_monitor_frame + 0.5));
        if (monitor_timer.interval() != intervalMs || !monitor_timer.isActive())
        {
            qDebug() << "开始监视..." << ui_monitor_frame;
            monitor_timer.start(intervalMs);
            EVA_icon = QIcon(":/logo/jimu.png"); // 千年积木
            QApplication::setWindowIcon(EVA_icon);
            trayIcon->setIcon(EVA_icon);
        }
    }
    else
    {
        if (monitor_timer.isActive()) monitor_timer.stop();
    }
}
