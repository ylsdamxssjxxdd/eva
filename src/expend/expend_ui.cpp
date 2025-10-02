#include "expend.h"

#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------界面相关--------------------------------
//-------------------------------------------------------------------------

//初始化增殖窗口
void Expend::init_expend()
{
    this->setWindowTitle(jtr("expend window"));                                                //标题
    ui->tabWidget->setTabText(window_map[INTRODUCTION_WINDOW], jtr("introduction"));           //软件介绍
    ui->tabWidget->setTabText(window_map[MODELINFO_WINDOW], jtr("model info"));                //模型信息
    ui->tabWidget->setTabText(window_map[MODELCARD_WINDOW], jtr("model download"));            // 模型下载
    ui->tabWidget->setTabText(window_map[MODELCONVERT_WINDOW], jtr("model") + jtr("convert")); //模型转换
    ui->tabWidget->setTabText(window_map[QUANTIZE_WINDOW], jtr("model") + jtr("quantize"));    //模型量化
    ui->tabWidget->setTabText(window_map[MCP_WINDOW], jtr("mcp_server"));                      //软件介绍
    ui->tabWidget->setTabText(window_map[KNOWLEDGE_WINDOW], jtr("knowledge"));                 //知识库
    ui->tabWidget->setTabText(window_map[TXT2IMG_WINDOW], jtr("text2image"));                  //文生图
    ui->tabWidget->setTabText(window_map[WHISPER_WINDOW], jtr("speech2text"));                 //声转文
    ui->tabWidget->setTabText(window_map[TTS_WINDOW], jtr("text2speech"));                     //文转声

    //模型信息
    ui->vocab_groupBox->setTitle(jtr("vocab_groupBox_title"));
    ui->brain_groupBox->setTitle(jtr("brain_groupBox_title"));
    ui->modellog_groupBox->setTitle(jtr("model log"));

    //软件介绍
    showReadme();

    //模型转换
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
    //模型量化
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
    //知识库
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")});   //设置列名
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{jtr("embedless text segment")}); //设置列名
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
    //文生图
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

    //声转文
    ui->whisper_modelpath_label->setText(jtr("whisper path"));
    ui->whisper_load_modelpath_linedit->setPlaceholderText(jtr("whisper_load_modelpath_linedit_placeholder"));
    ui->speech_load_groupBox_4->setTitle("whisper " + jtr("log"));
    ui->whisper_wav2text_label->setText(jtr("wav2text"));
    ui->whisper_wavpath_pushButton->setText(jtr("wav path"));
    ui->whisper_format_label->setText(jtr("format"));
    ui->whisper_execute_pushbutton->setText(jtr("exec convert"));

    //文转声
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

    // mcp服务器
    ui->mcp_server_state_groupBox->setTitle(jtr("mcp_available_servers"));
    ui->mcp_server_config_groupBox->setTitle(jtr("mcp_server_config"));
    ui->mcp_server_reflash_pushButton->setText(jtr("link"));
    ui->mcp_server_config_textEdit->setPlaceholderText(jtr("mcp_server_config_textEdit placehold"));

    // 模型卡
    if (language_flag == 0)
    {
        ui->model_card->openCsv(":/model_card_zh.csv"); //更新视图
    }
    else
    {
        ui->model_card->openCsv(":/model_card_en.csv"); //更新视图
    }
}

//用户切换选项卡时响应
// 0软件介绍,1模型信息
void Expend::on_tabWidget_tabBarClicked(int index)
{
    if (index == window_map[INTRODUCTION_WINDOW] && is_first_show_info) //第一次点软件介绍
    {
        is_first_show_info = false;

        //展示readme内容
        showReadme();

        //强制延迟见顶
        QTimer::singleShot(0, this, [this]() {
            ui->info_card->verticalScrollBar()->setValue(0);
            ui->info_card->horizontalScrollBar()->setValue(0);
        });
    }
    else if (index == window_map[QUANTIZE_WINDOW] && is_first_show_modelproliferation) //第一次展示量化方法
    {
        is_first_show_modelproliferation = false;
        show_quantize_types();
    }
    else if (index == window_map[MODELINFO_WINDOW] && is_first_show_modelinfo) //第一次展示模型信息窗口
    {
        is_first_show_modelinfo = false;
        ui->vocab_card->setPlainText(vocab); //更新一次模型词表
    }
    else if (index == window_map[MODELCARD_WINDOW] && is_first_show_modelcard) //第一次展示模型信息窗口
    {
        is_first_show_modelcard = false;
        if (language_flag == 0)
        {
            ui->model_card->openCsv(":/model_card_zh.csv"); //更新视图
        }
        else
        {
            ui->model_card->openCsv(":/model_card_en.csv"); //更新视图
        }
    }
}

// 接收模型词表
void Expend::recv_vocab(QString vocab_)
{
    vocab = vocab_;
    if (!is_first_show_modelinfo) { ui->vocab_card->setPlainText(vocab); }
    init_brain_matrix();
    reflash_brain_matrix();
}

//通知显示增殖窗口
void Expend::recv_expend_show(EXPEND_WINDOW window)
{
    if (window == NO_WINDOW)
    {
        this->close();
        return;
    }
    if (is_first_show_expend) //第一次显示的话
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

    //词表滑动到底部
    QScrollBar *vScrollBar = ui->vocab_card->verticalScrollBar();
    vScrollBar->setValue(vScrollBar->maximum());
    //打开指定页数窗口
    ui->tabWidget->setCurrentIndex(window_map[window]);
    this->setWindowState(Qt::WindowActive); // 激活窗口并恢复正常状态
    this->setWindowFlags(Qt::Window);
    this->show();
    this->raise();
    this->activateWindow(); // 激活增殖窗口
}

