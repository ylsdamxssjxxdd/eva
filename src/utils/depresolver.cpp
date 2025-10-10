#include "depresolver.h"
#include "processrunner.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

#ifdef Q_OS_WIN
static inline QStringList venvCandidates(const QString &base)
{
    return {QDir(base).filePath(".venv/Scripts/python.exe"),
            QDir(base).filePath("venv/Scripts/python.exe")};
}
#else
static inline QStringList venvCandidates(const QString &base)
{
    return {QDir(base).filePath(".venv/bin/python3"),
            QDir(base).filePath("venv/bin/python3")};
}
#endif

static inline bool looksLikePython3(const QString &program, const QStringList &prefixArgs, QString *outVersion)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList args = prefixArgs;
    args << "-c" << "import sys;print(str(sys.version_info[0])+'.'+str(sys.version_info[1]))";
    ProcessResult r = ProcessRunner::run(program, args, QDir::currentPath(), env, 3000);
    if (r.timedOut || r.exitCode != 0) return false;
    const QString v = r.stdOut.trimmed();
    if (!v.startsWith("3")) return false;
    if (outVersion) *outVersion = v;
    return true;
}

ExecSpec DependencyResolver::discoverPython3(const QString &projectDir)
{
    ExecSpec spec;
    // 1) prefer local venv under project dir
    for (const QString &cand : venvCandidates(projectDir))
    {
        if (QFileInfo::exists(cand))
        {
            QString ver;
            if (looksLikePython3(cand, {}, &ver))
            {
                spec.program = cand;
                spec.absolutePath = cand;
                spec.version = ver;
                return spec;
            }
        }
    }
#ifdef Q_OS_WIN
    // 2) try py -3 launcher
    {
        QString ver;
        if (looksLikePython3(QStringLiteral("py"), {"-3"}, &ver))
        {
            spec.program = QStringLiteral("py");
            spec.extraArgs = QStringList{QStringLiteral("-3")};
            // try to get absolute path to the selected interpreter
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            ProcessResult r = ProcessRunner::run(QStringLiteral("py"), {"-3", "-c", "import sys;print(sys.executable)"}, QDir::currentPath(), env, 3000);
            spec.absolutePath = r.stdOut.trimmed();
            spec.version = ver;
            return spec;
        }
    }
    // 3) fallbacks by name
    const QStringList names = {QStringLiteral("python3.exe"), QStringLiteral("python.exe")};
#else
    const QStringList names = {QStringLiteral("python3"), QStringLiteral("python")};
#endif
    for (const QString &n : names)
    {
        const QString exe = ProcessRunner::findExecutable(n);
        if (!exe.isEmpty())
        {
            QString ver;
            if (looksLikePython3(exe, {}, &ver))
            {
                spec.program = exe;
                spec.absolutePath = exe;
                spec.version = ver;
                return spec;
            }
        }
    }
    // empty spec signals not found; caller provides guidance
    return spec;
}

QString DependencyResolver::pythonVersion(const ExecSpec &spec, int timeoutMs)
{
    if (spec.program.isEmpty()) return QString();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList args = spec.extraArgs;
    args << "--version";
    ProcessResult r = ProcessRunner::run(spec.program, args, QDir::currentPath(), env, timeoutMs);
    QString s = r.stdOut + r.stdErr;
    return s.trimmed();
}

QString DependencyResolver::findGit()
{
    QString exe = ProcessRunner::findExecutable(QStringLiteral("git"));
#ifdef Q_OS_WIN
    if (exe.isEmpty()) exe = ProcessRunner::findExecutable(QStringLiteral("git.exe"));
#endif
    return exe;
}

QString DependencyResolver::findCMake()
{
    QString exe = ProcessRunner::findExecutable(QStringLiteral("cmake"));
#ifdef Q_OS_WIN
    if (exe.isEmpty()) exe = ProcessRunner::findExecutable(QStringLiteral("cmake.exe"));
#endif
    return exe;
}

QString DependencyResolver::doctorReport(const QString &projectDir)
{
    QStringList lines;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    lines << QStringLiteral("PATH=%1").arg(env.value(QStringLiteral("PATH")));
    ExecSpec py = discoverPython3(projectDir);
    if (py.program.isEmpty())
    {
        lines << QStringLiteral("python: not found");
    }
    else
    {
        const QString ver = pythonVersion(py);
        lines << QStringLiteral("python: %1 (%2) program=%3 extra=%4").arg(ver, py.absolutePath, py.program, py.extraArgs.join(' '));
    }
    const QString git = findGit();
    lines << QStringLiteral("git: %1").arg(git.isEmpty() ? QStringLiteral("not found") : git);
    const QString cm = findCMake();
    lines << QStringLiteral("cmake: %1").arg(cm.isEmpty() ? QStringLiteral("not found") : cm);
    return lines.join('\n');
}
