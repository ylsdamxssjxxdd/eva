#include "expend.h"
#include "ui_expend.h"

Expend::Expend(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Expend)
{
    ui->setupUi(this);

    QFile file(":/ui/QSS-master/Aqua.qss");//加载皮肤
    file.open(QFile::ReadOnly);QString stylesheet = tr(file.readAll());
    this->setStyleSheet(stylesheet);file.close();

    //初始化选项卡
    ui->info_card->setReadOnly(1);
    ui->vocab_card->setReadOnly(1);//这样才能滚轮放大
    ui->modellog_card->setReadOnly(1);
    ui->tabWidget->setCurrentIndex(2);//默认显示模型日志
    ui->sd_prompt_textEdit->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    ui->sd_prompt_textEdit->installEventFilter(this);//安装事件过滤器
    ui->sd_antiprompt_lineEdit->installEventFilter(this);//安装事件过滤器

    ui->vocab_card->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    ui->modellog_card->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    ui->whisper_log->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    ui->embedding_test_log->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    ui->embedding_test_result->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    ui->model_quantize_log->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    ui->sd_log->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    ui->embedding_test_log->setLineWrapMode(QPlainTextEdit::NoWrap);// 禁用自动换行
    ui->sd_log->setLineWrapMode(QPlainTextEdit::NoWrap);// 禁用自动换行

    //塞入第三方exe
    server_process = new QProcess(this);// 创建一个QProcess实例用来启动server.exe
    connect(server_process, &QProcess::started, this, &Expend::server_onProcessStarted);//连接开始信号
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Expend::server_onProcessFinished);//连接结束信号        

    quantize_process = new QProcess(this);// 创建一个QProcess实例用来启动quantize.exe
    connect(quantize_process, &QProcess::started, this, &Expend::quantize_onProcessStarted);//连接开始信号
    connect(quantize_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Expend::quantize_onProcessFinished);//连接结束信号        
    
    whisper_process = new QProcess(this);//实例化
    connect(whisper_process, &QProcess::started, this, &Expend::whisper_onProcessStarted);//连接开始信号
    connect(whisper_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Expend::whisper_onProcessFinished);//连接结束信号

    sd_process = new QProcess(this);// 创建一个QProcess实例用来启动quantize.exe
    connect(sd_process, &QProcess::started, this, &Expend::sd_onProcessStarted);//连接开始信号
    connect(sd_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Expend::sd_onProcessFinished);//连接结束信号        

    
    ui->embedding_txt_wait->setColumnCount(1);//设置一列
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{"待嵌入文本段"});//设置列名

    ui->embedding_txt_over->setColumnCount(1);//设置一列
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{"已嵌入文本段"});//设置列名

    //添加采样算法
    ui->sd_sampletype->addItems({"euler", "euler_a", "heun", "dpm2", "dpm++2s_a", "dpm++2m", "dpm++2mv2", "lcm"});
    ui->sd_sampletype->setCurrentText("euler_a");
    //添加输出格式
    ui->whisper_output_format->addItems({"文本文档txt","视频字幕srt","逗号分隔csv","json"});

    //声转文相关
    get_sys_voice();
    connect(ui->voice_enable_radioButton, &QRadioButton::clicked, this, &Expend::voice_enable_change);
    connect(ui->voice_source_comboBox, &QComboBox::currentTextChanged, this, &Expend::voice_source_change);
    
    //文生图相关
    ui->sd_antiprompt_lineEdit->setText(sd_params.negative_prompt);

    //如果存在配置文件则读取它，并且应用，目前主要是文生图/声转文/文转声
    readConfig();

}

Expend::~Expend()
{
    delete ui;
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
//-------------------------------------------------------------------------
//----------------------------------界面相关--------------------------------
//-------------------------------------------------------------------------

//用户切换选项卡时响应
//0软件介绍,1模型词表,2模型日志
void Expend::on_tabWidget_tabBarClicked(int index)
{
    if(index==1 && is_first_show_vocab)//第一次点模型词表
    {
        ui->vocab_card->setPlainText(vocab);
        is_first_show_vocab = false;
    }
    if(index==0 && is_first_show_info)//第一次点软件介绍
    {
        is_first_show_info = false;

        //展示readme内容
        QString readme_content;
        QFile file(":/README.md");
        // 打开文件
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) 
        {
            QTextStream in(&file);// 创建 QTextStream 对象
            in.setCodec("UTF-8");
            readme_content = in.readAll();// 读取文件内容
        }
        file.close();
        //正则表达式,删除<img src=\"https://github.com/ylsdamxssjxxdd/eva/assets/直到>的所有文本
        QRegularExpression re("<img src=\"https://github.com/ylsdamxssjxxdd/eva/assets/[^>]+>");

        ui->info_card->setMarkdown(readme_content.remove(re));

        // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
        QImage image(":/ui/ui_demo.png");
        
        int originalWidth = image.width()/devicePixelRatioF()/1.5;
        int originalHeight = image.height()/devicePixelRatioF()/1.5;

        QTextCursor cursor(ui->info_card->textCursor());
        cursor.movePosition(QTextCursor::Start);

        QTextImageFormat imageFormat;
        imageFormat.setWidth(originalWidth);  // 设置图片的宽度
        imageFormat.setHeight(originalHeight); // 设置图片的高度
        imageFormat.setName(":/ui/ui_demo.png");  // 图片资源路径
        cursor.insertImage(imageFormat);

        // QImage image2(":/ui/knowledge_demo.png");
        // originalWidth = image2.width()/devicePixelRatioF()/2.5;
        // originalHeight = image2.height()/devicePixelRatioF()/2.5;
        // imageFormat.setWidth(originalWidth);  // 设置图片的宽度
        // imageFormat.setHeight(originalHeight); // 设置图片的高度
        // imageFormat.setName(":/ui/knowledge_demo.png");  // 图片资源路径
        // cursor.insertImage(imageFormat);

        cursor.insertText("\n\n");

        //强制延迟见顶
        QTimer::singleShot(0, this, [this]() {ui->info_card->verticalScrollBar()->setValue(0);ui->info_card->horizontalScrollBar()->setValue(0);});
    }

    if(index==3 && is_first_show_modelproliferation)//第一次点模型增殖
    {
        is_first_show_modelproliferation = false;
        //量化方法说明
        quantize_types = {
            { "Q8_0", "48.5%", "+0.0004", "⭐"},
            { "Q6_K", "60.4%", "+0.0008", "⭐"},
            { "Q5_1", "63.8%", "+0.0349", "⭐"},
            { "Q5_K_M", "65.8%", "+0.0122", "⭐⭐⭐"},
            { "Q5_K_S", "66.7%", "+0.0400", "⭐"},
            { "Q5_0", "66.7%", "+0.0683", "⭐"},
            { "Q4_1", "70%", "+0.1585", "⭐"},
            { "Q4_K_M", "70.8%", "+0.0532", "⭐"},
            { "Q4_K_S", "72.4%", "+0.0992", "⭐⭐"},
            { "Q4_0", "72.6%", "+0.2166", "⭐"},
            { "Q3_K_L", "74.2%", "+0.1764", "⭐"},
            { "Q3_K_M", "76.4%", "+0.2496", "⭐"},
            { "Q3_K_S", "78.8%", "+0.5551", "⭐"},
            { "Q2_K", "79.8%", "+0.6717", "⭐"},
            { "Q2_K_S", "83.4%", "+9.0634", ""},
            { "IQ4_NL", "71.9%", "和重要性矩阵有关", "⭐"},
            { "IQ4_XS", "73.4%", "和重要性矩阵有关", "⭐"},
            { "IQ3_M", "77.1%", "和重要性矩阵有关", "⭐"},
            { "IQ3_S", "78.5%", "和重要性矩阵有关", "⭐"},
            { "IQ3_XS", "79.4%", "和重要性矩阵有关", "⭐"},
            { "IQ3_XXS", "80.9%", "和重要性矩阵有关", "⭐"},
            { "IQ2_M", "83.1%", "和重要性矩阵有关", "⭐"},
            { "IQ2_S", "84.3%", "和重要性矩阵有关", "⭐"}, 
            { "IQ2_XS", "85.6%", "和重要性矩阵有关", "⭐"},
            { "IQ2_XXS", "87.1%", "和重要性矩阵有关", "⭐"},
            { "IQ1_S", "90.3%", "和重要性矩阵有关", "⭐⭐"},
        };

        //添加量化方法选项
        for(int i = 0;i < quantize_types.size(); ++i)
        {
            ui->model_quantize_type->addItem(quantize_types.at(i).typename_);
        }
        ui->model_quantize_type->setCurrentText("Q5_K_M");
        //添加量化方法说明
        ui->model_quantize_info->setRowCount(quantize_types.size());//创建行
        ui->model_quantize_info->setColumnCount(5);
        ui->model_quantize_info->setHorizontalHeaderLabels(QStringList{"量化方法","压缩率（f16->）","困惑度","推荐","预估大小（f16->）"});//设置列名
        for(int i=0;i<quantize_types.size(); ++i)
        {
            QTableWidgetItem *newItem1 = new QTableWidgetItem(quantize_types.at(i).typename_);
            ui->model_quantize_info->setItem(i, 0, newItem1);

            QTableWidgetItem *newItem2 = new QTableWidgetItem(quantize_types.at(i).bit);
            ui->model_quantize_info->setItem(i, 1, newItem2);

            QTableWidgetItem *newItem3 = new QTableWidgetItem(quantize_types.at(i).perplexity);
            ui->model_quantize_info->setItem(i, 2, newItem3);

            QTableWidgetItem *newItem4 = new QTableWidgetItem(quantize_types.at(i).recommand);
            ui->model_quantize_info->setItem(i, 3, newItem4);

        }
        QHeaderView* headerView = ui->model_quantize_info->horizontalHeader();//水平表头对象,用来控制列宽
        headerView->setSectionResizeMode(QHeaderView::Stretch);// 设置所有列为等宽且撑满整个表格宽度
    }

}

