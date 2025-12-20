#include "xbackend.h"
#include "utils/devicemanager.h"
#include "utils/flowtracer.h"
#include "utils/startuplogger.h"
#include "xbackend_args.h"
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
    return DeviceManager::programPath(QStringLiteral("llama-server-main"));
}

QStringList LocalServerManager::buildArgs() const
{
    LocalServerArgsInput input;
    input.settings = settings_;
    input.host = host_;
    input.port = port_;
    input.modelPath = modelpath_;
    input.mmprojPath = mmproj_;
    input.loraPath = lora_;
    input.resolvedDevice = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main"));
    input.win7Backend = (DeviceManager::currentOsId() == QStringLiteral("win7"));
    return buildLocalServerArgs(input);
}

void LocalServerManager::emitServerStoppedOnce()
{
    if (stoppedEmitted_) return;
    stoppedEmitted_ = true;
    emit serverStopped();
}

void LocalServerManager::hookProcessSignals()
{
    if (!proc_) return;
    // 注意：LocalServerManager 在重启/回收时会替换 proc_ 指针。
    // 若旧进程在 deleteLater 之前仍然触发信号，必须忽略它们，避免把“旧进程退出”误判成“新进程启动失败”。
    QProcess *p = proc_;

    connect(p, &QProcess::started, this, [this, p]()
            {
                if (p != proc_) return;
                // emit serverState("ui:backend starting", SIGNAL_SIGNAL);
                FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: process started"));
                StartupLogger::log(QStringLiteral("[backend] process started"));
            });
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, p](int exitCode, QProcess::ExitStatus exitStatus)
            {
        if (p != proc_) return;

        // 启动阶段如果没有进入“listening”，但进程却已经退出/崩溃，
        // 则必须视为启动失败并通知 UI（否则装载界面会一直等 serverReady 卡死）。
        if (!readyEmitted_ && !startFailedEmitted_)
        {
            startFailedEmitted_ = true;
            const QString status = (exitStatus == QProcess::CrashExit) ? QStringLiteral("crash") : QStringLiteral("exit");
            QString msg = QStringLiteral("ui:backend exited before ready (exitCode=%1, exitStatus=%2)")
                              .arg(exitCode)
                              .arg(status);
#if defined(_WIN32)
            // Windows: 0xC000001D = STATUS_ILLEGAL_INSTRUCTION（常见于 AVX/AVX2 不支持）
            const quint32 winCode = static_cast<quint32>(exitCode);
            if (exitStatus == QProcess::CrashExit && winCode == 0xC000001D)
            {
                msg += QStringLiteral(" [illegal instruction]");
            }
#endif
            emit serverState(msg, WRONG_SIGNAL);
            emit serverOutput(msg + QStringLiteral("\n"));
            emit serverStartFailed(msg);
            StartupLogger::log(QStringLiteral("[backend] early exit -> start failed: %1").arg(msg));
            FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: early exit (%1)").arg(msg));
        }
        // emit serverState("ui:backend stopped", SIGNAL_SIGNAL);
        emitServerStoppedOnce(); });
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p]()
            {
        if (p != proc_) return;
        const QString out = QString::fromUtf8(p->readAllStandardOutput());
        if (!out.isEmpty()) emit serverOutput(out);
        static int s_outCount = 0;
        if (s_outCount < 6 && !out.isEmpty())
        {
            ++s_outCount;
            StartupLogger::log(QStringLiteral("[backend][stdout %1] %2").arg(s_outCount).arg(out.left(200)));
        }
        if (!readyEmitted_ && (out.contains(SERVER_START) || out.contains("listening at") || out.contains("listening on")))
        {
        // emit serverState("ui:backend ready", SUCCESS_SIGNAL);
        readyEmitted_ = true;
        emit serverReady(endpointBase());
        FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: listening %1").arg(endpointBase()));
        } });
    connect(p, &QProcess::readyReadStandardError, this, [this, p]()
            {
        if (p != proc_) return;
        const QString err = QString::fromUtf8(p->readAllStandardError());
        if (!err.isEmpty()) emit serverOutput(err);
        static int s_errCount = 0;
        if (s_errCount < 6 && !err.isEmpty())
        {
            ++s_errCount;
            StartupLogger::log(QStringLiteral("[backend][stderr %1] %2").arg(s_errCount).arg(err.left(200)));
        }
        // llama.cpp may print slightly different phrases across versions
        if (!readyEmitted_ && (err.contains(SERVER_START) || err.contains("listening at") || err.contains("listening on")))
        {
            // emit serverState("ui:backend ready", SUCCESS_SIGNAL);
            readyEmitted_ = true;
            emit serverReady(endpointBase());
        } }); // Report process errors immediately so UI can recover
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e)
            {
        if (p != proc_) return;
        QString msg;
        switch (e) {
        case QProcess::FailedToStart: msg = QStringLiteral("ui:backend failed to start"); break;
        case QProcess::Crashed: msg = QStringLiteral("ui:backend crashed"); break;
        case QProcess::Timedout: msg = QStringLiteral("ui:backend start timed out"); break;
        case QProcess::WriteError: msg = QStringLiteral("ui:backend write error"); break;
        case QProcess::ReadError: msg = QStringLiteral("ui:backend read error"); break;
        default: msg = QStringLiteral(""); break;
        }
        if(msg!="") {emit serverState(msg, WRONG_SIGNAL);}
        emit serverOutput(msg);
        // 只有在“尚未 ready”时，才把错误当作启动失败；否则交由 serverStopped 处理运行中崩溃。
        if(!readyEmitted_ && !msg.isEmpty() && !startFailedEmitted_)
        {
            startFailedEmitted_ = true;
            emit serverStartFailed(msg);
        }
        emitServerStoppedOnce(); });
}

