#ifndef XBERT_H
#define XBERT_H
#include <QThread>
#include <QDebug>
#include <QFile>

#include "xconfig.h" //ui和bot都要导入的共有配置

class xBert : public QThread
{
    Q_OBJECT
public:
    xBert();
    ~xBert();

    void run() override;
public:

public slots:


signals:

};

#endif // XBERT_H
