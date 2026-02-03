// Simple per-session chat history persistence aligned with llama.cpp server slots
#ifndef HISTORY_STORE_H
#define HISTORY_STORE_H

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
        // 历史记录需要“可恢复”，但也要避免把 data:image/png;base64,... 这种超长内容直接写入磁盘。
        // 约定：
        // - UI 运行期/发给模型：仍使用 base64 data URL（OpenAI 兼容 schema）
        // - 写入历史：把 image_url.url 中的 base64 替换为本地路径（来自 local_images），显著降低 messages.jsonl 体积
        const QJsonObject stored = sanitizeMessageForStorage(msg);
        QJsonDocument d(stored);
        QByteArray line = d.toJson(QJsonDocument::Compact);
        line.append('\n');
        m.write(line);
        m.close();
        // 生成会话标题：优先取“第一条用户消息”的文本摘要。
        // 注意：挂载桌面控制器/多模态输入时，user.content 可能是数组（[{type:"text",...},{type:"image_url",...}]），
        // 旧实现对数组使用 toVariant().toString() 会得到空串，导致历史记录一直显示 (untitled)。
        if (meta_.title.isEmpty() && stored.value("role").toString() == QStringLiteral("user"))
        {
            auto sanitizeTitle = [](QString s) -> QString {
                // 仅做轻量清洗：压缩空白、去掉换行，避免标题在列表中撑开。
                s.replace('\n', ' ').replace('\r', ' ');
                s = s.simplified();
                // 再兜底一次：极端情况下 simplified 仍可能为空
                return s.trimmed();
            };

            auto deriveTitleFromUserMessage = [&](const QJsonObject &m) -> QString {
                const QJsonValue contentVal = m.value("content");

                // 1) 纯文本：最常见
                if (contentVal.isString())
                {
                    return sanitizeTitle(contentVal.toString()).left(64);
                }

                // 2) 多模态数组：优先取第一段文本（type=text 的 text 字段）
                if (contentVal.isArray())
                {
                    const QJsonArray parts = contentVal.toArray();
                    for (const QJsonValue &pv : parts)
                    {
                        if (!pv.isObject()) continue;
                        const QJsonObject po = pv.toObject();
                        const QString type = po.value("type").toString();
                        // 兼容 EVA 自己组装的 {type:"text", text:"..."} 结构
                        if (type == QStringLiteral("text"))
                        {
                            const QString t = sanitizeTitle(po.value("text").toString());
                            if (!t.isEmpty()) return t.left(64);
                        }
                        // 兼容少数实现可能直接放 {text:"..."} 的情况
                        const QString t2 = sanitizeTitle(po.value("text").toString());
                        if (!t2.isEmpty()) return t2.left(64);
                    }

                    // 3) 没有文本时：用附件占位，至少不要让标题为空
                    bool hasImage = false;
                    bool hasAudio = false;
                    for (const QJsonValue &pv : parts)
                    {
                        if (!pv.isObject()) continue;
                        const QString type = pv.toObject().value("type").toString();
                        if (type == QStringLiteral("image_url")) hasImage = true;
                        if (type == QStringLiteral("audio_url") || type == QStringLiteral("input_audio")) hasAudio = true;
                    }

                    // 优先使用本地图片路径（若存在），可读性更好
                    const QJsonArray locals = m.value(QStringLiteral("local_images")).toArray();
                    if (!locals.isEmpty())
                    {
                        const QString firstPath = locals.first().toString();
                        const QString name = QFileInfo(firstPath).fileName();
                        const QString label = name.isEmpty() ? firstPath : name;
                        return sanitizeTitle(label).left(64);
                    }

                    if (hasImage) return QStringLiteral("[image]");
                    if (hasAudio) return QStringLiteral("[audio]");
                }

                // 4) 其它类型：尽量从 variant 中提取，但要避免空串
                const QString fallback = sanitizeTitle(contentVal.toVariant().toString());
                if (!fallback.isEmpty()) return fallback.left(64);
                return QString();
            };

            const QString title = deriveTitleFromUserMessage(stored);
            if (!title.isEmpty())
            {
                meta_.title = title;
                saveMeta();
            }
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
            // 与 appendMessage() 一致：重写历史时也要清理 base64 图片，避免历史文件膨胀。
            const QJsonObject stored = sanitizeMessageForStorage(obj);
            QJsonDocument d(stored);
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
    // 将一条消息转换为“适合落盘”的历史格式：
    // - 目标：避免把 data:image/...;base64,... 的超长字符串写入 messages.jsonl
    // - 手段：当 content 为数组且包含 image_url 时：
    //   1) 若 image_url.url 是 base64 data URL，则用 local_images 中同序号的本地路径替换
    //   2) 若找不到可用路径，则直接丢弃该 image_url part（宁可少一张图，也不要写入巨量 base64）
    // 注意：
    // - 这里只做“历史落盘”清理，不改变 UI 内存/发给模型的数据结构；
    // - local_images 为 EVA 的本地扩展字段，专用于历史恢复与 UI 展示，不会发给模型。
    static inline QJsonObject sanitizeMessageForStorage(const QJsonObject &msg)
    {
        QJsonObject out = msg;
        const QJsonValue contentVal = out.value(QStringLiteral("content"));
        if (!contentVal.isArray()) return out;

        const QJsonArray parts = contentVal.toArray();
        if (parts.isEmpty()) return out;

        const QJsonArray locals = out.value(QStringLiteral("local_images")).toArray();
        int imageIndex = 0; // 与 content 中 image_url 的出现顺序一一对应

        QJsonArray fixed;
        for (const QJsonValue &pv : parts)
        {
            if (!pv.isObject())
            {
                fixed.append(pv);
                continue;
            }
            QJsonObject po = pv.toObject();
            const QString type = po.value(QStringLiteral("type")).toString();
            if (type != QStringLiteral("image_url"))
            {
                fixed.append(po);
                continue;
            }

            QJsonObject iu = po.value(QStringLiteral("image_url")).toObject();
            const QString url = iu.value(QStringLiteral("url")).toString();
            const bool isDataUrl = url.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive) &&
                                   url.contains(QStringLiteral(";base64,"), Qt::CaseInsensitive);
            if (!isDataUrl)
            {
                // 不是 base64 data URL（例如 http(s) 链接或本地路径），体积可控，直接保留
                fixed.append(po);
                imageIndex++;
                continue;
            }

            // base64 data URL：尝试用 local_images 替换为本地路径
            QString replPath;
            if (imageIndex >= 0 && imageIndex < locals.size() && locals.at(imageIndex).isString())
            {
                replPath = locals.at(imageIndex).toString();
            }
            imageIndex++;

            if (replPath.isEmpty())
            {
                // 找不到路径：跳过该图片 part，避免把巨量 base64 落盘
                continue;
            }
            iu.insert(QStringLiteral("url"), replPath);
            po.insert(QStringLiteral("image_url"), iu);
            fixed.append(po);
        }

        out.insert(QStringLiteral("content"), fixed);
        return out;
    }

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
