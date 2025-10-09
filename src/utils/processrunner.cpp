#include "processrunner.h"

#include <QProcess>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif

ProcessResult ProcessRunner::run(const QString &program,
                                 const QStringList &args,
                                 const QString &workingDir,
                                 const QProcessEnvironment &env,
                                 int timeoutMs)
{
    QProcess p;
    p.setProcessEnvironment(env);
    if (!workingDir.isEmpty()) p.setWorkingDirectory(workingDir);

    ProcessResult r;
    p.start(program, args);
    if (!p.waitForStarted()) {
        r.stdErr = QString::fromUtf8("failed to start: ") + program;
        r.exitCode = -1;
        return r;
    }

    if (timeoutMs > 0) {
        if (!p.waitForFinished(timeoutMs)) {
            r.timedOut = true;
            p.kill();
            p.waitForFinished(500);
        }
    } else {
        p.waitForFinished(-1);
    }

    QByteArray out = p.readAllStandardOutput();
    QByteArray err = p.readAllStandardError();
#ifdef Q_OS_WIN
    r.stdOut = QString::fromLocal8Bit(out);
    r.stdErr = QString::fromLocal8Bit(err);
#else
    r.stdOut = QString::fromUtf8(out);
    r.stdErr = QString::fromUtf8(err);
#endif
    r.exitCode = p.exitCode();
    return r;
}

ProcessResult ProcessRunner::runShellCommand(const QString &commandLine,
                                             const QString &workingDir,
                                             const QProcessEnvironment &env,
                                             int timeoutMs)
{
#ifdef Q_OS_WIN
    const QString shell = QStringLiteral("cmd.exe");
    QStringList args; args << "/c" << commandLine;
#else
    const QString shell = QStringLiteral("/bin/sh");
    QStringList args; args << "-lc" << commandLine;
#endif
    return run(shell, args, workingDir, env, timeoutMs);
}

QProcessEnvironment ProcessRunner::envWithPathPrepend(const QStringList &pathsToPrepend)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value(QStringLiteral("PATH"));
#ifdef Q_OS_WIN
    const QChar sep = QChar(';');
#else
    const QChar sep = QChar(':');
#endif
    QStringList current = path.split(sep, Qt::SkipEmptyParts);
    QStringList merged = pathsToPrepend;
    merged.append(current);
    env.insert(QStringLiteral("PATH"), merged.join(sep));
    return env;
}

QString ProcessRunner::findExecutable(const QString &name)
{
    // QStandardPaths::findExecutable handles PATH search and suffixes on Windows
#include <QFileInfo>
#include <QDir>
    QString path = QStandardPaths::findExecutable(name);
#include <QFileInfo>
#include <QDir>
    if (!path.isEmpty()) return path;
#ifdef Q_OS_WIN
    // Fallback: check App Execution Aliases directory explicitly
    const QString aliasDir = QDir::home().filePath(QStringLiteral("AppData/Local/Microsoft/WindowsApps"));
    QString cand = QStandardPaths::findExecutable(name, {aliasDir});
#include <QFileInfo>
#include <QDir>
    if (!cand.isEmpty()) return cand;
#endif
    return QString();
}

