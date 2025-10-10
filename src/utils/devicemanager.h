// DeviceManager - runtime backend discovery and path resolution
#ifndef EVA_DEVICE_MANAGER_H
#define EVA_DEVICE_MANAGER_H

#include <QString>
#include <QStringList>

#include "../xconfig.h" // for SFX_NAME and BODY_LINUX_PACK

// Lightweight helper to:
// - detect available backends under the app directory (cpu/cuda/vulkan/opencl)
// - remember user's backend choice (auto/cpu/cuda/vulkan/opencl)
// - resolve executable paths for third-party tools per selected backend
class DeviceManager
{
  public:
    // Return available backends by scanning filesystem
    static QStringList availableBackends(); // e.g. ["cpu", "cuda"]

    // Persist current user selection in-process ("auto" to pick best-available)
    static void setUserChoice(const QString &backend); // lower-cased
    static QString userChoice();

    // Effective backend after applying "auto" and fallbacks
    static QString effectiveBackend();

    // Resolve absolute path to a tool in the selected backend folder
    // name examples: "llama-server", "whisper-cli", "sd"
    static QString programPath(const QString &name);

    // Base directory holding backend subfolders (cpu/cuda/...) for current platform
    static QString backendsRootDir();

    // Return normalized architecture id for current runtime
    // One of: x86_64, x86_32, arm64, arm32
    static QString currentArchId();

  private:
    static QStringList preferredOrder();
    // Map a known executable name to its canonical project folder
    // For example: "llama-server" -> "llama.cpp"
    static QString projectForProgram(const QString &name);
};

#endif // EVA_DEVICE_MANAGER_H

