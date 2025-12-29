// DeviceManager - runtime backend discovery and path resolution
#include "devicemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QDebug>
#include <QLibrary>
#include <QOperatingSystemVersion>
#include <QProcessEnvironment>
#include <QSet>
#include <QSysInfo>
#include <QtGlobal>

#if defined(Q_PROCESSOR_X86_64) || defined(Q_PROCESSOR_X86)
#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif
#endif

static QString g_userChoice = QStringLiteral("auto"); // process-local selection
static QStringList g_lastProbed;                      // debug: last probed paths
static QHash<QString, QString> g_lastResolvedDevice;  // program -> device that was actually resolved
static QHash<QString, QString> g_programOverrides;    // role id -> absolute override path

namespace
{
struct ProgramDescriptor
{
    QString roleId;
    QString binaryName;
    QString label;
};

static QString normalizeRoleId(const QString &raw)
{
    QString id = raw.trimmed();
    if (id.isEmpty()) return QStringLiteral("llama-server-main");
    QString lower = id.toLower();
    if (lower == QLatin1String("llama-server"))
    {
        lower = QStringLiteral("llama-server-main");
    }
    return lower;
}

static const QVector<ProgramDescriptor> &programDescriptors()
{
    static const QVector<ProgramDescriptor> roles = {
        {QStringLiteral("llama-server-main"), QStringLiteral("llama-server"), QStringLiteral("LLM server (chat)")},
        {QStringLiteral("llama-server-embed"), QStringLiteral("llama-server"), QStringLiteral("Embedding server")},
        {QStringLiteral("whisper-cli"), QStringLiteral("whisper-cli"), QStringLiteral("Whisper CLI")},
        {QStringLiteral("tts-cli"), QStringLiteral("tts-cli"), QStringLiteral("TTS CLI")},
        {QStringLiteral("sd"), QStringLiteral("sd"), QStringLiteral("Stable Diffusion")}    };
    return roles;
}

static ProgramDescriptor descriptorFor(const QString &name)
{
    const QString normalized = normalizeRoleId(name);
    for (const ProgramDescriptor &desc : programDescriptors())
    {
        if (desc.roleId == normalized) return desc;
    }
    return ProgramDescriptor{normalized, normalized, normalized};
}

static QString canonicalFilePath(const QString &path)
{
    QFileInfo fi(path);
    const QString canonical = fi.canonicalFilePath();
    if (!canonical.isEmpty()) return canonical;
    return fi.absoluteFilePath();
}

static QString projectSegment(const QString &deviceDir, const QString &filePath)
{
    const QString rel = QDir(deviceDir).relativeFilePath(filePath);
    const QString normalized = QDir::fromNativeSeparators(rel);
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!parts.isEmpty()) return parts.first();
    return QString();
}
} // namespace
QString DeviceManager::backendsRootDir()
{
    const QStringList roots = candidateBackendRoots();
    if (!roots.isEmpty()) return roots.first();
    // Fall back to a predictable location even if it does not exist yet
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("EVA_BACKEND"));
}

QStringList DeviceManager::candidateBackendRoots()
{
    QStringList probes; // include non-existing for diagnostics

    // 0) Explicit override via env (power users/packagers)
    const QString envRoot = QString::fromLocal8Bit(qgetenv("EVA_BACKEND_ROOT"));
    if (!envRoot.isEmpty()) probes << envRoot;

#if defined(Q_OS_LINUX)
    // 1) AppImage internal mount (packaged)
    const QString appDir = QString::fromLocal8Bit(qgetenv("APPDIR"));
    if (!appDir.isEmpty())
    {
        probes << QDir(appDir).filePath("usr/bin/EVA_BACKEND");
        probes << QDir(appDir).filePath("EVA_BACKEND");
    }

    // 2) If exe under /usr/bin, try resolving root of mount
    const QString exeDir = QCoreApplication::applicationDirPath();
    QDir maybeUsrBin(exeDir);
    if (maybeUsrBin.path().endsWith(QStringLiteral("/usr/bin")))
    {
        maybeUsrBin.cdUp(); // usr
        maybeUsrBin.cdUp(); // APPDIR
        probes << maybeUsrBin.filePath("usr/bin/EVA_BACKEND");
        probes << maybeUsrBin.filePath("EVA_BACKEND");
    }

    // 3) External folder next to the .AppImage file (portable layout)
    const QString appImage = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
    if (!appImage.isEmpty())
    {
        const QFileInfo appImageInfo(appImage);
        if (appImageInfo.exists())
        {
            probes << appImageInfo.absoluteDir().filePath("EVA_BACKEND");
            const QString canonical = appImageInfo.canonicalFilePath();
            if (!canonical.isEmpty())
            {
                probes << QFileInfo(canonical).absoluteDir().filePath("EVA_BACKEND");
            }
        }
    }
#endif

    // Always probe next to the running executable
    probes << QDir(QCoreApplication::applicationDirPath()).filePath("EVA_BACKEND");

    // CWD fallback for developers launching from arbitrary folders
    probes << QDir::currentPath() + "/EVA_BACKEND";

    // De-duplicate while preserving order
    QStringList seen;
    QStringList existing;
    for (const QString &p : probes)
    {
        if (seen.contains(p)) continue;
        seen << p;
        if (QFileInfo::exists(p)) existing << p;
    }
    g_lastProbed = seen; // save for diagnostics
    return existing;
}

