#include "expend.h"

#include "ui_expend.h"

Expend::Expend(QWidget *parent, QString applicationDirPath_) : QWidget(parent), ui(new Ui::Expend) {
    ui->setupUi(this);
    applicationDirPath = applicationDirPath_;
    QFile file(":/QSS-master/Aqua.qss");  //加载皮肤
    file.open(QFile::ReadOnly);
    QString stylesheet = tr(file.readAll());
    this->setStyleSheet(stylesheet);
    file.close();
    //初始化选项卡
    ui->info_card->setReadOnly(1);
    ui->vocab_card->setReadOnly(1);  //这样才能滚轮放大
    ui->modellog_card->setReadOnly(1);
    ui->tabWidget->setCurrentIndex(2);                                //默认显示模型日志
    ui->sd_prompt_textEdit->setContextMenuPolicy(Qt::NoContextMenu);  //取消右键菜单
    ui->sd_prompt_textEdit->installEventFilter(this);                 //安装事件过滤器
    ui->sd_negative_lineEdit->installEventFilter(this);             //安装事件过滤器
    ui->sd_modify_lineEdit->installEventFilter(this);             //安装事件过滤器
    ui->sd_img2img_lineEdit->installEventFilter(this);             //安装事件过滤器

    ui->vocab_card->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                          //灰色
    ui->modellog_card->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                       //灰色
    ui->whisper_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                         //灰色
    ui->embedding_test_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                  //灰色
    ui->embedding_test_result->setStyleSheet("background-color: rgba(128, 128, 128, 200);");               //灰色
    ui->model_quantize_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                  //灰色
    ui->sd_log->setStyleSheet("background-color: rgba(128, 128, 128, 200);");                              //灰色
    ui->embedding_test_log->setLineWrapMode(QPlainTextEdit::NoWrap);                                       // 禁用自动换行
    ui->sync_plainTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);                                       // 禁用自动换行
    ui->sd_log->setLineWrapMode(QPlainTextEdit::NoWrap);                                                   // 禁用自动换行
    ui->model_quantize_info->setStyleSheet("QTableWidget::item:selected { background-color: #FFA500; }");  // 设置选中行的颜色为橘黄色

    //塞入第三方exe
    server_process = new QProcess(this);                                                                                              // 创建一个QProcess实例用来启动llama-server
    connect(server_process, &QProcess::started, this, &Expend::server_onProcessStarted);                                              //连接开始信号
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::server_onProcessFinished);  //连接结束信号

    quantize_process = new QProcess(this);                                                                                                // 创建一个QProcess实例用来启动llama-quantize
    connect(quantize_process, &QProcess::started, this, &Expend::quantize_onProcessStarted);                                              //连接开始信号
    connect(quantize_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::quantize_onProcessFinished);  //连接结束信号

    whisper_process = new QProcess(this);                                                                                               //实例化
    connect(whisper_process, &QProcess::started, this, &Expend::whisper_onProcessStarted);                                              //连接开始信号
    connect(whisper_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::whisper_onProcessFinished);  //连接结束信号

    sd_process = new QProcess(this);                                                                                          // 创建一个QProcess实例用来启动llama-quantize
    connect(sd_process, &QProcess::started, this, &Expend::sd_onProcessStarted);                                              //连接开始信号
    connect(sd_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::sd_onProcessFinished);  //连接结束信号

    outetts_process = new QProcess(this);   
    connect(outetts_process, &QProcess::started, this, &Expend::outetts_onProcessStarted);                                              //连接开始信号
    connect(outetts_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::outetts_onProcessFinished);  //连接结束信号
    connect(outetts_process, &QProcess::readyReadStandardOutput, this, &Expend::readyRead_outetts_process_StandardOutput);
    connect(outetts_process, &QProcess::readyReadStandardError, this, &Expend::readyRead_outetts_process_StandardError);

    ui->embedding_txt_wait->setContextMenuPolicy(Qt::CustomContextMenu);  //添加右键菜单
    connect(ui->embedding_txt_wait, &QTableWidget::customContextMenuRequested, this, &Expend::show_embedding_txt_wait_menu);

    //知识库相关
    ui->embedding_txt_wait->setColumnCount(1);  //设置一列
    ui->embedding_txt_over->setColumnCount(1);  //设置一列
    connect(server_process, &QProcess::readyReadStandardOutput, this, &Expend::readyRead_server_process_StandardOutput);
    connect(server_process, &QProcess::readyReadStandardError, this, &Expend::readyRead_server_process_StandardError);

    //同步率相关
    ui->sync_tableWidget->setColumnCount(5);  //设置列数
    ui->sync_tableWidget->setRowCount(30);    //创建行数
    // ui->sync_tableWidget->verticalHeader()->setVisible(false);// 隐藏行头部
    ui->sync_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);  // 充满

    // 设置某几列的最大宽度
    QHeaderView *header = ui->sync_tableWidget->horizontalHeader();  // 获取水平表头
    header->setSectionResizeMode(2, QHeaderView::Interactive);       // action_name
    header->setSectionResizeMode(4, QHeaderView::Interactive);       // pass
    header->setMaximumSectionSize(80);

    //添加采样算法
    ui->sd_sampletype->addItems({"euler", "euler_a", "heun", "dpm2", "dpm++2s_a", "dpm++2m", "dpm++2mv2", "lcm"});
    ui->sd_sampletype->setCurrentText("euler");
    //添加输出格式
    ui->whisper_output_format->addItems({"txt", "srt", "csv", "json"});



    // 文生图相关
    // 构建模板 sd1.5-anything-3,sdxl-animagine-3.1,sd3.5-large,flux1-dev,custom1,custom2

    SD_PARAMS sd_sd1_5_anything_3_template{"euler_a","EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure","masterpieces, best quality, beauty, detailed, Pixar, 8k",512,512,20,1,-1,2,7.5};
    SD_PARAMS sd_sdxl_animagine_3_1_template{"euler_a","nsfw, lowres, (bad), text, error, fewer, extra, missing, worst quality, jpeg artifacts, low quality, watermark, unfinished, displeasing, oldest, early, chromatic aberration, signature, extra digits, artistic error, username, scan, [abstract]","masterpiece, best quality",768,768,30,1,-1,2,7.5};
    SD_PARAMS sd_sd3_5_large_template{"euler","","",768,768,20,1,-1,-1,4.5};
    SD_PARAMS sd_flux1_dev_template{"euler","","",768,768,30,1,-1,-1,1.0};
    SD_PARAMS sd_custom1_template{"euler","","",512,512,20,1,-1,-1,7.5};
    SD_PARAMS sd_custom2_template{"euler","","",512,512,20,1,-1,-1,7.5};

    sd_params_templates.insert("sd1.5-anything-3", sd_sd1_5_anything_3_template);
    sd_params_templates.insert("sdxl-animagine-3.1", sd_sdxl_animagine_3_1_template);
    sd_params_templates.insert("sd3.5-large", sd_sd3_5_large_template);
    sd_params_templates.insert("flux1-dev", sd_flux1_dev_template);
    sd_params_templates.insert("custom1", sd_custom1_template);
    sd_params_templates.insert("custom2", sd_custom2_template);

    for (const auto& key : sd_params_templates.keys()) {
        ui->params_template_comboBox->addItem(key); // 添加模板选项
    }

    //记忆矩阵相关
    ui->brain_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);  //让表格自动撑满所在区域
    ui->brain_tableWidget->verticalHeader()->setVisible(false);                             // 隐藏行头部

    //文转声相关
    connect(ui->speech_enable_radioButton, &QRadioButton::clicked, this, &Expend::speech_enable_change);
    connect(ui->speech_source_comboBox, &QComboBox::currentTextChanged, this, &Expend::speech_source_change);
    sys_speech = new QTextToSpeech();// 系统声源
    // 检查是否成功创建
    if (sys_speech->state() == QTextToSpeech::Ready) {
        // 遍历所有可用音色
        foreach (const QVoice &speech, sys_speech->availableVoices()) { avaliable_speech_list << speech.name(); }
        connect(sys_speech, &QTextToSpeech::stateChanged, this, &Expend::speechOver);  //朗读结束后动作
        is_sys_speech_available = true;
    }
    else{is_sys_speech_available = false;}

    avaliable_speech_list << SPPECH_OUTETTS;// 模型声源
    this->set_sys_speech(avaliable_speech_list);  // 设置可用声源

    // 创建播放器对象
    speech_player = new QMediaPlayer;
    // 连接 mediaStatusChanged 信号到槽函数
    QObject::connect(speech_player, &QMediaPlayer::mediaStatusChanged, this, &Expend::speech_player_over);
    outettsDir = applicationDirPath + "/EVA_TEMP/outetts/";// outetts生成的音频存放目录
    connect(&speechTimer, SIGNAL(timeout()), this, SLOT(speech_process()));
    connect(&speechPlayTimer, SIGNAL(timeout()), this, SLOT(speech_play_process()));
    speechTimer.start(500);  //每半秒检查一次是否需要朗读
    speechPlayTimer.start(500);  //每半秒检查一次是否有音频需要朗读

    //如果存在配置文件则读取它，并且应用，目前主要是文生图/声转文/文转声
    readConfig();

    qDebug() << "expend init over";
}

Expend::~Expend() {
    delete ui;
    server_process->kill();  //有点问题
    sd_process->kill();
    whisper_process->kill();
    quantize_process->kill();
}

//创建临时文件夹EVA_TEMP
bool Expend::createTempDirectory(const QString &path) {
    QDir dir;
    // 检查路径是否存在
    if (dir.exists(path)) {
        return false;
    } else {
        // 尝试创建目录
        if (dir.mkpath(path)) {
            return true;
        } else {
            return false;
        }
    }
}

//传递llama.cpp的log，显示模型日志
void Expend::recv_llama_log(QString log) {
    QTextCursor cursor = ui->modellog_card->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    cursor.insertText(log);
    ui->modellog_card->setTextCursor(cursor);

}

// 根据language.json和language_flag中找到对应的文字
QString Expend::jtr(QString customstr) { return wordsObj[customstr].toArray()[language_flag].toString(); }

//-------------------------------------------------------------------------
//----------------------------------界面相关--------------------------------
//-------------------------------------------------------------------------

