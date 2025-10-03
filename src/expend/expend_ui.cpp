#include "expend.h"

#include "ui_expend.h"
#include "cmakeconfig.h"

//-------------------------------------------------------------------------
//----------------------------------????--------------------------------
//-------------------------------------------------------------------------

//???????
void Expend::init_expend()
{
    this->setWindowTitle(jtr("expend window"));                                                //??
    ui->tabWidget->setTabText(window_map[INTRODUCTION_WINDOW], jtr("introduction"));           //????
    ui->tabWidget->setTabText(window_map[MODELINFO_WINDOW], jtr("model info"));                //????
    ui->tabWidget->setTabText(window_map[MODELCARD_WINDOW], jtr("model download"));            // ????
    ui->tabWidget->setTabText(window_map[MODELCONVERT_WINDOW], jtr("model") + jtr("convert")); //????
    ui->tabWidget->setTabText(window_map[QUANTIZE_WINDOW], jtr("model") + jtr("quantize"));    //????
    ui->tabWidget->setTabText(window_map[MCP_WINDOW], jtr("mcp_server"));                      //????
    ui->tabWidget->setTabText(window_map[KNOWLEDGE_WINDOW], jtr("knowledge"));                 //???
    ui->tabWidget->setTabText(window_map[TXT2IMG_WINDOW], jtr("text2image"));                  //???
    ui->tabWidget->setTabText(window_map[WHISPER_WINDOW], jtr("speech2text"));                 //???
    ui->tabWidget->setTabText(window_map[TTS_WINDOW], jtr("text2speech"));                     //???

    //????
    ui->vocab_groupBox->setTitle(jtr("vocab_groupBox_title"));
    ui->brain_groupBox->setTitle(jtr("brain_groupBox_title"));
    ui->modellog_groupBox->setTitle(jtr("model log"));

    //????
    showReadme();

    //????
    ui->modelconvert_modeltype_label->setText(jtr("modelconvert_modeltype_label_text"));
    ui->modelconvert_script_label->setText(jtr("modelconvert_script_label_text"));
    ui->modelconvert_modelpath_label->setText(jtr("modelconvert_modelpath_label_text"));
    ui->modelconvert_converttype_label->setText(jtr("modelconvert_converttype_label_text"));
    ui->modelconvert_outputname_label->setText(jtr("modelconvert_outputname_label_text"));
    ui->modelconvert_modelpath_lineEdit->setPlaceholderText(jtr("modelconvert_modelpath_lineEdit_placeholder"));
    ui->modelconvert_outputname_lineEdit->setPlaceholderText(jtr("modelconvert_outputname_lineEdit_placeholder"));
    ui->modelconvert_log->setPlaceholderText(jtr("modelconvert_log_placeholder"));
    ui->modelconvert_log_groupBox->setTitle(jtr("log"));

    if (ui->modelconvert_exec_pushButton->text() == wordsObj["exec convert"].toArray()[0].toString() || ui->modelconvert_exec_pushButton->text() == wordsObj["exec convert"].toArray()[1].toString()) { ui->modelconvert_exec_pushButton->setText(jtr("exec convert")); }
    else
    {
        ui->modelconvert_exec_pushButton->setText(jtr("shut down"));
    }
    //????
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
    //???
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")});   //????
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{jtr("embedless text segment")}); //????
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
    //???
    ui->sd_pathset_groupBox->setTitle(jtr("path set"));
    ui->sd_paramsset_groupBox->setTitle(jtr("params set"));
    ui->sd_prompt_groupBox->setTitle(jtr("prompt"));
    ui->sd_result_groupBox->setTitle(jtr("result"));
    ui->sd_log_groupBox->setTitle(jtr("log"));

    ui->sd_modelpath_label->setText(jtr("diffusion") + jtr("model"));
    ui->sd_vaepath_label->setText("vae " + jtr("model"));
    ui->sd_clip_l_path_label->setText("clip_l " + jtr("model"));
    ui->sd_clip_g_path_label->setText("clip_g " + jtr("model"));
    ui->sd_t5path_label->setText("t5 " + jtr("model"));
    ui->sd_lorapath_label->setText("lora " + jtr("model"));
    ui->sd_modelpath_lineEdit->setPlaceholderText(jtr("sd_modelpath_lineEdit_placeholder"));
    ui->sd_vaepath_lineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    ui->sd_clip_l_path_lineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    ui->sd_clip_g_path_lineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    ui->sd_t5path_lineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    ui->sd_lorapath_lineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));

    ui->params_template_label->setText(jtr("params template"));
    ui->sd_imagewidth_label->setText(jtr("image width"));
    ui->sd_imageheight_label->setText(jtr("image height"));
    ui->sd_sampletype_label->setText(jtr("sample type"));
    ui->sd_samplesteps_label->setText(jtr("sample steps"));
    ui->sd_cfg_label->setText(jtr("cfg scale"));
    ui->sd_imagenums_label->setText(jtr("image nums"));
    ui->sd_seed_label->setText(jtr("seed"));
    ui->sd_clipskip_label->setText(jtr("clip skip"));
    ui->sd_negative_label->setText(jtr("negative"));
    ui->sd_negative_lineEdit->setPlaceholderText(jtr("sd_negative_lineEdit_placeholder"));
    ui->sd_modify_label->setText(jtr("modify"));
    ui->sd_modify_lineEdit->setPlaceholderText(jtr("sd_modify_lineEdit_placeholder"));

    ui->sd_prompt_textEdit->setPlaceholderText(jtr("sd_prompt_textEdit_placeholder"));
    if (ui->sd_draw_pushButton->text() == wordsObj["text to image"].toArray()[0].toString() || ui->sd_draw_pushButton->text() == wordsObj["text to image"].toArray()[1].toString()) { ui->sd_draw_pushButton->setText(jtr("text to image")); }
    else
    {
        ui->sd_draw_pushButton->setText("stop");
    }
    if (ui->sd_img2img_pushButton->text() == wordsObj["image to image"].toArray()[0].toString() || ui->sd_img2img_pushButton->text() == wordsObj["image to image"].toArray()[1].toString()) { ui->sd_img2img_pushButton->setText(jtr("image to image")); }
    else
    {
        ui->sd_img2img_pushButton->setText("stop");
    }
    ui->sd_img2img_lineEdit->setPlaceholderText(jtr("sd_img2img_lineEdit_placeholder"));

    ui->sd_log->setPlainText(jtr("sd_log_plainText"));

    //???
    ui->whisper_modelpath_label->setText(jtr("whisper path"));
    ui->whisper_load_modelpath_linedit->setPlaceholderText(jtr("whisper_load_modelpath_linedit_placeholder"));
    ui->speech_load_groupBox_4->setTitle("whisper " + jtr("log"));
    ui->whisper_wav2text_label->setText(jtr("wav2text"));
    ui->whisper_wavpath_pushButton->setText(jtr("wav path"));
    ui->whisper_format_label->setText(jtr("format"));
    ui->whisper_execute_pushbutton->setText(jtr("exec convert"));

    //???
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
    ui->speech_enable_radioButton->setChecked(speech_params.enable_speech);

    // mcp???
    ui->mcp_server_state_groupBox->setTitle(jtr("mcp_available_servers"));
    ui->mcp_server_config_groupBox->setTitle(jtr("mcp_server_config"));
    ui->mcp_server_reflash_pushButton->setText(jtr("link"));
    ui->mcp_server_config_textEdit->setPlaceholderText(jtr("mcp_server_config_textEdit placehold"));

    // ???
    if (language_flag == 0)
    {
        ui->model_card->openCsv(":/model_card_zh.csv"); //????
    }
    else
    {
        ui->model_card->openCsv(":/model_card_en.csv"); //????
    }
}

