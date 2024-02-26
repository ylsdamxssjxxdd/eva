#include "gpuChecker.h"

gpuChecker::gpuChecker()
{
    nvmlInit();// 初始化NVML库
    nvmlDeviceGetHandleByIndex(0, &device);// 获取第一个GPU的句柄
    status_pTimer = new QTimer(this);//启动后,达到规定时间将发射终止信号
    connect(status_pTimer, SIGNAL(timeout()), this, SLOT(encode_handleTimeout()));//设置终止信号触发的槽函数
    status_pTimer->start(300);
}


gpuChecker::~gpuChecker()
{
    ;
}

void gpuChecker::encode_handleTimeout()
{
    nvmlDeviceGetUtilizationRates(device, &utilization);// 获取GPU利用率
    nvmlDeviceGetMemoryInfo(device, &memory);
    float vmem = memory.total / 1024 / 1024;//总显存MB
    float vram = float(memory.used) / float(memory.total) * 100.0;//gpu显存占用率
    float vcore = utilization.gpu;//gpu核心利用率
    float vfree_ = float(memory.free)/ 1024.0 / 1024.0;//剩余显存MB
    emit gpu_status(vmem,vram, vcore,vfree_);
}


void gpuChecker::run()
{
    ;
}

