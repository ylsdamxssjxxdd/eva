// DeviceManager - runtime backend discovery and path resolution
#include "devicemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcessEnvironment>

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
    const QString root = backendsRootDir();
    QStringList out;
    QDir d(root);
    if (!d.exists()) return out;
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
    const QString root = backendsRootDir();
    const QString backend = effectiveBackend();
    const QString backendDir = QDir(root).filePath(backend);
    const QString exe = name + QStringLiteral(SFX_NAME);

    // Search recursively under backend/<device>/ for the executable
    if (QFileInfo::exists(backendDir))
    {
        // 1) direct file in backend dir
        const QString direct = QDir(backendDir).filePath(exe);
        if (QFileInfo::exists(direct)) return direct;
        // 2) any subfolder
        const QString rec = findProgramRecursive(backendDir, exe);
        if (!rec.isEmpty()) return rec;
    }

    // Not found in backend device dir; do not guess arbitrary locations
    return QString();
}