QStringList DeviceManager::probedBackendRoots()
{
    return g_lastProbed;
}

bool DeviceManager::hasCustomOverride()
{
    return !g_programOverrides.isEmpty();
}

QMap<QString, QString> DeviceManager::programOverrides()
{
    QMap<QString, QString> out;
    for (auto it = g_programOverrides.constBegin(); it != g_programOverrides.constEnd(); ++it)
    {
        out.insert(it.key(), it.value());
    }
    return out;
}

QString DeviceManager::programOverride(const QString &roleId)
{
    const QString key = descriptorFor(roleId).roleId;
    return g_programOverrides.value(key);
}

void DeviceManager::setProgramOverride(const QString &roleId, const QString &path)
{
    const QString key = descriptorFor(roleId).roleId;
    if (key.isEmpty())
    {
        return;
    }
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
    {
        g_programOverrides.remove(key);
        return;
    }
    QFileInfo fi(trimmed);
    QString stored = fi.absoluteFilePath();
    g_programOverrides.insert(key, stored);
}

void DeviceManager::clearProgramOverride(const QString &roleId)
{
    const QString key = descriptorFor(roleId).roleId;
    if (key.isEmpty()) return;
    g_programOverrides.remove(key);
}

void DeviceManager::clearProgramOverrides()
{
    g_programOverrides.clear();
}

QVector<DeviceManager::BackendRole> DeviceManager::managedRoles()
{
    QVector<BackendRole> roles;
    roles.reserve(programDescriptors().size());
    for (const ProgramDescriptor &desc : programDescriptors())
    {
        BackendRole role;
        role.id = desc.roleId;
        role.binary = desc.binaryName;
        role.label = desc.label;
        roles.append(role);
    }
    return roles;
}

