#include "expend.h"

#include "ui_expend.h"

Expend::Expend(QWidget *parent, QString applicationDirPath_)
    : QWidget(parent), ui(new Ui::Expend)
{
    ui->setupUi(this);
    applicationDirPath = applicationDirPath_;

    //初始化选项卡
    ui->info_card->setReadOnly(1);                                   // 只读
    ui->vocab_card->setReadOnly(1);                                  // 只读
    ui->modellog_card->setReadOnly(1);                               // 只读
    ui->tabWidget->setCurrentIndex(0);                               //默认显示模机体介绍窗口
    ui->sd_prompt_textEdit->setContextMenuPolicy(Qt::NoContextMenu); //取消右键菜单
    ui->sd_prompt_textEdit->installEventFilter(this);                //安装事件过滤器
    ui->sd_negative_lineEdit->installEventFilter(this);              //安装事件过滤器
    ui->sd_modify_lineEdit->installEventFilter(this);                //安装事件过滤器
    ui->sd_img2img_lineEdit->installEventFilter(this);               //安装事件过滤器

    ui->vocab_card->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                   //灰色
    ui->modellog_card->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                //灰色
    ui->whisper_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                  //灰色
    ui->embedding_test_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");           //灰色
    ui->embedding_test_result->setStyleSheet("background-color: rgba(128, 128, 128, 200);");        //灰色
    ui->model_quantize_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");           //灰色
    ui->sd_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                       //灰色
    ui->speech_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                   //灰色
    ui->modelconvert_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");             //灰色
    ui->mcp_server_log_plainTextEdit->setStyleSheet("background-color: rgba(128, 128, 128, 200);"); //灰色
    ui->mcp_server_config_textEdit->setLineWrapMode(QTextEdit::NoWrap);                             // 禁用自动换行
    ui->mcp_server_log_plainTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);                      // 禁用自动换行
    ui->modellog_card->setLineWrapMode(QPlainTextEdit::NoWrap);                                     // 禁用自动换行
    ui->embedding_test_log->setLineWrapMode(QPlainTextEdit::NoWrap);                                // 禁用自动换行
    ui->sd_log->setLineWrapMode(QPlainTextEdit::NoWrap);                                            // 禁用自动换行
    ui->vocab_card->setLineWrapMode(QPlainTextEdit::NoWrap);                                        // 禁用自动换行
    ui->speech_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    ui->model_quantize_info->setStyleSheet("QTableWidget::item:selected { background-color: #FFA500; }"); // 设置选中行的颜色为橘黄色
    ui->splitter->setStretchFactor(0, 2);                                                                 //设置分隔器中第一个元素初始宽度占比为3
    ui->splitter->setStretchFactor(1, 1);                                                                 //设置分隔器中第二个元素初始宽度占比为1
    //模型转换相关
    // ui->modelconvert_modeltype_comboBox->addItems({modeltype_map[MODEL_TYPE_LLM],modeltype_map[MODEL_TYPE_WHISPER],modeltype_map[MODEL_TYPE_SD],modeltype_map[MODEL_TYPE_OUTETTS]});
    ui->modelconvert_modeltype_comboBox->addItems({modeltype_map[MODEL_TYPE_LLM]});
    ui->modelconvert_converttype_comboBox->addItems({modelquantize_map[MODEL_QUANTIZE_F32], modelquantize_map[MODEL_QUANTIZE_F16], modelquantize_map[MODEL_QUANTIZE_BF16], modelquantize_map[MODEL_QUANTIZE_Q8_0]});
    ui->modelconvert_converttype_comboBox->setCurrentText(modelquantize_map[MODEL_QUANTIZE_F16]); //默认转换为f16的模型
    ui->modelconvert_script_comboBox->addItems({CONVERT_HF_TO_GGUF_SCRIPT});
    convert_command_process = new QProcess(this);
    connect(convert_command_process, &QProcess::started, this, &Expend::convert_command_onProcessStarted);                                             //连接开始信号
    connect(convert_command_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::convert_command_onProcessFinished); //连接结束信号
    connect(convert_command_process, &QProcess::readyReadStandardOutput, this, &Expend::readyRead_convert_command_process_StandardOutput);
    connect(convert_command_process, &QProcess::readyReadStandardError, this, &Expend::readyRead_convert_command_process_StandardError);

    //塞入第三方exe
    server_process = new QProcess(this);                                                                                             // 创建一个QProcess实例用来启动llama-server
    connect(server_process, &QProcess::started, this, &Expend::server_onProcessStarted);                                             //连接开始信号
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::server_onProcessFinished); //连接结束信号

    quantize_process = new QProcess(this);                                                                                               // 创建一个QProcess实例用来启动llama-quantize
    connect(quantize_process, &QProcess::started, this, &Expend::quantize_onProcessStarted);                                             //连接开始信号
    connect(quantize_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::quantize_onProcessFinished); //连接结束信号

    whisper_process = new QProcess(this);                                                                                              //实例化
    connect(whisper_process, &QProcess::started, this, &Expend::whisper_onProcessStarted);                                             //连接开始信号
    connect(whisper_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::whisper_onProcessFinished); //连接结束信号

    sd_process = new QProcess(this);                                                                                         // 创建一个QProcess实例用来启动llama-quantize
    connect(sd_process, &QProcess::started, this, &Expend::sd_onProcessStarted);                                             //连接开始信号
    connect(sd_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::sd_onProcessFinished); //连接结束信号

    outetts_process = new QProcess(this);
    connect(outetts_process, &QProcess::started, this, &Expend::outetts_onProcessStarted);                                             //连接开始信号
    connect(outetts_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::outetts_onProcessFinished); //连接结束信号
    connect(outetts_process, &QProcess::readyReadStandardOutput, this, &Expend::readyRead_outetts_process_StandardOutput);
    connect(outetts_process, &QProcess::readyReadStandardError, this, &Expend::readyRead_outetts_process_StandardError);

    ui->embedding_txt_wait->setContextMenuPolicy(Qt::CustomContextMenu); //添加右键菜单
    connect(ui->embedding_txt_wait, &QTableWidget::customContextMenuRequested, this, &Expend::show_embedding_txt_wait_menu);

    //知识库相关
    ui->embedding_txt_wait->setColumnCount(1);                                              //设置一列
    ui->embedding_txt_over->setColumnCount(1);                                              //设置一列
    ui->embedding_txt_wait->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // 充满
    ui->embedding_txt_over->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // 充满
    connect(server_process, &QProcess::readyReadStandardOutput, this, &Expend::readyRead_server_process_StandardOutput);
    connect(server_process, &QProcess::readyReadStandardError, this, &Expend::readyRead_server_process_StandardError);

    //添加采样算法
    ui->sd_sampletype->addItems({"euler", "euler_a", "heun", "dpm2", "dpm++2s_a", "dpm++2m", "dpm++2mv2", "lcm"});
    ui->sd_sampletype->setCurrentText("euler");
    //添加输出格式
    ui->whisper_output_format->addItems({"txt", "srt", "csv", "json"});

    // 文生图相关
    // 构建模板 sd1.5-anything-3,sdxl-animagine-3.1,sd3.5-large,flux1-dev,custom1,custom2

    SD_PARAMS sd_sd1_5_anything_3_template{"euler_a", "EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure", "masterpieces, best quality, beauty, detailed, Pixar, 8k", 512, 512, 20, 1, -1, 2, 7.5};
    SD_PARAMS sd_sdxl_animagine_3_1_template{"euler_a", "nsfw, lowres, (bad), text, error, fewer, extra, missing, worst quality, jpeg artifacts, low quality, watermark, unfinished, displeasing, oldest, early, chromatic aberration, signature, extra digits, artistic error, username, scan, [abstract]", "masterpiece, best quality", 768, 768, 30, 1, -1, 2, 7.5};
    SD_PARAMS sd_sd3_5_large_template{"euler", "", "", 768, 768, 20, 1, -1, -1, 4.5};
    SD_PARAMS sd_flux1_dev_template{"euler", "", "", 768, 768, 30, 1, -1, -1, 1.0};
    SD_PARAMS sd_custom1_template{"euler", "", "", 512, 512, 20, 1, -1, -1, 7.5};
    SD_PARAMS sd_custom2_template{"euler", "", "", 512, 512, 20, 1, -1, -1, 7.5};

    sd_params_templates.insert("sd1.5-anything-3", sd_sd1_5_anything_3_template);
    sd_params_templates.insert("sdxl-animagine-3.1", sd_sdxl_animagine_3_1_template);
    sd_params_templates.insert("sd3.5-large", sd_sd3_5_large_template);
    sd_params_templates.insert("flux1-dev", sd_flux1_dev_template);
    sd_params_templates.insert("custom1", sd_custom1_template);
    sd_params_templates.insert("custom2", sd_custom2_template);

    for (const auto &key : sd_params_templates.keys())
    {
        ui->params_template_comboBox->addItem(key); // 添加模板选项
    }

    //记忆矩阵相关
    ui->brain_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); //让表格自动撑满所在区域
    ui->brain_tableWidget->verticalHeader()->setVisible(false);                            // 隐藏行头部

    //文转声相关
    connect(ui->speech_enable_radioButton, &QRadioButton::clicked, this, &Expend::speech_enable_change);
    connect(ui->speech_source_comboBox, &QComboBox::currentTextChanged, this, &Expend::speech_source_change);
    sys_speech = new QTextToSpeech(); // 系统声源
    // 检查是否成功创建