//??????????
// 0????,1????
void Expend::on_tabWidget_tabBarClicked(int index)
{
    if (index == window_map[INTRODUCTION_WINDOW] && is_first_show_info) //????????
    {
        is_first_show_info = false;

        //??readme??
        showReadme();

        //??????
        QTimer::singleShot(0, this, [this]() {
            ui->info_card->verticalScrollBar()->setValue(0);
            ui->info_card->horizontalScrollBar()->setValue(0);
        });
    }
    else if (index == window_map[QUANTIZE_WINDOW] && is_first_show_modelproliferation) //?????????
    {
        is_first_show_modelproliferation = false;
        show_quantize_types();
    }
    else if (index == window_map[MODELINFO_WINDOW] && is_first_show_modelinfo) //???????????
    {
        is_first_show_modelinfo = false;
        ui->vocab_card->setPlainText(vocab); //????????
    }
    else if (index == window_map[MODELCARD_WINDOW] && is_first_show_modelcard) //???????????
    {
        is_first_show_modelcard = false;
        if (language_flag == 0)
        {
            ui->model_card->openCsv(":/model_card_zh.csv"); //????
        }
        else
        {
            ui->model_card->openCsv(":/model_card_en.csv"); //????
        }
    }
}

// ??????
void Expend::recv_vocab(QString vocab_)
{
    vocab = vocab_;
    if (!is_first_show_modelinfo) { ui->vocab_card->setPlainText(vocab); }
    init_brain_matrix();
    reflash_brain_matrix();
}

