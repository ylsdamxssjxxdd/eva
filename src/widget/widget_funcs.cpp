// 功能函数
#include "../utils/depresolver.h"
#include "../utils/processrunner.h"
#include "../utils/textparse.h"
#include "terminal_pane.h"
#include "ui_widget.h"
#include "widget.h"
#include "toolcall_test_dialog.h"
#include <QtGlobal>
#include <QDir>
#include <QFileInfo>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCheckBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QStringList>
#include <QSet>
#include <QProcessEnvironment>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGlobal>
#include <QVariant>
#include <algorithm>

// 添加右击问题
void Widget::create_right_menu()
{
    QDate currentDate = QDate::currentDate(); // 历史中的今天
    QString dateString = currentDate.toString("M" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    //---------------创建一般问题菜单--------------
    if (right_menu != nullptr)
    {
        delete right_menu;
    }
    right_menu = new QMenu(this);
    for (int i = 1; i < 14; ++i)
    {
        QString question;
        if (i == 4)
        {
            question = jtr(QString("Q%1").arg(i)).replace("{today}", dateString);
        } // 历史中的今天
        else
        {
            question = jtr(QString("Q%1").arg(i));
        }
        QAction *action = right_menu->addAction(question);
        connect(action, &QAction::triggered, this, [=]()
                { ui->input->textEdit->setPlainText(question); });
    }
    //------------创建自动化问题菜单-------------
    // 上传图像
    QAction *action14 = right_menu->addAction(jtr("Q14"));
    connect(action14, &QAction::triggered, this, [=]()
            {
        //用户选择图片
        QStringList paths = QFileDialog::getOpenFileNames(nullptr, jtr("Q14"), currentpath, "(*.png *.jpg *.bmp)");
        ui->input->addFiles(paths); });
    // 历史对话入口（打开管理界面）
    right_menu->addSeparator();
    QAction *histMgr = right_menu->addAction(jtr("history sessions"));
    connect(histMgr, &QAction::triggered, this, [this]()
            { openHistoryManager(); });
}
// 添加托盘右击事件
void Widget::create_tray_right_menu()
{
    trayMenu->clear();
    QAction *showAction_shortcut = trayMenu->addAction(jtr("shortcut"));
    QAction *blank1 = trayMenu->addAction(""); // 占位符
    QAction *blank2 = trayMenu->addAction(""); // 占位符，目的是把截图顶出去，用户点击后才会隐藏
    blank1->setEnabled(false);
    blank2->setEnabled(false);
    trayMenu->addSeparator(); // 添加分割线
    QAction *showAction_widget = trayMenu->addAction(jtr("show widget"));
    QAction *showAction_expend = trayMenu->addAction(jtr("show expend"));
    trayMenu->addSeparator(); // 添加分割线
    QAction *exitAction = trayMenu->addAction(jtr("quit"));
    QObject::connect(showAction_widget, &QAction::triggered, this, [&]()
                     {
                         toggleWindowVisibility(this, true); // 显示窗体
                     });
    QObject::connect(showAction_expend, &QAction::triggered, this, [&]()
                     { emit ui2expend_show(PREV_WINDOW); });
    QObject::connect(showAction_shortcut, &QAction::triggered, this, [&]()
                     {
                         trayMenu->hide();
                         QTimer::singleShot(100, [this]()
                                            { onShortcutActivated_F1(); }); // 触发截图
                     });
    QObject::connect(exitAction, &QAction::triggered, QApplication::quit); // 退出程序
}
// 获取设置中的纸面值
void Widget::get_set()
{
    ui_SETTINGS.temp = settings_ui->temp_slider->value() / 100.0;
    ui_SETTINGS.repeat = settings_ui->repeat_slider->value() / 100.0;
    ui_SETTINGS.hid_parallel = settings_ui->parallel_slider->value();
    ui_SETTINGS.top_k = settings_ui->topk_slider->value();
    ui_SETTINGS.hid_top_p = settings_ui->topp_slider->value() / 100.0;
    ui_SETTINGS.nthread = settings_ui->nthread_slider->value();
    ui_SETTINGS.nctx = settings_ui->nctx_slider->value(); // 获取nctx滑块的值
    ui_SETTINGS.ngl = settings_ui->ngl_slider->value();   // 获取ngl滑块的值
    ui_SETTINGS.lorapath = settings_ui->lora_LineEdit->text();
    ui_SETTINGS.mmprojpath = settings_ui->mmproj_LineEdit->text();
    const int newLazyMinutes = settings_ui->lazy_timeout_spin ? settings_ui->lazy_timeout_spin->value() : qMax(0, int(lazyUnloadMs_ / 60000));
    lazyUnloadMs_ = qMax(0, newLazyMinutes) * 60000;
    if (lazyUnloadMs_ <= 0)
    {
        lazyUnloaded_ = false;
        if (lazyUnloadTimer_) lazyUnloadTimer_->stop();
        if (lazyCountdownTimer_) lazyCountdownTimer_->stop();
    }
    else if (lazyUnloadTimer_)
    {
        if (backendOnline_ && !turnActive_ && !toolInvocationActive_)
        {
            lazyUnloadTimer_->start(lazyUnloadMs_);
        }
    }
    updateLazyCountdownLabel();
    ui_SETTINGS.complete_mode = settings_ui->complete_btn->isChecked();
    const int npredictValue = settings_ui->npredict_spin->value();
    if (npredictValue < 0)
        ui_SETTINGS.hid_npredict = -1;
    else
        ui_SETTINGS.hid_npredict = qMin(npredictValue, predictTokenCap());
    if (settings_ui->reasoning_comboBox)
    {
        ui_SETTINGS.reasoning_effort = sanitizeReasoningEffort(settings_ui->reasoning_comboBox->currentData().toString());
    }
    else
    {
        ui_SETTINGS.reasoning_effort = QStringLiteral("auto");
    }
    if (settings_ui->chat_btn->isChecked())
    {
        ui_state = CHAT_STATE;
    }
    else if (settings_ui->complete_btn->isChecked())
    {
        ui_state = COMPLETE_STATE;
    }
    // 服务状态已弃用
    ui_port = settings_ui->port_lineEdit->text();
    // 推理设备：同步到 DeviceManager（auto/cpu/cuda/vulkan/opencl）
    ui_device_backend = settings_ui->device_comboBox->currentText().trimmed().toLower();
    DeviceManager::setUserChoice(ui_device_backend);
}
// 获取约定中的纸面值
void Widget::get_date(bool applySandbox)
{
    const bool prevEngineerChecked = ui_engineer_ischecked;
    const bool prevSandboxEnabled = ui_dockerSandboxEnabled;
    const DockerTargetMode prevDockerMode = dockerTargetMode_;
    const QString prevDockerImage = engineerDockerImage;
    const QString prevDockerContainer = engineerDockerContainer;

    ui_date_prompt = date_ui->date_prompt_TextEdit->toPlainText();
    // 合并附加指令
    if (ui_extra_prompt != "")
    {
        ui_DATES.date_prompt = ui_date_prompt + "\n\n" + ui_extra_prompt;
    }
    else
    {
        ui_DATES.date_prompt = ui_date_prompt;
    }
    ui_DATES.is_load_tool = is_load_tool;
    ui_template = date_ui->chattemplate_comboBox->currentText();
    if (date_ui->toolcall_mode_comboBox)
    {
        const QVariant data = date_ui->toolcall_mode_comboBox->currentData();
        ui_tool_call_mode = data.isValid() ? data.toInt() : DEFAULT_TOOL_CALL_MODE;
    }
    // 额外指令语种：由下拉框提供（优先取 data，其次取 text）
    if (date_ui->switch_lan_button)
    {
        const QString data = date_ui->switch_lan_button->currentData().toString().trimmed();
        const QString text = date_ui->switch_lan_button->currentText().trimmed();
        ui_extra_lan = !data.isEmpty() ? data : text;
        if (ui_extra_lan.isEmpty()) ui_extra_lan = evaLanguageCodeFromFlag(language_flag);
    }
    ui_calculator_ischecked = date_ui->calculator_checkbox->isChecked();
    ui_engineer_ischecked = date_ui->engineer_checkbox->isChecked();
    ui_MCPtools_ischecked = date_ui->MCPtools_checkbox->isChecked();
    ui_knowledge_ischecked = date_ui->knowledge_checkbox->isChecked();
    ui_controller_ischecked = date_ui->controller_checkbox->isChecked();
    if (date_ui->controller_norm_x_spin)
    {
        ui_controller_norm_x = date_ui->controller_norm_x_spin->value();
    }
    if (date_ui->controller_norm_y_spin)
    {
        ui_controller_norm_y = date_ui->controller_norm_y_spin->value();
    }
    ui_stablediffusion_ischecked = date_ui->stablediffusion_checkbox->isChecked();
    // 同步给工具层：用于把模型输出的归一化坐标换算为真实屏幕坐标
    emit ui2tool_controllerNormalize(ui_controller_norm_x, ui_controller_norm_y);
    if (date_ui->docker_target_comboBox)
    {
        const QVariant modeData = date_ui->docker_target_comboBox->currentData();
        if (modeData.isValid()) dockerTargetMode_ = static_cast<DockerTargetMode>(modeData.toInt());
    }
    ui_dockerSandboxEnabled = (dockerTargetMode_ != DockerTargetMode::None);
    if (date_ui->docker_image_comboBox)
    {
        QString text = date_ui->docker_image_comboBox->currentText().trimmed();
        if (dockerTargetMode_ == DockerTargetMode::Container)
        {
            if (isDockerNoneSentinel(text)) text.clear();
            text = sanitizeDockerContainerValue(text);
            if (text != engineerDockerContainer)
            {
                dockerMountPromptedContainers_.clear();
                engineerDockerContainer = text;
            }
        }
        else if (dockerTargetMode_ == DockerTargetMode::Image)
        {
            engineerDockerImage = text;
        }
    }
    // 记录自定义模板
    if (ui_template == jtr("custom set1"))
    {
        custom1_date_system = ui_date_prompt;
    }
    else if (ui_template == jtr("custom set2"))
    {
        custom2_date_system = ui_date_prompt;
    }
    // 添加额外停止标志
    addStopwords();

    const bool sandboxConfigChanged = (prevEngineerChecked != ui_engineer_ischecked) ||
                                      (prevSandboxEnabled != ui_dockerSandboxEnabled) ||
                                      (prevDockerMode != dockerTargetMode_) ||
                                      (prevDockerImage != engineerDockerImage) ||
                                      (prevDockerContainer != engineerDockerContainer);

    if (!applySandbox && (sandboxConfigChanged || engineerWorkDirPendingApply_))
    {
        markEngineerSandboxDirty();
    }

    if (applySandbox)
    {
        syncDockerSandboxConfig();
    }
}
// 手搓输出解析器，提取可能的xml，目前只支持一个参数
mcp::json Widget::XMLparser(const QString &text, QStringList *debugLog)
{
    const auto logStep = [&](const QString &line)
    {
        if (debugLog) debugLog->append(line);
    };
    const auto logText = [&](const QString &key, const QString &fallback) -> QString {
        const QString textValue = jtr(key);
        return textValue.isEmpty() ? fallback : textValue;
    };

    QString payload = text;
    logStep(logText(QStringLiteral("toolcall xml log input length"), QStringLiteral("Input length: %1 chars")).arg(payload.size()));
    const int beforeStrip = payload.size();
    TextParse::stripTagBlocksCaseInsensitive(payload, QStringLiteral("think"));
    if (payload.size() != beforeStrip)
    {
        logStep(logText(QStringLiteral("toolcall xml log think stripped"), QStringLiteral("Removed <think> block, remaining length: %1"))
                    .arg(payload.size()));
    }

    QStringList tagCandidates;
    QSet<QString> deduped;
    const QStringList rawTags = TextParse::collectTagBlocks(payload, QStringLiteral("tool_call"));
    for (const QString &captured : rawTags)
    {
        const QString normalized = captured.trimmed();
        if (normalized.isEmpty()) continue;
        if (deduped.contains(normalized)) continue;
        deduped.insert(normalized);
        tagCandidates.append(captured);
    }
    logStep(logText(QStringLiteral("toolcall xml log tag count"), QStringLiteral("Found <tool_call> candidates: %1")).arg(tagCandidates.size()));

    mcp::json result; // empty by default
    auto parseCandidateList = [&](const QStringList &list, const QString &sourceLabel) -> bool
    {
        if (list.isEmpty())
        {
            logStep(logText(QStringLiteral("toolcall xml log list empty"), QStringLiteral("%1 candidates empty, skip.")).arg(sourceLabel));
            return false;
        }
        logStep(logText(QStringLiteral("toolcall xml log list try"), QStringLiteral("Trying %1 candidates: %2"))
                    .arg(sourceLabel)
                    .arg(list.size()));
        for (int idx = list.size() - 1; idx >= 0; --idx)
        {
            QString content = list.at(idx).trimmed();
            if (content.isEmpty())
            {
                logStep(logText(QStringLiteral("toolcall xml log slot empty"), QStringLiteral("%1 candidate #%2 empty"))
                            .arg(sourceLabel)
                            .arg(idx + 1));
                continue;
            }

            content = TextParse::stripCodeFenceMarkers(content);
            if (content.startsWith(QStringLiteral("json"), Qt::CaseInsensitive))
            {
                const int newlineIndex = content.indexOf('\n');
                if (newlineIndex > -1)
                {
                    content = content.mid(newlineIndex + 1).trimmed();
                }
                else
                {
                    content.clear();
                }
            }
            if (content.isEmpty())
            {
                continue;
            }

            QString candidateJson = content;
            const int firstBrace = candidateJson.indexOf('{');
            const int lastBrace = candidateJson.lastIndexOf('}');
            if (firstBrace == -1 || lastBrace <= firstBrace)
            {
                qWarning() << "tool_call candidate missing braces in" << sourceLabel << "slot" << idx;
                continue;
            }
            candidateJson = candidateJson.mid(firstBrace, lastBrace - firstBrace + 1).trimmed();
            if (candidateJson.isEmpty())
            {
                continue;
            }

            const QByteArray jsonBytes = candidateJson.toUtf8();
            try
            {
                result = mcp::json::parse(jsonBytes.constData(), jsonBytes.constData() + jsonBytes.size());
                qDebug() << "parsed tool_call from" << sourceLabel << "slot" << idx;
                logStep(logText(QStringLiteral("toolcall xml log parse success"), QStringLiteral("Parsed %1 candidate #%2"))
                            .arg(sourceLabel)
                            .arg(idx + 1));
                return true;
            }
            catch (const std::exception &e)
            {
                qCritical() << "tool JSON parse error from" << sourceLabel << "slot" << idx << ":" << e.what();
                logStep(logText(QStringLiteral("toolcall xml log parse error"), QStringLiteral("%1 candidate #%2 parse error: %3"))
                            .arg(sourceLabel)
                            .arg(idx + 1)
                            .arg(QString::fromUtf8(e.what())));
            }
        }
        return false;
    };

    if (parseCandidateList(tagCandidates, QStringLiteral("tagged")))
    {
        return result;
    }

    QStringList fallbackCandidates;
    const QStringList looseObjects = TextParse::collectLooseJsonObjects(payload);
    logStep(logText(QStringLiteral("toolcall xml log loose count"), QStringLiteral("Loose JSON fragments: %1")).arg(looseObjects.size()));
    for (const QString &obj : looseObjects)
    {
        const QString normalized = obj.trimmed();
        if (normalized.isEmpty()) continue;
        if (!normalized.contains(QStringLiteral("\"name\"")) || !normalized.contains(QStringLiteral("\"arguments\"")))
        {
            continue;
        }
        if (deduped.contains(normalized)) continue;
        deduped.insert(normalized);
        fallbackCandidates.append(obj);
    }

    if (!fallbackCandidates.isEmpty())
    {
        qWarning() << "tool_call tags missing or invalid, attempting loose JSON fallback";
        logStep(logText(QStringLiteral("toolcall xml log fallback attempt"),
                        QStringLiteral("Missing <tool_call> tags, trying loose candidates: %1"))
                    .arg(fallbackCandidates.size()));
        if (parseCandidateList(fallbackCandidates, QStringLiteral("fallback")))
        {
            return result;
        }
    }

    qDebug() << "no valid tool_call found";
    logStep(logText(QStringLiteral("toolcall xml log none found"), QStringLiteral("No usable tool-call snippet found.")));
    return result;
}
// 构建额外指令
QString Widget::create_extra_prompt()
{
    QString skill_usage_block;
    QString engineer_info; // 系统工程师信息
    QString wunder_prompt; // Wunder 系统提示词（仅在系统工程师启用时追加）

    if (!is_load_tool)
    {
        return ""; // 没有挂载工具则为空
    }

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

    if (date_ui && date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked())
    {
        wunder_prompt = promptx::wunderSystemPromptTemplate();
        engineer_info = isArchitectModeActive() ? create_architect_info() : create_engineer_info();
    }

    // function_call 模式：不注入工具提示词与工具清单，仅保留工程信息与技能提示
    if (ui_tool_call_mode == TOOL_CALL_FUNCTION)
    {
        QString extra_prompt_;
        if (!wunder_prompt.isEmpty())
        {
            extra_prompt_ = wunder_prompt;
        }
        if (!engineer_info.isEmpty())
        {
            if (!extra_prompt_.isEmpty()) extra_prompt_ += "\n\n";
            extra_prompt_ += engineer_info;
        }
        if (!skill_usage_block.isEmpty())
        {
            if (!extra_prompt_.isEmpty()) extra_prompt_ += "\n\n";
            extra_prompt_ += skill_usage_block;
        }
        return extra_prompt_;
    }

    QString extra_prompt_ = promptx::extraPromptTemplate();
    if (!wunder_prompt.isEmpty())
    {
        extra_prompt_ = wunder_prompt + "\n\n" + extra_prompt_;
    }
    QString available_tools_describe; // 工具名和描述
    extra_prompt_.replace("{OBSERVATION_STOPWORD}", DEFAULT_OBSERVATION_STOPWORD);
    available_tools_describe += promptx::toolAnswer().text + "\n\n";
    // qDebug()<< MCP_TOOLS_INFO_LIST.size();
    if (date_ui->MCPtools_checkbox->isChecked())
    {
        QStringList mcpToolEntries;
        for (const auto &info : MCP_TOOLS_INFO_LIST)
        {
            mcpToolEntries << info.text;
        }
        if (!mcpToolEntries.isEmpty())
        {
            available_tools_describe += mcpToolEntries.join("\n") + "\n\n";
        }
    }
    if (date_ui->calculator_checkbox->isChecked())
    {
        available_tools_describe += promptx::toolCalculator().text + "\n\n";
    }
    if (date_ui->knowledge_checkbox->isChecked())
    {
        QString knowledgeText = promptx::toolKnowledge().text;
        knowledgeText.replace("{embeddingdb describe}", embeddingdb_describe);
        available_tools_describe += knowledgeText + "\n\n";
    }
    if (date_ui->stablediffusion_checkbox->isChecked())
    {
        available_tools_describe += promptx::toolStableDiffusion().text + "\n\n";
    }
    if (date_ui->controller_checkbox->isChecked())
    {
        // 桌面控制器提示词：把“截图归一化尺寸”写进提示词里（用当前设置值替换占位符），避免每条消息都额外注入图片元信息文本。
        // 额外需求：当用户界面语言为中文时，要让模型在 controller.arguments.description 中使用中文描述动作，
        // 否则屏幕叠加提示会显示英文，用户看不懂也无法确认即将执行/已执行的动作。
        // 英文界面保持原提示词不变。
        TOOLS_INFO controllerInfo = promptx::toolController();
        controllerInfo.description.replace(QStringLiteral("{controller_norm_x}"), QString::number(ui_controller_norm_x));
        controllerInfo.description.replace(QStringLiteral("{controller_norm_y}"), QString::number(ui_controller_norm_y));
        if (language_flag == 0) // 0=中文，1=英文
        {
            controllerInfo.description += QStringLiteral(
                "\n- IMPORTANT (中文界面)：`description` 是用户可见的屏幕提示文本，必须用中文描述动作（例如：点击浏览器地址栏，在搜索框输入文本）。");
        }
        else if (language_flag == EVA_LANG_JA) // 日文界面
        {
            controllerInfo.description += QStringLiteral(
                "\n- IMPORTANT (日本語UI)：`description` はユーザーに表示される画面上の説明文です。必ず日本語で動作を説明してください（例：ブラウザーのアドレスバーをクリックし、検索欄にテキストを入力）。");
        }
        controllerInfo.generateToolText();
        available_tools_describe += controllerInfo.text + "\n\n";

        // 桌面监视器提示词：与 controller 配套使用，用“等待 + 截图”的观察回路解决需要持续监视屏幕变化的任务。
        // 监视器截图与 controller 截图采用同一套归一化坐标系，避免模型在 monitor->controller 切换时坐标空间不一致。
        TOOLS_INFO monitorInfo = promptx::toolMonitor();
        monitorInfo.description.replace(QStringLiteral("{controller_norm_x}"), QString::number(ui_controller_norm_x));
        monitorInfo.description.replace(QStringLiteral("{controller_norm_y}"), QString::number(ui_controller_norm_y));
        monitorInfo.generateToolText();
        available_tools_describe += monitorInfo.text + "\n\n";
    }
    if (date_ui->engineer_checkbox->isChecked())
    {
        if (isArchitectModeActive())
        {
            available_tools_describe += promptx::toolEngineerProxy().text + "\n\n";
        }
        else
        {
            available_tools_describe += promptx::toolExecuteCommand().text + "\n\n";
            available_tools_describe += promptx::toolReadFile().text + "\n\n";
            available_tools_describe += promptx::toolSkillCall().text + "\n\n";
            available_tools_describe += promptx::toolScheduleTask().text + "\n\n";
            available_tools_describe += promptx::toolWriteFile().text + "\n\n";
            available_tools_describe += promptx::toolReplaceInFile().text + "\n\n";
            available_tools_describe += promptx::toolEditInFile().text + "\n\n";
            available_tools_describe += promptx::toolListFiles().text + "\n\n";
            available_tools_describe += promptx::toolSearchContent().text + "\n\n";
            available_tools_describe += promptx::toolPtc().text + "\n\n";
        }
    }

    extra_prompt_.replace("{available_tools_describe}", available_tools_describe); // 替换相应内容
    if (!engineer_info.isEmpty())
    {
        extra_prompt_ += "\n\n" + engineer_info;
    }
    if (!skill_usage_block.isEmpty())
    {
        extra_prompt_ = extra_prompt_ + "\n\n" + skill_usage_block; // 技能描述放到最后
    }
    return extra_prompt_;
}

QJsonArray Widget::buildFunctionTools() const
{
    QJsonArray tools;
    if (!is_load_tool || !date_ui)
    {
        return tools;
    }

    auto defaultParams = []() -> QJsonObject {
        QJsonObject params;
        params.insert(QStringLiteral("type"), QStringLiteral("object"));
        params.insert(QStringLiteral("properties"), QJsonObject());
        return params;
    };

    auto sanitizeSchemaValue = [&](const QJsonValue &value, const auto &self) -> QJsonValue {
        if (value.isObject())
        {
            QJsonObject obj = value.toObject();
            // llama.cpp 的 JSON schema 转换器对 anyOf/oneOf 等关键字支持不完整，统一移除做兼容。
            obj.remove(QStringLiteral("$schema"));
            obj.remove(QStringLiteral("anyOf"));
            obj.remove(QStringLiteral("oneOf"));
            obj.remove(QStringLiteral("allOf"));
            obj.remove(QStringLiteral("not"));
            obj.remove(QStringLiteral("if"));
            obj.remove(QStringLiteral("then"));
            obj.remove(QStringLiteral("else"));
            obj.remove(QStringLiteral("additionalProperties"));
            for (auto it = obj.begin(); it != obj.end(); ++it)
            {
                if (it.value().isObject() || it.value().isArray())
                {
                    it.value() = self(it.value(), self);
                }
            }
            if (!obj.contains(QStringLiteral("type")))
            {
                if (obj.contains(QStringLiteral("properties")) || obj.contains(QStringLiteral("required")))
                {
                    obj.insert(QStringLiteral("type"), QStringLiteral("object"));
                }
                else if (obj.contains(QStringLiteral("items")))
                {
                    obj.insert(QStringLiteral("type"), QStringLiteral("array"));
                }
            }
            return obj;
        }
        if (value.isArray())
        {
            QJsonArray arr;
            for (const auto &item : value.toArray())
            {
                arr.append(self(item, self));
            }
            return arr;
        }
        return value;
    };

    auto parseParams = [&](const QString &raw) -> QJsonObject {
        if (raw.trimmed().isEmpty()) return defaultParams();
        QJsonParseError err{};
        QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
        {
            const QJsonValue sanitized = sanitizeSchemaValue(doc.object(), sanitizeSchemaValue);
            return sanitized.isObject() ? sanitized.toObject() : defaultParams();
        }
        return defaultParams();
    };

    auto appendTool = [&](const TOOLS_INFO &info) {
        const QString name = info.name.trimmed();
        if (name.isEmpty()) return;
        QJsonObject fn;
        fn.insert(QStringLiteral("name"), name);
        if (!info.description.trimmed().isEmpty())
        {
            fn.insert(QStringLiteral("description"), info.description);
        }
        fn.insert(QStringLiteral("parameters"), parseParams(info.arguments));
        QJsonObject item;
        item.insert(QStringLiteral("type"), QStringLiteral("function"));
        item.insert(QStringLiteral("function"), fn);
        tools.append(item);
    };

    // MCP 工具
    if (date_ui->MCPtools_checkbox && date_ui->MCPtools_checkbox->isChecked())
    {
        for (const auto &info : MCP_TOOLS_INFO_LIST)
        {
            appendTool(info);
        }
    }
    if (date_ui->calculator_checkbox && date_ui->calculator_checkbox->isChecked())
    {
        appendTool(promptx::toolCalculator());
    }
    if (date_ui->knowledge_checkbox && date_ui->knowledge_checkbox->isChecked())
    {
        TOOLS_INFO knowledgeInfo = promptx::toolKnowledge();
        knowledgeInfo.description.replace(QStringLiteral("{embeddingdb describe}"), embeddingdb_describe);
        appendTool(knowledgeInfo);
    }
    if (date_ui->stablediffusion_checkbox && date_ui->stablediffusion_checkbox->isChecked())
    {
        appendTool(promptx::toolStableDiffusion());
    }
    if (date_ui->controller_checkbox && date_ui->controller_checkbox->isChecked())
    {
        TOOLS_INFO controllerInfo = promptx::toolController();
        controllerInfo.description.replace(QStringLiteral("{controller_norm_x}"), QString::number(ui_controller_norm_x));
        controllerInfo.description.replace(QStringLiteral("{controller_norm_y}"), QString::number(ui_controller_norm_y));
        if (language_flag == 0) // 中文界面
        {
            controllerInfo.description += QStringLiteral(
                "\n- IMPORTANT (中文界面)：`description` 是用户可见的屏幕提示文本，必须用中文描述动作（例如：点击浏览器地址栏，在搜索框输入文本）。");
        }
        else if (language_flag == EVA_LANG_JA) // 日文界面
        {
            controllerInfo.description += QStringLiteral(
                "\n- IMPORTANT (日本語UI)：`description` はユーザーに表示される画面上の説明文です。必ず日本語で動作を説明してください（例：ブラウザーのアドレスバーをクリックし、検索欄にテキストを入力）。");
        }
        appendTool(controllerInfo);

        TOOLS_INFO monitorInfo = promptx::toolMonitor();
        monitorInfo.description.replace(QStringLiteral("{controller_norm_x}"), QString::number(ui_controller_norm_x));
        monitorInfo.description.replace(QStringLiteral("{controller_norm_y}"), QString::number(ui_controller_norm_y));
        appendTool(monitorInfo);
    }
    if (date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked())
    {
        if (isArchitectModeActive())
        {
            appendTool(promptx::toolEngineerProxy());
        }
        else
        {
            appendTool(promptx::toolExecuteCommand());
            appendTool(promptx::toolReadFile());
            appendTool(promptx::toolSkillCall());
            appendTool(promptx::toolScheduleTask());
            appendTool(promptx::toolWriteFile());
            appendTool(promptx::toolReplaceInFile());
            appendTool(promptx::toolEditInFile());
            appendTool(promptx::toolListFiles());
            appendTool(promptx::toolSearchContent());
            appendTool(promptx::toolPtc());
        }
    }
    return tools;
}
void Widget::recv_mcp_tools_changed()
{
    ui_extra_prompt = create_extra_prompt();
    if (date_ui)
    {
        get_date(shouldApplySandboxNow());
        auto_save_user();
    }
}

QString Widget::truncateString(const QString &str, int maxLength)
{
    if (str.size() <= maxLength)
    {
        return str;
    }
    // 使用QTextStream来处理多字节字符
    QTextStream stream(const_cast<QString *>(&str), QIODevice::ReadOnly);
    stream.setCodec("UTF-8");
    // 找到开始截取的位置
    int startIndex = str.size() - maxLength;
    // 确保不截断多字节字符
    stream.seek(startIndex);
    return QString(stream.readAll());
}
QString Widget::checkPython()
{
    // 暂停工程师环境探测（Python/git/cmake），避免启动或刷新时执行外部检测。
#if 0
    QStringList lines;
    if (shouldUseDockerEnv())
    {
        auto dockerLine = [&](const QString &label, const QString &cmd) {
            const QString output = runDockerExecCommand(cmd);
            QString trimmed = output.trimmed();
            if (trimmed.isEmpty())
            {
                lines << QStringLiteral("%1: not found").arg(label);
            }
            else
            {
                const int nl = trimmed.indexOf('\n');
                if (nl != -1) trimmed = trimmed.left(nl);
                lines << QStringLiteral("%1: %2").arg(label, trimmed.trimmed());
            }
        };
        dockerLine(QStringLiteral("Python"), QStringLiteral("python3 --version || python --version"));
        dockerLine(QStringLiteral("git"), QStringLiteral("git --version"));
        dockerLine(QStringLiteral("cmake"), QStringLiteral("cmake --version | head -n 1"));
        return lines.join('\n') + QStringLiteral("\n");
    }

    const QString projDir = applicationDirPath;
    ExecSpec spec = DependencyResolver::discoverPython3(projDir);
    if (spec.program.isEmpty())
    {
        lines << QStringLiteral("Python: not found");
    }
    else
    {
        QString version = DependencyResolver::pythonVersion(spec);
        if (version.isEmpty()) version = QStringLiteral("Python 3");
        QString sourceHint;
        if (spec.program.compare(QStringLiteral("py"), Qt::CaseInsensitive) == 0)
        {
            sourceHint = QStringLiteral(" via py -3");
        }
        else if (spec.absolutePath.contains(QStringLiteral(".venv"), Qt::CaseInsensitive) ||
                 spec.absolutePath.contains(QStringLiteral("/venv/"), Qt::CaseInsensitive) ||
                 spec.absolutePath.contains(QStringLiteral("\\venv\\"), Qt::CaseInsensitive))
        {
            sourceHint = QStringLiteral(" from project venv");
        }
        lines << QStringLiteral("Python: %1%2").arg(version, sourceHint);
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    auto versionLine = [&](const QString &program, const QStringList &args) -> QString {
        ProcessResult result = ProcessRunner::run(program, args, projDir, env, 3000);
        if (result.exitCode != 0) return QString();
        QString text = result.stdOut.isEmpty() ? result.stdErr : result.stdOut;
        text = text.trimmed();
        if (text.isEmpty()) return QString();
        const int nl = text.indexOf('\n');
        if (nl != -1) text = text.left(nl);
        return text.trimmed();
    };

    const QString gitLine = versionLine(QStringLiteral("git"), {QStringLiteral("--version")});
    lines << (gitLine.isEmpty() ? QStringLiteral("git: not found") : QStringLiteral("git: %1").arg(gitLine));

    const QString cmakeLine = versionLine(QStringLiteral("cmake"), {QStringLiteral("--version")});
    lines << (cmakeLine.isEmpty() ? QStringLiteral("cmake: not found") : QStringLiteral("cmake: %1").arg(cmakeLine));

    return lines.join('\n') + QStringLiteral("\n");
#endif
    return QString();
}
QString Widget::checkCompile()
{
    // 暂停工程师环境探测（编译器版本），避免执行外部检查命令。
#if 0
    if (shouldUseDockerEnv())
    {
        QStringList lines;
        auto dockerLine = [&](const QString &label, const QString &cmd) {
            QString output = runDockerExecCommand(cmd);
            QString trimmed = output.trimmed();
            if (trimmed.isEmpty()) return;
            const int nl = trimmed.indexOf('\n');
            if (nl != -1) trimmed = trimmed.left(nl);
            lines << QStringLiteral("%1: %2").arg(label, trimmed.trimmed());
        };
        dockerLine(QStringLiteral("GCC version"), QStringLiteral("gcc --version | head -n 1"));
        dockerLine(QStringLiteral("Clang version"), QStringLiteral("clang --version | head -n 1"));
        if (lines.isEmpty()) lines << QStringLiteral("No compiler detected.");
        return lines.join('\n') + QStringLiteral("\n");
    }

    QString compilerInfo;
    QProcess process;
    // Windows平台的编译器检查
#ifdef Q_OS_WIN
    // 尝试检查 MinGW
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "g++ --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            QString versionLine = lines.first();
            QRegExp regExp("\\s*\\(.*\\)"); // 使用正则表达式去除括号中的内容 匹配括号及其中的内容
            versionLine = versionLine.replace(regExp, "");
            compilerInfo += "MinGW version: ";
            compilerInfo += versionLine.trimmed(); // 去除前后的空格
            compilerInfo += "\n";
        }
    }
    // 检查 MSVC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "cl /Bv";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QByteArray output = process.readAllStandardOutput();
        output += process.readAllStandardError();
        if (!output.isEmpty())
        {
            QString outputStr = QString::fromLocal8Bit(output);
            QStringList lines = outputStr.split('\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty())
            {
                compilerInfo += "MSVC version: ";
                compilerInfo += lines.first().trimmed();
                compilerInfo += "\n";
            }
        }
    }
    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            compilerInfo += "Clang version: ";
            compilerInfo += lines.first().trimmed();
            compilerInfo += "\n";
        }
    }