QVector<DeviceManager::BackendExecutableInfo> DeviceManager::enumerateExecutables(const QString &binaryFilter)
{
    QVector<BackendExecutableInfo> result;
    const QStringList roots = candidateBackendRoots();
    if (roots.isEmpty()) return result;

    QStringList binaries;
    const QString filter = binaryFilter.trimmed();
    if (filter.isEmpty())
    {
        const QVector<BackendRole> roles = managedRoles();
        for (const BackendRole &role : roles)
        {
            if (!role.binary.isEmpty() && !binaries.contains(role.binary))
            {
                binaries << role.binary;
            }
        }
    }
    else
    {
        const ProgramDescriptor desc = descriptorFor(filter);
        if (!desc.binaryName.isEmpty()) binaries << desc.binaryName;
    }
    binaries.removeDuplicates();
    if (binaries.isEmpty()) return result;

    QSet<QString> seen;
    for (const QString &root : roots)
    {
        QDir rootDir(root);
        const QFileInfoList archDirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &archInfo : archDirs)
        {
            const QString archName = archInfo.fileName();
            QDir archDir(archInfo.absoluteFilePath());
            const QFileInfoList osDirs = archDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QFileInfo &osInfo : osDirs)
            {
                const QString osName = osInfo.fileName();
                QDir osDir(osInfo.absoluteFilePath());
                const QFileInfoList deviceDirs = osDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for (const QFileInfo &deviceInfo : deviceDirs)
                {
                    const QString deviceName = deviceInfo.fileName();
                    const QString devicePath = deviceInfo.absoluteFilePath();
                    for (const QString &binary : binaries)
                    {
                        const QString pattern = binary + QStringLiteral(SFX_NAME);
                        QDirIterator it(devicePath, QStringList{pattern}, QDir::Files, QDirIterator::Subdirectories);
                        while (it.hasNext())
                        {
                            const QString abs = canonicalFilePath(it.next());
                            if (abs.isEmpty() || seen.contains(abs)) continue;
                            seen.insert(abs);
                            BackendExecutableInfo info;
                            info.root = root;
                            info.arch = archName;
                            info.os = osName;
                            info.device = deviceName;
                            info.programName = binary;
                            info.absolutePath = abs;
                            info.project = projectSegment(devicePath, abs);
                            result.append(info);
                        }
                    }
                }
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// CUDA 12 runtime 探测辅助：用于在选择 CUDA 后端前判断运行时依赖是否可用
// ---------------------------------------------------------------------------
static QStringList osSearchOrder();

static QStringList splitSearchPaths(const QString &raw)
{
    QStringList out;
    const QChar sep = QDir::listSeparator();
    const QStringList parts = raw.split(sep, Qt::SkipEmptyParts);
    for (const QString &p : parts)
    {
        const QString cleaned = QDir::cleanPath(p.trimmed());
        if (!cleaned.isEmpty()) out << cleaned;
    }
    out.removeDuplicates();
    return out;
}

static QStringList cudaRuntimePatterns()
{
    return DEFAULT_CUDA_RUNTIME_LIB_PATTERNS;
}

static QStringList cudaRuntimeSearchDirs(const QString &exeDir)
{
    QStringList dirs;
    if (!exeDir.trimmed().isEmpty())
    {
        dirs << QDir::cleanPath(exeDir.trimmed());
    }
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#if defined(Q_OS_WIN)
    dirs << splitSearchPaths(env.value(QStringLiteral("PATH")));
#elif defined(Q_OS_LINUX)
    dirs << splitSearchPaths(env.value(QStringLiteral("LD_LIBRARY_PATH")));
    // 常见系统库目录补充（避免 LD_LIBRARY_PATH 为空导致误判）
    dirs << QStringLiteral("/usr/lib")
         << QStringLiteral("/usr/lib64")
         << QStringLiteral("/usr/lib/x86_64-linux-gnu")
         << QStringLiteral("/usr/local/lib")
         << QStringLiteral("/usr/local/cuda/lib")
         << QStringLiteral("/usr/local/cuda/lib64");
#elif defined(Q_OS_MAC)
    dirs << splitSearchPaths(env.value(QStringLiteral("DYLD_LIBRARY_PATH")));
#endif
    dirs.removeDuplicates();
    return dirs;
}

static bool dirHasPattern(const QString &dirPath, const QString &pattern)
{
    if (dirPath.trimmed().isEmpty()) return false;
    QDir dir(dirPath);
    if (!dir.exists()) return false;
    return !dir.entryList(QStringList{pattern}, QDir::Files).isEmpty();
}

static bool hasRequiredPatternsInDirs(const QStringList &dirs, const QStringList &patterns, QStringList *missing)
{
    if (patterns.isEmpty()) return true;
    QSet<int> found;
    for (const QString &dir : dirs)
    {
        for (int i = 0; i < patterns.size(); ++i)
        {
            if (found.contains(i)) continue;
            if (dirHasPattern(dir, patterns.at(i)))
            {
                found.insert(i);
            }
        }
        if (found.size() == patterns.size()) break;
    }
    if (missing)
    {
        missing->clear();
        for (int i = 0; i < patterns.size(); ++i)
        {
            if (!found.contains(i)) missing->append(patterns.at(i));
        }
    }
    return found.size() == patterns.size();
}

static void scanDirRecursivelyForPatterns(const QString &root, const QStringList &patterns, QSet<int> *found)
{
    if (!found || patterns.isEmpty()) return;
    if (root.trimmed().isEmpty()) return;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && found->size() < patterns.size())
    {
        const QString filePath = it.next();
        const QString fileName = QFileInfo(filePath).fileName();
        for (int i = 0; i < patterns.size(); ++i)
        {
            if (found->contains(i)) continue;
            if (QDir::match(patterns.at(i), fileName))
            {
                found->insert(i);
            }
        }
    }
}

static bool queryCudaDriverVersion(int *outVersion)
{
#if defined(Q_OS_WIN)
    QLibrary lib(QStringLiteral("nvcuda"));
#elif defined(Q_OS_LINUX)
    QLibrary lib(QStringLiteral("libcuda.so.1"));
#else
    QLibrary lib;
#endif
    if (!lib.load()) return false;
    using CuDriverGetVersionFn = int (*)(int *);
    auto fn = reinterpret_cast<CuDriverGetVersionFn>(lib.resolve("cuDriverGetVersion"));
    if (!fn)
    {
        lib.unload();
        return false;
    }
    int version = 0;
    const int rc = fn(&version);
    lib.unload();
    if (rc != 0) return false;
    if (outVersion) *outVersion = version;
    return true;
}

static bool hasCudaRuntimeInBackendDirs(QStringList *missing, QString *hitDir)
{
    if (!DEFAULT_CUDA_REQUIRE_RUNTIME_LIBS) return true;
    const QStringList patterns = cudaRuntimePatterns();
    if (patterns.isEmpty()) return true;

    const QStringList roots = DeviceManager::candidateBackendRoots();
    if (roots.isEmpty()) return false;

    const QString arch = DeviceManager::currentArchId();
    const QStringList osOrder = osSearchOrder();
    const QString project = DeviceManager::projectForProgram(QStringLiteral("llama-server"));

    for (const QString &root : roots)
    {
        for (const QString &osName : osOrder)
        {
            const QString deviceDir = QDir(QDir(root).filePath(arch)).filePath(osName + QStringLiteral("/cuda"));
            if (!QDir(deviceDir).exists()) continue;

            const QString projDir = QDir(deviceDir).filePath(project);
            QSet<int> found;
            if (QDir(projDir).exists())
            {
                scanDirRecursivelyForPatterns(projDir, patterns, &found);
            }
            if (found.size() < patterns.size())
            {
                scanDirRecursivelyForPatterns(deviceDir, patterns, &found);
            }
            if (found.size() == patterns.size())
            {
                if (hitDir) *hitDir = QDir::cleanPath(QDir(projDir).exists() ? projDir : deviceDir);
                return true;
            }
        }
    }

    if (missing) *missing = patterns;
    return false;
}

static bool cudaRuntimeReadyForHost(QString *detail)
{
    if (!DEFAULT_CUDA_REQUIRE_RUNTIME_LIBS) return true;
    const QStringList patterns = cudaRuntimePatterns();
    if (patterns.isEmpty()) return true;

    const int skipRuntimeProbe = qEnvironmentVariableIntValue("EVA_SKIP_CUDA_RUNTIME_CHECK");
    if (skipRuntimeProbe != 0)
    {
        qInfo().noquote() << "[backend-probe] EVA_SKIP_CUDA_RUNTIME_CHECK=1 -> skip CUDA runtime probe";
        return true;
    }

    const QStringList searchDirs = cudaRuntimeSearchDirs(QString());
    QStringList missing;
    if (hasRequiredPatternsInDirs(searchDirs, patterns, &missing)) return true;

    QString backendHit;
    if (hasCudaRuntimeInBackendDirs(nullptr, &backendHit)) return true;

    if (detail)
    {
        const QString miss = missing.isEmpty() ? patterns.join(QStringLiteral(", ")) : missing.join(QStringLiteral(", "));
        *detail = QStringLiteral("missing: %1; PATH=%2").arg(miss, searchDirs.join(QStringLiteral("; ")));
    }
    return false;
}

static bool cudaRuntimeReadyForExecutable(const QString &exePath, QString *detail)
{
    if (!DEFAULT_CUDA_REQUIRE_RUNTIME_LIBS) return true;
    const QStringList patterns = cudaRuntimePatterns();
    if (patterns.isEmpty()) return true;

    const QString exeDir = QFileInfo(exePath).absolutePath();
    const QStringList dirs = cudaRuntimeSearchDirs(exeDir);
    QStringList missing;
    const bool ok = hasRequiredPatternsInDirs(dirs, patterns, &missing);
    if (ok) return true;

    if (detail)
    {
        QStringList hint = missing;
        hint.removeDuplicates();
        *detail = QStringLiteral("missing: %1; search=%2")
                      .arg(hint.join(QStringLiteral(", ")), dirs.join(QStringLiteral("; ")));
    }
    return false;
}

// --- Runtime capability probes (very lightweight; best-effort) ---
static bool supportsCuda()
{
    static int cached = -1;
    if (cached != -1) return cached != 0;
    // 允许通过环境变量跳过 GPU 驱动探测，便于在 Win7 等老环境规避 nvcuda 初始化触发的非法指令
    static const bool skipProbe = (qEnvironmentVariableIntValue("EVA_SKIP_CUDA_PROBE") != 0);
    if (skipProbe)
    {
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            qInfo().noquote() << "[backend-probe] EVA_SKIP_CUDA_PROBE=1 -> skip CUDA driver probe";
        }
        cached = 0;
        return false;
    }
#if defined(Q_OS_WIN)
    QLibrary lib(QStringLiteral("nvcuda"));
    bool driverOk = lib.load();
    if (driverOk) lib.unload();
#elif defined(Q_OS_LINUX)
    QLibrary lib(QStringLiteral("libcuda.so.1"));
    bool driverOk = lib.load();
    if (driverOk) lib.unload();
    if (!driverOk)
    {
        // Fallback heuristics
        driverOk = QFileInfo::exists("/proc/driver/nvidia/version") || QFileInfo::exists("/dev/nvidiactl");
    }
#else
    bool driverOk = false;
#endif
    if (!driverOk)
    {
        cached = 0;
        return false;
    }

    // CUDA 12 only: driver 版本过低时直接视为不可用
    if (DEFAULT_CUDA_REQUIRED_MAJOR > 0)
    {
        int driverVersion = 0;
        if (queryCudaDriverVersion(&driverVersion))
        {
            const int required = DEFAULT_CUDA_REQUIRED_MAJOR * 1000;
            if (driverVersion < required)
            {
                qInfo().noquote() << QStringLiteral("[backend-probe] CUDA driver version %1 < %2 (CUDA %3)")
                                         .arg(driverVersion)
                                         .arg(required)
                                         .arg(DEFAULT_CUDA_REQUIRED_MAJOR);
                cached = 0;
                return false;
            }
        }
        else
        {
            qInfo().noquote() << "[backend-probe] CUDA driver version unavailable; assume compatible";
        }
    }

    // CUDA 12 runtime 依赖缺失时，直接阻止选择 CUDA 后端
    QString runtimeDetail;
    if (!cudaRuntimeReadyForHost(&runtimeDetail))
    {
        qInfo().noquote() << QStringLiteral("[backend-probe] CUDA runtime not ready (%1)").arg(runtimeDetail);
        cached = 0;
        return false;
    }

    cached = 1;
    return true;
}
static bool supportsVulkan()
{
#if defined(Q_OS_WIN)
    QLibrary lib(QStringLiteral("vulkan-1"));
    const bool ok = lib.load();
    if (ok) lib.unload();
    return ok;
#elif defined(Q_OS_LINUX)
    QLibrary lib(QStringLiteral("libvulkan.so.1"));
    const bool ok = lib.load();
    if (ok) lib.unload();
    return ok;
#else
    return false;
#endif
}
static bool supportsOpenCL()
{
#if defined(Q_OS_WIN)
    QLibrary lib(QStringLiteral("OpenCL"));
    const bool ok = lib.load();
    if (ok) lib.unload();
    return ok;
#elif defined(Q_OS_LINUX)
    QLibrary lib(QStringLiteral("libOpenCL.so.1"));
    const bool ok = lib.load();
    if (ok) lib.unload();
    return ok;
#else
    return false;
#endif
}

