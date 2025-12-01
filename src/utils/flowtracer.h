#ifndef FLOWTRACER_H
#define FLOWTRACER_H

#include <QString>
#include <QtGlobal>

enum class FlowChannel
{
    Lifecycle,
    Backend,
    UI,
    Net,
    Tool,
    Session
};

class FlowTracer
{
  public:
    // Print a unified flow log to the terminal with channel and optional turn id.
    static void log(FlowChannel channel, const QString &message, quint64 turnId = 0);
};

#endif // FLOWTRACER_H
