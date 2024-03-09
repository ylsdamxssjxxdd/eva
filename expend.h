#ifndef EXPEND_H
#define EXPEND_H

#include <QWidget>
#include <QJsonObject>
#include <QScrollBar>
#include <QTimer>
#include <QDebug>
#include <QFileDialog>
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
private slots:
    void on_tabWidget_tabBarClicked(int index);//用户切换选项卡时响应    
private:
    Ui::Expend *ui;
//-------------------------------------------------------------------------
//----------------------------------语音相关--------------------------------
//-------------------------------------------------------------------------
public:
    QString whisper_model_path="";
signals:
    void expend2whisper_modelpath(QString whisper_model_path_);
    void expend2whisper_load();
public slots:    
    void recv_expend_show(bool is_show);//通知显示扩展窗口
private slots:    
    void on_voice_load_modelpath_button_clicked();//用户点击选择whisper路径时响应
    void on_voice_load_load_button_clicked();//用户点击加载whisper模型时响应

};

#endif // EXPEND_H