#endif
    // Linux平台的编译器检查
#ifdef Q_OS_LINUX
    // 检查 GCC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "gcc --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            compilerInfo += "GCC version: ";
            compilerInfo += lines.first().trimmed();
            compilerInfo += "\n";
        }
    }
    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            compilerInfo += "Clang version: ";
            compilerInfo += lines.first().trimmed();
            compilerInfo += "\n";
        }
    }
#endif
    // 如果没有找到任何编译器信息
    if (compilerInfo.isEmpty())
    {
        compilerInfo = "No compiler detected.\n";
    }
    return compilerInfo;
#endif
    return QString();
}
QString Widget::checkNode()
{
    // 暂停工程师环境探测（Node/npm），避免执行外部检查命令。
#if 0
    if (shouldUseDockerEnv())
    {
        QStringList lines;
        auto dockerLine = [&](const QString &label, const QString &cmd) {
            QString output = runDockerExecCommand(cmd);
            QString trimmed = output.trimmed();
            if (trimmed.isEmpty())
                lines << QStringLiteral("%1: not found").arg(label);
            else
                lines << QStringLiteral("%1: %2").arg(label, trimmed.split('\n').first().trimmed());
        };
        dockerLine(QStringLiteral("node"), QStringLiteral("node --version"));
        dockerLine(QStringLiteral("npm"), QStringLiteral("npm --version"));
        return lines.join('\n') + QStringLiteral("\n");
    }

    const QString workingDir = applicationDirPath;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    auto versionLine = [&](const QString &program, const QStringList &args) -> QString {
        auto pickFirstLine = [](const ProcessResult &r) -> QString {
            QString text = r.stdOut.isEmpty() ? r.stdErr : r.stdOut;
            text = text.trimmed();
            if (text.isEmpty()) return QString();
            const int nl = text.indexOf('\n');
            if (nl != -1) text = text.left(nl);
            return text.trimmed();
        };

        ProcessResult result = ProcessRunner::run(program, args, workingDir, env, 3000);
        QString text = (result.exitCode == 0) ? pickFirstLine(result) : QString();
#ifdef Q_OS_WIN
        if (text.isEmpty())
        {
            QString cmdLine = program;
            for (const QString &arg : args)
            {
                if (arg.isEmpty()) continue;
                if (arg.contains(' '))
                {
                    cmdLine += QStringLiteral(" \"") + arg + QStringLiteral("\"");
                }
                else
                {
                    cmdLine += QStringLiteral(" ") + arg;
                }
            }
            ProcessResult shellResult = ProcessRunner::runShellCommand(cmdLine.trimmed(), workingDir, env, 3000);
            text = pickFirstLine(shellResult);
        }
#endif
        return text;
    };

    QStringList lines;
    const QString nodeLine = versionLine(QStringLiteral("node"), {QStringLiteral("--version")});
    lines << (nodeLine.isEmpty() ? QStringLiteral("node: not found") : QStringLiteral("node: %1").arg(nodeLine));
    const QString npmLine = versionLine(QStringLiteral("npm"), {QStringLiteral("--version")});
    lines << (npmLine.isEmpty() ? QStringLiteral("npm: not found") : QStringLiteral("npm: %1").arg(npmLine));
    return lines.join('\n') + QStringLiteral("\n");
#endif
    return QString();
}

