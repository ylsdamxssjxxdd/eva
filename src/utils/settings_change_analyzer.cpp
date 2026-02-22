#include "utils/settings_change_analyzer.h"

namespace
{
void appendUniqueIfChanged(bool changed, const QString &tag, QStringList *out)
{
    if (!changed || !out) return;
    const QString normalized = tag.trimmed();
    if (normalized.isEmpty()) return;
    if (!out->contains(normalized)) out->append(normalized);
}

bool isTrimmedCaseInsensitiveEqual(const QString &lhs, const QString &rhs)
{
    return lhs.trimmed().compare(rhs.trimmed(), Qt::CaseInsensitive) == 0;
}

bool isNglEquivalent(int lhs, int rhs, int knownMaxNgl)
{
    if (lhs == rhs) return true;
    if (knownMaxNgl > 0)
    {
        if ((lhs == 999 && rhs == knownMaxNgl) || (rhs == 999 && lhs == knownMaxNgl))
        {
            return true;
        }
    }
    return false;
}
} // namespace

SettingsChangeSummary analyzeSettingsChanges(const SETTINGS &beforeSettings,
                                            const SETTINGS &afterSettings,
                                            const QString &beforePort,
                                            const QString &afterPort,
                                            const QString &beforeDevice,
                                            const QString &afterDevice,
                                            bool backendOverrideDirty,
                                            int knownMaxNgl)
{
    SettingsChangeSummary summary;

    // 1) 先分析需要重启后端的变更项（与 LocalServerManager::buildArgs 保持一致）
    appendUniqueIfChanged(beforeSettings.modelpath != afterSettings.modelpath, QStringLiteral("model"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.mmprojpath != afterSettings.mmprojpath, QStringLiteral("mmproj"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.lorapath != afterSettings.lorapath, QStringLiteral("lora"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.nctx != afterSettings.nctx, QStringLiteral("nctx"), &summary.restartItems);
    appendUniqueIfChanged(!isNglEquivalent(beforeSettings.ngl, afterSettings.ngl, knownMaxNgl), QStringLiteral("ngl"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.nthread != afterSettings.nthread, QStringLiteral("nthread"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.hid_batch != afterSettings.hid_batch, QStringLiteral("batch"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.hid_parallel != afterSettings.hid_parallel, QStringLiteral("parallel"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.hid_use_mmap != afterSettings.hid_use_mmap, QStringLiteral("mmap"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.hid_use_mlock != afterSettings.hid_use_mlock, QStringLiteral("mlock"), &summary.restartItems);
    appendUniqueIfChanged(beforeSettings.hid_flash_attn != afterSettings.hid_flash_attn, QStringLiteral("flash_attn"), &summary.restartItems);
    appendUniqueIfChanged(beforePort != afterPort, QStringLiteral("port"), &summary.restartItems);
    appendUniqueIfChanged(!isTrimmedCaseInsensitiveEqual(beforeDevice, afterDevice), QStringLiteral("device"), &summary.restartItems);
    appendUniqueIfChanged(backendOverrideDirty, QStringLiteral("backend_override"), &summary.restartItems);
    summary.requiresBackendRestart = !summary.restartItems.isEmpty();

    // 2) 再分析仅需重置会话的采样/回合参数
    appendUniqueIfChanged(beforeSettings.temp != afterSettings.temp, QStringLiteral("temp"), &summary.resetItems);
    appendUniqueIfChanged(beforeSettings.repeat != afterSettings.repeat, QStringLiteral("repeat"), &summary.resetItems);
    appendUniqueIfChanged(beforeSettings.top_k != afterSettings.top_k, QStringLiteral("top_k"), &summary.resetItems);
    appendUniqueIfChanged(beforeSettings.hid_top_p != afterSettings.hid_top_p, QStringLiteral("top_p"), &summary.resetItems);
    appendUniqueIfChanged(beforeSettings.hid_npredict != afterSettings.hid_npredict, QStringLiteral("npredict"), &summary.resetItems);
    appendUniqueIfChanged(beforeSettings.hid_n_ubatch != afterSettings.hid_n_ubatch, QStringLiteral("ubatch"), &summary.resetItems);
    appendUniqueIfChanged(beforeSettings.complete_mode != afterSettings.complete_mode, QStringLiteral("mode"), &summary.resetItems);
    appendUniqueIfChanged(beforeSettings.reasoning_effort != afterSettings.reasoning_effort, QStringLiteral("reasoning"), &summary.resetItems);

    // 3) 统一输出行为：只要存在配置变化，就需要重置上下文（重启场景也包含其中）
    summary.requiresSessionReset = summary.requiresBackendRestart || !summary.resetItems.isEmpty();
    summary.hasAnyChange = summary.requiresSessionReset;
    return summary;
}

QString compactChangeItems(const QStringList &items, int maxItems)
{
    QStringList normalized;
    for (const QString &item : items)
    {
        const QString trimmed = item.trimmed();
        if (trimmed.isEmpty()) continue;
        if (!normalized.contains(trimmed)) normalized.append(trimmed);
    }
    if (normalized.isEmpty()) return QStringLiteral("none");

    const int safeMax = qMax(1, maxItems);
    if (normalized.size() <= safeMax) return normalized.join(QStringLiteral(", "));

    const QStringList prefix = normalized.mid(0, safeMax);
    return QStringLiteral("%1 +%2").arg(prefix.join(QStringLiteral(", "))).arg(normalized.size() - safeMax);
}
