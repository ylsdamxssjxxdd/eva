#include "widget.h"
#include "ui_widget.h"
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScreen>
#include <QProcess>
#include <QtConcurrent/QtConcurrentRun>
#include <QMenu>
#include <QTextDocument>
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

    const QString canonicalRoot = canonicalOrAbsolutePath(rootDir);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool cacheMatch = !workspaceSnapshotDirty_ &&
                            cachedWorkspaceDockerView_ == dockerView &&
                            !cachedWorkspaceRoot_.isEmpty() &&
                            cachedWorkspaceRoot_ == canonicalRoot &&
                            (cachedWorkspaceSnapshotAtMs_ > 0) &&
                            (nowMs - cachedWorkspaceSnapshotAtMs_ < WORKSPACE_SNAPSHOT_CACHE_MS) &&
                            !cachedWorkspaceSnapshot_.isEmpty();
    if (cacheMatch)
    {
        return cachedWorkspaceSnapshot_;
    }

    QStringList lines;
    if (dockerView)
        lines << QStringLiteral("Root: %1").arg(DockerSandbox::defaultContainerWorkdir());
    else
        lines << QString("Root: %1").arg(QDir::toNativeSeparators(canonicalRoot));

    constexpr int kMaxDepth = 2;
    constexpr int kMaxEntriesPerDir = 60;
    appendWorkspaceListing(rootDir, 0, kMaxDepth, kMaxEntriesPerDir, lines);

    cachedWorkspaceSnapshot_ = lines.join(QChar('\n'));
    cachedWorkspaceRoot_ = canonicalRoot;
    cachedWorkspaceDockerView_ = dockerView;
    cachedWorkspaceSnapshotAtMs_ = nowMs;
    workspaceSnapshotDirty_ = false;
    return cachedWorkspaceSnapshot_;
}

void Widget::invalidateWorkspaceSnapshotCache()
{
    workspaceSnapshotDirty_ = true;
    cachedWorkspaceSnapshotAtMs_ = 0;
}

bool Widget::isArchitectModeActive() const
{
    return ui_engineer_ischecked && engineerArchitectMode_;
}

void Widget::updateEngineerConsoleVisibility()
{
    if (!ui || !ui->engineerConsole || !ui->outputSplitter) return;
    const bool visible = isArchitectModeActive();
    ui->engineerConsole->setVisible(visible);
    if (visible)
    {
        const int splitterWidth = qMax(1, ui->outputSplitter->width());
        const int secondary = qMax(220, splitterWidth / 3);
        QList<int> sizes;
        sizes << qMax(splitterWidth - secondary, 320) << secondary;
        ui->outputSplitter->setSizes(sizes);
    }
    else
    {
        QList<int> sizes;
        sizes << qMax(1, ui->output->width()) << 0;
        ui->outputSplitter->setSizes(sizes);
    }
}

void Widget::resetEngineerConsole()
{
    if (!ui || !ui->engineerConsole) return;
    ui->engineerConsole->clear();
    engineerConsolePreviewActive_ = false;
    resetEngineerStreamState();
}

void Widget::appendEngineerConsole(const QString &line, bool reset)
{
    if (!ui || !ui->engineerConsole) return;
    if (reset)
    {
        ui->engineerConsole->clear();
        engineerConsolePreviewActive_ = false;
    }
    if (!line.isEmpty())
    {
        engineerConsolePreviewActive_ = false;
        ui->engineerConsole->append(line);
    }
}

void Widget::resetEngineerStreamState()
{
    engineerThinkActive_ = false;
    engineerThinkHeaderPrinted_ = false;
    engineerAssistantHeaderPrinted_ = false;
    engineerReasoningStreamSeen_ = false;
    engineerAssistantStreamSeen_ = false;
    engineerReasoningNeedsLineBreak_ = false;
    engineerAssistantNeedsLineBreak_ = false;
}

void Widget::appendEngineerText(const QString &text, bool newline, const QColor &color)
{
    if (!ui || !ui->engineerConsole) return;
    QTextCursor c = ui->engineerConsole->textCursor();
    c.movePosition(QTextCursor::End);
    const QColor resolved = color.isValid() ? color : themeTextPrimary();
    QTextCharFormat fmt;
    fmt.setForeground(QBrush(resolved));
    c.mergeCharFormat(fmt);
    if (!text.isEmpty()) c.insertText(text);
    if (newline) c.insertText(QStringLiteral("\n"));
    QTextCharFormat reset;
    reset.setForeground(QBrush(themeTextPrimary()));
    c.mergeCharFormat(reset);
    ui->engineerConsole->setTextCursor(c);
    if (QScrollBar *sb = ui->engineerConsole->verticalScrollBar()) sb->setValue(sb->maximum());
}

