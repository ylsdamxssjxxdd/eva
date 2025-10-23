#ifndef TEXTSPACING_H
#define TEXTSPACING_H

#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QPlainTextEdit>

namespace TextSpacing
{
// Apply proportional line spacing to an entire document; factor is 1.0 for 100%.
void applyToDocument(QTextDocument *doc, qreal factor);

inline void apply(QTextEdit *edit, qreal factor)
{
    if (!edit) return;
    applyToDocument(edit->document(), factor);
}

inline void apply(QPlainTextEdit *edit, qreal factor)
{
    if (!edit) return;
    applyToDocument(edit->document(), factor);
}
} // namespace TextSpacing

#endif // TEXTSPACING_H
