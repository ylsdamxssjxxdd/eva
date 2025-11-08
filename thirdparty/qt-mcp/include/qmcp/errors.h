#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QJsonArray>
#include <stdexcept>

namespace qmcp {

class McpError : public std::runtime_error {
public:
    explicit McpError(const QString& message, int code = -32000, const QJsonObject& data = {})
        : std::runtime_error(message.toStdString()), m_code(code), m_data(data) {}

    int code() const noexcept { return m_code; }
    QJsonObject data() const noexcept { return m_data; }

private:
    int m_code;
    QJsonObject m_data;
};

inline QString jsonToString(const QJsonValue& value) {
    if (value.isNull()) {
        return QStringLiteral("null");
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble(), 'g', 16);
    }
    if (value.isString()) {
        return value.toString();
    }
    QJsonDocument doc;
    if (value.isArray()) {
        doc = QJsonDocument(value.toArray());
    } else if (value.isObject()) {
        doc = QJsonDocument(value.toObject());
    }
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

} // namespace qmcp
