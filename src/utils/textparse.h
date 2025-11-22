#ifndef TEXTPARSE_H
#define TEXTPARSE_H

#include <QString>
#include <QStringList>

namespace TextParse
{
bool parseFirstInt(const QString &text, int &value);
bool extractIntAfterKeyword(const QString &line, const QString &keyword, int &value);
bool extractIntBetweenMarkers(const QString &line, const QString &left, const QString &right, int &value);
bool extractLastIntBeforeSuffix(const QString &line, const QString &suffix, int &value);
QString textAfterKeyword(const QString &line, const QString &keyword);

void stripTagBlocksCaseInsensitive(QString &text, const QString &tagName);
QStringList collectTagBlocks(const QString &text, const QString &tagName);
QString stripCodeFenceMarkers(const QString &input);
QStringList collectLooseJsonObjects(const QString &text);
QString removeAllWhitespace(const QString &input);
} // namespace TextParse

#endif // TEXTPARSE_H
