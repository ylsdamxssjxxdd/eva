#include "expend.h"
#include "ui_expend.h"

Expend::Expend(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Expend)
{
    ui->setupUi(this);
    //初始化选项卡
    ui->info_card->setReadOnly(1);
    ui->vocab_card->setReadOnly(1);//这样才能滚轮放大
    ui->modellog_card->setReadOnly(1);
    ui->tabWidget->setCurrentIndex(2);//默认显示模型日志 

    ui->voice_load_log->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    
}

Expend::~Expend()
{
    delete ui;
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
        ui->info_card->setMarkdown(readme_content);

        // // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
        // QString imagePath = ":/ui/run.png";
        // QImage image(imagePath);
        // int originalWidth = image.width()/devicePixelRatioF();
        // int originalHeight = image.height()/devicePixelRatioF();

        // QTextCursor cursor(ui->info_card->textCursor());
        // cursor.movePosition(QTextCursor::End);

        // QTextImageFormat imageFormat;
        // imageFormat.setWidth(originalWidth);  // 设置图片的宽度
        // imageFormat.setHeight(originalHeight); // 设置图片的高度
        // imageFormat.setName(imagePath);  // 图片资源路径

        // cursor.insertImage(imageFormat);
        //强制延迟见顶
        QTimer::singleShot(0, this, [this]() {ui->info_card->verticalScrollBar()->setValue(0);ui->info_card->horizontalScrollBar()->setValue(0);});
    }
}

//接收模型日志
void Expend::recv_log(QString log)
{
    ui->modellog_card->appendPlainText(log);
}

//初始化扩展窗口
void Expend::init_expend()
{
    ui->tabWidget->setTabText(0,wordsObj["introduction"].toString());//软件介绍
    ui->tabWidget->setTabText(1,wordsObj["model vocab"].toString());//模型词表
    ui->tabWidget->setTabText(2,wordsObj["model log"].toString());//模型日志
    ui->tabWidget->setTabText(3,wordsObj["model"].toString() + wordsObj["proliferation"].toString());//模型增殖
    ui->tabWidget->setTabText(4,wordsObj["tool"].toString() + wordsObj["proliferation"].toString());//工具增殖
    ui->tabWidget->setTabText(5,wordsObj["image"].toString() + wordsObj["proliferation"].toString());//图像增殖
    ui->tabWidget->setTabText(6,wordsObj["voice"].toString() + wordsObj["proliferation"].toString());//语音增殖
    ui->tabWidget->setTabText(7,wordsObj["video"].toString() + wordsObj["proliferation"].toString());//视频增殖
}

// 接收模型词表
void Expend::recv_vocab(QString vocab_)
{
    vocab = vocab_;
    ui->vocab_card->setPlainText(vocab);
}

//通知显示扩展窗口
void Expend::recv_expend_show(int index_)
{
    init_expend();
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
    this->setWindowTitle(wordsObj["expend window"].toString());
    this->show();
    this->activateWindow(); // 激活扩展窗口
}

//-------------------------------------------------------------------------
//----------------------------------语音相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择whisper路径时响应
void Expend::on_voice_load_modelpath_button_clicked()
{
    whisper_params.model = QFileDialog::getOpenFileName(this,"choose whisper model",QString::fromStdString(whisper_params.model)).toStdString();
    ui->voice_load_modelpath_linedit->setText(QString::fromStdString(whisper_params.model));
    emit expend2ui_whisper_modelpath(QString::fromStdString(whisper_params.model));
    ui->voice_load_log->setPlainText("选择好了就可以按f2录音了");
    if(is_first_choose_whispermodel){this->close();is_first_choose_whispermodel=false;}
}

//开始语音转文字
void Expend::recv_voicedecode(QString wavpath)
{
    whisper_time.restart();

    QString resourcePath = ":/whisper.exe";
    QString localPath = "./EVA_TEMP/whisper.exe";
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
    arguments << "--output-txt";//结果输出为一个txt
    
    // 开始运行程序
    QProcess *whisper_process;//用来启动whisper.exe
    whisper_process = new QProcess(this);//实例化
    connect(whisper_process, &QProcess::started, this, &Expend::whisper_onProcessStarted);//连接开始信号
    connect(whisper_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Expend::whisper_onProcessFinished);//连接结束信号
    //连接信号和槽,获取程序的输出
    connect(whisper_process, &QProcess::readyReadStandardOutput, [=]() {
        QString output = whisper_process->readAllStandardOutput();
        ui->voice_load_log->appendPlainText(output);
    });    
    connect(whisper_process, &QProcess::readyReadStandardError, [=]() {
        QString output = whisper_process->readAllStandardError();
        ui->voice_load_log->appendPlainText(output);
    });
    whisper_process->start(program, arguments);
}

void Expend::whisper_onProcessStarted()
{
    emit expend2ui_state("expend:调用whisper.exe解码录音",USUAL_);
}

void Expend::whisper_onProcessFinished()
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