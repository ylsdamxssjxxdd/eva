#ifndef EVA_OPENAI_COMPAT_H
#define EVA_OPENAI_COMPAT_H

#include <QString>
#include <QUrl>

// OpenAI 兼容端点的小工具：
// 不同厂商的“base url”习惯并不一致：
// 1) OpenAI / llama.cpp：base 通常是 host:port（不含版本号），接口固定是 /v1/...
// 2) 火山方舟 Ark：base 本身就带了版本前缀 /api/v3，接口则直接是 /chat/completions、/models 等
// 如果我们一律把 /v1/... 追加到 base 后面，就会把 Ark 拼成 /api/v3/v1/... 导致请求失败。
namespace OpenAiCompat
{
// 判断是否为火山方舟 Ark 的 OpenAI 兼容 base url（典型：https://ark.cn-beijing.volces.com/api/v3）
inline bool isVolcArkV3Base(const QUrl &base)
{
    const QString host = base.host().trimmed().toLower();
    if (host.isEmpty()) return false;

    // 目前已知 Ark 的域名形态：ark.<region>.volces.com
    if (!host.endsWith(QStringLiteral(".volces.com"))) return false;
    if (!host.startsWith(QStringLiteral("ark."))) return false;

    const QString path = base.path().trimmed().toLower();
    return (path == QStringLiteral("/api/v3") || path == QStringLiteral("/api/v3/"));
}

// 根据 base url 返回 chat/completions 路径（带 leading '/'）
inline QString chatCompletionsPath(const QUrl &base)
{
    return isVolcArkV3Base(base) ? QStringLiteral("/chat/completions") : QStringLiteral("/v1/chat/completions");
}

// 根据 base url 返回 completions 路径（带 leading '/'）
inline QString completionsPath(const QUrl &base)
{
    return isVolcArkV3Base(base) ? QStringLiteral("/completions") : QStringLiteral("/v1/completions");
}

// 根据 base url 返回 models 路径（带 leading '/'）
inline QString modelsPath(const QUrl &base)
{
    return isVolcArkV3Base(base) ? QStringLiteral("/models") : QStringLiteral("/v1/models");
}

// 将 base url 与一个“以 / 开头”的相对路径拼成新 URL（不会出现 //）
inline QUrl joinPath(const QUrl &base, const QString &absolutePath)
{
    QUrl url(base);
    url.setFragment(QString());
    url.setQuery(QString());

    QString basePath = url.path();
    basePath = basePath.trimmed();
    if (basePath.isEmpty() || basePath == QStringLiteral("/"))
    {
        basePath.clear();
    }
    else
    {
        while (basePath.endsWith(QLatin1Char('/')))
        {
            basePath.chop(1);
        }
    }

    QString suffix = absolutePath.trimmed();
    if (!suffix.startsWith(QLatin1Char('/')))
    {
        suffix.prepend(QLatin1Char('/'));
    }

    url.setPath(basePath + suffix);
    return url;
}
} // namespace OpenAiCompat

#endif // EVA_OPENAI_COMPAT_H