QString Expend::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);

    return filepath;
}

//传递使用的语言
void Expend::recv_language(int language_flag_)
{
    language_flag = language_flag_;
    init_expend();
}

//读取配置文件并应用
void Expend::readConfig()
{
    QFile configfile(applicationDirPath + "/EVA_TEMP/eva_config.ini");

    //如果默认要填入的模型存在，则默认填入
    QString default_sd_modelpath = applicationDirPath + DEFAULT_SD_MODEL_PATH;
    QString default_sd_params_template = "sd1.5-anything-3";
    QFile default_sd_modelpath_file(default_sd_modelpath);
    if (!default_sd_modelpath_file.exists())
    {
        default_sd_modelpath = "";
        default_sd_params_template = "custom1";
    } //不存在则默认为空

    QString default_whisper_modelpath = applicationDirPath + DEFAULT_WHISPER_MODEL_PATH;
    QFile default_whisper_modelpath_file(default_whisper_modelpath);
    if (!default_whisper_modelpath_file.exists()) { default_whisper_modelpath = ""; } //不存在则默认为空

    QString default_outetts_modelpath = applicationDirPath + DEFAULT_OUTETTS_MODEL_PATH;
    QFile default_outetts_modelpath_file(default_outetts_modelpath);
    if (!default_outetts_modelpath_file.exists()) { default_outetts_modelpath = ""; } //不存在则默认为空

    QString default_WavTokenizer_modelpath = applicationDirPath + DEFAULT_WAVTOKENIZER_MODEL_PATH;
    QFile default_WavTokenizer_modelpath_file(default_WavTokenizer_modelpath);
    if (!default_WavTokenizer_modelpath_file.exists()) { default_WavTokenizer_modelpath = ""; } //不存在则默认为空

    // 读取配置文件中的值
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    QString sd_params_template = settings.value("sd_params_template", default_sd_params_template).toString(); // 参数模板，默认是custom1
    QString sd_modelpath = settings.value("sd_modelpath", default_sd_modelpath).toString();                   // sd模型路径
    QString vae_modelpath = settings.value("vae_modelpath", "").toString();                                   // vae模型路径
    QString clip_l_modelpath = settings.value("clip_l_modelpath", "").toString();                             // clip_l模型路径
    QString clip_g_modelpath = settings.value("clip_g_modelpath", "").toString();                             // clip_g模型路径
    QString t5_modelpath = settings.value("t5_modelpath", "").toString();                                     // t5模型路径
    QString lora_modelpath = settings.value("lora_modelpath", "").toString();                                 // lora模型路径

    QString sd_prompt = settings.value("sd_prompt", "").toString(); // sd提示词

    QString whisper_modelpath = settings.value("whisper_modelpath", default_whisper_modelpath).toString(); // whisper模型路径

    speech_params.enable_speech = settings.value("speech_enable", "").toBool();                                           //是否启用语音朗读
    speech_params.speech_name = settings.value("speech_name", "").toString();                                             //朗读者
    QString outetts_modelpath = settings.value("outetts_modelpath", default_outetts_modelpath).toString();                // outetts模型路径
    QString wavtokenizer_modelpath = settings.value("wavtokenizer_modelpath", default_WavTokenizer_modelpath).toString(); // wavtokenizer模型路径
    ui->speech_outetts_modelpath_lineEdit->setText(outetts_modelpath);
    ui->speech_wavtokenizer_modelpath_lineEdit->setText(wavtokenizer_modelpath);

    ui->mcp_server_config_textEdit->setText(settings.value("Mcpconfig", "").toString()); //mcp配置

    // 应用值
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

    ui->params_template_comboBox->setCurrentText(sd_params_template); // 应用模板
    sd_apply_template(sd_params_templates[ui->params_template_comboBox->currentText()]);
    is_readconfig = true;

    QFile whisper_load_modelpath_file(whisper_modelpath);
    if (whisper_load_modelpath_file.exists())
    {
        ui->whisper_load_modelpath_linedit->setText(whisper_modelpath);
        whisper_params.model = whisper_modelpath.toStdString();
    }

    //知识库，在main.cpp里有启动的部分
    ui->embedding_txt_describe_lineEdit->setText(settings.value("embedding_describe", "").toString()); //知识库描述
    ui->embedding_split_spinbox->setValue(settings.value("embedding_split", DEFAULT_EMBEDDING_SPLITLENTH).toInt());
    ui->embedding_resultnumb_spinBox->setValue(settings.value("embedding_resultnumb", DEFAULT_EMBEDDING_RESULTNUMB).toInt());
    ui->embedding_overlap_spinbox->setValue(settings.value("embedding_overlap", DEFAULT_EMBEDDING_OVERLAP).toInt());
    ui->embedding_dim_spinBox->setValue(settings.value("embedding_dim", 1024).toInt());

    QString embedding_sourcetxt = settings.value("embedding_sourcetxt", "").toString(); //源文档路径
    QFile embedding_sourcetxt_file(embedding_sourcetxt);
    if (embedding_sourcetxt_file.exists())
    {
        txtpath = embedding_sourcetxt;
        ui->embedding_txt_lineEdit->setText(txtpath);
        preprocessTXT(); //预处理文件内容
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

    // 使用正则表达式删除指定的 HTML 内容
    QRegularExpression imgRegex("<img src=\"https://github.com/ylsdamxssjxxdd/eva/assets/[^>]+>");
    readme_content.remove(imgRegex);

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
