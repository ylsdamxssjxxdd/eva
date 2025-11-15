#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "thirdparty/miniz/miniz.h"
#include "utils/zip_extractor.h"

namespace
{
QString createZip(QTemporaryDir &dir, const QList<QPair<QString, QByteArray>> &entries)
{
    const QString archivePath = dir.filePath(QStringLiteral("fixture.zip"));
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    const QByteArray target = QFile::encodeName(archivePath);
    REQUIRE(mz_zip_writer_init_file(&archive, target.constData(), 0) != 0);
    for (const auto &entry : entries)
    {
        const QByteArray nameUtf8 = entry.first.toUtf8();
        const QByteArray payload = entry.second;
        REQUIRE(mz_zip_writer_add_mem(&archive, nameUtf8.constData(), payload.constData(), payload.size(), MZ_DEFAULT_COMPRESSION) != 0);
    }
    REQUIRE(mz_zip_writer_finalize_archive(&archive) != 0);
    mz_zip_writer_end(&archive);
    return archivePath;
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}
} // namespace

TEST_CASE("zip::extractArchive writes files safely into destination directory")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir outDir;
    REQUIRE(outDir.isValid());

    const QString archive = createZip(dir, {{QStringLiteral("notes/readme.txt"), QByteArrayLiteral("EVA-CONTENT")}});
    QString error;
    REQUIRE(zip::extractArchive(archive, outDir.path(), &error));
    CHECK(error.isEmpty());

    const QString extracted = QDir(outDir.path()).filePath(QStringLiteral("notes/readme.txt"));
    REQUIRE(QFile::exists(extracted));
    CHECK(readFile(extracted) == QByteArrayLiteral("EVA-CONTENT"));
}

TEST_CASE("zip::extractArchive rejects archives with traversal entries")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QTemporaryDir outDir;
    REQUIRE(outDir.isValid());

    const QString archive = createZip(dir, {{QStringLiteral("../escape.txt"), QByteArrayLiteral("bad")}});
    QString error;
    CHECK_FALSE(zip::extractArchive(archive, outDir.path(), &error));
    const bool hasGuardMessage = error.contains(QStringLiteral("unsafe path")) ||
                                 error.contains(QStringLiteral("escapes"));
    CHECK(hasGuardMessage);

    const QString escaped = QDir(outDir.path()).filePath(QStringLiteral("escape.txt"));
    CHECK_FALSE(QFile::exists(escaped));
}

TEST_CASE("zip::extractArchive surface errors for missing archives")
{
    QTemporaryDir outDir;
    REQUIRE(outDir.isValid());

    QString error;
    CHECK_FALSE(zip::extractArchive(QStringLiteral("missing.zip"), outDir.path(), &error));
    CHECK(error.contains(QStringLiteral("Archive not found")));
}