void Widget::appendEngineerRoleBlock(const QString &role, const QString &text)
{
    if (!ui || !ui->engineerConsole) return;
    if (!ui->engineerConsole->document()->isEmpty()) appendEngineerText(QString(), true);

    const QString trimmed = role.trimmed();
    const QString canonical = trimmed.toLower();
    const QString labelSystem = jtr("role_system");
    const QString labelUser = jtr("role_user");
    const QString labelThink = jtr("role_think");
    const QString labelTool = jtr("role_tool");
    const QString labelModel = jtr("role_model");
    RecordRole roleType = RecordRole::System;
    if (canonical == QStringLiteral("tool") || trimmed == labelTool)
        roleType = RecordRole::Tool;
    else if (canonical == QStringLiteral("think") || trimmed == labelThink)
        roleType = RecordRole::Think;
    else if (canonical == QStringLiteral("assistant") || canonical == QStringLiteral("model") || trimmed == labelModel)
        roleType = RecordRole::Assistant;
    else if (canonical == QStringLiteral("user") || trimmed == labelUser)
        roleType = RecordRole::User;
    else
        roleType = RecordRole::System;

    appendEngineerText(role, true, chipColorForRole(roleType));
    if (!text.isEmpty()) appendEngineerText(text, true);
}

void Widget::processEngineerStreamChunk(const QString &chunk)
{
    if (!ui || !ui->engineerConsole) return;
    if (chunk.isEmpty()) return;

    const QString begin = QString(DEFAULT_THINK_BEGIN);
    const QString tend = QString(DEFAULT_THINK_END);
    const QString labelThink = jtr("role_think").isEmpty() ? QStringLiteral("think") : jtr("role_think");
    const QString labelAssistant = jtr("role_model").isEmpty() ? QStringLiteral("model") : jtr("role_model");

    int pos = 0;
    const int n = chunk.size();
    while (pos < n)
    {
        if (engineerThinkActive_)
        {
            int endIdx = chunk.indexOf(tend, pos);
            const int until = (endIdx == -1) ? n : endIdx;
            QString thinkPart = chunk.mid(pos, until - pos);
            thinkPart.replace(begin, QString());
            thinkPart.replace(tend, QString());
            if (!thinkPart.isEmpty())
            {
                if (!engineerThinkHeaderPrinted_)
                {
                    appendEngineerRoleBlock(labelThink, QString());
                    engineerThinkHeaderPrinted_ = true;
                }
                appendEngineerText(thinkPart, false);
                engineerReasoningStreamSeen_ = true;
                const bool endsWithNewline = thinkPart.endsWith(QChar('\n')) || thinkPart.endsWith(QChar('\r'));
                engineerReasoningNeedsLineBreak_ = !endsWithNewline;
            }
            if (endIdx == -1)
            {
                break;
            }
            engineerThinkActive_ = false;
            pos = endIdx + tend.size();
            if (!engineerAssistantHeaderPrinted_)
            {
                appendEngineerRoleBlock(labelAssistant, QString());
                engineerAssistantHeaderPrinted_ = true;
            }
            continue;
        }
        else
        {
            int beginIdx = chunk.indexOf(begin, pos);
            const int until = (beginIdx == -1) ? n : beginIdx;
            QString asstPart = chunk.mid(pos, until - pos);
            asstPart.replace(begin, QString());
            asstPart.replace(tend, QString());
            if (!asstPart.isEmpty())
            {
                if (!engineerAssistantHeaderPrinted_)
                {
                    appendEngineerRoleBlock(labelAssistant, QString());
                    engineerAssistantHeaderPrinted_ = true;
                }
                appendEngineerText(asstPart, false);
                engineerAssistantStreamSeen_ = true;
                const bool endsWithNewline = asstPart.endsWith(QChar('\n')) || asstPart.endsWith(QChar('\r'));
                engineerAssistantNeedsLineBreak_ = !endsWithNewline;
            }
            if (beginIdx == -1)
            {
                break;
            }
            if (!engineerThinkHeaderPrinted_)
            {
                appendEngineerRoleBlock(labelThink, QString());
                engineerThinkHeaderPrinted_ = true;
            }
            engineerThinkActive_ = true;
            pos = beginIdx + begin.size();
            continue;
        }
    }
}

