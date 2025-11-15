#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>
#include <QTemporaryDir>

#include "utils/devicemanager.h"
#include "xconfig.h"

namespace
{
QCoreApplication *ensureQtApp()
{
    static int argc = 0;
    static char **argv = nullptr;
    static QCoreApplication app(argc, argv);
    return &app;
}

class ScopedEnvVar
{
  public:
    ScopedEnvVar(const char *name, const QString &value)
        : name_(name), previous_(qgetenv(name)), wasSet_(qEnvironmentVariableIsSet(name))
    {
        qputenv(name_.constData(), value.toUtf8());
    }
    ~ScopedEnvVar()
    {
        if (wasSet_)
            qputenv(name_.constData(), previous_);
        else
            qputenv(name_.constData(), QByteArray());
    }

  private:
    QByteArray name_;
    QByteArray previous_;
    bool wasSet_;
};

class ScopedDeviceChoice
{
  public:
    ScopedDeviceChoice() : previous_(DeviceManager::userChoice()) {}
    ~ScopedDeviceChoice() { DeviceManager::setUserChoice(previous_); }

  private:
    QString previous_;
};

QString prepareBackendTree(QTemporaryDir &dir, const QString &deviceName)
{
    const QString root = dir.filePath(QStringLiteral("EVA_BACKEND"));
    const QString arch = DeviceManager::currentArchId();
    const QString os = DeviceManager::currentOsId();
    const QString project = QStringLiteral("llama.cpp");

    QDir().mkpath(root);
    QDir archDir(QDir(root).filePath(arch));
    QDir().mkpath(archDir.path());
    QDir osDir(archDir.filePath(os));
    QDir().mkpath(osDir.path());
    QDir deviceDir(osDir.filePath(deviceName));
    QDir().mkpath(deviceDir.path());
    QDir projectDir(deviceDir.filePath(project));
    QDir().mkpath(projectDir.path());

    const QString exePath = projectDir.filePath(QStringLiteral("llama-server") + QStringLiteral(SFX_NAME));
    QFile exe(exePath);
    if (!exe.exists())
    {
        exe.open(QIODevice::WriteOnly | QIODevice::Truncate);
        exe.write("eva");
        exe.close();
    }
    return root;
}
} // namespace

TEST_CASE("DeviceManager resolves cpu backend executables from EVA_BACKEND_ROOT")
{
    ensureQtApp();
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString backendRoot = prepareBackendTree(tempDir, QStringLiteral("cpu"));
    ScopedEnvVar env("EVA_BACKEND_ROOT", backendRoot);
    ScopedDeviceChoice choiceGuard;
    DeviceManager::setUserChoice(QStringLiteral("cpu"));

    const QString exePath = DeviceManager::programPath(QStringLiteral("llama-server"));
    REQUIRE_FALSE(exePath.isEmpty());
    CHECK(QFileInfo::exists(exePath));

    const QStringList available = DeviceManager::availableBackends();
    CHECK(available.contains(QStringLiteral("cpu")));
}

TEST_CASE("DeviceManager enumerates all backend folders under the configured root")
{
    ensureQtApp();
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString backendRoot = prepareBackendTree(tempDir, QStringLiteral("cpu"));
    // Create an additional synthetic backend to ensure enumeration sees it
    prepareBackendTree(tempDir, QStringLiteral("opencl"));
    ScopedEnvVar env("EVA_BACKEND_ROOT", backendRoot);

    const QStringList available = DeviceManager::availableBackends();
    CHECK(available.contains(QStringLiteral("cpu")));
    CHECK(available.contains(QStringLiteral("opencl")));

    const QString cpuExplicit = DeviceManager::effectiveBackendFor(QStringLiteral("cpu"));
    CHECK(cpuExplicit == QStringLiteral("cpu"));
}
