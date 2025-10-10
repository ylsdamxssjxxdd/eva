#ifndef EVA_DEP_RESOLVER_H
#define EVA_DEP_RESOLVER_H

#include <QPair>
#include <QString>
#include <QStringList>

struct ExecSpec
{
    QString program;       // program to invoke (absolute or name)
    QStringList extraArgs; // extra args to prepend when invoking, e.g. {"-3"} for py -3
    QString absolutePath;  // resolved absolute path if known
    QString version;       // cached version string (short)
};

// Discover external dependencies (python, git, cmake, etc.).
class DependencyResolver
{
  public:
    static ExecSpec discoverPython3(const QString &projectDir); // prefer local venv; Windows-aware
    static QString pythonVersion(const ExecSpec &spec, int timeoutMs = 3000);

    static QString findGit();
    static QString findCMake();

    // Simple doctor text for UI/debug.
    static QString doctorReport(const QString &projectDir);
};

#endif // EVA_DEP_RESOLVER_H
