#include "widget.h"
#include "ui_widget.h"
#include "toolcall_test_dialog.h"
#include "backendmanagerdialog.h"
#include "../utils/simpleini.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSignalBlocker>

namespace
{
QHash<int, QString> loadLanguageEntries(const QString &path, QMap<int, QString> *idToKey = nullptr)
{
    QHash<int, QString> table;
    const auto map = simpleini::parseFile(path);
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
    {
        QString rawKey = it.key().trimmed();
        QString keyName;
        const int pipePos = rawKey.indexOf(QLatin1Char('|'));
        if (pipePos >= 0)
        {
            keyName = rawKey.mid(pipePos + 1).trimmed();
            rawKey = rawKey.left(pipePos);
        }
        bool ok = false;
        const int id = rawKey.trimmed().toInt(&ok);
        if (!ok) continue;
        if (idToKey && !keyName.isEmpty())
            idToKey->insert(id, keyName);
        table.insert(id, it.value());
    }
    return table;
}

bool loadLanguagePack(const QString &root, QJsonObject &out)
{
    // Resource-only loader: avoid probing local disk on Win7.
    const QString base = root.endsWith(QLatin1Char('/')) ? root.left(root.size() - 1) : root;
    // 语言包文件列表：顺序必须与 language_flag 的索引一致（0=zh,1=en,2=ja）
    const QStringList languageFiles{QStringLiteral("lang_zh.ini"), QStringLiteral("lang_en.ini"), QStringLiteral("lang_ja.ini")};

    QVector<QHash<int, QString>> languageTables;
    QMap<int, QString> idToKey;
    int englishIndex = -1;

    for (int idx = 0; idx < languageFiles.size(); ++idx)
    {
        const QString filePath = base + QStringLiteral("/") + languageFiles.at(idx);
        QFileInfo fi(filePath);
        if (!fi.exists())
        {
            qWarning() << "language file missing in resources:" << filePath;
            continue;
        }
        if (languageFiles.at(idx).contains(QStringLiteral("lang_en")))
        {
            englishIndex = languageTables.size();
            languageTables.push_back(loadLanguageEntries(filePath, &idToKey));
        }
        else
        {
            languageTables.push_back(loadLanguageEntries(filePath, nullptr));
        }
    }

    if (languageTables.isEmpty() || idToKey.isEmpty())
    {
        qWarning() << "language pack incomplete in resources" << root;
        return false;
    }

    if (englishIndex < 0) englishIndex = languageTables.size() - 1; // best-effort fallback

    QJsonObject jsonObj;
    for (auto it = idToKey.constBegin(); it != idToKey.constEnd(); ++it)
    {
        const int id = it.key();
        const QString keyName = it.value();
        QJsonArray translations;
        for (int idx = 0; idx < languageTables.size(); ++idx)
        {
            QString resolved = languageTables[idx].value(id);
            if (resolved.isEmpty() && englishIndex >= 0 && englishIndex < languageTables.size())
            {
                const QString fallback = languageTables[englishIndex].value(id);
                resolved = fallback.isEmpty() ? keyName : fallback;
            }
            if (resolved.isEmpty()) resolved = keyName;
            translations.append(resolved);
        }
        jsonObj.insert(keyName, translations);
    }
    out = jsonObj;
    return true;
}
} // namespace

void Widget::getWords(const QString &languageRoot)
{
    // Win7: stay within resource qrc, avoid any filesystem path probing.
    try
    {
        if (loadLanguagePack(languageRoot, wordsObj)) return;
    }
    catch (...)
    {
        qWarning() << "language load threw, falling back to empty";
    }
    qWarning() << "Failed to load language files from resources" << languageRoot;
    wordsObj = QJsonObject();
}

void Widget::switch_lan_change()
{
    // 约定窗口底部语言切换：由“按钮”升级为“下拉框”，直接按选项决定 language_flag。
    // 兼容：若 currentData 为空，则使用 currentText 兜底。
    if (!date_ui || !date_ui->switch_lan_button) return;
    const QString data = date_ui->switch_lan_button->currentData().toString();
    const QString code = data.isEmpty() ? date_ui->switch_lan_button->currentText() : data;
    language_flag = evaLanguageFlagFromCode(code);
    ui_extra_lan = evaLanguageCodeFromFlag(language_flag);
    apply_language(language_flag);
    ui_extra_prompt = create_extra_prompt();
    emit ui2tool_language(language_flag);
    emit ui2expend_language(language_flag);
}

