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
    return appDirPath + "/usr/bin/backend";
#else
    return QCoreApplication::applicationDirPath() + "/backend";
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
        if (QFileInfo::exists(dir) && QFileInfo(dir).isDir())
        {
            out.push_back(b);
        }
    }
    // Always ensure CPU appears if a cpu folder exists; otherwise do not synthesize it
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
    const QString backendDir = QDir(root).filePath(backend);
    const QString exe = name + QStringLiteral(SFX_NAME);

    // 1) New layout: bin/backend/<device>/<project>/<exe>
    // Try project-specific preferred order for well-known tools
    QStringList prefProjects;
    if (name == QLatin1String("llama-server") || name == QLatin1String("llama-quantize") || name == QLatin1String("llama-tts"))
        prefProjects << QStringLiteral("llama.cpp");
    else if (name == QLatin1String("whisper-cli"))
        prefProjects << QStringLiteral("whisper.cpp");
    else if (name == QLatin1String("sd"))
        prefProjects << QStringLiteral("stable-diffusion.cpp");

    for (const QString &proj : prefProjects)
    {
        const QString candidate = QDir(QDir(backendDir).filePath(proj)).filePath(exe);
        if (QFileInfo::exists(candidate)) return candidate;
    }
    // Fallback: search any project subfolder one level deep
    QDir d(backendDir);
    const QFileInfoList subs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : subs)
    {
        const QString candidate = QDir(fi.absoluteFilePath()).filePath(exe);
        if (QFileInfo::exists(candidate)) return candidate;
    }

    // 2) Legacy layout: bin/<device>/<exe>
    const QString legacy = QDir(backendDir).filePath(exe);
    if (QFileInfo::exists(legacy)) return legacy;

    // 3) Last resort: alongside application (useful for dev without staging)
    const QString beside = QDir(QCoreApplication::applicationDirPath()).filePath(exe);
    return beside;
}


