// docparser.cpp - implementation

#include "docparser.h"

#include "thirdparty/miniz/miniz.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QtEndian>
#include <cstring>
#include <limits>
#include <algorithm>

namespace DocParser
{
QString readPlainTextFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    in.setCodec("UTF-8");
    const QString s = in.readAll();
    return s;
}

// naive markdown cleanup
QString markdownToText(const QString &md)
{
    QString s = md;
    // remove fenced code blocks
    s.remove(QRegularExpression("```[\s\S]*?```"));
    // inline code
    s.replace(QRegularExpression("`([^`]*)`"), "\\1");
    // images ![alt](...)
    s.replace(QRegularExpression("!\\[([^\\]]*)\\]\\([^\\)]*\\)"), "\\1");
    // links [text](url) -> text
    s.replace(QRegularExpression("\\[([^\\]]*)\\]\\([^\\)]*\\)"), "\\1");
    // headings leading #'s
    s.replace(QRegularExpression("^\n?\n?\s*#+\\s*", QRegularExpression::MultilineOption), "");
    // blockquotes
    s.replace(QRegularExpression("^>\\s*", QRegularExpression::MultilineOption), "");
    // tables: strip pipes/separators
    s.replace(QRegularExpression("^\\|.*\\|$", QRegularExpression::MultilineOption), "");
    s.replace(QRegularExpression("^\\s*\|?\\s*:-*:?\\s*(\\|\\s*:-*:?\\s*)*$", QRegularExpression::MultilineOption), "");
    // HTML tags
    s.replace(QRegularExpression("<[^>]+>"), "");
    // emphasis **bold**, *italic*
    s.replace("**", "");
    s.replace("*", "");
    return s;
}

// Extract human-readable text from DOCX document.xml
static QString parseDocxDocumentXml(const QString &xml)
{
    QString out;
    QXmlStreamReader xr(xml);
    QString currentParagraph;
    while (!xr.atEnd())
    {
        auto t = xr.readNext();
        if (t == QXmlStreamReader::StartElement)
        {
            const QStringRef name = xr.name();
            if (name == QLatin1String("t"))
            {
                // text run inside paragraph
                currentParagraph += xr.readElementText(QXmlStreamReader::IncludeChildElements);
            }
            else if (name == QLatin1String("br") || name == QLatin1String("cr"))
            {
                // explicit line break in a run
                currentParagraph += QLatin1Char('\n');
            }
            else if (name == QLatin1String("tab"))
            {
                currentParagraph += QLatin1Char(' ');
            }
        }
        else if (t == QXmlStreamReader::EndElement)
        {
            const QStringRef name = xr.name();
            if (name == QLatin1String("p"))
            {
                // paragraph end
                if (!currentParagraph.trimmed().isEmpty())
                {
                    out += currentParagraph;
                    out += QLatin1Char('\n');
                }
                currentParagraph.clear();
            }
        }
    }
    if (!currentParagraph.trimmed().isEmpty())
    {
        out += currentParagraph;
        out += QLatin1Char('\n');
    }
    return out;
}

static QString readDocxViaZipArchive(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists() || !info.isFile()) return {};

    QFile archiveFile(info.absoluteFilePath());
    if (!archiveFile.open(QIODevice::ReadOnly)) return {};
    const QByteArray archiveBytes = archiveFile.readAll();
    if (archiveBytes.isEmpty()) return {};

    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_reader_init_mem(&archive, archiveBytes.constData(), archiveBytes.size(), 0))
    {
        mz_zip_reader_end(&archive);
        return {};
    }

    const int fileIndex = mz_zip_reader_locate_file(&archive, "word/document.xml", nullptr, 0);
    if (fileIndex < 0)
    {
        mz_zip_reader_end(&archive);
        return {};
    }

    size_t xmlSize = 0;
    void *xmlPtr = mz_zip_reader_extract_to_heap(&archive, fileIndex, &xmlSize, 0);
    mz_zip_reader_end(&archive);
    if (!xmlPtr || xmlSize == 0)
    {
        if (xmlPtr) mz_free(xmlPtr);
        return {};
    }
    if (xmlSize > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        mz_free(xmlPtr);
        return {};
    }

    const QString xml = QString::fromUtf8(static_cast<const char *>(xmlPtr), static_cast<int>(xmlSize));
    mz_free(xmlPtr);
    return parseDocxDocumentXml(xml);
}