//接收模型日志
void Expend::recv_log(QString log)
{
    ui->modellog_card->appendPlainText(log);
}

//初始化增殖窗口
void Expend::init_expend()
{
    this->setWindowTitle(wordsObj["expend window"].toString());//标题
    ui->tabWidget->setTabText(0,wordsObj["introduction"].toString());//软件介绍
    ui->tabWidget->setTabText(1,wordsObj["model vocab"].toString());//模型词表
    ui->tabWidget->setTabText(2,wordsObj["model log"].toString());//模型日志
    ui->tabWidget->setTabText(3,wordsObj["model"].toString() + wordsObj["quantize"].toString());//模型量化
    ui->tabWidget->setTabText(4,wordsObj["knowledge"].toString());//知识库
    ui->tabWidget->setTabText(5,wordsObj["text2image"].toString());//文生图
    ui->tabWidget->setTabText(6,wordsObj["voice2text"].toString());//声转文
}

// 接收模型词表
void Expend::recv_vocab(QString vocab_)
{
    vocab = vocab_;
    ui->vocab_card->setPlainText(vocab);
}

//通知显示增殖窗口
void Expend::recv_expend_show(int index_)
{
    init_expend();//主要是设置界面的语言
    if(is_first_show_expend)//第一次显示的话
    {
        is_first_show_expend = false;
        if(vocab == "")
        {
            vocab = wordsObj["lode model first"].toString();
        }
        if(ui->modellog_card->toPlainText() == "")
        {
            ui->modellog_card->setPlainText(wordsObj["lode model first"].toString());
        }
    }
    ui->tabWidget->setCurrentIndex(index_);
    this->setWindowState(Qt::WindowActive); // 激活窗口并恢复正常状态
    this->show();
    this->activateWindow(); // 激活增殖窗口
}

QString Expend::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath="";
#if defined(_WIN32)
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);
#else 
    QFileDialog dlg(NULL, describe);
    dlg.setDirectory(dirpath);//默认打开路径
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);//不使用系统原生的窗口
    dlg.setFileMode(QFileDialog::ExistingFile);dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setNameFilter(format);//筛选格式
    if (dlg.exec() == QDialog::Accepted) {filepath = dlg.selectedFiles().first();}//只要一个文件
#endif

    return filepath;
}

