#include "expend.h"
#include "ui_expend.h"

Expend::Expend(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Expend)
{
    ui->setupUi(this);
    //设置风格
    QFile file(":/ui/QSS-master/ConsoleStyle.qss");
    file.open(QFile::ReadOnly);
    QString stylesheet = tr(file.readAll());
    this->setStyleSheet(stylesheet);
    file.close();
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

//用户切换选项卡时响应
//0版本日志,1模型词表,2软件介绍,3模型日志
void Expend::on_tabWidget_tabBarClicked(int index)
{
    if(index==1 && is_first_show_vocab)//第一次点模型词表
    {
        ui->vocab_card->setPlainText(vocab);
        is_first_show_vocab = false;
    }
    if(index==2 && is_first_show_info)//第一次点软件介绍
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

//接收模型词表
void Expend::recv_vocab(QString vocab_)
{
    vocab = vocab_;
    ui->vocab_card->setPlainText(vocab);
}

//通知显示扩展窗口
void Expend::recv_expend_show(bool is_show)
{
    if(is_first_show_expend)//第一次显示的话
    {
        is_first_show_expend = false;
        qDebug()<<ui->vocab_card->toPlainText();
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
