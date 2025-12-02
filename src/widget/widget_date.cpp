#include "ui_widget.h"
#include "widget.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QHash>
#include "../utils/processrunner.h"

//-------------------------------------------------------------------------
//--------------------------------约定选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::set_DateDialog()
{
    // 初始化约定窗口
    date_dialog = new QDialog(this);
    date_dialog->setWindowTitle(jtr("date"));
    date_dialog->setWindowFlags(date_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint); // 隐藏?按钮
    date_ui = new Ui::Date_Dialog_Ui;
    date_ui->setupUi(date_dialog);
    for (const QString &key : date_map.keys())
    {
        date_ui->chattemplate_comboBox->addItem(key);
    }
    date_ui->chattemplate_comboBox->addItem(jtr("custom set1")); // 添加自定义模板
    date_ui->chattemplate_comboBox->addItem(jtr("custom set2")); // 添加自定义模板
    date_ui->chattemplate_comboBox->setCurrentText(ui_template); // 默认使用default的提示词模板
    connect(date_ui->chattemplate_comboBox, &QComboBox::currentTextChanged, this, &Widget::prompt_template_change);
    connect(date_ui->confirm_button, &QPushButton::clicked, this, &Widget::date_ui_confirm_button_clicked);
    connect(date_ui->cancel_button, &QPushButton::clicked, this, &Widget::date_ui_cancel_button_clicked);
    connect(date_ui->switch_lan_button, &QPushButton::clicked, this, &Widget::switch_lan_change);
    connect(date_ui->knowledge_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);       // 点击工具响应
    connect(date_ui->stablediffusion_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); // 点击工具响应
    connect(date_ui->calculator_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);      // 点击工具响应
    connect(date_ui->controller_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);      // 点击工具响应
    connect(date_ui->MCPtools_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);        // 点击工具响应
    connect(date_ui->engineer_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change);        // 点击工具响应
    if (date_ui->engineer_checkbox)
    {
        if (engineerCheckboxLabel_.isEmpty()) engineerCheckboxLabel_ = date_ui->engineer_checkbox->text();
        date_ui->engineer_checkbox->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(date_ui->engineer_checkbox, &QCheckBox::customContextMenuRequested, this, &Widget::onEngineerCheckboxContextMenuRequested);
        if (engineerArchitectMode_) date_ui->engineer_checkbox->setText(QStringLiteral("系统架构师"));
    }
    connect(date_dialog, &QDialog::rejected, this, &Widget::onDateDialogRejected);
    const auto autosave = [this]()
    { get_date(false); auto_save_user(); };
    if (date_ui->docker_target_comboBox)
    {
        date_ui->docker_target_comboBox->setVisible(false);
        date_ui->docker_target_comboBox->clear();
        date_ui->docker_target_comboBox->addItem(jtr("docker target option none"), static_cast<int>(DockerTargetMode::None));
        date_ui->docker_target_comboBox->addItem(jtr("docker target option image"), static_cast<int>(DockerTargetMode::Image));
        date_ui->docker_target_comboBox->addItem(jtr("docker target option container"), static_cast<int>(DockerTargetMode::Container));
        const int currentIndex = date_ui->docker_target_comboBox->findData(static_cast<int>(dockerTargetMode_));
        if (currentIndex >= 0) date_ui->docker_target_comboBox->setCurrentIndex(currentIndex);
        connect(date_ui->docker_target_comboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [=](int index)
                {
            const QVariant data = date_ui->docker_target_comboBox->itemData(index);
            if (!data.isValid()) return;
            const auto mode = static_cast<DockerTargetMode>(data.toInt());
            applyDockerTargetMode(mode, false, false);
            autosave(); });
    }
    if (date_ui->docker_image_comboBox)
    {
        date_ui->docker_image_comboBox->setVisible(false);
        date_ui->docker_image_comboBox->setEditable(true);
        date_ui->docker_image_comboBox->setInsertPolicy(QComboBox::NoInsert);
        updateDockerImageCombo();
        connect(date_ui->docker_image_comboBox, &QComboBox::editTextChanged, this, [=](const QString &)
                {
            updateDockerComboToolTip();
            autosave(); });
        connect(date_ui->docker_image_comboBox, &QComboBox::currentTextChanged, this, [=](const QString &)
                {
            updateDockerComboToolTip();
            autosave(); });
    }

    if (language_flag == 0)
    {
        ui_extra_lan = "zh";
    }
    if (language_flag == 1)
    {
        ui_extra_lan = "en";
    }

    prompt_template_change(); // ��Ӧ����ʾ��ģ��    // Auto-save on template/tool toggles (no reset)
    connect(date_ui->chattemplate_comboBox, &QComboBox::currentTextChanged, this, [=](const QString &)
            { autosave(); });
    connect(date_ui->knowledge_checkbox, &QCheckBox::stateChanged, this, [=](int)
            { autosave(); });
    connect(date_ui->stablediffusion_checkbox, &QCheckBox::stateChanged, this, [=](int)
            { autosave(); });
    connect(date_ui->calculator_checkbox, &QCheckBox::stateChanged, this, [=](int)
            { autosave(); });
    connect(date_ui->controller_checkbox, &QCheckBox::stateChanged, this, [=](int)
            { autosave(); });
    connect(date_ui->MCPtools_checkbox, &QCheckBox::stateChanged, this, [=](int)
            { autosave(); });
        connect(date_ui->engineer_checkbox, &QCheckBox::stateChanged, this, [=](int)
                {
        updateSkillVisibility(date_ui->engineer_checkbox->isChecked());
        if (date_ui->engineer_checkbox->isChecked())
        {
            refreshDockerImageList();
            if (dockerTargetMode_ == DockerTargetMode::Container) refreshDockerContainerList();
        }
        else
        {
            dockerImagesFetched_ = false;
            dockerImageList_.clear();
            dockerContainersFetched_ = false;
            dockerContainerList_.clear();
            dockerContainerTooltips_.clear();
            updateDockerImageCombo();
        }
        autosave(); });
    if (date_ui->skills_list)
    {
        connect(date_ui->skills_list, &SkillDropArea::skillDropRequested, this, &Widget::onSkillDropRequested);
        connect(date_ui->skills_list, &SkillDropArea::skillToggleRequested, this, &Widget::onSkillToggleRequested);
        connect(date_ui->skills_list, &SkillDropArea::skillRemoveRequested, this, &Widget::onSkillRemoveRequested);
    }
    if (ui_engineer_ischecked)
    {
        refreshDockerImageList(true);
        if (dockerTargetMode_ == DockerTargetMode::Container)
            refreshDockerContainerList(true);
    }
    else
    {
        updateDockerImageCombo();
    }
    // 工程师工作目录（默认隐藏，仅在勾选“系统工程师”后显示）
    if (date_ui->date_engineer_workdir_label)
    {
        date_ui->date_engineer_workdir_label->setText(jtr("work dir"));
        date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
        date_ui->date_engineer_workdir_LineEdit->setToolTip(jtr("engineer workdir tooltip"));
        date_ui->date_engineer_workdir_label->setVisible(false);
        date_ui->date_engineer_workdir_LineEdit->setVisible(false);
        date_ui->date_engineer_workdir_browse->setVisible(false);
        connect(date_ui->date_engineer_workdir_browse, &QPushButton::clicked, this, [this]()
                {
            const QString startDir = engineerWorkDir.isEmpty() ? QDir(applicationDirPath).filePath("EVA_WORK") : engineerWorkDir;
            QString picked = QFileDialog::getExistingDirectory(this, jtr("choose work dir"), startDir,
                                                               QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
            if (!picked.isEmpty())
            {
                setEngineerWorkDirSilently(picked);
                markEngineerWorkDirPending();
                markEngineerSandboxDirty();
                date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
                auto_save_user();
            } });
    }
    updateSkillVisibility(date_ui->engineer_checkbox->isChecked());
    refreshSkillsUI();
}

// 约定选项卡确认按钮响应
void Widget::date_ui_confirm_button_clicked()
{
    set_date();
}

// 约定选项卡取消按钮响应
void Widget::date_ui_cancel_button_clicked()
{
    if (date_dialog)
    {
        date_dialog->reject();
    }
}


void Widget::on_date_clicked()
{
    // reflash_state("ui:" + jtr("clicked date"), SIGNAL_SIGNAL);
    if (isControllerActive())
    {
        releaseControl();
        return;
    }

    // 展示最近一次设置值
    date_ui->chattemplate_comboBox->setCurrentText(ui_template); // 默认使用default的提示词模板
    date_ui->date_prompt_TextEdit->setPlainText(ui_date_prompt);

    date_ui->calculator_checkbox->setChecked(ui_calculator_ischecked);
    date_ui->knowledge_checkbox->setChecked(ui_knowledge_ischecked);
    date_ui->stablediffusion_checkbox->setChecked(ui_stablediffusion_ischecked);
    date_ui->controller_checkbox->setChecked(ui_controller_ischecked);
    date_ui->MCPtools_checkbox->setChecked(ui_MCPtools_ischecked);
    date_ui->engineer_checkbox->setChecked(ui_engineer_ischecked);
    if (date_ui->docker_target_comboBox)
    {
        QSignalBlocker blocker(date_ui->docker_target_comboBox);
        const int idx = date_ui->docker_target_comboBox->findData(static_cast<int>(dockerTargetMode_));
        if (idx >= 0) date_ui->docker_target_comboBox->setCurrentIndex(idx);
    }
    if (ui_engineer_ischecked)
    {
        refreshDockerImageList(true);
        if (dockerTargetMode_ == DockerTargetMode::Container) refreshDockerContainerList(true);
    }
    if (date_ui->docker_image_comboBox) updateDockerImageCombo();
    if (date_ui->date_engineer_workdir_LineEdit)
    {
        date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
        const bool vis = date_ui->engineer_checkbox->isChecked();
        date_ui->date_engineer_workdir_label->setVisible(vis);
        date_ui->date_engineer_workdir_LineEdit->setVisible(vis);
        date_ui->date_engineer_workdir_browse->setVisible(vis);
        if (date_ui->docker_target_comboBox) date_ui->docker_target_comboBox->setVisible(vis);
        if (date_ui->docker_image_comboBox) date_ui->docker_image_comboBox->setVisible(vis);
        updateSkillVisibility(vis);
        if (vis) refreshSkillsUI();
    }

    date_ui->switch_lan_button->setText(ui_extra_lan);

    captureDateDialogSnapshot();

    date_dialog->exec();
}

void Widget::captureDateDialogSnapshot()
{
    DateDialogState state;
    state.ui_template = ui_template;
    state.ui_extra_lan = ui_extra_lan;
    state.ui_extra_prompt = ui_extra_prompt;
    state.ui_date_prompt = ui_date_prompt;
    state.ui_dates = ui_DATES;
    state.ui_calculator_ischecked = ui_calculator_ischecked;
    state.ui_knowledge_ischecked = ui_knowledge_ischecked;
    state.ui_stablediffusion_ischecked = ui_stablediffusion_ischecked;
    state.ui_controller_ischecked = ui_controller_ischecked;
    state.ui_MCPtools_ischecked = ui_MCPtools_ischecked;
    state.ui_engineer_ischecked = ui_engineer_ischecked;
    state.ui_dockerSandboxEnabled = ui_dockerSandboxEnabled;
    state.engineerDockerImage = engineerDockerImage;
    state.engineerDockerContainer = engineerDockerContainer;
    state.dockerTargetMode = dockerTargetMode_;
    state.is_load_tool = is_load_tool;
    state.engineerWorkDir = engineerWorkDir;
    state.language_flag = language_flag;
    if (skillManager)
    {
        state.enabledSkills = skillManager->enabledSkillIds();
    }
    dateDialogSnapshot_ = state;
}

void Widget::set_date()
{
    if (date_dialog && date_dialog->isVisible())
    {
        date_dialog->accept();
    }

    // 如果用户在“约定”对话框中修改了工程师工作目录，点击“确定”后立即生效
    // 仅当已勾选“系统工程师”工具时才向 xTool 下发（未勾选时仅保存值供下次使用）
    if (date_ui && date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked() && date_ui->date_engineer_workdir_LineEdit)
    {
        const QString typed = date_ui->date_engineer_workdir_LineEdit->text().trimmed();
        if (!typed.isEmpty())
        {
            const QString norm = QDir::cleanPath(typed);
            const bool needsApply = engineerWorkDirPendingApply_ || (norm != engineerWorkDir);
            if (needsApply)
            {
                // setEngineerWorkDir 会更新成员变量、同步到 xTool，并刷新行编辑显示
                setEngineerWorkDir(norm);
            }
        }
    }

    // 同步其余“约定”参数到内存
    get_date(); // 获取约定中的纸面值
    updateSkillVisibility(ui_engineer_ischecked);
    if (ui_engineer_ischecked) refreshSkillsUI();
    updateEngineerConsoleVisibility();

    // 约定变化后统一重置对话上下文（本地/远端一致）并持久化
    auto_save_user(); // persist date settings
    dateDialogSnapshot_.reset();

    auto performReset = [this]()
    {
        on_reset_clicked();
        enforceEngineerEnvReadyCheckpoint();
    };
    queueEngineerGateAction(performReset, ui_engineer_ischecked && ui_dockerSandboxEnabled);
}

QString Widget::loadPersistedDockerImage() const
{
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    return settings.value("docker_sandbox_image").toString().trimmed();
}

QString Widget::loadPersistedDockerContainer() const
{
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    return sanitizeDockerContainerValue(settings.value("docker_sandbox_container").toString());
}

DockerTargetMode Widget::loadPersistedDockerMode() const
{
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    const QString stored = settings.value("docker_sandbox_mode").toString().trimmed().toLower();
    if (stored == QStringLiteral("container")) return DockerTargetMode::Container;
    if (stored == QStringLiteral("image")) return DockerTargetMode::Image;
    if (stored == QStringLiteral("none")) return DockerTargetMode::None;
    const bool legacyEnabled = settings.value("docker_sandbox_checkbox", false).toBool();
    return legacyEnabled ? DockerTargetMode::Image : DockerTargetMode::None;
}

QString Widget::sanitizeDockerContainerValue(const QString &value) const
{
    QString trimmed = value.trimmed();
    if (trimmed.compare(QStringLiteral("ubuntu:latest"), Qt::CaseInsensitive) == 0) return QString();
    return trimmed;
}

void Widget::refreshDockerImageList(bool force)
{
    if (!date_ui || !date_ui->docker_image_comboBox) return;
    if (!ui_engineer_ischecked)
    {
        updateDockerImageCombo();
        return;
    }
    if (dockerTargetMode_ == DockerTargetMode::None)
    {
        updateDockerImageCombo();
        return;
    }
    if (!force && dockerImagesFetched_)
    {
        updateDockerImageCombo();
        return;
    }
    QStringList foundImages;
    bool success = false;
    QString program = ProcessRunner::findExecutable(QStringLiteral("docker"));
#ifdef Q_OS_WIN
    if (program.isEmpty()) program = ProcessRunner::findExecutable(QStringLiteral("docker.exe"));
#endif
    qDebug() << "docker list program" << program;
    auto parseOutput = [&](const QString &text) {
        const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
        QSet<QString> seen;
        for (QString line : lines)
        {
            line = line.trimmed();
            if (line.isEmpty()) continue;
            if (line.contains(QStringLiteral("<none>"), Qt::CaseInsensitive)) continue;
            if (seen.contains(line)) continue;
            seen.insert(line);
            foundImages << line;
        }
        return !foundImages.isEmpty();
    };
    if (!program.isEmpty())
    {
        QStringList args{QStringLiteral("images"), QStringLiteral("--format"), QStringLiteral("{{.Repository}}:{{.Tag}}")};
        ProcessResult result = ProcessRunner::run(program, args, applicationDirPath, QProcessEnvironment::systemEnvironment(), 15000);
        if (!result.timedOut && result.exitCode == 0)
        {
            qDebug() << "docker images direct out" << result.stdOut << "err" << result.stdErr;
            success = parseOutput(result.stdOut);
        }
        else
        {
            qWarning() << "docker images direct failed" << result.exitCode << result.timedOut << result.stdErr;
        }
    }
    if (!success)
    {
        const QString shellCmd = QStringLiteral("docker images --format \"{{.Repository}}:{{.Tag}}\"");
        ProcessResult shellResult = ProcessRunner::runShellCommand(shellCmd, applicationDirPath, QProcessEnvironment::systemEnvironment(), 15000);
        if (!shellResult.timedOut && shellResult.exitCode == 0)
        {
            qDebug() << "docker images shell out" << shellResult.stdOut << "err" << shellResult.stdErr;
            foundImages.clear();
            success = parseOutput(shellResult.stdOut);
        }
        else
        {
            qWarning() << "docker images shell failed" << shellResult.exitCode << shellResult.timedOut << shellResult.stdErr;
        }
    }
    if (success)
    {
        dockerImagesFetched_ = true;
        dockerImageList_ = foundImages;
    }
    else if (force)
    {
        dockerImagesFetched_ = false;
        dockerImageList_.clear();
    }
    updateDockerImageCombo();
}

void Widget::refreshDockerContainerList(bool force)
{
    if (!date_ui || !date_ui->docker_image_comboBox) return;
    if (!ui_engineer_ischecked)
    {
        updateDockerImageCombo();
        return;
    }
    if (dockerTargetMode_ != DockerTargetMode::Container)
    {
        updateDockerImageCombo();
        return;
    }
    if (!force && dockerContainersFetched_)
    {
        updateDockerImageCombo();
        return;
    }
    QStringList foundContainers;
    QHash<QString, QString> tooltips;
    bool success = false;
    QString program = ProcessRunner::findExecutable(QStringLiteral("docker"));
#ifdef Q_OS_WIN
    if (program.isEmpty()) program = ProcessRunner::findExecutable(QStringLiteral("docker.exe"));
#endif
    auto parseOutput = [&](const QString &text) {
        const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
        QSet<QString> seen;
        for (QString line : lines)
        {
            line = line.trimmed();
            if (line.isEmpty()) continue;
            const QStringList parts = line.split(QLatin1Char('|'));
            const QString name = parts.value(0).trimmed();
            if (name.isEmpty()) continue;
            if (seen.contains(name)) continue;
            seen.insert(name);
            foundContainers << name;
            const QString image = parts.value(1).trimmed();
            const QString status = parts.value(2).trimmed();
            QString tooltip;
            if (!image.isEmpty()) tooltip += QStringLiteral("Image: %1").arg(image);
            if (!status.isEmpty())
            {
                if (!tooltip.isEmpty()) tooltip.append(QChar('\n'));
                tooltip += QStringLiteral("Status: %1").arg(status);
            }
            if (!tooltip.isEmpty()) tooltips.insert(name, tooltip);
        }
        return !foundContainers.isEmpty();
    };

    if (!program.isEmpty())
    {
        QStringList args{QStringLiteral("ps"), QStringLiteral("-a"), QStringLiteral("--format"), QStringLiteral("{{.Names}}|{{.Image}}|{{.Status}}")};
        ProcessResult result = ProcessRunner::run(program, args, applicationDirPath, QProcessEnvironment::systemEnvironment(), 15000);
        if (!result.timedOut && result.exitCode == 0)
        {
            success = parseOutput(result.stdOut);
        }
        else
        {
            qWarning() << "docker ps direct failed" << result.exitCode << result.timedOut << result.stdErr;
        }
    }
    if (!success)
    {
        const QString shellCmd = QStringLiteral("docker ps -a --format \"{{.Names}}|{{.Image}}|{{.Status}}\"");
        ProcessResult shellResult = ProcessRunner::runShellCommand(shellCmd, applicationDirPath, QProcessEnvironment::systemEnvironment(), 15000);
        if (!shellResult.timedOut && shellResult.exitCode == 0)
        {
            foundContainers.clear();
            tooltips.clear();
            success = parseOutput(shellResult.stdOut);
        }
        else
        {
            qWarning() << "docker ps shell failed" << shellResult.exitCode << shellResult.timedOut << shellResult.stdErr;
        }
    }

    if (success)
    {
        dockerContainersFetched_ = true;
        dockerContainerList_ = foundContainers;
        dockerContainerTooltips_ = tooltips;
    }
    else if (force)
    {
        dockerContainersFetched_ = false;
        dockerContainerList_.clear();
        dockerContainerTooltips_.clear();
    }
    updateDockerImageCombo();
}

void Widget::updateDockerImageCombo()
{
    if (!date_ui || !date_ui->docker_image_comboBox) return;
    QSignalBlocker blocker(date_ui->docker_image_comboBox);
    date_ui->docker_image_comboBox->clear();
    if (dockerTargetMode_ == DockerTargetMode::None)
    {
        date_ui->docker_image_comboBox->setEnabled(false);
        date_ui->docker_image_comboBox->setEditText(QString());
        updateDockerComboToolTip();
        return;
    }
    date_ui->docker_image_comboBox->setEnabled(true);

    if (dockerTargetMode_ == DockerTargetMode::Container)
    {
        QStringList ordered;
        auto addUnique = [&](const QString &value) {
            const QString trimmed = value.trimmed();
            if (trimmed.isEmpty()) return;
            if (ordered.contains(trimmed)) return;
            ordered << trimmed;
        };
        if (!dockerContainerList_.isEmpty())
        {
            for (const QString &name : dockerContainerList_) addUnique(name);
        }
        QString containerValue = sanitizeDockerContainerValue(engineerDockerContainer);
        if (containerValue.isEmpty())
        {
            const QString persisted = loadPersistedDockerContainer();
            if (!persisted.isEmpty())
            {
                engineerDockerContainer = persisted;
                containerValue = persisted.trimmed();
            }
        }
        if (!containerValue.isEmpty()) addUnique(containerValue);
        if (!ordered.isEmpty()) date_ui->docker_image_comboBox->addItems(ordered);
        for (int i = 0; i < ordered.size(); ++i)
        {
            const QString name = ordered.at(i);
            if (dockerContainerTooltips_.contains(name))
            {
                date_ui->docker_image_comboBox->setItemData(i, dockerContainerTooltips_.value(name), Qt::ToolTipRole);
            }
        }
        const bool showNoneSentinel = containerValue.isEmpty() && dockerNoneSentinelEnabled();
        const QString editValue = showNoneSentinel ? QStringLiteral("none") : containerValue;
        date_ui->docker_image_comboBox->setEditText(editValue);
    }
    else
    {
        if (engineerDockerImage.trimmed().isEmpty())
        {
            const QString persisted = loadPersistedDockerImage();
            if (!persisted.isEmpty()) engineerDockerImage = persisted;
        }
        QStringList ordered;
        auto addUniqueImage = [&](const QString &value) {
            const QString trimmed = value.trimmed();
            if (trimmed.isEmpty()) return;
            if (ordered.contains(trimmed)) return;
            ordered << trimmed;
        };
        addUniqueImage(QStringLiteral("ubuntu:latest"));
        for (const QString &img : dockerImageList_) addUniqueImage(img);
        if (!engineerDockerImage.trimmed().isEmpty()) addUniqueImage(engineerDockerImage);
        if (!ordered.isEmpty()) date_ui->docker_image_comboBox->addItems(ordered);
        for (int i = 0; i < ordered.size(); ++i)
        {
            date_ui->docker_image_comboBox->setItemData(i, ordered.at(i), Qt::ToolTipRole);
        }
        const QString target = engineerDockerImage.trimmed().isEmpty() ? QStringLiteral("ubuntu:latest") : engineerDockerImage.trimmed();
        date_ui->docker_image_comboBox->setEditText(target);
    }

    updateDockerComboToolTip();
}

void Widget::updateDockerComboToolTip()
{
    if (!date_ui || !date_ui->docker_image_comboBox) return;
    const QString text = date_ui->docker_image_comboBox->currentText().trimmed();
    QString tooltipText = text;
    QString placeholder;
    if (dockerTargetMode_ == DockerTargetMode::Container)
    {
        if (dockerContainerTooltips_.contains(text))
            tooltipText = dockerContainerTooltips_.value(text);
        else if (tooltipText.isEmpty())
            tooltipText = jtr("docker container tooltip");
        placeholder = jtr("docker container placeholder");
    }
    else if (dockerTargetMode_ == DockerTargetMode::Image)
    {
        placeholder = jtr("docker image placeholder");
        if (tooltipText.isEmpty()) tooltipText = jtr("docker image tooltip");
    }
    else
    {
        placeholder = jtr("docker target none placeholder");
        tooltipText = placeholder;
    }
    if (tooltipText.isEmpty()) tooltipText = text;
    date_ui->docker_image_comboBox->setToolTip(tooltipText);
    if (QLineEdit *edit = date_ui->docker_image_comboBox->lineEdit())
    {
        edit->setToolTip(tooltipText);
        edit->setPlaceholderText(placeholder);
    }
}

bool Widget::dockerNoneSentinelEnabled() const
{
    const QString sentinel = QStringLiteral("none");
    for (const QString &name : dockerContainerList_)
    {
        if (name.compare(sentinel, Qt::CaseInsensitive) == 0) return false;
    }
    return true;
}

bool Widget::isDockerNoneSentinel(const QString &text) const
{
    if (!dockerNoneSentinelEnabled()) return false;
    return text.trimmed().compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0;
}

void Widget::applyDockerTargetMode(DockerTargetMode mode, bool autosave, bool syncNow)
{
    ui_dockerSandboxEnabled = (mode != DockerTargetMode::None);
    if (dockerTargetMode_ == mode) return;
    dockerTargetMode_ = mode;
    dockerMountPromptedContainers_.clear();
    if (date_ui && date_ui->docker_target_comboBox)
    {
        QSignalBlocker blocker(date_ui->docker_target_comboBox);
        const int idx = date_ui->docker_target_comboBox->findData(static_cast<int>(mode));
        if (idx >= 0) date_ui->docker_target_comboBox->setCurrentIndex(idx);
    }
    if (mode == DockerTargetMode::Container)
    {
        if (ui_engineer_ischecked)
            refreshDockerContainerList(true);
        else
            updateDockerImageCombo();
    }
    else
    {
        updateDockerImageCombo();
    }
    if (autosave)
    {
        auto_save_user();
    }
    if (syncNow)
    {
        syncDockerSandboxConfig();
    }
    else
    {
        markEngineerSandboxDirty();
    }
}

void Widget::restoreDateDialogSnapshot()
{
    if (!date_ui)
    {
        return;
    }

    if (!dateDialogSnapshot_)
    {
        // 回落：使用当前已确认的状态刷新 UI
        if (date_ui->chattemplate_comboBox)
        {
            date_ui->chattemplate_comboBox->setCurrentText(ui_template);
        }
        if (date_ui->date_prompt_TextEdit)
        {
            date_ui->date_prompt_TextEdit->setPlainText(ui_date_prompt);
        }
        auto restoreCheck = [&](QCheckBox *box, bool desired) {
            if (!box) return;
            if (box->isChecked() != desired) box->setChecked(desired);
        };
        restoreCheck(date_ui->calculator_checkbox, ui_calculator_ischecked);
        restoreCheck(date_ui->controller_checkbox, ui_controller_ischecked);
        restoreCheck(date_ui->knowledge_checkbox, ui_knowledge_ischecked);
        restoreCheck(date_ui->stablediffusion_checkbox, ui_stablediffusion_ischecked);
        restoreCheck(date_ui->MCPtools_checkbox, ui_MCPtools_ischecked);
        restoreCheck(date_ui->engineer_checkbox, ui_engineer_ischecked);
        if (date_ui->date_engineer_workdir_LineEdit)
        {
            date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
        }
        if (date_ui->date_engineer_workdir_label)
        {
            const bool vis = ui_engineer_ischecked;
            date_ui->date_engineer_workdir_label->setVisible(vis);
            if (date_ui->date_engineer_workdir_LineEdit) date_ui->date_engineer_workdir_LineEdit->setVisible(vis);
            if (date_ui->date_engineer_workdir_browse) date_ui->date_engineer_workdir_browse->setVisible(vis);
        }
        if (date_ui->switch_lan_button)
        {
            date_ui->switch_lan_button->setText(ui_extra_lan);
        }
        updateSkillVisibility(ui_engineer_ischecked);
        if (ui_engineer_ischecked) refreshSkillsUI();

        apply_language(language_flag);
        emit ui2tool_language(language_flag);
        emit ui2net_language(language_flag);
        emit ui2expend_language(language_flag);

        ui_extra_prompt = create_extra_prompt();
        engineerSandboxDirty_ = false;
        engineerWorkDirPendingApply_ = false;
        auto_save_user();
        return;
    }

    const DateDialogState snapshot = *dateDialogSnapshot_;
    dateDialogSnapshot_.reset();

    ui_template = snapshot.ui_template;
    ui_date_prompt = snapshot.ui_date_prompt;
    ui_extra_lan = snapshot.ui_extra_lan;
    ui_DATES = snapshot.ui_dates;
    ui_calculator_ischecked = snapshot.ui_calculator_ischecked;
    ui_knowledge_ischecked = snapshot.ui_knowledge_ischecked;
    ui_stablediffusion_ischecked = snapshot.ui_stablediffusion_ischecked;
    ui_controller_ischecked = snapshot.ui_controller_ischecked;
    ui_MCPtools_ischecked = snapshot.ui_MCPtools_ischecked;
    ui_engineer_ischecked = snapshot.ui_engineer_ischecked;
    refreshWindowIcon();
    engineerDockerImage = snapshot.engineerDockerImage;
    engineerDockerContainer = snapshot.engineerDockerContainer;
    dockerTargetMode_ = snapshot.dockerTargetMode;
    ui_dockerSandboxEnabled = (dockerTargetMode_ != DockerTargetMode::None);
    language_flag = snapshot.language_flag;

    if (date_ui->chattemplate_comboBox && date_ui->chattemplate_comboBox->currentText() != snapshot.ui_template)
    {
        QSignalBlocker blocker(date_ui->chattemplate_comboBox);
        date_ui->chattemplate_comboBox->setCurrentText(snapshot.ui_template);
    }
    if (date_ui->date_prompt_TextEdit && date_ui->date_prompt_TextEdit->toPlainText() != snapshot.ui_date_prompt)
    {
        date_ui->date_prompt_TextEdit->setPlainText(snapshot.ui_date_prompt);
    }

    auto restoreCheck = [&](QCheckBox *box, bool desired) {
        if (!box) return;
        if (box->isChecked() != desired) box->setChecked(desired);
    };
    restoreCheck(date_ui->calculator_checkbox, snapshot.ui_calculator_ischecked);
    restoreCheck(date_ui->knowledge_checkbox, snapshot.ui_knowledge_ischecked);
    restoreCheck(date_ui->stablediffusion_checkbox, snapshot.ui_stablediffusion_ischecked);
    restoreCheck(date_ui->controller_checkbox, snapshot.ui_controller_ischecked);
    restoreCheck(date_ui->MCPtools_checkbox, snapshot.ui_MCPtools_ischecked);
    restoreCheck(date_ui->engineer_checkbox, snapshot.ui_engineer_ischecked);
    if (date_ui->docker_target_comboBox)
    {
        QSignalBlocker blocker(date_ui->docker_target_comboBox);
        const int idx = date_ui->docker_target_comboBox->findData(static_cast<int>(snapshot.dockerTargetMode));
        if (idx >= 0) date_ui->docker_target_comboBox->setCurrentIndex(idx);
    }
    if (date_ui->docker_image_comboBox)
    {
        engineerDockerImage = snapshot.engineerDockerImage;
        engineerDockerContainer = snapshot.engineerDockerContainer;
        updateDockerImageCombo();
    }

    if (date_ui->date_engineer_workdir_LineEdit)
    {
        date_ui->date_engineer_workdir_LineEdit->setText(snapshot.engineerWorkDir);
    }
    if (date_ui->date_engineer_workdir_label)
    {
        const bool vis = snapshot.ui_engineer_ischecked;
        date_ui->date_engineer_workdir_label->setVisible(vis);
        if (date_ui->date_engineer_workdir_LineEdit) date_ui->date_engineer_workdir_LineEdit->setVisible(vis);
        if (date_ui->date_engineer_workdir_browse) date_ui->date_engineer_workdir_browse->setVisible(vis);
        if (date_ui->docker_target_comboBox) date_ui->docker_target_comboBox->setVisible(vis);
        if (date_ui->docker_image_comboBox) date_ui->docker_image_comboBox->setVisible(vis);
    }
    if (date_ui->switch_lan_button)
    {
        date_ui->switch_lan_button->setText(snapshot.ui_extra_lan);
    }

    if (skillManager)
    {
        QSet<QString> enabledSet;
        for (const QString &id : snapshot.enabledSkills)
        {
            if (!id.isEmpty()) enabledSet.insert(id);
        }
        skillManager->restoreEnabledSet(enabledSet);
    }

    if (!snapshot.engineerWorkDir.isEmpty())
    {
        if (engineerWorkDir != snapshot.engineerWorkDir)
        {
            setEngineerWorkDir(snapshot.engineerWorkDir);
        }
        else
        {
            setEngineerWorkDirSilently(snapshot.engineerWorkDir);
        }
    }
    else
    {
        setEngineerWorkDirSilently(snapshot.engineerWorkDir);
    }

    updateSkillVisibility(snapshot.ui_engineer_ischecked);
    if (snapshot.ui_engineer_ischecked) refreshSkillsUI();

    apply_language(language_flag);
    emit ui2tool_language(language_flag);
    emit ui2net_language(language_flag);
    emit ui2expend_language(language_flag);

    is_load_tool = snapshot.is_load_tool;
    ui_extra_prompt = create_extra_prompt();
    ui_DATES = snapshot.ui_dates;

    engineerSandboxDirty_ = false;
    engineerWorkDirPendingApply_ = false;
    auto_save_user();
}

void Widget::onDateDialogRejected()
{
    cancel_date();
}

void Widget::cancel_date()
{
    restoreDateDialogSnapshot();
}

void Widget::on_set_clicked()
{
    if (isControllerActive())
    {
        releaseControl();
        return;
    }
    reflash_state("ui:" + jtr("clicked") + jtr("set"), SIGNAL_SIGNAL);
    if (ui_state == CHAT_STATE)
    {
        settings_ui->chat_btn->setChecked(1), chat_change();
    }
    else if (ui_state == COMPLETE_STATE)
    {
        settings_ui->complete_btn->setChecked(1), complete_change();
    }
    // 服务模式已移除
    // 展示最近一次设置值
    settings_ui->temp_slider->setValue(qRound(ui_SETTINGS.temp * 100.0));
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0));
    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
    settings_ui->nctx_slider->setValue(ui_SETTINGS.nctx);
    settings_ui->repeat_slider->setValue(qRound(ui_SETTINGS.repeat * 100.0));
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0));
    // Ensure top-k/top-p sliders reflect last confirmed settings on every open
    settings_ui->topk_slider->setValue(ui_SETTINGS.top_k);
    settings_ui->topp_slider->setValue(qRound(ui_SETTINGS.hid_top_p * 100.0));
    {
        const double val = settings_ui->topp_slider->value() / 100.0;
        settings_ui->topp_label->setText("TOP_P " + QString::number(val));
        settings_ui->topp_label->setToolTip(QString::fromUtf8("核采样阈值（top_p），范围 0.00–1.00；当前：%1").arg(QString::number(val, 'f', 2)));
    }
    if (settings_ui->reasoning_comboBox)
    {
        const QString normalized = sanitizeReasoningEffort(ui_SETTINGS.reasoning_effort);
        int idx = settings_ui->reasoning_comboBox->findData(normalized);
        if (idx < 0) idx = 0;
        settings_ui->reasoning_comboBox->setCurrentIndex(idx);
    }
    {
        const int cap = qMax(1, predictTokenCap());
        settings_ui->npredict_spin->setValue(qBound(-1, ui_SETTINGS.hid_npredict, cap));
    }
    npredict_change();
    settings_ui->lora_LineEdit->setText(ui_SETTINGS.lorapath);
    settings_ui->mmproj_LineEdit->setText(ui_SETTINGS.mmprojpath);
    settings_ui->nthread_slider->setValue(ui_SETTINGS.nthread);
    settings_ui->port_lineEdit->setText(ui_port);
    // 打开设置时记录当前设置快照，用于确认时判断是否有修改
    settings_snapshot_ = ui_SETTINGS;
    port_snapshot_ = ui_port;
    device_snapshot_ = settings_ui->device_comboBox->currentText().trimmed().toLower();
    backendOverrideSnapshot_ = DeviceManager::programOverrides();
    pendingBackendOverrides_ = backendOverrideSnapshot_;
    backendOverrideDirty_ = false;
    // Ensure device label reflects current auto->(effective) preview before showing the dialog
    refreshDeviceBackendUI();
    applySettingsDialogSizing();
    settings_dialog->exec();
}
void foo(){}
