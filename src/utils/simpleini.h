#pragma once

#include <QHash>
#include <QString>

namespace simpleini
{
// Parse a flat key=value ini file and return decoded string pairs.
// Lines starting with '#' or ';' are ignored. Keys preserve whitespace inside
// the identifier but ignore surrounding spaces. Values keep leading/trailing
// spaces and support basic escape sequences (\\n, \\r, \\t, \\\", \\\\).
QHash<QString, QString> parseFile(const QString &path, QString *error = nullptr);

// Decode a single escaped value. Exposed for cases where callers need to
// decode fragments that were not processed through parseFile.
QString decodeValue(const QString &value);
} // namespace simpleini
