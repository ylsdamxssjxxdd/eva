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
    ui->embedding_server_log->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    //ui->embedding_txt_over->setStyleSheet("background-color: rgba(128, 128, 128, 127);");//灰色
    server_process = new QProcess(this);// 创建一个QProcess实例用来启动server.exe
    connect(server_process, &QProcess::started, this, &Expend::server_onProcessStarted);//连接开始信号
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Expend::server_onProcessFinished);//连接结束信号        

    
    ui->embedding_txt_wait->setColumnCount(1);//设置一列
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{"待嵌入"});//设置列名

    ui->embedding_txt_over->setColumnCount(1);//设置一列
    ui->embedding_txt_over->setHorizontalHeaderLabels(QStringList{"已嵌入"});//设置列名
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
        QString imagePath = ":/ui/server_demo.png";
        QImage image(imagePath);
        int originalWidth = image.width()/devicePixelRatioF();
        int originalHeight = image.height()/devicePixelRatioF();

        QTextCursor cursor(ui->info_card->textCursor());
        cursor.movePosition(QTextCursor::Start);

        QTextImageFormat imageFormat;
        imageFormat.setWidth(originalWidth);  // 设置图片的宽度
        imageFormat.setHeight(originalHeight); // 设置图片的高度
        imageFormat.setName(imagePath);  // 图片资源路径

        cursor.insertImage(imageFormat);
        cursor.insertText("\n\n");

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
    ui->tabWidget->setTabText(3,wordsObj["knowledge"].toString());//知识库
    ui->tabWidget->setTabText(4,wordsObj["model"].toString() + wordsObj["proliferation"].toString());//模型增殖
    ui->tabWidget->setTabText(5,wordsObj["image"].toString() + wordsObj["proliferation"].toString());//图像增殖
    ui->tabWidget->setTabText(6,wordsObj["voice"].toString() + wordsObj["proliferation"].toString());//语音增殖
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
    if(is_first_choose_whispermodel)
    {
        this->close();
        is_first_choose_whispermodel=false;
    }
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
//-------------------------------------------------------------------------
//----------------------------------知识库相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择嵌入模型路径时响应
void Expend::on_embedding_modelpath_button_clicked()
{
    embedding_params.modelpath = QFileDialog::getOpenFileName(this,"choose embedding model",embedding_params.modelpath);
    ui->embedding_modelpath_lineedit->setText(embedding_params.modelpath);
    if(embedding_params.modelpath!=""){ui->embedding_server_start->setEnabled(1);}
    else{ui->embedding_server_start->setEnabled(0);}
}

// 尝试启动server
void Expend::on_embedding_server_start_clicked()
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
    arguments << "--port" << embedding_port;//服务端口
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
        //启动成功的标志
        if(server_output.contains("warming up the model with an empty run"))
        {
            server_output += "\n" + wordsObj["server"].toString() + wordsObj["address"].toString() + " " + QString("http://") + ipAddress + ":"+ embedding_port;
            server_output += "\n" + wordsObj["embedding"].toString() + wordsObj["endpoint"].toString() + " " + "v1/embeddings"+"\n";
            ui->embedding_server_start->setEnabled(0);
            ui->embedding_server_stop->setEnabled(1);//启动成功后只能点终止按钮
            ui->embedding_modelpath_button->setEnabled(0);
        }//替换ip地址
        ui->embedding_server_log->append(server_output);
        
    });    
    connect(server_process, &QProcess::readyReadStandardError, [=]() {
        QString server_output = server_process->readAllStandardError();
        if(server_output.contains("0.0.0.0")){server_output.replace("0.0.0.0", ipAddress);}//替换ip地址
        ui->embedding_server_log->append(server_output);
    });
}

//进程开始响应
void Expend::server_onProcessStarted()
{
    ui->embedding_server_log->append("嵌入服务启动");
}

//进程结束响应
void Expend::server_onProcessFinished()
{
    ui->embedding_server_start->setEnabled(1);
    ui->embedding_modelpath_button->setEnabled(1);
    ui->embedding_server_stop->setEnabled(0);
    ui->embedding_server_log->append("嵌入服务终止");
}
//终止server
void Expend::on_embedding_server_stop_clicked()
{
    server_process->kill();
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
    txtpath = QFileDialog::getOpenFileName(this,"选择一个txt文件嵌入到知识库",txtpath);
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

    //分段
    //按字数,100字分一段,每一段保留上一段20字重合
    QStringList split_txt;
    int splitLength = 100;
    int overlap = 20;
    int splitNums = 0;

    if (splitLength <= overlap) {
        split_txt << content;
        splitNums++;
    }

    int start = 0;
    int actualNewLength = splitLength - overlap; // 实际上每次新增加的字符长度
    while (start < content.length()) 
    {
        if (!content.isEmpty()) {start -= overlap;}// 如果不是第一段，则保留前一段的部分字符

        int endLength = qMin(splitLength, content.length() - start);
        split_txt << content.mid(start, endLength);
        splitNums++;
        start += actualNewLength;
    }
    
    //显示在待嵌入表格中
    ui->embedding_txt_wait->clear();
    ui->embedding_txt_wait->setRowCount(splitNums);//创建splitNums很多行

    for(int i=0;i<split_txt.size(); ++i)
    {
        QTableWidgetItem *newItem = new QTableWidgetItem(split_txt.at(i));
        ui->embedding_txt_wait->setItem(i, 0, newItem);
    }
    ui->embedding_txt_wait->setColumnWidth(0,ui->embedding_txt_wait->width());// 列宽保持控件宽度
    ui->embedding_txt_wait->resizeRowsToContents();// 自动调整行高
    ui->embedding_txt_wait->setHorizontalHeaderLabels(QStringList{"待嵌入"});//设置列名
}
//用户点击嵌入时响应
void Expend::on_embedding_txt_embedding_clicked()
{
    ready_embedding_split_txt.clear();
    //读取待嵌入表格中的内容
    for(int i=0;i<ui->embedding_txt_wait->rowCount(); ++i)
    {
        QTableWidgetItem *item = ui->embedding_txt_wait->item(i, 0);
        if (item && item->text()!="") //空的内容不要
        {
            ready_embedding_split_txt << item->text();
        }
    }
    //进行嵌入工作,发送ready_embedding_split_txt给server.exe
    //测试v1/embedding端点

    //显示在已嵌入表格中
    ui->embedding_txt_over->setRowCount(ready_embedding_split_txt.size());//创建splitNums行表

    for(int i=0;i<ready_embedding_split_txt.size(); ++i)
    {
        QTableWidgetItem *newItem = new QTableWidgetItem(ready_embedding_split_txt.at(i));
        newItem->setFlags(newItem->flags() & ~Qt::ItemIsEditable);//单元格不可编辑
        newItem->setBackground(QColor(128, 128, 128)); // 设置单元格背景颜色
        ui->embedding_txt_over->setItem(i, 0, newItem);
    }
    ui->embedding_txt_over->setColumnWidth(0,ui->embedding_txt_over->width());// 列宽保持控件宽度
    ui->embedding_txt_over->resizeRowsToContents();// 自动调整行高


}