// x86 指令集探测：是否可安全使用 AVX（CPU 支持 + OS 启用保存 YMM 状态）。
// 说明：我们用它来决定 cpu/cpu-noavx 的优先级，避免在老 CPU 上直接触发“非法指令”崩溃。
// - 仅在 x86/x64 下生效；其它架构直接返回 true（不存在 AVX 指令的问题）。
// - 提供环境变量 EVA_FORCE_NOAVX=1 方便排障/强制回退。
static quint64 xgetbv0()
{
#if defined(Q_PROCESSOR_X86_64) || defined(Q_PROCESSOR_X86)
#if defined(_MSC_VER)
    return static_cast<quint64>(_xgetbv(0));
#elif defined(__GNUC__) || defined(__clang__)
    quint32 eax = 0, edx = 0;
    __asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0));
    return (static_cast<quint64>(edx) << 32) | eax;
#else
    return 0;
#endif
#else
    return 0;
#endif
}

static bool supportsAvx()
{
#if defined(Q_PROCESSOR_X86_64) || defined(Q_PROCESSOR_X86)
    // 进程内缓存：CPU 能力不会变化，且这些探测在 Win7/老硬件上越少越好。
    static int cached = -1;
    if (cached != -1) return cached != 0;

    // 允许用户强制关闭 AVX（例如：打包的 cpu 后端误用了更高指令集，或用于复现问题）。
    if (qEnvironmentVariableIntValue("EVA_FORCE_NOAVX") != 0)
    {
        cached = 0;
        return false;
    }

    bool ok = false;
#if defined(_MSC_VER)
    int cpuInfo[4] = {0, 0, 0, 0};
    __cpuid(cpuInfo, 1);
    const bool hasXsave = (cpuInfo[2] & (1 << 26)) != 0;
    const bool hasOsxsave = (cpuInfo[2] & (1 << 27)) != 0;
    const bool hasAvx = (cpuInfo[2] & (1 << 28)) != 0;
    if (hasXsave && hasOsxsave && hasAvx)
    {
        // XCR0 bit1(XMM) 与 bit2(YMM) 必须同时打开，否则 AVX 指令会触发非法指令。
        const quint64 xcr0 = xgetbv0();
        ok = ((xcr0 & 0x6) == 0x6);
    }
#elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
    {
        const bool hasXsave = (ecx & (1u << 26)) != 0;
        const bool hasOsxsave = (ecx & (1u << 27)) != 0;
        const bool hasAvx = (ecx & (1u << 28)) != 0;
        if (hasXsave && hasOsxsave && hasAvx)
        {
            const quint64 xcr0 = xgetbv0();
            ok = ((xcr0 & 0x6) == 0x6);
        }
    }
#else
    ok = true; // 不认识的编译器：保守返回 true，后续仍有“崩溃后回退”兜底
#endif

    cached = ok ? 1 : 0;
    if (!ok)
    {
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            qInfo().noquote() << "[backend-probe] AVX unsupported -> prefer cpu-noavx";
        }
    }
    return ok;
