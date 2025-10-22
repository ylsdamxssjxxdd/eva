#include "expend.h"

#include "cmakeconfig.h"
#include "ui_expend.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QFrame>
#include <QPainter>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <src/utils/imagedropwidget.h>

// Local lightweight image drop/click widget for img2img

//-------------------------------------------------------------------------
//----------------------------------界面相关--------------------------------
//-------------------------------------------------------------------------

// 初始化增殖窗口
void Expend::init_expend()
{
    this->setWindowTitle(jtr("expend window"));                                             // 标题
    ui->tabWidget->setTabText(window_map[INTRODUCTION_WINDOW], jtr("introduction"));        // 软件介绍
    ui->tabWidget->setTabText(window_map[MODELINFO_WINDOW], jtr("model info"));             // 模型信息
    ui->tabWidget->setTabText(window_map[QUANTIZE_WINDOW], jtr("model") + jtr("quantize")); // 模型量化
    ui->tabWidget->setTabText(window_map[MCP_WINDOW], jtr("mcp_server"));                   // 软件介绍
    ui->tabWidget->setTabText(window_map[KNOWLEDGE_WINDOW], jtr("knowledge"));              // 知识库
    ui->tabWidget->setTabText(window_map[TXT2IMG_WINDOW], jtr("text2image"));               // 文生图
    ui->tabWidget->setTabText(window_map[WHISPER_WINDOW], jtr("speech2text"));              // 声转文
    ui->tabWidget->setTabText(window_map[TTS_WINDOW], jtr("text2speech"));                  // 文转声
    ui->tabWidget->setTabText(window_map[MODELEVAL_WINDOW], jtr("model evaluate"));         // 模型评估
    // 软件介绍
    showReadme();
    // 模型量化
    ui->model_quantize_label->setText(jtr("model_quantize_label_text"));
    ui->model_quantize_label_2->setText(jtr("model_quantize_label_2_text"));
    ui->model_quantize_label_3->setText(jtr("model_quantize_label_3_text"));
    ui->model_quantize_row_modelpath_lineedit->setPlaceholderText(jtr("model_quantize_row_modelpath_lineedit_placeholder"));
    ui->model_quantize_important_datapath_lineedit->setPlaceholderText(jtr("model_quantize_important_datapath_lineedit_placeholder"));
    ui->model_quantize_output_modelpath_lineedit->setPlaceholderText(jtr("model_quantize_output_modelpath_lineedit_placeholder"));
    ui->quantize_info_groupBox->setTitle(jtr("quantize_info_groupBox_title"));
    show_quantize_types();
    ui->model_quantize_type_label->setText(jtr("select quantize type"));
    ui->model_quantize_execute->setText(jtr("execute quantize"));
    ui->quantize_log_groupBox->setTitle("llama-quantize " + jtr("execute log"));
    // 知识库
    if (ui->embedding_txt_wait && ui->embedding_txt_wait->columnCount() == 0) ui->embedding_txt_wait->setColumnCount(1);
    if (ui->embedding_txt_over && ui->embedding_txt_over->columnCount() == 0) ui->embedding_txt_over->setColumnCount(1);
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")});   // 设置列名
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{jtr("embedless text segment")}); // 设置列名
    ui->embedding_model_label->setText(jtr("embd model"));
    ui->embedding_dim_label->setText(jtr("embd dim"));
    ui->embedding_model_lineedit->setPlaceholderText(jtr("embedding_model_lineedit_placeholder"));
    ui->embedding_split_label->setText(jtr("split length"));
    ui->embedding_overlap_label->setText(jtr("overlap length"));
    ui->embedding_source_doc_label->setText(jtr("source txt"));
    ui->embedding_txt_lineEdit->setPlaceholderText(jtr("embedding_txt_lineEdit_placeholder"));
    ui->embedding_describe_label->setText(jtr("knowledge base description"));
    ui->embedding_txt_describe_lineEdit->setPlaceholderText(jtr("embedding_txt_describe_lineEdit_placeholder"));
    ui->embedding_txt_embedding->setText(jtr("embedding txt"));
    ui->embedding_test_groupBox->setTitle(jtr("test"));
    ui->embedding_test_textEdit->setPlaceholderText(jtr("embedding_test_textEdit_placeholder"));
    ui->embedding_test_pushButton->setText(jtr("retrieval"));
    ui->embedding_result_groupBox->setTitle(jtr("retrieval result"));
    ui->embedding_log_groupBox->setTitle(jtr("log"));
    ui->embedding_resultnumb_label->setText(jtr("resultnumb"));
    ui->sd_result_groupBox->setTitle(jtr("result"));
    if (ui->embedding_test_log)
    {
        ui->embedding_test_log->setLineWrapMode(QPlainTextEdit::NoWrap);
        ui->embedding_test_log->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
    if (ui->embedding_txt_wait)
    {
        if (ui->embedding_txt_wait->horizontalHeader())
            ui->embedding_txt_wait->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        ui->embedding_txt_wait->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->embedding_txt_wait, &QTableWidget::customContextMenuRequested, this, &Expend::show_embedding_txt_wait_menu, Qt::UniqueConnection);
    }
    if (ui->embedding_txt_over)
    {
        if (ui->embedding_txt_over->horizontalHeader())
            ui->embedding_txt_over->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        ui->embedding_txt_over->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->embedding_txt_over, &QTableWidget::customContextMenuRequested, this, &Expend::show_embedding_txt_over_menu, Qt::UniqueConnection);
    }
    // Use MediaResultWidget declared in .ui
    sd_mediaResult = ui->sd_media_result;
    // 旧的修饰词/负面词行内输入移除；请在“高级设置…”中编辑

    ui->sd_prompt_textEdit->setPlaceholderText(jtr("sd_prompt_textEdit_placeholder"));
    if (ui->sd_draw_pushButton->text() != "stop") { ui->sd_draw_pushButton->setText(QStringLiteral("生成")); }
    // Use ImageDropWidget declared in .ui
    sd_imgDrop = ui->sd_img_drop;
    if (sd_imgDrop)
    {
        sd_imgDrop->setMinimumHeight(150);
        sd_imgDrop->setPlaceholderText(jtr("sd_img2img_lineEdit_placeholder"));
    }
    ui->sd_log->setPlainText(jtr("sd_log_plainText"));
    ui->sd_open_params_button->setText(jtr("params set"));
    ui->sd_draw_pushButton->setText(jtr("generate"));

    // 声转文
    ui->whisper_modelpath_label->setText(jtr("whisper path"));
    ui->whisper_load_modelpath_linedit->setPlaceholderText(jtr("whisper_load_modelpath_linedit_placeholder"));
    ui->speech_load_groupBox_4->setTitle("whisper " + jtr("log"));
    ui->whisper_wav2text_label->setText(jtr("wav2text"));
    ui->whisper_wavpath_pushButton->setText(jtr("wav path"));
    ui->whisper_format_label->setText(jtr("format"));
    ui->whisper_execute_pushbutton->setText(jtr("exec convert"));

    // 文转声
    ui->speech_available_label->setText(jtr("Available sound"));
    ui->speech_enable_radioButton->setText(jtr("enable"));
    ui->speech_outetts_modelpath_label->setText("OuteTTS " + jtr("model"));
    ui->speech_wavtokenizer_modelpath_label->setText("WavTokenizer " + jtr("model"));
    ui->speech_log_groupBox->setTitle(jtr("log"));
    ui->speech_outetts_modelpath_lineEdit->setPlaceholderText(jtr("speech_outetts_modelpath_lineEdit placehold"));
    ui->speech_wavtokenizer_modelpath_lineEdit->setPlaceholderText(jtr("speech_outetts_modelpath_lineEdit placehold"));
    ui->speech_manual_plainTextEdit->setPlaceholderText(jtr("speech_manual_plainTextEdit placehold"));
    ui->speech_manual_pushButton->setText(jtr("convert to audio"));
    ui->speech_source_comboBox->setCurrentText(speech_params.speech_name);
    ui->speech_enable_radioButton->setChecked(speech_params.enable_speech); // Initialize Text-to-Speech source list: always provide outetts, plus system voices if available
    QStringList ttsSources;
    ttsSources << SPPECH_OUTETTS;
    // Lazily create system TTS and enumerate voices
    if (!sys_speech) sys_speech = new QTextToSpeech(this);
    is_sys_speech_available = false;
    if (sys_speech)
    {
        const auto voices = sys_speech->availableVoices();
        if (!voices.isEmpty())
        {
            is_sys_speech_available = true;
            for (const QVoice &v : voices) ttsSources << v.name();
        }
    }
    // Pick a sensible default on first boot
    if (speech_params.speech_name.trimmed().isEmpty())
    {
        const bool haveOute = QFileInfo::exists(ui->speech_outetts_modelpath_lineEdit->text()) &&
                              QFileInfo::exists(ui->speech_wavtokenizer_modelpath_lineEdit->text());
        if (haveOute)
            speech_params.speech_name = SPPECH_OUTETTS;
        else if (is_sys_speech_available)
        {
            for (const QString &n : ttsSources)
            {
                if (n != SPPECH_OUTETTS)
                {
                    speech_params.speech_name = n;
                    break;
                }
            }
        }
    }
    set_sys_speech(ttsSources);
    // React to toggles/selection changes
    connect(ui->speech_enable_radioButton, &QRadioButton::toggled, this, &Expend::speech_enable_change);
    connect(ui->speech_source_comboBox, &QComboBox::currentTextChanged, this, &Expend::speech_source_change);
    speech_source_change();

    // mcp服务器
    ui->mcp_server_state_groupBox->setTitle(jtr("mcp_available_servers"));
    ui->mcp_server_config_groupBox->setTitle(jtr("mcp_server_config"));
    ui->mcp_server_reflash_pushButton->setText(jtr("link"));
    ui->mcp_server_config_textEdit->setPlaceholderText(jtr("mcp_server_config_textEdit placehold"));

    // 模型评估（双语）
    if (ui->eval_info_groupBox) ui->eval_info_groupBox->setTitle(jtr("current info"));
    if (ui->eval_mode_label) ui->eval_mode_label->setText(jtr("mode"));
    if (ui->eval_endpoint_label) ui->eval_endpoint_label->setText(jtr("endpoint"));
    if (ui->eval_model_label) ui->eval_model_label->setText(jtr("model"));
    if (ui->eval_device_label) ui->eval_device_label->setText(jtr("device"));
    if (ui->eval_nctx_label) ui->eval_nctx_label->setText(jtr("context length"));
    if (ui->eval_threads_label) ui->eval_threads_label->setText(jtr("threads"));
    if (ui->eval_summary_ttfb_label) ui->eval_summary_ttfb_label->setText(jtr("first token"));
    if (ui->eval_summary_gen_label) ui->eval_summary_gen_label->setText(jtr("gen speed"));
    if (ui->eval_summary_qa_label) ui->eval_summary_qa_label->setText(jtr("common qa"));
    if (ui->eval_summary_logic_label) ui->eval_summary_logic_label->setText(jtr("logic"));
    if (ui->eval_summary_tool_label) ui->eval_summary_tool_label->setText(jtr("tool call"));
    if (ui->eval_summary_sync_label) ui->eval_summary_sync_label->setText(jtr("sync rate"));
    updateEvalButtonState();
    if (ui->eval_result_groupBox) ui->eval_result_groupBox->setTitle(jtr("steps and results"));
    if (ui->eval_log_groupBox) ui->eval_log_groupBox->setTitle(jtr("evaluation log"));
    if (ui->eval_progressBar) ui->eval_progressBar->setFormat(jtr("progress steps"));
    if (ui->eval_table)
    {
        QStringList headers;
        headers << jtr("metric/step") << jtr("state") << jtr("elapsed(s)") << jtr("value");
        ui->eval_table->setHorizontalHeaderLabels(headers);
    }
    auto applySummaryFlag = [](QWidget *w, const char *prop)
    {
        if (!w) return;
        w->setProperty(prop, true);
        if (auto style = w->style())
        {
            style->unpolish(w);
            style->polish(w);
        }
        w->update();
    };
    auto primeSummaryCard = [&](QFrame *card, QLabel *label, QLabel *value)
    {
        applySummaryFlag(card, "summaryRole");
        applySummaryFlag(label, "summaryLabel");
        applySummaryFlag(value, "summaryValue");
    };
    primeSummaryCard(ui->eval_summary_card_ttfb, ui->eval_summary_ttfb_label, ui->eval_summary_ttfb_value);
    primeSummaryCard(ui->eval_summary_card_gen, ui->eval_summary_gen_label, ui->eval_summary_gen_value);
    primeSummaryCard(ui->eval_summary_card_qa, ui->eval_summary_qa_label, ui->eval_summary_qa_value);
    primeSummaryCard(ui->eval_summary_card_logic, ui->eval_summary_logic_label, ui->eval_summary_logic_value);
    primeSummaryCard(ui->eval_summary_card_tool, ui->eval_summary_tool_label, ui->eval_summary_tool_value);
    primeSummaryCard(ui->eval_summary_card_sync, ui->eval_summary_sync_label, ui->eval_summary_sync_value);
    updateEvalSummary(true);
}

