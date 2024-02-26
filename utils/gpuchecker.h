#ifndef GPUCHECKER_H
#define GPUCHECKER_H
#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QElapsedTimer>
#include <QTimer>
#include "nvml.h"

//llama模型类
class gpuChecker : public QThread
{
    Q_OBJECT
public:
    gpuChecker();
    ~gpuChecker();
    void run() override;//运行推理,多线程的实现方式
    QTimer *status_pTimer;
    nvmlDevice_t device;//显卡设备
    nvmlUtilization_t utilization;//利用率
    nvmlMemory_t memory;
signals:
    void gpu_status(float vmem,float vram, float vcore, float vfree_);
public slots:
    void encode_handleTimeout();
};

#endif // GPUCHECKER_H