#else
    return true;
#endif
}

// Windows 7/8 detection; Qt may lie without manifest, so be redundant
static bool isWindows7Or8Family()
{
#if defined(Q_OS_WIN)
    const auto v = QOperatingSystemVersion::current();
    if (v.majorVersion() == 6 && v.minorVersion() >= 1 && v.minorVersion() <= 3) return true; // 6.1 (Win7), 6.2 (Win8), 6.3 (8.1)
    const QString pv = QSysInfo::productVersion();
    if (pv.startsWith("7") || pv.startsWith("8")) return true;
    return false;
#else
    return false;
#endif
}

// OS search order for EVA_BACKEND lookup. On Win7/8, try win7/ then fallback to win/.
static QStringList osSearchOrder()
{
    QStringList out;
#if defined(Q_OS_WIN)
    if (isWindows7Or8Family()) { out << QStringLiteral("win7") << QStringLiteral("win"); }
    else
    {
        out << QStringLiteral("win");
    }
#elif defined(Q_OS_LINUX)
    out << QStringLiteral("linux");
#elif defined(Q_OS_MAC)
    out << QStringLiteral("mac");
#else
    out << QStringLiteral("linux");
#endif
    return out;
}

// Device search order honoring user's choice and runtime capability
static QStringList deviceSearchOrder()
{
    QStringList order;
    const QString choice = DeviceManager::userChoice();
    const bool autoMode = (choice == QLatin1String("auto") || choice == QLatin1String("custom"));
    const bool avxOk = supportsAvx();
    auto add = [&](const QString &b)
    { if (!order.contains(b)) order << b; };
    auto addIf = [&](const QString &b, bool ok)
    { if (ok) add(b); };

    if (autoMode)
    {
        const QStringList preferred = DeviceManager::preferredOrder();
        for (const QString &backend : preferred)
        {
            if (backend == QLatin1String("cpu"))
            {
                add(QStringLiteral("cpu"));
            }
            else if (backend == QLatin1String("cpu-noavx"))
            {
                add(QStringLiteral("cpu-noavx"));
            }
            else if (backend == QLatin1String("cuda"))
            {
                addIf(QStringLiteral("cuda"), supportsCuda());
            }
            else if (backend == QLatin1String("vulkan"))
            {
                addIf(QStringLiteral("vulkan"), supportsVulkan());
            }
            else if (backend == QLatin1String("opencl"))
            {
                addIf(QStringLiteral("opencl"), supportsOpenCL());
            }
            else
            {
                add(backend);
            }
        }
        add(QStringLiteral("cpu")); // make sure cpu fallback is included
        add(QStringLiteral("cpu-noavx"));
    }
    else
    {
        if (choice == QLatin1String("cuda"))
            addIf(QStringLiteral("cuda"), supportsCuda());
        else if (choice == QLatin1String("vulkan"))
            addIf(QStringLiteral("vulkan"), supportsVulkan());
        else if (choice == QLatin1String("opencl"))
            addIf(QStringLiteral("opencl"), supportsOpenCL());
        else if (choice == QLatin1String("cpu"))
        {
            // 老 CPU 不支持 AVX 时，仍允许用户选 cpu，但优先尝试 cpu-noavx，避免直接崩溃。
            if (!avxOk) add(QStringLiteral("cpu-noavx"));
            add(QStringLiteral("cpu"));
        }
        else if (choice == QLatin1String("cpu-noavx"))
        {
            add(QStringLiteral("cpu-noavx"));
            // cpu-noavx 是更“保守”的二进制；如果机器支持 AVX，仍把 cpu 作为次选，方便用户切回更快版本。
            add(QStringLiteral("cpu"));
        }
        else
        {
            addIf(QStringLiteral("cuda"), supportsCuda());
            addIf(QStringLiteral("vulkan"), supportsVulkan());
            addIf(QStringLiteral("opencl"), supportsOpenCL());
        }
        // Fallback chain
        addIf(QStringLiteral("cuda"), supportsCuda());
        addIf(QStringLiteral("vulkan"), supportsVulkan());
        addIf(QStringLiteral("opencl"), supportsOpenCL());
        // CPU 回退：若不支持 AVX，先尝试 cpu-noavx，避免 cpu 版本“非法指令”秒退。
        if (!avxOk) add(QStringLiteral("cpu-noavx"));
        add(QStringLiteral("cpu"));
        add(QStringLiteral("cpu-noavx"));
    }
    return order;
}
QStringList DeviceManager::preferredOrder()
{
    const QString arch = currentArchId();
    const bool win7Family = isWindows7Or8Family();
    const bool cpuFirst = arch.startsWith(QLatin1String("arm")) || win7Family;
    const bool avxOk = supportsAvx();
    QStringList order;
    if (cpuFirst)
    {
        // Win7/老硬件：默认优先 CPU，避免 GPU driver probe 引发崩溃。
        // 但若机器不支持 AVX，则必须把 cpu-noavx 放在最前面，防止 llama-server/tts/sd 等直接异常退出。
        if (!avxOk) order << QStringLiteral("cpu-noavx");
        order << QStringLiteral("cpu");
        if (win7Family && !order.contains(QStringLiteral("cpu-noavx"))) order << QStringLiteral("cpu-noavx");
        order << QStringLiteral("cuda") << QStringLiteral("vulkan") << QStringLiteral("opencl");
        return order;
    }
    order << QStringLiteral("cuda") << QStringLiteral("vulkan") << QStringLiteral("opencl") << QStringLiteral("cpu");
    // 非 Win7 的默认顺序仍以 GPU 优先，但 CPU 回退也要考虑 AVX 兼容性。
    if (!avxOk)
    {
        // 放在 cpu 前面更安全；cpu-noavx 不存在时会自动跳过。
        order.removeAll(QStringLiteral("cpu"));
        order << QStringLiteral("cpu-noavx") << QStringLiteral("cpu");
    }
    if (win7Family && !order.contains(QStringLiteral("cpu-noavx"))) order << QStringLiteral("cpu-noavx");
    return order;
}

