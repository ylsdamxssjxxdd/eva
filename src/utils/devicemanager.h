// DeviceManager - runtime backend discovery and path resolution
#ifndef EVA_DEVICE_MANAGER_H
#define EVA_DEVICE_MANAGER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>

#include "../xconfig.h" // for SFX_NAME and BODY_LINUX_PACK

// Lightweight helper to:
// - detect available backends under EVA_BACKEND/<arch>/<os> (cpu/cuda/vulkan/opencl)
// - remember user's backend choice (auto/cpu/cuda/vulkan/opencl)
// - resolve executable paths for third-party tools per selected backend
class DeviceManager
{
  public:
    struct BackendRole
    {
        QString id;     // role id (e.g., "llama-server-main")
        QString binary; // canonical executable name without suffix
        QString label;  // human-readable label
    };

    struct BackendExecutableInfo
    {
        QString root;         // EVA_BACKEND root folder
        QString arch;         // architecture folder
        QString os;           // operating system folder
        QString device;       // device folder (cpu/cuda/…)
        QString project;      // first-level project folder
        QString programName;  // executable base name (without suffix)
        QString absolutePath; // absolute path to the executable
    };

    // Return available backends by scanning filesystem
    static QStringList availableBackends(); // e.g. ["cpu", "cuda"]

    // Persist current user selection in-process ("auto" to pick best-available)
    static void setUserChoice(const QString &backend); // lower-cased
    static QString userChoice();
    static bool hasCustomOverride();
    static QMap<QString, QString> programOverrides();
    static QString programOverride(const QString &roleId);
    static void setProgramOverride(const QString &roleId, const QString &path);
    static void clearProgramOverride(const QString &roleId);
    static void clearProgramOverrides();
    static QVector<BackendRole> managedRoles();
    static QVector<BackendExecutableInfo> enumerateExecutables(const QString &binaryFilter = QString());

    // Effective backend after applying "auto" and fallbacks
    static QString effectiveBackend();
    // Compute the effective backend as if the given choice were active,
    // without mutating the global user choice. Useful for UI preview
    // (e.g., when combobox is on "auto"). The input should be one of
    // auto/cpu/cuda/vulkan/opencl (case-insensitive).
    static QString effectiveBackendFor(const QString &preferred);

    // Resolve absolute path to a tool in the selected backend folder
    // name examples: "llama-server", "whisper-cli", "sd"
    static QString programPath(const QString &name);

    // Base directory holding backend subfolders (cpu/cuda/...) for current platform
    // Returns the first existing candidate (see candidateBackendRoots).
    static QString backendsRootDir();

    // All candidate root directories we probe in priority order. Only existing
    // directories are returned. Useful for robust lookup (e.g., AppImage
    // outside-bundle EVA_BACKEND next to the .AppImage file).
    static QStringList candidateBackendRoots();

    // Most recent list of probed paths (including non-existing) from the last
    // candidateBackendRoots()/backendsRootDir() invocation. Intended for error
    // diagnostics.
    static QStringList probedBackendRoots();

    // Return normalized architecture id for current runtime
    // One of: x86_64, x86_32, arm64, arm32
    static QString currentArchId();
    // Normalized OS id: win, linux, mac, or win7 on Windows 7/8 family
    static QString currentOsId();

    // The backend device actually resolved for a given program (from last
    // programPath() call). Falls back to effectiveBackend() if unknown.
    static QString lastResolvedDeviceFor(const QString &programName);

    static QStringList preferredOrder();
    // Map a known executable name to its canonical project folder
    // For example: "llama-server" -> "llama.cpp"
    static QString projectForProgram(const QString &name);
};

#endif // EVA_DEVICE_MANAGER_H
