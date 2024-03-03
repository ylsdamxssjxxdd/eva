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
#include <QTime>
#include <QTimer>
#include "xconfig.h"
class xTool : public QThread
{
    Q_OBJECT
public:
    xTool();
    ~xTool();
    //拯救中文
    QJsonObject wordsObj;
    void getWords(QString json_file_path);
    bool warmup=true;//需要读取中文
    void run() override;
public:
    QStringList func_arg_list;
    QTimer *positron_p;//阳电子步枪充能定时器
    void positronShoot();//阳电子步枪发射
    int positron_power=0;//阳电子步枪充能值
public slots:
    void recv_func_arg(QStringList func_arg_list);
    void positronPower();//阳电子步枪充能
    
signals:
    void tool2ui_pushover(QString tool_result);
    void tool2ui_state(const QString &state_string, STATE state=USUAL_);//发送的状态信号
    void positron_starter();

};

#endif // XTOOL_H
