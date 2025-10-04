#include "prompt_builder.h"

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

static inline QJsonArray fixContentArray(const QJsonArray &arr)
{
    QJsonArray fixed;
    for (const auto &pv : arr)
    {
        if (pv.isObject())
        {
            QJsonObject p = pv.toObject();
            const QString type = p.value("type").toString();
            if (type == "text" || type == "image_url" || type == "input_audio")
            {
                // pass-through supported multimodal parts
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
                                const QString &asstRole)
{
    QJsonArray out;

    // Copy messages, preserving multimodal arrays and stripping past reasoning
    for (const auto &v : uiMessages)
    {
        if (!v.isObject()) continue;
        QJsonObject m = v.toObject();
        const QString role = m.value("role").toString();
        if (!(role == userRole || role == asstRole || role == systemRole)) continue;

        QJsonValue contentVal = m.value("content");
        if (contentVal.isArray())
        {
            m["content"] = fixContentArray(contentVal.toArray());
        }
        else
        {
            const QString s = contentVal.isString() ? contentVal.toString() : contentVal.toVariant().toString();
            if (role == asstRole)
            {
                QString reasoning, content;
                splitThink(s, reasoning, content);
                m.insert("content", content);
            }
            else
            {
                m.insert("content", s);
            }
        }
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