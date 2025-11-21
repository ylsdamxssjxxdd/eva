// DeviceManager - runtime backend discovery and path resolution
#include "devicemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QLibrary>
#include <QOperatingSystemVersion>
#include <QProcessEnvironment>
#include <QSet>
#include <QSysInfo>

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

// --- Runtime capability probes (very lightweight; best-effort) ---
static bool supportsCuda()
{
#if defined(Q_OS_WIN)
    QLibrary lib(QStringLiteral("nvcuda"));
    const bool ok = lib.load();
    if (ok) lib.unload();
    return ok;
#elif defined(Q_OS_LINUX)
    QLibrary lib(QStringLiteral("libcuda.so.1"));
    if (lib.load())
    {
        lib.unload();
        return true;
    }
    // Fallback heuristics
    return QFileInfo::exists("/proc/driver/nvidia/version") || QFileInfo::exists("/dev/nvidiactl");
#else
    return false; // no CUDA on macOS and others
#endif
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
            add(QStringLiteral("cpu"));
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
        add(QStringLiteral("cpu"));
    }
    return order;
}
QStringList DeviceManager::preferredOrder()
{
    const QString arch = currentArchId();
    const bool cpuFirst = arch.startsWith(QLatin1String("arm")) || isWindows7Or8Family();
    if (cpuFirst)
    {
        return {QStringLiteral("cpu"), QStringLiteral("cuda"), QStringLiteral("vulkan"), QStringLiteral("opencl")};
    }
    // Best-effort preference. Do not query hardware here; we only reflect what was shipped.
    return {QStringLiteral("cuda"), QStringLiteral("vulkan"), QStringLiteral("opencl"), QStringLiteral("cpu")};
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
    if (b == QLatin1String("auto") || b == QLatin1String("custom") || b == QLatin1String("cpu") || b == QLatin1String("cuda") || b == QLatin1String("vulkan") || b == QLatin1String("opencl"))
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
    // Explicit choice: honor if available and supported; else degrade
    if (choice == QLatin1String("cuda") && supportsCuda() && isAvail("cuda")) return QStringLiteral("cuda");
    if (choice == QLatin1String("vulkan") && supportsVulkan() && isAvail("vulkan")) return QStringLiteral("vulkan");
    if (choice == QLatin1String("opencl") && supportsOpenCL() && isAvail("opencl")) return QStringLiteral("opencl");
    if (choice == QLatin1String("cpu") && isAvail("cpu")) return QStringLiteral("cpu");
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
    if (choice == QLatin1String("cuda") && supportsCuda() && isAvail("cuda")) return QStringLiteral("cuda");
    if (choice == QLatin1String("vulkan") && supportsVulkan() && isAvail("vulkan")) return QStringLiteral("vulkan");
    if (choice == QLatin1String("opencl") && supportsOpenCL() && isAvail("opencl")) return QStringLiteral("opencl");
    if (choice == QLatin1String("cpu") && isAvail("cpu")) return QStringLiteral("cpu");
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
                    g_lastResolvedDevice.insert(roleId, device);
                    return direct;
                }
                const QString rec = findProgramRecursive(projDirNew, exe);
                if (!rec.isEmpty())
                {
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
