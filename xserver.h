#ifndef XSERVER_H
#define XSERVER_H
#include <QThread>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>
#include <QEventLoop>
#include <QFile>

#include "xconfig.h" //ui和bot都要导入的共有配置

class xServer : public QThread
{
    Q_OBJECT
public:
    xServer();
    ~xServer();

    void run() override;
public:

public slots:


signals:

};

#endif // XSERVER_H