QString Widget::buildEngineerSystemDetails() const
{
    QString engineer_system_info_ = promptx::engineerSystemInfo();
    QDate currentDate = QDate::currentDate();
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
    return engineer_system_info_;
}

QString Widget::loadAgentsGuidance() const
{
    if (!ui_engineer_ischecked) return {};
    if (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked()) return {};

    QStringList searchRoots;
    const auto pushCandidate = [&searchRoots](const QString &dir) {
        const QString trimmed = dir.trimmed();
        if (trimmed.isEmpty()) return;
        const QString normalized = QDir(trimmed).absolutePath();
        if (normalized.isEmpty()) return;
        if (!searchRoots.contains(normalized, Qt::CaseInsensitive)) searchRoots << normalized;
    };

    if (!engineerWorkDir.trimmed().isEmpty())
        pushCandidate(engineerWorkDir);
    else
        pushCandidate(QDir(applicationDirPath).filePath(QStringLiteral("EVA_WORK")));
    pushCandidate(applicationDirPath);
    pushCandidate(QDir::currentPath());

    constexpr qint64 kMaxAgentsBytes = 256 * 1024; // hard cap to avoid oversized prompts
    const QString targetName = QStringLiteral("AGENTS.md");

    for (const QString &root : searchRoots)
    {
        const QString candidatePath = QDir(root).filePath(targetName);
        QFileInfo info(candidatePath);
        if (!info.exists() || !info.isFile()) continue;
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QByteArray raw = file.read(kMaxAgentsBytes);
        if (raw.isEmpty()) continue;
        QString text = QString::fromUtf8(raw);
        QString trimmed = text.trimmed();
        if (trimmed.isEmpty()) continue;
        if (file.bytesAvailable() > 0)
        {
            trimmed.append(QStringLiteral("\n[AGENTS.md truncated at %1 bytes]").arg(kMaxAgentsBytes));
        }
        return trimmed;
    }

    return {};
}

QString Widget::create_engineer_info()
{
    QString engineer_info_ = promptx::engineerInfo();
    engineer_info_.replace("{engineer_system_info}", buildEngineerSystemDetails());
    const QString doctrine = loadAgentsGuidance();
    if (!doctrine.isEmpty())
    {
        engineer_info_.append(QStringLiteral("\n\n[AGENTS.md Guidance]\n%1").arg(doctrine));
    }
    return engineer_info_;
}

QString Widget::create_architect_info()
{
    QString architect_info = promptx::architectInfo();
    return architect_info;
}

QString Widget::create_engineer_proxy_prompt()
{
    // 专供工程师子智能体使用：强制下发完整工具清单与技能挂载提示
    QString prompt = promptx::extraPromptTemplate();
    prompt.replace(QStringLiteral("{OBSERVATION_STOPWORD}"), QStringLiteral(DEFAULT_OBSERVATION_STOPWORD));

    QStringList tools;
    tools << promptx::toolAnswer().text;
    tools << promptx::toolExecuteCommand().text;
    tools << promptx::toolReadFile().text;
    tools << promptx::toolSkillCall().text;
    tools << promptx::toolWriteFile().text;
    tools << promptx::toolReplaceInFile().text;
    tools << promptx::toolEditInFile().text;
    tools << promptx::toolListFiles().text;
    tools << promptx::toolSearchContent().text;
    tools << promptx::toolPtc().text;
    // 知识库工具只有在主 UI 勾选时才暴露
    if (date_ui && date_ui->knowledge_checkbox && date_ui->knowledge_checkbox->isChecked())
    {
        QString knowledgeText = promptx::toolKnowledge().text;
        knowledgeText.replace("{embeddingdb describe}", embeddingdb_describe);
        tools << knowledgeText;
    }

    QString skill_usage_block;
    if (skillManager && date_ui && date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked())
    {
        QString workspaceDisplay = engineerWorkDir;
        if (workspaceDisplay.isEmpty())
        {
            workspaceDisplay = QDir(applicationDirPath).filePath(QStringLiteral("EVA_WORK"));
        }
        workspaceDisplay = QDir::toNativeSeparators(QDir::cleanPath(workspaceDisplay));
        QString skillDisplayRoot;
        if (shouldUseDockerEnv())
        {
            workspaceDisplay = dockerSandboxStatus_.containerWorkdir.isEmpty() ? DockerSandbox::defaultContainerWorkdir()
                                                                              : dockerSandboxStatus_.containerWorkdir;
            skillDisplayRoot = dockerSandboxStatus_.skillsMountPoint.isEmpty() ? DockerSandbox::skillsMountPoint()
                                                                               : dockerSandboxStatus_.skillsMountPoint;
        }
        const QString skillBlock = skillManager->composePromptBlock(engineerWorkDir, true, workspaceDisplay, skillDisplayRoot);
        if (!skillBlock.isEmpty()) skill_usage_block = skillBlock;
    }

    prompt.replace("{available_tools_describe}", tools.join(QStringLiteral("\n\n")));
    const QString engineerInfo = create_engineer_info();
    if (!engineerInfo.isEmpty())
    {
        prompt.append(QStringLiteral("\n\n") + engineerInfo);
    }
    if (!skill_usage_block.isEmpty()) prompt.append(QStringLiteral("\n\n") + skill_usage_block);
    return prompt;
}

