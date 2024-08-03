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

#ifdef BODY_USE_CUDA
#include "nvml.h"
#endif
//llama模型类
class gpuChecker : public QThread
{
    Q_OBJECT
public:
    //获取gpu厂家
    QString getGpuVendor()
    {
        QProcess process;
#ifdef _WIN32
        process.start("wmic path win32_videocontroller get caption");
#elif __linux__
        process.start("lspci -nn | grep VGA");
#endif
        process.waitForFinished();
        QString output = process.readAllStandardOutput();

        if (output.contains("NVIDIA", Qt::CaseInsensitive)) {
            return "NVIDIA";
        } else if (output.contains("AMD", Qt::CaseInsensitive) || output.contains("Radeon", Qt::CaseInsensitive)) {
            return "AMD";
        } else if (output.contains("Intel", Qt::CaseInsensitive)) {
            return "Intel";
        } else {
            return "Unknown";
        }
    }

    void getNvidiaGpuInfo() {
        QProcess process;
        // Use nvidia-smi to get memory and utilization info
        process.start("nvidia-smi --query-gpu=memory.total,memory.free,memory.used,utilization.gpu --format=csv,noheader,nounits");
        process.waitForFinished();

        QString output = process.readAllStandardOutput();
        QStringList gpuInfoList = output.split('\n', QString::SkipEmptyParts);

        if (!gpuInfoList.isEmpty()) {
            for (int i = 0; i < gpuInfoList.size(); ++i) {
                QStringList values = gpuInfoList[i].split(',', QString::SkipEmptyParts);
                if (values.size() == 4) {
                    int totalMemory = values[0].trimmed().toInt();
                    int freeMemory = values[1].trimmed().toInt();
                    int usedMemory = values[2].trimmed().toInt();
                    int utilization = values[3].trimmed().toInt();

                    emit gpu_status(totalMemory, float(usedMemory) / float(totalMemory) * 100.0, utilization,freeMemory);
                    // qDebug() << "NVIDIA GPU" << i
                    //         << "Total Memory:" << totalMemory << "MB"
                    //         << "Free Memory:" << freeMemory << "MB"
                    //         << "Used Memory:" << usedMemory << "MB"
                    //         << "Utilization:" << utilization << "%";
                } else {
                    qDebug() << "Failed to parse NVIDIA GPU information.";
                    emit gpu_status(0, 0, 0, 0);
                }
            }
        } else {
            qDebug() << "Failed to retrieve NVIDIA GPU information.";
            emit gpu_status(0, 0, 0, 0);
        }
    }

    void getAmdGpuInfo() {
        QProcess process;
        // Use rocm-smi to get memory and utilization info
        process.start("rocm-smi --showmeminfo vram --showuse --csv");
        process.waitForFinished();

        QString output = process.readAllStandardOutput();
        QStringList gpuInfoList = output.split('\n', QString::SkipEmptyParts);

        bool foundMemoryInfo = false;
        for (const QString &line : gpuInfoList) {
            if (line.contains("GPU", Qt::CaseInsensitive)) {
                QStringList values = line.split(',', QString::SkipEmptyParts);
                if (values.size() >= 5) {
                    QString gpuName = values[0].trimmed();
                    int totalMemory = values[1].trimmed().toInt();
                    int usedMemory = values[2].trimmed().toInt();
                    int freeMemory = values[3].trimmed().toInt();
                    int utilization = values[4].trimmed().toInt();

                    emit gpu_status(totalMemory, float(usedMemory) / float(totalMemory) * 100.0, utilization,freeMemory);
                    // qDebug() << "AMD" << gpuName
                    //         << "Total Memory:" << totalMemory << "MB"
                    //         << "Free Memory:" << freeMemory << "MB"
                    //         << "Used Memory:" << usedMemory << "MB"
                    //         << "Utilization:" << utilization << "%";
                    foundMemoryInfo = true;
                } else {
                    qDebug() << "Failed to parse AMD GPU information.";
                    emit gpu_status(0, 0, 0, 0);
                }
            }
        }

        if (!foundMemoryInfo) {
            qDebug() << "Failed to retrieve AMD GPU information.";
            emit gpu_status(0, 0, 0, 0);
        }
    }
#ifdef BODY_USE_CUDA
    nvmlDevice_t device;//显卡设备
    nvmlUtilization_t utilization;//利用率
    nvmlMemory_t memory;
#endif

    QString gpu_vendor;// 显卡种类 nvidia/amd/inter/asend

    // 初始化
    gpuChecker()
    {
#ifdef BODY_USE_CUDA        
        nvmlInit();// 初始化NVML库
        nvmlDeviceGetHandleByIndex(0, &device);// 获取第一个GPU的句柄
#else
        // 判断显卡类型
        gpu_vendor = getGpuVendor();
        qDebug()<<gpu_vendor;
#endif
    }

    // 释放
    ~gpuChecker()
    {
#ifdef BODY_USE_CUDA    
        nvmlShutdown(); // 关闭NVML库
#endif
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
#ifdef BODY_USE_CUDA   
        nvmlDeviceGetUtilizationRates(device, &utilization);// 获取GPU利用率
        nvmlDeviceGetMemoryInfo(device, &memory);
        float vmem = memory.total / 1024 / 1024;//总显存MB
        float vram = float(memory.used) / float(memory.total) * 100.0;//gpu显存占用率
        float vcore = utilization.gpu;//gpu核心利用率
        float vfree_ = float(memory.free)/ 1024.0 / 1024.0;//剩余显存MB
        emit gpu_status(vmem, vram, vcore,vfree_);
#else
        if(gpu_vendor == "NVIDIA")
        {
            getNvidiaGpuInfo();
        }
        else if(gpu_vendor == "AMD")
        {
            getAmdGpuInfo();
        }
        else
        {
            emit gpu_status(0, 0, 0, 0);
        }
#endif
    }

    void recv_gpu_reflash()
    {
        chekGpu();
    }
};

#endif // GPUCHECKER_H