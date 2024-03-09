#ifndef XWHISPER_H
#define XWHISPER_H

#include <QThread>
#include <QDebug>
#include <QFile>

#include "common.h"
#include "whisper.cpp/whisper.h"

#include "xconfig.h" //ui和bot都要导入的共有配置

class xWhisper : public QThread
{
    Q_OBJECT
public:
    xWhisper();
    ~xWhisper();

    void run() override;
public:
    whisper_context * ctx;

    QString model_path;

public slots:
    void recv_modelpath(QString model_path_);

signals:

};

#endif // XWHISPER_H
