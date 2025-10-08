// gpuChecker
// 用于查询gpu的显存/显存利用率/gpu利用率/剩余显存
// 跨平台实现：优先使用 nvidia-smi/rocm-smi；Windows 不再依赖 NVML；不可用时返回 0
// 需要连接对应的信号槽
// 依赖qt5

#ifndef GPUCHECKER_H
#define GPUCHECKER_H

#include <QDebug>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QThread>
#include <QStandardPaths>
#include <QFileInfo>


// 进程执行辅助：带超时、失败安全
// Try to robustly start an external process and capture its standard output.
// On 32-bit Windows, calling 64-bit tools in System32 may be affected by WOW64 redirection.
// We attempt to resolve the absolute path (including Sysnative / ProgramW6432 fallbacks) and
// allow a slightly longer start/finish window.
static inline QString resolveProgramPath(const QString &program)
{
#ifdef _WIN32
    // If already absolute and exists, return as-is
    QFileInfo fi(program);
    if (fi.isAbsolute() && fi.exists()) return fi.absoluteFilePath();

    auto ensureExe = [](QString name) {
        return name.endsWith(".exe", Qt::CaseInsensitive) ? name : (name + ".exe");
    };

    // First, search PATH
    QString found = QStandardPaths::findExecutable(program);
    if (found.isEmpty()) found = QStandardPaths::findExecutable(ensureExe(program));
    if (!found.isEmpty()) return found;

    // Special handling for NVIDIA CLI which is commonly in these locations
    const QString baseWin = qEnvironmentVariable("SystemRoot", "C:/Windows");
    const QString sysnative = baseWin + "/Sysnative/" + ensureExe(program);
    const QString system32 = baseWin + "/System32/" + ensureExe(program);
    const QString pf64 = qEnvironmentVariable("ProgramW6432"); // 64-bit Program Files from 32-bit process
    const QString nvsmip = pf64.isEmpty() ? QString() : (pf64 + "/NVIDIA Corporation/NVSMI/" + ensureExe(program));

    for (const QString &cand : {sysnative, system32, nvsmip})
    {
        if (!cand.isEmpty() && QFileInfo::exists(cand)) return cand;
    }
#endif
    return program; // Fallback; QProcess will try PATH resolution
}

static inline bool runProcessReadAll(const QString &program,
                                     const QStringList &args,
                                     QString &stdOut,
                                     int timeoutMs = 3000)
{
    QString prog = resolveProgramPath(program);
    QProcess p;
    p.setProcessChannelMode(QProcess::SeparateChannels);
    // qDebug() << "Running process:" << prog << args;
    p.start(prog, args);

    if (!p.waitForStarted(1200))
    {
#ifdef _WIN32
        // One more attempt: if we resolved to System32, try Sysnative explicitly
        const QString baseWin = qEnvironmentVariable("SystemRoot", "C:/Windows");
        const QString fname = QFileInfo(prog).fileName();
        const QString sysnative = baseWin + "/Sysnative/" + (fname.endsWith(".exe", Qt::CaseInsensitive) ? fname : fname + ".exe");
        if (QFileInfo::exists(sysnative))
        {
            // qDebug() << "Retrying via Sysnative:" << sysnative;
            p.start(sysnative, args);
            if (!p.waitForStarted(1200)) return false;
        }
        else
#endif
        {
            return false;
        }
    }

    if (!p.waitForFinished(timeoutMs))
    {
        p.kill();
        p.waitForFinished(200);
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit)
    {
        // qDebug() << "Process abnormal exit, code=" << p.exitCode() << ", err=" << QString::fromUtf8(p.readAllStandardError());
        return false;
    }
    stdOut = QString::fromUtf8(p.readAllStandardOutput());
    if (stdOut.trimmed().isEmpty())
    {
        // If nothing in stdout, include stderr for diagnostics
        const QString err = QString::fromUtf8(p.readAllStandardError());
        if (!err.isEmpty()) qDebug() << "stderr:" << err;
    }
    return p.exitCode() == 0 && !stdOut.trimmed().isEmpty();
}class GpuInfoProvider : public QObject
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
        // qDebug() << "Getting NVIDIA GPU info...";
        QString output;
        const QString program = "nvidia-smi";
        const QStringList args = {
            "--query-gpu=memory.total,memory.free,memory.used,utilization.gpu",
            "--format=csv,noheader,nounits"};
        if (!runProcessReadAll(program, args, output))
        {
            emit gpu_status(0, 0, 0, 0);
            return;
        }