//初始化增殖窗口
void Expend::init_expend() {
    this->setWindowTitle(jtr("expend window"));                    //标题
    ui->tabWidget->setTabText(0, jtr("introduction"));             //软件介绍
    ui->tabWidget->setTabText(1, jtr("model brain"));              //模型记忆
    ui->tabWidget->setTabText(2, jtr("model log"));                //模型日志
    ui->tabWidget->setTabText(3, jtr("model") + jtr("quantize"));  //模型量化
    ui->tabWidget->setTabText(4, jtr("knowledge"));                //知识库
    ui->tabWidget->setTabText(5, jtr("text2image"));               //文生图
    ui->tabWidget->setTabText(6, jtr("speech2text"));               //声转文
    ui->tabWidget->setTabText(7, jtr("text2speech"));               //文转声
    ui->tabWidget->setTabText(8, jtr("sync rate"));                //同步率

    //模型记忆
    ui->vocab_groupBox->setTitle(jtr("vocab_groupBox_title"));
    ui->brain_groupBox->setTitle(jtr("brain_groupBox_title"));

    //软件介绍
    showReadme();

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
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")});    //设置列名
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{jtr("embedless text segment")});  //设置列名
    ui->embedding_endpoint_label->setText(jtr("embd api"));
    ui->embedding_dim_label->setText(jtr("embd dim"));
    ui->embedding_txt_api_lineedit->setPlaceholderText(jtr("embedding_txt_api_lineedit_placeholder"));
    ui->embedding_split_label->setText(jtr("split length"));
    ui->embedding_overlap_label->setText(jtr("overlap length"));
    ui->embedding_source_doc_label->setText(jtr("source doc"));
    ui->embedding_txt_lineEdit->setPlaceholderText(jtr("embedding_txt_lineEdit_placeholder"));
    ui->embedding_describe_label->setText(jtr("knowledge base description"));
    ui->embedding_txt_describe_lineEdit->setPlaceholderText(jtr("embedding_txt_describe_lineEdit_placeholder"));
    ui->embedding_txt_embedding->setText(jtr("embedding txt"));
    ui->embedding_test_groupBox->setTitle(jtr("test"));
    ui->embedding_test_textEdit->setPlaceholderText(jtr("embedding_test_textEdit_placeholder"));
    ui->embedding_test_pushButton->setText(jtr("retrieval"));
    ui->embedding_result_groupBox->setTitle(jtr("retrieval result"));
    ui->embedding_log_groupBox->setTitle(jtr("log"));
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
    ui->sd_draw_pushButton->setText(jtr("text to image"));

    ui->sd_img2img_pushButton->setText(jtr("image to image"));
    ui->sd_img2img_lineEdit->setPlaceholderText(jtr("sd_img2img_lineEdit_placeholder"));
    
    ui->sd_log->setPlainText(jtr("sd_log_plainText"));

    //声转文
    ui->whisper_modelpath_label->setText(jtr("whisper path"));
    ui->whisper_load_modelpath_linedit->setPlaceholderText(jtr("whisper_load_modelpath_linedit_placeholder"));
    ui->speech_load_groupBox_4->setTitle("whisper " + jtr("log"));
    ui->whisper_wav2text_label->setText(jtr("wav2text"));
    ui->whisper_wavpath_pushButton->setText(jtr("wav path"));
    ui->whisper_format_label->setText(jtr("format"));
    ui->whisper_execute_pushbutton->setText(jtr("convert"));

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

    //同步率
    init_syncrate();

}

//用户切换选项卡时响应
// 0软件介绍,1模型记忆,2模型日志
void Expend::on_tabWidget_tabBarClicked(int index) {
    if (index == 1)  //点模型记忆
    {
        if (is_first_show_this_vocab) {
            ui->vocab_card->setPlainText(vocab);
            is_first_show_this_vocab = false;
        }
        reflash_brain_matrix();
    } else if (index == 0 && is_first_show_info)  //第一次点软件介绍
    {
        is_first_show_info = false;

        //展示readme内容
        showReadme();

        //强制延迟见顶
        QTimer::singleShot(0, this, [this]() {
            ui->info_card->verticalScrollBar()->setValue(0);
            ui->info_card->horizontalScrollBar()->setValue(0);
        });
    } else if (index == 3 && is_first_show_modelproliferation)  //第一次点模型增殖
    {
        is_first_show_modelproliferation = false;
        show_quantize_types();                    //展示量化方法
    } else if (index == 8 && is_first_show_sync)  //第一次点模型增殖
    {
        is_first_show_sync = false;
        ui->sync_tableWidget->setHorizontalHeaderLabels(QStringList{jtr("task"), jtr("response"), "action_name", "action_input", jtr("pass")});  //设置列名
    }
}

// 接收模型词表
void Expend::recv_vocab(QString vocab_) {
    vocab = vocab_;
    is_first_show_this_vocab = true;
    init_brain_matrix();
}

//通知显示增殖窗口
void Expend::recv_expend_show(int index_) {
    if (index_ == 999) {
        this->close();
        return;
    } 
    else if (index_ == 8 && is_first_show_sync)  //第一次点同步率
    {
        is_first_show_sync = false;
        ui->sync_tableWidget->setHorizontalHeaderLabels(QStringList{jtr("task"), jtr("response"), "action_name", "action_input", jtr("pass")});  //设置列名
    }

    if (is_first_show_expend)  //第一次显示的话
    {
        is_first_show_expend = false;
        this->init_expend();

        if (vocab == "") { vocab = jtr("lode model first"); }

        if (ui->modellog_card->toPlainText() == "") { ui->modellog_card->setPlainText(jtr("lode model first")); }
    }

    //打开指定页数窗口
    ui->tabWidget->setCurrentIndex(index_);
    this->setWindowState(Qt::WindowActive);  // 激活窗口并恢复正常状态
    this->show();
    this->activateWindow();  // 激活增殖窗口
}

QString Expend::customOpenfile(QString dirpath, QString describe, QString format) {
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);

    return filepath;
}

//传递使用的语言
void Expend::recv_language(int language_flag_) {
    language_flag = language_flag_;
    init_expend();
}

//读取配置文件并应用
void Expend::readConfig() {
    QFile configfile(applicationDirPath + "/EVA_TEMP/eva_config.ini");
    if (configfile.exists()) {
        // 创建 QSettings 对象，指定配置文件的名称和格式
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        // 读取配置文件中的值
        QString sd_params_template = settings.value("sd_params_template", "custom1").toString();       // 参数模板
        QString sd_modelpath = settings.value("sd_modelpath", "").toString();              // sd模型路径
        QString vae_modelpath = settings.value("vae_modelpath", "").toString();            // vae模型路径
        QString clip_l_modelpath = settings.value("clip_l_modelpath", "").toString();            // clip_l模型路径
        QString clip_g_modelpath = settings.value("clip_g_modelpath", "").toString();            // clip_g模型路径
        QString t5_modelpath = settings.value("t5_modelpath", "").toString();            // t5模型路径
        QString lora_modelpath = settings.value("lora_modelpath", "").toString();            // lora模型路径

        QString negative = settings.value("negative", "").toString();  //反提示
        QString modify = settings.value("modify", "").toString();  //修饰词
        int image_width = settings.value("image_width", 512).toInt();                      //图像宽度
        int image_height = settings.value("image_height", 512).toInt();                    //图像高度
        QString sample_type = settings.value("sample_type", "euler").toString();         //采样方式
        int sample_steps = settings.value("sample_steps", 20).toInt();                     //采样步数
        float cfg = settings.value("cfg", 7.5).toFloat();                                  //相关系数
        int seed = settings.value("seed", -1).toInt();                                     //随机数种子
        int image_nums = settings.value("image_nums", 1).toInt();                          //生成图像数目
        int clip_skip = settings.value("clip_skip", -1).toInt();                            //跳层数
        QString sd_prompt = settings.value("sd_prompt", "").toString();                    // sd提示词

        QString whisper_modelpath = settings.value("whisper_modelpath", "").toString();  // whisper模型路径

        speech_params.enable_speech = settings.value("speech_enable", "").toBool();    //是否启用语音朗读
        speech_params.speech_name = settings.value("speech_name", "").toString();  //朗读者
        QString outetts_modelpath = settings.value("outetts_modelpath", "").toString();  // outetts模型路径
        QString wavtokenizer_modelpath = settings.value("wavtokenizer_modelpath", "").toString();  // wavtokenizer模型路径
        ui->speech_outetts_modelpath_lineEdit->setText(outetts_modelpath);
        ui->speech_wavtokenizer_modelpath_lineEdit->setText(wavtokenizer_modelpath);

        // 应用值
        QFile sd_modelpath_file(sd_modelpath);
        if (sd_modelpath_file.exists()) {ui->sd_modelpath_lineEdit->setText(sd_modelpath);}

        QFile vae_modelpath_file(vae_modelpath);
        if (vae_modelpath_file.exists()) {ui->sd_vaepath_lineEdit->setText(vae_modelpath);}

        QFile clip_l_modelpath_file(clip_l_modelpath);
        if (clip_l_modelpath_file.exists()) {ui->sd_clip_l_path_lineEdit->setText(clip_l_modelpath);}

        QFile clip_g_modelpath_file(clip_g_modelpath);
        if (clip_g_modelpath_file.exists()) {ui->sd_clip_g_path_lineEdit->setText(clip_g_modelpath);}

        QFile t5_modelpath_file(t5_modelpath);
        if (t5_modelpath_file.exists()) {ui->sd_t5path_lineEdit->setText(t5_modelpath);}

        QFile lora_modelpath_file(lora_modelpath);
        if (lora_modelpath_file.exists()) {ui->sd_lorapath_lineEdit->setText(lora_modelpath);}

        ui->params_template_comboBox->setCurrentText(sd_params_template);
        ui->sd_negative_lineEdit->setText(negative);
        ui->sd_modify_lineEdit->setText(modify);
        ui->sd_imagewidth->setValue(image_width);
        ui->sd_imageheight->setValue(image_height);
        ui->sd_sampletype->setCurrentText(sample_type);
        ui->sd_samplesteps->setValue(sample_steps);
        ui->sd_cfgscale->setValue(cfg);
        ui->sd_seed->setValue(seed);
        ui->sd_batch_count->setValue(image_nums);
        ui->sd_clipskip->setValue(clip_skip);
        ui->sd_prompt_textEdit->setText(sd_prompt);

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

        QFile whisper_load_modelpath_file(whisper_modelpath);
        if (whisper_load_modelpath_file.exists()) {
            ui->whisper_load_modelpath_linedit->setText(whisper_modelpath);
            whisper_params.model = whisper_modelpath.toStdString();
        }

        //知识库，在main.cpp里有启动的部分
        ui->embedding_txt_api_lineedit->setText(settings.value("embedding_endpoint", "").toString());       //如果模型不存在则直接使用端点
        ui->embedding_txt_describe_lineEdit->setText(settings.value("embedding_describe", "").toString());  //知识库描述
        ui->embedding_split_spinbox->setValue(settings.value("embedding_split", 300).toInt());
        ui->embedding_overlap_spinbox->setValue(settings.value("embedding_overlap", 50).toInt());
        ui->embedding_dim_spinBox->setValue(settings.value("embedding_dim", 1024).toInt());

        QString embedding_sourcetxt = settings.value("embedding_sourcetxt", "").toString();  //源文档路径
        QFile embedding_sourcetxt_file(embedding_sourcetxt);
        if (embedding_sourcetxt_file.exists()) {
            txtpath = embedding_sourcetxt;
            ui->embedding_txt_lineEdit->setText(txtpath);
            preprocessTXT();  //预处理文件内容
        }
    }
}

//展示readme内容
void Expend::showReadme() {
    QString readme_content;
    QFile file;
    QString imagefile;
    if (language_flag == 0) {
        file.setFileName(":/README.md");
        imagefile = ":/logo/ui_demo.png";
    } else if (language_flag == 1) {
        file.setFileName(":/README_en.md");
        imagefile = ":/logo/ui_demo.png";
    }

    // 打开文件
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);  // 创建 QTextStream 对象
        in.setCodec("UTF-8");
        readme_content = in.readAll();  // 读取文件内容
    }
    file.close();
    
    //正则表达式,删除<img src=\"https://github.com/ylsdamxssjxxdd/eva/assets/直到>的所有文本
    QRegularExpression re("<img src=\"https://github.com/ylsdamxssjxxdd/eva/assets/[^>]+>");
    readme_content.remove(re);
    QRegExp regExp("<summary>|</summary>");// 使用QRegExp创建正则表达式来匹配<summary>和</summary>标记
    readme_content.replace(regExp, "");// 使用replace函数去掉所有<summary>和</summary>标记
    readme_content.remove("<details>");
    readme_content.remove("</details>");

    // 添加编译信息
    readme_content.push_front(jtr("EVA_PRODUCT_TIME") + " " +QString(EVA_PRODUCT_TIME) + "; " + DEFAULT_SPLITER);
    readme_content.push_front(jtr("QT_VERSION_") + " " +QString(QT_VERSION_) + "; ");
    readme_content.push_front(jtr("COMPILE_VERSION") + " " +QString(COMPILE_VERSION) + "; ");
    readme_content.push_front(jtr("EVA_VERSION") + " " +QString(EVA_VERSION) + "; ");
    
    ui->info_card->setMarkdown(readme_content);

    // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来

    QImage image(imagefile);

    int originalWidth = image.width() / devicePixelRatioF() / 1.5;
    int originalHeight = image.height() / devicePixelRatioF() / 1.5;

    QTextCursor cursor(ui->info_card->textCursor());
    cursor.movePosition(QTextCursor::Start);

    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);    // 设置图片的宽度
    imageFormat.setHeight(originalHeight);  // 设置图片的高度
    imageFormat.setName(imagefile);         // 图片资源路径
    cursor.insertImage(imageFormat);

    cursor.insertText("\n\n");
    
}

