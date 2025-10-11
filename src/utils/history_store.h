// Simple per-session chat history persistence aligned with llama.cpp server slots
#ifndef HISTORY_STORE_H
#define HISTORY_STORE_H

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

// Lightweight session metadata for future retrieval/resume
struct SessionMeta
{
    QString id;          // unique id (timestamp + random suffix)
    QString title;       // short title (first user message)
    QString endpoint;    // server endpoint
    QString model;       // model name/path
    QString system;      // system prompt
    int n_ctx = 0;       // context length at start
    int slot_id = -1;    // llama-server assigned slot id, if any
    QDateTime startedAt; // start time

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["id"] = id;
        o["title"] = title;
        o["endpoint"] = endpoint;
        o["model"] = model;
        o["system"] = system;
        o["n_ctx"] = n_ctx;
        o["slot_id"] = slot_id;
        o["started_at"] = startedAt.toString(Qt::ISODate);
        return o;
    }
};

// Append-only JSONL writer: meta.json + messages.jsonl inside a session dir
class HistoryStore
{
  public:
    // Recent list entry
    struct ListItem
    {
        QString id;
        QString title;
        QDateTime startedAt;
    };

  public:
    explicit HistoryStore(const QString &baseDir)
        : baseDir_(baseDir)
    {
        QDir().mkpath(baseDir_);
    }

