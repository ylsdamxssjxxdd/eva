#include "flowtracer.h"

#include <QDebug>

namespace
{
QString channelLabel(FlowChannel channel)
{
    switch (channel)
    {
    case FlowChannel::Lifecycle: return QStringLiteral("lifecycle");
    case FlowChannel::Backend: return QStringLiteral("backend");
    case FlowChannel::UI: return QStringLiteral("ui");
    case FlowChannel::Net: return QStringLiteral("net");
    case FlowChannel::Tool: return QStringLiteral("tool");
    case FlowChannel::Session: return QStringLiteral("session");
    }
    return QStringLiteral("unknown");
}
} // namespace

void FlowTracer::log(FlowChannel channel, const QString &message, quint64 turnId)
{
    const QString channelPart = QStringLiteral("[flow][%1]").arg(channelLabel(channel));
    QString turnPart;
    if (turnId > 0)
    {
        turnPart = QStringLiteral("[turn%1]").arg(turnId);
    }
    const QString line = turnPart.isEmpty() ? QStringLiteral("%1 %2").arg(channelPart, message)
                                            : QStringLiteral("%1%2 %3").arg(channelPart, turnPart, message);
    qInfo().noquote() << line;
}
