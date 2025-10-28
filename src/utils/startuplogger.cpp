#include "startuplogger.h"

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>

namespace
{
QElapsedTimer g_timer;
bool g_started = false;
QMutex g_mutex;
} // namespace

void StartupLogger::start()
{
    QMutexLocker locker(&g_mutex);
    g_timer.start();
    g_started = true;
    qInfo().noquote() << QStringLiteral("[startup] timer started");
}

void StartupLogger::log(const QString &step)
{
    QMutexLocker locker(&g_mutex);
    if (!g_started) return;
    const qint64 ms = g_timer.elapsed();
    qInfo().noquote() << QStringLiteral("[startup] %1 @ %2 ms").arg(step, QString::number(ms));
}

qint64 StartupLogger::elapsedMs()
{
    QMutexLocker locker(&g_mutex);
    return g_started ? g_timer.elapsed() : -1;
}
