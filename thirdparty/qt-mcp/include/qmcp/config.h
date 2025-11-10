#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace qmcp {

enum class TransportType {
    Sse,
    StreamableHttp,
    Stdio
};

struct ExampleToolCall {
    QString name;
    QJsonObject arguments;

    bool isValid() const { return !name.isEmpty(); }
};

struct ServerConfig {
    QString key;
    QString name;
    QString description;
    bool isActive = true;
    TransportType transport = TransportType::Sse;
    QUrl baseUrl;
    QString command;
    QStringList args;
    QJsonObject env;
    QJsonObject headers;
    ExampleToolCall toolCall;
};

class ServerConfigLoader {
public:
    static QList<ServerConfig> loadFromFile(const QString& path);
};

QString transportToString(TransportType type);

} // namespace qmcp