#ifndef BODY_USE_32BIT // win7就不用检查了
    if (sys_speech->state() == QTextToSpeech::Ready)
    {
        // 遍历所有可用音色
        foreach (const QVoice &speech, sys_speech->availableVoices())
        {
            avaliable_speech_list << speech.name();
        }
        connect(sys_speech, &QTextToSpeech::stateChanged, this, &Expend::speechOver); //朗读结束后动作
        is_sys_speech_available = true;
    }
    else
    {
        is_sys_speech_available = false;
    }
#endif
    avaliable_speech_list << SPPECH_OUTETTS;     // 模型声源
    this->set_sys_speech(avaliable_speech_list); // 设置可用声源

    // 创建播放器对象
    speech_player = new QMediaPlayer;
    // 连接 mediaStatusChanged 信号到槽函数
    QObject::connect(speech_player, &QMediaPlayer::mediaStatusChanged, this, &Expend::speech_player_over);
    outettsDir = applicationDirPath + "/EVA_TEMP/outetts/"; // outetts生成的音频存放目录
    connect(&speechTimer, SIGNAL(timeout()), this, SLOT(speech_process()));
    connect(&speechPlayTimer, SIGNAL(timeout()), this, SLOT(speech_play_process()));
#ifndef BODY_USE_32BIT

    speechTimer.start(500);     //每半秒检查一次是否需要朗读
    speechPlayTimer.start(500); //每半秒检查一次是否有音频需要朗读