static QString firstPreferredAvailable(const QStringList &available)
{
    const QStringList preferred = DeviceManager::preferredOrder();
    for (const QString &backend : preferred)
    {
        if (!available.contains(backend)) continue;
        if (backend == QLatin1String("cuda"))
        {
            if (supportsCuda()) return QStringLiteral("cuda");
            continue;
        }
        if (backend == QLatin1String("vulkan"))
        {
            if (supportsVulkan()) return QStringLiteral("vulkan");
            continue;
        }
        if (backend == QLatin1String("opencl"))
        {
            if (supportsOpenCL()) return QStringLiteral("opencl");
            continue;
        }
        if (backend == QLatin1String("cpu")) return QStringLiteral("cpu");
        if (backend == QLatin1String("cpu-noavx")) return QStringLiteral("cpu-noavx");
        // Unknown backend types: honor availability order as-is.
        return backend;
    }
    return available.isEmpty() ? QStringLiteral("cpu") : available.first();
}

QStringList DeviceManager::availableBackends()
{
    // Enumerate device folders under first matching OS dir for each candidate root.
    const QStringList roots = candidateBackendRoots();
    const QString arch = currentArchId();
    QStringList out;
    const QStringList osOrder = osSearchOrder();
    for (const QString &root : roots)
    {
        QString osDir;
        for (const QString &osName : osOrder)
        {
            const QString tryDir = QDir(QDir(root).filePath(arch)).filePath(osName);
            if (QDir(tryDir).exists())
            {
                osDir = tryDir;
                break;
            }
        }
        if (osDir.isEmpty()) continue;
        QDir d(osDir);
        const QFileInfoList subs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &fi : subs)
        {
            if (!out.contains(fi.fileName())) out << fi.fileName();
        }
    }
    return out;
}