//事件过滤器,鼠标跟踪效果不好要在各种控件单独实现
bool Expend::eventFilter(QObject *obj, QEvent *event) {
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->sd_prompt_textEdit && event->type() == QEvent::ContextMenu) {
        // 自动填充提示词
        ui->sd_prompt_textEdit->setText("full body, Ayanami Rei, beautiful face, Blue hair, 1 girl");
        return true;
    } else if (obj == ui->sd_negative_lineEdit && event->type() == QEvent::ContextMenu) {
        //还原负面词
        ui->sd_negative_lineEdit->setText(sd_params_templates[ui->params_template_comboBox->currentText()].negative_prompt);
        return true;
    } else if (obj == ui->sd_modify_lineEdit && event->type() == QEvent::ContextMenu) {
        //还原修饰词
        ui->sd_modify_lineEdit->setText(sd_params_templates[ui->params_template_comboBox->currentText()].modify_prompt);
    } else if (obj == ui->sd_img2img_lineEdit && event->type() == QEvent::ContextMenu) {
        //选择图像
        currentpath = customOpenfile(currentpath, "choose an imgage", "(*.png *.jpg *.bmp)");
        if(currentpath != "")
        {
            ui->sd_img2img_pushButton->setEnabled(1);
            ui->sd_img2img_lineEdit->setText(currentpath);
        }
        else{ui->sd_img2img_pushButton->setEnabled(0);}

        return true;
    } 
    
    return QObject::eventFilter(obj, event);
}

//关闭事件
void Expend::closeEvent(QCloseEvent *event) {
    //--------------保存当前用户配置---------------
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

    settings.setValue("negative", ui->sd_negative_lineEdit->text());
    settings.setValue("modify", ui->sd_modify_lineEdit->text());
    settings.setValue("image_width", ui->sd_imagewidth->value());
    settings.setValue("image_height", ui->sd_imageheight->value());
    settings.setValue("sample_type", ui->sd_sampletype->currentText());
    settings.setValue("sample_steps", ui->sd_samplesteps->value());
    settings.setValue("cfg", ui->sd_cfgscale->value());
    settings.setValue("seed", ui->sd_seed->value());
    settings.setValue("image_nums", ui->sd_batch_count->value());
    settings.setValue("clip_skip", ui->sd_clipskip->value());

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
    settings.setValue("embedding_endpoint", ui->embedding_txt_api_lineedit->text());  //如果模型不存在则直接使用端点
    settings.setValue("embedding_dim", ui->embedding_dim_spinBox->text());
    if (embedding_need) {
        if (ui->embedding_txt_api_lineedit->text() == "")  //如果用户删除了嵌入端点的内容则不自动嵌入
        {
            embedding_need = false;
        }
    }
    settings.setValue("embedding_need", embedding_need);
    settings.setValue("embedding_split", ui->embedding_split_spinbox->value());
    settings.setValue("embedding_overlap", ui->embedding_overlap_spinbox->value());
    settings.setValue("embedding_sourcetxt", ui->embedding_txt_lineEdit->text());
    settings.setValue("embedding_describe", ui->embedding_txt_describe_lineEdit->text());
    // event->accept();
}

//-------------------------------------------------------------------------
//----------------------------------声转文相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择whisper路径时响应
void Expend::on_whisper_load_modelpath_button_clicked() {
    currentpath = customOpenfile(currentpath, "choose whisper model", "(*.bin *.gguf)");
    whisper_params.model = currentpath.toStdString();
    ui->whisper_load_modelpath_linedit->setText(currentpath);
    emit expend2ui_whisper_modelpath(currentpath);
    ui->whisper_log->setPlainText(jtr("once selected, you can record by pressing f2"));
}

//开始语音转文字
void Expend::recv_speechdecode(QString wavpath, QString out_format) {
    whisper_time.restart();

#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/whisper-cli") + SFX_NAME;
#else
    QString localPath = QString("./whisper-cli") + SFX_NAME;
#endif

    //将wav文件重采样为16khz音频文件
#ifdef _WIN32
    QTextCodec* code = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    std::string wav_path_c = code->fromUnicode(wavpath).data();
#elif __linux__
    std::string wav_path_c = wavpath.toStdString();
#endif
    resampleWav(wav_path_c, wav_path_c);

    // 设置要运行的exe文件的路径
    QString program = localPath;
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;

    arguments << "-m" << QString::fromStdString(whisper_params.model);             //模型路径
    arguments << "-f" << wavpath;                                                  // wav文件路径
    arguments << "--language" << QString::fromStdString(whisper_params.language);  //识别语种
    arguments << "--threads" << QString::number(max_thread * 0.5);
    if (out_format == "txt") { arguments << "--output-txt";}  //结果输出为一个txt
    else if (out_format == "srt") {arguments << "--output-srt";} 
    else if (out_format == "csv") {arguments << "--output-csv";} 
    else if (out_format == "json") {arguments << "--output-json";}

    // 开始运行程序
    //连接信号和槽,获取程序的输出
    connect(whisper_process, &QProcess::readyReadStandardOutput, [=]() {
        QString output = whisper_process->readAllStandardOutput();
        ui->whisper_log->appendPlainText(output);
    });
    connect(whisper_process, &QProcess::readyReadStandardError, [=]() {
        QString output = whisper_process->readAllStandardError();
        ui->whisper_log->appendPlainText(output);
    });

    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    whisper_process->start(program, arguments);
}

void Expend::whisper_onProcessStarted() {
    if (!is_handle_whisper) {
        emit expend2ui_state("expend:" + jtr("calling whisper to decode recording"), USUAL_SIGNAL);
    }
}

void Expend::whisper_onProcessFinished() {
    if (!is_handle_whisper) {
        QString content;
        // 文件路径
        QString filePath = applicationDirPath + "/EVA_TEMP/" + QString("EVA_") + ".wav.txt";
        // 创建 QFile 对象
        QFile file(filePath);
        // 打开文件
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);  // 创建 QTextStream 对象
            in.setCodec("UTF-8");
            content = in.readAll();  // 读取文件内容
        }
        file.close();
        emit expend2ui_state("expend:" + jtr("decode over") + " " + QString::number(whisper_time.nsecsElapsed() / 1000000000.0, 'f', 2) + "s ->" + content, SUCCESS_SIGNAL);
        emit expend2ui_speechdecode_over(content);
    } else {
        ui->whisper_log->appendPlainText(jtr("the result has been saved in the source wav file directory") + " " + QString::number(whisper_time.nsecsElapsed() / 1000000000.0, 'f', 2) + "s");
    }
    is_handle_whisper = false;
}

//用户点击选择wav路径时响应
void Expend::on_whisper_wavpath_pushButton_clicked() {
    currentpath = customOpenfile(currentpath, "choose a .wav file", "(*.wav)");
    wavpath = currentpath;
    if (wavpath == "") {
        return;
    }
    ui->whisper_wavpath_lineedit->setText(wavpath);
}
//用户点击执行转换时响应
void Expend::on_whisper_execute_pushbutton_clicked() {
    //执行whisper
    is_handle_whisper = true;
    whisper_process->kill();
    recv_speechdecode(ui->whisper_wavpath_lineedit->text(), ui->whisper_output_format->currentText());
}

//-------------------------------------------------------------------------
//----------------------------------知识库相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择嵌入模型路径时响应
void Expend::on_embedding_txt_modelpath_button_clicked() {
    server_process->kill();                                                                       // 终止server
    Embedding_DB.clear();                                                                         // 清空向量数据库
    ui->embedding_txt_over->clear();                                                              //清空已嵌入文本段表格内容
    ui->embedding_txt_over->setRowCount(0);                                                       //设置已嵌入文本段表格为0行
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")});  //设置列名
    // 清空表格
    currentpath = customOpenfile(currentpath, jtr("select embedding model"), "(*.bin *.gguf)");
    embedding_params.modelpath = currentpath;
    if (embedding_params.modelpath == "") {
        return;
    }

    // 100 ms后尝试启动服务, 因为要等待server_process->kill()
    QTimer::singleShot(100, this, &Expend::embedding_server_start);
}

// 尝试启动server
void Expend::embedding_server_start() {
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/llama-server") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#else
    QString localPath = QString("./llama-server") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#endif

    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << embedding_params.modelpath;
    arguments << "--host"
              << "0.0.0.0";                                         //暴露本机ip
    arguments << "--port" << DEFAULT_EMBEDDING_PORT;                //服务端口
    arguments << "--threads" << QString::number(max_thread * 0.5);  //使用线程
    arguments << "-cb";                                             //允许连续批处理
    arguments << "--embedding";                                     //允许词嵌入
    // arguments << "--log-disable";                                   //不要日志
    // 开始运行程序
    server_process->start(program, arguments);

    //连接信号和槽,获取程序的输出
    ipAddress = getFirstNonLoopbackIPv4Address();
}

// 获取server_process日志输出
void Expend::readyRead_server_process_StandardOutput() {
    QString server_output = server_process->readAllStandardOutput();
}

// 获取server_process日志输出
void Expend::readyRead_server_process_StandardError() {
    QString server_output = server_process->readAllStandardError();
    // qDebug()<<server_output;

    //启动成功的标志
    QString log_output;
    if (server_output.contains(SERVER_START)) {
        keep_embedding_server = true;
        embedding_server_api = "http://" + ipAddress + ":" + DEFAULT_EMBEDDING_PORT + "/v1/embeddings";
        ui->embedding_txt_api_lineedit->setText(embedding_server_api);  //启动成功后将端点地址写进去
        keep_embedding_server = false;
        log_output += jtr("embedding") + jtr("service startup completed") + "\n";
        log_output += jtr("embd api") + ": " + embedding_server_api;
        log_output += "\n" + jtr("embd dim") + ": " + QString::number(embedding_server_dim);

    }
    if (log_output != "") {
        ui->embedding_test_log->appendPlainText(log_output);
    }

    if (server_output.contains(LLM_EMBD)) {
        embedding_server_dim = server_output.split(LLM_EMBD).at(1).split("\r\n").at(0).toInt();
        ui->embedding_dim_spinBox->setValue(embedding_server_dim);
        if (embedding_need_auto)  //用来自动构建知识库
        {
            embedding_need_auto = false;
            embedding_processing();                                                                                            //执行嵌入
            emit expend2tool_embedding_serverapi(ui->embedding_txt_api_lineedit->text(), ui->embedding_dim_spinBox->value());  //传递嵌入服务端点
        }
    }  //截获n_embd嵌入维度
}

//进程开始响应
void Expend::server_onProcessStarted() {}

//进程结束响应
void Expend::server_onProcessFinished() { ui->embedding_test_log->appendPlainText(jtr("embedding server abort")); }

//获取本机第一个ip地址
QString Expend::getFirstNonLoopbackIPv4Address() {
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (int i = 0; i < list.count(); i++) {
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol) {
            return list[i].toString();
        }
    }
    return QString();
}
//用户点击上传路径时响应
void Expend::on_embedding_txt_upload_clicked() {
    currentpath = customOpenfile(currentpath, jtr("choose a txt to embed"), "(*.txt)");
    txtpath = currentpath;
    ui->embedding_txt_lineEdit->setText(txtpath);

    preprocessTXT();  //预处理文件内容
}