//读取配置文件并应用
void Expend::readConfig()
{
    QFile configfile("./EVA_TEMP/eva_config.ini");
    if(configfile.exists())
    {
        // 创建 QSettings 对象，指定配置文件的名称和格式
        QSettings settings("./EVA_TEMP/eva_config.ini", QSettings::IniFormat);

        // 读取配置文件中的值
        QString sd_modelpath = settings.value("sd_modelpath", "").toString();//sd模型路径
        QString vae_modelpath = settings.value("vae_modelpath", "").toString();//vae模型路径
        QString antiprompt = settings.value("antiprompt", "").toString();//反提示
        int image_width = settings.value("image_width", 512).toInt();//图像宽度
        int image_height = settings.value("image_height", 512).toInt();//图像高度
        QString sample_type = settings.value("sample_type", "euler_a").toString();//采样方式
        int sample_steps = settings.value("sample_steps", 20).toInt();//采样步数
        float cfg = settings.value("cfg", 7.5).toFloat();//相关系数
        int seed = settings.value("seed", -1).toInt();//随机数种子
        int image_nums = settings.value("image_nums", 1).toInt();//生成图像数目
        int clip_skip = settings.value("clip_skip", 2).toInt();//跳层数
        QString sd_prompt = settings.value("sd_prompt", "").toString();//sd提示词

        QString whisper_modelpath = settings.value("whisper_modelpath", "").toString();//whisper模型路径

        voice_params.is_voice = settings.value("voice_enable", "").toBool();//是否启用语音朗读
        voice_params.voice_name = settings.value("voice_name", "").toString();//朗读者

        // 应用值
        QFile sd_modelpath_file(sd_modelpath);
        if(sd_modelpath_file.exists())
        {
            ui->sd_modelpath_lineEdit->setText(sd_modelpath);
        }
        
        QFile vae_modelpath_file(vae_modelpath);
        if(vae_modelpath_file.exists())
        {
            ui->sd_vaepath_lineEdit->setText(vae_modelpath);
        }
        ui->sd_antiprompt_lineEdit->setText(antiprompt);
        ui->sd_imagewidth->setValue(image_width);
        ui->sd_imageheight->setValue(image_height);
        ui->sd_sampletype->setCurrentText(sample_type);
        ui->sd_samplesteps->setValue(sample_steps);
        ui->sd_cfgscale->setValue(cfg);
        ui->sd_seed->setValue(seed);
        ui->sd_batch_count->setValue(image_nums);
        ui->sd_skipclip->setValue(clip_skip);
        ui->sd_prompt_textEdit->setText(sd_prompt);

        QFile whisper_load_modelpath_file(whisper_modelpath);
        if(whisper_load_modelpath_file.exists())
        {
            ui->whisper_load_modelpath_linedit->setText(whisper_modelpath);
            whisper_params.model = whisper_modelpath.toStdString();
        }
        
        for(int i = 0; i < ui->voice_source_comboBox->count(); ++i) 
        {
            QString itemText = ui->voice_source_comboBox->itemText(i);
            if(voice_params.voice_name == itemText)//如果有同名的声源则应用它
            {
                ui->voice_source_comboBox->setCurrentText(voice_params.voice_name);
                ui->voice_enable_radioButton->setChecked(voice_params.is_voice); 
            }
        }
        
    }
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
    else if(obj == ui->sd_antiprompt_lineEdit && event->type() == QEvent::ContextMenu)
    {
        //还原反提示
        ui->sd_antiprompt_lineEdit->setText("EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure");
        return true;
    }

    return QObject::eventFilter(obj, event);
}

//关闭事件
void Expend::closeEvent(QCloseEvent *event)
{
    //--------------保存当前用户配置---------------
    // 创建 QSettings 对象，指定配置文件的名称和格式
    createTempDirectory("./EVA_TEMP");
    QSettings settings("./EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setValue("sd_modelpath",ui->sd_modelpath_lineEdit->text());
    settings.setValue("vae_modelpath",ui->sd_vaepath_lineEdit->text());
    settings.setValue("antiprompt",ui->sd_antiprompt_lineEdit->text());
    settings.setValue("image_width",ui->sd_imagewidth->value());
    settings.setValue("image_height",ui->sd_imageheight->value());
    settings.setValue("sample_type",ui->sd_sampletype->currentText());
    settings.setValue("sample_steps",ui->sd_samplesteps->value());
    settings.setValue("cfg",ui->sd_cfgscale->value());
    settings.setValue("seed",ui->sd_seed->value());
    settings.setValue("image_nums",ui->sd_batch_count->value());
    settings.setValue("clip_skip",ui->sd_skipclip->value());
    settings.setValue("sd_prompt",ui->sd_prompt_textEdit->toPlainText());
    settings.setValue("whisper_modelpath",ui->whisper_load_modelpath_linedit->text());
    settings.setValue("voice_enable",ui->voice_enable_radioButton->isChecked());
    settings.setValue("voice_name",ui->voice_source_comboBox->currentText());
    //event->accept();
}

//-------------------------------------------------------------------------
//----------------------------------声转文相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择whisper路径时响应
void Expend::on_whisper_load_modelpath_button_clicked()
{
    whisper_params.model = customOpenfile(DEFAULT_MODELPATH,"choose whisper model","(*.bin *.gguf)").toStdString();
 
    ui->whisper_load_modelpath_linedit->setText(QString::fromStdString(whisper_params.model));
    emit expend2ui_whisper_modelpath(QString::fromStdString(whisper_params.model));
    ui->whisper_log->setPlainText("选择好了就可以按f2录音了");
}

//开始语音转文字
void Expend::recv_voicedecode(QString wavpath, QString out_format)
{
    whisper_time.restart();
    QString resourcePath = ":/whisper.exe";
    QString localPath = "./EVA_TEMP/whisper.exe";
    createTempDirectory("./EVA_TEMP");
    // 获取资源文件
    QFile resourceFile(resourcePath);
    // 尝试打开资源文件进行读取
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning("cannot open qrc file");
        return ;
    }
    // 读取资源文件的内容
    QByteArray fileData = resourceFile.readAll();
    resourceFile.close();
    QFile localFile(localPath);
    // 尝试打开本地文件进行写入
    if (localFile.open(QIODevice::WriteOnly)) 
    {
        localFile.write(fileData);
        localFile.close();
    }
    // 设置要运行的exe文件的路径
    QString program = localPath;
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << QString::fromStdString(whisper_params.model);//模型路径
    arguments << "-f" << wavpath;//wav文件路径
    arguments << "--language" << QString::fromStdString(whisper_params.language);//识别语种
    arguments << "--threads" << QString::number(max_thread*0.7);
    if(out_format=="txt"){arguments << "--output-txt";}//结果输出为一个txt
    else if(out_format=="文本文档txt"){arguments << "--output-txt";}//结果输出为一个txt
    else if(out_format=="视频字幕srt"){arguments << "--output-srt";}
    else if(out_format=="逗号分隔csv"){arguments << "--output-csv";}
    else if(out_format=="json"){arguments << "--output-json";}
    
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
    whisper_process->start(program, arguments);
}

void Expend::whisper_onProcessStarted()
{
    if(!is_handle_whisper)
    {
        emit expend2ui_state("expend:调用whisper.exe解码录音",USUAL_);
    }
    
}

void Expend::whisper_onProcessFinished()
{
    if(!is_handle_whisper)
    {
        QString content;
        // 文件路径
        QString filePath = qApp->applicationDirPath() + "./EVA_TEMP/" + QString("EVA_") + ".wav.txt";
        // 创建 QFile 对象
        QFile file(filePath);
        // 打开文件
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) 
        {
            QTextStream in(&file);// 创建 QTextStream 对象
            in.setCodec("UTF-8");
            content = in.readAll();// 读取文件内容
        }
        file.close();
        emit expend2ui_state("expend:解码完成 " + QString::number(whisper_time.nsecsElapsed()/1000000000.0,'f',2) + "s ->" + content,SUCCESS_);
        emit expend2ui_voicedecode_over(content);
    }
    else
    {
        ui->whisper_log->appendPlainText("结果已保存在源wav文件目录 "+ QString::number(whisper_time.nsecsElapsed()/1000000000.0,'f',2) + "s");
    }
    is_handle_whisper = false;
}

//用户点击选择wav路径时响应
void Expend::on_whisper_wavpath_pushButton_clicked()
{
    wavpath = customOpenfile(DEFAULT_MODELPATH,"choose whisper model","(*.wav)");
    if(wavpath==""){return;}
    ui->whisper_wavpath_lineedit->setText(wavpath);

}
//用户点击执行转换时响应
void Expend::on_whisper_execute_pushbutton_clicked()
{   
    //执行whisper.exe
    is_handle_whisper = true;
    whisper_process->kill();
    recv_voicedecode(ui->whisper_wavpath_lineedit->text(),ui->whisper_output_format->currentText());

}