QString readDocxText(const QString &path)
{
#ifdef _WIN32
    // Try: read document.xml directly via .NET ZipFile to avoid disk extraction.
    auto psReadDocXml = [&](const QString &psExe) -> QString
    {
        QString pEsc = path;
        pEsc.replace("'", "''");
        const QString script =
            "$ErrorActionPreference='Stop';"
            "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8;"
            "Add-Type -AssemblyName 'System.IO.Compression.FileSystem';"
            "$p='" +
            pEsc + "';"
                   "$zip=[System.IO.Compression.ZipFile]::OpenRead($p);"
                   "try{"
                   "$e=$zip.GetEntry('word/document.xml');"
                   "if($e -eq $null){ exit 2 }"
                   "$sr=New-Object System.IO.StreamReader($e.Open(),[System.Text.Encoding]::UTF8);"
                   "$xml=$sr.ReadToEnd();$sr.Close();"
                   "[Console]::Write($xml)"
                   "}finally{$zip.Dispose()}";
        QProcess pr;
        QStringList args;
        args << QStringLiteral("-NoLogo") << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << script;
        pr.start(psExe, args);
        if (!pr.waitForFinished(20000)) return {};
        if (pr.exitCode() != 0) return {};
        const QByteArray out = pr.readAllStandardOutput();
        return QString::fromUtf8(out);
    };

    QString xml = psReadDocXml(QStringLiteral("powershell"));
    if (xml.isEmpty()) xml = psReadDocXml(QStringLiteral("pwsh"));
    if (!xml.isEmpty()) return parseDocxDocumentXml(xml);
#endif

    return readDocxViaZipArchive(path);
}

static bool isLikelyPrintableWordChar(quint16 ch)
{
    if (ch == 0x0000 || ch == 0xFFFF || ch == 0xFFFE) return false;
    if (ch == 0x0009 || ch == 0x000A || ch == 0x000D) return true;
    if (ch == 0x3000) return true; // ideographic space
    if (ch >= 0x20 && ch <= 0xD7FF) return true;
    if (ch >= 0xE000 && ch <= 0xF8FF) return false; // private use blocks
    if (ch >= 0xF000 && ch <= 0xFFFF) return false;
    return false;
}

static bool looksLikeDocumentText(const QString &chunk)
{
    const QString trimmed = chunk.trimmed();
    if (trimmed.size() < 2 || trimmed.size() > 1024) return false;
    static const QSet<QString> noise = {
        QStringLiteral("Root Entry"),
        QStringLiteral("SummaryInformation"),
        QStringLiteral("DocumentSummaryInformation"),
        QStringLiteral("WordDocument"),
        QStringLiteral("0Table"),
        QStringLiteral("1Table"),
        QStringLiteral("Normal.dotm"),
        QStringLiteral("WpsCustomData"),
        QStringLiteral("KSOProductBuildVer"),
        QStringLiteral("KSOTemplateDocerSaveRecord"),
        QStringLiteral("默认段落字体"),
        QStringLiteral("Calibri"),
        QStringLiteral("Calibr"),
        QStringLiteral("普通表格")
    };
    if (noise.contains(trimmed)) return false;
    if (trimmed.contains(QLatin1Char('@'))) return false;

    int nonSpaceLen = 0;
    for (QChar c : trimmed)
    {
        if (!c.isSpace()) ++nonSpaceLen;
    }
    if (nonSpaceLen == 0) return false;

    int cjk = 0;
    int digits = 0;
    int asciiUpper = 0;
    int asciiAlpha = 0;
    for (QChar ch : trimmed)
    {
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) ++cjk;
        if (ch.isDigit()) ++digits;
        if (ch.isUpper() && ch.unicode() <= 0x7F) ++asciiUpper;
        if (ch.isLetter() && ch.unicode() <= 0x7F) ++asciiAlpha;
    }
    const int important = cjk + digits;
    if (important == 0) return false;
    if (important * 2 < nonSpaceLen && cjk == 0) return false;
    if (asciiUpper > important) return false;
    if (cjk == 0)
    {
        if (digits == 0) return false;
        if (asciiUpper > 0 || asciiAlpha > 0) return false;
        if (nonSpaceLen > digits + 2) return false;
    }
    return true;
}

