#include "xbackend.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextCodec>
#include "utils/devicemanager.h"

LocalServerManager::LocalServerManager(QObject *parent, const QString &appDirPath)
    : QObject(parent), appDirPath_(appDirPath) {}

LocalServerManager::~LocalServerManager()
{
    stop();
}

void LocalServerManager::setSettings(const SETTINGS &s)
{
    settings_ = s;
}
void LocalServerManager::setPort(const QString &port)
{
    port_ = port;
}
void LocalServerManager::setModelPath(const QString &path)
{
    modelpath_ = path;
}
void LocalServerManager::setMmprojPath(const QString &path)
{
    mmproj_ = path;
}
void LocalServerManager::setLoraPath(const QString &path)
{
    lora_ = path;
}

QString LocalServerManager::endpointBase() const
{
    return QString("http://127.0.0.1:%1").arg(port_);
}

bool LocalServerManager::isRunning() const
{
    return proc_ && proc_->state() == QProcess::Running;
}

QString LocalServerManager::programPath() const
{
    Q_UNUSED(appDirPath_);
    // Resolve per selected backend (auto/cpu/cuda/vulkan/opencl)
    return DeviceManager::programPath(QStringLiteral("llama-server"));
}

QStringList LocalServerManager::buildArgs() const
{
    QStringList args;
    if (!modelpath_.isEmpty())
    {
        args << "-m" << modelpath_;
    }
    args << "--host"
         << host_;
    args << "--port" << port_;
    args << "-c" << QString::number(settings_.nctx);
    // 仅在 GPU 型后端下传递 -ngl；CPU 后端无此选项意义
    if (DeviceManager::effectiveBackend() != QLatin1String("cpu"))
    {
        args << "-ngl" << QString::number(settings_.ngl);
    }
    args << "--threads" << QString::number(settings_.nthread);
    args << "-b" << QString::number(settings_.hid_batch);
    args << "--parallel" << QString::number(settings_.hid_parallel);
    args << "--jinja"; // enable toolcalling prompt templating
    args << "--reasoning-format"
         << "none"; // think tags in content
    args << "--verbose-prompt";

    if (!lora_.isEmpty())
    {
        args << "--no-mmap"; // lora with mmap can be fragile across platforms
        args << "--lora" << lora_;
    }
    else
    {
        if (!settings_.hid_use_mmap) args << "--no-mmap";
    }
    if (!mmproj_.isEmpty())
    {
        args << "--mmproj" << mmproj_;
    }
    if (settings_.hid_flash_attn)
    {
        args << "-fa";
    }
    if (settings_.hid_use_mlock)
    {
        args << "--mlock";
    }
    // Enable slots metadata endpoints and slot saving to disk for future resume
    // This aligns with llama.cpp tools/server capabilities
    const QString slotPath = QDir(appDirPath_).filePath("EVA_TEMP/slots");
    QDir().mkpath(slotPath);
    args << "--slots";                      // enable /slots endpoint
    args << "--slot-save-path" << slotPath; // allow save/restore of KV cache per slot
    return args;
}

void LocalServerManager::hookProcessSignals()
{
    if (!proc_) return;

    connect(proc_, &QProcess::started, this, [this]() {
        emit serverState("ui:backend starting", SIGNAL_SIGNAL);
    });
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        emit serverState("ui:backend stopped", SIGNAL_SIGNAL);
        emit serverStopped();
    });
    connect(proc_, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString out = QString::fromUtf8(proc_->readAllStandardOutput());
        if (!out.isEmpty()) emit serverOutput(out);
        if (out.contains(SERVER_START) || out.contains("listening at") || out.contains("listening on"))
        {
            // emit serverState("ui:backend ready", SUCCESS_SIGNAL);
            emit serverReady(endpointBase());
        }
    });
    connect(proc_, &QProcess::readyReadStandardError, this, [this]() {
        const QString err = QString::fromUtf8(proc_->readAllStandardError());
        if (!err.isEmpty()) emit serverOutput(err);
        // llama.cpp may print slightly different phrases across versions
        if (err.contains(SERVER_START) || err.contains("listening at") || err.contains("listening on"))
        {
            // emit serverState("ui:backend ready", SUCCESS_SIGNAL);
            emit serverReady(endpointBase());
        }
    });
}

void LocalServerManager::startProcess(const QStringList &args)
{
    if (proc_)
    {
        proc_->deleteLater();
        proc_.clear();
    }
    proc_ = new QProcess(this);
    hookProcessSignals();
    const QString prog = programPath();
    lastProgram_ = prog;
    lastArgs_ = args;
    proc_->start(prog, args);
}

void LocalServerManager::ensureRunning()
{
    const QString prog = programPath();
    const QStringList args = buildArgs();
    // If not running -> start; if running with different args -> restart
    if (!isRunning())
    {
        startProcess(args);
        return;
    }
    if (prog != lastProgram_ || args != lastArgs_)
    {
        restart();
    }
}

void LocalServerManager::restart()
{
    const QStringList args = buildArgs();
    if (isRunning())
    {
        proc_->kill(); // fast stop is fine here
        proc_->waitForFinished(2000);
    }
    startProcess(args);
}

void LocalServerManager::stop()
{
    if (proc_)
    {
        proc_->kill();
        proc_->waitForFinished(1000);
        proc_.clear();
    }
}

bool LocalServerManager::needsRestart() const
{
    const QString prog = programPath();
    const QStringList args = buildArgs();
    if (!isRunning()) return true;
    return (prog != lastProgram_) || (args != lastArgs_);
}

void LocalServerManager::setHost(const QString &host)
{
    host_ = host;
}
