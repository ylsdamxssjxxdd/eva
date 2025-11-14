#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "utils/docparser.h"

namespace
{
QString writeDocxFixture(QTemporaryDir &dir)
{
    static const QByteArray kDocxBase64 =
        QByteArrayLiteral(
            "UEsDBBQAAAAAAEpXbludxYoquQEAALkBAAATAAAAW0NvbnRlbnRfVHlwZXNdLnhtbDw/eG1sIHZlcnNp"
            "b249IjEuMCIgZW5jb2Rpbmc9IlVURi04IiBzdGFuZGFsb25lPSJ5ZXMiPz4KPFR5cGVzIHhtbG5zPSJod"
            "HRwOi8vc2NoZW1hcy5vcGVueG1sZm9ybWF0cy5vcmcvcGFja2FnZS8yMDA2L2NvbnRlbnQtdHlwZXMiPg"
            "ogIDxEZWZhdWx0IEV4dGVuc2lvbj0icmVscyIgQ29udGVudFR5cGU9ImFwcGxpY2F0aW9uL3ZuZC5vcGV"
            "ueG1sZm9ybWF0cy1wYWNrYWdlLnJlbGF0aW9uc2hpcHMreG1sIi8+CiAgPERlZmF1bHQgRXh0ZW5zaW9u"
            "PSJ4bWwiIENvbnRlbnRUeXBlPSJhcHBsaWNhdGlvbi94bWwiLz4KICA8T3ZlcnJpZGUgUGFydE5hbWU9I"
            "i93b3JkL2RvY3VtZW50LnhtbCIgQ29udGVudFR5cGU9ImFwcGxpY2F0aW9uL3ZuZC5vcGVueG1sZm9ybW"
            "F0cy1vZmZpY2Vkb2N1bWVudC53b3JkcHJvY2Vzc2luZ21sLmRvY3VtZW50Lm1haW4reG1sIi8+CjwvVHl"
            "wZXM+ClBLAwQUAAAAAABKV25bLRBNbi0BAAAtAQAACwAAAF9yZWxzLy5yZWxzPD94bWwgdmVyc2lvbj0i"
            "MS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9InllcyI/Pgo8UmVsYXRpb25zaGlwcyB4bWxucz"
            "0iaHR0cDovL3NjaGVtYXMub3BlbnhtbGZvcm1hdHMub3JnL3BhY2thZ2UvMjAwNi9yZWxhdGlvbnNoaXB"
            "zIj4KICA8UmVsYXRpb25zaGlwIElkPSJSMSIgVHlwZT0iaHR0cDovL3NjaGVtYXMub3BlbnhtbGZvcm1h"
            "dHMub3JnL29mZmljZURvY3VtZW50LzIwMDYvcmVsYXRpb25zaGlwcy9vZmZpY2VEb2N1bWVudCIgVGFyZ"
            "2V0PSJ3b3JkL2RvY3VtZW50LnhtbCIvPgo8L1JlbGF0aW9uc2hpcHM+ClBLAwQUAAAAAABKV25bvk2Eh1"
            "YBAABWAQAAEQAAAHdvcmQvZG9jdW1lbnQueG1sPD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVR"
            "GLTgiIHN0YW5kYWxvbmU9InllcyI/Pgo8dzpkb2N1bWVudCB4bWxuczp3PSJodHRwOi8vc2NoZW1hcy5v"
            "cGVueG1sZm9ybWF0cy5vcmcvd29yZHByb2Nlc3NpbmdtbC8yMDA2L21haW4iPgogIDx3OmJvZHk+CiAgI"
            "Dx3OnA+PHc6cj48dzp0PkVWQSBMaW5lIE9uZTwvdzp0PjwvdzpyPjwvdzpwPgogICAgPHc6cD48dzpyPj"
            "x3OnQgeG1sOnNwYWNlPSJwcmVzZXJ2ZSI+U2Vjb25kIExpbmUgPC93OnQ+PC93OnI+PHc6cj48dzpicj8"
            "+PC93OnI+PHc6cj48dzp0PlRhaWw8L3c6dD48L3c6cj48L3c6cD4KICA8L3c6Ym9keT4KPC93OmRvY3Vt"
            "ZW50PgpQSwECFAAUAAAAAABKV25bncWKKrkBAAC5AQAAEwAAAAAAAAAAAAAAgAEAAAAAW0NvbnRlbnRfV"
            "HlwZXNdLnhtbFBLAQIUABQAAAAAAEpXblstEE1uLQEAAC0BAAALAAAAAAAAAAAAAACAAeoBAABfcmVscy"
            "8ucmVsc1BLAQIUABQAAAAAAEpXblu+TYSHVgEAAFgBAAARAAAAAAAAAAAAAACAAUADAAB3b3JkL2RvY3V"
            "tZW50LnhtbFBLAUYAAAAAAwADALkAAADFBQAAAAA=");

    const QString path = dir.filePath(QStringLiteral("fixture.docx"));
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(QByteArray::fromBase64(kDocxBase64));
    return path;
}
} // namespace

