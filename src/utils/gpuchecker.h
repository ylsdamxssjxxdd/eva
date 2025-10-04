// gpuChecker
// 用于查询gpu的显存/显存利用率/gpu利用率/剩余显存
// 使用时可直接包含此头文件，windows平台依赖nvml.h和nvml.lib
// 需要连接对应的信号槽
// 依赖qt5

#ifndef GPUCHECKER_H
#define GPUCHECKER_H

#include <QDebug>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QThread>

#if defined(BODY_USE_CUDA) && !defined(__linux__)
#include "nvml.h"
#endif

// 进程执行辅助：带超时、失败安全
static inline bool runProcessReadAll(const QString &program,
                                     const QStringList &args,
                                     QString &stdOut,
                                     int timeoutMs = 500)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start(program, args);
    if (!p.waitForStarted(200)) return false;
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(200);
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit) return false;
    stdOut = QString::fromUtf8(p.readAllStandardOutput());
    return p.exitCode() == 0 && !stdOut.trimmed().isEmpty();
}

class GpuInfoProvider : public QObject
{
    Q_OBJECT
  public:
    virtual void getGpuInfo() = 0;

  signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree);
};

class NvidiaGpuInfoProvider : public GpuInfoProvider
{
    Q_OBJECT
  public:
    void getGpuInfo() override
    {
        QString output;
        const QString program = "nvidia-smi";
        const QStringList args = {
            "--query-gpu=memory.total,memory.free,memory.used,utilization.gpu",
            "--format=csv,noheader,nounits"
        };
        if (!runProcessReadAll(program, args, output))
        {
            emit gpu_status(0, 0, 0, 0);
            return;
        }

        const QStringList gpuInfoList = output.split('\n', Qt::SkipEmptyParts);
        if (gpuInfoList.isEmpty()) { emit gpu_status(0, 0, 0, 0); return; }

        // 仅取第一个 GPU（UI 若需多卡，可扩展为列表信号）
        const QString &info = gpuInfoList.first();
        const QStringList values = info.split(',', Qt::SkipEmptyParts);
        if (values.size() != 4) { emit gpu_status(0, 0, 0, 0); return; }

        bool ok0=false, ok1=false, ok2=false, ok3=false;
        const int totalMemory = values[0].trimmed().toInt(&ok0);
        const int freeMemory  = values[1].trimmed().toInt(&ok1);
        const int usedMemory  = values[2].trimmed().toInt(&ok2);
        const int utilization = values[3].trimmed().toInt(&ok3);
        if (!(ok0 && ok1 && ok2 && ok3) || totalMemory <= 0) { emit gpu_status(0, 0, 0, 0); return; }

        const float vmem  = float(totalMemory);
        const float vram  = float(usedMemory) / float(totalMemory) * 100.0f;
        const float vcore = float(utilization);
        const float vfree = float(freeMemory);
        emit gpu_status(vmem, vram, vcore, vfree);
    }
};

class AmdGpuInfoProvider : public GpuInfoProvider
{
    Q_OBJECT
  public:
    void getGpuInfo() override
    {
        QString output;
        const QString program = "rocm-smi";
        const QStringList args = { "--showmeminfo", "vram", "--showuse", "--csv" };
        if (!runProcessReadAll(program, args, output))
        {
            emit gpu_status(0, 0, 0, 0);
            return;
        }

        // rocm-smi CSV 每行类似：GPU, <total>, <used>, <free>, <utilization(%)>
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
        {
            if (!line.contains("GPU", Qt::CaseInsensitive)) continue;
            const QStringList cols = line.split(',', Qt::SkipEmptyParts);
            if (cols.size() < 5) continue;
            bool ok0=false, ok1=false, ok2=false, ok3=false;
            const int totalMemory = cols[1].trimmed().toInt(&ok0);
            const int usedMemory  = cols[2].trimmed().toInt(&ok1);
            const int freeMemory  = cols[3].trimmed().toInt(&ok2);
            const int utilization = cols[4].trimmed().toInt(&ok3);
            if (!(ok0 && ok1 && ok2 && ok3) || totalMemory <= 0) continue;

            const float vmem  = float(totalMemory);
            const float vram  = float(usedMemory) / float(totalMemory) * 100.0f;
            const float vcore = float(utilization);
            const float vfree = float(freeMemory);
            emit gpu_status(vmem, vram, vcore, vfree);
            return;
        }
        emit gpu_status(0, 0, 0, 0);
    }
};