// 添加额外停止标志，本地模式时在xbot.cpp里已经现若同时包含"<|" 和 "|>"也停止
void Widget::addStopwords()
{
    ui_DATES.extra_stop_words.clear(); // 重置额外停止标志
    if (ui_DATES.is_load_tool)         // 如果挂载了工具则增加额外停止标志
    {
        // ui_DATES.extra_stop_words << DEFAULT_OBSERVATION_STOPWORD;//在后端已经处理了
    }
}
// 获取本机第一个ip地址 排除以.1结尾的地址 如果只有一个.1结尾的则保留它
QString Widget::getFirstNonLoopbackIPv4Address()
{
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    QString ipWithDot1; // 用于存储以.1结尾的IP地址
    for (int i = 0; i < list.count(); i++)
    {
        QString ip = list[i].toString();
        // 排除回环地址和非IPv4地址
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol)
        {
            if (ip.endsWith(".1"))
            {
                ipWithDot1 = ip; // 记录以.1结尾的IP地址
            }
            else
            {
                return ip; // 返回第一个不以.1结尾的IP地址
            }
        }
    }
    // 如果没有找到不以.1结尾的IP地址，则返回以.1结尾的IP地址
    if (!ipWithDot1.isEmpty())
    {
        return ipWithDot1;
    }
    return QString(); // 如果没有找到任何符合条件的IP地址，返回空字符串
}
// 服务模式已移除：server_onProcessStarted/server_onProcessFinished
// llama-bench进程结束响应
void Widget::bench_onProcessFinished()
{
    qDebug() << "llama-bench进程结束响应";
}

