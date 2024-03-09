#include "expend.h"
#include "ui_expend.h"

Expend::Expend(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Expend)
{
    ui->setupUi(this);
    //设置风格
    // QFile file(":/ui/QSS-master/ConsoleStyle.qss");
    // file.open(QFile::ReadOnly);
    // QString stylesheet = tr(file.readAll());
    // this->setStyleSheet(stylesheet);
    // file.close();
    //初始化选项卡
    ui->version_card->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    //ui->model_vocab->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    //ui->modellog_cards->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    ui->info_card->setReadOnly(1);
    ui->vocab_card->setReadOnly(1);//这样才能滚轮放大
    ui->modellog_card->setReadOnly(1);
    ui->tabWidget->setCurrentIndex(3);//默认显示模型日志    
    
}

Expend::~Expend()
{
    delete ui;
}

//-------------------------------------------------------------------------
//----------------------------------界面相关--------------------------------
//-------------------------------------------------------------------------

//用户切换选项卡时响应
//0版本日志,1软件介绍,2模型词表,3模型日志
void Expend::on_tabWidget_tabBarClicked(int index)
{
    if(index==2 && is_first_show_vocab)//第一次点模型词表
    {
        ui->vocab_card->setPlainText(vocab);
        is_first_show_vocab = false;
    }
    if(index==1 && is_first_show_info)//第一次点软件介绍
    {
        is_first_show_info = false;
        // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
        QString imagePath = ":/ui/run.png";
        QImage image(imagePath);
        int originalWidth = image.width()/devicePixelRatioF();
        int originalHeight = image.height()/devicePixelRatioF();

        QTextCursor cursor(ui->info_card->textCursor());
        cursor.movePosition(QTextCursor::End);

        QTextImageFormat imageFormat;
        imageFormat.setWidth(originalWidth);  // 设置图片的宽度
        imageFormat.setHeight(originalHeight); // 设置图片的高度
        imageFormat.setName(imagePath);  // 图片资源路径

        cursor.insertImage(imageFormat);
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
    ui->tabWidget->setTabText(0,wordsObj["version log"].toString());//版本日志
    ui->tabWidget->setTabText(1,wordsObj["introduction"].toString());//软件介绍
    ui->tabWidget->setTabText(2,wordsObj["model vocab"].toString());//模型词表
    ui->tabWidget->setTabText(3,wordsObj["model log"].toString());//模型日志
    ui->tabWidget->setTabText(4,wordsObj["model"].toString() + wordsObj["proliferation"].toString());//模型增殖
    ui->tabWidget->setTabText(5,wordsObj["tool"].toString() + wordsObj["proliferation"].toString());//工具增殖
    ui->tabWidget->setTabText(6,wordsObj["image"].toString() + wordsObj["proliferation"].toString());//图像增殖
    ui->tabWidget->setTabText(7,wordsObj["voice"].toString() + wordsObj["proliferation"].toString());//语音增殖
    ui->tabWidget->setTabText(8,wordsObj["video"].toString() + wordsObj["proliferation"].toString());//视频增殖

}

// 接收模型词表
void Expend::recv_vocab(QString vocab_)
{
    vocab = vocab_;
    ui->vocab_card->setPlainText(vocab);
}

//通知显示扩展窗口
void Expend::recv_expend_show(bool is_show)
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
    whisper_model_path = QFileDialog::getOpenFileName(this,"choose whisper model",whisper_model_path);
    ui->voice_load_modelpath_linedit->setText(whisper_model_path);
}

//用户点击加载whisper模型时响应
void Expend::on_voice_load_load_button_clicked()
{
    emit expend2whisper_modelpath(ui->voice_load_modelpath_linedit->text());
}
