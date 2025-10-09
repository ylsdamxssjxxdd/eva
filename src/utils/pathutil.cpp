// pathutil.cpp - see header
#include "pathutil.h"

#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

static inline QString toNativeAbs(const QString &p)
{
    if (p.isEmpty()) return p;
    const QFileInfo fi(p);
    const QString abs = fi.isAbsolute() ? p : fi.absoluteFilePath();
    return QDir::toNativeSeparators(abs);
}

QString toToolFriendlyPath(const QString &path)
{
    if (path.isEmpty()) return path;

#ifdef Q_OS_WIN
    // Many third-party ggml tools on Windows still use "char*" argv and fopen(),
    // which break on non-ASCII paths. Convert to 8.3 short path if available.
    const QString native = toNativeAbs(path);

    // Prepare buffer size by first calling with zero to get required length.
    const wchar_t *longPath = reinterpret_cast<const wchar_t *>(native.utf16());
    // GetShortPathNameW requires the path to exist.
    DWORD required = GetShortPathNameW(longPath, nullptr, 0);
    if (required == 0) {
        // Fallback: return native path; QProcess will pass Unicode. Some tools may still handle it.
        return native;
    }

    QString out;
    out.resize(int(required)); // includes room for terminator
    wchar_t *buf = reinterpret_cast<wchar_t *>(out.data());
    DWORD written = GetShortPathNameW(longPath, buf, required);
    if (written == 0) {
        return native; // fallback
    }
    // QString length handling: written excludes terminator
    out.resize(int(written));
    return out;
#else
    Q_UNUSED(path);
    return path; // unchanged on non-Windows
#endif
}

#ifdef Q_OS_WIN
static inline QString driveRootFor(const QString &absPath)
{
    // Expect like "C:/..." or "C:\\..."; return e.g. "C:/"
    if (absPath.length() >= 2 && absPath[1] == QChar(':'))
    {
        return absPath.left(3).replace('\\', '/');
    }
    return QString();
}

static inline QString asciiAliasDirOnSameDrive(const QString &absPath)
{
    const QString root = driveRootFor(absPath);
    if (root.isEmpty()) return QString();
    // Place alias under root to keep same volume. Directory name is ASCII only
    QString aliasRoot = QDir(root).filePath("EVA_ALIAS");
    QDir().mkpath(aliasRoot);
    return aliasRoot;
}

static inline QString genAsciiAliasName(const QString &absPath, const QString &ext)
{
    // Use hash of full path to avoid collisions; keep extension for clarity
    QByteArray h = QCryptographicHash::hash(absPath.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QString::fromLatin1("f_") + QString::fromLatin1(h.constData(), h.size()) + (ext.isEmpty() ? QString() : (QStringLiteral(".") + ext));
}

static inline bool createHardLinkIfPossible(const QString &linkPath, const QString &targetPath)
{
    // CreateHardLinkW does not require admin and works on NTFS same volume
    const wchar_t *lpFileName = reinterpret_cast<const wchar_t *>(linkPath.utf16());
    const wchar_t *lpExisting = reinterpret_cast<const wchar_t *>(targetPath.utf16());
    BOOL ok = CreateHardLinkW(lpFileName, lpExisting, nullptr);
    return ok == TRUE;
}
#endif // Q_OS_WIN

bool isAsciiOnly(const QString &s)
{
    for (QChar c : s)
    {
        if (c.unicode() > 0x7F) return false;
    }
    return true;
}

QString ensureToolFriendlyFilePath(const QString &filePath)
{
    if (filePath.isEmpty()) return filePath;
    const QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile())
    {
        // For non-existing files (e.g. output), prefer to just return tool-friendly form of the parent dir
        return toToolFriendlyPath(filePath);
    }

#ifdef Q_OS_WIN
    // Step 1: try short path
    const QString shortOrNative = toToolFriendlyPath(filePath);
    if (isAsciiOnly(shortOrNative)) return shortOrNative;

    // Step 2: build same-drive ASCII alias via hard link, fallback to copy
    const QString abs = toNativeAbs(filePath);
    const QString ext = fi.suffix();
    const QString aliasRoot = asciiAliasDirOnSameDrive(abs);
    if (!aliasRoot.isEmpty())
    {
        const QString aliasName = genAsciiAliasName(abs, ext);
        const QString aliasPath = QDir(aliasRoot).filePath(aliasName);
        if (!QFileInfo::exists(aliasPath))
        {
            // Try hard link first to avoid large copies
            if (!createHardLinkIfPossible(aliasPath, abs))
            {
                // Fallback to copy (may be large)
                if (!QFile::copy(abs, aliasPath))
                {
                    qWarning() << "ensureToolFriendlyFilePath: failed to alias" << abs << "->" << aliasPath;
                    return shortOrNative; // give back what we have
                }
            }
        }
        return aliasPath;
    }
    return shortOrNative;
#else
    Q_UNUSED(filePath);
    return toToolFriendlyPath(filePath);
#endif
}
