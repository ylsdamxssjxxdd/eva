#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QProcessEnvironment>

#include "utils/processrunner.h"

namespace
{
#ifdef Q_OS_WIN
const QString kShellProgram = QStringLiteral("cmd.exe");
const QStringList kShellEchoArgs = {QStringLiteral("/c"), QStringLiteral("echo EVA_PROCESS_RUNNER")};
const QString kShellSleepCommand = QStringLiteral("ping 127.0.0.1 -n 3 >NUL");
#else
const QString kShellProgram = QStringLiteral("/bin/sh");
const QStringList kShellEchoArgs = {QStringLiteral("-c"), QStringLiteral("echo EVA_PROCESS_RUNNER")};
const QString kShellSleepCommand = QStringLiteral("sleep 2");
#endif

QProcessEnvironment baseEnv()
{
    return QProcessEnvironment::systemEnvironment();
}
} // namespace

TEST_CASE("ProcessRunner::run executes commands and captures stdout")
{
    const ProcessResult result = ProcessRunner::run(kShellProgram, kShellEchoArgs, QString(), baseEnv(), 2000);
    REQUIRE_FALSE(result.timedOut);
    CHECK(result.exitCode == 0);
    CHECK(result.stdOut.contains(QStringLiteral("EVA_PROCESS_RUNNER")));
}

TEST_CASE("ProcessRunner::runShellCommand respects timeout and sets timedOut flag")
{
    const ProcessResult fast = ProcessRunner::runShellCommand(QStringLiteral("echo EVA_SHELL_FAST"), QString(), baseEnv(), 0);
    REQUIRE(fast.exitCode == 0);
    CHECK_FALSE(fast.timedOut);
    CHECK(fast.stdOut.contains(QStringLiteral("EVA_SHELL_FAST")));

    ProcessResult slow = ProcessRunner::runShellCommand(kShellSleepCommand, QString(), baseEnv(), 10);
    CHECK(slow.timedOut);
    CHECK(slow.exitCode != 0);
}

TEST_CASE("ProcessRunner::envWithPathPrepend injects directories to the front of PATH")
{
    const QStringList injected = {QStringLiteral("C:/eva/tools"), QStringLiteral("/opt/eva/bin")};
    const QProcessEnvironment env = ProcessRunner::envWithPathPrepend(injected);
    const QChar sep =
#ifdef Q_OS_WIN
        QChar(';');
#else
        QChar(':');
#endif
    const QStringList parts = env.value(QStringLiteral("PATH")).split(sep, Qt::SkipEmptyParts);
    REQUIRE(parts.size() >= injected.size());
    for (int i = 0; i < injected.size(); ++i)
    {
        CHECK(parts.at(i) == injected.at(i));
    }
}
