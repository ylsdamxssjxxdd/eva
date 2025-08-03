#ifndef XNET_H
#define XNET_H
#include <QColor>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QThread>

#include "xconfig.h"  //ui和bot都要导入的共有配置

class xNet : public QObject {
    Q_OBJECT
   public:
    xNet();
    ~xNet();

    void run();

   public:
    APIS apis;
    ENDPOINT_DATA endpoint_data;  //端点参数
    QJsonObject wordsObj;
    int language_flag = 0;
    QString jtr(QString customstr);   // 根据language.json(wordsObj)和language_flag中找到对应的文字
    QByteArray createChatBody();      //构造请求的数据体,对话模式
    QByteArray createCompleteBody();  //构造请求的数据体,补完模式
    bool is_stop = false;             //终止标签
    bool thinkFlag = false; // 是否正在思考
    QString current_content;//模型最近的输出

    QString extractContentFromJson(const QString &jsonString);
    QStringList extractAllContent(const QString &data);
   public slots:
    void recv_data(ENDPOINT_DATA data);  //传递端点参数
    void recv_apis(APIS apis_);          //传递api设置参数
    void recv_stop(bool stop);           //传递停止信号
    void recv_language(int language_flag_);

   signals:
    void net2ui_state(const QString &state_string, SIGNAL_STATE state = USUAL_SIGNAL);             //发送的状态信号
    void net2ui_output(const QString &result, bool is_while = 1, QColor color = QColor(0, 0, 0));  //发送的输出信号,is_while表示从流式输出的token
    void net2ui_pushover();                                                                        //推理完成的信号
};

#endif  // XNET_H


