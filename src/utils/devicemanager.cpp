// DeviceManager - runtime backend discovery and path resolution
#include "devicemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSysInfo>

static QString g_userChoice = QStringLiteral("auto"); // process-local selection

QString DeviceManager::backendsRootDir()
{
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    if (appDirPath.isEmpty())
    {
        appDirPath = QCoreApplication::applicationDirPath();
    }
    return appDirPath + "/usr/bin/EVA_BACKEND";
#else
    return QCoreApplication::applicationDirPath() + "/EVA_BACKEND";
#endif
}

QStringList DeviceManager::preferredOrder()
{
    // Best-effort preference. Do not query hardware here; we only reflect what was shipped.
    return {QStringLiteral("cuda"), QStringLiteral("vulkan"), QStringLiteral("opencl"), QStringLiteral("cpu")};
}

QStringList DeviceManager::availableBackends()
{
    // Enumerate device folders under EVA_BACKEND/<arch>/<os>/ (no legacy fallback)
    const QString root = backendsRootDir();
    const QString arch = currentArchId();
    const QString os = currentOsId();
    QStringList out;

    const QString osDir = QDir(QDir(root).filePath(arch)).filePath(os);
    QDir d(osDir);
    if (!d.exists()) return out; // no fallback, require new layout

    const QFileInfoList subs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &fi : subs)
    {
        out << fi.fileName();
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
    const QString root = backendsRootDir();
    const QString arch = currentArchId();
    const QString os = currentOsId();
    const QString device = effectiveBackend();
    const QString project = projectForProgram(name);
    const QString exe = name + QStringLiteral(SFX_NAME);

    // 1) strict path within project folder (new layout only)
    const QString projDirNew = QDir(QDir(QDir(QDir(root).filePath(arch)).filePath(os)).filePath(device)).filePath(project);
    if (QFileInfo::exists(projDirNew))
    {
        // try direct and recursive within project
        const QString direct = QDir(projDirNew).filePath(exe);
        if (QFileInfo::exists(direct)) return direct;
        const QString rec = findProgramRecursive(projDirNew, exe);
        if (!rec.isEmpty()) return rec;
    }

    // 2) fallback within new layout: search under device dir
    const QString devDirNew = QDir(QDir(QDir(root).filePath(arch)).filePath(os)).filePath(device);
    if (QFileInfo::exists(devDirNew))
    {
        const QString rec = findProgramRecursive(devDirNew, exe);
        if (!rec.isEmpty()) return rec;
    }

    // Not found under new layout; no legacy fallback
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
    // default to using the name itself as folder (best-effort)
    return name;
}





