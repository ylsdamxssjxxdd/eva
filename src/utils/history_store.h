// Simple per-session chat history persistence aligned with llama.cpp server slots
#ifndef HISTORY_STORE_H
#define HISTORY_STORE_H

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QString>

// Lightweight session metadata for future retrieval/resume
struct SessionMeta {
    QString id;             // unique id (timestamp + random suffix)
    QString title;          // short title (first user message)
    QString endpoint;       // server endpoint
    QString model;          // model name/path
    QString system;         // system prompt
    int     n_ctx = 0;      // context length at start
    int     slot_id = -1;   // llama-server assigned slot id, if any
    QDateTime startedAt;    // start time

    QJsonObject toJson() const {
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
class HistoryStore {
public:
    explicit HistoryStore(const QString &baseDir)
        : baseDir_(baseDir) {
        QDir().mkpath(baseDir_);
    }

    // Begin a new session directory using meta.id; writes meta.json and an empty messages.jsonl
    bool begin(const SessionMeta &meta) {
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
    void appendMessage(const QJsonObject &msg) {
        if (sessionDir_.isEmpty()) return;
        QFile m(QDir(sessionDir_).filePath("messages.jsonl"));
        if (!m.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
        QJsonDocument d(msg);
        QByteArray line = d.toJson(QJsonDocument::Compact);
        line.append('\n');
        m.write(line);
        m.close();
        if (meta_.title.isEmpty() && msg.value("role").toString() == QStringLiteral("user")) {
            const QString content = msg.value("content").isString()
                                        ? msg.value("content").toString()
                                        : msg.value("content").toVariant().toString();
            meta_.title = content.left(64);
            saveMeta();
        }
    }

    // Update slot id in meta.json (persist for later resume)
    void updateSlotId(int slot_id) {
        if (slot_id < 0) return;
        if (meta_.slot_id == slot_id) return;
        meta_.slot_id = slot_id;
        saveMeta();
    }

    QString sessionId() const { return meta_.id; }
    QString sessionDir() const { return sessionDir_; }

private:
    void saveMeta() {
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

