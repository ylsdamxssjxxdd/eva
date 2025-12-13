// gpuChecker
// 用于查询gpu的显存/显存利用率/gpu利用率/剩余显存
// 跨平台实现：
// - NVIDIA：优先使用 nvidia-smi
// - AMD：Linux 优先使用 rocm-smi；Windows 使用内置 Get-GpuStatus.ps1（释放到 EVA_TEMP 并允许生成缓存）
// 不可用时返回 0
// 需要连接对应的信号槽
// 依赖qt5

#ifndef GPUCHECKER_H
#define GPUCHECKER_H

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

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

    auto ensureExe = [](QString name)
    {
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
}

// -------------------------------
// Windows AMD 显存查询脚本支持
// -------------------------------
// 需求背景：
// - UI 每 500ms 会请求一次 GPU 状态（见 Widget::updateGpuStatus）。
// - Get-GpuStatus.ps1 首次运行会生成 Get-GpuStatus.cache.json（同目录），耗时约 10s；后续约 3s。
// - 因此必须：
//   1) 把脚本打包到资源文件，按需释放到 EVA_TEMP
//   2) C++ 侧做节流（最小刷新间隔）
//   3) 任何一次脚本运行报错后永久熔断，避免反复失败拖累其它功能
#ifdef _WIN32
static inline QString psEscapeSingleQuoted(const QString &s)
{
    // PowerShell 单引号字符串内，单引号需要写成两个单引号
    QString out = s;
    out.replace('\'', "''");
    return out;
}

static inline QString evaTempRootPath()
{
    // 与主程序保持一致：EVA_TEMP 放在可执行程序同级目录
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("EVA_TEMP"));
}

static inline QString normalizeWinPathForCompare(const QString &path)
{
    // Windows 路径大小写不敏感：统一成 '/' + 小写，便于比较
    return QDir::fromNativeSeparators(path).trimmed().toLower();
}

static inline QString resolveNativePowerShellExe()
{
    // 关键点：当前机体可能是 32 位进程（例如 MinGW i686 静态构建），
    // 若直接启动 "powershell" 或 System32 下的 powershell.exe，会被 WOW64 重定向到 32 位版本。
    // 32 位 PowerShell 进一步调用 dxdiag 时，可能得到被截断的显存（常见为 4GB 级别），导致缓存错误。
    //
    // 解决：优先使用 Sysnative 访问 64 位 powershell.exe（仅 32 位进程可见），从而使用 64 位 dxdiag 生成正确缓存。
    const QString baseWin = qEnvironmentVariable("SystemRoot", "C:/Windows");
    const QString sysnative = baseWin + "/Sysnative/WindowsPowerShell/v1.0/powershell.exe";
    const QString system32 = baseWin + "/System32/WindowsPowerShell/v1.0/powershell.exe";
    const QString syswow64 = baseWin + "/SysWOW64/WindowsPowerShell/v1.0/powershell.exe";

    if (QFileInfo::exists(sysnative)) return sysnative;
    if (QFileInfo::exists(system32)) return system32;
    if (QFileInfo::exists(syswow64)) return syswow64;

    QString found = QStandardPaths::findExecutable(QStringLiteral("powershell"));
    if (!found.isEmpty()) return found;
    found = QStandardPaths::findExecutable(QStringLiteral("powershell.exe"));
    if (!found.isEmpty()) return found;
    return QStringLiteral("powershell");
}