//预处理文件内容
void Expend::preprocessTXT() {
    //读取
    QString content;
    QFile file(txtpath);
    // 打开文件
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);  // 创建 QTextStream 对象
        in.setCodec("UTF-8");
        content = in.readAll();  // 读取文件内容
    }
    file.close();

    //-------------------分词&分段-----------------
    QStringList paragraphs;
    int splitLength = ui->embedding_split_spinbox->value();
    int overlap = ui->embedding_overlap_spinbox->value();
    int splitNums = 0;
    int start = 0;
    int actualNewLength = splitLength - overlap;  // 实际上每次新增加的字符长度
    if (splitLength <= overlap) {
        paragraphs << content;
        splitNums++;
    }

    while (start < content.length()) {
        if (!content.isEmpty()) {
            start -= overlap;
        }  // 如果不是第一段，则保留前一段的部分字符

        int endLength = qMin(splitLength, content.length() - start);
        paragraphs << content.mid(start, endLength);
        splitNums++;
        start += actualNewLength;
    }

    //显示在待嵌入表格中
    ui->embedding_txt_wait->clear();
    ui->embedding_txt_wait->setRowCount(splitNums);  //创建splitNums很多行

    for (int i = 0; i < paragraphs.size(); ++i) {
        QTableWidgetItem *newItem = new QTableWidgetItem(paragraphs.at(i));
        ui->embedding_txt_wait->setItem(i, 0, newItem);
    }
    ui->embedding_txt_wait->setColumnWidth(0, qMax(ui->embedding_txt_wait->width(), 400));  // 列宽保持控件宽度

    ui->embedding_txt_wait->resizeRowsToContents();                                                 // 自动调整行高
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{jtr("embedless text segment")});  //设置列名
}

//右击表格显示菜单
void Expend::show_embedding_txt_wait_menu(const QPoint &pos) {
    // 创建菜单并添加动作
    QMenu contextMenu(tr("Context menu"), this);

    QAction actionAdd(jtr("add"), this);
    connect(&actionAdd, &QAction::triggered, this, &Expend::embedding_txt_wait_onAdd);
    contextMenu.addAction(&actionAdd);

    QAction actionDelete(jtr("delete"), this);
    connect(&actionDelete, &QAction::triggered, this, &Expend::embedding_txt_wait_onDelete);
    contextMenu.addAction(&actionDelete);

    // 显示菜单
    contextMenu.exec(ui->embedding_txt_wait->viewport()->mapToGlobal(pos));
}

//添加表格
void Expend::embedding_txt_wait_onAdd() {
    // 获取选中的行
    int row = ui->embedding_txt_wait->currentRow() + 1;
    ui->embedding_txt_wait->insertRow(row);  // 在选中的行的下一行添加一行

    // 根据需要设置新行的内容
    QTableWidgetItem *newItem = new QTableWidgetItem(jtr("please input the text that needs to be embedded"));
    ui->embedding_txt_wait->setItem(row, 0, newItem);  // 假设我们只设置第一列
}
//删除表格
void Expend::embedding_txt_wait_onDelete() {
    // 获取选中的行号
    QList<QTableWidgetItem *> selectedItems = ui->embedding_txt_wait->selectedItems();
    QSet<int> rows;
    for (auto item : selectedItems) {
        rows.insert(item->row());  // 只添加行号
    }

    // 转换为列表并排序（从大到小）
    QList<int> sortedRows = QList<int>(rows.begin(), rows.end());
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    // 删除行
    for (int row : sortedRows) {
        ui->embedding_txt_wait->removeRow(row);
    }
}

//---------------构建知识库------------------
//用户点击嵌入时响应
void Expend::on_embedding_txt_embedding_clicked() { embedding_processing(); }

//用户点击检索时响应
void Expend::on_embedding_test_pushButton_clicked() {
    //锁定界面
    ui->embedding_txt_upload->setEnabled(0);            //上传按钮
    ui->embedding_txt_embedding->setEnabled(0);         //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(0);       //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(0);  //选择模型按钮

    QEventLoop loop;  // 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QNetworkRequest request(QUrl(ui->embedding_txt_api_lineedit->text()));
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key = "Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());
    //构造请求的数据体
    QJsonObject json;
    json.insert("model", "gpt-3.5-turbo");
    json.insert("encoding_format", "float");
    json.insert("input", ui->embedding_test_textEdit->toPlainText());
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    // POST 请求
    QNetworkReply *reply = manager.post(request, data);

    // 处理响应
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        QString jsonString = reply->readAll();
        QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8());  // 使用QJsonDocument解析JSON数据
        QJsonObject rootObject = document.object();
        // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
        QJsonArray dataArray = rootObject["data"].toArray();
        QString vector_str = "[";

        for (int i = 0; i < dataArray.size(); ++i) {
            QJsonObject dataObj = dataArray[i].toObject();

            // 检查"data"对象中是否存在"embedding"
            if (dataObj.contains("embedding")) {
                QJsonArray embeddingArray = dataObj["embedding"].toArray();
                user_embedding_vector.value.resize(ui->embedding_dim_spinBox->value());  // 分配空间
                // 处理"embedding"数组
                for (int j = 0; j < embeddingArray.size(); ++j) {
                    user_embedding_vector.value[j] = embeddingArray[j].toDouble();
                    vector_str += QString::number(user_embedding_vector.value[j], 'f', 4) + ", ";
                }
            }
        }
        vector_str += "]";
        ui->embedding_test_log->appendPlainText(jtr("The query text segment has been embedded") + "! " + jtr("dimension") + ": " + QString::number(user_embedding_vector.value.size()) + " " + jtr("word vector") + ": " + vector_str);
    });
    // 完成
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        if (reply->error() == QNetworkReply::NoError) {
            // 请求完成，所有数据都已正常接收
            //计算余弦相似度
            // A向量点积B向量除以(A模乘B模)
            std::vector<std::pair<int, double>> score;
            score = similar_indices(user_embedding_vector.value, Embedding_DB);
            ui->embedding_test_result->appendPlainText(jtr("The three text segments with the highest similarity") + ":");
            //将分数前三的结果显示出来
            for (int i = 0; i < 3 && i < score.size(); ++i) {
                // qDebug()<<score[i].first<<score[i].second;
                ui->embedding_test_result->appendPlainText(QString::number(score[i].first + 1) + " " + jtr("Number text segment similarity") + ": " + QString::number(score[i].second));
            }

        } else {
            // 请求出错
            ui->embedding_test_log->appendPlainText(jtr("Request error, please make sure to start the embedded service"));
        }

        reply->abort();  //终止
        reply->deleteLater();
    });

    // 回复完成时退出事件循环
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // 进入事件循环
    loop.exec();
    //解锁界面
    ui->embedding_txt_upload->setEnabled(1);            //上传按钮
    ui->embedding_txt_embedding->setEnabled(1);         //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(1);       //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(1);  //选择模型按钮
}

// 计算两个向量的余弦相似度
double Expend::cosine_similarity(const std::vector<double> &a, const std::vector<double> &b) {
    // 确保两个向量维度相同
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vectors must be of the same length");
    }

    double dot_product = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    return dot_product / (sqrt(norm_a) * sqrt(norm_b));
}

// 计算user_vector与Embedding_DB中每个元素的相似度，并返回得分最高的3个索引
std::vector<std::pair<int, double>> Expend::similar_indices(const std::vector<double> &user_vector, const QVector<Embedding_vector> &embedding_DB) {
    std::vector<std::pair<int, double>> scores;  // 存储每个索引和其相应的相似度得分

    // 计算相似度得分
    for (const auto &emb : embedding_DB) {
        double sim = cosine_similarity(user_vector, emb.value);
        scores.emplace_back(emb.index, sim);
    }

    // 根据相似度得分排序（降序）
    std::sort(scores.begin(), scores.end(), [](const std::pair<int, double> &a, const std::pair<int, double> &b) { return a.second > b.second; });

    return scores;
}