void Widget::setEngineerArchitectMode(bool enabled, bool persist)
{
    if (engineerArchitectMode_ == enabled) return;
    engineerArchitectMode_ = enabled;
    if (date_ui && date_ui->engineer_checkbox)
    {
        if (engineerCheckboxLabel_.isEmpty()) engineerCheckboxLabel_ = date_ui->engineer_checkbox->text();
        if (enabled)
            date_ui->engineer_checkbox->setText(QStringLiteral("系统架构师"));
        else
            date_ui->engineer_checkbox->setText(engineerCheckboxLabel_);
    }
    updateEngineerConsoleVisibility();
    ui_extra_prompt = create_extra_prompt();
    if (date_ui)
    {
        get_date(shouldApplySandboxNow());
        if (persist) auto_save_user();
    }
    if (enabled)
    {
        refreshEngineerPromptPreview();
    }
    else if (!engineerProxyRuntime_.active)
    {
        resetEngineerConsole();
    }
    const QString notice = enabled ? QStringLiteral("ui:系统架构师模式已开启") : QStringLiteral("ui:系统架构师模式已关闭");
    reflash_state(notice, SIGNAL_SIGNAL);
}

void Widget::onEngineerCheckboxContextMenuRequested(const QPoint &pos)
{
    if (!date_ui || !date_ui->engineer_checkbox) return;
    if (!date_ui->engineer_checkbox->isChecked()) return;
    QMenu menu(date_ui->engineer_checkbox);
    QAction *upgrade = menu.addAction(QStringLiteral("升级为系统架构师"));
    QAction *downgrade = menu.addAction(QStringLiteral("恢复系统工程师"));
    upgrade->setEnabled(!engineerArchitectMode_);
    downgrade->setEnabled(engineerArchitectMode_);
    const QPoint globalPos = date_ui->engineer_checkbox->mapToGlobal(pos);
    QAction *picked = menu.exec(globalPos);
    if (!picked) return;
    if (picked == upgrade)
        setEngineerArchitectMode(true);
    else if (picked == downgrade)
        setEngineerArchitectMode(false);
}

auto Widget::ensureEngineerSession(const QString &engineerId) -> QSharedPointer<EngineerSession>
{
    QString key = engineerId.trimmed();
    if (key.isEmpty()) key = QStringLiteral("default");
    auto it = engineerSessions_.find(key);
    if (it != engineerSessions_.end())
    {
        const int budget = qMax(0, ui_SETTINGS.nctx);
        if (it.value()->tokenBudget != budget)
        {
            it.value()->tokenBudget = budget;
            it.value()->usedTokens = qMin(it.value()->usedTokens, it.value()->tokenBudget);
        }
        it.value()->systemPrompt = create_engineer_proxy_prompt();
        return it.value();
    }
    auto session = QSharedPointer<EngineerSession>::create();
    session->id = key;
    session->systemPrompt = create_engineer_proxy_prompt();
    session->tokenBudget = qMax(0, ui_SETTINGS.nctx);
    session->usedTokens = 0;
    engineerSessions_.insert(key, session);
    return session;
}

