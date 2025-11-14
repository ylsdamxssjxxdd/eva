#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "utils/pathutil.h"

TEST_CASE("isAsciiOnly detects unicode codepoints")
{
    CHECK(isAsciiOnly(QStringLiteral("ascii_ONLY_123")));
    CHECK_FALSE(isAsciiOnly(QStringLiteral("含有中文")));
}

TEST_CASE("ensureToolFriendlyFilePath returns normalized path for missing files")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString nested = tempDir.filePath(QStringLiteral("子目录/输出.bin"));

    const QString friendly = ensureToolFriendlyFilePath(nested);
    const QString expected = QDir::toNativeSeparators(QFileInfo(nested).absoluteFilePath());
    CHECK(friendly == expected);
}

TEST_CASE("ensureToolFriendlyFilePath keeps files accessible and ASCII friendly")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    const QString filePath = tempDir.filePath(QStringLiteral("sample_data.txt"));
    {
        QFile f(filePath);
        REQUIRE(f.open(QIODevice::WriteOnly));
        f.write("eva-test");
    }

    const QString friendly = ensureToolFriendlyFilePath(filePath);
    CHECK(QFileInfo(friendly).exists());
    CHECK(isAsciiOnly(friendly));

    QFile friendlyFile(friendly);
    REQUIRE(friendlyFile.open(QIODevice::ReadOnly));
    CHECK(friendlyFile.readAll() == QByteArray("eva-test"));
}

TEST_CASE("toToolFriendlyPath resolves relative paths to absolute forms")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString relative = QStringLiteral("relative_dir/sample.txt");
    const QString cwdBefore = QDir::currentPath();
    QDir::setCurrent(tempDir.path());

    const QString friendly = toToolFriendlyPath(relative);
    CHECK(QFileInfo(friendly).isAbsolute());

    QDir::setCurrent(cwdBefore);
}
