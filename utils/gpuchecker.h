// gpuChecker
// 用于查询gpu的显存/显存利用率/gpu利用率/剩余显存
// 使用时可直接包含此头文件，windows平台依赖nvml.h和nvml.lib
// 需要连接对应的信号槽
// 依赖qt5

#ifndef GPUCHECKER_H
#define GPUCHECKER_H

#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QElapsedTimer>
#include "nvml.h"

//llama模型类
class gpuChecker : public QThread
{
    Q_OBJECT
public:
    nvmlDevice_t device;//显卡设备
    nvmlUtilization_t utilization;//利用率
    nvmlMemory_t memory;

    // 初始化
    gpuChecker()
    {
        nvmlInit();// 初始化NVML库
        nvmlDeviceGetHandleByIndex(0, &device);// 获取第一个GPU的句柄
    }

    ~gpuChecker()
    {
        nvmlShutdown(); // 关闭NVML库
    }

    // 多线程支持
    void run() override
    {

        chekGpu();

    } 

signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree_);

public slots:
    void chekGpu()
    {
        nvmlDeviceGetUtilizationRates(device, &utilization);// 获取GPU利用率
        nvmlDeviceGetMemoryInfo(device, &memory);
        float vmem = memory.total / 1024 / 1024;//总显存MB
        float vram = float(memory.used) / float(memory.total) * 100.0;//gpu显存占用率
        float vcore = utilization.gpu;//gpu核心利用率
        float vfree_ = float(memory.free)/ 1024.0 / 1024.0;//剩余显存MB
        emit gpu_status(vmem, vram, vcore,vfree_);
    }
    void recv_gpu_reflash()
    {
        chekGpu();
    }
};

#endif // GPUCHECKER_H