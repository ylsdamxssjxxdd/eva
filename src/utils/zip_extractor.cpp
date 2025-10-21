#include "zip_extractor.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QObject>
#include <QtGlobal>
#include <cstring>

#include "thirdparty/miniz/miniz.h"

namespace
{
bool ensureDirectory(QDir &baseDir, const QString &relativePath, QString *errorMessage)
{
    const QString parentRelative = QFileInfo(relativePath).path();
    if (parentRelative.isEmpty() || parentRelative == QStringLiteral("."))
    {
        return true;
    }
    if (baseDir.mkpath(parentRelative)) return true;
    if (errorMessage)
    {
        *errorMessage = QObject::tr("Failed to create directory: %1").arg(baseDir.filePath(parentRelative));
    }
    return false;
}

bool isSubPath(const QString &base, const QString &candidate)
{
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    QString normalizedBase = base;
    QString normalizedCandidate = candidate;
    normalizedBase.replace('\\', '/');
    normalizedCandidate.replace('\\', '/');
    const QString baseWithSlash = normalizedBase.endsWith(QChar('/')) ? normalizedBase : normalizedBase + QChar('/');
    if (!normalizedCandidate.endsWith(QChar('/')))
        return normalizedCandidate.startsWith(baseWithSlash, sensitivity) ||
               normalizedCandidate.compare(normalizedBase, sensitivity) == 0;
    return normalizedCandidate.startsWith(baseWithSlash, sensitivity) ||
           normalizedCandidate.compare(baseWithSlash, sensitivity) == 0;
}

class ZipReaderGuard
{
public:
    explicit ZipReaderGuard(mz_zip_archive &archive)
        : archive_(archive), active_(true)
    {
    }
    ZipReaderGuard(const ZipReaderGuard &) = delete;
    ZipReaderGuard &operator=(const ZipReaderGuard &) = delete;
    ~ZipReaderGuard()
    {
        if (active_) mz_zip_reader_end(&archive_);
    }
    void release() { active_ = false; }

private:
    mz_zip_archive &archive_;
    bool active_;
};
} // namespace

namespace zip
{
bool extractArchive(const QString &archivePath, const QString &destinationDir, QString *errorMessage)
{
    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile())
    {
        if (errorMessage) *errorMessage = QObject::tr("Archive not found: %1").arg(archivePath);
        return false;
    }

    const QString baseDestination = QDir::cleanPath(QFileInfo(destinationDir).absoluteFilePath());
    QDir destDir(baseDestination);
    if (!destDir.exists() && !destDir.mkpath(QStringLiteral(".")))
    {
        if (errorMessage) *errorMessage = QObject::tr("Failed to prepare destination: %1").arg(baseDestination);
        return false;
    }

    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    const QByteArray archiveName = QFile::encodeName(archiveInfo.absoluteFilePath());
    if (!mz_zip_reader_init_file(&archive, archiveName.constData(), 0))
    {
        if (errorMessage) *errorMessage = QObject::tr("Failed to open archive: %1").arg(archivePath);
        return false;
    }
    ZipReaderGuard guard(archive);

    const mz_uint fileCount = mz_zip_reader_get_num_files(&archive);
    for (mz_uint i = 0; i < fileCount; ++i)
    {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&archive, i, &stat))
        {
            if (errorMessage) *errorMessage = QObject::tr("Failed to read archive entry metadata.");
            return false;
        }

        QString relativePath = QString::fromUtf8(stat.m_filename);
        if (relativePath.isEmpty()) continue;
        relativePath.replace('\\', '/');
        while (relativePath.startsWith('/')) relativePath.remove(0, 1);

        const QString cleanedRelative = QDir::cleanPath(relativePath);
        if (cleanedRelative.isEmpty() || cleanedRelative == QStringLiteral("."))
        {
            continue;
        }
        if (cleanedRelative.contains(QStringLiteral("../")) || cleanedRelative.startsWith(QStringLiteral("../")) ||
            cleanedRelative == QStringLiteral(".."))
        {
            if (errorMessage) *errorMessage = QObject::tr("Archive contains unsafe path: %1").arg(relativePath);
            return false;
        }

        const QString absoluteOutput = QDir::cleanPath(destDir.filePath(cleanedRelative));
        if (!isSubPath(baseDestination, absoluteOutput))
        {
            if (errorMessage) *errorMessage = QObject::tr("Archive entry escapes destination: %1").arg(relativePath);
            return false;
        }

        if (mz_zip_reader_is_file_a_directory(&archive, i))
        {
            if (!destDir.mkpath(cleanedRelative))
            {
                if (errorMessage)
                {
                    *errorMessage = QObject::tr("Failed to create directory: %1").arg(absoluteOutput);
                }
                return false;
            }
            continue;
        }

        if (!ensureDirectory(destDir, cleanedRelative, errorMessage)) return false;

        const QByteArray outputName = QFile::encodeName(absoluteOutput);
        if (!mz_zip_reader_extract_to_file(&archive, i, outputName.constData(), 0))
        {
            if (errorMessage) *errorMessage = QObject::tr("Failed to extract %1").arg(relativePath);
            return false;
        }
    }
    guard.release();
    mz_zip_reader_end(&archive);
    return true;
}
} // namespace zip