//知识库构建过程
void Expend::embedding_processing() {
    //锁定界面
    ui->embedding_txt_upload->setEnabled(0);            //上传按钮
    ui->embedding_txt_embedding->setEnabled(0);         //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(0);       //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(0);  //选择模型按钮

    ui->embedding_txt_over->clear();                                                              //清空已嵌入文本段表格内容
    ui->embedding_txt_over->setRowCount(0);                                                       //设置已嵌入文本段表格为0行
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")});  //设置列名
    show_chunk_index = 0;                                                                         //待显示的嵌入文本段的序号

    //----------------------相同的内容不再嵌入, 先保留再新增-----------------------
    QVector<Embedding_vector> new_Embedding_DB;
    QVector<int> save_list;
    //构造一个如果文本段一致则保留的数据库
    for (int i = 0; i < Embedding_DB.size(); ++i) {
        bool remove_flag = true;
        int current_index_table = 0;  //记录未嵌入之前在表格中的序号，保证将来在表格的位置
        //如果原来的数据库中有当前待嵌入文本一致的内容则保留
        for (int j = 0; j < ui->embedding_txt_wait->rowCount(); ++j) {
            QTableWidgetItem *item = ui->embedding_txt_wait->item(j, 0);
            if (item) {
                if (Embedding_DB.at(i).chunk == item->text()) {
                    bool already_save = false;

                    // 若已经保留则不保留
                    for (int k = 0; k < new_Embedding_DB.size(); ++k) {
                        if (new_Embedding_DB.at(k).chunk == item->text()) {
                            already_save = true;
                        }
                    }

                    if (!already_save) {
                        remove_flag = false;
                        current_index_table = j;
                    }
                }
            }
        }
        if (!remove_flag) {
            new_Embedding_DB.append(Embedding_DB.at(i));
            new_Embedding_DB.last().index = current_index_table;
            save_list.append(current_index_table);
            // qDebug()<<"保留的"<<i<<Embedding_DB.at(i).chunk;
        }
    }
    Embedding_DB.clear();
    Embedding_DB = new_Embedding_DB;

    //读取待嵌入表格中的内容
    for (int i = 0; i < ui->embedding_txt_wait->rowCount(); ++i) {
        QTableWidgetItem *item = ui->embedding_txt_wait->item(i, 0);
        if (item) {
            if (!save_list.contains(i)) {
                Embedding_DB.append({i, item->text()});
                // qDebug()<<"新增的"<<i<<item->text();
            }
        }
    }

    //先排好序
    std::sort(Embedding_DB.begin(), Embedding_DB.end(), [](const Embedding_vector &a, const Embedding_vector &b) { return a.index < b.index; });

    //进行嵌入工作,发送ready_embedding_chunks给llama-server
    //测试v1/embedding端点
    QElapsedTimer time;
    time.start();
    QEventLoop loop;  // 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QNetworkRequest request(QUrl(ui->embedding_txt_api_lineedit->text()));
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key = "Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());
    //-------------------循环发送请求直到文本段处理完-------------------
    for (int o = 0; o < Embedding_DB.size(); o++) {
        //已经嵌入的就不处理了
        if (save_list.contains(Embedding_DB.at(o).index)) {
            ui->embedding_txt_over->insertRow(ui->embedding_txt_over->rowCount());  // 在表格末尾添加新行
            QTableWidgetItem *newItem = new QTableWidgetItem(Embedding_DB.at(o).chunk);
            newItem->setFlags(newItem->flags() & ~Qt::ItemIsEditable);  //单元格不可编辑
            newItem->setBackground(QColor(255, 165, 0, 60));            // 设置单元格背景颜色,橘黄色
            ui->embedding_txt_over->setItem(o, 0, newItem);
            ui->embedding_txt_over->setColumnWidth(0, qMax(ui->embedding_txt_over->width(), 400));  // 列宽保持控件宽度
            ui->embedding_txt_over->resizeRowsToContents();                                         // 自动调整行高
            ui->embedding_txt_over->scrollToItem(newItem, QAbstractItemView::PositionAtTop);        // 滚动到新添加的行
            show_chunk_index++;
            continue;
        }

        //构造请求的数据体
        QJsonObject json;
        json.insert("model", "gpt-3.5-turbo");
        json.insert("encoding_format", "float");
        json.insert("input", Embedding_DB.at(o).chunk);  //待嵌入文本段
        QJsonDocument doc(json);
        QByteArray data = doc.toJson();

        // POST 请求
        QNetworkReply *reply = manager.post(request, data);

        // 处理响应
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
            QString jsonString = reply->readAll();

            QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8());  // 使用QJsonDocument解析JSON数据
            QJsonObject rootObject = document.object();

            // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
            QJsonArray dataArray = rootObject["data"].toArray();
            QString vector_str = "[";
            for (int i = 0; i < dataArray.size(); ++i) {
                QJsonObject dataObj = dataArray[i].toObject();

                // 检查"data"对象中是否存在"embedding"
                if (dataObj.contains("embedding")) {
                    QJsonArray embeddingArray = dataObj["embedding"].toArray();
                    Embedding_DB[o].value.resize(ui->embedding_dim_spinBox->value());  // 分配空间
                    // 处理"embedding"数组
                    for (int j = 0; j < embeddingArray.size(); ++j) {
                        Embedding_DB[o].value[j] = embeddingArray[j].toDouble();
                        vector_str += QString::number(Embedding_DB[o].value[j], 'f', 4) + ", ";
                    }
                }
            }
            vector_str += "]";
            QString message = QString::number(Embedding_DB.at(o).index + 1) + " " + jtr("Number text segment embedding over") + "! " + jtr("dimension") + ": " + QString::number(Embedding_DB.at(o).value.size()) + " " + jtr("word vector") + ": " + vector_str;
            ui->embedding_test_log->appendPlainText(message);
            ui->embedding_test_log->verticalScrollBar()->setValue(ui->embedding_test_log->verticalScrollBar()->maximum());      //滚动条滚动到最下面
            ui->embedding_test_log->horizontalScrollBar()->setValue(ui->embedding_test_log->horizontalScrollBar()->minimum());  // 水平滚动条滚动到最左边
            emit expend2ui_state("expend:" + message, USUAL_SIGNAL);
        });
        // 完成
        QObject::connect(reply, &QNetworkReply::finished, [&]() {
            if (reply->error() == QNetworkReply::NoError) {
                // 请求完成，所有数据都已正常接收
                // 显示在已嵌入表格中
                ui->embedding_txt_over->insertRow(ui->embedding_txt_over->rowCount());  // 在表格末尾添加新行
                QTableWidgetItem *newItem = new QTableWidgetItem(Embedding_DB.at(show_chunk_index).chunk);
                newItem->setFlags(newItem->flags() & ~Qt::ItemIsEditable);  //单元格不可编辑
                newItem->setBackground(LCL_ORANGE);                         // 设置单元格背景颜色,橘黄色
                ui->embedding_txt_over->setItem(show_chunk_index, 0, newItem);
                ui->embedding_txt_over->setColumnWidth(0, qMax(ui->embedding_txt_over->width(), 400));  // 列宽保持控件宽度
                ui->embedding_txt_over->resizeRowsToContents();                                         // 自动调整行高
                ui->embedding_txt_over->scrollToItem(newItem, QAbstractItemView::PositionAtTop);        // 滚动到新添加的行
                show_chunk_index++;
                embedding_need = true;
            } else {
                // 请求出错
                ui->embedding_test_log->appendPlainText(jtr("Request error, please make sure to start the embedded service"));
                embedding_need = false;
            }

            reply->abort();  //终止
            reply->deleteLater();
        });

        // 回复完成时退出事件循环
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        // 进入事件循环
        loop.exec();
    }

    //解锁界面
    ui->embedding_txt_upload->setEnabled(1);            //上传按钮
    ui->embedding_txt_embedding->setEnabled(1);         //嵌入按钮
    ui->embedding_test_pushButton->setEnabled(1);       //检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(1);  //选择模型按钮

    ui->embedding_test_log->appendPlainText(jtr("embedding over") + " " + jtr("use time") + QString::number(time.nsecsElapsed() / 1000000000.0, 'f', 2) + "s");
    emit expend2tool_embeddingdb(Embedding_DB);  //发送已嵌入文本段数据给tool
}

//嵌入服务端点改变响应
void Expend::on_embedding_txt_api_lineedit_textChanged() {
    if (!keep_embedding_server) {
        server_process->kill();                                                                       // 终止server
        Embedding_DB.clear();                                                                         // 清空向量数据库
        ui->embedding_txt_over->clear();                                                              //清空已嵌入文本段表格内容
        ui->embedding_txt_over->setRowCount(0);                                                       //设置已嵌入文本段表格为0行
        ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{jtr("embeded text segment")});  //设置列名
    }

    emit expend2tool_embedding_serverapi(ui->embedding_txt_api_lineedit->text(), ui->embedding_dim_spinBox->value());  //传递嵌入服务端点
}

//嵌入服务端点改变响应
void Expend::on_embedding_dim_spinBox_textChanged() {
    emit expend2tool_embedding_serverapi(ui->embedding_txt_api_lineedit->text(), ui->embedding_dim_spinBox->value());  //传递嵌入服务端点
}

//知识库描述改变响应
void Expend::on_embedding_txt_describe_lineEdit_textChanged() {
    emit expend2ui_embeddingdb_describe(ui->embedding_txt_describe_lineEdit->text());  //传递知识库的描述
}

//-------------------------------------------------------------------------
//----------------------------------模型量化相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择待量化模型路径时响应
void Expend::on_model_quantize_row_modelpath_pushButton_clicked() {
    currentpath = customOpenfile(currentpath, jtr("model_quantize_row_modelpath_lineedit_placeholder"), "(*.gguf)");
    ui->model_quantize_row_modelpath_lineedit->setText(currentpath);
}

//用户点击选择重要性矩阵路径时响应
void Expend::on_model_quantize_important_datapath_pushButton_clicked() {
    currentpath = customOpenfile(currentpath, jtr("model_quantize_important_datapath_lineedit_placeholder"), "(*.dat)");
    ui->model_quantize_important_datapath_lineedit->setText(currentpath);
}
//待量化模型路径改变响应
void Expend::on_model_quantize_row_modelpath_lineedit_textChanged() {
    output_modelpath_change();  //根据待量化模型路径和量化方法填入量化后模型路径
}
//展示量化方法
void Expend::show_quantize_types() {
    //量化方法说明
    quantize_types.clear();
    ui->model_quantize_type->clear();
    // 以f16-7B模型的大小为准，8bit是一个字节，f16就是两字节，或者16bpw(每个权重占16位)
    // f16-7B模型的大小 = 70*10^8*2 / 1024 /1024 /1024 = 13GB
    quantize_types = {
        {"F32", "-100.0%", "0", "⭐"},
        {"BF16", "0.0%", "-0.0050", "⭐"},
        {"F16", "0.0%", "-0.0020", "⭐"},
        {"Q8_0", "48.5%", "+0.0004", "⭐"},
        {"Q6_K", "60.4%", "+0.0008", "⭐"},
        {"Q5_1", "63.8%", "+0.0349", "⭐"},
        {"Q5_K_M", "65.8%", "+0.0122", "⭐⭐"},
        {"Q5_K_S", "66.7%", "+0.0400", "⭐"},
        {"Q5_0", "66.7%", "+0.0683", "⭐"},
        {"Q4_1", "70%", "+0.1585", "⭐"},
        {"Q4_K_M", "70.8%", "+0.0532", "⭐"},
        {"Q4_K_S", "72.4%", "+0.0992", "⭐"},
        {"Q4_0", "72.6%", "+0.2166", "⭐⭐"},
        {"Q3_K_L", "74.2%", "+0.1764", "⭐"},
        {"Q3_K_M", "76.4%", "+0.2496", "⭐⭐"},
        {"Q3_K_S", "78.8%", "+0.5551", "⭐"},
        {"Q2_K", "79.8%", "+0.6717", "⭐"},
        {"Q2_K_S", "83.4%", "+9.0634", ""},
        {"IQ4_NL", "71.9%", jtr("related to the imatrix"), "⭐"},
        {"IQ4_XS", "73.4%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_M", "77.1%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_S", "78.5%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_XS", "79.4%", jtr("related to the imatrix"), "⭐"},
        {"IQ3_XXS", "80.9%", jtr("related to the imatrix"), "⭐"},
        {"IQ2_M", "83.1%", jtr("related to the imatrix"), "⭐⭐"},
        {"IQ2_S", "84.3%", jtr("related to the imatrix"), "⭐"},
        {"IQ2_XS", "85.6%", jtr("related to the imatrix"), "⭐"},
        {"IQ2_XXS", "87.1%", jtr("related to the imatrix"), "⭐"},
        {"IQ1_M", "89.1%", jtr("related to the imatrix"), "⭐"},
        {"IQ1_S", "90.3%", jtr("related to the imatrix"), "⭐"},
    };

    //添加量化方法选项
    for (int i = 0; i < quantize_types.size(); ++i) {
        ui->model_quantize_type->addItem(quantize_types.at(i).typename_);
    }

    //添加量化方法说明
    ui->model_quantize_info->setRowCount(quantize_types.size());  //创建行
    ui->model_quantize_info->setColumnCount(5);
    ui->model_quantize_info->setHorizontalHeaderLabels(QStringList{jtr("quantize type"), jtr("compression") + "（f16->）", jtr("perplexity"), jtr("recommend"), jtr("estimated size") + "（f16->）"});  //设置列名
    for (int i = 0; i < quantize_types.size(); ++i) {
        QTableWidgetItem *newItem1 = new QTableWidgetItem(quantize_types.at(i).typename_);
        ui->model_quantize_info->setItem(i, 0, newItem1);

        QTableWidgetItem *newItem2 = new QTableWidgetItem(quantize_types.at(i).bit);
        ui->model_quantize_info->setItem(i, 1, newItem2);

        QTableWidgetItem *newItem3 = new QTableWidgetItem(quantize_types.at(i).perplexity);
        ui->model_quantize_info->setItem(i, 2, newItem3);

        QTableWidgetItem *newItem4 = new QTableWidgetItem(quantize_types.at(i).recommand);
        ui->model_quantize_info->setItem(i, 3, newItem4);
    }
    QHeaderView *headerView = ui->model_quantize_info->horizontalHeader();  //水平表头对象,用来控制列宽
    headerView->setSectionResizeMode(QHeaderView::Stretch);                 // 设置所有列为等宽且撑满整个表格宽度

    // 默认的量化级别
    ui->model_quantize_type->setCurrentText("Q5_K_M");
}

