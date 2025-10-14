// docparser.cpp - implementation

#include "docparser.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>
#include <QXmlStreamReader>

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

    // Fallback: Expand-Archive to a temp dir then read file
    QTemporaryDir tmp;
    if (!tmp.isValid()) return {};
    const QString dst = tmp.path();
    const QString ps = QStringLiteral(
                           "$ErrorActionPreference='Stop';"
                           "Expand-Archive -Force -LiteralPath \"%1\" -DestinationPath \"%2\"")
                           .arg(path, dst);
    QProcess proc;
    QStringList args;
    args << QStringLiteral("-NoLogo") << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << ps;
    proc.start(QStringLiteral("powershell"), args);
    if (!proc.waitForFinished(20000) || proc.exitCode() != 0)
    {
        // try pwsh as fallback
        QProcess proc2;
        QStringList args2;
        args2 << QStringLiteral("-NoLogo") << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << ps;
        proc2.start(QStringLiteral("pwsh"), args2);
        proc2.waitForFinished(20000);
        if (proc2.exitCode() != 0) return {};
    }
    const QString docxml = dst + "/word/document.xml";
    QFile f(docxml);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    in.setCodec("UTF-8");
    xml = in.readAll();
    return parseDocxDocumentXml(xml);
#else
    Q_UNUSED(path);
    return {};
#endif
}

} // namespace DocParser
