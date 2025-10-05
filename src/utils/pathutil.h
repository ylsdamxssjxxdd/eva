// pathutil.h - utilities for making file paths safe for third-party tools
#ifndef EVA_PATHUTIL_H
#define EVA_PATHUTIL_H

#include <QString>

// Return a path representation that external third-party command-line tools can handle.
// - On Windows, many upstream tools do not support Unicode paths when built with narrow char APIs.
//   We attempt to convert to an ASCII-only 8.3 short path (GetShortPathNameW).
// - On other platforms, the original path is returned.
// The function is a no-op for empty paths.
QString toToolFriendlyPath(const QString &path);

// Stronger variant for existing files: ensure the returned path is ASCII-only by creating
// a same-volume alias (hard link) or copying to an ASCII temp alias if 8.3 short name is not available.
// - This avoids huge copies if we can create a hard link (NTFS, same drive). If not, falls back to copy.
// - On non-Windows, returns the absolute native path.
QString ensureToolFriendlyFilePath(const QString &filePath);

// Quick predicate; useful for tests and conditional handling
bool isAsciiOnly(const QString &s);

#endif // EVA_PATHUTIL_H
