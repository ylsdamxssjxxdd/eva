#pragma once

#include <QString>

namespace zip
{
bool extractArchive(const QString &archivePath, const QString &destinationDir, QString *errorMessage = nullptr);
}

