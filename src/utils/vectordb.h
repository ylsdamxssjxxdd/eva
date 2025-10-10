// vectordb.h - Simple SQLite-backed vector store for embeddings
// Stores (index, chunk, vector blob) bound to a single model_id and dimension.
// Uses QtSql (QSQLITE) and keeps schema minimal. Vectors are stored as raw
// bytes of doubles to match in-memory representation.

#pragma once

#include <QByteArray>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <QVector>
#include <vector>

#include "../xconfig.h" // Embedding_vector

class VectorDB
{
  public:
    VectorDB();
    ~VectorDB();

    bool open(const QString &dbPath);
    void close();

    // Set or switch current model binding; clears embeddings if mismatched.
    // If dim <= 0, keep previous dim (or 0 if none). Returns true if a reset occurred.
    bool setCurrentModel(const QString &modelId, int dim);

    QString currentModelId() const { return modelId_; }
    int currentDim() const { return dim_; }

    // CRUD
    bool clearAll();
    bool upsertChunk(int idx, const QString &chunk, const std::vector<double> &vec);
    bool deleteByChunk(const QString &chunk);
    QVector<Embedding_vector> loadAll() const; // ordered by idx asc

  private:
    bool ensureSchema();
    bool setMeta(const QString &key, const QVariant &val);
    QVariant getMeta(const QString &key) const;

    static QByteArray vecToBlob(const std::vector<double> &vec);
    static std::vector<double> blobToVec(const QByteArray &blob, int expectDim);

  private:
    QSqlDatabase db_;
    bool opened_ = false;
    QString modelId_;
    int dim_ = 0;
};
