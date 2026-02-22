#ifndef RECOVERY_GUIDANCE_H
#define RECOVERY_GUIDANCE_H

#include "utils/eva_error.h"

#include <QString>

// 恢复动作提示：
// - 用于把“错误码”与“下一步动作”统一组织成稳定状态文案。
// - 目标是让用户看到错误时，马上知道系统将自动做什么，或自己该做什么。
enum class RecoveryHintAction
{
    None = 0,
    AutoRetry,
    AutoFallback,
    AdjustPort,
    AdjustDevice,
    CheckBackendPackage,
};

inline QString recoveryHintActionName(RecoveryHintAction action)
{
    switch (action)
    {
    case RecoveryHintAction::AutoRetry: return QStringLiteral("auto_retry");
    case RecoveryHintAction::AutoFallback: return QStringLiteral("auto_fallback");
    case RecoveryHintAction::AdjustPort: return QStringLiteral("adjust_port");
    case RecoveryHintAction::AdjustDevice: return QStringLiteral("adjust_device");
    case RecoveryHintAction::CheckBackendPackage: return QStringLiteral("check_backend_package");
    case RecoveryHintAction::None:
    default:
        break;
    }
    return QStringLiteral("none");
}

inline QString recoveryHintText(RecoveryHintAction action)
{
    switch (action)
    {
    case RecoveryHintAction::AutoRetry:
        return QStringLiteral("action: system will retry automatically");
    case RecoveryHintAction::AutoFallback:
        return QStringLiteral("action: system is switching to fallback backend");
    case RecoveryHintAction::AdjustPort:
        return QStringLiteral("action: adjust port or keep blank for random localhost port");
    case RecoveryHintAction::AdjustDevice:
        return QStringLiteral("action: change backend device in settings and retry");
    case RecoveryHintAction::CheckBackendPackage:
        return QStringLiteral("action: verify EVA_BACKEND package and executable path");
    case RecoveryHintAction::None:
    default:
        break;
    }
    return QString();
}

inline QString appendRecoveryHint(const QString &baseLine, RecoveryHintAction action)
{
    const QString hint = recoveryHintText(action);
    if (hint.isEmpty()) return baseLine;
    if (baseLine.trimmed().isEmpty()) return hint;
    return QStringLiteral("%1 | %2").arg(baseLine, hint);
}

inline QString formatEvaErrorWithHint(EvaErrorCode code,
                                      const QString &message,
                                      RecoveryHintAction action = RecoveryHintAction::None)
{
    return appendRecoveryHint(formatEvaError(code, message), action);
}

#endif // RECOVERY_GUIDANCE_H