//????????
void Expend::recv_expend_show(EXPEND_WINDOW window)
{
    if (window == NO_WINDOW)
    {
        this->close();
        return;
    }
    if (is_first_show_expend) //???????
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

    //???????
    QScrollBar *vScrollBar = ui->vocab_card->verticalScrollBar();
    vScrollBar->setValue(vScrollBar->maximum());
    //????????
    ui->tabWidget->setCurrentIndex(window_map[window]);
    this->setWindowState(Qt::WindowActive); // ???????????
    this->setWindowFlags(Qt::Window);
    this->show();
    this->raise();
    this->activateWindow(); // ??????
}

QString Expend::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);

    return filepath;
}

//???????
void Expend::recv_language(int language_flag_)
{
    language_flag = language_flag_;
    init_expend();
}

//?????????
void Expend::readConfig()
{
    QFile configfile(applicationDirPath + "/EVA_TEMP/eva_config.ini");

    //??????????????????
    QString default_sd_modelpath = applicationDirPath + DEFAULT_SD_MODEL_PATH;
    QString default_sd_params_template = "sd1.5-anything-3";
    QFile default_sd_modelpath_file(default_sd_modelpath);
    if (!default_sd_modelpath_file.exists())
    {
        default_sd_modelpath = "";
        default_sd_params_template = "custom1";
    } //????????

    QString default_whisper_modelpath = applicationDirPath + DEFAULT_WHISPER_MODEL_PATH;
    QFile default_whisper_modelpath_file(default_whisper_modelpath);
    if (!default_whisper_modelpath_file.exists()) { default_whisper_modelpath = ""; } //????????

    QString default_outetts_modelpath = applicationDirPath + DEFAULT_OUTETTS_MODEL_PATH;
    QFile default_outetts_modelpath_file(default_outetts_modelpath);
    if (!default_outetts_modelpath_file.exists()) { default_outetts_modelpath = ""; } //????????

    QString default_WavTokenizer_modelpath = applicationDirPath + DEFAULT_WAVTOKENIZER_MODEL_PATH;
    QFile default_WavTokenizer_modelpath_file(default_WavTokenizer_modelpath);
    if (!default_WavTokenizer_modelpath_file.exists()) { default_WavTokenizer_modelpath = ""; } //????????

    // ?????????
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    QString sd_params_template = settings.value("sd_params_template", default_sd_params_template).toString(); // ????????custom1
    QString sd_modelpath = settings.value("sd_modelpath", default_sd_modelpath).toString();                   // sd????
    QString vae_modelpath = settings.value("vae_modelpath", "").toString();                                   // vae????
    QString clip_l_modelpath = settings.value("clip_l_modelpath", "").toString();                             // clip_l????
    QString clip_g_modelpath = settings.value("clip_g_modelpath", "").toString();                             // clip_g????
    QString t5_modelpath = settings.value("t5_modelpath", "").toString();                                     // t5????
    QString lora_modelpath = settings.value("lora_modelpath", "").toString();                                 // lora????

    QString sd_prompt = settings.value("sd_prompt", "").toString(); // sd???

    QString whisper_modelpath = settings.value("whisper_modelpath", default_whisper_modelpath).toString(); // whisper????

    speech_params.enable_speech = settings.value("speech_enable", "").toBool();                                           //????????
    speech_params.speech_name = settings.value("speech_name", "").toString();                                             //???
    QString outetts_modelpath = settings.value("outetts_modelpath", default_outetts_modelpath).toString();                // outetts????
    QString wavtokenizer_modelpath = settings.value("wavtokenizer_modelpath", default_WavTokenizer_modelpath).toString(); // wavtokenizer????
    ui->speech_outetts_modelpath_lineEdit->setText(outetts_modelpath);
    ui->speech_wavtokenizer_modelpath_lineEdit->setText(wavtokenizer_modelpath);

    ui->mcp_server_config_textEdit->setText(settings.value("Mcpconfig", "").toString()); //mcp??

    // ???
    QFile sd_modelpath_file(sd_modelpath);
    if (sd_modelpath_file.exists())
    {
        ui->sd_modelpath_lineEdit->setText(sd_modelpath);
    }

    QFile vae_modelpath_file(vae_modelpath);
    if (vae_modelpath_file.exists())
    {
        ui->sd_vaepath_lineEdit->setText(vae_modelpath);
    }

    QFile clip_l_modelpath_file(clip_l_modelpath);
    if (clip_l_modelpath_file.exists())
    {
        ui->sd_clip_l_path_lineEdit->setText(clip_l_modelpath);
    }

    QFile clip_g_modelpath_file(clip_g_modelpath);
    if (clip_g_modelpath_file.exists())
    {
        ui->sd_clip_g_path_lineEdit->setText(clip_g_modelpath);
    }

    QFile t5_modelpath_file(t5_modelpath);
    if (t5_modelpath_file.exists())
    {
        ui->sd_t5path_lineEdit->setText(t5_modelpath);
    }

    QFile lora_modelpath_file(lora_modelpath);
    if (lora_modelpath_file.exists())
    {
        ui->sd_lorapath_lineEdit->setText(lora_modelpath);
    }

    sd_params_templates["custom1"].batch_count = settings.value("sd_custom1_image_nums", 1).toInt();
    sd_params_templates["custom1"].cfg_scale = settings.value("sd_custom1_cfg", 7.5).toFloat();
    sd_params_templates["custom1"].clip_skip = settings.value("sd_custom1_clip_skip", -1).toInt();
    sd_params_templates["custom1"].height = settings.value("sd_custom1_image_height", 512).toInt();
    sd_params_templates["custom1"].width = settings.value("sd_custom1_image_width", 512).toInt();
    sd_params_templates["custom1"].seed = settings.value("sd_custom1_seed", -1).toInt();
    sd_params_templates["custom1"].steps = settings.value("sd_custom1_sample_steps", 20).toInt();
    sd_params_templates["custom1"].sample_type = settings.value("sd_custom1_sample_type", "euler").toString();
    sd_params_templates["custom1"].negative_prompt = settings.value("sd_custom1_negative", "").toString();
    sd_params_templates["custom1"].modify_prompt = settings.value("sd_custom1_modify", "").toString();

    sd_params_templates["custom2"].batch_count = settings.value("sd_custom2_image_nums", 1).toInt();
    sd_params_templates["custom2"].cfg_scale = settings.value("sd_custom2_cfg", 7.5).toFloat();
    sd_params_templates["custom2"].clip_skip = settings.value("sd_custom2_clip_skip", -1).toInt();
    sd_params_templates["custom2"].height = settings.value("sd_custom2_image_height", 512).toInt();
    sd_params_templates["custom2"].width = settings.value("sd_custom2_image_width", 512).toInt();
    sd_params_templates["custom2"].seed = settings.value("sd_custom2_seed", -1).toInt();
    sd_params_templates["custom2"].steps = settings.value("sd_custom2_sample_steps", 20).toInt();
    sd_params_templates["custom2"].sample_type = settings.value("sd_custom2_sample_type", "euler").toString();
    sd_params_templates["custom2"].negative_prompt = settings.value("sd_custom2_negative", "").toString();
    sd_params_templates["custom2"].modify_prompt = settings.value("sd_custom2_modify", "").toString();

    ui->params_template_comboBox->setCurrentText(sd_params_template); // ????
    sd_apply_template(sd_params_templates[ui->params_template_comboBox->currentText()]);
    is_readconfig = true;

    QFile whisper_load_modelpath_file(whisper_modelpath);
    if (whisper_load_modelpath_file.exists())
    {
        ui->whisper_load_modelpath_linedit->setText(whisper_modelpath);
        whisper_params.model = whisper_modelpath.toStdString();
    }

    //?????main.cpp???????
    ui->embedding_txt_describe_lineEdit->setText(settings.value("embedding_describe", "").toString()); //?????
    ui->embedding_split_spinbox->setValue(settings.value("embedding_split", DEFAULT_EMBEDDING_SPLITLENTH).toInt());
    ui->embedding_resultnumb_spinBox->setValue(settings.value("embedding_resultnumb", DEFAULT_EMBEDDING_RESULTNUMB).toInt());
    ui->embedding_overlap_spinbox->setValue(settings.value("embedding_overlap", DEFAULT_EMBEDDING_OVERLAP).toInt());
    ui->embedding_dim_spinBox->setValue(settings.value("embedding_dim", 1024).toInt());

    QString embedding_sourcetxt = settings.value("embedding_sourcetxt", "").toString(); //?????
    QFile embedding_sourcetxt_file(embedding_sourcetxt);
    if (embedding_sourcetxt_file.exists())
    {
        txtpath = embedding_sourcetxt;
        ui->embedding_txt_lineEdit->setText(txtpath);
        preprocessTXT(); //???????
    }
}