    // Begin a new session directory using meta.json + messages.jsonl inside a session dir
    bool begin(const SessionMeta &meta)
    {
        meta_ = meta;
        sessionDir_ = QDir(baseDir_).filePath(meta_.id);
        QDir().mkpath(sessionDir_);
        // write meta.json
        QFile f(QDir(sessionDir_).filePath("meta.json"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(QJsonDocument(meta_.toJson()).toJson(QJsonDocument::Compact));
        f.close();
        // create messages.jsonl
        QFile m(QDir(sessionDir_).filePath("messages.jsonl"));
        if (!m.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return false;
        m.close();
        return true;
    }

    // Append one message object to messages.jsonl (one compact JSON per line)
    void appendMessage(const QJsonObject &msg)
    {
        if (sessionDir_.isEmpty()) return;
        QFile m(QDir(sessionDir_).filePath("messages.jsonl"));
        if (!m.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
        QJsonDocument d(msg);
        QByteArray line = d.toJson(QJsonDocument::Compact);
        line.append('\n');
        m.write(line);
        m.close();
        if (meta_.title.isEmpty() && msg.value("role").toString() == QStringLiteral("user"))
        {
            const QString content = msg.value("content").isString()
                                        ? msg.value("content").toString()
                                        : msg.value("content").toVariant().toString();
            meta_.title = content.left(64);
            saveMeta();
        }
    }

    // Rewrite the entire messages.jsonl from a UI messages array (persist edits)
    // Each element must be a JSON object; one compact JSON per line will be written.
    bool rewriteAllMessages(const QJsonArray &messages)
    {
        if (sessionDir_.isEmpty()) return false;
        QFile m(QDir(sessionDir_).filePath("messages.jsonl"));
        if (!m.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
        for (const auto &v : messages)
        {
            QJsonObject obj = v.toObject();
            if (obj.isEmpty())
                continue; // skip non-object entries to keep file valid
            QJsonDocument d(obj);
            QByteArray line = d.toJson(QJsonDocument::Compact);
            line.append('\n');
            m.write(line);
        }
        m.close();
        return true;
    }

    // Update slot id in meta.json (persist for later resume)
    void updateSlotId(int slot_id)
    {
        if (slot_id < 0) return;
        if (meta_.slot_id == slot_id) return;
        meta_.slot_id = slot_id;
        saveMeta();
    }

    QString sessionId() const { return meta_.id; }
    QString sessionDir() const { return sessionDir_; }

    // Clear current in-memory session context
    void clearCurrent()
    {
        sessionDir_.clear();
        meta_ = SessionMeta();
    }

    // Resume an existing session (load meta.json into memory)
    bool resume(const QString &id)
    {
        const QString dir = QDir(baseDir_).filePath(id);
        QFile f(QDir(dir).filePath("meta.json"));
        if (!f.open(QIODevice::ReadOnly)) return false;
        const auto o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        meta_.id = o.value("id").toString();
        meta_.title = o.value("title").toString();
        meta_.endpoint = o.value("endpoint").toString();
        meta_.model = o.value("model").toString();
        meta_.system = o.value("system").toString();
        meta_.n_ctx = o.value("n_ctx").toInt();
        meta_.slot_id = o.value("slot_id").toInt(-1);
        meta_.startedAt = QDateTime::fromString(o.value("started_at").toString(), Qt::ISODate);
        sessionDir_ = dir;
        return true;
    }

    // Load a session fully (meta + messages)
    bool loadSession(const QString &id, SessionMeta &meta, QJsonArray &msgs) const
    {
        const QString dir = QDir(baseDir_).filePath(id);
        QFile f(QDir(dir).filePath("meta.json"));
        if (!f.open(QIODevice::ReadOnly)) return false;
        const auto o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        meta.id = o.value("id").toString();
        meta.title = o.value("title").toString();
        meta.endpoint = o.value("endpoint").toString();
        meta.model = o.value("model").toString();
        meta.system = o.value("system").toString();
        meta.n_ctx = o.value("n_ctx").toInt();
        meta.slot_id = o.value("slot_id").toInt(-1);
        meta.startedAt = QDateTime::fromString(o.value("started_at").toString(), Qt::ISODate);
        QFile m(QDir(dir).filePath("messages.jsonl"));
        if (!m.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        msgs = QJsonArray();
        while (!m.atEnd())
        {
            QByteArray line = m.readLine();
            if (line.trimmed().isEmpty()) continue;
            QJsonParseError err;
            QJsonDocument d = QJsonDocument::fromJson(line, &err);
            if (err.error == QJsonParseError::NoError) msgs.append(d.object());
        }
        m.close();
        return true;
    }

    // List recent sessions (desc by startedAt)
    QVector<ListItem> listRecent(int maxCount) const
    {
        QVector<ListItem> out;
        QDir dir(baseDir_);
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time | QDir::Reversed);
        for (const auto &fi : entries)
        {
            QFile f(QDir(fi.absoluteFilePath()).filePath("meta.json"));
            if (!f.open(QIODevice::ReadOnly)) continue;
            const auto o = QJsonDocument::fromJson(f.readAll()).object();
            f.close();
            ListItem it;
            it.id = o.value("id").toString(fi.fileName());
            it.title = o.value("title").toString();
            it.startedAt = QDateTime::fromString(o.value("started_at").toString(), Qt::ISODate);
            out.append(it);
        }
        std::sort(out.begin(), out.end(), [](const ListItem &a, const ListItem &b)
                  { return a.startedAt > b.startedAt; });
        if (maxCount > 0 && out.size() > maxCount) out.resize(maxCount);
        return out;
    }

    // Rename title inside meta.json
    bool renameSession(const QString &id, const QString &newTitle)
    {
        const QString dir = QDir(baseDir_).filePath(id);
        QFile f(QDir(dir).filePath("meta.json"));
        if (!f.open(QIODevice::ReadOnly)) return false;
        QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        o["title"] = newTitle;
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
        f.close();
        if (meta_.id == id) meta_.title = newTitle;
        return true;
    }

    // Delete one session directory
    bool deleteSession(const QString &id) const
    {
        const QString dir = QDir(baseDir_).filePath(id);
        QDir d(dir);
        if (!d.exists()) return true;
        return d.removeRecursively();
    }

    // Purge all sessions under baseDir
    bool purgeAll() const
    {
        QDir d(baseDir_);
        bool ok = true;
        const auto entries = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &name : entries)
        {
            QDir sub(d.filePath(name));
            ok = sub.removeRecursively() && ok;
        }
        return ok;
    }

  private:
    void saveMeta()
    {
        if (sessionDir_.isEmpty()) return;
        QFile f(QDir(sessionDir_).filePath("meta.json"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
        f.write(QJsonDocument(meta_.toJson()).toJson(QJsonDocument::Compact));
        f.close();
    }

    QString baseDir_;
    QString sessionDir_;
    SessionMeta meta_;
};

#endif // HISTORY_STORE_H