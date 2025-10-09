#ifndef EVA_PROCESS_RUNNER_H
#define EVA_PROCESS_RUNNER_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QProcessEnvironment>

struct ProcessResult {
    int exitCode = -1;
    bool timedOut = false;
    QString stdOut;
    QString stdErr;
};

// Cross-platform process execution wrapper; avoids shell unless requested.
class ProcessRunner {
  public:
    // Run a program with args. Working dir and env are explicit. Timeout ms (0 = no timeout).
    static ProcessResult run(const QString &program,
                             const QStringList &args,
                             const QString &workingDir,
                             const QProcessEnvironment &env,
                             int timeoutMs = 0);

    // Convenience: run a single-line shell command using platform shell (cmd /c or /bin/sh -lc).
    static ProcessResult runShellCommand(const QString &commandLine,
                                         const QString &workingDir,
                                         const QProcessEnvironment &env,
                                         int timeoutMs = 0);

    // Build an env with PATH prefix injection; keeps system env and prepends "paths" to PATH.
    static QProcessEnvironment envWithPathPrepend(const QStringList &pathsToPrepend);

    // Find an executable by name; returns absolute path or empty. Tries platform-specific lookup.
    static QString findExecutable(const QString &name);
};

#endif // EVA_PROCESS_RUNNER_H