void Expend::showReadme()
{
    QString readme_content;
    QFile file;
    QString imagefile = ":/logo/ui_demo.png"; // ??????

    // ??????????? README ??
    if (language_flag == 0)
    {
        file.setFileName(":/README.md");
    }
    else if (language_flag == 1)
    {
        file.setFileName(":/README_en.md");
    }

    // ?????????
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        in.setCodec("UTF-8");
        readme_content = in.readAll();
        file.close();
    }

    // ???????????? HTML ??
    QRegularExpression imgRegex("<img src=\"https://github.com/ylsdamxssjxxdd/eva/assets/[^>]+>");
    readme_content.remove(imgRegex);

    // ?? <summary> ? </summary> ??
    readme_content.remove(QRegularExpression("<summary>|</summary>"));

    // ?? <details> ? </details> ??
    readme_content.remove("<details>");
    readme_content.remove("</details>");

    // ??????
    QString compileInfo = QString("%1: %2\n\n %3: %4\n\n %5: %6\n\n %7: %8\n\n %9: %10\n\n")
                              .arg(jtr("EVA_ENVIRONMENT"), QString(EVA_ENVIRONMENT))
                              .arg(jtr("EVA_PRODUCT_TIME"), QString(EVA_PRODUCT_TIME))
                              .arg(jtr("QT_VERSION_"), QString(QT_VERSION_))
                              .arg(jtr("COMPILE_VERSION"), QString(COMPILE_VERSION))
                              .arg(jtr("EVA_VERSION"), QString(EVA_VERSION));
    readme_content.prepend(compileInfo); // ????????????

    // ?? Markdown ??
    ui->info_card->setMarkdown(readme_content);

    // ???????
    QImage image(imagefile);
    int originalWidth = image.width() / devicePixelRatioF() / 1.5;
    int originalHeight = image.height() / devicePixelRatioF() / 1.5;

    // ????? QTextEdit
    QTextCursor cursor(ui->info_card->textCursor());
    cursor.movePosition(QTextCursor::Start);
    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);
    imageFormat.setHeight(originalHeight);
    imageFormat.setName(imagefile);

    cursor.insertImage(imageFormat);
    cursor.insertText("\n\n");
}

