#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "utils/recovery_guidance.h"

TEST_CASE("recovery guidance action names stay stable")
{
    CHECK(recoveryHintActionName(RecoveryHintAction::None) == QStringLiteral("none"));
    CHECK(recoveryHintActionName(RecoveryHintAction::AutoRetry) == QStringLiteral("auto_retry"));
    CHECK(recoveryHintActionName(RecoveryHintAction::AutoFallback) == QStringLiteral("auto_fallback"));
    CHECK(recoveryHintActionName(RecoveryHintAction::AdjustPort) == QStringLiteral("adjust_port"));
    CHECK(recoveryHintActionName(RecoveryHintAction::AdjustDevice) == QStringLiteral("adjust_device"));
    CHECK(recoveryHintActionName(RecoveryHintAction::CheckBackendPackage) == QStringLiteral("check_backend_package"));
}

TEST_CASE("recovery guidance text appends only when action is provided")
{
    const QString base = QStringLiteral("ui:[EVA-BE-003] backend start failure");
    CHECK(appendRecoveryHint(base, RecoveryHintAction::None) == base);
    CHECK(appendRecoveryHint(QString(), RecoveryHintAction::AdjustPort).contains(QStringLiteral("action:")));
    CHECK(appendRecoveryHint(base, RecoveryHintAction::AdjustDevice).contains(QStringLiteral("device")));
}

TEST_CASE("formatEvaErrorWithHint keeps code and adds action hint")
{
    const QString line = formatEvaErrorWithHint(EvaErrorCode::BeStartFailed,
                                                QStringLiteral("backend start failure -> cuda"),
                                                RecoveryHintAction::AutoFallback);
    CHECK(line.contains(QStringLiteral("EVA-BE-003")));
    CHECK(line.contains(QStringLiteral("backend start failure -> cuda")));
    CHECK(line.contains(QStringLiteral("action: system is switching to fallback backend")));
}