//-------------------------------------------------------------------------
//----------------------------------知识库相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择嵌入模型路径时响应
void Expend::on_embedding_txt_modelpath_button_clicked()
{
    server_process->kill();//终止server

    embedding_params.modelpath = customOpenfile(DEFAULT_MODELPATH,"choose embedding model","(*.bin *.gguf)");

    if(embedding_params.modelpath==""){return;}
    ui->embedding_txt_modepath_lineedit->setText(embedding_params.modelpath);

    //尝试启动服务
    embedding_server_start();
}

// 尝试启动server
void Expend::embedding_server_start()
{
    QString resourcePath = ":/server.exe";
    QString localPath = "./EVA_TEMP/server.exe";
       // 获取资源文件
    QFile resourceFile(resourcePath);

    // 尝试打开资源文件进行读取
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning("cannot open qrc file");
        return ;
    }

    // 读取资源文件的内容
    QByteArray fileData = resourceFile.readAll();
    resourceFile.close();

    createTempDirectory("./EVA_TEMP");
    QFile localFile(localPath);

    // 尝试打开本地文件进行写入
    if (localFile.open(QIODevice::WriteOnly)) 
    {
        localFile.write(fileData);
        localFile.close();
    }

    // 设置要运行的exe文件的路径
    QString program = localPath;

    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << embedding_params.modelpath;
    arguments << "--host" << "0.0.0.0";//暴露本机ip
    arguments << "--port" << DEFAULT_EMBEDDING_PORT;//服务端口
    arguments << "--threads" << QString::number(std::thread::hardware_concurrency()*0.5);//使用线程
    arguments << "-cb";//允许连续批处理
    arguments << "--embedding";//允许词嵌入
    arguments << "--log-disable";//不要日志
    // 开始运行程序
    server_process->start(program, arguments);

    //连接信号和槽,获取程序的输出
    ipAddress = getFirstNonLoopbackIPv4Address();

    connect(server_process, &QProcess::readyReadStandardOutput, [=]() {
        QString server_output = server_process->readAllStandardOutput();
        QString log_output;
        //qDebug()<<server_output;
        //启动成功的标志
        if(server_output.contains("warming up the model with an empty run"))
        {
            
            embedding_server_api = "http://" + ipAddress + ":" + DEFAULT_EMBEDDING_PORT + "/v1/embeddings";
            ui->embedding_txt_modepath_lineedit->setText(embedding_server_api);//启动成功后将端点地址写进去
            log_output += wordsObj["embedding"].toString() + "服务启动完成" + "\n";
            log_output += wordsObj["embedding"].toString() + wordsObj["endpoint"].toString() + " " + embedding_server_api;
            if(embedding_server_n_embd!=1024)
            {
                log_output += "\n" + QString("嵌入维度 ") +QString::number(embedding_server_n_embd) + " 不符合要求请更换模型" +"\n";
            }
            else
            {
                log_output += "\n" + QString("嵌入维度 ") +QString::number(embedding_server_n_embd);
            }
            
        }//替换ip地址
        ui->embedding_test_log->appendPlainText(log_output);
        
    });    
    connect(server_process, &QProcess::readyReadStandardError, [=]() {
        QString server_output = server_process->readAllStandardError();
        //qDebug()<<server_output;
        if(server_output.contains("0.0.0.0")){server_output.replace("0.0.0.0", ipAddress);}//替换ip地址
        if(server_output.contains("llm_load_print_meta: n_embd           = "))
        {
            embedding_server_n_embd = server_output.split("llm_load_print_meta: n_embd           = ").at(1).split("\r\n").at(0).toInt();
        }//截获n_embd嵌入维度
    });
}

//进程开始响应
void Expend::server_onProcessStarted()
{
    ;
}

//进程结束响应
void Expend::server_onProcessFinished()
{
    ui->embedding_test_log->appendPlainText("嵌入服务终止");
}

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
void Expend::on_embedding_txt_upload_clicked()
{
    txtpath = customOpenfile(DEFAULT_MODELPATH,"选择一个txt文件嵌入到知识库","(*.txt)");

    ui->embedding_txt_lineEdit->setText(txtpath);
    if(txtpath!=""){ui->embedding_txt_embedding->setEnabled(1);}
    else{ui->embedding_txt_embedding->setEnabled(0);return;}

    preprocessTXT();//预处理文件内容
}

//预处理文件内容
void Expend::preprocessTXT()
{
    //读取
    QString content;
    QFile file(txtpath);
    // 打开文件
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) 
    {
        QTextStream in(&file);// 创建 QTextStream 对象
        in.setCodec("UTF-8");
        content = in.readAll();// 读取文件内容
    }
    file.close();

    //-------------------分词&分段-----------------
    //按字数,250字分一段,每一段保留上一段50字重合
    QStringList paragraphs;
    int splitLength = 250;
    int overlap = 50;
    int splitNums = 0;
    int start = 0;
    int actualNewLength = splitLength - overlap; // 实际上每次新增加的字符长度
    if (splitLength <= overlap) {
        paragraphs << content;
        splitNums++;
    }
    
    while (start < content.length()) 
    {
        if (!content.isEmpty()) {start -= overlap;}// 如果不是第一段，则保留前一段的部分字符

        int endLength = qMin(splitLength, content.length() - start);
        paragraphs << content.mid(start, endLength);
        splitNums++;
        start += actualNewLength;
    }
    
    //显示在待嵌入表格中
    ui->embedding_txt_wait->clear();
    ui->embedding_txt_wait->setRowCount(splitNums);//创建splitNums很多行

    for(int i=0;i<paragraphs.size(); ++i)
    {
        QTableWidgetItem *newItem = new QTableWidgetItem(paragraphs.at(i));
        ui->embedding_txt_wait->setItem(i, 0, newItem);
    }
    ui->embedding_txt_wait->setColumnWidth(0,ui->embedding_txt_wait->width());// 列宽保持控件宽度
    ui->embedding_txt_wait->resizeRowsToContents();// 自动调整行高
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{"待嵌入文本段"});//设置列名
}