// 用户切换选项卡时响应
//  0软件介绍,1模型信息
void Expend::on_tabWidget_tabBarClicked(int index)
{
    if (index == window_map[INTRODUCTION_WINDOW] && is_first_show_info) // 第一次点软件介绍
    {
        is_first_show_info = false;

        // 展示readme内容
        showReadme();

        // 强制延迟见顶
        QTimer::singleShot(0, this, [this]()
                           {
            ui->info_card->verticalScrollBar()->setValue(0);
            ui->info_card->horizontalScrollBar()->setValue(0); });
    }
    else if (index == window_map[QUANTIZE_WINDOW] && is_first_show_modelproliferation) // 第一次展示量化方法
    {
        is_first_show_modelproliferation = false;
        show_quantize_types();
    }
    else if (index == window_map[MODELINFO_WINDOW] && is_first_show_modelinfo) // 第一次展示模型信息窗口
    {
        is_first_show_modelinfo = false;
    }
    // 每次手动点击切换后，依据目标页启停动画
    updateModelInfoAnim();
}

// 通知显示增殖窗口
void Expend::recv_expend_show(EXPEND_WINDOW window)
{
    if (window == NO_WINDOW)
    {
        this->close();
        return;
    }
    if (is_first_show_expend) // 第一次显示的话
    {
        is_first_show_expend = false;
        this->init_expend();

        if (vocab == "")
        {
            vocab = jtr("lode model first");
        }

        if (ui->modellog_card->toPlainText() == "")
        {
            ui->modellog_card->setPlainText(jtr("lode model first"));
        }
    }

    // 打开指定页数窗口
    ui->tabWidget->setCurrentIndex(window_map[window]);
    this->setWindowState(Qt::WindowActive); // 激活窗口并恢复正常状态
    this->setWindowFlags(Qt::Window);
    this->show();
    this->raise();
    this->activateWindow(); // 激活增殖窗口
    // 窗口显示后，按当前页决定是否启动模型信息动画
    updateModelInfoAnim();
}