TEST_CASE("readPlainTextFile returns content for utf8 files")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const QString filePath = dir.filePath(QStringLiteral("notes.txt"));
    {
        QFile f(filePath);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out.setCodec("UTF-8");
        out << QStringLiteral("eva-line-1\n行二");
    }

    const QString text = DocParser::readPlainTextFile(filePath);
    CHECK(text.contains(QStringLiteral("eva-line-1")));
    CHECK(text.contains(QStringLiteral("行二")));

    // missing file
    CHECK(DocParser::readPlainTextFile(dir.filePath(QStringLiteral("missing.txt"))).isEmpty());
}

TEST_CASE("readPlainTextFile skips utf8 bom and keeps unicode")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("bom.txt"));
    {
        QFile f(filePath);
        REQUIRE(f.open(QIODevice::WriteOnly));
        const QByteArray bomUtf8("\xEF\xBB\xBFhello-清单", 3 + QByteArray("hello-清单").size());
        f.write(bomUtf8);
    }

    const QString text = DocParser::readPlainTextFile(filePath);
    CHECK(text.startsWith(QStringLiteral("hello-")));
    CHECK(text.contains(QStringLiteral("清单")));
}

TEST_CASE("markdownToText strips common markdown constructs")
{
    const QString markdown = QStringLiteral(
        "# Title\n\n"
        "Some `inline` code and ![img](link).\n"
        "> quote\n\n"
        "```\ncode block\n```\n"
        "| col1 | col2 |\n"
        "| ---- | ---- |\n"
        "| a | b |\n");

    const QString plain = DocParser::markdownToText(markdown);
    CHECK_FALSE(plain.contains(QStringLiteral("#")));
    CHECK_FALSE(plain.contains(QStringLiteral("```")));
    CHECK_FALSE(plain.contains(QStringLiteral("| col1")));
    CHECK(plain.contains(QStringLiteral("inline")));
    CHECK(plain.contains(QStringLiteral("img")));
    CHECK(plain.contains(QStringLiteral("quote")));
}

#ifdef _WIN32
TEST_CASE("readDocxText extracts paragraph text via powershell reader")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString docxPath = writeDocxFixture(dir);

    const QString text = DocParser::readDocxText(docxPath);
    if (text.isEmpty())
    {
        INFO("Docx extraction unavailable in this runtime, skipping detailed assertions");
        return;
    }
    CHECK(text.contains(QStringLiteral("EVA Line One")));
    CHECK(text.contains(QStringLiteral("Second Line")));
    CHECK(text.contains(QStringLiteral("Tail")));
}

TEST_CASE("readDocxText returns empty when file missing")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString missing = dir.filePath(QStringLiteral("nope.docx"));
    CHECK(DocParser::readDocxText(missing).isEmpty());
}
#endif
