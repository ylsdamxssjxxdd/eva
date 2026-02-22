#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "utils/net_retry_policy.h"

TEST_CASE("retry policy marks transient HTTP status codes as retryable")
{
    CHECK(isRetryableHttpStatus(408));
    CHECK(isRetryableHttpStatus(429));
    CHECK(isRetryableHttpStatus(500));
    CHECK(isRetryableHttpStatus(503));
    CHECK_FALSE(isRetryableHttpStatus(400));
    CHECK_FALSE(isRetryableHttpStatus(404));
}

TEST_CASE("retry policy marks transient network errors as retryable")
{
    CHECK(isRetryableNetworkError(QNetworkReply::TimeoutError));
    CHECK(isRetryableNetworkError(QNetworkReply::TemporaryNetworkFailureError));
    CHECK(isRetryableNetworkError(QNetworkReply::RemoteHostClosedError));
    CHECK_FALSE(isRetryableNetworkError(QNetworkReply::AuthenticationRequiredError));
    CHECK_FALSE(isRetryableNetworkError(QNetworkReply::ProtocolFailure));
}

TEST_CASE("shouldRetryNetRequest enforces cancel/streaming/attempt guards")
{
    CHECK_FALSE(shouldRetryNetRequest(true, false, QNetworkReply::TimeoutError, 0, 0, 1));
    CHECK_FALSE(shouldRetryNetRequest(false, true, QNetworkReply::TimeoutError, 0, 0, 1));
    CHECK_FALSE(shouldRetryNetRequest(false, false, QNetworkReply::TimeoutError, 0, 1, 1));
    CHECK_FALSE(shouldRetryNetRequest(false, false, QNetworkReply::TimeoutError, 0, 0, 0));
}

TEST_CASE("shouldRetryNetRequest supports HTTP and network error fallback")
{
    CHECK(shouldRetryNetRequest(false, false, QNetworkReply::NoError, 503, 0, 1));
    CHECK(shouldRetryNetRequest(false, false, QNetworkReply::TimeoutError, 0, 0, 1));
    CHECK_FALSE(shouldRetryNetRequest(false, false, QNetworkReply::AuthenticationRequiredError, 401, 0, 1));
}

TEST_CASE("nextRetryBackoffMs uses capped exponential growth")
{
    CHECK(nextRetryBackoffMs(0, 400, 2000) == 400);
    CHECK(nextRetryBackoffMs(1, 400, 2000) == 800);
    CHECK(nextRetryBackoffMs(2, 400, 2000) == 1600);
    CHECK(nextRetryBackoffMs(3, 400, 2000) == 2000);
    CHECK(nextRetryBackoffMs(9, 400, 2000) == 2000);
}
