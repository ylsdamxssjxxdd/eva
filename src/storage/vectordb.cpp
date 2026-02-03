// vectordb.cpp - implementation

#include "storage/vectordb.h"

#include <QDir>
#include <QSqlError>
#include <QSqlRecord>
#include <QtDebug>

VectorDB::VectorDB() {}
VectorDB::~VectorDB()
{
    close();
}

bool VectorDB::open(const QString &dbPath)
{
    if (opened_) return true;

    // Ensure parent dir exists
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    db_ = QSqlDatabase::addDatabase("QSQLITE", QStringLiteral("eva_vectordb"));
    db_.setDatabaseName(dbPath);
    if (!db_.open())
    {
        qWarning() << "VectorDB open failed:" << db_.lastError().text();
        return false;
    }
    opened_ = ensureSchema();

    // Load meta
    if (opened_)
    {
        modelId_ = getMeta("model_id").toString();
        dim_ = getMeta("dim").toInt();
    }
    return opened_;
}

void VectorDB::close()
{
    if (!opened_) return;
    db_.close();
    opened_ = false;
}

bool VectorDB::ensureSchema()
{
    QSqlQuery q(db_);

    // meta table for simple key/value
    if (!q.exec("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)"))
    {
        qWarning() << "VectorDB schema(meta) failed:" << q.lastError().text();
        return false;
    }

    // embeddings table - chunk unique to allow UPSERT
    if (!q.exec("CREATE TABLE IF NOT EXISTS embeddings (\n"
                "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                "  idx INTEGER NOT NULL,\n"
                "  chunk TEXT NOT NULL UNIQUE,\n"
                "  vector BLOB NOT NULL\n"
                ")"))
    {
        qWarning() << "VectorDB schema(embeddings) failed:" << q.lastError().text();
        return false;
    }

    // index for order and lookups
    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_embeddings_idx ON embeddings(idx)"))
    {
        qWarning() << "VectorDB index failed:" << q.lastError().text();
        return false;
    }

    return true;
}

bool VectorDB::setMeta(const QString &key, const QVariant &val)
{
    QSqlQuery q(db_);
    q.prepare("INSERT INTO meta(key, value) VALUES(:k, :v)\n"
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.bindValue(":k", key);
    q.bindValue(":v", val.toString());
    if (!q.exec())
    {
        qWarning() << "VectorDB setMeta failed:" << key << q.lastError().text();
        return false;
    }
    return true;
}

QVariant VectorDB::getMeta(const QString &key) const
{
    QSqlQuery q(db_);
    q.prepare("SELECT value FROM meta WHERE key = :k");
    q.bindValue(":k", key);
    if (!q.exec())
    {
        qWarning() << "VectorDB getMeta failed:" << key << q.lastError().text();
        return {};
    }
    if (q.next()) return q.value(0);
    return {};
}

bool VectorDB::setCurrentModel(const QString &modelId, int dim)
{
    if (!opened_) return false;
    const QString newId = modelId;
    const int newDim = (dim > 0 ? dim : dim_);

    bool reset = false;
    if (modelId_ != newId || (dim_ > 0 && newDim > 0 && dim_ != newDim))
    {
        // Model changed or dimension mismatch: clear store to avoid mixing
        clearAll();
        reset = true;
    }
    modelId_ = newId;
    dim_ = newDim;

    setMeta("model_id", modelId_);
    if (dim_ > 0) setMeta("dim", dim_);
    return reset;
}

bool VectorDB::clearAll()
{
    if (!opened_) return false;
    QSqlQuery q(db_);
    if (!q.exec("DELETE FROM embeddings"))
    {
        qWarning() << "VectorDB clearAll failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QByteArray VectorDB::vecToBlob(const std::vector<double> &vec)
{
    if (vec.empty()) return {};
    QByteArray blob;
    blob.resize(static_cast<int>(vec.size() * sizeof(double)));
    memcpy(blob.data(), vec.data(), blob.size());
    return blob;
}

std::vector<double> VectorDB::blobToVec(const QByteArray &blob, int expectDim)
{
    std::vector<double> out;
    const int n = blob.size() / static_cast<int>(sizeof(double));
    if (n <= 0) return out;
    out.resize(n);
    memcpy(out.data(), blob.data(), n * sizeof(double));
    if (expectDim > 0 && n != expectDim)
    {
        // Tolerate by padding or truncating to expected dim
        std::vector<double> fixed(expectDim, 0.0);
        const int m = std::min(expectDim, n);
        std::copy(out.begin(), out.begin() + m, fixed.begin());
        return fixed;
    }
    return out;
}

bool VectorDB::upsertChunk(int idx, const QString &chunk, const std::vector<double> &vec)
{
    if (!opened_) return false;
    QSqlQuery q(db_);
    q.prepare("INSERT INTO embeddings(idx, chunk, vector) VALUES(:i, :c, :v)\n"
              "ON CONFLICT(chunk) DO UPDATE SET idx = excluded.idx, vector = excluded.vector");
    q.bindValue(":i", idx);
    q.bindValue(":c", chunk);
    q.bindValue(":v", vecToBlob(vec));
    if (!q.exec())
    {
        qWarning() << "VectorDB upsertChunk failed:" << q.lastError().text();
        return false;
    }
    return true;
}

bool VectorDB::deleteByChunk(const QString &chunk)
{
    if (!opened_) return false;
    QSqlQuery q(db_);
    q.prepare("DELETE FROM embeddings WHERE chunk = :c");
    q.bindValue(":c", chunk);
    if (!q.exec())
    {
        qWarning() << "VectorDB deleteByChunk failed:" << q.lastError().text();
        return false;
    }
    return true;
}

QVector<Embedding_vector> VectorDB::loadAll() const
{
    QVector<Embedding_vector> out;
    if (!opened_) return out;

    QSqlQuery q(db_);
    if (!q.exec("SELECT idx, chunk, vector FROM embeddings ORDER BY idx ASC"))
    {
        qWarning() << "VectorDB loadAll failed:" << q.lastError().text();
        return out;
    }
    while (q.next())
    {
        Embedding_vector ev{};
        ev.index = q.value(0).toInt();
        ev.chunk = q.value(1).toString();
        ev.value = blobToVec(q.value(2).toByteArray(), dim_);
        out.push_back(std::move(ev));
    }
    return out;
}
