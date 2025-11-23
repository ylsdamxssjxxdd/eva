#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStringList>
#include <QTextStream>

#include "thirdparty/miniz/miniz.h"
#include "utils/docparser.h"

namespace
{
QString writeDocxFixture(QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("fixture.docx"));
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    const QByteArray encoded = QFile::encodeName(path);
    REQUIRE(mz_zip_writer_init_file(&archive, encoded.constData(), 0) != 0);

    const QByteArray contentTypes = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "</Types>");
    REQUIRE(mz_zip_writer_add_mem(&archive, "[Content_Types].xml", contentTypes.constData(), contentTypes.size(),
                                  MZ_DEFAULT_COMPRESSION) != 0);

    const QByteArray rels = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"R1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"word/document.xml\"/>"
        "</Relationships>");
    REQUIRE(mz_zip_writer_add_mem(&archive, "_rels/.rels", rels.constData(), rels.size(), MZ_DEFAULT_COMPRESSION) != 0);

    const QByteArray docXml = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>"
        "<w:p><w:r><w:t>EVA Line One</w:t></w:r></w:p>"
        "<w:p><w:r><w:t>Second Line </w:t></w:r><w:r><w:br/></w:r></w:p>"
        "<w:p><w:r><w:t>Tail</w:t></w:r></w:p>"
        "</w:body>"
        "</w:document>");
    REQUIRE(mz_zip_writer_add_mem(&archive, "word/document.xml", docXml.constData(), docXml.size(),
                                  MZ_DEFAULT_COMPRESSION) != 0);

    REQUIRE(mz_zip_writer_finalize_archive(&archive) != 0);
    mz_zip_writer_end(&archive);
    return path;
}

const QStringList kReadmeAnchors = {
    QString::fromUtf8(u8"测试wps格式文件读取\n1\n1\n3\n3\n4\n1\n1\n3\n3\n4\n1\n1\n3\n3\n4\n1\n1\n3\n3\n4\n1\n1\n3\n3\n4\n6"),
    QString::fromUtf8(u8"# 机体"),
    QString::fromUtf8(u8"## 快速开始"),
    QString::fromUtf8(u8"EVA_BACKEND/<架构>/<系统>/<设备>/<项目>/"),
    QStringLiteral("cmake --build build --config Release -j 8")
};
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

TEST_CASE("readDocxText extracts paragraph text from docx")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString docxPath = writeDocxFixture(dir);

    const QString text = DocParser::readDocxText(docxPath);
    REQUIRE_MESSAGE(!text.isEmpty(), "Docx extraction unavailable in this runtime");
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

TEST_CASE("readWpsText extracts plain text from legacy WPS docs")
{
    const QString samplePath = QStringLiteral(EVA_SOURCE_DIR "/tests/测试.wps");
    QFileInfo fi(samplePath);
    REQUIRE(fi.exists());

    const QString text = DocParser::readWpsText(fi.absoluteFilePath());
    INFO(text.toStdString());
    REQUIRE_FALSE(text.isEmpty());
    for (const QString &anchor : kReadmeAnchors)
    {
        CHECK(text.contains(anchor));
    }
}

TEST_CASE("readWpsText extracts plain text from legacy DOC docs")
{
    const QString samplePath = QStringLiteral(EVA_SOURCE_DIR "/tests/测试.doc");
    QFileInfo fi(samplePath);
    REQUIRE(fi.exists());

    const QString text = DocParser::readWpsText(fi.absoluteFilePath());
    INFO(text.toStdString());
    REQUIRE_FALSE(text.isEmpty());
    for (const QString &anchor : kReadmeAnchors)
    {
        CHECK(text.contains(anchor));
    }
}

TEST_CASE("readXlsxText extracts text from worksheets")
{
    const QString samplePath = QStringLiteral(EVA_SOURCE_DIR "/tests/测试.xlsx");
    QFileInfo fi(samplePath);
    REQUIRE(fi.exists());

    const QString text = DocParser::readXlsxText(fi.absoluteFilePath());
    INFO(text.toStdString());
    REQUIRE_FALSE(text.isEmpty());
    CHECK(text.contains(QStringLiteral("## Sheet")));
    CHECK(text.contains(QStringLiteral("| --- |")));
    CHECK(text.contains(QString::fromUtf8(u8"| qt creator")));
}

TEST_CASE("readPptxText extracts paragraphs from slides")
{
    const QString samplePath = QStringLiteral(EVA_SOURCE_DIR "/resource/软件介绍.pptx");
    QFileInfo fi(samplePath);
    REQUIRE(fi.exists());

    const QString text = DocParser::readPptxText(fi.absoluteFilePath());
    INFO(text.toStdString());
    REQUIRE_FALSE(text.isEmpty());
    CHECK(text.contains(QStringLiteral("## Slide")));
    CHECK(text.contains(QString::fromUtf8(u8"- 机体全面介绍")));
    CHECK(text.contains(QString::fromUtf8(u8"- 四、服务模式")));
}