void DeviceManager::setUserChoice(const QString &backend)
{
    const QString b = backend.trimmed().toLower();
    if (b == QLatin1String("auto") || b == QLatin1String("custom") || b == QLatin1String("cpu") || b == QLatin1String("cpu-noavx") || b == QLatin1String("cuda") || b == QLatin1String("vulkan") || b == QLatin1String("opencl"))
    {
        g_userChoice = b;
    }
    else
    {
        g_userChoice = QStringLiteral("auto");
    }
}

QString DeviceManager::userChoice()
{
    return g_userChoice;
}

QString DeviceManager::effectiveBackend()
{
    const QStringList avail = availableBackends();
    auto isAvail = [&](const QString &b)
    { return avail.contains(b); };
    const QString choice = userChoice();
    const bool autoMode = (choice == QLatin1String("auto") || choice == QLatin1String("custom"));

    if (autoMode)
    {
        return firstPreferredAvailable(avail);
    }
    // 显式选择 cpu 但机器不支持 AVX：若存在 cpu-noavx，则自动降级，避免直接非法指令崩溃。
    if (choice == QLatin1String("cpu") && !supportsAvx() && isAvail("cpu-noavx")) return QStringLiteral("cpu-noavx");
    // Explicit choice: honor if available and supported; else degrade
    if (choice == QLatin1String("cuda") && supportsCuda() && isAvail("cuda")) return QStringLiteral("cuda");
    if (choice == QLatin1String("vulkan") && supportsVulkan() && isAvail("vulkan")) return QStringLiteral("vulkan");
    if (choice == QLatin1String("opencl") && supportsOpenCL() && isAvail("opencl")) return QStringLiteral("opencl");
    if (choice == QLatin1String("cpu") && isAvail("cpu")) return QStringLiteral("cpu");
    if (choice == QLatin1String("cpu-noavx") && isAvail("cpu-noavx")) return QStringLiteral("cpu-noavx");
    return firstPreferredAvailable(avail);
}

// UI helper: compute effective backend for a given preferred value without
// changing the stored user choice. Mirrors effectiveBackend() but uses the
// provided string instead of DeviceManager::userChoice().
QString DeviceManager::effectiveBackendFor(const QString &preferred)
{
    const QStringList avail = availableBackends();
    auto isAvail = [&](const QString &b)
    { return avail.contains(b); };
    const QString choice = preferred.trimmed().toLower();
    const bool autoMode = (choice == QLatin1String("auto") || choice == QLatin1String("custom"));

    if (autoMode)
    {
        return firstPreferredAvailable(avail);
    }
    if (choice == QLatin1String("cpu") && !supportsAvx() && isAvail("cpu-noavx")) return QStringLiteral("cpu-noavx");
    if (choice == QLatin1String("cuda") && supportsCuda() && isAvail("cuda")) return QStringLiteral("cuda");
    if (choice == QLatin1String("vulkan") && supportsVulkan() && isAvail("vulkan")) return QStringLiteral("vulkan");
    if (choice == QLatin1String("opencl") && supportsOpenCL() && isAvail("opencl")) return QStringLiteral("opencl");
    if (choice == QLatin1String("cpu") && isAvail("cpu")) return QStringLiteral("cpu");
    if (choice == QLatin1String("cpu-noavx") && isAvail("cpu-noavx")) return QStringLiteral("cpu-noavx");
    return firstPreferredAvailable(avail);
}

static QString findProgramRecursive(const QString &dir, const QString &exe)
{
    QDirIterator it(dir, QStringList{exe}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString p = it.next();
        if (QFileInfo::exists(p)) return p;
    }
    return QString();
}