// qDebug() << "nvidia-smi output:" << output;
        const QStringList gpuInfoList = output.split('\n', Qt::SkipEmptyParts);
        if (gpuInfoList.isEmpty())
        {
            emit gpu_status(0, 0, 0, 0);
            return;
        }
// qDebug() << "Parsed GPU info lines:" << gpuInfoList;
        // 仅取第一个 GPU（UI 若需多卡，可扩展为列表信号）
        const QString &info = gpuInfoList.first();
        const QStringList values = info.split(',', Qt::SkipEmptyParts);
        if (values.size() != 4)
        {
            emit gpu_status(0, 0, 0, 0);
            return;
        }
// qDebug() << "Parsed GPU info values:" << values;
        bool ok0 = false, ok1 = false, ok2 = false, ok3 = false;
        const int totalMemory = values[0].trimmed().toInt(&ok0);
        const int freeMemory = values[1].trimmed().toInt(&ok1);
        const int usedMemory = values[2].trimmed().toInt(&ok2);
        const int utilization = values[3].trimmed().toInt(&ok3);
        if (!(ok0 && ok1 && ok2 && ok3) || totalMemory <= 0)
        {
            emit gpu_status(0, 0, 0, 0);
            return;
        }
// qDebug() << "Parsed GPU info integers:"<< totalMemory << freeMemory << usedMemory << utilization;
        const float vmem = float(totalMemory);
        const float vram = float(usedMemory) / float(totalMemory) * 100.0f;
        const float vcore = float(utilization);
        const float vfree = float(freeMemory);
        emit gpu_status(vmem, vram, vcore, vfree);
        // qDebug() << "GPU Info:" << vmem << "MB total," << vram << "% used," << vcore << "% core," << vfree << "MB free";
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
        const QStringList args = {"--showmeminfo", "vram", "--showuse", "--csv"};
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
            bool ok0 = false, ok1 = false, ok2 = false, ok3 = false;
            const int totalMemory = cols[1].trimmed().toInt(&ok0);
            const int usedMemory = cols[2].trimmed().toInt(&ok1);
            const int freeMemory = cols[3].trimmed().toInt(&ok2);
            const int utilization = cols[4].trimmed().toInt(&ok3);
            if (!(ok0 && ok1 && ok2 && ok3) || totalMemory <= 0) continue;

            const float vmem = float(totalMemory);
            const float vram = float(usedMemory) / float(totalMemory) * 100.0f;
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
        const QString gpuVendor = getGpuVendor();
        // qDebug()<<"Detected GPU Vendor:"<<gpuVendor;
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
        if (gpuInfoProvider)
        {
            connect(gpuInfoProvider, &GpuInfoProvider::gpu_status, this, &gpuChecker::gpu_status);
        }
    }

    ~gpuChecker()
    {
        if (gpuInfoProvider)
        {
            delete gpuInfoProvider;
        }
    }

    void checkGpu()
    {
        // qDebug() << "Checking GPU status...";
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
            if (runProcessReadAll("nvidia-smi", {"-L"}, tmp))
                output = tmp;
            else if (runProcessReadAll("rocm-smi", {"--showuse"}, tmp))
                output = tmp;
        }
#elif __linux__
        if (!runProcessReadAll("bash", {"-lc", "lspci -nn | grep -i 'VGA'"}, output)) output.clear();
#endif
        if (output.contains("NVIDIA", Qt::CaseInsensitive)) return "NVIDIA";
        if (output.contains("AMD", Qt::CaseInsensitive) || output.contains("Radeon", Qt::CaseInsensitive)) return "AMD";
        if (output.contains("Intel", Qt::CaseInsensitive)) return "Intel";
        return "Unknown";
    }

    GpuInfoProvider *gpuInfoProvider = nullptr;

  signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree);
};

#endif // GPUCHECKER_H



