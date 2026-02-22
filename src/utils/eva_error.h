#ifndef EVA_ERROR_H
#define EVA_ERROR_H

#include <QString>

// EVA 统一错误码：
// - 目标：让状态区文案可读，同时让日志/自动化能稳定匹配错误类别。
// - 约定：
//   - BE = Backend 生命周期与端口/进程相关
//   - NET = 网络与请求链路相关
//   - TOOL = 工具执行相关
//   - UI = 交互状态相关
enum class EvaErrorCode
{
    None = 0,
    BeProxyListenFailed,
    BePortConflict,
    BeStartFailed,
    BeExecutableMissing,
    NetRequestFailed,
    ToolExecutionFailed,
    UiInvalidState,
};

inline QString evaErrorCodeTag(EvaErrorCode code)
{
    switch (code)
    {
    case EvaErrorCode::BeProxyListenFailed: return QStringLiteral("EVA-BE-001");
    case EvaErrorCode::BePortConflict: return QStringLiteral("EVA-BE-002");
    case EvaErrorCode::BeStartFailed: return QStringLiteral("EVA-BE-003");
    case EvaErrorCode::BeExecutableMissing: return QStringLiteral("EVA-BE-004");
    case EvaErrorCode::NetRequestFailed: return QStringLiteral("EVA-NET-001");
    case EvaErrorCode::ToolExecutionFailed: return QStringLiteral("EVA-TOOL-001");
    case EvaErrorCode::UiInvalidState: return QStringLiteral("EVA-UI-001");
    case EvaErrorCode::None:
    default:
        break;
    }
    return QStringLiteral("EVA-UNKNOWN");
}

inline QString formatEvaError(EvaErrorCode code, const QString &message)
{
    if (code == EvaErrorCode::None) return message;
    return QStringLiteral("ui:[%1] %2").arg(evaErrorCodeTag(code), message);
}

#endif // EVA_ERROR_H

