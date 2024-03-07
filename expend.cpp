#include "expend.h"
#include "ui_expend.h"

Expend::Expend(QWidget *parent, QJsonObject wordsObj, QString vocab, QStringList model_logs) :
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
    
    ui->version_log->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    //ui->model_vocab->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    //ui->model_logs->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    vocab_ = vocab;
    wordsObj_ = wordsObj;
    this->setWindowTitle(wordsObj_["expend window"].toString());
    for(int i=0;i<model_logs.size();i++)
    {
        model_logs_ += model_logs.at(i);
    }
    
    ui->model_vocab->setReadOnly(1);//这样才能滚轮放大
    ui->model_log->setReadOnly(1);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);//取消自带的问号按钮

    QString imagePath = ":/ui/run.png";
    
    // 设置图片的新宽度
    int newWidth = 1000; // 新宽度，高度自适应

    // 使用HTML插入调整尺寸后的图片
    ui->textBrowser->setHtml(QString("<img src=\"%1\" width=\"%2\"/>")
                    .arg(imagePath).arg(newWidth));
    

    ui->model_log->setPlainText(model_logs_);
    ui->tabWidget->setCurrentIndex(3);    
    // 延迟设置滚动条位置
    QTimer::singleShot(0, this, [this]() {ui->model_log->verticalScrollBar()->setValue(ui->model_log->verticalScrollBar()->maximum());});
    
}

Expend::~Expend()
{
    delete ui;
}

void Expend::on_tabWidget_tabBarClicked(int index)
{
    //ui->textBrowser->verticalScrollBar()->setValue(0);
    if(index==1 && !is_show_vocab)
    {
        //QElapsedTimer time1;time1.start();
        ui->model_vocab->setPlainText(vocab_);
        //qDebug()<<QString::number(time1.nsecsElapsed()/1000000000.0,'f',4);
        is_show_vocab = true;
    }
    if(index==2)
    {
        QTimer::singleShot(0, this, [this]() {ui->textBrowser->verticalScrollBar()->setValue(0);});
    }
}


void Expend::recv_log(QString log)
{
    if(log.contains("load_percent"))
    {
        //上一次也是这个则删除
        if(load_percent_tag)
        {
            QTextCursor cursor = ui->model_log->textCursor();
            // 移动光标到文档的末尾
            cursor.movePosition(QTextCursor::End);
            cursor.movePosition(QTextCursor::StartOfLine);
        
            // 选择从当前位置到文档末尾的文本
            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            
            // 删除选中的文本（即最后一行）
            cursor.removeSelectedText();
            
            // 删除因为删除最后一行而产生的额外的换行符
            cursor.deletePreviousChar();
            // 将修改后的光标应用回编辑器
            ui->model_log->setTextCursor(cursor);
        }
        load_percent_tag = true;
    }
    else
    {
        load_percent_tag = false;
    }
    ui->model_log->appendPlainText(log);
    
}

void Expend::recv_vocab(QString vocab)
{
    vocab_ = vocab;
    ui->model_vocab->setPlainText(vocab);
}