class gpuChecker : public QObject
{
    Q_OBJECT

  public:
    gpuChecker()
    {
#if defined(BODY_USE_CUDA) && !defined(__linux__)
        if (nvmlInit() == NVML_SUCCESS)
        {
            nvmlDeviceGetHandleByIndex(0, &device);
            nvmlReady = true;
        }
        else
        {
            nvmlReady = false;
        }
#elif BODY_USE_32BIT
        gpuInfoProvider = nullptr;
#else
        const QString gpuVendor = getGpuVendor();
        if (gpuVendor == "NVIDIA")
        {
            gpuInfoProvider = new NvidiaGpuInfoProvider();
        }
        else if (gpuVendor == "AMD")
        {
            gpuInfoProvider = new AmdGpuInfoProvider();
        }
        else
        {
            gpuInfoProvider = nullptr;
        }
#endif
        if (gpuInfoProvider)
        {
            connect(gpuInfoProvider, &GpuInfoProvider::gpu_status, this, &gpuChecker::gpu_status);
        }
    }

    ~gpuChecker()
    {
#if defined(BODY_USE_CUDA) && !defined(__linux__)
        if (nvmlReady) nvmlShutdown();
#endif
        if (gpuInfoProvider)
        {
            delete gpuInfoProvider;
        }
    }

    void checkGpu()
    {
#if defined(BODY_USE_CUDA) && !defined(__linux__)
        if (nvmlReady)
        {
            nvmlUtilization_t utilization{};
            nvmlMemory_t memory{};
            if (nvmlDeviceGetUtilizationRates(device, &utilization) == NVML_SUCCESS &&
                nvmlDeviceGetMemoryInfo(device, &memory) == NVML_SUCCESS)
            {
                const float vmem = memory.total / 1024.0f / 1024.0f; // MB
                const float vram = memory.total ? (float(memory.used) / float(memory.total) * 100.0f) : 0.0f;
                const float vcore = float(utilization.gpu);
                const float vfree = float(memory.free) / 1024.0f / 1024.0f; // MB
                emit gpu_status(vmem, vram, vcore, vfree);
                return;
            }
            // NVML 失败，降级为 0
            emit gpu_status(0, 0, 0, 0);
            return;
        }
        // 未就绪时走通用分支
#endif
        if (gpuInfoProvider)
        {
            gpuInfoProvider->getGpuInfo();
        }
        else
        {
            emit gpu_status(0, 0, 0, 0);
        }
    }

  private:
    QString getGpuVendor()
    {
        QString output;
#ifdef _WIN32
        // 优先使用 PowerShell CIM，兼容新系统；失败再回退 wmic
        if (!runProcessReadAll("powershell", {"-NoProfile", "-Command", "Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name"}, output))
        {
            runProcessReadAll("wmic", {"path", "win32_videocontroller", "get", "caption"}, output);
        }
        if (output.trimmed().isEmpty())
        {
            // 再尝试 nvidia-smi / rocm-smi 来粗略判断
            QString tmp;
            if (runProcessReadAll("nvidia-smi", {"-L"}, tmp)) output = tmp;
            else if (runProcessReadAll("rocm-smi", {"--showuse"}, tmp)) output = tmp;
        }
#elif __linux__
        if (!runProcessReadAll("bash", {"-lc", "lspci -nn | grep -i 'VGA'"}, output)) output.clear();
#endif
        if (output.contains("NVIDIA", Qt::CaseInsensitive)) return "NVIDIA";
        if (output.contains("AMD", Qt::CaseInsensitive) || output.contains("Radeon", Qt::CaseInsensitive)) return "AMD";
        if (output.contains("Intel", Qt::CaseInsensitive)) return "Intel";
        return "Unknown";
    }

#if defined(BODY_USE_CUDA) && !defined(__linux__)
    nvmlDevice_t device{};
    bool nvmlReady = false;
#endif
    GpuInfoProvider *gpuInfoProvider = nullptr;

  signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree);
};

#endif // GPUCHECKER_H


