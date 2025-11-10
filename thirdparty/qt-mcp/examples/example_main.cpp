#include "qmcp/config.h"
#include "qmcp/errors.h"
#include "qmcp/sseclient.h"
#include "qmcp/streamablehttpclient.h"
#include "qmcp/stdioclient.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

#include <memory>

using namespace qmcp;

namespace {

QString jsonToCompact(const QJsonValue& value) {
    if (value.isObject()) {
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    }
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 16);
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isNull()) {
        return QStringLiteral("null");
    }
    return QStringLiteral("undefined");
}

std::unique_ptr<McpClient> makeClient(const ServerConfig& config) {
    if (config.transport == TransportType::Stdio) {
        return std::make_unique<StdioClient>(config);
    }
    if (config.transport == TransportType::StreamableHttp) {
        return std::make_unique<StreamableHttpClient>(config);
    }
    return std::make_unique<SseClient>(config);
}

} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qtmcp-example"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Minimal Qt-based MCP client example"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOpt(
        QStringList{QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("Path to MCP server configuration JSON."),
        QStringLiteral("file"),
        QStringLiteral("../test/test.json"));
    parser.addOption(configOpt);
    parser.process(app);

    const QString configPath = parser.value(configOpt);
    QTextStream out(stdout);
    out << "Loading configuration from " << configPath << '\n';

    QList<ServerConfig> servers;
    try {
        servers = ServerConfigLoader::loadFromFile(configPath);
    } catch (const std::exception& ex) {
        out << "Failed to parse config: " << ex.what() << '\n';
        return 1;
    }

    if (servers.isEmpty()) {
        out << "No active MCP servers found in config.\n";
        return 0;
    }

    for (const ServerConfig& server : servers) {
        out << "\n=== " << server.name << " (" << transportToString(server.transport) << ") ===\n";
        out << "Description: " << server.description << '\n';

        std::unique_ptr<McpClient> client = makeClient(server);
        if (!client->initialize(QStringLiteral("Qt MCP Example"), QCoreApplication::applicationVersion())) {
            out << "Initialization failed for " << server.name << '\n';
            continue;
        }

        out << "Connected. Server capabilities: "
            << jsonToCompact(QJsonValue(client->serverCapabilities())) << '\n';

        try {
            client->ping();
            out << "Ping request succeeded.\n";
        } catch (const McpError& err) {
            out << "Ping failed (" << err.code() << "): " << err.what() << '\n';
        }

        const QJsonArray tools = client->listTools();
        if (tools.isEmpty()) {
            out << "No tools exposed by this server.\n";
        } else {
            out << "Tools:\n";
            for (const QJsonValue& toolValue : tools) {
                const QJsonObject toolObj = toolValue.toObject();
                out << " - " << toolObj.value(QStringLiteral("name")).toString()
                    << ": " << toolObj.value(QStringLiteral("description")).toString() << '\n';
            }
        }

        if (server.toolCall.isValid()) {
            out << "Calling sample tool '" << server.toolCall.name << "'...\n";
            try {
                const QJsonObject result = client->callTool(server.toolCall.name, server.toolCall.arguments);
                out << "Tool result: " << jsonToCompact(QJsonValue(result)) << '\n';
            } catch (const McpError& err) {
                out << "Tool call failed (" << err.code() << "): " << err.what() << '\n';
            }
        } else {
            out << "No exampleToolCall configured; skipping tool invocation.\n";
        }
    }

    return 0;
}