// 更新gpu内存使用率
void Widget::updateGpuStatus()
{
    if (blockLocalMonitor_) return;
    emit gpu_reflash();
}
// 更新cpu内存使用率
void Widget::updateCpuStatus()
{
    if (blockLocalMonitor_) return;
    emit cpu_reflash();
}

// 创建临时文件夹EVA_TEMP
bool Widget::createTempDirectory(const QString &path)
{
    QDir dir;
    // 检查路径是否存在
    if (dir.exists(path))
    {
        return false;
    }
    else
    {
        // 尝试创建目录
        if (dir.mkpath(path))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}
// 打开文件夹
QString Widget::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);
    return filepath;
}
// 语音朗读相关 文转声相关
// 每次约定和设置后都保存配置到本地
void Widget::auto_save_user()
{
    //--------------保存当前用户配置---------------
    // 创建 QSettings 对象，指定配置文件的名称和格式
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    settings.setValue("ui_mode", ui_mode);         // 机体模式
    settings.setValue("ui_state", ui_state);       // 机体状态
    settings.setValue("shell", shell);             // shell路径
    settings.setValue("python", pythonExecutable); // python版本
    settings.setValue("global_font_family", globalUiSettings_.fontFamily);
    settings.setValue("global_font_size", globalUiSettings_.fontSizePt);
    settings.setValue("output_font_family", globalUiSettings_.outputFontFamily);
    settings.setValue("output_font_size", globalUiSettings_.outputFontSizePt);
    settings.setValue("global_theme", globalUiSettings_.themeId);
    // 保存设置参数
    settings.setValue("modelpath", ui_SETTINGS.modelpath); // 模型路径
    // Persist core sampling params as strings only to avoid float drift on reload
    settings.setValue("temp_str", QString::number(ui_SETTINGS.temp, 'f', 6));
    settings.setValue("repeat_str", QString::number(ui_SETTINGS.repeat, 'f', 6));
    settings.setValue("top_k", ui_SETTINGS.top_k);     // top-k 采样
    settings.setValue("ngl", ui_SETTINGS.ngl);         // gpu负载层数
    settings.setValue("nthread", ui_SETTINGS.nthread); // cpu线程数
    settings.setValue("nctx", ui_SETTINGS.nctx);
    settings.setValue("mmprojpath", ui_SETTINGS.mmprojpath); // 视觉
    settings.setValue("lorapath", ui_SETTINGS.lorapath);     // lora
    settings.remove("monitor_frame"); // 监视帧/监视帧率功能已移除：清理旧配置项，避免误解
    // 保存隐藏设置
    settings.setValue("hid_npredict", ui_SETTINGS.hid_npredict); // 最大输出长度
    settings.setValue("hid_top_p_str", QString::number(ui_SETTINGS.hid_top_p, 'f', 6));
    // Clean legacy percent keys and numeric keys to keep only string keys
    settings.remove("hid_top_p_percent");
    settings.remove("temp_percent");
    settings.remove("repeat_percent");
    settings.remove("temp");
    settings.remove("repeat");
    settings.remove("hid_top_p");
    settings.setValue("hid_batch", ui_SETTINGS.hid_batch);
    settings.setValue("hid_n_ubatch", ui_SETTINGS.hid_n_ubatch);
    settings.setValue("hid_use_mmap", ui_SETTINGS.hid_use_mmap);
    settings.setValue("hid_use_mlock", ui_SETTINGS.hid_use_mlock);
    settings.setValue("hid_flash_attn", ui_SETTINGS.hid_flash_attn);
    settings.setValue("hid_parallel", ui_SETTINGS.hid_parallel);
    settings.setValue("reasoning_effort", ui_SETTINGS.reasoning_effort);
    // 上下文压缩（Compaction）配置：可在配置文件中手动调整
    settings.setValue("compaction_enabled", compactionSettings_.enabled);
    settings.setValue("compaction_trigger_ratio", QString::number(compactionSettings_.trigger_ratio, 'f', 3));
    settings.setValue("compaction_reserve_tokens", compactionSettings_.reserve_tokens);
    settings.setValue("compaction_keep_last_messages", compactionSettings_.keep_last_messages);
    settings.setValue("compaction_max_message_chars", compactionSettings_.max_message_chars);
    settings.setValue("compaction_max_source_chars", compactionSettings_.max_source_chars);
    settings.setValue("compaction_max_summary_chars", compactionSettings_.max_summary_chars);
    settings.setValue("compaction_temp", QString::number(compactionSettings_.temp, 'f', 3));
    settings.setValue("compaction_n_predict", compactionSettings_.n_predict);
    // 定时任务（Scheduler）配置：支持通过 ini 手动调整
    settings.setValue("cron_enabled", schedulerSettings_.enabled);
    settings.setValue("cron_min_interval_ms", schedulerSettings_.min_interval_ms);
    settings.setValue("cron_page_refresh_ms", schedulerSettings_.page_refresh_ms);
    settings.setValue("cron_lookahead_days", schedulerSettings_.cron_lookahead_days);
    settings.setValue("port", ui_port);                     // 服务端口
    settings.setValue("control_host_enabled", controlHostAllowed_);
    settings.setValue("device_backend", ui_device_backend); // 推理设备auto/cpu/cuda/vulkan/opencl
    settings.setValue("lazy_unload_minutes", lazyUnloadMs_ / 60000); // 惰性卸载(分钟)
    settings.setValue("chattemplate", date_ui->chattemplate_comboBox->currentText()); // 对话模板
    QStringList enabledTools;
    auto appendTool = [&](QCheckBox *box, const QString &id) {
        if (box && box->isChecked()) enabledTools << id;
    };
    appendTool(date_ui->calculator_checkbox, QStringLiteral("calculator"));
    appendTool(date_ui->knowledge_checkbox, QStringLiteral("knowledge"));
    appendTool(date_ui->controller_checkbox, QStringLiteral("controller"));
    appendTool(date_ui->stablediffusion_checkbox, QStringLiteral("stablediffusion"));
    appendTool(date_ui->engineer_checkbox, QStringLiteral("engineer"));
    appendTool(date_ui->MCPtools_checkbox, QStringLiteral("mcp"));
    settings.setValue("docker_sandbox_checkbox", ui_dockerSandboxEnabled);
    settings.setValue("docker_sandbox_image", engineerDockerImage);
    settings.setValue("docker_sandbox_container", engineerDockerContainer);
    QString dockerModeValue = QStringLiteral("none");
    if (dockerTargetMode_ == DockerTargetMode::Image) dockerModeValue = QStringLiteral("image");
    else if (dockerTargetMode_ == DockerTargetMode::Container)
        dockerModeValue = QStringLiteral("container");
    settings.setValue("docker_sandbox_mode", dockerModeValue);
    settings.setValue("enabled_tools", enabledTools);
    settings.setValue("controller_norm_x", ui_controller_norm_x); // 桌面控制器归一化 X
    settings.setValue("controller_norm_y", ui_controller_norm_y); // 桌面控制器归一化 Y
    settings.setValue("calculator_checkbox", date_ui->calculator_checkbox->isChecked());           // 计算器工具
    settings.setValue("knowledge_checkbox", date_ui->knowledge_checkbox->isChecked());             // knowledge工具
    settings.setValue("controller_checkbox", date_ui->controller_checkbox->isChecked());           // controller工具
    settings.setValue("stablediffusion_checkbox", date_ui->stablediffusion_checkbox->isChecked()); // 文生图工具
    settings.setValue("engineer_checkbox", date_ui->engineer_checkbox->isChecked());               // engineer工具
    settings.setValue("MCPtools_checkbox", date_ui->MCPtools_checkbox->isChecked());               // MCPtools工具
    settings.setValue("engineer_work_dir", engineerWorkDir);                                       // 工程师工作目录
    settings.setValue("engineer_architect_mode", engineerArchitectMode_);
    settings.setValue("extra_lan", ui_extra_lan);                                                  // 额外指令语种
    settings.setValue("tool_call_mode", ui_tool_call_mode);                                        // 工具调用方式
    settings.beginGroup("backend_overrides");
    settings.remove("");
    const QMap<QString, QString> overrides = DeviceManager::programOverrides();
    for (auto it = overrides.cbegin(); it != overrides.cend(); ++it)
    {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
    // 保存自定义的约定模板
    settings.setValue("custom1_date_system", custom1_date_system);
    // 保存 api 参数：仅在链接模式下更新，避免切到本地模式后把远端配置覆盖掉
    if (ui_mode == LINK_MODE)
    {
        settings.setValue("api_endpoint", apis.api_endpoint);
        settings.setValue("api_key", apis.api_key);
        settings.setValue("api_model", apis.api_model);
    }
    if (skillManager)
    {
        settings.setValue("skills_enabled", skillManager->enabledSkillIds());
    }
    else
    {
        settings.remove("skills_enabled");
    }
    settings.sync(); // flush to disk immediately
    // reflash_state("ui:" + jtr("save_config_mess"), USUAL_SIGNAL);
}

// ???????????????
void Widget::emit_send(const ENDPOINT_DATA &data)
{
    RequestSnapshot snapshot;
    snapshot.apis = apis;
    snapshot.endpoint = data;
    snapshot.wordsObj = wordsObj;
    snapshot.languageFlag = language_flag;
    snapshot.turnId = activeTurnId_;
    emit ui2net_send(snapshot);
}
