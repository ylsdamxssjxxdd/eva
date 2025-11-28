#include "widget.h"
#include "ui_widget.h"
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QScreen>
#include <QProcess>
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

QString Widget::dockerSandboxDisplayName() const
{
    const bool sandboxRequested = ui_engineer_ischecked && ui_dockerSandboxEnabled;
    const bool containerModeActive =
        dockerSandboxStatus_.usingExistingContainer || (sandboxRequested && dockerTargetMode_ == DockerTargetMode::Container);
    if (containerModeActive)
    {
        const QString statusName = dockerSandboxStatus_.containerName.trimmed();
        if (!statusName.isEmpty()) return statusName;
        if (sandboxRequested)
        {
            const QString configured = engineerDockerContainer.trimmed();
            if (!configured.isEmpty()) return configured;
        }
        return QStringLiteral("none");
    }
    const QString statusImage = dockerSandboxStatus_.image.trimmed();
    if (!statusImage.isEmpty()) return statusImage;
    if (sandboxRequested)
    {
        const QString configuredImage = engineerDockerImage.trimmed();
        if (!configuredImage.isEmpty()) return configuredImage;
    }
    return QStringLiteral("ubuntu:latest");
}

QString Widget::buildWorkspaceSnapshot(const QString &root, bool dockerView) const
{
    QDir rootDir(root);
    if (!rootDir.exists())
    {
        return QStringLiteral("Workspace directory not found.");
    }

    QStringList lines;
    if (dockerView)
        lines << QStringLiteral("Root: %1").arg(DockerSandbox::defaultContainerWorkdir());
    else
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
    const QString hostDir = engineerWorkDir.isEmpty() ? QDir(applicationDirPath).filePath("EVA_WORK") : engineerWorkDir;
    const QString hostDirDisplay = QDir::toNativeSeparators(hostDir);
    const bool sandboxRequested = ui_engineer_ischecked && ui_dockerSandboxEnabled;
    const bool sandboxReady = sandboxRequested && dockerSandboxStatus_.ready;
    const QString containerDir = sandboxReady && !dockerSandboxStatus_.containerWorkdir.isEmpty() ? dockerSandboxStatus_.containerWorkdir : DockerSandbox::defaultContainerWorkdir();
    const QString dockerDisplayName = dockerSandboxDisplayName();
    QString osDisplay = USEROS;
    QString shellDisplay = shell;
    QString workdirDisplay = hostDirDisplay;
    if (sandboxRequested)
    {
        osDisplay = sandboxReady ? QStringLiteral("Docker container (%1)").arg(dockerDisplayName)
                                 : QStringLiteral("Docker sandbox (%1) pending").arg(dockerDisplayName);
        shellDisplay = QStringLiteral("/bin/sh");
        workdirDisplay = containerDir;
    }
    engineer_system_info_.replace("{OS}", osDisplay);
    engineer_system_info_.replace("{DATE}", dateString);
    engineer_system_info_.replace("{SHELL}", shellDisplay);
    engineer_system_info_.replace("{COMPILE_ENV}", compile_env);
    engineer_system_info_.replace("{PYTHON_ENV}", python_env);
    engineer_system_info_.replace("{NODE_ENV}", node_env);
    engineer_system_info_.replace("{DIR}", workdirDisplay);
    engineer_system_info_.replace("{WORKSPACE_TREE}", buildWorkspaceSnapshot(hostDir, sandboxRequested));
    if (sandboxRequested)
    {
        QStringList dockerNotes;
        dockerNotes << QStringLiteral("Container mount point: %1").arg(containerDir);
        if (sandboxReady)
        {
            const QString skillsTarget = dockerSandboxStatus_.skillsMountPoint.isEmpty() ? DockerSandbox::skillsMountPoint()
                                                                                        : dockerSandboxStatus_.skillsMountPoint;
            if (!skillsTarget.isEmpty())
            {
                dockerNotes << QStringLiteral("Skills mount: %1").arg(skillsTarget);
            }
            if (!dockerSandboxStatus_.osPretty.isEmpty()) dockerNotes << dockerSandboxStatus_.osPretty;
            if (!dockerSandboxStatus_.kernelPretty.isEmpty()) dockerNotes << dockerSandboxStatus_.kernelPretty;
        }
        else if (!dockerSandboxStatus_.lastError.isEmpty())
        {
            dockerNotes << QStringLiteral("Sandbox status: %1").arg(dockerSandboxStatus_.lastError);
        }
        engineer_system_info_.append(QStringLiteral("\nDocker sandbox details:\n") + dockerNotes.join(QStringLiteral("\n")));
    }
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

void Widget::refreshEngineerPromptBlock()
{
    if (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked()) return;
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

void Widget::markEngineerEnvDirty()
{
    if (!ui_engineer_ischecked) return;
    engineerEnvReady_ = false;
}

void Widget::applyEngineerUiLock(bool locked)
{
    if (engineerUiLockActive_ == locked) return;
    engineerUiLockActive_ = locked;
    if (locked)
    {
        reflash_state("ui:" + jtr("engineer env initializing"), SIGNAL_SIGNAL);
    }
    else
    {
        reflash_state("ui:" + jtr("engineer env ready"), SUCCESS_SIGNAL);
    }
    ui_state_normal();
}

void Widget::onEngineerEnvReady()
{
    engineerEnvReady_ = true;
    drainEngineerGateQueue();
    ensureOutputAtBottom();
}

void Widget::enforceEngineerEnvReadyCheckpoint()
{
    if (!ui_engineer_ischecked) return;
    if (engineerEnvReady_) return;
    if (!engineerEnvWatcher_.isRunning())
    {
        triggerEngineerEnvRefresh(true);
    }
    if (!engineerUiLockActive_) applyEngineerUiLock(true);
}

void Widget::queueEngineerGateAction(const std::function<void()> &action, bool requireDockerReady)
{
    if (!ui_engineer_ischecked)
    {
        if (action) action();
        return;
    }
    engineerGateActive_ = true;
    markEngineerEnvDirty();
    if (requireDockerReady) engineerDockerReady_ = false;
    if (action) engineerGateQueue_.append(action);
    applyEngineerUiLock(true);
    enforceEngineerEnvReadyCheckpoint();
    drainEngineerGateQueue();
}

void Widget::drainEngineerGateQueue()
{
    if (!engineerGateActive_) return;
    if (!engineerEnvReady_ || !engineerDockerReady_) return;
    const auto pending = engineerGateQueue_;
    engineerGateQueue_.clear();
    engineerGateActive_ = false;
    for (const auto &fn : pending)
    {
        if (fn) fn();
    }
    if (engineerUiLockActive_)
    {
        QTimer::singleShot(0, this, [this]() {
            if (engineerUiLockActive_) applyEngineerUiLock(false);
        });
    }
}

void Widget::triggerEngineerEnvRefresh(bool updatePrompt)
{
    if (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked()) return;
    markEngineerEnvDirty();
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
    onEngineerEnvReady();
    if (updatePrompt && !date_ui) updatePrompt = false;
    if (updatePrompt && (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked()))
    {
        updatePrompt = false;
    }
    if (!updatePrompt) return;

    refreshEngineerPromptBlock();
}

void Widget::setEngineerWorkDirSilently(const QString &dir)
{
    if (dir.isEmpty()) return;
    engineerWorkDir = QDir::cleanPath(dir);
    dockerMountPromptedContainers_.clear();
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
    dockerMountPromptedContainers_.clear();
    markEngineerEnvDirty();
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
    syncDockerSandboxConfig();
}

void Widget::syncDockerSandboxConfig(bool forceEmit)
{
    DockerSandbox::Config cfg;
    cfg.hostWorkdir = engineerWorkDir.trimmed();
    if (!cfg.hostWorkdir.isEmpty()) cfg.hostWorkdir = QDir::cleanPath(cfg.hostWorkdir);
    QString skillsRoot;
    if (skillManager)
    {
        skillsRoot = skillManager->skillsRoot().trimmed();
    }
    if (skillsRoot.isEmpty())
    {
        skillsRoot = QDir(applicationDirPath).filePath(QStringLiteral("EVA_SKILLS"));
    }
    cfg.hostSkillsDir = skillsRoot.trimmed();
    if (!cfg.hostSkillsDir.isEmpty()) cfg.hostSkillsDir = QDir::cleanPath(cfg.hostSkillsDir);

    const bool baseEnabled = ui_engineer_ischecked && ui_dockerSandboxEnabled && !cfg.hostWorkdir.isEmpty();
    cfg.target = (dockerTargetMode_ == DockerTargetMode::Container) ? DockerSandbox::TargetType::Container : DockerSandbox::TargetType::Image;

    if (cfg.target == DockerSandbox::TargetType::Image)
    {
        cfg.image = engineerDockerImage.trimmed();
        if (cfg.image.isEmpty())
        {
            const QString persisted = loadPersistedDockerImage();
            if (!persisted.isEmpty())
            {
                cfg.image = persisted;
                engineerDockerImage = persisted;
                updateDockerImageCombo();
            }
        }
        if (cfg.image.isEmpty()) cfg.image = QStringLiteral("ubuntu:latest");
        cfg.enabled = baseEnabled;
    }
    else
    {
        cfg.containerName = sanitizeDockerContainerValue(engineerDockerContainer);
        if (cfg.containerName.isEmpty())
        {
            const QString persisted = loadPersistedDockerContainer();
            if (!persisted.isEmpty())
            {
                cfg.containerName = persisted;
                engineerDockerContainer = persisted;
                updateDockerImageCombo();
            }
        }
        cfg.enabled = baseEnabled && !cfg.containerName.isEmpty();
    }

    if (!forceEmit && hasDockerConfigSnapshot_ && cfg.enabled == lastDockerConfigSnapshot_.enabled &&
        cfg.image == lastDockerConfigSnapshot_.image && cfg.containerName == lastDockerConfigSnapshot_.containerName &&
        cfg.hostWorkdir == lastDockerConfigSnapshot_.hostWorkdir && cfg.hostSkillsDir == lastDockerConfigSnapshot_.hostSkillsDir &&
        cfg.target == lastDockerConfigSnapshot_.target)
    {
        return;
    }
    lastDockerConfigSnapshot_ = cfg;
    hasDockerConfigSnapshot_ = true;
    emit ui2tool_dockerConfigChanged(cfg);
}

bool Widget::shouldUseDockerEnv() const
{
    return ui_engineer_ischecked && ui_dockerSandboxEnabled && dockerSandboxStatusValid_ &&
           dockerSandboxStatus_.ready && !dockerSandboxStatus_.containerName.isEmpty();
}

QString Widget::runDockerExecCommand(const QString &command, int timeoutMs) const
{
    if (!shouldUseDockerEnv()) return {};
#ifdef Q_OS_WIN
    const QString program = QStringLiteral("docker.exe");
#else
    const QString program = QStringLiteral("docker");
#endif
    QStringList args;
    args << QStringLiteral("exec") << QStringLiteral("-i") << dockerSandboxStatus_.containerName
         << QStringLiteral("/bin/sh") << QStringLiteral("-c") << command;
    QProcess process;
    process.start(program, args);
    if (!process.waitForFinished(timeoutMs))
    {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput());
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
            setBaseWindowIcon(QIcon(":/logo/jimu.png")); // 千年积木
        }
    }
    else
    {
        if (monitor_timer.isActive()) monitor_timer.stop();
    }
}