QString Expend::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);

    return filepath;
}

// 传递使用的语言
void Expend::recv_language(int language_flag_)
{
    language_flag = language_flag_;
    init_expend();
    updateEvalInfoUi();
}

// 读取配置文件并应用
void Expend::readConfig()
{
    QFile configfile(applicationDirPath + "/EVA_TEMP/eva_config.ini");

    // 如果默认要填入的模型存在，则默认填入
    QString default_sd_modelpath = applicationDirPath + DEFAULT_SD_MODEL_PATH;
    QString default_sd_params_template = "sd1.5-anything-3";
    QFile default_sd_modelpath_file(default_sd_modelpath);
    if (!default_sd_modelpath_file.exists())
    {
        default_sd_modelpath = "";
        default_sd_params_template = "custom1";
    } // 不存在则默认为空

    QString default_whisper_modelpath = applicationDirPath + DEFAULT_WHISPER_MODEL_PATH;
    QFile default_whisper_modelpath_file(default_whisper_modelpath);
    if (!default_whisper_modelpath_file.exists()) { default_whisper_modelpath = ""; } // 不存在则默认为空

    QString default_outetts_modelpath = applicationDirPath + DEFAULT_OUTETTS_MODEL_PATH;
    QFile default_outetts_modelpath_file(default_outetts_modelpath);
    if (!default_outetts_modelpath_file.exists()) { default_outetts_modelpath = ""; } // 不存在则默认为空

    QString default_WavTokenizer_modelpath = applicationDirPath + DEFAULT_WAVTOKENIZER_MODEL_PATH;
    QFile default_WavTokenizer_modelpath_file(default_WavTokenizer_modelpath);
    if (!default_WavTokenizer_modelpath_file.exists()) { default_WavTokenizer_modelpath = ""; } // 不存在则默认为空

    // 读取配置文件中的值
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    QString sd_params_template = settings.value("sd_params_template", default_sd_params_template).toString(); // 参数模板
    QString sd_modelpath = settings.value("sd_modelpath", default_sd_modelpath).toString();                   // sd模型路径
    QString vae_modelpath = settings.value("vae_modelpath", "").toString();                                   // vae模型路径
    QString clip_l_modelpath = settings.value("clip_l_modelpath", "").toString();                             // clip_l模型路径
    QString clip_g_modelpath = settings.value("clip_g_modelpath", "").toString();                             // clip_g模型路径
    QString t5_modelpath = settings.value("t5_modelpath", "").toString();                                     // t5模型路径
    QString lora_modelpath = settings.value("lora_modelpath", "").toString();                                 // lora模型路径
    // Advanced run-config (new popup)
    sd_run_config_.modelPath = settings.value("sd_adv_model_path", sd_modelpath).toString();
    sd_run_config_.vaePath = settings.value("sd_adv_vae_path", vae_modelpath).toString();
    sd_run_config_.clipLPath = settings.value("sd_adv_clip_l_path", clip_l_modelpath).toString();
    sd_run_config_.clipGPath = settings.value("sd_adv_clip_g_path", clip_g_modelpath).toString();
    sd_run_config_.clipVisionPath = settings.value("sd_adv_clip_vision_path", "").toString();
    sd_run_config_.t5xxlPath = settings.value("sd_adv_t5xxl_path", t5_modelpath).toString();
    sd_run_config_.qwen2vlPath = settings.value("sd_adv_qwen2vl_path", "").toString();

    sd_run_config_.qwen2vlVisionPath = settings.value("sd_adv_qwen2vl_vision_path", "").toString();
    sd_run_config_.modifyPrompt = settings.value("sd_adv_modify", "").toString();
    sd_run_config_.negativePrompt = settings.value("sd_adv_negative", "").toString();
    sd_run_config_.loraDirPath = settings.value("sd_adv_lora_dir", "").toString();
    sd_run_config_.taesdPath = settings.value("sd_adv_taesd_path", "").toString();
    sd_run_config_.upscaleModelPath = settings.value("sd_adv_upscale_model", "").toString();
    // Build strict per-preset configuration map and pick the active one
    const QStringList presetsAll = {"flux1-dev", "qwen-image", "sd1.5-anything-3", "custom1", "custom2"};
    for (const QString &p : presetsAll)
        sd_preset_configs_[p] = loadPresetConfig(p);
    // Active preset is read below (sd_params_template); sync current run config after that

    QString sd_prompt = settings.value("sd_prompt", "").toString(); // sd提示词

    QString whisper_modelpath = settings.value("whisper_modelpath", default_whisper_modelpath).toString(); // whisper模型路径

    speech_params.enable_speech = settings.value("speech_enable", "").toBool();                                           // 是否启用语音朗读
    speech_params.speech_name = settings.value("speech_name", "").toString();                                             // 朗读者
    QString outetts_modelpath = settings.value("outetts_modelpath", default_outetts_modelpath).toString();                // outetts模型路径
    QString wavtokenizer_modelpath = settings.value("wavtokenizer_modelpath", default_WavTokenizer_modelpath).toString(); // wavtokenizer模型路径
    ui->speech_outetts_modelpath_lineEdit->setText(outetts_modelpath);
    ui->speech_wavtokenizer_modelpath_lineEdit->setText(wavtokenizer_modelpath);

    ui->mcp_server_config_textEdit->setText(settings.value("Mcpconfig", "").toString()); // mcp配置

    // 旧的行内路径/参数不再应用到界面；统一由高级设置管理

    // 预设列表固定：flux1-dev, qwen-image, sd1.5-anything-3, wan2.2, custom1, custom2
    // Clamp template key to supported set
    if (!QStringList({"flux1-dev", "qwen-image", "sd1.5-anything-3", "wan2.2", "custom1", "custom2"}).contains(sd_params_template))
        sd_params_template = "sd1.5-anything-3";
    // 当前预设的运行配置
    sd_run_config_ = sd_preset_configs_.value(sd_params_template, SDRunConfig{});
    // Load per-preset prompts from config (avoid cross-preset leakage)
    auto sanitize = [this](QString s)
    { return sanitizePresetKey(s); };
    const QStringList presets = {"flux1-dev", "qwen-image", "sd1.5-anything-3", "wan2.2", "custom1", "custom2"};
    for (const QString &p : presets)
    {
        const QString key = sanitize(p);
        sd_preset_modify_[p] = settings.value("sd_preset_" + key + "_modify", "").toString();
        sd_preset_negative_[p] = settings.value("sd_preset_" + key + "_negative", "").toString();
    }
    // Ensure sd1.5 defaults if empty (compat only)
    if (sd_preset_modify_["sd1.5-anything-3"].isEmpty()) sd_preset_modify_["sd1.5-anything-3"] = QStringLiteral("masterpieces, best quality, beauty, detailed, Pixar, 8k");
    if (sd_preset_negative_["sd1.5-anything-3"].isEmpty()) sd_preset_negative_["sd1.5-anything-3"] = QStringLiteral("EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure");
    // Apply current preset prompts from store (kept for compatibility)
    if (!sd_preset_modify_.value(sd_params_template).isEmpty())
        sd_run_config_.modifyPrompt = sd_preset_modify_.value(sd_params_template);
    if (!sd_preset_negative_.value(sd_params_template).isEmpty())
        sd_run_config_.negativePrompt = sd_preset_negative_.value(sd_params_template);
    // Mirror last-used global keys for older flows
    settings.setValue("sd_adv_modify", sd_run_config_.modifyPrompt);
    settings.setValue("sd_adv_negative", sd_run_config_.negativePrompt);
    is_readconfig = true;

    QFile whisper_load_modelpath_file(whisper_modelpath);
    if (whisper_load_modelpath_file.exists())
    {
        ui->whisper_load_modelpath_linedit->setText(whisper_modelpath);
        whisper_params.model = whisper_modelpath.toStdString();
    }

    // 知识库，在main.cpp里有启动的部分
    ui->embedding_txt_describe_lineEdit->setText(settings.value("embedding_describe", "").toString()); // 知识库描述
    ui->embedding_split_spinbox->setValue(settings.value("embedding_split", DEFAULT_EMBEDDING_SPLITLENTH).toInt());
    ui->embedding_resultnumb_spinBox->setValue(settings.value("embedding_resultnumb", DEFAULT_EMBEDDING_RESULTNUMB).toInt());
    ui->embedding_overlap_spinbox->setValue(settings.value("embedding_overlap", DEFAULT_EMBEDDING_OVERLAP).toInt());
    ui->embedding_dim_spinBox->setValue(settings.value("embedding_dim", 1024).toInt());

    QString embedding_sourcetxt = settings.value("embedding_sourcetxt", "").toString(); // 源文档路径
    QFile embedding_sourcetxt_file(embedding_sourcetxt);
    if (embedding_sourcetxt_file.exists())
    {
        txtpath = embedding_sourcetxt;
        ui->embedding_txt_lineEdit->setText(txtpath);
        preprocessTXT(); // 预处理文件内容
    }
}