#endif
    //如果存在配置文件则读取它，并且应用，目前主要是文生图/声转文/文转声
    readConfig();
    qDebug() << "expend init over";
}

// 窗口状态变化处理
void Expend::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        if (isMinimized())
        {
            setWindowFlags(Qt::Tool); // 隐藏任务栏条目
        }
    }
}

Expend::~Expend()
{
    delete ui;
    server_process->kill(); //有点问题
    sd_process->kill();
    whisper_process->kill();
    quantize_process->kill();
}

//创建临时文件夹EVA_TEMP
bool Expend::createTempDirectory(const QString &path)
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

//传递llama.cpp的log，显示模型日志
void Expend::recv_llama_log(QString log)
{
    QTextCursor cursor = ui->modellog_card->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    cursor.insertText(log);
    ui->modellog_card->setTextCursor(cursor);
}

// 根据language.json和language_flag中找到对应的文字
QString Expend::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}

//事件过滤器,鼠标跟踪效果不好要在各种控件单独实现
bool Expend::eventFilter(QObject *obj, QEvent *event)
{
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->sd_prompt_textEdit && event->type() == QEvent::ContextMenu)
    {
        // 自动填充提示词
        ui->sd_prompt_textEdit->setText("full body, Ayanami Rei, beautiful face, Blue hair, 1 girl");
        return true;
    }
    else if (obj == ui->sd_negative_lineEdit && event->type() == QEvent::ContextMenu)
    {
        //还原负面词
        ui->sd_negative_lineEdit->setText(sd_params_templates[ui->params_template_comboBox->currentText()].negative_prompt);
        return true;
    }
    else if (obj == ui->sd_modify_lineEdit && event->type() == QEvent::ContextMenu)
    {
        //还原修饰词
        ui->sd_modify_lineEdit->setText(sd_params_templates[ui->params_template_comboBox->currentText()].modify_prompt);
    }
    else if (obj == ui->sd_img2img_lineEdit && event->type() == QEvent::ContextMenu)
    {
        //选择图像
        currentpath = customOpenfile(currentpath, "choose an imgage", "(*.png *.jpg *.bmp)");
        if (currentpath != "")
        {
            ui->sd_img2img_pushButton->setEnabled(1);
            ui->sd_img2img_lineEdit->setText(currentpath);
        }
        else
        {
            ui->sd_img2img_pushButton->setEnabled(0);
        }

        return true;
    }

    return QObject::eventFilter(obj, event);
}