void LocalServerManager::startProcess(const QStringList &args)
{
    if (proc_)
    {
        proc_->deleteLater();
        proc_.clear();
    }
    proc_ = new QProcess(this);
    // 新进程：重置状态机
    readyEmitted_ = false;
    startFailedEmitted_ = false;
    stoppedEmitted_ = false;
    hookProcessSignals();
    const QString prog = programPath();
    lastProgram_ = prog;
    lastArgs_ = args;
    FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: launch %1").arg(QDir::toNativeSeparators(prog)));
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
    // 启动前记录解析结果，便于复现 Win7/老 CPU 环境的非法指令或找不到后端的问题
    FlowTracer::log(FlowChannel::Backend,
                    QStringLiteral("backend: resolve program=%1 device=%2 args=%3")
                        .arg(QDir::toNativeSeparators(prog),
                             DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main")),
                             args.join(QLatin1Char(' '))));
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
        FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: executable missing (%1)").arg(prog));
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
        FlowTracer::log(FlowChannel::Backend, QStringLiteral("backend: executable missing (%1)").arg(prog));
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
        emitServerStoppedOnce();
        return;
    }
    // If not running, just cleanup and emit
    if (proc_->state() != QProcess::Running)
    {
        proc_->deleteLater();
        proc_.clear();
        emitServerStoppedOnce();
        return;
    }

    // stopAsync() 可能与 UI 的“立即重启/切端口”等流程并发触发：
    // 必须把当前进程指针捕获下来，避免 finished 回调误清理了“新进程”的 proc_。
    QPointer<QProcess> p = proc_;

    // Request graceful termination on our owning thread (UI thread)
    p->terminate();

    // When it finishes, clean up and notify
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, p](int, QProcess::ExitStatus)
            {
        if (p)
        {
            p->deleteLater();
            if (proc_ == p) proc_.clear();
        }
        emitServerStoppedOnce(); });

    // Guard: force kill if it doesn't exit in time; avoid blocking UI
    QTimer::singleShot(1500, this, [this, p]()
                       {
        if (!p) return;
        if (p->state() == QProcess::Running)
        {
#ifdef _WIN32
            const qint64 pid = p->processId();
            if (pid > 0)
            {
                QStringList args;
                args << "/PID" << QString::number(pid) << "/T" << "/F";
                QProcess::execute("taskkill", args);
            }
            else
            {
                p->kill();
            }
#else
            p->kill();
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