void Widget::startEngineerProxyTool(const mcp::json &call)
{
    // Support OpenAI-style {name, arguments:{...}} payloads
    mcp::json args = call;
    if (call.contains("arguments"))
    {
        try
        {
            args = call.at("arguments");
        }
        catch (const std::exception &)
        {
            args = mcp::json();
        }
    }

    auto readArg = [](const mcp::json &obj, const std::string &key) -> QString {
        try
        {
            if (obj.contains(key) && obj.at(key).is_string())
            {
                return QString::fromStdString(obj.at(key).get<std::string>());
            }
        }
        catch (const std::exception &)
        {
        }
        return QString();
    };

    if (!isArchitectModeActive())
    {
        toolInvocationActive_ = false;
        engineerProxyOuterActive_ = false;
        tool_result = QStringLiteral("system_engineer_proxy 未启用。");
        ENDPOINT_DATA data = prepareEndpointData();
        currentTask_ = ConversationTask::ToolLoop;
        handleToolLoop(data);
        return;
    }
    if (engineerProxyOuterActive_)
    {
        toolInvocationActive_ = false;
        tool_result = QStringLiteral("已有系统工程师任务在执行，请稍候。");
        ENDPOINT_DATA data = prepareEndpointData();
        currentTask_ = ConversationTask::ToolLoop;
        handleToolLoop(data);
        return;
    }
    QString engineerId = readArg(args, "engineer_id");
    if (engineerId.isEmpty()) engineerId = readArg(args, "engineerId");
    QString task = readArg(args, "task");
    if (task.isEmpty()) task = readArg(args, "objective");
    engineerId = engineerId.trimmed();
    if (engineerId.isEmpty()) engineerId = QStringLiteral("default");
    task = task.trimmed();
    if (task.isEmpty())
    {
        toolInvocationActive_ = false;
        tool_result = QStringLiteral("system_engineer_proxy 任务描述为空，无法执行。");
        ENDPOINT_DATA data = prepareEndpointData();
        currentTask_ = ConversationTask::ToolLoop;
        handleToolLoop(data);
        return;
    }
    const auto session = ensureEngineerSession(engineerId);
    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral(DEFAULT_USER_NAME));
    userMsg.insert(QStringLiteral("content"), task);
    session->messages.append(userMsg);
    engineerProxyRuntime_.active = true;
    engineerProxyRuntime_.session = session;
    engineerProxyRuntime_.engineerId = engineerId;
    engineerProxyRuntime_.task = task;
    engineerProxyRuntime_.streamBuffer.clear();
    engineerProxyRuntime_.waitingToolResult = false;
    engineerProxyRuntime_.lastAssistantText.clear();
    engineerProxyRuntime_.lastReasoningText.clear();
    engineerProxyRuntime_.lastPromptTokens = 0;
    engineerProxyRuntime_.lastGeneratedTokens = 0;
    engineerProxyRuntime_.lastReasoningTokens = 0;
    engineerProxyOuterActive_ = true;
    toolInvocationActive_ = true;
    resetEngineerConsole();
    resetEngineerStreamState();
    const QString systemPrompt = create_engineer_proxy_prompt();
    engineerProxyRuntime_.session->systemPrompt = systemPrompt;
    const QString labelSystem = jtr("role_system").isEmpty() ? QStringLiteral("system") : jtr("role_system");
    const QString labelUser = jtr("role_user").isEmpty() ? QStringLiteral("user") : jtr("role_user");
    appendEngineerRoleBlock(labelSystem, systemPrompt);
    appendEngineerRoleBlock(labelUser, task);
    sendEngineerProxyRequest(QStringLiteral("task dispatched"));
}

void Widget::sendEngineerProxyRequest(const QString &reason)
{
    if (!engineerProxyRuntime_.session) return;
    engineerProxyRuntime_.session->systemPrompt = create_engineer_proxy_prompt();
    resetEngineerStreamState();
    temp_assistant_history.clear();
    ENDPOINT_DATA data;
    data.date_prompt = engineerProxyRuntime_.session->systemPrompt;
    data.messagesArray = engineerProxyRuntime_.session->messages;
    data.is_complete_state = false;
    data.temp = ui_SETTINGS.temp;
    data.repeat = ui_SETTINGS.repeat;
    data.top_k = ui_SETTINGS.top_k;
    data.top_p = ui_SETTINGS.hid_top_p;
    data.n_predict = ui_SETTINGS.hid_npredict;
    data.stopwords = ui_DATES.extra_stop_words;
    data.id_slot = -1;
    emit_send(data);
    reflash_state(QStringLiteral("tool:system engineer -> %1").arg(reason), SIGNAL_SIGNAL);
}

void Widget::handleEngineerStreamOutput(const QString &chunk, bool streaming)
{
    Q_UNUSED(streaming);
    processEngineerStreamChunk(chunk);
}