static inline QString ensureGetGpuStatusScriptExtracted(QString *errorMessage = nullptr)
{
    // 释放位置：EVA_TEMP/tools/Get-GpuStatus.ps1
    // 说明：脚本会在同目录写入缓存 Get-GpuStatus.cache.json
    const QString tempRoot = evaTempRootPath();
    const QString toolDir = QDir(tempRoot).filePath(QStringLiteral("tools"));
    const QString dstPath = QDir(toolDir).filePath(QStringLiteral("Get-GpuStatus.ps1"));
    const QString resPath = QStringLiteral(":/Get-GpuStatus.ps1");

    if (!QDir().mkpath(toolDir))
    {
        if (errorMessage) *errorMessage = QStringLiteral("创建目录失败：%1").arg(QDir::toNativeSeparators(toolDir));
        return QString();
    }

    QFile res(resPath);
    if (!res.open(QIODevice::ReadOnly))
    {
        if (errorMessage) *errorMessage = QStringLiteral("打开资源失败：%1").arg(resPath);
        return QString();
    }

    const QByteArray payload = res.readAll();
    if (payload.isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("资源脚本为空：%1").arg(resPath);
        return QString();
    }

    // 若文件已存在且大小一致，认为无需重复释放（避免影响脚本缓存文件的 locality）
    const QFileInfo dstInfo(dstPath);
    if (dstInfo.exists() && dstInfo.size() == payload.size())
    {
        return dstPath;
    }

    QSaveFile out(dstPath);
    if (!out.open(QIODevice::WriteOnly))
    {
        if (errorMessage) *errorMessage = QStringLiteral("写入脚本失败：%1").arg(QDir::toNativeSeparators(dstPath));
        return QString();
    }
    if (out.write(payload) != payload.size())
    {
        if (errorMessage) *errorMessage = QStringLiteral("写入脚本数据不完整：%1").arg(QDir::toNativeSeparators(dstPath));
        return QString();
    }
    if (!out.commit())
    {
        if (errorMessage) *errorMessage = QStringLiteral("落盘失败：%1").arg(QDir::toNativeSeparators(dstPath));
        return QString();
    }
    return dstPath;
}
#endif
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
#ifdef _WIN32
        // Windows 下不依赖 rocm-smi：改用内置 PowerShell 脚本获取显存/利用率信息
        getGpuInfoViaGetGpuStatusScript();
#else
        // 非 Windows 仍然优先使用 rocm-smi
        getGpuInfoViaRocmSmi();
#endif
    }

  private:
#ifdef _WIN32
    void emitCachedOrZero()
    {
        if (hasCached_)
        {
            emit gpu_status(cachedVmemMb_, cachedVrampPct_, cachedVcorePct_, cachedVfreeMb_);
            return;
        }
        emit gpu_status(0, 0, 0, 0);
    }

    void disableScriptAndEmitCached(const QString &reason)
    {
        // 熔断：只要出现一次错误，后续不再尝试运行脚本，避免 500ms 刷新把系统拖垮
        scriptDisabled_ = true;
        qWarning().noquote() << "[gpuchecker] Get-GpuStatus.ps1 disabled:" << reason;
        emitCachedOrZero();
    }

    void cacheAndEmit(float vmemMb, float vrampPct, float vcorePct, float vfreeMb)
    {
        cachedVmemMb_ = vmemMb;
        cachedVrampPct_ = vrampPct;
        cachedVcorePct_ = vcorePct;
        cachedVfreeMb_ = vfreeMb;
        hasCached_ = true;

        if (!lastOkTimer_.isValid())
        {
            lastOkTimer_.start();
        }
        else
        {
            lastOkTimer_.restart();
        }
        emit gpu_status(vmemMb, vrampPct, vcorePct, vfreeMb);
    }

    void getGpuInfoViaGetGpuStatusScript()
    {
        // 节流：脚本缓存后单次执行约 3s；UI 每 500ms 触发一次刷新，因此这里必须降低实际执行频率
        // 需求：拿到一次结果后，约 1s 再允许下一次执行（实际周期还会叠加脚本自身运行耗时）
        constexpr int kMinRefreshIntervalMs = 1000;

        if (scriptDisabled_)
        {
            emitCachedOrZero();
            return;
        }

        if (hasCached_ && lastOkTimer_.isValid() && lastOkTimer_.elapsed() < kMinRefreshIntervalMs)
        {
            emitCachedOrZero();
            return;
        }

        QString extractErr;
        const QString scriptPath = ensureGetGpuStatusScriptExtracted(&extractErr);
        if (scriptPath.isEmpty())
        {
            disableScriptAndEmitCached(extractErr.isEmpty() ? QStringLiteral("extract failed") : extractErr);
            return;
        }

        const QString scriptDir = QFileInfo(scriptPath).absolutePath();
        const QString cachePath = QDir(scriptDir).filePath(QStringLiteral("Get-GpuStatus.cache.json"));
        const QString metaPath = QDir(scriptDir).filePath(QStringLiteral("Get-GpuStatus.eva.meta.json"));

        const QString psExe = resolveNativePowerShellExe();

        // 若已存在缓存但来源不明（例如曾经用 32 位 PowerShell 生成），则强制刷新一次以纠正总显存。
        bool refreshCache = false;
        const bool cacheExists = QFileInfo::exists(cachePath);
        if (cacheExists)
        {
            bool metaOk = false;
            QFile metaFile(metaPath);
            if (metaFile.open(QIODevice::ReadOnly))
            {
                QJsonParseError metaErr;
                const QJsonDocument metaDoc = QJsonDocument::fromJson(metaFile.readAll(), &metaErr);
                if (metaErr.error == QJsonParseError::NoError && metaDoc.isObject())
                {
                    const QJsonObject metaObj = metaDoc.object();
                    const int ver = metaObj.value(QStringLiteral("MetaVersion")).toInt(-1);
                    const QString lastPs = metaObj.value(QStringLiteral("PowerShellExe")).toString();
                    if (ver == 1 && !lastPs.isEmpty())
                    {
                        metaOk = true;
                        if (normalizeWinPathForCompare(lastPs) != normalizeWinPathForCompare(psExe))
                        {
                            refreshCache = true;
                        }
                    }
                }
            }
            if (!metaOk)
            {
                // 没有 meta 但缓存已存在：无法判断是否可靠，刷新一次最稳妥
                refreshCache = true;
            }
        }

        // 根据缓存/刷新决策设置超时：首次生成/强制刷新更慢
        const int timeoutMs = (!cacheExists || refreshCache) ? 30000 : 12000;

        // 让脚本仅输出汇总对象，并转换为 JSON，便于稳定解析
        // 输出示例：{"TotalGB":96.0,"UsedGB":2.13,"FreeGB":93.87,"UtilizationPct":7.71}
        const QString psPath = psEscapeSingleQuoted(QDir::toNativeSeparators(scriptPath));
        const QString refreshArg = refreshCache ? QStringLiteral(" -RefreshCache") : QString();
        const QString cmd = QStringLiteral(
                                "& { "
                                "$ErrorActionPreference='Stop'; "
                                "$ProgressPreference='SilentlyContinue'; "
                                "& '%1' -All:$false%2 | ConvertTo-Json -Compress "
                                "}")
                                .arg(psPath, refreshArg);

        QString output;
        const QStringList args = {"-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", cmd};
        if (!runProcessReadAll(psExe, args, output, timeoutMs))
        {
            disableScriptAndEmitCached(QStringLiteral("PowerShell 执行失败或超时（timeout=%1ms, refresh=%2）")
                                           .arg(timeoutMs)
                                           .arg(refreshCache ? QStringLiteral("yes") : QStringLiteral("no")));
            return;
        }

        const QByteArray jsonBytes = output.trimmed().toUtf8();
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        {
            disableScriptAndEmitCached(QStringLiteral("JSON 解析失败：%1").arg(parseErr.errorString()));
            return;
        }
        const QJsonObject obj = doc.object();

        const QJsonValue totalV = obj.value(QStringLiteral("TotalGB"));
        const QJsonValue usedV = obj.value(QStringLiteral("UsedGB"));
        const QJsonValue freeV = obj.value(QStringLiteral("FreeGB"));
        const QJsonValue utilV = obj.value(QStringLiteral("UtilizationPct"));
        if (!totalV.isDouble() || !usedV.isDouble() || !freeV.isDouble())
        {
            disableScriptAndEmitCached(QStringLiteral("脚本输出缺少字段：%1").arg(QString::fromUtf8(jsonBytes.left(120))));
            return;
        }

        const double totalGB = totalV.toDouble();
        const double usedGB = usedV.toDouble();
        const double freeGB = freeV.toDouble();
        const double utilPct = utilV.isDouble() ? utilV.toDouble() : 0.0;

        if (!(totalGB > 0.0) || usedGB < 0.0 || freeGB < 0.0)
        {
            disableScriptAndEmitCached(QStringLiteral("脚本输出异常：TotalGB=%1 UsedGB=%2 FreeGB=%3").arg(totalGB).arg(usedGB).arg(freeGB));
            return;
        }

        // 统一到现有 UI 的单位：v* 以 MiB 为主（历史兼容），百分比 0~100
        const float totalMb = float(totalGB * 1024.0);
        const float usedMb = float(usedGB * 1024.0);
        const float freeMb = float(freeGB * 1024.0);

        float usedPct = 0.0f;
        if (totalMb > 0.0f) usedPct = usedMb / totalMb * 100.0f;
        if (usedPct < 0.0f) usedPct = 0.0f;
        if (usedPct > 100.0f) usedPct = 100.0f;

        float corePct = float(utilPct);
        if (corePct < 0.0f) corePct = 0.0f;
        if (corePct > 100.0f) corePct = 100.0f;

        // 记录本次缓存的生成环境（用于下一次判断是否需要强制刷新）
        {
            QJsonObject meta;
            meta.insert(QStringLiteral("MetaVersion"), 1);
            meta.insert(QStringLiteral("PowerShellExe"), QDir::toNativeSeparators(psExe));
            meta.insert(QStringLiteral("ScriptPath"), QDir::toNativeSeparators(scriptPath));
            meta.insert(QStringLiteral("CachePath"), QDir::toNativeSeparators(cachePath));
            const QJsonDocument metaDoc(meta);
            QSaveFile metaOut(metaPath);
            if (metaOut.open(QIODevice::WriteOnly))
            {
                metaOut.write(metaDoc.toJson(QJsonDocument::Compact));
                metaOut.commit();
            }
        }

        cacheAndEmit(totalMb, usedPct, corePct, freeMb);
    }

    bool scriptDisabled_ = false;
    bool hasCached_ = false;
    QElapsedTimer lastOkTimer_;
    float cachedVmemMb_ = 0.0f;
    float cachedVrampPct_ = 0.0f;
    float cachedVcorePct_ = 0.0f;
    float cachedVfreeMb_ = 0.0f;
