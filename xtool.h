#ifndef XTOOL_H
#define XTOOL_H
#include <QThread>
#include <QDebug>
#include <QScriptEngine>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QElapsedTimer>
#include <QProcess>

class xTool : public QThread
{
    Q_OBJECT
public:
    xTool();
    ~xTool();
    //拯救中文
    QJsonObject wordsObj;
    void getWords();
    bool warmup=true;//需要读取中文
    void run() override;
public:
    QStringList func_arg_list;

public slots:
    void recv_func_arg(QStringList func_arg_list);

signals:
    void tool2ui_pushover(QString tool_result);
    void tool2ui_state(const QString &state,int state_num=0);//发送的状态信号

};

#endif // XTOOL_H
