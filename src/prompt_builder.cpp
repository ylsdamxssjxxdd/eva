#include "prompt_builder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace
{
// split <think>...</think> from text; returns reasoning + content
static inline void splitThink(const QString &s, QString &reasoning, QString &content,
                              const QString &begin = QStringLiteral("<think>"),
                              const QString &end = QStringLiteral("</think>"))
{
    reasoning.clear();
    content = s;
    const int endIdx = s.indexOf(end);
    if (endIdx == -1) return;
    QString before = s.left(endIdx);
    const int startIdx = before.indexOf(begin);
    if (startIdx == -1) return;
    const int rStart = startIdx + begin.size();
    reasoning = s.mid(rStart, endIdx - rStart).trimmed();
    content = (s.mid(endIdx + end.size())).trimmed();
}

static inline bool looksLikeHttpUrl(const QString &url)
{
    return url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
           url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

static inline bool looksLikeDataUrl(const QString &url)
{
    return url.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive);
}

static inline QString guessImageMime(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QStringLiteral("jpg") || ext == QStringLiteral("jpeg")) return QStringLiteral("image/jpeg");
    if (ext == QStringLiteral("png")) return QStringLiteral("image/png");
    if (ext == QStringLiteral("webp")) return QStringLiteral("image/webp");
    if (ext == QStringLiteral("gif")) return QStringLiteral("image/gif");
    if (ext == QStringLiteral("bmp")) return QStringLiteral("image/bmp");
    return QStringLiteral("image/png");
}

static inline bool tryLoadLocalImageAsDataUrl(const QString &raw, QString &outDataUrl)
{
    outDataUrl.clear();
    QString path = raw.trimmed();
    if (path.isEmpty()) return false;

    // 兼容 file:// URL
    if (path.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive))
    {
        const QString local = QUrl(path).toLocalFile();
        if (!local.isEmpty()) path = local;
    }

    // 历史里可能存的是 Windows 原生分隔符；Qt 的 QFile/QFileInfo 通常可兼容，但这里统一一下更稳。
    path = QDir::toNativeSeparators(path);
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) return false;

    QFile f(fi.absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray bytes = f.readAll();
    if (bytes.isEmpty()) return false;

    const QString mime = guessImageMime(fi.absoluteFilePath());
    outDataUrl = QStringLiteral("data:%1;base64,").arg(mime) + bytes.toBase64();
    return true;
}

