#include "qmcp/config.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

#include <stdexcept>

namespace {

using qmcp::ServerConfig;
using qmcp::TransportType;

TransportType parseTransport(const QString& typeValue) {
    if (typeValue.compare(QStringLiteral("stdio"), Qt::CaseInsensitive) == 0) {
        return TransportType::Stdio;
    }
    if (typeValue.compare(QStringLiteral("streamablehttp"), Qt::CaseInsensitive) == 0
        || typeValue.compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        return TransportType::StreamableHttp;
    }
    return TransportType::Sse;
}

QString describeError(const QString& message, const QString& path, const QString& key) {
    if (key.isEmpty()) {
        return QStringLiteral("%1 (%2)").arg(message, path);
    }
    return QStringLiteral("%1 (server: %2, file: %3)").arg(message, key, path);
}

ServerConfig parseServer(const QString& key, const QJsonObject& object, const QString& sourcePath) {
    ServerConfig config;
    config.key = key;
    config.name = object.value(QStringLiteral("name")).toString(key);
    config.description = object.value(QStringLiteral("description")).toString();
    config.isActive = object.value(QStringLiteral("isActive")).toBool(true);
    config.transport = parseTransport(object.value(QStringLiteral("type")).toString(QStringLiteral("sse")));

    if (config.transport == TransportType::Sse || config.transport == TransportType::StreamableHttp) {
        const QString baseUrl = object.value(QStringLiteral("baseUrl")).toString();
        config.baseUrl = QUrl(baseUrl);
        if (!config.baseUrl.isValid() || config.baseUrl.scheme().isEmpty()) {
            throw std::runtime_error(describeError(QStringLiteral("HTTP server is missing a valid baseUrl"),
                                                   sourcePath,
                                                   key)
                                         .toStdString());
        }
    } else {
        config.command = object.value(QStringLiteral("command")).toString();
        if (config.command.isEmpty()) {
            throw std::runtime_error(
                describeError(QStringLiteral("stdio server is missing command"), sourcePath, key).toStdString());
        }
        if (object.contains(QStringLiteral("args")) && object.value(QStringLiteral("args")).isArray()) {
            const auto args = object.value(QStringLiteral("args")).toArray();
            for (const QJsonValue& entry : args) {
                config.args.append(entry.toString());
            }
        }
    }

    if (object.contains(QStringLiteral("env")) && object.value(QStringLiteral("env")).isObject()) {
        config.env = object.value(QStringLiteral("env")).toObject();
    }

    if (object.contains(QStringLiteral("headers")) && object.value(QStringLiteral("headers")).isObject()) {
        config.headers = object.value(QStringLiteral("headers")).toObject();
    }

    if (object.contains(QStringLiteral("exampleToolCall")) && object.value(QStringLiteral("exampleToolCall")).isObject()) {
        QJsonObject sample = object.value(QStringLiteral("exampleToolCall")).toObject();
        config.toolCall.name = sample.value(QStringLiteral("name")).toString();
        if (sample.value(QStringLiteral("arguments")).isObject()) {
            config.toolCall.arguments = sample.value(QStringLiteral("arguments")).toObject();
        }
    }

    return config;
}

QList<QJsonObject> extractContainers(const QJsonDocument& doc) {
    QList<QJsonObject> containers;
    if (doc.isObject()) {
        containers.append(doc.object());
    } else if (doc.isArray()) {
        const auto array = doc.array();
        for (const QJsonValue& value : array) {
            if (value.isObject()) {
                containers.append(value.toObject());
            }
        }
    }
    return containers;
}

QByteArray stripLineComments(const QByteArray& raw) {
    QByteArray cleaned;
    cleaned.reserve(raw.size());
    const QList<QByteArray> lines = raw.split('\n');
    for (const QByteArray& line : lines) {
        QByteArray trimmed = line;
        trimmed = trimmed.trimmed();
        if (trimmed.startsWith("//")) {
            continue;
        }
        cleaned.append(line);
        cleaned.append('\n');
    }
    return cleaned;
}

QList<QByteArray> splitDocuments(const QByteArray& raw) {
    QList<QByteArray> documents;
    const QByteArray data = raw.trimmed();
    if (data.isEmpty()) {
        return documents;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    int start = -1;

    for (int i = 0; i < data.size(); ++i) {
        const char ch = data.at(i);

        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                inString = false;
            }
            continue;
        }

        if (ch == '"') {
            inString = true;
            continue;
        }

        if (ch == '{' || ch == '[') {
            if (depth == 0) {
                start = i;
            }
            ++depth;
        } else if (ch == '}' || ch == ']') {
            --depth;
            if (depth == 0 && start != -1) {
                documents.append(data.mid(start, i - start + 1));
                start = -1;
            }
        }
    }

    if (documents.isEmpty()) {
        documents.append(data);
    }

    return documents;
}

} // namespace

namespace qmcp {

QString transportToString(TransportType type) {
    switch (type) {
    case TransportType::Stdio:
        return QStringLiteral("stdio");
    case TransportType::StreamableHttp:
        return QStringLiteral("streamableHttp");
    case TransportType::Sse:
    default:
        return QStringLiteral("sse");
    }
}

QList<ServerConfig> ServerConfigLoader::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QStringLiteral("Unable to open config file %1").arg(path).toStdString());
    }

    const QByteArray raw = file.readAll();
    const QByteArray cleaned = stripLineComments(raw);
    const QList<QByteArray> documents = splitDocuments(cleaned);

    QList<ServerConfig> configs;

    for (const QByteArray& fragment : documents) {
        if (fragment.trimmed().isEmpty()) {
            continue;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(fragment, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            throw std::runtime_error(QStringLiteral("Invalid JSON in %1: %2")
                                         .arg(path, parseError.errorString())
                                         .toStdString());
        }

        const auto containers = extractContainers(doc);
        for (const QJsonObject& container : containers) {
            if (!container.contains(QStringLiteral("mcpServers"))) {
                continue;
            }
            const QJsonValue serversValue = container.value(QStringLiteral("mcpServers"));
            if (!serversValue.isObject()) {
                continue;
            }

            const QJsonObject servers = serversValue.toObject();
            for (auto it = servers.constBegin(); it != servers.constEnd(); ++it) {
                if (!it.value().isObject()) {
                    continue;
                }
                ServerConfig config = parseServer(it.key(), it.value().toObject(), path);
                if (config.isActive) {
                    configs.append(config);
                }
            }
        }
    }

    return configs;
}

} // namespace qmcp
