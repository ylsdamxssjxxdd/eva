#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "utils/eva_error.h"

TEST_CASE("eva error code tags stay stable")
{
    CHECK(evaErrorCodeTag(EvaErrorCode::BeProxyListenFailed) == QStringLiteral("EVA-BE-001"));
    CHECK(evaErrorCodeTag(EvaErrorCode::BePortConflict) == QStringLiteral("EVA-BE-002"));
    CHECK(evaErrorCodeTag(EvaErrorCode::BeStartFailed) == QStringLiteral("EVA-BE-003"));
    CHECK(evaErrorCodeTag(EvaErrorCode::BeExecutableMissing) == QStringLiteral("EVA-BE-004"));
    CHECK(evaErrorCodeTag(EvaErrorCode::NetRequestFailed) == QStringLiteral("EVA-NET-001"));
    CHECK(evaErrorCodeTag(EvaErrorCode::ToolExecutionFailed) == QStringLiteral("EVA-TOOL-001"));
    CHECK(evaErrorCodeTag(EvaErrorCode::UiInvalidState) == QStringLiteral("EVA-UI-001"));
}

TEST_CASE("formatEvaError keeps message unchanged for None")
{
    const QString message = QStringLiteral("net: timeout");
    CHECK(formatEvaError(EvaErrorCode::None, message) == message);
}

TEST_CASE("formatEvaError prefixes message with code tag")
{
    const QString formatted = formatEvaError(EvaErrorCode::NetRequestFailed, QStringLiteral("net: http 500"));
    CHECK(formatted == QStringLiteral("ui:[EVA-NET-001] net: http 500"));
}

TEST_CASE("unknown enum values fallback to EVA-UNKNOWN tag")
{
    const EvaErrorCode unknown = static_cast<EvaErrorCode>(9999);
    CHECK(evaErrorCodeTag(unknown) == QStringLiteral("EVA-UNKNOWN"));
}
