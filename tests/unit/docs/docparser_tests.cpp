#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "utils/docparser.h"

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
