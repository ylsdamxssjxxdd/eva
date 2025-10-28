#ifndef STARTUPLOGGER_H
#define STARTUPLOGGER_H

#include <QString>

namespace StartupLogger
{
// Start global startup timer. Safe to call multiple times (later calls restart).
void start();

// Log a step message with the current elapsed milliseconds since start().
void log(const QString &step);

// Retrieve current elapsed milliseconds since start(); returns -1 if not started.
qint64 elapsedMs();
} // namespace StartupLogger

#endif // STARTUPLOGGER_H