void Widget::syncDefaultSystemPrompt()
{
    const QString defaultPrompt = promptx::systemPromptTemplate();
    EVA_DATES base = date_map.value(QStringLiteral("default"), EVA_DATES());
    base.date_prompt = defaultPrompt;
    base.is_load_tool = false;
    date_map.insert(QStringLiteral("default"), base);

    const QString wunderPrompt = promptx::wunderSystemPromptTemplate();
    EVA_DATES wunder = date_map.value(QStringLiteral("wunder"), EVA_DATES());
    wunder.date_prompt = wunderPrompt;
    wunder.is_load_tool = false;
    date_map.insert(QStringLiteral("wunder"), wunder);

    QString currentTemplate = ui_template;
    if (date_ui && date_ui->chattemplate_comboBox)
    {
        currentTemplate = date_ui->chattemplate_comboBox->currentText();
    }
    const bool usingDefaultTemplate = (currentTemplate == QStringLiteral("default"));
    const bool usingWunderTemplate = (currentTemplate == QStringLiteral("wunder"));
    if (!usingDefaultTemplate && !usingWunderTemplate) return;

    const QString activePrompt = usingWunderTemplate ? wunderPrompt : defaultPrompt;
    ui_date_prompt = activePrompt;
    if (date_ui && date_ui->date_prompt_TextEdit)
    {
        const QSignalBlocker blocker(date_ui->date_prompt_TextEdit);
        date_ui->date_prompt_TextEdit->setPlainText(activePrompt);
    }
}

