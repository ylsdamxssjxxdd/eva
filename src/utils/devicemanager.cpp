// DeviceManager - runtime backend discovery and path resolution
#include "devicemanager.h"

#include <QCoreApplication>
#include <QDir>
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
    return appDirPath + "/usr/bin";
#else
    return QCoreApplication::applicationDirPath();
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
    const QStringList candidates = preferredOrder();
    for (const QString &b : candidates)
    {
        const QString dir = QDir(root).filePath(b);
        const QString probe = QDir(dir).filePath(QStringLiteral("llama-server") + QStringLiteral(SFX_NAME));
        if (QFileInfo::exists(probe))
        {
            out.push_back(b);
        }
    }
    // Always ensure CPU shows up as the lowest fallback option
    if (!out.contains(QStringLiteral("cpu")))
    {
        const QString cpuDir = QDir(root).filePath(QStringLiteral("cpu"));
        const QString probe = QDir(cpuDir).filePath(QStringLiteral("llama-server") + QStringLiteral(SFX_NAME));
        if (QFileInfo::exists(probe))
        {
            out.push_back(QStringLiteral("cpu"));
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
        for (const QString &b : preferredOrder())
        {
            if (avail.contains(b)) return b;
        }
        return QStringLiteral("cpu");
    }
    // If explicit choice exists, honor it; otherwise fall back to auto strategy
    if (avail.contains(g_userChoice)) return g_userChoice;
    for (const QString &b : preferredOrder())
    {
        if (avail.contains(b)) return b;
    }
    return QStringLiteral("cpu");
}

QString DeviceManager::programPath(const QString &name)
{
    const QString root = backendsRootDir();
    const QString backend = effectiveBackend();
    const QString dir = QDir(root).filePath(backend);
    const QString exe = name + QStringLiteral(SFX_NAME);
    return QDir(dir).filePath(exe);
}