QString DeviceManager::programPath(const QString &name)
{
    // Central doctrine layout:
    // EVA_BACKEND/<arch>/<os>/<device>/<project>/<exe>
    // example: EVA_BACKEND/x86_64/win/cuda/llama.cpp/llama-server(.exe)
    const ProgramDescriptor desc = descriptorFor(name);
    const QString roleId = desc.roleId;
    const QString overridePath = g_programOverrides.value(roleId);
    if (!overridePath.isEmpty() && QFileInfo::exists(overridePath))
    {
        g_lastResolvedDevice.insert(roleId, QStringLiteral("custom"));
        return overridePath;
    }
    const QStringList roots = candidateBackendRoots();
    const QString arch = currentArchId();
    const QStringList osOrder = osSearchOrder();
    const QStringList devices = deviceSearchOrder();
    const QString project = projectForProgram(desc.binaryName);
    const QString exe = desc.binaryName + QStringLiteral(SFX_NAME);

    g_lastResolvedDevice.remove(roleId);

    auto acceptCandidate = [&](const QString &device, const QString &path) -> bool
    {
        if (device != QStringLiteral("cuda")) return true;
        QString detail;
        if (!cudaRuntimeReadyForExecutable(path, &detail))
        {
            qInfo().noquote() << QStringLiteral("[backend-probe] CUDA runtime not ready for %1 (%2)")
                                     .arg(QDir::toNativeSeparators(path), detail);
            return false;
        }
        return true;
    };

    for (const QString &root : roots)
    {
        QString osDir;
        for (const QString &osName : osOrder)
        {
            const QString tryDir = QDir(QDir(root).filePath(arch)).filePath(osName);
            if (QDir(tryDir).exists())
            {
                osDir = tryDir;
                break;
            }
        }
        if (osDir.isEmpty()) continue;

        for (const QString &device : devices)
        {
            const QString projDirNew = QDir(QDir(osDir).filePath(device)).filePath(project);
            if (QFileInfo::exists(projDirNew))
            {
                const QString direct = QDir(projDirNew).filePath(exe);
                if (QFileInfo::exists(direct))
                {
                    if (!acceptCandidate(device, direct)) continue;
                    g_lastResolvedDevice.insert(roleId, device);
                    return direct;
                }
                const QString rec = findProgramRecursive(projDirNew, exe);
                if (!rec.isEmpty())
                {
                    if (!acceptCandidate(device, rec)) continue;
                    g_lastResolvedDevice.insert(roleId, device);
                    return rec;
                }
            }
            const QString devDirNew = QDir(osDir).filePath(device);
            if (QFileInfo::exists(devDirNew))
            {
                const QString rec = findProgramRecursive(devDirNew, exe);
                if (!rec.isEmpty())
                {
                    if (!acceptCandidate(device, rec)) continue;
                    g_lastResolvedDevice.insert(roleId, device);
                    return rec;
                }
            }
        }
    }
    return QString();
}
QString DeviceManager::currentArchId()
{
    // Normalize to one of: x86_64, x86_32, arm64, arm32
    const QString cpu = QSysInfo::currentCpuArchitecture().toLower();
    if (cpu.contains("x86_64") || cpu.contains("amd64") || cpu.contains("x64")) return QStringLiteral("x86_64");
    if (cpu.contains("i386") || cpu.contains("i686") || cpu == QLatin1String("x86") || cpu.contains("x86_32")) return QStringLiteral("x86_32");
    if (cpu.contains("aarch64") || cpu.contains("arm64")) return QStringLiteral("arm64");
    if (cpu == QLatin1String("arm") || cpu.contains("armv7") || cpu.contains("armv8") || cpu.contains("arm32")) return QStringLiteral("arm32");

    // Fallback: assume 64-bit x86 if pointer size is 8, otherwise 32-bit x86
    if (sizeof(void *) == 8) return QStringLiteral("x86_64");
    return QStringLiteral("x86_32");
}

QString DeviceManager::currentOsId()
{
#if defined(Q_OS_WIN)
    return isWindows7Or8Family() ? QStringLiteral("win7") : QStringLiteral("win");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#elif defined(Q_OS_MAC)
    return QStringLiteral("mac");
#else
    // Fallback to QSysInfo productType for uncommon OSes
    const QString t = QSysInfo::productType().toLower();
    if (t.contains("win")) return QStringLiteral("win");
    if (t.contains("linux")) return QStringLiteral("linux");
    if (t.contains("osx") || t.contains("mac")) return QStringLiteral("mac");
    return QStringLiteral("linux"); // default to linux-like
#endif
}
QString DeviceManager::projectForProgram(const QString &name)
{
    const QString key = name.trimmed().toLower();
    // Minimal mapping for known third-party projects; extend as new tools are added
    if (key == QLatin1String("llama-server") || key == QLatin1String("llama-server-main") || key == QLatin1String("llama-server-embed"))
        return QStringLiteral("llama.cpp");
    if (key == QLatin1String("whisper-cli")) return QStringLiteral("whisper.cpp");
    if (key == QLatin1String("llama-quantize")) return QStringLiteral("llama.cpp");
    if (key == QLatin1String("llama-tts")) return QStringLiteral("llama-tts");
    if (key == QLatin1String("tts-cli")) return QStringLiteral("tts.cpp");
    // default to using the name itself as folder (best-effort)
    return key;
}
QString DeviceManager::lastResolvedDeviceFor(const QString &programName)
{
    const QString roleId = descriptorFor(programName).roleId;
    const QString d = g_lastResolvedDevice.value(roleId);
    if (!d.isEmpty()) return d;
    return effectiveBackend();
}