//用户点击嵌入时响应
void Expend::on_embedding_txt_embedding_clicked()
{

    //锁定界面
    ui->embedding_txt_upload->setEnabled(0);//上传按钮
    ui->embedding_txt_embedding->setEnabled(0);//嵌入按钮
    ui->embedding_test_pushButton->setEnabled(0);//检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(0);//选择模型按钮

    Embedding_DB.clear();//清空向量数据库
    show_chunk_index = 0;//待显示的嵌入文本段的序号
    //读取待嵌入表格中的内容
    int index_ = 0;
    for(int i=0;i<ui->embedding_txt_wait->rowCount(); ++i)
    {
        QTableWidgetItem *item = ui->embedding_txt_wait->item(i, 0);
        if (item)
        {
            Embedding_DB.append({index_,item->text()});
            index_++;
        }
    }

    //进行嵌入工作,发送ready_embedding_chunks给server.exe
    //测试v1/embedding端点
    QElapsedTimer time;time.start();
    QEventLoop loop;// 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QNetworkRequest request(QUrl(ui->embedding_txt_modepath_lineedit->text()));
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key ="Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());

    ui->embedding_txt_over->clear();//清空已嵌入文本段表格内容
    ui->embedding_txt_over->setRowCount(0);//设置已嵌入文本段表格为0行
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{"已嵌入文本段"});//设置列名
    //-------------------循环发送请求直到文本段处理完-------------------
    for(int o = 0;o<Embedding_DB.size();o++)
    {
        //构造请求的数据体
        QJsonObject json;
        json.insert("model", "gpt-3.5-turbo");
        json.insert("encoding_format", "float");
        json.insert("input", Embedding_DB.at(o).chunk);//待嵌入文本段
        QJsonDocument doc(json);
        QByteArray data = doc.toJson();

        // POST 请求
        QNetworkReply *reply = manager.post(request, data);

        // 处理响应
        QObject::connect(reply, &QNetworkReply::readyRead, [&]() 
        {
            QString jsonString = reply->readAll();

            QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8());// 使用QJsonDocument解析JSON数据
            QJsonObject rootObject = document.object();

            // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
            QJsonArray dataArray = rootObject["data"].toArray();
            QString vector_str = "[";
            for(int i = 0; i < dataArray.size(); ++i)
            {
                QJsonObject dataObj = dataArray[i].toObject();

                // 检查"data"对象中是否存在"embedding"
                if(dataObj.contains("embedding"))
                {
                    QJsonArray embeddingArray = dataObj["embedding"].toArray();
                    // 处理"embedding"数组
                    for(int j = 0; j < embeddingArray.size(); ++j)
                    {
                        Embedding_DB[o].value[j] = embeddingArray[j].toDouble();
                        vector_str += QString::number(Embedding_DB[o].value[j],'f',4)+", ";
                    }
                }
            }
            vector_str += "]";
            ui->embedding_test_log->appendPlainText(QString::number(Embedding_DB.at(o).index+1) + "号文本段嵌入完毕!"+" "+"维度:"+QString::number(Embedding_DB.at(o).value.size()) + " " + "词向量: "+ vector_str);
            
        });
        // 完成
        QObject::connect(reply, &QNetworkReply::finished, [&]() 
        {
            if (reply->error() == QNetworkReply::NoError) 
            {
                // 请求完成，所有数据都已正常接收
                // 显示在已嵌入表格中
                ui->embedding_txt_over->insertRow(ui->embedding_txt_over->rowCount());// 在表格末尾添加新行
                QTableWidgetItem *newItem = new QTableWidgetItem(Embedding_DB.at(show_chunk_index).chunk);
                newItem->setFlags(newItem->flags() & ~Qt::ItemIsEditable);//单元格不可编辑
                newItem->setBackground(QColor(255, 165, 0)); // 设置单元格背景颜色,橘黄色
                ui->embedding_txt_over->setItem(show_chunk_index, 0, newItem);
                ui->embedding_txt_over->setColumnWidth(0,ui->embedding_txt_over->width());// 列宽保持控件宽度
                ui->embedding_txt_over->resizeRowsToContents();// 自动调整行高
                ui->embedding_txt_over->scrollToItem(newItem, QAbstractItemView::PositionAtTop);// 滚动到新添加的行
                show_chunk_index++;
            } 
            else 
            {
                // 请求出错
                ui->embedding_test_log->appendPlainText("请求出错，请确保启动嵌入服务");
            }

            reply->abort();//终止
            reply->deleteLater();
        });

        // 回复完成时退出事件循环
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        // 进入事件循环
        loop.exec();
    }

    //解锁界面
    ui->embedding_txt_upload->setEnabled(1);//上传按钮
    ui->embedding_txt_embedding->setEnabled(1);//嵌入按钮
    ui->embedding_test_pushButton->setEnabled(1);//检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(1);//选择模型按钮

    ui->embedding_test_log->appendPlainText("嵌入完成 耗时 "+ QString::number(time.nsecsElapsed()/1000000000.0,'f',2) + "s");
    emit expend2tool_embeddingdb(Embedding_DB);//发送已嵌入文本段数据给tool
}

//用户点击检索时响应
void Expend::on_embedding_test_pushButton_clicked()
{
    //锁定界面
    ui->embedding_txt_upload->setEnabled(0);//上传按钮
    ui->embedding_txt_embedding->setEnabled(0);//嵌入按钮
    ui->embedding_test_pushButton->setEnabled(0);//检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(0);//选择模型按钮

    QEventLoop loop;// 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QNetworkRequest request(QUrl(ui->embedding_txt_modepath_lineedit->text()));
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key ="Bearer " + QString("sjxx");
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
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() 
    {
        QString jsonString = reply->readAll();
        QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8());// 使用QJsonDocument解析JSON数据
        QJsonObject rootObject = document.object();
        // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
        QJsonArray dataArray = rootObject["data"].toArray();
        QString vector_str = "[";
        
        for(int i = 0; i < dataArray.size(); ++i)
        {
            QJsonObject dataObj = dataArray[i].toObject();
            
            // 检查"data"对象中是否存在"embedding"
            if(dataObj.contains("embedding"))
            {
                QJsonArray embeddingArray = dataObj["embedding"].toArray();
                // 处理"embedding"数组
                for(int j = 0; j < embeddingArray.size(); ++j)
                {
                    user_embedding_vector.value[j] = embeddingArray[j].toDouble();
                    vector_str += QString::number(user_embedding_vector.value[j],'f',4)+", ";
                }
            }
        }
        vector_str += "]";
        ui->embedding_test_log->appendPlainText("查询文本段嵌入完毕!" + QString(" ") + "维度:"+QString::number(user_embedding_vector.value.size()) + " " + "词向量: "+ vector_str);
    });
    // 完成
    QObject::connect(reply, &QNetworkReply::finished, [&]() 
    {
        if (reply->error() == QNetworkReply::NoError) 
        {
            // 请求完成，所有数据都已正常接收
            //计算余弦相似度
            //A向量点积B向量除以(A模乘B模)
            std::vector<std::pair<int, double>> score;
            score = similar_indices(user_embedding_vector.value,Embedding_DB);
            ui->embedding_test_result->appendPlainText("相似度最高的前三个文本段:");
            //将分数前三的结果显示出来
            for(int i = 0;i < 3 && i < score.size();++i)
            {
                //qDebug()<<score[i].first<<score[i].second;
                ui->embedding_test_result->appendPlainText(QString::number(score[i].first + 1) + "号文本段 相似度: " + QString::number(score[i].second));
            }

        } 
        else 
        {
            // 请求出错
            ui->embedding_test_log->appendPlainText("请求出错，请确保启动嵌入服务");
        }
        
        reply->abort();//终止
        reply->deleteLater();
    });

    // 回复完成时退出事件循环
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // 进入事件循环
    loop.exec();
    //解锁界面
    ui->embedding_txt_upload->setEnabled(1);//上传按钮
    ui->embedding_txt_embedding->setEnabled(1);//嵌入按钮
    ui->embedding_test_pushButton->setEnabled(1);//检索按钮
    ui->embedding_txt_modelpath_button->setEnabled(1);//选择模型按钮
    
}