static inline QJsonArray fixContentArray(const QJsonArray &arr, const QJsonArray &localImages)
{
    QJsonArray fixed;
    int imageIndex = 0; // 与 content 中 image_url 的出现顺序一一对应（用于 local_images 映射）
    for (const auto &pv : arr)
    {
        if (pv.isObject())
        {
            QJsonObject p = pv.toObject();
            const QString type = p.value("type").toString();
            if (type == "text" || type == "image_url" || type == "input_audio")
            {
                // 兼容历史：image_url.url 可能是本地路径（为了避免 messages.jsonl 写入 base64）。
                // 这里在“发给模型前”把本地路径重新转回 base64 data URL，保持 OpenAI 兼容。
                if (type == QStringLiteral("image_url"))
                {
                    QJsonObject imageUrlObj = p.value(QStringLiteral("image_url")).toObject();
                    QString url = imageUrlObj.value(QStringLiteral("url")).toString();
                    if (url.isEmpty() && imageIndex >= 0 && imageIndex < localImages.size() && localImages.at(imageIndex).isString())
                    {
                        url = localImages.at(imageIndex).toString();
                    }
                    if (!url.isEmpty() && !looksLikeDataUrl(url) && !looksLikeHttpUrl(url))
                    {
                        QString dataUrl;
                        if (tryLoadLocalImageAsDataUrl(url, dataUrl))
                        {
                            imageUrlObj.insert(QStringLiteral("url"), dataUrl);
                            p.insert(QStringLiteral("image_url"), imageUrlObj);
                        }
                    }
                    imageIndex++;
                }
                // pass-through supported multimodal parts (after optional normalization)
                fixed.append(p);
            }
            else if (type == "audio_url")
            {
                // legacy: map to input_audio { data, format }
                QJsonObject audioUrlObj = p.value("audio_url").toObject();
                const QString url = audioUrlObj.value("url").toString();
                const int comma = url.indexOf(',');
                if (comma != -1)
                {
                    const QString header = url.left(comma);
                    const QString data = url.mid(comma + 1);
                    QString format = QStringLiteral("mp3");
                    if (header.contains("audio/wav"))
                        format = QStringLiteral("wav");
                    else if (header.contains("audio/mpeg"))
                        format = QStringLiteral("mp3");
                    else if (header.contains("audio/ogg"))
                        format = QStringLiteral("mp3");
                    QJsonObject q;
                    q["type"] = "input_audio";
                    QJsonObject ia;
                    ia["data"] = data;
                    ia["format"] = format;
                    q["input_audio"] = ia;
                    fixed.append(q);
                }
            }
        }
        else if (pv.isString())
        {
            QJsonObject q;
            q["type"] = "text";
            q["text"] = pv.toString();
            fixed.append(q);
        }
    }
    return fixed;
}
} // namespace
namespace promptx
{

QJsonArray buildOaiChatMessages(const QJsonArray &uiMessages,
                                const QString &systemPrompt,
                                const QString &systemRole,
                                const QString &userRole,
                                const QString &asstRole,
                                const QString &toolRole)
{
    QJsonArray out;

    // Copy messages and strip past reasoning from assistant; skip explicit think role
    for (const auto &v : uiMessages)
    {
        if (!v.isObject()) continue;
        QJsonObject m = v.toObject();
        const QString role = m.value("role").toString();
        if (!(role == userRole || role == asstRole || role == systemRole || role == toolRole)) continue;

        if (role == QStringLiteral("think"))
            continue;

        // EVA 本地扩展字段：仅用于历史恢复/记录条展示，不应发给模型
        const QJsonArray localImages = m.value(QStringLiteral("local_images")).toArray();

        QJsonValue contentVal = m.value("content");
        if (contentVal.isArray())
        {
            m["content"] = fixContentArray(contentVal.toArray(), localImages);
        }
        else
        {
            const QString s = contentVal.isString() ? contentVal.toString() : contentVal.toVariant().toString();
            if (role == asstRole)
            {
                QString reasoningExisting = m.value("reasoning_content").toString();
                if (reasoningExisting.isEmpty()) reasoningExisting = m.value("thinking").toString();
                QString reasoningInline, content = s;
                splitThink(s, reasoningInline, content);
                m.insert("content", content);
                if (reasoningExisting.isEmpty()) reasoningExisting = reasoningInline;
                if (!reasoningExisting.isEmpty())
                {
                    m.insert("reasoning_content", reasoningExisting);
                }
                else
                {
                    m.remove("reasoning_content");
                }
                m.remove("thinking");
            }
            else
            {
                m.insert("content", s);
            }
        }

        // 移除本地扩展字段（避免污染 OpenAI 兼容请求）
        m.remove(QStringLiteral("local_images"));
        m.remove(QStringLiteral("tool"));

        out.append(m);
    }

    // Ensure first message is the system prompt
    if (out.isEmpty())
    {
        QJsonObject sys;
        sys.insert("role", systemRole);
        sys.insert("content", systemPrompt);
        out.append(sys);
        return out;
    }
    const QJsonObject first = out.at(0).toObject();
    if (first.value("role").toString() != systemRole)
    {
        QJsonObject sys;
        sys.insert("role", systemRole);
        sys.insert("content", systemPrompt);
        QJsonArray fixed;
        fixed.append(sys);
        for (const auto &v2 : out) fixed.append(v2);
        return fixed;
    }

    return out;
}

} // namespace promptx