#else
    void getGpuInfoViaRocmSmi()
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
#endif
};

class gpuChecker : public QObject
{
    Q_OBJECT

  public:
    gpuChecker() = default;

    ~gpuChecker()
    {
        delete gpuInfoProvider;
    }

    void checkGpu()
    {
        ensureProvider();
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
    void ensureProvider()
    {
        if (gpuInfoProvider) return;
        const QString vendor = getGpuVendor();
        if (vendor == QLatin1String("NVIDIA"))
        {
            gpuInfoProvider = new NvidiaGpuInfoProvider();
        }
        else if (vendor == QLatin1String("AMD"))
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

    QString getGpuVendor()
    {
        if (!vendorResolved_)
        {
            vendorCache_ = detectGpuVendor();
            vendorResolved_ = true;
        }
        return vendorCache_;
    }

    QString detectGpuVendor()
    {
        QString output;
#ifdef _WIN32
        if (!runProcessReadAll("powershell", {"-NoProfile", "-Command", "Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name"}, output))
        {
            runProcessReadAll("wmic", {"path", "win32_videocontroller", "get", "caption"}, output);
        }
        if (output.trimmed().isEmpty())
        {
            QString tmp;
            if (runProcessReadAll("nvidia-smi", {"-L"}, tmp))
                output = tmp;
        }
#elif __linux__
        if (!runProcessReadAll("bash", {"-lc", "lspci -nn | grep -i 'VGA'"}, output)) output.clear();
#endif
        if (output.contains("NVIDIA", Qt::CaseInsensitive)) return QStringLiteral("NVIDIA");
        if (output.contains("AMD", Qt::CaseInsensitive) || output.contains("Radeon", Qt::CaseInsensitive)) return QStringLiteral("AMD");
        if (output.contains("Intel", Qt::CaseInsensitive)) return QStringLiteral("Intel");
        return QStringLiteral("Unknown");
    }

    GpuInfoProvider *gpuInfoProvider = nullptr;
    QString vendorCache_;
    bool vendorResolved_ = false;

  signals:
    void gpu_status(float vmem, float vram, float vcore, float vfree);
};

#endif // GPUCHECKER_H
