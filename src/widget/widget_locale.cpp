#include "widget.h"
#include "ui_widget.h"
#include "toolcall_test_dialog.h"
#include "../utils/simpleini.h"
#include <QDebug>
#include <QFile>
#include <QDir>

namespace
{
QHash<int, QString> loadLanguageEntries(const QString &path)
{
    QHash<int, QString> table;
    const auto map = simpleini::parseFile(path);
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
    {
        bool ok = false;
        const int id = it.key().toInt(&ok);
        if (!ok) continue;
        table.insert(id, it.value());
    }
    return table;
}
} // namespace

namespace
{
bool loadLanguagePack(const QString &root, QJsonObject &out)
{
    QDir dir(root);
    const QString keyFile = dir.filePath(QStringLiteral("key_index.ini"));
    const auto keyIndex = simpleini::parseFile(keyFile);
    if (keyIndex.isEmpty()) return false;
    QStringList discovered = dir.entryList(QStringList() << QStringLiteral("lang_*.ini"), QDir::Files, QDir::Name);
    QStringList languageFiles;
    auto takeIfPresent = [&](const QString &name)
    {
        const int idx = discovered.indexOf(name);
        if (idx >= 0)
        {
            languageFiles << discovered.takeAt(idx);
        }
    };
    takeIfPresent(QStringLiteral("lang_zh.ini"));
    takeIfPresent(QStringLiteral("lang_en.ini"));
    languageFiles << discovered;
    if (languageFiles.isEmpty())
    {
        languageFiles.clear();
        languageFiles << QStringLiteral("lang_zh.ini") << QStringLiteral("lang_en.ini");
    }
    QList<QHash<int, QString>> languageTables;
    for (const QString &file : languageFiles)
    {
        languageTables.push_back(loadLanguageEntries(dir.filePath(file)));
    }
    int englishIndex = languageFiles.indexOf(QStringLiteral("lang_en.ini"));
    if (englishIndex < 0 && !languageTables.isEmpty()) englishIndex = 0;
    QJsonObject jsonObj;
    for (auto it = keyIndex.constBegin(); it != keyIndex.constEnd(); ++it)
    {
        bool ok = false;
        const int id = it.value().toInt(&ok);
        if (!ok) continue;
        QJsonArray translations;
        for (int idx = 0; idx < languageTables.size(); ++idx)
        {
            QString resolved = languageTables[idx].value(id);
            if (resolved.isEmpty() && englishIndex >= 0 && englishIndex < languageTables.size())
                resolved = languageTables[englishIndex].value(id);
            if (resolved.isEmpty()) resolved = it.key();
            translations.append(resolved);
        }
        jsonObj.insert(it.key(), translations);
    }
    out = jsonObj;
    return true;
}
} // namespace

void Widget::getWords(const QString &languageRoot)
{
    if (loadLanguagePack(languageRoot, wordsObj)) return;
    if (!languageRoot.startsWith(":/"))
    {
        if (loadLanguagePack(QStringLiteral(":/language"), wordsObj)) return;
    }
    qWarning() << "Failed to load language files from" << languageRoot;
    wordsObj = QJsonObject();
}

void Widget::switch_lan_change()
{
    if (date_ui->switch_lan_button->text() == "zh")
    {
        language_flag = 1;
        date_ui->switch_lan_button->setText("en");
    }
    else if (date_ui->switch_lan_button->text() == "en")
    {
        language_flag = 0;
        date_ui->switch_lan_button->setText("zh");
    }
    apply_language(language_flag);
    ui_extra_prompt = create_extra_prompt();
    emit ui2tool_language(language_flag);
    emit ui2net_language(language_flag);
    emit ui2expend_language(language_flag);
}

void Widget::apply_language(int language_flag_)
{
    Q_UNUSED(language_flag_);
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
    date_ui->knowledge_checkbox->setText(jtr("knowledge"));
    date_ui->knowledge_checkbox->setToolTip(jtr("knowledge_checkbox_tooltip"));
    date_ui->MCPtools_checkbox->setText(jtr("MCPtools"));
    date_ui->MCPtools_checkbox->setToolTip(jtr("MCPtools_checkbox_tooltip"));
    date_ui->stablediffusion_checkbox->setText(jtr("stablediffusion"));
    date_ui->stablediffusion_checkbox->setToolTip(jtr("stablediffusion_checkbox_tooltip"));
    date_ui->date_engineer_workdir_label->setText(jtr("work dir"));
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
    settings_ui->frame_label->setText(jtr("frame"));
    settings_ui->frame_label->setToolTip(jtr("frame_label_tooltip"));
    settings_ui->frame_lineEdit->setToolTip(jtr("frame_label_tooltip"));
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
}

QString Widget::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}
