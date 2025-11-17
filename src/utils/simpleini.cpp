#include "simpleini.h"

#include <QFile>
#include <QTextStream>

namespace
{
QString decodeEscapes(const QString &value)
{
    QString result;
    result.reserve(value.size());
    bool escape = false;
    for (const QChar &ch : value)
    {
        if (!escape)
        {
            if (ch == QLatin1Char('\\'))
            {
                escape = true;
            }
            else
            {
                result.append(ch);
            }
            continue;
        }
        escape = false;
        switch (ch.unicode())
        {
        case 'n':
            result.append(QLatin1Char('\n'));
            break;
        case 'r':
            result.append(QLatin1Char('\r'));
            break;
        case 't':
            result.append(QLatin1Char('\t'));
            break;
        case '\\':
            result.append(QLatin1Char('\\'));
            break;
        case '"':
            result.append(QLatin1Char('"'));
            break;
        default:
            result.append(ch);
            break;
        }
    }
    if (escape) result.append(QLatin1Char('\\'));
    return result;
}
} // namespace

namespace simpleini
{
QString decodeValue(const QString &value)
{
    return decodeEscapes(value);
}

QHash<QString, QString> parseFile(const QString &path, QString *error)
{
    QHash<QString, QString> map;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error) *error = QStringLiteral("open failed: %1").arg(path);
        return map;
    }
    QTextStream ts(&file);
    ts.setCodec("utf-8");
    int lineNumber = 0;
    while (!ts.atEnd())
    {
        const QString rawLine = ts.readLine();
        lineNumber++;
        const QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) continue;
        if (trimmed.startsWith(QLatin1Char('#')) || trimmed.startsWith(QLatin1Char(';'))) continue;
        const int equalPos = rawLine.indexOf(QLatin1Char('='));
        if (equalPos <= 0)
        {
            if (error) *error = QStringLiteral("invalid line %1 in %2").arg(lineNumber).arg(path);
            continue;
        }
        QString key = rawLine.left(equalPos).trimmed();
        if (key.startsWith(QChar(0xfeff))) key.remove(0, 1);
        const QString value = rawLine.mid(equalPos + 1);
        map.insert(key, decodeEscapes(value));
    }
    return map;
}
} // namespace simpleini