// 计算两个向量的余弦相似度
double Expend::cosine_similarity(const std::array<double, 1024>& a, const std::array<double, 1024>& b)
{
    double dot_product = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (int i = 0; i < 1024; ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot_product / (sqrt(norm_a) * sqrt(norm_b));
}

// 计算user_vector与Embedding_DB中每个元素的相似度，并返回得分最高的3个索引
std::vector<std::pair<int, double>> Expend::similar_indices(const std::array<double, 1024>& user_vector, const QVector<Embedding_vector>& embedding_DB)
{
    std::vector<std::pair<int, double>> scores; // 存储每个索引和其相应的相似度得分

    // 计算相似度得分
    for (const auto& emb : embedding_DB) {
        double sim = cosine_similarity(user_vector, emb.value);
        scores.emplace_back(emb.index, sim);
    }

    // 根据相似度得分排序（降序）
    std::sort(scores.begin(), scores.end(), [](const std::pair<int, double>& a, const std::pair<int, double>& b) 
    {
        return a.second > b.second;
    });

    return scores;
}

//嵌入服务端点改变响应
void Expend::on_embedding_txt_modepath_lineedit_textChanged()
{
    emit expend2tool_serverapi(embedding_server_api);//传递嵌入服务端点
}
//知识库描述改变响应
void Expend::on_embedding_txt_describe_lineEdit_textChanged()
{
    emit expend2ui_embeddingdb_describe(ui->embedding_txt_describe_lineEdit->text());//传递知识库的描述
}

//-------------------------------------------------------------------------
//----------------------------------模型量化相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择待量化模型路径时响应
void Expend::on_model_quantize_row_modelpath_pushButton_clicked()
{
    QString row_modelpath = customOpenfile(DEFAULT_MODELPATH,"需要选择fp32或fp16的gguf模型","(*.gguf)");
    ui->model_quantize_row_modelpath_lineedit->setText(row_modelpath);
}
//用户点击选择重要性矩阵路径时响应
void Expend::on_model_quantize_important_datapath_pushButton_clicked()
{
    QString importtant_datapath = customOpenfile(DEFAULT_MODELPATH,"iq量化方法必须,选择重要性矩阵","(*.dat)");
    ui->model_quantize_important_datapath_lineedit->setText(importtant_datapath);
}
//待量化模型路径改变响应
void Expend::on_model_quantize_row_modelpath_lineedit_textChanged()
{
    output_modelpath_change();//根据待量化模型路径和量化方法填入量化后模型路径
}
//根据待量化模型路径和量化方法填入量化后模型路径
void Expend::output_modelpath_change()
{
    //提取模型名
    QString modelpath = ui->model_quantize_row_modelpath_lineedit->text();
    if(modelpath.contains(".gguf") && QFile::exists(modelpath))
    {
        //重构名称，将尾部的量化词条更换为当前量化方法
        QString output_modelpath = modelpath.replace(modelpath.split(".gguf")[0].split("-").last(),ui->model_quantize_type->currentText());
        //qDebug()<<output_modelpath<<modelpath.split(".gguf")[0].split("-").last()<<modelpath.split(".gguf")[0];
        ui->model_quantize_output_modelpath_lineedit->setText(output_modelpath);

        //顺便改变量化方法说明中的预估量化后大小
        QFileInfo fileInfo1(ui->model_quantize_row_modelpath_lineedit->text());//获取文件大小
        float in_modelsize = fileInfo1.size() /1024.0/1024.0;

        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        float totalPhysMem = memInfo.ullTotalPhys;//总内存

        for(int i=0;i<quantize_types.size(); ++i)
        {
            QTableWidgetItem *item = ui->model_quantize_info->item(i, 1);
            float estimate_modelsize = in_modelsize * (1 - item->text().split("%")[0].toFloat()/100.0);
            QString estimate_modelsize_str = QString::number(estimate_modelsize,'f',1) + " MB";
            QTableWidgetItem *newItem1 = new QTableWidgetItem(estimate_modelsize_str);
            ui->model_quantize_info->setItem(i, 4, newItem1);

            //加星,如果量化后的大小比本机内存小20%以上就加一颗星
            QString star;
            if(estimate_modelsize < totalPhysMem /1024.0/1024.0 * 0.8)
            {
                star = quantize_types.at(i).recommand + "⭐";
            }
            else{star = quantize_types.at(i).recommand;}
            QTableWidgetItem *newItem4 = new QTableWidgetItem(star);
            ui->model_quantize_info->setItem(i, 3, newItem4);
        }
    }

}
//量化方法改变响应
void Expend::on_model_quantize_type_currentIndexChanged(int index)
{
    output_modelpath_change();//根据待量化模型路径和量化方法填入量化后模型路径
}
//用户点击执行量化按钮时响应
void Expend::on_model_quantize_execute_clicked()
{
    //锁定界面
    ui->model_quantize_frame1->setEnabled(0);
    ui->model_quantize_frame2->setEnabled(0);
    ui->model_quantize_frame3->setEnabled(0);
    ui->model_quantize_frame4->setEnabled(0);

    //执行量化
    quantize(ui->model_quantize_row_modelpath_lineedit->text(),ui->model_quantize_output_modelpath_lineedit->text(),ui->model_quantize_important_datapath_lineedit->text(),ui->model_quantize_type->currentText());
}

//执行量化
void Expend::quantize(QString in_modelpath, QString out_modelpath, QString important_datapath, QString quantize_type)
{
    //结束quantize.exe
    quantize_process->kill();

    QString resourcePath = "://quantize.exe";
    QString localPath = "./EVA_TEMP/quantize.exe";
    createTempDirectory("./EVA_TEMP");
    // 获取资源文件
    QFile resourceFile(resourcePath);
    // 尝试打开资源文件进行读取
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning("cannot open qrc file");
        return ;
    }
    // 读取资源文件的内容
    QByteArray fileData = resourceFile.readAll();
    resourceFile.close();
    QFile localFile(localPath);
    // 尝试打开本地文件进行写入
    if (localFile.open(QIODevice::WriteOnly)) 
    {
        localFile.write(fileData);
        localFile.close();
    }
    // 设置要运行的exe文件的路径
    QString program = localPath;
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    if(important_datapath!=""){arguments << "--imatrix" << important_datapath;}//重要性矩阵路径
    arguments << in_modelpath;//待量化模型路径
    arguments << out_modelpath;//输出路径
    arguments << quantize_type;//量化方法
    arguments << QString::number(max_thread*0.7);//使用线程数
    
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
void Expend::quantize_onProcessStarted()
{

}
//结束信号
void Expend::quantize_onProcessFinished()
{
    //解锁界面
    ui->model_quantize_frame1->setEnabled(1);
    ui->model_quantize_frame2->setEnabled(1);
    ui->model_quantize_frame3->setEnabled(1);
    ui->model_quantize_frame4->setEnabled(1);

    ui->model_quantize_log->appendPlainText("量化完成! 模型保存:" + ui->model_quantize_output_modelpath_lineedit->text());
    QFileInfo fileInfo1(ui->model_quantize_row_modelpath_lineedit->text());//获取文件大小
    float modelsize1_MB = fileInfo1.size() /1024.0/1024.0;
    QFileInfo fileInfo2(ui->model_quantize_output_modelpath_lineedit->text());//获取文件大小
    float modelsize2_MB = fileInfo2.size() /1024.0/1024.0;
    ui->model_quantize_log->appendPlainText(QString::number(modelsize1_MB) + " MB" + " -> " + QString::number(modelsize2_MB) + " MB" + " 压缩率:" + QString::number((1-modelsize2_MB/modelsize1_MB)*100) + "%");

}


//-------------------------------------------------------------------------
//----------------------------------文生图相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择sd模型路径时响应  
void Expend::on_sd_modelpath_pushButton_clicked()
{
    sd_params.modelpath = customOpenfile(DEFAULT_MODELPATH,"choose sd model","(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if(sd_params.modelpath!=""){ui->sd_modelpath_lineEdit->setText(sd_params.modelpath);}
    if(sd_params.modelpath.contains("xl")){ui->sd_log->appendPlainText("检测到xl模型，推荐将图像宽高设置在768以上");}

}
//用户点击选择vae模型路径时响应 
void Expend::on_sd_vaepath_pushButton_clicked()
{
    sd_params.vaepath = customOpenfile(DEFAULT_MODELPATH,"choose sd model","(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if(sd_params.vaepath!=""){ui->sd_vaepath_lineEdit->setText(sd_params.vaepath);}

}

//用户点击开始绘制时响应  
void Expend::on_sd_draw_pushButton_clicked()
{
    if(is_handle_sd && ui->sd_prompt_textEdit->toPlainText()=="")
    {
        ui->sd_log->appendPlainText("请输入提示词告诉模型你想绘制图像的样子");
        return;
    }
    else if(is_handle_sd && ui->sd_modelpath_lineEdit->text()=="")
    {
        ui->sd_log->appendPlainText("请先指定sd模型路径");
        return;
    }
    else if(!is_handle_sd)
    {
        emit expend2ui_state("expend:sd.exe程序绘制中",USUAL_);
    }

    //锁定界面
    ui->groupBox_6->setEnabled(0);

    //收集参数
    sd_params.prompt = ui->sd_prompt_textEdit->toPlainText();
    sd_params.modelpath = ui->sd_modelpath_lineEdit->text();
    sd_params.vaepath = ui->sd_vaepath_lineEdit->text();
    sd_params.width = ui->sd_imagewidth->value();
    sd_params.height = ui->sd_imageheight->value();
    sd_params.sampletype = ui->sd_sampletype->currentText();
    sd_params.steps = ui->sd_samplesteps->value();
    sd_params.cfg_scale = ui->sd_cfgscale->value();
    sd_params.seed = ui->sd_seed->value();
    sd_params.clip_skip = ui->sd_skipclip->value();
    sd_params.batch_count = ui->sd_batch_count->value();
    sd_params.negative_prompt = ui->sd_antiprompt_lineEdit->text();
    
    QTime currentTime = QTime::currentTime();// 获取当前时间
    QString timeString = currentTime.toString("-hh-mm-ss");// 格式化时间为时-分-秒
    sd_params.outpath = "./EVA_TEMP/sd_output" + timeString + ".png";

    //结束sd.exe
    sd_process->kill();

    QString resourcePath = "://sd.exe";
    QString localPath = "./EVA_TEMP/sd.exe";
    createTempDirectory("./EVA_TEMP");
    // 获取资源文件
    QFile resourceFile(resourcePath);
    // 尝试打开资源文件进行读取
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning("cannot open qrc file");
        return ;
    }
    // 读取资源文件的内容
    QByteArray fileData = resourceFile.readAll();
    resourceFile.close();
    QFile localFile(localPath);
    // 尝试打开本地文件进行写入
    if (localFile.open(QIODevice::WriteOnly)) 
    {
        localFile.write(fileData);
        localFile.close();
    }
    // 设置要运行的exe文件的路径
    QString program = localPath;
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;

    if(img2img)
    {
        arguments << "-M" << "img2img";//运行模式 图生图
        arguments << "-i" << uploadimagepath;
        img2img = false;
    }
    else
    {
        arguments << "-M" << "txt2img";//运行模式 文生图
    }

    arguments << "-m" << sd_params.modelpath;//模型路径
    arguments << "--vae" << sd_params.vaepath;//vae路径
    arguments << "--sampling-method" << sd_params.sampletype;//采样方法
    arguments << "--clip-skip" << QString::number(sd_params.clip_skip);//跳层
    arguments << "-t" << QString::number(sd_params.nthreads);//线程数
    arguments << "-o" << sd_params.outpath;//输出路径
    
    arguments << "-p" << sd_params.extra_prompt + sd_params.prompt;//提示词
    arguments << "-n" << sd_params.negative_prompt;//反向提示词
    arguments << "--cfg-scale" << QString::number(sd_params.cfg_scale);//相关系数
    arguments << "--strength" << QString::number(sd_params.noise_strength);//噪声系数
    arguments << "-W" << QString::number(sd_params.width);//图像宽
    arguments << "-H" << QString::number(sd_params.height);//图像长
    arguments << "--steps" << QString::number(sd_params.steps);//采样步数
    arguments << "-s" << QString::number(sd_params.seed);//随机种子
    arguments << "-b" << QString::number(sd_params.batch_count);//出图张数

    //连接信号和槽,获取程序的输出
    connect(sd_process, &QProcess::readyReadStandardOutput, [=]() {
        sd_process_output = sd_process->readAllStandardOutput();
        QTextCursor cursor(ui->sd_log->textCursor());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(sd_process_output);
        ui->sd_log->verticalScrollBar()->setValue(ui->sd_log->verticalScrollBar()->maximum());//滚动条滚动到最下面
        if(sd_process_output.contains("CUDA error"))
        {sd_process->kill();}
    });    
    connect(sd_process, &QProcess::readyReadStandardError, [=]() {
        sd_process_output = sd_process->readAllStandardError();
        QTextCursor cursor(ui->sd_log->textCursor());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(sd_process_output);
        ui->sd_log->verticalScrollBar()->setValue(ui->sd_log->verticalScrollBar()->maximum());//滚动条滚动到最下面
        if(sd_process_output.contains("CUDA error"))
        {sd_process->kill();}
    });
    sd_process->start(program, arguments);

}
//进程开始响应
void Expend::sd_onProcessStarted()
{

}
//进程结束响应
void Expend::sd_onProcessFinished()
{
    //解锁界面
    ui->groupBox_6->setEnabled(1);

    //绘制结果
    QImage image(sd_params.outpath);
    int originalWidth = image.width()/devicePixelRatioF();
    int originalHeight = image.height()/devicePixelRatioF();
    QTextCursor cursor(ui->sd_result->textCursor());
    cursor.movePosition(QTextCursor::End);

    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);  // 设置图片的宽度
    imageFormat.setHeight(originalHeight); // 设置图片的高度
    imageFormat.setName(sd_params.outpath);  // 图片资源路径
    cursor.insertImage(imageFormat);
    ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum());//滚动条滚动到最下面
    //如果是多幅
    if(sd_params.batch_count>1)
    {
        for(int i = 1; i < sd_params.batch_count; ++i)
        {
            QTextImageFormat imageFormats;
            imageFormats.setWidth(originalWidth);  // 设置图片的宽度
            imageFormats.setHeight(originalHeight); // 设置图片的高度
            imageFormats.setName(sd_params.outpath.split(".png")[0] + "_" + QString::number(i+1) + ".png");  // 图片资源路径
            cursor.insertImage(imageFormats);
            ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum());//滚动条滚动到最下面
        }
    }

    //处理工具调用情况
    if(!is_handle_sd && originalWidth>0)
    {
        is_handle_sd = true;
        emit expend2ui_state("expend:绘制完毕",USUAL_);
        emit expend2tool_drawover(sd_params.outpath,1);//绘制完成信号
    }
    else if(!is_handle_sd)
    {
        is_handle_sd = true;
        if(sd_process_output.contains("CUDA error"))
        {
            emit expend2ui_state("expend:绘制失败，显存不足，请用户减小图像宽高",WRONG_);
            emit expend2tool_drawover("绘制失败，显存不足，请用户减小图像宽高",0);//绘制完成信号
        }
        else
        {
            emit expend2ui_state("expend:绘制失败，注意描述的文本需要用纯英文",WRONG_);
            emit expend2tool_drawover("绘制失败，注意描述的文本需要用纯英文",0);//绘制完成信号
        }
        
    }


}
//sd模型路径改变响应
void Expend::on_sd_modelpath_lineEdit_textChanged()
{
    //提取模型名
    QString modelpath = ui->sd_modelpath_lineEdit->text();

    if(QFile::exists(modelpath))
    {
        if(modelpath.contains("fp16"))
        {
            QString vae_modelpath = modelpath.replace("fp16","vae");
            if(QFile::exists(vae_modelpath))
            {
                ui->sd_vaepath_lineEdit->setText(vae_modelpath);
            }
            
        }
        else if(modelpath.contains("fp32"))
        {
            QString vae_modelpath = modelpath.replace("fp32","vae");
            if(QFile::exists(vae_modelpath))
            {
                ui->sd_vaepath_lineEdit->setText(vae_modelpath);
            }
        }
        else if(modelpath.contains("q8_0"))
        {
            QString vae_modelpath = modelpath.replace("q8_0","vae");
            if(QFile::exists(vae_modelpath))
            {
                ui->sd_vaepath_lineEdit->setText(vae_modelpath);
            }
        }
    }
}