void Widget::handleEngineerAssistantMessage(const QString &message, const QString &reasoning)
{
    if (!engineerProxyRuntime_.active || engineerProxyRuntime_.session.isNull())
    {
        return;
    }
    const QString labelThink = jtr("role_think").isEmpty() ? QStringLiteral("think") : jtr("role_think");
    const QString labelAssistant = jtr("role_model").isEmpty() ? QStringLiteral("model") : jtr("role_model");
    if (!reasoning.isEmpty())
    {
        if (!engineerThinkHeaderPrinted_)
        {
            appendEngineerRoleBlock(labelThink, QString());
            engineerThinkHeaderPrinted_ = true;
        }
        if (!engineerReasoningStreamSeen_)
        {
            appendEngineerText(reasoning, true);
            engineerReasoningStreamSeen_ = true;
            engineerReasoningNeedsLineBreak_ = false;
        }
        else if (engineerReasoningNeedsLineBreak_)
        {
            appendEngineerText(QString(), true);
            engineerReasoningNeedsLineBreak_ = false;
        }
    }
    if (!message.isEmpty())
    {
        if (!engineerAssistantHeaderPrinted_)
        {
            appendEngineerRoleBlock(labelAssistant, QString());
            engineerAssistantHeaderPrinted_ = true;
        }
        if (!engineerAssistantStreamSeen_)
        {
            appendEngineerText(message, true);
            engineerAssistantStreamSeen_ = true;
            engineerAssistantNeedsLineBreak_ = false;
        }
        else if (engineerAssistantNeedsLineBreak_)
        {
            appendEngineerText(QString(), true);
            engineerAssistantNeedsLineBreak_ = false;
        }
    }
    engineerThinkActive_ = false;
    QJsonObject assistant;
    assistant.insert(QStringLiteral("role"), QStringLiteral(DEFAULT_MODEL_NAME));
    assistant.insert(QStringLiteral("content"), message);
    if (!reasoning.isEmpty()) assistant.insert(QStringLiteral("reasoning_content"), reasoning);
    engineerProxyRuntime_.session->messages.append(assistant);
    engineerProxyRuntime_.lastAssistantText = message;
    engineerProxyRuntime_.lastReasoningText = reasoning;
    mcp::json call = XMLparser(message);
    if (call.empty() || !call.contains("name"))
    {
        finalizeEngineerProxy(message);
        return;
    }
    const QString toolName = QString::fromStdString(call.value("name", ""));
    if (toolName == QStringLiteral("answer") || toolName == QStringLiteral("response"))
    {
        finalizeEngineerProxy(message);
        return;
    }
    if (toolName == QStringLiteral("system_engineer_proxy"))
    {
        finalizeEngineerProxy(QStringLiteral("系统工程师不能嵌套调用自身。"));
        return;
    }
    // 下一次模型回复应重新打印“模型”标题，避免工具调用时只打印 JSON 而最终回答缺少头
    resetEngineerStreamState();
    engineerProxyRuntime_.waitingToolResult = true;
    emit ui2tool_exec(call);
}

void Widget::handleEngineerToolResult(const QString &result)
{
    if (!engineerProxyRuntime_.active || engineerProxyRuntime_.session.isNull())
    {
        return;
    }
    const QString labelTool = jtr("role_tool").isEmpty() ? QStringLiteral("tool") : jtr("role_tool");
    appendEngineerRoleBlock(labelTool, result);
    engineerProxyRuntime_.waitingToolResult = false;
    QJsonObject toolMsg;
    toolMsg.insert(QStringLiteral("role"), QStringLiteral("tool"));
    toolMsg.insert(QStringLiteral("content"), result);
    engineerProxyRuntime_.session->messages.append(toolMsg);
    sendEngineerProxyRequest(QStringLiteral("tool response"));
    toolInvocationActive_ = true;
}

