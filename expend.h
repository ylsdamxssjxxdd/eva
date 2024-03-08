#ifndef EXPEND_H
#define EXPEND_H

#include <QWidget>
#include <QJsonObject>
#include <QScrollBar>
#include <QTimer>
#include <QDebug>
namespace Ui {
class Expend;
}

class Expend : public QWidget
{
    Q_OBJECT

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
public slots:
    void recv_log(QString log);
    void recv_vocab(QString vocab);
    void recv_expend_show(bool is_show);//通知显示扩展窗口

private slots:
    void on_tabWidget_tabBarClicked(int index);//用户切换选项卡时响应

private:
    Ui::Expend *ui;
};

#endif // EXPEND_H