//上传图像文本区改变响应
void Expend::on_sd_uploadimage_textEdit_textChanged()
{
    if(uploadimaging){return;}//防止卡死
    QString str = ui->sd_uploadimage_textEdit->toPlainText();

    if(str.contains("file:///"))
    {
        if(str.contains(".png")||str.contains(".jpg")||str.contains(".jpeg")||str.contains(".bmp"))
        {
            QString imagepath = str.split("file:///")[1];
            QFile upload_file(imagepath);
            if(upload_file.exists())
            {
                uploadimaging = true;
                ui->sd_uploadimage_textEdit->clear();
                
                //文件存在则展示图像，并且记录当前的图像路径，并且解锁图生图按钮
                // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
                QImage image(imagepath);
                
                int originalWidth = image.width()/devicePixelRatioF()/1.5;
                int originalHeight = image.height()/devicePixelRatioF()/1.5;

                QTextCursor cursor(ui->sd_uploadimage_textEdit->textCursor());
                cursor.movePosition(QTextCursor::End);

                QTextImageFormat imageFormat;
                imageFormat.setWidth(originalWidth);  // 设置图片的宽度
                imageFormat.setHeight(originalHeight); // 设置图片的高度
                imageFormat.setName(imagepath);  // 图片资源路径
                cursor.insertImage(imageFormat);

                uploadimagepath = imagepath;
                ui->sd_draw_pushButton_2->setEnabled(1);//解锁
                uploadimaging = false;
                return;
            }
        }
    }

    ui->sd_draw_pushButton_2->setEnabled(0);//上锁
}

