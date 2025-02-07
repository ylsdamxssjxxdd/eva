// gpuChecker
// 用于查询gpu的显存/显存利用率/gpu利用率/剩余显存
// 使用时可直接包含此头文件，windows平台依赖nvml.h和nvml.lib
// 需要连接对应的信号槽
// 依赖qt5

#ifndef GPUCHECKER_H
#define GPUCHECKER_H

#include <QObject>
#include <QDebug>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>

#ifdef BODY_USE_CUDA
#include "nvml.h"
#endif





class GpuInfoProvider : public QObject {
    Q_OBJECT
public:
    virtual void getGpuInfo() = 0;

signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree);
};

class NvidiaGpuInfoProvider : public GpuInfoProvider {
    Q_OBJECT
public:
    void getGpuInfo() override {
        QProcess process;
        process.start("nvidia-smi --query-gpu=memory.total,memory.free,memory.used,utilization.gpu --format=csv,noheader,nounits");
        process.waitForFinished();

        QString output = process.readAllStandardOutput();
        QStringList gpuInfoList = output.split('\n', QString::SkipEmptyParts);

        if (!gpuInfoList.isEmpty()) {
            for (const QString& info : gpuInfoList) {
                QStringList values = info.split(',', QString::SkipEmptyParts);
                if (values.size() == 4) {
                    int totalMemory = values[0].trimmed().toInt();
                    int freeMemory = values[1].trimmed().toInt();
                    int usedMemory = values[2].trimmed().toInt();
                    int utilization = values[3].trimmed().toInt();

                    emit gpu_status(totalMemory, float(usedMemory) / float(totalMemory) * 100.0, utilization, freeMemory);
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
};

class AmdGpuInfoProvider : public GpuInfoProvider {
    Q_OBJECT
public:
    void getGpuInfo() override {
        QProcess process;
        process.start("rocm-smi --showmeminfo vram --showuse --csv");
        process.waitForFinished();

        QString output = process.readAllStandardOutput();
        QStringList gpuInfoList = output.split('\n', QString::SkipEmptyParts);

        bool foundMemoryInfo = false;
        for (const QString& line : gpuInfoList) {
            if (line.contains("GPU", Qt::CaseInsensitive)) {
                QStringList values = line.split(',', QString::SkipEmptyParts);
                if (values.size() >= 5) {
                    int totalMemory = values[1].trimmed().toInt();
                    int usedMemory = values[2].trimmed().toInt();
                    int freeMemory = values[3].trimmed().toInt();
                    int utilization = values[4].trimmed().toInt();

                    emit gpu_status(totalMemory, float(usedMemory) / float(totalMemory) * 100.0, utilization, freeMemory);
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
};

class gpuChecker : public QObject {
    Q_OBJECT

public:
    gpuChecker() {
#ifdef BODY_USE_CUDA
        nvmlInit();
        nvmlDeviceGetHandleByIndex(0, &device);
#else
        QString gpuVendor = getGpuVendor();
        if (gpuVendor == "NVIDIA") {
            gpuInfoProvider = new NvidiaGpuInfoProvider();
        } else if (gpuVendor == "AMD") {
            gpuInfoProvider = new AmdGpuInfoProvider();
        } else {
            gpuInfoProvider = nullptr;
        }
#endif
        if (gpuInfoProvider) {
            connect(gpuInfoProvider, &GpuInfoProvider::gpu_status, this, &gpuChecker::gpu_status);
        }
    }

    ~gpuChecker() {
#ifdef BODY_USE_CUDA
        nvmlShutdown();
#endif
        if (gpuInfoProvider) {
            delete gpuInfoProvider;
        }
    }

    void checkGpu() {
#ifdef BODY_USE_CUDA
        nvmlUtilization_t utilization;
        nvmlMemory_t memory;
        nvmlDeviceGetUtilizationRates(device, &utilization);
        nvmlDeviceGetMemoryInfo(device, &memory);

        float vmem = memory.total / 1024 / 1024;
        float vram = float(memory.used) / float(memory.total) * 100.0;
        float vcore = utilization.gpu;
        float vfree = float(memory.free) / 1024.0 / 1024.0;

        emit gpu_status(vmem, vram, vcore, vfree);
#else
        if (gpuInfoProvider) {
            gpuInfoProvider->getGpuInfo();
        } else {
            emit gpu_status(0, 0, 0, 0);
        }
#endif
    }

private:
    QString getGpuVendor() {
        QProcess process;
#ifdef _WIN32
        process.start("wmic path win32_videocontroller get caption");
#elif __linux__
        process.start("bash", QStringList() << "-c" << "lspci -nn | grep VGA");
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

#ifdef BODY_USE_CUDA
    nvmlDevice_t device;
#endif
    GpuInfoProvider* gpuInfoProvider = nullptr;

signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree);
};

#endif