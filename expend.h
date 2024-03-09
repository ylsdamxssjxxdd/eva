#ifndef EXPEND_H
#define EXPEND_H

#include <QWidget>
#include <QJsonObject>
#include <QScrollBar>
#include <QTimer>
#include <QDebug>
#include <QFileDialog>
#include <QProcess>
#include <QFile>
#include <QTextStream>

#include "xconfig.h"
namespace Ui {
class Expend;
}

class Expend : public QWidget
{
    Q_OBJECT
//-------------------------------------------------------------------------
//----------------------------------界面相关--------------------------------
//-------------------------------------------------------------------------
public:
    Expend(QWidget *parent = nullptr);
    ~Expend();
    QJsonObject wordsObj;
    QString vocab;
    QString model_logs;
    bool is_first_show_expend = true;
    bool is_first_show_vocab = true;
    bool is_first_show_logs = true;
    bool is_first_show_info = true;
    bool load_percent_tag = false;
    void init_expend();//初始化扩展窗口
public slots:
    void recv_log(QString log);
    void recv_vocab(QString vocab);    
    void recv_expend_show(int index_);//通知显示扩展窗口
private slots:
    void on_tabWidget_tabBarClicked(int index);//用户切换选项卡时响应    
private:
    Ui::Expend *ui;
//-------------------------------------------------------------------------
//----------------------------------语音相关--------------------------------
//-------------------------------------------------------------------------
public:
    Whisper_Params whisper_params;//whisper.exe可以传入的参数
    int max_thread;
signals:
    void whisper_kill();
    void expend2ui_voicedecode_over(QString result);
    void expend2ui_whisper_modelpath(QString modelpath);
public slots:
    void recv_voicedecode(QString wavpath);//开始语音转文字
    void whisper_onProcessStarted();
    void whisper_onProcessFinished();
private slots:    
    void on_voice_load_modelpath_button_clicked();//用户点击选择whisper路径时响应

};

#endif // EXPEND_H
