#include "textparse.h"

namespace TextParse
{
bool parseFirstInt(const QString &text, int &value)
{
    int i = 0;
    while (i < text.size() && !text.at(i).isDigit() && text.at(i) != '-') ++i;
    if (i >= text.size()) return false;
    bool negative = false;
    if (text.at(i) == '-')
    {
        negative = true;
        ++i;
    }
    const int start = i;
    while (i < text.size() && text.at(i).isDigit()) ++i;
    if (start == i) return false;
    bool ok = false;
    const int parsed = text.mid(start, i - start).toInt(&ok);
    if (!ok) return false;
    value = negative ? -parsed : parsed;
    return true;
}

bool extractIntAfterKeyword(const QString &line, const QString &keyword, int &value)
{
    const int pos = line.indexOf(keyword);
    if (pos < 0) return false;
    const QString tail = line.mid(pos + keyword.size());
    return parseFirstInt(tail, value);
}

bool extractIntBetweenMarkers(const QString &line, const QString &left, const QString &right, int &value)
{
    int start = line.indexOf(left);
    if (start < 0) return false;
    start += left.size();
    int end = line.indexOf(right, start);
    if (end < 0) end = line.size();
    return parseFirstInt(line.mid(start, end - start), value);
}

bool extractLastIntBeforeSuffix(const QString &line, const QString &suffix, int &value)
{
    const int idx = line.indexOf(suffix);
    if (idx < 0) return false;
    QString prefix = line.left(idx).trimmed();
    int end = prefix.size() - 1;
    while (end >= 0 && !prefix.at(end).isDigit()) --end;
    if (end < 0) return false;
    int start = end;
    while (start >= 0 && prefix.at(start).isDigit()) --start;
    ++start;
    bool ok = false;
    const int parsed = prefix.mid(start, end - start + 1).toInt(&ok);
    if (!ok) return false;
    value = parsed;
    return true;
}

QString textAfterKeyword(const QString &line, const QString &keyword)
{
    int pos = line.indexOf(keyword);
    if (pos < 0) return QString();
    pos += keyword.size();
    return line.mid(pos).trimmed();
}

void stripTagBlocksCaseInsensitive(QString &text, const QString &tagName)
{
    const QString openLower = QStringLiteral("<%1>").arg(tagName.toLower());
    const QString closeLower = QStringLiteral("</%1>").arg(tagName.toLower());
    QString lower = text.toLower();
    int searchStart = 0;
    while (true)
    {
        const int openIdx = lower.indexOf(openLower, searchStart);
        if (openIdx < 0) break;
        const int closeIdx = lower.indexOf(closeLower, openIdx + openLower.size());
        if (closeIdx < 0) break;
        const int removeLen = closeIdx + closeLower.size() - openIdx;
        text.remove(openIdx, removeLen);
        lower.remove(openIdx, removeLen);
        searchStart = openIdx;
    }
}

QStringList collectTagBlocks(const QString &text, const QString &tagName)
{
    QStringList blocks;
    const QString openLower = QStringLiteral("<%1>").arg(tagName.toLower());
    const QString closeLower = QStringLiteral("</%1>").arg(tagName.toLower());
    const int openLen = openLower.size();
    const int closeLen = closeLower.size();
    QString lower = text.toLower();
    int searchStart = 0;
    while (true)
    {
        const int openIdx = lower.indexOf(openLower, searchStart);
        if (openIdx < 0) break;
        const int closeIdx = lower.indexOf(closeLower, openIdx + openLen);
        if (closeIdx < 0) break;
        const int contentStart = openIdx + openLen;
        const QString inner = text.mid(contentStart, closeIdx - contentStart);
        blocks.append(inner);
        searchStart = closeIdx + closeLen;
    }
    return blocks;
}

QString stripCodeFenceMarkers(const QString &input)
{
    QString out = input.trimmed();
    if (out.startsWith(QStringLiteral("```")))
    {
        int newlineIndex = out.indexOf('\n', 3);
        if (newlineIndex >= 0)
        {
            out = out.mid(newlineIndex + 1);
        }
        else
        {
            out.clear();
        }
    }
    out = out.trimmed();
    const QString trimmed = out.trimmed();
    if (trimmed.endsWith(QStringLiteral("```")))
    {
        int idx = trimmed.lastIndexOf(QStringLiteral("```"));
        if (idx >= 0) out = trimmed.left(idx);
    }
    return out.trimmed();
}

QStringList collectLooseJsonObjects(const QString &text)
{
    QStringList objects;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    int start = -1;

    for (int i = 0; i < text.size(); ++i)
    {
        const QChar ch = text.at(i);
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (ch == '\\')
            {
                escaped = true;
                continue;
            }
            if (ch == '"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == '"')
        {
            inString = true;
            continue;
        }

        if (ch == '{')
        {
            if (depth == 0) start = i;
            ++depth;
        }
        else if (ch == '}')
        {
            if (depth == 0) continue;
            --depth;
            if (depth == 0 && start != -1)
            {
                objects << text.mid(start, i - start + 1);
                start = -1;
            }
        }
    }

    return objects;
}

QString removeAllWhitespace(const QString &input)
{
    QString out;
    out.reserve(input.size());
    for (const QChar ch : input)
    {
        if (!ch.isSpace()) out.append(ch);
    }
    return out;
}
} // namespace TextParse
