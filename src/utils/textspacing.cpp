#include "textspacing.h"

#include <QSignalBlocker>
#include <QtGlobal>

namespace TextSpacing
{
static QTextBlockFormat buildFormat(qreal factor)
{
    QTextBlockFormat fmt;
    const qreal clamped = qBound<qreal>(0.8, factor, 3.0);
    fmt.setLineHeight(qRound(clamped * 100.0), QTextBlockFormat::ProportionalHeight);
    return fmt;
}

void applyToDocument(QTextDocument *doc, qreal factor)
{
    if (!doc) return;
    QTextCursor cursor(doc);
    if (!cursor.hasSelection()) cursor.select(QTextCursor::Document);
    QSignalBlocker blocker(doc);
    cursor.beginEditBlock();
    cursor.mergeBlockFormat(buildFormat(factor));
    cursor.endEditBlock();
}
} // namespace TextSpacing