// 根据待量化模型路径和量化方法填入量化后模型路径
void Expend::output_modelpath_change() {
    //提取模型名
    QString modelpath = ui->model_quantize_row_modelpath_lineedit->text();
    if (modelpath.contains(".gguf") && QFile::exists(modelpath)) {
        //重构名称，将尾部的量化词条更换为当前量化方法
        QString output_modelpath = modelpath.replace(modelpath.split(".gguf")[0].split("-").last(), ui->model_quantize_type->currentText());
        // qDebug()<<output_modelpath<<modelpath.split(".gguf")[0].split("-").last()<<modelpath.split(".gguf")[0];
        ui->model_quantize_output_modelpath_lineedit->setText(output_modelpath);

        //顺便改变量化方法说明中的预估量化后大小
        QFileInfo fileInfo1(ui->model_quantize_row_modelpath_lineedit->text());  //获取文件大小
        float in_modelsize = fileInfo1.size() / 1024.0 / 1024.0 / 1024.0;

#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        float totalPhysMem = memInfo.ullTotalPhys;  // 总内存
#elif __linux__
        // 读取/proc/meminfo文件获取内存信息
        std::ifstream meminfoFile("/proc/meminfo");
        std::string line;
        float totalPhysMem = 0;

        if (meminfoFile.is_open()) {
            while (getline(meminfoFile, line)) {
                if (line.find("MemTotal:") == 0) {
                    std::istringstream iss(line);
                    std::string key;
                    float value;
                    std::string unit;
                    iss >> key >> value >> unit;
                    if (unit == "kB") {
                        totalPhysMem = value * 1024;  // 将kB转换为字节
                    }
                    break;
                }
            }
            meminfoFile.close();
        }
#endif
        //  估计量化后大小
        for (int i = 0; i < quantize_types.size(); ++i) {
            QTableWidgetItem *item = ui->model_quantize_info->item(i, 1);
            float estimate_modelsize = in_modelsize * abs(1 - item->text().split("%")[0].toFloat() / 100.0);
            QString estimate_modelsize_str = QString::number(estimate_modelsize, 'f', 1) + " GB";
            QTableWidgetItem *newItem1 = new QTableWidgetItem(estimate_modelsize_str);
            ui->model_quantize_info->setItem(i, 4, newItem1);

            //加星,如果量化后的大小比本机内存小20%以上就加一颗星
            QString star;
            if (estimate_modelsize < totalPhysMem / 1024.0 / 1024.0 / 1024.0 * 0.8) {
                star = quantize_types.at(i).recommand + "⭐";
            } else {
                star = quantize_types.at(i).recommand;
            }
            QTableWidgetItem *newItem4 = new QTableWidgetItem(star);
            ui->model_quantize_info->setItem(i, 3, newItem4);
        }
    }
}
//量化方法改变响应
void Expend::on_model_quantize_type_currentIndexChanged(int index) {
    //根据待量化模型路径和量化方法填入量化后模型路径
    output_modelpath_change();

    // 表格中对应的量化信息高亮
    ui->model_quantize_info->clearSelection();                                     // 清除当前所有选择
    ui->model_quantize_info->setSelectionBehavior(QAbstractItemView::SelectRows);  // 设置选择行为为选择行
    ui->model_quantize_info->selectRow(index);                                     // 选择指定的行
}
//用户点击执行量化按钮时响应
void Expend::on_model_quantize_execute_clicked() {
    //锁定界面
    ui->model_quantize_frame1->setEnabled(0);
    ui->model_quantize_frame2->setEnabled(0);
    ui->model_quantize_frame3->setEnabled(0);
    ui->model_quantize_frame4->setEnabled(0);

    //执行量化
    quantize(ui->model_quantize_row_modelpath_lineedit->text(), ui->model_quantize_output_modelpath_lineedit->text(), ui->model_quantize_important_datapath_lineedit->text(), ui->model_quantize_type->currentText());
}

//执行量化
void Expend::quantize(QString in_modelpath, QString out_modelpath, QString important_datapath, QString quantize_type) {
    //结束llama-quantize
    quantize_process->kill();
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/llama-quantize") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#else
    QString localPath = QString("./llama-quantize") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#endif
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    if (important_datapath != "") {
        arguments << "--imatrix" << important_datapath;
    }                                                //重要性矩阵路径
    arguments << in_modelpath;                       //待量化模型路径
    arguments << out_modelpath;                      //输出路径
    arguments << quantize_type;                      //量化方法
    arguments << QString::number(max_thread * 0.5);  //使用线程数

    //连接信号和槽,获取程序的输出
    connect(quantize_process, &QProcess::readyReadStandardOutput, [=]() {
        QString output = quantize_process->readAllStandardOutput();
        ui->model_quantize_log->appendPlainText(output);
    });
    connect(quantize_process, &QProcess::readyReadStandardError, [=]() {
        QString output = quantize_process->readAllStandardError();
        ui->model_quantize_log->appendPlainText(output);
        // if(output.contains("llama_model_quantize_internal: model size  =  "))
        // {
        //     in_modelsize = output.split("llama_model_quantize_internal: model size  =  ")[1];
        //     qDebug()<<in_modelsize;
        // }
    });
    quantize_process->start(program, arguments);
}
//开始信号
void Expend::quantize_onProcessStarted() {}
//结束信号
void Expend::quantize_onProcessFinished() {
    //解锁界面
    ui->model_quantize_frame1->setEnabled(1);
    ui->model_quantize_frame2->setEnabled(1);
    ui->model_quantize_frame3->setEnabled(1);
    ui->model_quantize_frame4->setEnabled(1);

    ui->model_quantize_log->appendPlainText(jtr("quantize completed! model save") + ":" + ui->model_quantize_output_modelpath_lineedit->text());
    QFileInfo fileInfo1(ui->model_quantize_row_modelpath_lineedit->text());  //获取文件大小
    float modelsize1_GB = fileInfo1.size() / 1024.0 / 1024.0 / 1024.0;
    QFileInfo fileInfo2(ui->model_quantize_output_modelpath_lineedit->text());  //获取文件大小
    float modelsize2_GB = fileInfo2.size() / 1024.0 / 1024.0 / 1024.0;
    ui->model_quantize_log->appendPlainText(QString::number(modelsize1_GB) + " GB" + " -> " + QString::number(modelsize2_GB) + " GB " + jtr("compression") + " :" + QString::number((1 - modelsize2_GB / modelsize1_GB) * 100) + "%");
}

//-------------------------------------------------------------------------
//----------------------------------文生图相关--------------------------------
//-------------------------------------------------------------------------

// 遍历目录
QStringList Expend::listFiles(const QString &path) {
    QStringList file_paths;
    QDir dir(path);

    // Set the filter to include files and no special files/links
    dir.setFilter(QDir::Files | QDir::NoSymLinks);

    // Get the list of files in the directory
    QFileInfoList fileList = dir.entryInfoList();

    // Iterate through the list and print the absolute file paths
    foreach (const QFileInfo &fileInfo, fileList) {
        file_paths << fileInfo.absoluteFilePath();
    }
    
    return file_paths;
}

//用户点击选择sd模型路径时响应
void Expend::on_sd_modelpath_pushButton_clicked() {
    currentpath = customOpenfile(currentpath, "choose diffusion model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if(currentpath == ""){return ;}

    QString modelpath = currentpath;
    ui->sd_modelpath_lineEdit->setText(currentpath);
    
    // 自动寻找其它模型
    if (QFile::exists(modelpath)) {

        //先清空其它路径
        ui->sd_lorapath_lineEdit->setText("");
        ui->sd_vaepath_lineEdit->setText("");
        ui->sd_clip_l_path_lineEdit->setText("");
        ui->sd_clip_g_path_lineEdit->setText("");
        ui->sd_t5path_lineEdit->setText("");

        // 遍历当前目录
        QFileInfo modelfileInfo(modelpath);
        QString model_directoryPath = modelfileInfo.absolutePath();// 提取目录路径
        QStringList file_list = listFiles(model_directoryPath);

        for(int i = 0; i < file_list.size(); ++i)
        {
            QString file_path_name = file_list.at(i);
            if(file_path_name.contains("vae"))
            {
                ui->sd_vaepath_lineEdit->setText(file_path_name);
            }
            else if(file_path_name.contains("clip_l"))
            {
                ui->sd_clip_l_path_lineEdit->setText(file_path_name);
            }
            else if(file_path_name.contains("clip_g"))
            {
                ui->sd_clip_g_path_lineEdit->setText(file_path_name);
            }
            else if(file_path_name.contains("t5"))
            {
                ui->sd_t5path_lineEdit->setText(file_path_name);
            }
        }
    }

    // 自动设置参数模板
    if (modelpath.contains("sd1.5-anything-3")){ui->params_template_comboBox->setCurrentText("sd1.5-anything-3");}
    else if(modelpath.contains("sdxl-animagine-3.1")){ui->params_template_comboBox->setCurrentText("sdxl-animagine-3.1");}
    else if(modelpath.contains("sd3.5-large")){ui->params_template_comboBox->setCurrentText("sd3.5-large");}
    else if(modelpath.contains("flux1-dev")){ui->params_template_comboBox->setCurrentText("flux1-dev");}

}

//用户点击选择vae模型路径时响应
void Expend::on_sd_vaepath_pushButton_clicked() {
    currentpath = customOpenfile(currentpath, "choose vae model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "") {
        ui->sd_vaepath_lineEdit->setText(currentpath);
    }
}

//用户点击选择clip模型路径时响应
void Expend::on_sd_clip_l_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_l model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "") {
        ui->sd_clip_l_path_lineEdit->setText(currentpath);
    }
}

//用户点击选择clip模型路径时响应
void Expend::on_sd_clip_g_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_g model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "") {
        ui->sd_clip_g_path_lineEdit->setText(currentpath);
    }
}

//用户点击选择t5模型路径时响应       
void Expend::on_sd_t5path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose t5 model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "") {
        ui->sd_t5path_lineEdit->setText(currentpath);
    }
}

//用户点击选择lora模型路径时响应
void Expend::on_sd_lorapath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose lora model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "") {
        ui->sd_lorapath_lineEdit->setText(currentpath);
    }
}