//用户点击图生图时响应  
void Expend::on_sd_draw_pushButton_2_clicked()
{
    img2img = true;
    ui->sd_draw_pushButton->click();
}

//接收到tool的开始绘制图像信号
void Expend::recv_draw(QString prompt_)
{
    //判断是否空闲
    if(!ui->sd_draw_pushButton->isEnabled())
    {
        emit expend2tool_drawover("stablediffusion正在运行，请稍后重试",0);//绘制完成信号
        return;
    }
    else if(ui->sd_modelpath_lineEdit->text() == "")
    {
        emit expend2tool_drawover("指令无效，请先让用户在增殖窗口指定sd模型路径",0);//绘制完成信号
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

//获取系统可用声源并设置到combobox
void Expend::get_sys_voice()
{
    QTextToSpeech *speech = new QTextToSpeech();
    // 遍历所有可用音色
    foreach (const QVoice &voice, speech->availableVoices()) 
    {
        // qDebug() << "Name:" << voice.name();
        // qDebug() << "Age:" << voice.age();
        // qDebug() << "Gender:" << voice.gender();
        ui->voice_source_comboBox->addItem(voice.name());//添加到下拉框
    }
}

//用户点击启用声音选项响应
void Expend::voice_enable_change()
{
    Voice_Params Voice_Params_;

    Voice_Params_.voice_name = ui->voice_source_comboBox->currentText();

    if(ui->voice_enable_radioButton->isChecked())
    {
        Voice_Params_.is_voice = true;
        ui->voice_source_comboBox->setEnabled(1);
    }
    else
    {
        Voice_Params_.is_voice = false;
        ui->voice_source_comboBox->setEnabled(0);
    }

    emit expend2ui_voiceparams(Voice_Params_);
}

//用户切换音源响应
void Expend::voice_source_change()
{
    Voice_Params Voice_Params_;

    Voice_Params_.voice_name = ui->voice_source_comboBox->currentText();

    if(ui->voice_enable_radioButton->isChecked())
    {
        Voice_Params_.is_voice = true;
    }
    else
    {
        Voice_Params_.is_voice = false;
    }

    emit expend2ui_voiceparams(Voice_Params_);
}