static int chunkScore(const QString &chunk)
{
    int cjk = 0;
    int digits = 0;
    int asciiAlpha = 0;
    for (QChar ch : chunk)
    {
        if (ch.unicode() >= 0x4E00 && ch.unicode() <= 0x9FFF) ++cjk;
        if (ch.isDigit()) ++digits;
        if (ch.isLetter() && ch.unicode() <= 0x7F) ++asciiAlpha;
    }
    int score = cjk * 5 + digits * 3 - asciiAlpha;
    if (digits >= 6 && digits >= cjk && digits > asciiAlpha) score += digits * 10;
    return score;
}

QString readWpsText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = f.readAll();
    if (data.isEmpty()) return {};

    QStringList chunks;
    QString current;
    const auto flushChunk = [&]() {
        if (!current.isEmpty()) chunks << current;
        current.clear();
    };

    int offset = 0;
    const int size = data.size();
    bool reading = false;
    while (offset + 1 < size)
    {
        if (!reading && (offset & 1))
        {
            ++offset;
            continue;
        }
        const quint16 value = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(data.constData() + offset));
        if (isLikelyPrintableWordChar(value))
        {
            reading = true;
            if (value == 0x000D)
                current.append(QChar('\n'));
            else if (value == 0x000A)
                current.append(QChar('\n'));
            else
                current.append(QChar(value));
            offset += 2;
        }
        else
        {
            if (reading)
            {
                if (current.size() >= 3) flushChunk();
                else current.clear();
            }
            reading = false;
            offset += 2;
        }
    }
    if (reading && current.size() >= 3) flushChunk();

    // Second pass: attempt to catch ASCII sequences embedded in UTF-16 with zero high-byte
    QString asciiAccumulator;
    const auto flushAscii = [&]() {
        if (asciiAccumulator.size() >= 3)
        {
            const QString trimmed = asciiAccumulator.trimmed();
            if (!trimmed.isEmpty()) chunks << trimmed;
        }
        asciiAccumulator.clear();
    };
    for (int i = 0; i < size; ++i)
    {
        const uchar ch = static_cast<uchar>(data.at(i));
        if (ch >= 32 && ch <= 126)
            asciiAccumulator.append(QChar(ch));
        else if (ch == '\r' || ch == '\n')
            asciiAccumulator.append(QChar('\n'));
        else
            flushAscii();
    }
    flushAscii();

    QSet<QString> seen;
    QStringList filtered;
    for (const QString &chunk : chunks)
    {
        const QString trimmed = chunk.trimmed();
        if (!looksLikeDocumentText(trimmed)) continue;
        if (seen.contains(trimmed)) continue;
        seen.insert(trimmed);
        filtered << trimmed;
    }
    if (filtered.size() > 1)
    {
        QVector<int> scores;
        scores.reserve(filtered.size());
        int bestScore = std::numeric_limits<int>::min();
        for (const QString &chunk : std::as_const(filtered))
        {
            const int s = chunkScore(chunk);
            scores << s;
            bestScore = std::max(bestScore, s);
        }
        const int cutoff = bestScore > 0 ? bestScore - 4 : bestScore;
        QStringList prioritized;
        for (int i = 0; i < filtered.size(); ++i)
        {
            if (scores.at(i) >= cutoff && scores.at(i) > 0) prioritized << filtered.at(i);
        }
        if (!prioritized.isEmpty()) filtered = prioritized;
    }
    return filtered.join(QStringLiteral("\n"));
}

} // namespace DocParser