void Expend::showReadme()
{
    QString readme_content;
    QFile file;
    QString imagefile = ":/logo/ui_demo.png"; // 图片路径固定

    // 根据语言标志选择不同的 README 文件
    if (language_flag == 0)
    {
        file.setFileName(":/README.md");
    }
    else if (language_flag == 1)
    {
        file.setFileName(":/README_en.md");
    }

    // 打开文件并读取内容
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        in.setCodec("UTF-8");
        readme_content = in.readAll();
        file.close();
    }

    // Strip images from README: HTML <img>, Markdown images, and common containers
    // 1) Remove any HTML <img ...> tags (case-insensitive)
    readme_content.remove(QRegularExpression("<img\\b[^>]*>", QRegularExpression::CaseInsensitiveOption));
    // 2) Replace Markdown inline images ![alt](url) with just the alt text
    readme_content = readme_content.replace(QRegularExpression("!\\[([^\\]]*)\\]\\(([^\\)]*)\\)"), "\\1");
    // 3) Replace Markdown reference-style images ![alt][id] with alt text
    readme_content = readme_content.replace(QRegularExpression("!\\[([^\\]]*)\\]\\s*\\[[^\\]]*\\]"), "\\1");
    // 4) Remove HTML picture/figure blocks that often wrap images
    readme_content.remove(QRegularExpression("<picture\\b[\\s\\S]*?</picture>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption));
    readme_content.remove(QRegularExpression("<figure\\b[\\s\\S]*?</figure>", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption));

    // 删除 <summary> 和 </summary> 标签
    readme_content.remove(QRegularExpression("<summary>|</summary>"));

    // 删除 <details> 和 </details> 标签
    readme_content.remove("<details>");
    readme_content.remove("</details>");

    // 添加编译信息
    QString compileInfo = QString("%1: %2\n\n %3: %4\n\n %5: %6\n\n %7: %8\n\n %9: %10\n\n")
                              .arg(jtr("EVA_ENVIRONMENT"), QString(EVA_ENVIRONMENT))
                              .arg(jtr("EVA_PRODUCT_TIME"), QString(EVA_PRODUCT_TIME))
                              .arg(jtr("QT_VERSION_"), QString(QT_VERSION_))
                              .arg(jtr("COMPILE_VERSION"), QString(COMPILE_VERSION))
                              .arg(jtr("EVA_VERSION"), QString(EVA_VERSION));
    readme_content.prepend(compileInfo); // 将编译信息放在文件内容前

    // 设置 Markdown 内容
    ui->info_card->setMarkdown(readme_content);

    // 加载并缩放图片
    QImage image(imagefile);
    int originalWidth = image.width() / devicePixelRatioF() / 1.5;
    int originalHeight = image.height() / devicePixelRatioF() / 1.5;

    // 插入图片到 QTextEdit
    QTextCursor cursor(ui->info_card->textCursor());
    cursor.movePosition(QTextCursor::Start);
    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);
    imageFormat.setHeight(originalHeight);
    imageFormat.setName(imagefile);

    cursor.insertImage(imageFormat);
    cursor.insertText("\n\n");
}