void Widget::apply_language(int language_flag_)
{
    // 同步提示词与技能协议语种
    promptx::setPromptLanguage(language_flag_);
    if (skillManager) skillManager->setLanguage(language_flag_);
    syncDefaultSystemPrompt();
    // 主界面语种
    ui->load->setText(jtr("load"));
    ui->load->setToolTip(jtr("load_button_tooltip"));
    ui->date->setText(jtr("date"));
    ui->set->setToolTip(jtr("set"));
    ui->reset->setToolTip(jtr("reset"));
    ui->send->setText(jtr("send"));
    ui->send->setToolTip(jtr("send_tooltip"));
    cutscreen_dialog->initAction(jtr("save cut image"), jtr("svae screen image"));
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(std::thread::hardware_concurrency()));
    ui->mem_bar->setShowText(jtr("mem"));   // 进度条里面的文本,强制重绘
    ui->vram_bar->setShowText(jtr("vram")); // 进度条里面的文本,强制重绘
    ui->cpu_bar->show_text = "cpu ";        // 进度条里面的文本
    ui->vcore_bar->show_text = "gpu ";      // 进度条里面的文本
    // 输入区右击菜单语种
    create_right_menu(); // 添加右击问题
    create_tray_right_menu();
    // api设置语种
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_endpoint_label->setText(jtr("api endpoint"));
    api_endpoint_LineEdit->setPlaceholderText(jtr("input api endpoint"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_key_label->setText(jtr("api key"));
    api_key_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_key_LineEdit->setToolTip(jtr("input api key"));
    api_model_label->setText(jtr("api model"));
    api_model_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_model_LineEdit->setToolTip(jtr("input api model"));
    if (linkTabWidget && apiTabWidget)
        linkTabWidget->setTabText(linkTabWidget->indexOf(apiTabWidget), jtr("api link tab"));
    if (linkTabWidget && controlTabWidget)
        linkTabWidget->setTabText(linkTabWidget->indexOf(controlTabWidget), jtr("control link tab"));
    if (control_host_label)
        control_host_label->setText(jtr("control target"));
    if (control_host_LineEdit)
        control_host_LineEdit->setPlaceholderText(jtr("control target placeholder"));
    if (control_port_label)
        control_port_label->setText(jtr("control port"));
    if (control_port_LineEdit)
        control_port_LineEdit->setPlaceholderText(QString::number(DEFAULT_CONTROL_PORT));
    if (control_token_label)
        control_token_label->setText(jtr("control token"));
    if (control_token_LineEdit)
    {
        control_token_LineEdit->setPlaceholderText(jtr("control token placeholder"));
        control_token_LineEdit->setToolTip(jtr("control token placeholder"));
    }
    // 约定选项语种
    date_ui->prompt_box->setTitle(jtr("character")); // 提示词模板设置区域
    date_ui->chattemplate_label->setText(jtr("chat template"));
    date_ui->chattemplate_label->setToolTip(jtr("chattemplate_label_tooltip"));
    date_ui->chattemplate_comboBox->setToolTip(jtr("chattemplate_label_tooltip"));
    date_ui->date_prompt_label->setText(jtr("date prompt"));
    date_ui->date_prompt_label->setToolTip(jtr("date_prompt_label_tooltip"));
    date_ui->date_prompt_TextEdit->setToolTip(jtr("date_prompt_label_tooltip"));
    date_ui->tool_box->setTitle(jtr("mount") + jtr("tool"));
    date_ui->calculator_checkbox->setText(jtr("calculator"));
    date_ui->calculator_checkbox->setToolTip(jtr("calculator_checkbox_tooltip"));
    date_ui->engineer_checkbox->setText(jtr("engineer"));
    date_ui->engineer_checkbox->setToolTip(jtr("engineer_checkbox_tooltip"));
    date_ui->controller_checkbox->setText(jtr("controller"));
    date_ui->controller_checkbox->setToolTip(jtr("controller_checkbox_tooltip"));
    if (date_ui->toolcall_mode_comboBox)
    {
        QSignalBlocker blocker(date_ui->toolcall_mode_comboBox);
        const int current = date_ui->toolcall_mode_comboBox->currentData().toInt();
        date_ui->toolcall_mode_comboBox->clear();
        date_ui->toolcall_mode_comboBox->addItem(jtr("tool call mode option tool_call"), static_cast<int>(TOOL_CALL_TEXT));
        date_ui->toolcall_mode_comboBox->addItem(jtr("tool call mode option function_call"), static_cast<int>(TOOL_CALL_FUNCTION));
        const int idx = date_ui->toolcall_mode_comboBox->findData(current);
        if (idx >= 0) date_ui->toolcall_mode_comboBox->setCurrentIndex(idx);
        date_ui->toolcall_mode_comboBox->setToolTip(jtr("tool call mode tooltip"));
    }
    if (date_ui->controller_norm_box)
    {
        date_ui->controller_norm_box->setTitle(jtr("controller normalize"));
        date_ui->controller_norm_box->setToolTip(jtr("controller normalize tooltip"));
    }
    if (date_ui->controller_norm_x_spin)
    {
        date_ui->controller_norm_x_spin->setToolTip(jtr("controller normalize tooltip"));
    }
    if (date_ui->controller_norm_y_spin)
    {
        date_ui->controller_norm_y_spin->setToolTip(jtr("controller normalize tooltip"));
    }
    date_ui->knowledge_checkbox->setText(jtr("knowledge"));
    date_ui->knowledge_checkbox->setToolTip(jtr("knowledge_checkbox_tooltip"));
    date_ui->MCPtools_checkbox->setText(jtr("MCPtools"));
    date_ui->MCPtools_checkbox->setToolTip(jtr("MCPtools_checkbox_tooltip"));
    date_ui->stablediffusion_checkbox->setText(jtr("stablediffusion"));
    date_ui->stablediffusion_checkbox->setToolTip(jtr("stablediffusion_checkbox_tooltip"));
    date_ui->date_engineer_workdir_label->setText(jtr("work dir"));
    if (date_ui->docker_target_comboBox)
    {
        QSignalBlocker blocker(date_ui->docker_target_comboBox);
        const int current = date_ui->docker_target_comboBox->currentData().toInt();
        date_ui->docker_target_comboBox->clear();
        date_ui->docker_target_comboBox->addItem(jtr("docker target option none"), static_cast<int>(DockerTargetMode::None));
        date_ui->docker_target_comboBox->addItem(jtr("docker target option image"), static_cast<int>(DockerTargetMode::Image));
        date_ui->docker_target_comboBox->addItem(jtr("docker target option container"), static_cast<int>(DockerTargetMode::Container));
        int idx = date_ui->docker_target_comboBox->findData(current);
        if (idx >= 0) date_ui->docker_target_comboBox->setCurrentIndex(idx);
    }
    updateDockerComboToolTip();
    if (date_ui->skills_box)
    {
        date_ui->skills_box->setTitle(jtr("skills mount"));
        if (date_ui->skills_hint_label)
        {
            date_ui->skills_hint_label->clear();
            date_ui->skills_hint_label->setVisible(false);
        }
    }
    date_ui->switch_lan_button->setToolTip(jtr("switch_lan_button_tooltip"));
    date_ui->confirm_button->setText(jtr("ok"));
    date_ui->cancel_button->setText(jtr("cancel"));
    date_dialog->setWindowTitle(jtr("date"));
    // 设置选项语种
    settings_ui->sample_box->setTitle(jtr("sample set")); // 采样设置区域
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(ui_SETTINGS.temp));
    settings_ui->temp_label->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    settings_ui->temp_slider->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat));
    settings_ui->topk_label->setText(jtr("top_k") + " " + QString::number(ui_SETTINGS.top_k));
    settings_ui->topk_slider->setToolTip(jtr("top_k_label_tooltip"));
    settings_ui->topk_label->setToolTip(jtr("top_k_label_tooltip"));
    if (settings_ui->reasoning_label)
        settings_ui->reasoning_label->setText(jtr("reasoning effort"));
    rebuildReasoningCombo();
    npredict_change();
    settings_ui->parallel_label->setText(jtr("parallel") + " " + QString::number(ui_SETTINGS.hid_parallel));
    settings_ui->parallel_slider->setToolTip(jtr("parallel_label_tooltip"));
    settings_ui->parallel_label->setToolTip(jtr("parallel_label_tooltip"));
    settings_ui->repeat_label->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    settings_ui->repeat_slider->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    settings_ui->backend_box->setTitle(jtr("backend set")); // 后端设置区域
    settings_ui->device_label->setText(jtr("device"));
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl));
    settings_ui->ngl_label->setToolTip(jtr("put some model paragram to gpu and reload model"));
    settings_ui->ngl_slider->setToolTip(jtr("put some model paragram to gpu and reload model"));
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread));
    settings_ui->nthread_label->setToolTip(jtr("not big better"));
    settings_ui->nthread_slider->setToolTip(jtr("not big better"));
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(ui_SETTINGS.nctx));
    settings_ui->nctx_label->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    settings_ui->nctx_slider->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    settings_ui->lora_label->setText(jtr("load lora"));
    settings_ui->lora_label->setToolTip(jtr("lora_label_tooltip"));
    settings_ui->lora_LineEdit->setToolTip(jtr("lora_label_tooltip"));
    settings_ui->lora_LineEdit->setPlaceholderText(jtr("right click and choose lora"));
    settings_ui->mmproj_label->setText(jtr("load mmproj"));
    settings_ui->mmproj_label->setToolTip(jtr("mmproj_label_tooltip"));
    settings_ui->mmproj_LineEdit->setToolTip(jtr("mmproj_label_tooltip"));
    settings_ui->mmproj_LineEdit->setPlaceholderText(jtr("right click and choose mmproj"));
    settings_ui->mode_box->setTitle(jtr("state set")); // 状态设置区域
    settings_ui->complete_btn->setText(jtr("complete state"));
    settings_ui->complete_btn->setToolTip(jtr("complete_btn_tooltip"));
    settings_ui->chat_btn->setText(jtr("chat state"));
    settings_ui->chat_btn->setToolTip(jtr("chat_btn_tooltip"));
    settings_ui->port_label->setText(jtr("exposed port"));
    settings_ui->port_label->setToolTip(jtr("port_label_tooltip"));
    settings_ui->port_lineEdit->setToolTip(jtr("port_label_tooltip"));
    if (settings_ui->allow_control_checkbox)
    {
        settings_ui->allow_control_checkbox->setText(jtr("control host allow"));
        settings_ui->allow_control_checkbox->setToolTip(jtr("control host allow tooltip").arg(QString::number(DEFAULT_CONTROL_PORT)));
    }
    if (settings_ui->lazy_timeout_label)
        settings_ui->lazy_timeout_label->setToolTip(jtr("pop countdown tooltip"));
    if (settings_ui->lazy_timeout_spin)
    {
        settings_ui->lazy_timeout_spin->setSuffix(QStringLiteral(" ") + jtr("minute short"));
        settings_ui->lazy_timeout_spin->setToolTip(jtr("pop disable tooltip"));
    }
    updateLazyCountdownLabel();
    settings_ui->confirm->setText(jtr("ok"));
    settings_ui->cancel->setText(jtr("cancel"));
    settings_dialog->setWindowTitle(jtr("set"));
    updateKvBarUi();
    // Language refresh rewrites many labels including device_label.
    // Re-apply device/backend UI hint so that auto(effective) suffix stays visible after first boot.
    refreshDeviceBackendUI();
    updateGlobalSettingsTranslations();
    if (toolCallTestDialog_) toolCallTestDialog_->refreshTranslations();
    if (backendManagerDialog_) backendManagerDialog_->refreshTranslations();
}

QString Widget::jtr(QString customstr) const
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}
