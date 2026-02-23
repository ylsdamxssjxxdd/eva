#include "app_bootstrap.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QResource>
#include <QStandardPaths>
#include <QtGlobal>

#include "default_model_finder.h"
#include "utils/devicemanager.h"
#include "xconfig.h"

void AppBootstrap::applyEarlyEnv()
{
    // 兼容老旧 CPU：禁用 Qt PCRE2 JIT，避免 SIGILL。
    qputenv("QT_DISABLE_REGEXP_JIT", QByteArray("1"));
}

void AppBootstrap::applyLinuxRuntimeEnv()
{
// 设置linux下动态库的默认路径
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR"); // 获取镜像的路径
    QString ldLibraryPath = appDirPath + "/usr/lib";
    std::string currentPath = ldLibraryPath.toLocal8Bit().constData();
    setenv("LD_LIBRARY_PATH", currentPath.c_str(), 1); // 指定找动态库的默认路径 LD_LIBRARY_PATH
#endif

#if defined(Q_OS_LINUX) && defined(EVA_LINUX_STATIC_BUILD)
    qputenv("QT_IM_MODULE", QByteArray("fcitx"));
    qputenv("XMODIFIERS", QByteArray("@im=fcitx"));
    qputenv("GTK_IM_MODULE", QByteArray("fcitx"));
#endif
}

AppContext AppBootstrap::buildContext()
{
    AppContext ctx;
#ifdef BODY_LINUX_PACK
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString appImagePath = env.value("APPIMAGE");
    const QFileInfo fileInfo(appImagePath);
    ctx.appDir = fileInfo.absolutePath(); // 在打包程序运行时所在目录创建EVA_TEMP文件夹
    ctx.appPath = appImagePath;          // .AppImage所在路径
#else
    ctx.appDir = QCoreApplication::applicationDirPath(); // 就在当前目录创建EVA_TEMP文件夹
    ctx.appPath = QCoreApplication::applicationFilePath();
#endif

    ctx.tempDir = QDir(ctx.appDir).filePath(EVA_TEMP_DIR_RELATIVE);
    ctx.modelsDir = QDir(ctx.appDir).filePath("EVA_MODELS");
    ctx.backendDir = QDir(ctx.appDir).filePath("EVA_BACKEND");
    ctx.skillsDir = QDir(ctx.appDir).filePath(EVA_SKILLS_DIR_RELATIVE);
    ctx.workDir = QDir(ctx.appDir).filePath("EVA_WORK");

    // 后端探测信息快照（用于日志）
    snapshotBackendProbe(ctx);
    return ctx;
}

void AppBootstrap::ensureTempDir(const AppContext &ctx)
{
    QDir().mkpath(ctx.tempDir);
}

void AppBootstrap::registerSarasaFontResource(const QString &runtimeDir)
{
    QDir dir(runtimeDir);
    QStringList candidates;
    candidates << dir.filePath(QStringLiteral("font_out.rcc"));
    candidates << dir.filePath(QStringLiteral("../font_out.rcc"));
    candidates << dir.filePath(QStringLiteral("resources/font_out.rcc"));
    candidates << dir.filePath(QStringLiteral("../resources/font_out.rcc"));
    candidates << dir.filePath(QStringLiteral("EVA_RES/font_out.rcc"));
    for (const QString &candidate : candidates)
    {
        QFileInfo info(candidate);
        if (!info.exists()) continue;
        if (QResource::registerResource(info.absoluteFilePath()))
        {
            qInfo() << "Registered Sarasa font resource from" << info.absoluteFilePath();
        }
        else
        {
            qWarning() << "Failed to register Sarasa font resource at" << info.absoluteFilePath();
        }
        return;
    }
    qWarning() << "font_out.rcc not found; Sarasa font resource unavailable.";
}

void AppBootstrap::createLinuxDesktopShortcut(const QString &appPath)
{
#ifdef Q_OS_LINUX
    // Prepare icon path and copy resource icon to user dir
    const QString iconDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/";
    QDir().mkpath(iconDir);
    const QString iconPath = iconDir + "eva.png";
    QFile::copy(":/logo/eva.png", iconPath);
    QFile::setPermissions(iconPath, QFile::ReadOwner | QFile::WriteOwner);

    // Compose .desktop content using Qt placeholders (%1, %2, %3)
    const QString desktopContent = QStringLiteral(
                                       "[Desktop Entry]\n"
                                       "Type=Application\n"
                                       "Name=%1\n"
                                       "Comment=a lite llm tool\n"
                                       "Exec=\"%2\"\n"
                                       "Icon=%3\n"
                                       "Terminal=false\n"
                                       "Categories=Utility;\n")
                                       .arg(QStringLiteral("eva"), appPath, iconPath);

    // Write to ~/.local/share/applications/eva.desktop
    const QString applicationsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/";
    QDir().mkpath(applicationsDir);
    QFile applicationsFile(applicationsDir + "eva.desktop");
    if (applicationsFile.open(QIODevice::WriteOnly))
    {
        applicationsFile.write(desktopContent.toUtf8());
        applicationsFile.close();
        applicationsFile.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
    }
    else
    {
        qWarning() << "Failed to write applications desktop file";
    }

    // Write to ~/Desktop/eva.desktop
    const QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/";
    QFile desktopFile(desktopDir + "eva.desktop");
    if (desktopFile.open(QIODevice::WriteOnly))
    {
        desktopFile.write(desktopContent.toUtf8());
        desktopFile.close();
        desktopFile.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
    }
    else
    {
        qWarning() << "Failed to write desktop shortcut";
    }
#else
    Q_UNUSED(appPath);
#endif
}

QString AppBootstrap::loadStylesheet()
{
    QFile file(":/QSS/theme_unit01.qss");
    if (!file.open(QFile::ReadOnly))
        return QString();
    const QString stylesheet = file.readAll();
    file.close();
    return stylesheet;
}

void AppBootstrap::ensureDefaultConfig(const AppContext &ctx)
{
    QDir().mkpath(ctx.tempDir);
    const QString cfgPath = QDir(ctx.tempDir).filePath(QStringLiteral("eva_config.ini"));
    if (QFile::exists(cfgPath))
        return;

    const QString modelsRoot = ctx.modelsDir;
    DefaultModelPaths paths = DefaultModelFinder::discover(modelsRoot);

    QSettings s(cfgPath, QSettings::IniFormat);
    s.setIniCodec("utf-8");
    DefaultModelFinder::applyToSettings(s, paths);

    // Default to local mode and auto device backend on first boot
    s.setValue("ui_mode", 0);
    s.setValue("device_backend", DeviceManager::userChoice().isEmpty() ? "auto" : DeviceManager::userChoice());
    s.sync();
}

void AppBootstrap::snapshotBackendProbe(AppContext &ctx)
{
    ctx.archId = DeviceManager::currentArchId();
    ctx.osId = DeviceManager::currentOsId();
    ctx.deviceChoice = DeviceManager::userChoice();
    ctx.effectiveBackend = DeviceManager::effectiveBackend();
    ctx.resolvedDevice = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main"));
    ctx.backendRoots = DeviceManager::candidateBackendRoots();
    ctx.backendProbed = DeviceManager::probedBackendRoots();
    ctx.backendAvailable = DeviceManager::availableBackends();
}
