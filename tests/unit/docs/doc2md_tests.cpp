#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include <doc2md/document_converter.h>
#include "thirdparty/miniz/miniz.h"

namespace
{
const QString kDoc2mdTestsDir = QStringLiteral(EVA_SOURCE_DIR "/thirdparty/doc2md/tests");

QCoreApplication *ensureQtApp()
{
    // 同 local_server_args_tests：避免某些环境下 Qt 全局析构顺序导致 exit 阶段崩溃。
    static int argc = 0;
    static char **argv = nullptr;
    static QCoreApplication app(argc, argv);
    return &app;
}

QString sampleFile(const QString &name)
{
    return QDir(kDoc2mdTestsDir).filePath(name);
}

QString convertFileToMarkdown(const QString &path)
{
    const QByteArray encoded = QFile::encodeName(path);
    REQUIRE_MESSAGE(!encoded.isEmpty(), "QFile::encodeName returned empty path");
    const std::string pathStr(encoded.constData(), static_cast<size_t>(encoded.size()));
    const doc2md::ConversionResult result = doc2md::convertFile(pathStr);
    for (const std::string &warn : result.warnings)
    {
        INFO("doc2md warning: " << warn);
    }
    if (!result.success || result.markdown.empty())
    {
        INFO("doc2md failed to parse file: " << path.toStdString());
        REQUIRE(result.success);
        return {};
    }
    return QString::fromUtf8(result.markdown.c_str(), static_cast<int>(result.markdown.size()));
}

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
} // namespace

TEST_CASE("convertFile preserves utf8 plain text")
{
    ensureQtApp();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const QString filePath = dir.filePath(QStringLiteral("notes.txt"));
    {
        QFile f(filePath);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out.setCodec("UTF-8");
        out << QStringLiteral("eva-line-1\n知识库");
    }

    const QString text = convertFileToMarkdown(filePath);
    CHECK(text.contains(QStringLiteral("eva-line-1")));
    CHECK(text.contains(QStringLiteral("知识库")));
}

TEST_CASE("convertFile strips markdown noise")
{
    ensureQtApp();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString filePath = dir.filePath(QStringLiteral("sample.md"));
    {
        QFile f(filePath);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&f);
        out.setCodec("UTF-8");
        out << QStringLiteral(
            "# Title\n\n"
            "Some `inline` code and ![img](link).\n"
            "> quote\n\n"
            "```\ncode block\n```\n"
            "| col1 | col2 |\n"
            "| ---- | ---- |\n"
            "| a | b |\n");
    }

    const QString text = convertFileToMarkdown(filePath);
    CHECK_FALSE(text.contains(QStringLiteral("# Title")));
    CHECK(text.contains(QStringLiteral("inline")));
    CHECK(text.contains(QStringLiteral("img")));
    CHECK(text.contains(QStringLiteral("quote")));
    CHECK_FALSE(text.contains(QStringLiteral("| col1 | col2 |")));
}

TEST_CASE("convertFile extracts docx paragraphs")
{
    ensureQtApp();
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = writeDocxFixture(dir);
    const QString text = convertFileToMarkdown(path);
    CHECK(text.contains(QStringLiteral("EVA Line One")));
    CHECK(text.contains(QStringLiteral("Second Line")));
    CHECK(text.contains(QStringLiteral("Tail")));
}

TEST_CASE("convertFile extracts text from doc/wps samples")
{
    ensureQtApp();
    const QString docPath = sampleFile(QStringLiteral(u"测试.doc"));
    const QString wpsPath = sampleFile(QStringLiteral(u"测试.wps"));
    QFileInfo docFi(docPath);
    QFileInfo wpsFi(wpsPath);
    REQUIRE(docFi.exists());
    REQUIRE(wpsFi.exists());

    const QString docText = convertFileToMarkdown(docFi.absoluteFilePath());
    CHECK(docText.contains(QStringLiteral("| 1 | 1 | 3 | 3 | 4 |")));
    CHECK(docText.contains(QStringLiteral("EVA_MODELS/llm")));

    const QString wpsText = convertFileToMarkdown(wpsFi.absoluteFilePath());
    CHECK(wpsText.contains(QStringLiteral("EVA_MODELS/speech2text")));
    CHECK(wpsText.contains(QStringLiteral("Agent")));
}

TEST_CASE("convertFile extracts worksheets from xlsx")
{
    ensureQtApp();
    const QString path = sampleFile(QStringLiteral(u"测试.xlsx"));
    QFileInfo fi(path);
    REQUIRE(fi.exists());

    const QString text = convertFileToMarkdown(fi.absoluteFilePath());
    CHECK(text.contains(QStringLiteral("## Sheet 1")));
    CHECK(text.contains(QStringLiteral("QML_IMPORT_NAME = \"io.qt.textproperties\"")));
}

TEST_CASE("convertFile extracts pptx slides")
{
    ensureQtApp();
    const QString path = sampleFile(QStringLiteral(u"测试.pptx"));
    QFileInfo fi(path);
    REQUIRE(fi.exists());

    const QString text = convertFileToMarkdown(fi.absoluteFilePath());
    CHECK(text.contains(QStringLiteral("## Slide")));
    CHECK(text.contains(QStringLiteral("https://hf-mirror.com/")));
    CHECK(text.contains(QStringLiteral("llama-server.exe")));
}

TEST_CASE("convertFile keeps et sheets consistent with xlsx")
{
    ensureQtApp();
    const QString etPath = sampleFile(QStringLiteral(u"测试.et"));
    const QString xlsxPath = sampleFile(QStringLiteral(u"测试.xlsx"));
    QFileInfo etFi(etPath);
    QFileInfo xlsxFi(xlsxPath);
    REQUIRE(etFi.exists());
    REQUIRE(xlsxFi.exists());

    const QString etText = convertFileToMarkdown(etFi.absoluteFilePath());
    const QString xlsxText = convertFileToMarkdown(xlsxFi.absoluteFilePath());
    REQUIRE_FALSE(etText.isEmpty());
    REQUIRE_FALSE(xlsxText.isEmpty());
    CHECK(etText.contains(QStringLiteral("## Sheet 1")));
    CHECK(xlsxText.contains(QStringLiteral("## Sheet 1")));
}

TEST_CASE("convertFile extracts dps slides")
{
    ensureQtApp();
    const QString path = sampleFile(QStringLiteral(u"测试.dps"));
    QFileInfo fi(path);
    REQUIRE(fi.exists());

    const QString text = convertFileToMarkdown(fi.absoluteFilePath());
    CHECK(text.contains(QStringLiteral("https://hf-mirror.com/")));
    CHECK(text.contains(QStringLiteral("___PPT10")));
}
