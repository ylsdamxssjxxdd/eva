// DeviceManager - runtime backend discovery and path resolution
#include "devicemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSysInfo>

static QString g_userChoice = QStringLiteral("auto"); // process-local selection
static QStringList g_lastProbed;                      // debug: last probed paths

QString DeviceManager::backendsRootDir()
{
#ifdef BODY_LINUX_PACK
    const QStringList roots = candidateBackendRoots();
    if (!roots.isEmpty()) return roots.first();
    // Fall back to next-to-exe path even if not existing
    return QCoreApplication::applicationDirPath() + "/EVA_BACKEND";
#else
    // Default: expect EVA_BACKEND next to the executable
    return QCoreApplication::applicationDirPath() + "/EVA_BACKEND";
#endif
}

QStringList DeviceManager::candidateBackendRoots()
{
    QStringList probes; // include non-existing for diagnostics

    // 0) Explicit override via env (power users/packagers)
    const QString envRoot = QString::fromLocal8Bit(qgetenv("EVA_BACKEND_ROOT"));
    if (!envRoot.isEmpty()) probes << envRoot;

#ifdef BODY_LINUX_PACK
    // 1) AppImage internal mount (packaged)
    const QString appDir = QString::fromLocal8Bit(qgetenv("APPDIR"));
    if (!appDir.isEmpty())
    {
        probes << QDir(appDir).filePath("usr/bin/EVA_BACKEND");
        probes << QDir(appDir).filePath("EVA_BACKEND");
    }

    // 2) Next to the running executable (inside mount in AppImage case)
    const QString exeDir = QCoreApplication::applicationDirPath();
    probes << QDir(exeDir).filePath("EVA_BACKEND");

    // 3) If exe under /usr/bin, try resolving root of mount
    QDir maybeUsrBin(exeDir);
    if (maybeUsrBin.path().endsWith("/usr/bin"))
    {
        maybeUsrBin.cdUp(); // usr
        maybeUsrBin.cdUp(); // APPDIR
        probes << maybeUsrBin.filePath("usr/bin/EVA_BACKEND");
        probes << maybeUsrBin.filePath("EVA_BACKEND");
    }

    // 4) External folder next to the .AppImage file
    const QString appImage = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
    if (!appImage.isEmpty())
    {
        const QString outer = QFileInfo(appImage).absoluteDir().filePath("EVA_BACKEND");
        probes << outer;
    }

    // 5) CWD fallback for developers launching from a folder
    probes << QDir::currentPath() + "/EVA_BACKEND";

#else
    // Non-linux builds: next to the executable and CWD
    const QString exeDir = QCoreApplication::applicationDirPath();
    probes << QDir(exeDir).filePath("EVA_BACKEND");
    probes << QDir::currentPath() + "/EVA_BACKEND";
#endif

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
QStringList DeviceManager::preferredOrder()
{
    // Best-effort preference. Do not query hardware here; we only reflect what was shipped.
    return {QStringLiteral("cuda"), QStringLiteral("vulkan"), QStringLiteral("opencl"), QStringLiteral("cpu")};
}

QStringList DeviceManager::availableBackends()
{
    // Enumerate device folders under each candidate root.
    const QStringList roots = candidateBackendRoots();
    const QString arch = currentArchId();
    const QString os = currentOsId();
    QStringList out;
    for (const QString &root : roots)
    {
        const QString osDir = QDir(QDir(root).filePath(arch)).filePath(os);
        QDir d(osDir);
        if (!d.exists()) continue;
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
    if (b == QLatin1String("auto") || b == QLatin1String("cpu") || b == QLatin1String("cuda") || b == QLatin1String("vulkan") || b == QLatin1String("opencl"))
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
    if (g_userChoice == QLatin1String("auto"))
    {
        // Prefer known accelerators if present, otherwise first available folder
        for (const QString &b : preferredOrder())
        {
            if (avail.contains(b)) return b;
        }
        if (!avail.isEmpty()) return avail.first();
        return QStringLiteral("cpu");
    }
    // If explicit choice exists, honor it; otherwise fall back to auto strategy
    if (avail.contains(g_userChoice)) return g_userChoice;
    for (const QString &b : preferredOrder())
    {
        if (avail.contains(b)) return b;
    }
    if (!avail.isEmpty()) return avail.first();
    return QStringLiteral("cpu");
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
    const QStringList roots = candidateBackendRoots();
    const QString arch = currentArchId();
    const QString os = currentOsId();
    const QString device = effectiveBackend();
    const QString project = projectForProgram(name);
    const QString exe = name + QStringLiteral(SFX_NAME);

    for (const QString &root : roots)
    {
        const QString projDirNew = QDir(QDir(QDir(QDir(root).filePath(arch)).filePath(os)).filePath(device)).filePath(project);
        if (QFileInfo::exists(projDirNew))
        {
            const QString direct = QDir(projDirNew).filePath(exe);
            if (QFileInfo::exists(direct)) return direct;
            const QString rec = findProgramRecursive(projDirNew, exe);
            if (!rec.isEmpty()) return rec;
        }
        const QString devDirNew = QDir(QDir(QDir(root).filePath(arch)).filePath(os)).filePath(device);
        if (QFileInfo::exists(devDirNew))
        {
            const QString rec = findProgramRecursive(devDirNew, exe);
            if (!rec.isEmpty()) return rec;
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
    return QStringLiteral("win");
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
}QString DeviceManager::projectForProgram(const QString &name)
{
    // Minimal mapping for known third-party projects; extend as new tools are added
    if (name == QLatin1String("llama-server")) return QStringLiteral("llama.cpp");
    if (name == QLatin1String("whisper-cli")) return QStringLiteral("whisper.cpp");
    if (name == QLatin1String("llama-quantize")) return QStringLiteral("llama.cpp");
    if (name == QLatin1String("llama-tts")) return QStringLiteral("llama-tts");
    // default to using the name itself as folder (best-effort)
    return name;
}






