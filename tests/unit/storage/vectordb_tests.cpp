#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <QSqlDatabase>
#include <QTemporaryDir>

#include "utils/vectordb.h"

namespace
{
QString makeDbPath(QTemporaryDir &dir)
{
    return dir.filePath(QStringLiteral("vectordb.sqlite"));
}

void cleanupConnection()
{
    if (QSqlDatabase::contains(QStringLiteral("eva_vectordb")))
    {
        QSqlDatabase::removeDatabase(QStringLiteral("eva_vectordb"));
    }
}

std::vector<double> makeVec(std::initializer_list<double> list)
{
    return std::vector<double>(list);
}
} // namespace

TEST_CASE("VectorDB opens and tracks metadata")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    {
        VectorDB db;
        REQUIRE(db.open(makeDbPath(dir)));
        CHECK(db.currentModelId().isEmpty());
        CHECK(db.currentDim() == 0);

        CHECK(db.setCurrentModel(QStringLiteral("modelA"), 128) == true);  // first bind triggers reset
        CHECK(db.currentModelId() == QStringLiteral("modelA"));
        CHECK(db.currentDim() == 128);

        CHECK(db.setCurrentModel(QStringLiteral("modelA"), 128) == false); // no change
        CHECK(db.setCurrentModel(QStringLiteral("modelB"), 128) == true);  // model switch forces clear
    }
    cleanupConnection();
}

TEST_CASE("VectorDB upsert, load and delete operate on stored vectors")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    {
        VectorDB db;
        REQUIRE(db.open(makeDbPath(dir)));
        db.setCurrentModel(QStringLiteral("modelA"), 3);

        REQUIRE(db.upsertChunk(0, QStringLiteral("chunk-0"), makeVec({1.0, 2.0, 3.0})));
        REQUIRE(db.upsertChunk(1, QStringLiteral("chunk-1"), makeVec({4.0, 5.0, 6.0})));

        auto rows = db.loadAll();
        REQUIRE(rows.size() == 2);
        CHECK(rows.at(0).index == 0);
        CHECK(rows.at(0).chunk == QStringLiteral("chunk-0"));
        CHECK(rows.at(0).value.size() == 3);
        CHECK(rows.at(0).value[0] == doctest::Approx(1.0));

        CHECK(db.deleteByChunk(QStringLiteral("chunk-0")));
        rows = db.loadAll();
        REQUIRE(rows.size() == 1);
        CHECK(rows.at(0).chunk == QStringLiteral("chunk-1"));
    }
    cleanupConnection();
}

TEST_CASE("VectorDB enforces dimension reset on mismatch and pads vectors")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    {
        VectorDB db;
        REQUIRE(db.open(makeDbPath(dir)));
        db.setCurrentModel(QStringLiteral("modelA"), 4);

        REQUIRE(db.upsertChunk(0, QStringLiteral("chunk"), makeVec({1.0, 2.0})));
        auto rows = db.loadAll();
        REQUIRE(rows.size() == 1);
        CHECK(rows.at(0).value.size() == 4);
        CHECK(rows.at(0).value[2] == doctest::Approx(0.0));

        // Changing dimension should trigger clear
        REQUIRE(db.setCurrentModel(QStringLiteral("modelA"), 6) == true);
        CHECK(db.loadAll().isEmpty());
    }
    cleanupConnection();
}
