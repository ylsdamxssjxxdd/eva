#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <algorithm>
#include <QFile>
#include <QTemporaryDir>

#include "xbackend_args.h"

namespace
{
QString touchFile(QTemporaryDir &dir, const QString &name)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QString();
    f.write("eva");
    f.close();
    return path;
}
} // namespace

TEST_CASE("buildLocalServerArgs composes baseline options")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const QString modelPath = touchFile(tempDir, QStringLiteral("model.gguf"));
    REQUIRE_FALSE(modelPath.isEmpty());

    LocalServerArgsInput input;
    input.settings = SETTINGS{};
    input.settings.nctx = 2048;
    input.settings.hid_parallel = 2;
    input.settings.ngl = 18;
    input.settings.nthread = 6;
    input.settings.hid_batch = 128;
    input.host = QStringLiteral("127.0.0.1");
    input.port = QStringLiteral("8300");
    input.modelPath = modelPath;
    input.resolvedDevice = QStringLiteral("cuda");

    const QStringList args = buildLocalServerArgs(input);
    auto valueAfter = [&](const QString &flag) -> QString
    {
        const int idx = args.indexOf(flag);
        REQUIRE_MESSAGE(idx >= 0, QStringLiteral("flag %1 not found").arg(flag).toStdString().c_str());
        REQUIRE(idx + 1 < args.size());
        return args.at(idx + 1);
    };

    const int modelIdx = args.indexOf(QStringLiteral("-m"));
    REQUIRE(modelIdx >= 0);
    REQUIRE(modelIdx + 1 < args.size());
    CHECK_FALSE(args.at(modelIdx + 1).isEmpty());
    CHECK(valueAfter(QStringLiteral("--host")) == QStringLiteral("127.0.0.1"));
    CHECK(valueAfter(QStringLiteral("--port")) == QStringLiteral("8300"));
    CHECK(valueAfter(QStringLiteral("-c")) == QString::number(4096));
    CHECK(valueAfter(QStringLiteral("-ngl")) == QString::number(input.settings.ngl));
    CHECK(valueAfter(QStringLiteral("--threads")) == QString::number(input.settings.nthread));
    CHECK(valueAfter(QStringLiteral("-b")) == QString::number(input.settings.hid_batch));
    CHECK(valueAfter(QStringLiteral("--parallel")) == QString::number(input.settings.hid_parallel));
    CHECK(args.contains(QStringLiteral("--jinja")));
    CHECK(args.contains(QStringLiteral("--reasoning-format")));
    CHECK(args.contains(QStringLiteral("--verbose-prompt")));
    CHECK(args.contains(QStringLiteral("--no-mmap")));
}

TEST_CASE("buildLocalServerArgs handles lora, mmproj, and cpu devices")
{
    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());

    LocalServerArgsInput input;
    input.settings = SETTINGS{};
    input.settings.nctx = -1;          // force default
    input.settings.hid_parallel = 0;   // fallback to 1
    input.settings.hid_flash_attn = false;
    input.settings.hid_use_mlock = true;
    input.settings.hid_use_mmap = false;
    input.modelPath = touchFile(tempDir, QStringLiteral("model.gguf"));
    input.loraPath = touchFile(tempDir, QStringLiteral("adapter.lora"));
    input.mmprojPath = touchFile(tempDir, QStringLiteral("vision.mmproj"));
    input.resolvedDevice = QStringLiteral("cpu");

    const QStringList args = buildLocalServerArgs(input);
    auto countFlag = [&](const QString &flag)
    { return std::count(args.begin(), args.end(), flag); };

    CHECK(countFlag(QStringLiteral("--no-mmap")) == 1);
    const int loraIdx = args.indexOf(QStringLiteral("--lora"));
    REQUIRE(loraIdx >= 0);
    REQUIRE(loraIdx + 1 < args.size());
    CHECK_FALSE(args.at(loraIdx + 1).isEmpty());

    const int mmprojIdx = args.indexOf(QStringLiteral("--mmproj"));
    REQUIRE(mmprojIdx >= 0);
    REQUIRE(mmprojIdx + 1 < args.size());
    CHECK_FALSE(args.at(mmprojIdx + 1).isEmpty());

    CHECK_FALSE(args.contains(QStringLiteral("-ngl")));
    CHECK(args.contains(QStringLiteral("--mlock")));
    CHECK(args.contains(QStringLiteral("-fa")));
    CHECK(args.contains(QStringLiteral("--reasoning-format")));
    const int hostIdx = args.indexOf(QStringLiteral("--host"));
    REQUIRE(hostIdx >= 0);
    CHECK(args.at(hostIdx + 1) == QStringLiteral("0.0.0.0"));
    const int portIdx = args.indexOf(QStringLiteral("--port"));
    REQUIRE(portIdx >= 0);
    CHECK(args.at(portIdx + 1) == QStringLiteral(DEFAULT_SERVER_PORT));
    const int ctxIdx = args.indexOf(QStringLiteral("-c"));
    REQUIRE(ctxIdx >= 0);
    CHECK(args.at(ctxIdx + 1) == QString::number(DEFAULT_NCTX));
}