//关闭事件
void Expend::closeEvent(QCloseEvent *event)
{
    //--------------保存当前用户配置---------------
    sd_save_template(ui->params_template_comboBox->currentText());

    // 创建 QSettings 对象，指定配置文件的名称和格式
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    settings.setValue("sd_params_template", ui->params_template_comboBox->currentText());
    settings.setValue("sd_modelpath", ui->sd_modelpath_lineEdit->text());
    settings.setValue("vae_modelpath", ui->sd_vaepath_lineEdit->text());
    settings.setValue("clip_l_modelpath", ui->sd_clip_l_path_lineEdit->text());
    settings.setValue("clip_g_modelpath", ui->sd_clip_g_path_lineEdit->text());
    settings.setValue("t5_modelpath", ui->sd_t5path_lineEdit->text());
    settings.setValue("lora_modelpath", ui->sd_lorapath_lineEdit->text());

    settings.setValue("sd_prompt", ui->sd_prompt_textEdit->toPlainText());
    settings.setValue("sd_params_template", ui->params_template_comboBox->currentText());
    settings.setValue("sd_custom1_negative", sd_params_templates["custom1"].negative_prompt);
    settings.setValue("sd_custom1_modify", sd_params_templates["custom1"].modify_prompt);
    settings.setValue("sd_custom1_image_width", sd_params_templates["custom1"].width);
    settings.setValue("sd_custom1_image_height", sd_params_templates["custom1"].height);
    settings.setValue("sd_custom1_sample_type", sd_params_templates["custom1"].sample_type);
    settings.setValue("sd_custom1_sample_steps", sd_params_templates["custom1"].steps);
    settings.setValue("sd_custom1_cfg", sd_params_templates["custom1"].cfg_scale);
    settings.setValue("sd_custom1_seed", sd_params_templates["custom1"].seed);
    settings.setValue("sd_custom1_image_nums", sd_params_templates["custom1"].batch_count);
    settings.setValue("sd_custom1_clip_skip", sd_params_templates["custom1"].clip_skip);
    settings.setValue("sd_custom2_negative", sd_params_templates["custom2"].negative_prompt);
    settings.setValue("sd_custom2_modify", sd_params_templates["custom2"].modify_prompt);
    settings.setValue("sd_custom2_image_width", sd_params_templates["custom2"].width);
    settings.setValue("sd_custom2_image_height", sd_params_templates["custom2"].height);
    settings.setValue("sd_custom2_sample_type", sd_params_templates["custom2"].sample_type);
    settings.setValue("sd_custom2_sample_steps", sd_params_templates["custom2"].steps);
    settings.setValue("sd_custom2_cfg", sd_params_templates["custom2"].cfg_scale);
    settings.setValue("sd_custom2_seed", sd_params_templates["custom2"].seed);
    settings.setValue("sd_custom2_image_nums", sd_params_templates["custom2"].batch_count);
    settings.setValue("sd_custom2_clip_skip", sd_params_templates["custom2"].clip_skip);

    settings.setValue("whisper_modelpath", ui->whisper_load_modelpath_linedit->text());

    settings.setValue("speech_enable", ui->speech_enable_radioButton->isChecked());
    settings.setValue("speech_name", ui->speech_source_comboBox->currentText());
    settings.setValue("outetts_modelpath", ui->speech_outetts_modelpath_lineEdit->text());
    settings.setValue("wavtokenizer_modelpath", ui->speech_wavtokenizer_modelpath_lineEdit->text());
    settings.setValue("embedding_modelpath", embedding_params.modelpath);
    settings.setValue("embedding_dim", ui->embedding_dim_spinBox->text());
    settings.setValue("embedding_server_need", embedding_server_need);
    settings.setValue("embedding_split", ui->embedding_split_spinbox->value());
    settings.setValue("embedding_resultnumb", embedding_resultnumb);
    settings.setValue("embedding_overlap", ui->embedding_overlap_spinbox->value());
    settings.setValue("embedding_sourcetxt", ui->embedding_txt_lineEdit->text());
    settings.setValue("embedding_describe", ui->embedding_txt_describe_lineEdit->text());
    settings.setValue("shell", shell);                                             //shell路径
    settings.setValue("python", pythonExecutable);                                 //python版本
    settings.setValue("Mcpconfig", ui->mcp_server_config_textEdit->toPlainText()); //mcp配置
    // event->accept();
}