void Widget::finalizeEngineerProxy(const QString &assistantText)
{
    if (!engineerProxyRuntime_.session)
    {
        engineerProxyRuntime_.active = false;
        engineerProxyOuterActive_ = false;
        toolInvocationActive_ = false;
        return;
    }
    // 若最终回复未在窗口中打出“模型”标题，则在收尾前补齐
    const QString labelThink = jtr("role_think").isEmpty() ? QStringLiteral("think") : jtr("role_think");
    const QString labelAssistant = jtr("role_model").isEmpty() ? QStringLiteral("model") : jtr("role_model");
    if (!engineerProxyRuntime_.lastReasoningText.isEmpty() && !engineerThinkHeaderPrinted_)
    {
        appendEngineerRoleBlock(labelThink, QString());
        engineerThinkHeaderPrinted_ = true;
        appendEngineerText(engineerProxyRuntime_.lastReasoningText, true);
    }
    if (!engineerProxyRuntime_.lastAssistantText.isEmpty() && !engineerAssistantHeaderPrinted_)
    {
        appendEngineerRoleBlock(labelAssistant, QString());
        engineerAssistantHeaderPrinted_ = true;
        appendEngineerText(engineerProxyRuntime_.lastAssistantText, true);
    }
    // 如果上层直接传入 assistantText 而未记录，也补打一遍
    if (engineerProxyRuntime_.lastAssistantText.isEmpty() && !assistantText.isEmpty())
    {
        appendEngineerRoleBlock(labelAssistant, assistantText);
        engineerAssistantHeaderPrinted_ = true;
    }
    auto session = engineerProxyRuntime_.session;
    const int delta = qMax(0, engineerProxyRuntime_.lastPromptTokens + engineerProxyRuntime_.lastGeneratedTokens +
                                 engineerProxyRuntime_.lastReasoningTokens);
    if (session->tokenBudget <= 0) session->tokenBudget = qMax(0, ui_SETTINGS.nctx);
    session->usedTokens = qBound(0, session->usedTokens + delta, qMax(session->tokenBudget, delta));
    const QString response = formatEngineerProxyResult(session, assistantText);
    engineerProxyRuntime_.session.clear();
    engineerProxyRuntime_.active = false;
    engineerProxyRuntime_.waitingToolResult = false;
    engineerProxyOuterActive_ = false;
    toolInvocationActive_ = false;
    engineerProxyRuntime_.engineerId.clear();
    engineerProxyRuntime_.task.clear();
    engineerProxyRuntime_.lastPromptTokens = 0;
    engineerProxyRuntime_.lastGeneratedTokens = 0;
    engineerProxyRuntime_.lastReasoningTokens = 0;
    tool_result = response;
    ENDPOINT_DATA data = prepareEndpointData();
    currentTask_ = ConversationTask::ToolLoop;
    handleToolLoop(data);
}

void Widget::cancelEngineerProxy(const QString &reason)
{
    if (!engineerProxyOuterActive_) return;
    engineerProxyRuntime_.active = false;
    engineerProxyRuntime_.waitingToolResult = false;
    engineerProxyRuntime_.session.clear();
    engineerProxyOuterActive_ = false;
    engineerProxyRuntime_.engineerId.clear();
    engineerProxyRuntime_.task.clear();
    appendEngineerConsole(QStringLiteral("系统架构师取消：%1").arg(reason), false);
    emit ui2net_stop(true);
    toolInvocationActive_ = false;
}

QString Widget::formatEngineerProxyResult(const QSharedPointer<EngineerSession> &session, const QString &assistantText)
{
    const int budget = session ? session->tokenBudget : ui_SETTINGS.nctx;
    const int used = session ? session->usedTokens : 0;
    Q_UNUSED(budget);
    Q_UNUSED(used);
    const QString summary = assistantText;
    QString engineerId = engineerProxyRuntime_.engineerId;
    if (engineerId.isEmpty() && session) engineerId = session->id;
    if (engineerId.isEmpty()) engineerId = QStringLiteral("default");
    return QStringLiteral("工程师[%1]\n结果：%2").arg(engineerId, summary);
}

void Widget::recordEngineerUsage(int promptTokens, int generatedTokens)
{
    if (!engineerProxyRuntime_.active) return;
    engineerProxyRuntime_.lastPromptTokens = promptTokens;
    engineerProxyRuntime_.lastGeneratedTokens = generatedTokens;
}

void Widget::recordEngineerReasoning(int tokens)
{
    if (!engineerProxyRuntime_.active) return;
    engineerProxyRuntime_.lastReasoningTokens = tokens;
}

Widget::EngineerEnvSnapshot Widget::collectEngineerEnvSnapshot()
{
    EngineerEnvSnapshot snapshot;
    // 暂停环境探测（Python/编译器/Node），避免执行外部检测命令。
    // snapshot.python = checkPython();
    // snapshot.compile = checkCompile();
    // snapshot.node = checkNode();
    return snapshot;
}

