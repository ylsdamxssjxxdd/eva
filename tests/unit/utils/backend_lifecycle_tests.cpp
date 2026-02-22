#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "xconfig.h"

TEST_CASE("Backend lifecycle names are stable")
{
    CHECK(backendLifecycleStateName(BackendLifecycleState::Stopped) == QStringLiteral("stopped"));
    CHECK(backendLifecycleStateName(BackendLifecycleState::Starting) == QStringLiteral("starting"));
    CHECK(backendLifecycleStateName(BackendLifecycleState::Running) == QStringLiteral("running"));
    CHECK(backendLifecycleStateName(BackendLifecycleState::Sleeping) == QStringLiteral("sleeping"));
    CHECK(backendLifecycleStateName(BackendLifecycleState::Error) == QStringLiteral("error"));
}

TEST_CASE("Backend lifecycle allows expected happy-path transitions")
{
    CHECK(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Stopped, BackendLifecycleState::Starting));
    CHECK(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Starting, BackendLifecycleState::Running));
    CHECK(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Running, BackendLifecycleState::Restarting));
    CHECK(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Restarting, BackendLifecycleState::Running));
    CHECK(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Running, BackendLifecycleState::Sleeping));
    CHECK(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Sleeping, BackendLifecycleState::Waking));
    CHECK(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Waking, BackendLifecycleState::Running));
}

TEST_CASE("Backend lifecycle blocks invalid direct transitions")
{
    CHECK_FALSE(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Stopped, BackendLifecycleState::Running));
    CHECK_FALSE(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Starting, BackendLifecycleState::Sleeping));
    CHECK_FALSE(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Running, BackendLifecycleState::Starting));
    CHECK_FALSE(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Sleeping, BackendLifecycleState::Running));
    CHECK_FALSE(isBackendLifecycleTransitionAllowed(BackendLifecycleState::Error, BackendLifecycleState::Running));
}
