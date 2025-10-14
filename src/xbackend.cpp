#include "xbackend.h"
#include "utils/devicemanager.h"
#include "utils/pathutil.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextCodec>

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
        args << "-m" << ensureToolFriendlyFilePath(modelpath_);
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
        args << "--lora" << ensureToolFriendlyFilePath(lora_);
    }
    else
    {
        if (!settings_.hid_use_mmap) args << "--no-mmap";
    }
    if (!mmproj_.isEmpty())
    {
        args << "--mmproj" << ensureToolFriendlyFilePath(mmproj_);
    }
    if (!settings_.hid_flash_attn)
    {
        args << "-fa" << "off";
    }
    if (settings_.hid_use_mlock)
    {
        args << "--mlock";
    }
    return args;
}

void LocalServerManager::hookProcessSignals()
{
    if (!proc_) return;

    connect(proc_, &QProcess::started, this, [this]()
            {
                // emit serverState("ui:backend starting", SIGNAL_SIGNAL);
            });
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus)
            {
        // emit serverState("ui:backend stopped", SIGNAL_SIGNAL);
        emit serverStopped(); });
    connect(proc_, &QProcess::readyReadStandardOutput, this, [this]()
            {
        const QString out = QString::fromUtf8(proc_->readAllStandardOutput());
        if (!out.isEmpty()) emit serverOutput(out);
        if (out.contains(SERVER_START) || out.contains("listening at") || out.contains("listening on"))
        {
            // emit serverState("ui:backend ready", SUCCESS_SIGNAL);
            emit serverReady(endpointBase());
        } });
    connect(proc_, &QProcess::readyReadStandardError, this, [this]()
            {
        const QString err = QString::fromUtf8(proc_->readAllStandardError());
        if (!err.isEmpty()) emit serverOutput(err);
        // llama.cpp may print slightly different phrases across versions
        if (err.contains(SERVER_START) || err.contains("listening at") || err.contains("listening on"))
        {
            // emit serverState("ui:backend ready", SUCCESS_SIGNAL);
            emit serverReady(endpointBase());
        } }); // Report process errors immediately so UI can recover
    connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e)
            {
        QString msg;
        switch (e) {
        case QProcess::FailedToStart: msg = QStringLiteral("ui:backend failed to start"); break;
        case QProcess::Timedout: msg = QStringLiteral("ui:backend start timed out"); break;
        case QProcess::WriteError: msg = QStringLiteral("ui:backend write error"); break;
        case QProcess::ReadError: msg = QStringLiteral("ui:backend read error"); break;
        default: msg = QStringLiteral(""); break;
        }
        if(msg!="") {emit serverState(msg, WRONG_SIGNAL);}
        emit serverOutput(msg);
        if(!msg.isEmpty()) emit serverStartFailed(msg);
        emit serverStopped(); });
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
    // Ensure program-local runtime deps can be found by the child process
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString toolDir = QFileInfo(prog).absolutePath();
#ifdef _WIN32
    env.insert("PATH", toolDir + ";" + env.value("PATH"));
#elif __APPLE__
    env.insert("DYLD_LIBRARY_PATH", toolDir + ":" + env.value("DYLD_LIBRARY_PATH"));
#else
    env.insert("LD_LIBRARY_PATH", toolDir + ":" + env.value("LD_LIBRARY_PATH"));
#endif
    proc_->setProcessEnvironment(env);
    proc_->setWorkingDirectory(toolDir);
    proc_->start(prog, args);
}

void LocalServerManager::ensureRunning()
{
    const QString prog = programPath();
    const QStringList args = buildArgs();
    // Validate executable presence
    if (prog.isEmpty() || !QFileInfo::exists(prog))
    {
        const QString eb = DeviceManager::effectiveBackend();
        const QString root = DeviceManager::backendsRootDir();
        const QString arch = DeviceManager::currentArchId();
        const QString os = DeviceManager::currentOsId();
        const QStringList searched = DeviceManager::probedBackendRoots();
        const QString msg = QStringLiteral("ui:backend executable not found (%1) for device '%2' (root=%3, arch=%4, os=%5)")
                                .arg(QStringLiteral("llama-server"), eb, root, arch, os);
        QString hint;
        if (!searched.isEmpty())
        {
            hint = QStringLiteral("searched: ") + searched.join("; ");
        }
        emit serverState(msg, WRONG_SIGNAL);
        emit serverOutput(msg + (hint.isEmpty() ? QStringLiteral("\n") : QStringLiteral("\n") + hint + QStringLiteral("\n")));
        emit serverStartFailed(msg);
        return;
    }
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
    const QString prog = programPath();
    const QStringList args = buildArgs();
    if (prog.isEmpty() || !QFileInfo::exists(prog))
    {
        const QString eb = DeviceManager::effectiveBackend();
        const QString root = DeviceManager::backendsRootDir();
        const QString arch = DeviceManager::currentArchId();
        const QString os = DeviceManager::currentOsId();
        const QStringList searched = DeviceManager::probedBackendRoots();
        const QString msg = QStringLiteral("ui:backend executable not found (%1) for device '%2' (root=%3, arch=%4, os=%5)")
                                .arg(QStringLiteral("llama-server"), eb, root, arch, os);
        QString hint;
        if (!searched.isEmpty())
        {
            hint = QStringLiteral("searched: ") + searched.join("; ");
        }
        emit serverState(msg, WRONG_SIGNAL);
        emit serverOutput(msg + (hint.isEmpty() ? QStringLiteral("\n") : QStringLiteral("\n") + hint + QStringLiteral("\n")));
        emit serverStartFailed(msg);
        return;
    }
    if (isRunning())
    {
        proc_->kill(); // fast stop is fine here
        proc_->waitForFinished(2000);
    }
    startProcess(args);
}

void LocalServerManager::stop()
{
    if (!proc_) return;

    if (proc_->state() == QProcess::Running)
    {
        // Try graceful termination first
        proc_->terminate();
        if (!proc_->waitForFinished(1500))
        {
#ifdef _WIN32
            // Fallback: force kill by PID and its child processes
            const qint64 pid = proc_->processId();
            if (pid > 0)
            {
                QStringList args;
                args << "/PID" << QString::number(pid) << "/T" << "/F"; // kill tree, force
                QProcess::execute("taskkill", args);
            }
            else
            {
                proc_->kill();
            }
#else
            proc_->kill();
#endif
            proc_->waitForFinished(1000);
        }
    }
    // Ensure object cleanup; do not keep a dangling QPointer
    proc_->deleteLater();
    proc_.clear();
}

void LocalServerManager::stopAsync()
{
    if (!proc_)
    {
        emit serverStopped();
        return;
    }
    // If not running, just cleanup and emit
    if (proc_->state() != QProcess::Running)
    {
        proc_->deleteLater();
        proc_.clear();
        emit serverStopped();
        return;
    }

    // Request graceful termination on our owning thread (UI thread)
    proc_->terminate();

    // When it finishes, clean up and notify
    connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus)
            {
        if (proc_)
        {
            proc_->deleteLater();
            proc_.clear();
        }
        emit serverStopped(); });

    // Guard: force kill if it doesn't exit in time; avoid blocking UI
    QTimer::singleShot(1500, this, [this]()
                       {
        if (!proc_) return;
        if (proc_->state() == QProcess::Running)
        {
#ifdef _WIN32
            const qint64 pid = proc_->processId();
            if (pid > 0)
            {
                QStringList args;
                args << "/PID" << QString::number(pid) << "/T" << "/F";
                QProcess::execute("taskkill", args);
            }
            else
            {
                proc_->kill();
            }
#else
            proc_->kill();
#endif
        } });
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