void Widget::refreshEngineerPromptBlock()
{
    if (!date_ui || !date_ui->engineer_checkbox || !date_ui->engineer_checkbox->isChecked()) return;
    ui_extra_prompt = create_extra_prompt();
    for (auto it = engineerSessions_.begin(); it != engineerSessions_.end(); ++it)
    {
        if (!it.value()) continue;
        it.value()->systemPrompt = create_engineer_proxy_prompt();
    }
    get_date(shouldApplySandboxNow());

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
    refreshEngineerPromptPreview();
}

void Widget::refreshEngineerPromptPreview()
{
    if (!isArchitectModeActive()) return;
    if (engineerProxyRuntime_.active) return;
    if (!ui || !ui->engineerConsole) return;
    QTextDocument *doc = ui->engineerConsole->document();
    const bool docEmpty = !doc || doc->isEmpty();
    if (!docEmpty && !engineerConsolePreviewActive_) return;

    const QString labelSystem = jtr("role_system").isEmpty() ? QStringLiteral("system") : jtr("role_system");
    const QString prompt = create_engineer_proxy_prompt();
    resetEngineerConsole();
    appendEngineerRoleBlock(labelSystem, prompt);
    engineerConsolePreviewActive_ = true;
}

void Widget::markEngineerEnvDirty()
{
    if (!ui_engineer_ischecked) return;
    engineerEnvReady_ = false;
    engineerEnvSummaryPending_ = true;
}

void Widget::markEngineerSandboxDirty()
{
    engineerSandboxDirty_ = true;
}

void Widget::markEngineerWorkDirPending()
{
    engineerWorkDirPendingApply_ = true;
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
    const bool waitForDocker = requireDockerReady &&
                               (engineerDockerLaunchPending_ || engineerSandboxDirty_ || engineerWorkDirPendingApply_);
    if (waitForDocker)
    {
        engineerDockerReady_ = false;
    }
    else
    {
        engineerDockerReady_ = true;
    }
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
    ensureSystemHeader(ui_DATES.date_prompt);
    const bool canLogNow = !ui_dockerSandboxEnabled || engineerDockerReady_;
    if (canLogNow)
    {
        if (engineerEnvSummaryPending_)
        {
            engineerEnvSummaryPending_ = false;
            logEngineerEnvSummary();
        }
    }
    else
    {
        engineerEnvSummaryPending_ = true;
    }
}

void Widget::setEngineerWorkDirSilently(const QString &dir)
{
    if (dir.isEmpty()) return;
    engineerWorkDir = QDir::cleanPath(dir);
    dockerMountPromptedContainers_.clear();
    invalidateWorkspaceSnapshotCache();
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
    invalidateWorkspaceSnapshotCache();
    markEngineerEnvDirty();
    engineerWorkDirPendingApply_ = false;
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
        skillsRoot = QDir(applicationDirPath).filePath(QStringLiteral(EVA_SKILLS_DIR_RELATIVE));
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
    engineerDockerLaunchPending_ = cfg.enabled;
    if (!cfg.enabled)
    {
        engineerDockerReady_ = true;
    }
    emit ui2tool_dockerConfigChanged(cfg);
    engineerSandboxDirty_ = false;
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

void Widget::logEngineerEnvSummary()
{
    QStringList segments;
    auto appendField = [&](const QString &label, const QString &value) {
        const QString normalized = value.trimmed().isEmpty() ? QStringLiteral("n/a") : value.trimmed();
        segments << QStringLiteral("%1=%2").arg(label, normalized);
    };
    appendField(QStringLiteral("Python"), python_env);
    appendField(QStringLiteral("Compile"), compile_env);
    appendField(QStringLiteral("Node"), node_env);
    const QString workdirDisplay = engineerWorkDir.isEmpty() ? jtr("engineer workdir") : QDir::toNativeSeparators(engineerWorkDir);
    segments << QStringLiteral("Workdir=%1").arg(workdirDisplay);
    if (ui_dockerSandboxEnabled)
    {
        const QString dockerLabel = dockerSandboxStatus_.ready ? QStringLiteral("Docker ready") : QStringLiteral("Docker pending");
        const QString dockerName = dockerSandboxDisplayName();
        segments << QStringLiteral("%1 (%2)").arg(dockerLabel, dockerName.isEmpty() ? QStringLiteral("n/a") : dockerName);
    }
    qInfo().noquote() << "[engineer-env]" << segments.join(QStringLiteral(" | "));
}