//用户点击开始绘制时响应
void Expend::on_sd_draw_pushButton_clicked() {
    //处理stop的情况
    if (ui->sd_img2img_pushButton->text() == "stop" || ui->sd_draw_pushButton->text() == "stop") {
        ui->sd_log->appendPlainText("stop");
        sd_process->kill();  //强制结束sd
        ui->sd_draw_pushButton->setText(jtr("text to image"));
        ui->sd_img2img_pushButton->setText(jtr("image to image"));
        img2img = false;
        return;
    }

    ui->sd_img2img_pushButton->setText("stop");
    ui->sd_draw_pushButton->setText("stop");

    if (is_handle_sd && ui->sd_prompt_textEdit->toPlainText() == "") {
        ui->sd_log->appendPlainText(jtr("Please enter prompt words to tell the model what you want the image to look like"));
        return;
    } else if (is_handle_sd && ui->sd_modelpath_lineEdit->text() == "") {
        ui->sd_log->appendPlainText(jtr("Please specify the SD model path first"));
        return;
    } else if (!is_handle_sd) {
        emit expend2ui_state(QString("expend:sd") + SFX_NAME + " " + jtr("drawing"), USUAL_SIGNAL);
    }

    QTime currentTime = QTime::currentTime();                // 获取当前时间
    QString timeString = currentTime.toString("-hh-mm-ss");  // 格式化时间为时-分-秒
    sd_outputpath = applicationDirPath + "/EVA_TEMP/sd_output" + timeString + ".png";

    //结束sd
    sd_process->kill();

#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/sd") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#else
    QString localPath = QString("./sd") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#endif
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;

    if (img2img) {
        arguments << "-M"
                  << "img2img";  //运行模式 图生图
        arguments << "-i" << ui->sd_img2img_lineEdit->text();// 传入图像路径
        img2img = false;
    } else {
        arguments << "-M"
                  << "txt2img";  //运行模式 文生图
    }

    //模型路径 sd系列模型用-m flux模型用--diffusion-model
    if(ui->sd_modelpath_lineEdit->text().contains("flux")){arguments << "--diffusion-model" << ui->sd_modelpath_lineEdit->text();}
    else {arguments << "-m" << ui->sd_modelpath_lineEdit->text();}

    if(QFile::exists(ui->sd_vaepath_lineEdit->text())){arguments << "--vae" << ui->sd_vaepath_lineEdit->text();}// vae路径
    if(QFile::exists(ui->sd_clip_l_path_lineEdit->text())){arguments << "--clip_l" << ui->sd_clip_l_path_lineEdit->text();}// clip_l路径
    if(QFile::exists(ui->sd_clip_g_path_lineEdit->text())){arguments << "--clip_g" << ui->sd_clip_g_path_lineEdit->text();}// clip_g路径
    if(QFile::exists(ui->sd_t5path_lineEdit->text())){arguments << "--t5xxl" << ui->sd_t5path_lineEdit->text();}// vae路径
    QString lora_prompt = "<lora:{model}:1>";// 应用lora的提示，将会添加到提示词的最后
    if(QFile::exists(ui->sd_lorapath_lineEdit->text()))
    {
        QFileInfo lorafileInfo(ui->sd_lorapath_lineEdit->text());
        QString lora_directoryPath = lorafileInfo.absolutePath();// 提取lora目录路径
        QString lora_name = lorafileInfo.fileName().replace(".safetensors","");
        if(lora_directoryPath != "")
        {
            arguments << "--lora-model-dir" << lora_directoryPath;
            lora_prompt.replace("{model}",lora_name);
        }
    }

    arguments << "-W" << QString::number(ui->sd_imagewidth->value()); //图像宽
    arguments << "-H" << QString::number(ui->sd_imageheight->value()); //图像长
    arguments << "--sampling-method" << ui->sd_sampletype->currentText(); //采样方法
    arguments << "--clip-skip" << QString::number(ui->sd_clipskip->value()); //跳层
    arguments << "--cfg-scale" << QString::number(ui->sd_cfgscale->value());//相关系数
    arguments << "--steps" << QString::number(ui->sd_samplesteps->value()); //采样步数
    arguments << "-s" << QString::number(ui->sd_seed->value()); //随机种子
    arguments << "-b" << QString::number(ui->sd_batch_count->value());//出图张数
    arguments << "-n" << ui->sd_negative_lineEdit->text();;//反向提示词

    //提示词
    if(arguments.contains("--lora-model-dir"))
    {
        // 应用lora的情况
        arguments << "-p" << ui->sd_modify_lineEdit->text() + ", " + ui->sd_prompt_textEdit->toPlainText() + lora_prompt;
    }
    else{arguments << "-p" << ui->sd_modify_lineEdit->text() + ", " + ui->sd_prompt_textEdit->toPlainText();}
     
    arguments << "-t" << QString::number(std::thread::hardware_concurrency() * 0.5);            //线程数
    arguments << "-o" << sd_outputpath;  //输出路径
    arguments << "--strength" << DEFAULT_SD_NOISE;  //噪声系数
    arguments << "-v"; // 打印细节

    //连接信号和槽,获取程序的输出
    connect(sd_process, &QProcess::readyReadStandardOutput, [=]() {
        sd_process_output = sd_process->readAllStandardOutput();
        QTextCursor cursor(ui->sd_log->textCursor());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(sd_process_output);
        ui->sd_log->verticalScrollBar()->setValue(ui->sd_log->verticalScrollBar()->maximum());  //滚动条滚动到最下面
        if (sd_process_output.contains("CUDA error")) {
            sd_process->kill();
        }
    });
    connect(sd_process, &QProcess::readyReadStandardError, [=]() {
        sd_process_output = sd_process->readAllStandardError();
        QTextCursor cursor(ui->sd_log->textCursor());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(sd_process_output);
        ui->sd_log->verticalScrollBar()->setValue(ui->sd_log->verticalScrollBar()->maximum());  //滚动条滚动到最下面
        if (sd_process_output.contains("CUDA error")) {
            sd_process->kill();
        }
    });

    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    sd_process->start(program, arguments);
}
//进程开始响应
void Expend::sd_onProcessStarted() {}
//进程结束响应
void Expend::sd_onProcessFinished() {

    ui->sd_draw_pushButton->setText(jtr("text to image"));
    ui->sd_img2img_pushButton->setText(jtr("image to image"));

    //绘制结果
    QImage image(sd_outputpath);
    int originalWidth = image.width() / devicePixelRatioF();
    int originalHeight = image.height() / devicePixelRatioF();
    QTextCursor cursor(ui->sd_result->textCursor());
    cursor.movePosition(QTextCursor::End);

    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);     // 设置图片的宽度
    imageFormat.setHeight(originalHeight);   // 设置图片的高度
    imageFormat.setName(sd_outputpath);  // 图片资源路径
    cursor.insertImage(imageFormat);
    ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum());  //滚动条滚动到最下面
    //如果是多幅
    if (ui->sd_batch_count->value() > 1) {
        for (int i = 1; i < ui->sd_batch_count->value(); ++i) {
            QTextImageFormat imageFormats;
            imageFormats.setWidth(originalWidth);                                                              // 设置图片的宽度
            imageFormats.setHeight(originalHeight);                                                            // 设置图片的高度
            imageFormats.setName(sd_outputpath.split(".png")[0] + "_" + QString::number(i + 1) + ".png");  // 图片资源路径
            cursor.insertImage(imageFormats);
            ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum());  //滚动条滚动到最下面
        }
    }

    //处理工具调用情况
    if (!is_handle_sd && originalWidth > 0) {
        is_handle_sd = true;
        emit expend2ui_state("expend:" + jtr("draw over"), USUAL_SIGNAL);
        emit expend2tool_drawover(sd_outputpath, 1);  //绘制完成信号
    } else if (!is_handle_sd) {
        is_handle_sd = true;
        if (sd_process_output.contains("CUDA error")) {
            emit expend2ui_state("expend:" + jtr("draw fail cuda"), WRONG_SIGNAL);
            emit expend2tool_drawover(jtr("draw fail cuda"), 0);  //绘制完成信号
        } else {
            emit expend2ui_state("expend:" + jtr("draw fail prompt"), WRONG_SIGNAL);
            emit expend2tool_drawover(jtr("draw fail prompt"), 0);  //绘制完成信号
        }
    }
}

//参数模板改变响应
void Expend::on_params_template_comboBox_currentIndexChanged(int index)
{
    // 以前是自定义模板，触发这个函数说明现在换了，保存以前的这个模板
    if(is_sd_custom1){sd_save_template("custom1");}
    else if(is_sd_custom2){sd_save_template("custom2");}

    if(ui->params_template_comboBox->currentText().contains("custom1")){is_sd_custom1 = true;is_sd_custom2 = false;}
    else if(ui->params_template_comboBox->currentText().contains("custom2")){is_sd_custom2 = true;is_sd_custom1 = false;}
    else{is_sd_custom1 = false;is_sd_custom2 = false;}

    sd_apply_template(sd_params_templates[ui->params_template_comboBox->currentText()]);
}

// 保存参数到自定义模板
void Expend::sd_save_template(QString template_name)
{
    sd_params_templates[template_name].batch_count = ui->sd_batch_count->value();
    sd_params_templates[template_name].cfg_scale = ui->sd_cfgscale->value();
    sd_params_templates[template_name].clip_skip = ui->sd_clipskip->value();
    sd_params_templates[template_name].height = ui->sd_imageheight->value();
    sd_params_templates[template_name].width = ui->sd_imagewidth->value();
    sd_params_templates[template_name].seed = ui->sd_seed->value();
    sd_params_templates[template_name].steps = ui->sd_samplesteps->value();
    sd_params_templates[template_name].sample_type = ui->sd_sampletype->currentText();
    sd_params_templates[template_name].negative_prompt = ui->sd_negative_lineEdit->text();
    sd_params_templates[template_name].modify_prompt = ui->sd_modify_lineEdit->text();

}

// 应用sd参数模板
void Expend::sd_apply_template(SD_PARAMS sd_params)
{
    ui->sd_imagewidth->setValue(sd_params.width);
    ui->sd_imageheight->setValue(sd_params.height);
    ui->sd_sampletype->setCurrentText(sd_params.sample_type);
    ui->sd_samplesteps->setValue(sd_params.steps);
    ui->sd_cfgscale->setValue(sd_params.cfg_scale);
    ui->sd_batch_count->setValue(sd_params.batch_count);
    ui->sd_imagewidth->setValue(sd_params.width);
    ui->sd_seed->setValue(sd_params.seed);
    ui->sd_clipskip->setValue(sd_params.clip_skip);
    ui->sd_negative_lineEdit->setText(sd_params.negative_prompt);
    ui->sd_modify_lineEdit->setText(sd_params.modify_prompt);
}

//用户点击图生图时响应
void Expend::on_sd_img2img_pushButton_clicked() {
    img2img = true;
    ui->sd_draw_pushButton->click();
}

//接收到tool的开始绘制图像信号
void Expend::recv_draw(QString prompt_) {
    //判断是否空闲
    if (!ui->sd_draw_pushButton->isEnabled()) {
        emit expend2tool_drawover("stablediffusion" + jtr("Running, please try again later"), 0);  //绘制完成信号
        return;
    } else if (ui->sd_modelpath_lineEdit->text() == "") {
        emit expend2tool_drawover(jtr("The command is invalid. Please ask the user to specify the SD model path in the breeding window first"), 0);  //绘制完成信号
        return;
    }
    //先把提示词写进输入框
    ui->sd_prompt_textEdit->setText(prompt_);
    //不是手动
    is_handle_sd = false;
    //触发绘制
    ui->sd_draw_pushButton->click();
}

//-------------------------------------------------------------------------
//----------------------------------文转声相关--------------------------------
//-------------------------------------------------------------------------

//用户点击启用声音选项响应
void Expend::speech_enable_change() {

    if (ui->speech_enable_radioButton->isChecked()) {
        speech_params.enable_speech = true;
    } else {
        speech_params.enable_speech = false;
    }

}

//用户切换声源响应
void Expend::speech_source_change() {
    speech_params.speech_name = ui->speech_source_comboBox->currentText();
    if(speech_params.speech_name==SPPECH_OUTETTS)
    {
        ui->speech_outetts_modelpath_frame->setEnabled(1);
        ui->speech_wavtokenizer_modelpath_frame->setEnabled(1);
    }
    else
    {
        ui->speech_outetts_modelpath_frame->setEnabled(0);
        ui->speech_wavtokenizer_modelpath_frame->setEnabled(0);
    }
}

// 添加可用声源
void Expend::set_sys_speech(QStringList avaliable_speech_list) {
    for (int i = 0; i < avaliable_speech_list.size(); ++i) {
        ui->speech_source_comboBox->addItem(avaliable_speech_list.at(i));  //添加到下拉框
    }
    ui->speech_source_comboBox->setCurrentText(speech_params.speech_name);  
    ui->speech_enable_radioButton->setChecked(speech_params.enable_speech);
}

//开始文字转语音
void Expend::start_tts(QString str)
{
    //如果禁用了朗读则直接退出
    // qDebug()<<speech_params.is_speech<<speech_params.speech_name;
    if (!speech_params.enable_speech) 
    {
        speechOver();
        return;
    }

    if (speech_params.speech_name != "") 
    {
        if(speech_params.speech_name == SPPECH_OUTETTS) // 使用模型声源
        {
            if(ui->speech_outetts_modelpath_lineEdit->text()!=""&&ui->speech_wavtokenizer_modelpath_lineEdit->text()!="")
            {
                outettsProcess(str);
            }
        }
        else
        {
            // 遍历所有可用音色
            foreach (const QVoice& voice, sys_speech->availableVoices()) 
            {
                // qDebug() << "Name:" << speech.name();
                // qDebug() << "Age:" << speech.age();
                // qDebug() << "Gender:" << speech.gender();
                //使用用户选择的音色
                if (voice.name() == speech_params.speech_name) 
                {
                    sys_speech->setVoice(voice);
                    break;
                }
            }
            
            // 设置语速，范围从-1到1
            sys_speech->setRate(0.3);

            // 设置音量，范围从0到1
            sys_speech->setVolume(1.0);

            // 开始文本到语音转换
            sys_speech->say(str);
        }

    }
    
}

void Expend::speechOver()
{
    speechTimer.stop();
    speechTimer.start(500);
    is_speech = false;  //解锁
}

void Expend::speechPlayOver()
{
    speechPlayTimer.stop();
    speechPlayTimer.start(500);
    is_speech_play = false;  //解锁
}

//每半秒检查列表，列表中有文字就读然后删，直到读完
void Expend::speech_process() {
    if (!is_speech) {
        if (wait_speech_txt_list.size() > 0) {
            speechTimer.stop();
            is_speech = true;
            start_tts(wait_speech_txt_list.first());
            wait_speech_txt_list.removeFirst();
        }
    }
}

//每半秒检查待播放列表，列表中有文字就读然后删，直到读完
void Expend::speech_play_process()
{
    if (!is_speech_play) {
        if (wait_speech_play_list.size() > 0) {
            speechPlayTimer.stop();
            is_speech_play = true;
            // 播放第一路径的音频
            speech_player->setMedia(QUrl::fromLocalFile(wait_speech_play_list.first()));
            speech_player->play();
            wait_speech_play_list.removeFirst();
        }
    }
}

void Expend::recv_output(const QString result, bool is_while, QColor color)
{
    if (is_while) 
    {
        //添加待朗读的文字
        temp_speech_txt += result;// 累计输出的文本
        //如果积累到包含 叹号/分号/顿号/逗号/句号/问号/冒号 时分段并等待朗读
        // QRegularExpression re("[！；、，。？：!;,?:]");
        QRegularExpression re("[！；、，。？：!;,?:]|\\.\\s");//新增对小数点后跟空格的捕获，但是如果模型输出带空格的字符将会分割异常，待修复
        QRegularExpressionMatch match = re.match(temp_speech_txt);
        if (match.hasMatch()) 
        {
            wait_speech_txt_list << temp_speech_txt;
            temp_speech_txt = "";
        }
    }
}

// 收到重置信号
void Expend::recv_resettts()
{
    temp_speech_txt = "";//清空待读列表
    wait_speech_txt_list.clear();//清空待读列表
    wait_speech_play_list.clear();//清空待读列表
    if (is_sys_speech_available) {
        sys_speech->stop();  //停止朗读
    }

    outetts_process->kill();//终止继续生成
    speech_player->stop();
    removeDir(outettsDir);//清空产生的音频
}

//使用outetts进行文转声
void Expend::outettsProcess(QString str)
{
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/llama-tts") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#else
    QString localPath = QString("./llama-tts") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#endif

    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << ui->speech_outetts_modelpath_lineEdit->text();
    arguments << "-mv" << ui->speech_wavtokenizer_modelpath_lineEdit->text();
    arguments << "-ngl" << "99";
    arguments << "-p" << str;

    // 开始运行程序
    outetts_process->start(program, arguments);
}

//进程开始响应
void Expend::outetts_onProcessStarted() 
{

}

//进程结束响应
void Expend::outetts_onProcessFinished()
{
    //修改生成的音频名称，添加一个时间后缀
    createTempDirectory(outettsDir);
    // 获取当前日期和时间，精确到分钟
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz"); // 格式化为 20241226_1430
    QString destinationPath = outettsDir + currentDateTime + ".wav";
    // 使用 QFile 移动并重命名文件
    QFile file("output.wav");
    if (file.rename(destinationPath)) {
        // qDebug() << "File moved and renamed successfully.";
        wait_speech_play_list << destinationPath;
    } else {
        qDebug() << "Failed to move and rename file." << file.errorString();
    }

    speechOver();// 利用speechOver再次进入文转声生成处理流程
}

void Expend::readyRead_outetts_process_StandardOutput()
{
    QString outetts_output = outetts_process->readAllStandardOutput();
    ui->speech_log->append(outetts_output);
}

void Expend::readyRead_outetts_process_StandardError()
{
    QString outetts_output = outetts_process->readAllStandardError();
    ui->speech_log->append(outetts_output);
}

//用户点击选择模型路径时响应
void Expend::on_speech_outetts_modelpath_pushButton_clicked() {
    currentpath = customOpenfile(currentpath, "choose outetts model", "(*.bin *.gguf)");
    ui->speech_outetts_modelpath_lineEdit->setText(currentpath);
}

//用户点击选择模型路径时响应
void Expend::on_speech_wavtokenizer_modelpath_pushButton_clicked() {
    currentpath = customOpenfile(currentpath, "choose outetts model", "(*.bin *.gguf)");
    ui->speech_wavtokenizer_modelpath_lineEdit->setText(currentpath);
}

//用户点击转为音频按钮时响应
void Expend::on_speech_manual_pushButton_clicked()
{
    if(ui->speech_outetts_modelpath_lineEdit->text()!=""&&ui->speech_wavtokenizer_modelpath_lineEdit->text()!="")
    {
        outettsProcess(ui->speech_manual_plainTextEdit->toPlainText());
    }
}

//音频播放完响应
void Expend::speech_player_over(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::MediaStatus::EndOfMedia) {
        // 播放停止时执行的操作
        speechPlayOver();
    }
}

//-------------------------------------------------------------------------
//----------------------------------记忆相关--------------------------------
//-------------------------------------------------------------------------

//传递记忆向量和上下文长度
void Expend::recv_brainvector(std::vector<Brain_Cell> Brain_vector_, int nctx_, bool reflash) {
    if (nctx != nctx_) {
        init_brain_matrix();
    }
    nctx = nctx_;
    Brain_vector = Brain_vector_;

    if (reflash && ui->tabWidget->currentIndex() == 1) {
        reflash_brain_matrix();
    }
}

//重置记忆矩阵(新词表过来时/nctx变化时)
void Expend::init_brain_matrix() {
    ui->brain_tableWidget->clear();
    ui->brain_tableWidget->setColumnCount(3);                                                    //设置多少列
    ui->brain_tableWidget->setRowCount(nctx);                                                    //创建很多行
    ui->brain_tableWidget->setHorizontalHeaderLabels(QStringList{"sequence", "token", "word"});  //设置列名
}

//刷新一次记忆矩阵
void Expend::reflash_brain_matrix() {
    ui->brain_tableWidget->clear();
    ui->brain_tableWidget->setHorizontalHeaderLabels(QStringList{"sequence", "token", "word"});  //设置列名
    QTableWidgetItem *lastItem = nullptr;                                                        // 初始化指向最后一个单元格的指针
    for (int i = 0; i < int(Brain_vector.size()); ++i) {
        QTableWidgetItem *newItem1 = new QTableWidgetItem(QString::number(Brain_vector.at(i).id));
        newItem1->setFlags(newItem1->flags() & ~Qt::ItemIsEditable);  //单元格不可编辑
        newItem1->setBackground(LCL_ORANGE);                          // 设置单元格背景颜色,橘黄色
        ui->brain_tableWidget->setItem(i, 0, newItem1);

        QTableWidgetItem *newItem2 = new QTableWidgetItem(QString::number(Brain_vector.at(i).token));
        newItem2->setFlags(newItem2->flags() & ~Qt::ItemIsEditable);  //单元格不可编辑
        newItem2->setBackground(LCL_ORANGE);                          // 设置单元格背景颜色,橘黄色
        ui->brain_tableWidget->setItem(i, 1, newItem2);

        QTableWidgetItem *newItem3 = new QTableWidgetItem(Brain_vector.at(i).word.replace("\n", "\\n"));
        newItem3->setFlags(newItem3->flags() & ~Qt::ItemIsEditable);  //单元格不可编辑
        newItem3->setBackground(LCL_ORANGE);                          // 设置单元格背景颜色,橘黄色
        ui->brain_tableWidget->setItem(i, 2, newItem3);

        lastItem = newItem3;  // 更新最后一个单元格的引用
    }
    if (lastItem != nullptr) {
        // 滚动到最后一个添加的单元格
        ui->brain_tableWidget->scrollToItem(lastItem);
    }
}

//-------------------------------------------------------------------------
//----------------------------------同步率相关--------------------------------
//-------------------------------------------------------------------------

//传递同步率结果
void Expend::recv_syncrate(int index, QString task, QString response, QString action_name, QString action_input, bool pass, float score) {
    // 如果接收到的index是1，则先重置
    if (index == 1) {
        init_syncrate();
        ui->sync_plainTextEdit->clearWaterWave();      // 清空水波
        ui->sync_plainTextEdit->startWaveAnimation();  // 开始水波动画
    }

    ui->sync_plainTextEdit->setPlainText(jtr("syncrate_describe") + jtr("current") + jtr("sync rate") + ": " + QString::number(score) + "%");

    QColor BackgroundColor;
    if (pass) {
        BackgroundColor.setRgba(qRgba(255, 165, 0, 255));  // 设置单元格背景颜色,橘黄色
    } else {
        BackgroundColor.setRgba(qRgba(128, 128, 128, 250));  // 设置单元格背景颜色,灰色
    }

    // // 序号
    // QTableWidgetItem *newItem1 = new QTableWidgetItem(QString::number(index));
    // newItem1->setBackground(BackgroundColor);
    // ui->sync_tableWidget->setItem(index - 1, 0, newItem1);

    // 任务
    QTableWidgetItem *newItem2 = new QTableWidgetItem(task);
    newItem2->setBackground(BackgroundColor);
    ui->sync_tableWidget->setItem(index - 1, 0, newItem2);

    // 回答
    QTableWidgetItem *newItem3 = new QTableWidgetItem(response);
    newItem3->setBackground(BackgroundColor);
    ui->sync_tableWidget->setItem(index - 1, 1, newItem3);

    // action_name
    QTableWidgetItem *newItem4 = new QTableWidgetItem(action_name);
    newItem4->setBackground(BackgroundColor);
    ui->sync_tableWidget->setItem(index - 1, 2, newItem4);

    // action_input
    QTableWidgetItem *newItem5 = new QTableWidgetItem(action_input);
    newItem5->setBackground(BackgroundColor);
    ui->sync_tableWidget->setItem(index - 1, 3, newItem5);

    // pass
    QTableWidgetItem *newItem6 = new QTableWidgetItem("√");
    if (pass) {
        newItem6 = new QTableWidgetItem("√");
    } else {
        newItem6 = new QTableWidgetItem("");
    }
    newItem6->setBackground(BackgroundColor);  // 设置单元格背景颜色,橘黄色
    ui->sync_tableWidget->setItem(index - 1, 4, newItem6);

    ui->sync_tableWidget->scrollToItem(newItem6, QAbstractItemView::PositionAtBottom);  // 滚动到新添加的行
}

// 重置同步率显示
void Expend::init_syncrate() {
    ui->sync_plainTextEdit->clear();
    ui->sync_plainTextEdit->appendPlainText(jtr("syncrate_describe"));

    ui->sync_tableWidget->clearContents();
    //插入任务列表
    for (int i = 1; i < 31; ++i) {
        // QTableWidgetItem *newItem1 = new QTableWidgetItem(QString::number(i));
        // ui->sync_tableWidget->setItem(i - 1, 0, newItem1);

        QTableWidgetItem *newItem2 = new QTableWidgetItem(jtr(QString("sync_Q%1").arg(i)));
        ui->sync_tableWidget->setItem(i - 1, 0, newItem2);
    }
}

// 用于设置whisper模型路径
void Expend::setWhisperModelpath(QString modelpath)
{
    ui->whisper_load_modelpath_linedit->setText(modelpath);
    whisper_params.model = modelpath.toStdString();
}

// 用于设置sd模型路径
void Expend::setSdModelpath(QString modelpath)
{
    ui->sd_modelpath_lineEdit->setText(modelpath);
    ui->params_template_comboBox->setCurrentText("sd1.5-anything-3");// 默认
}

// 递归删除文件夹及其内容的函数
bool Expend::removeDir(const QString &dirName) {
    QDir dir(dirName);

    if (!dir.exists()) {
        return false;
    }

    // 删除目录中的所有文件和子目录
    foreach (QFileInfo item, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        if (item.isDir()) {
            // 如果是子目录，递归删除
            if (!removeDir(item.absoluteFilePath())) {
                return false;
            }
        } else {
            // 如果是文件，删除文件
            if (!QFile::remove(item.absoluteFilePath())) {
                return false;
            }
        }
    }

    // 删除目录自身
    return dir.rmdir(dirName);
}