#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "../common/TestUtils.h"

TEST_CASE("makeToolCall preserves calculator payload")
{
    const mcp::json args = mcp::json::object({{"expression", "1 + 2"}});
    const auto call = eva::test::makeToolCall("calculator", args);

    CHECK(call.contains("name"));
    CHECK(call.contains("arguments"));
    CHECK(call["name"] == "calculator");
    CHECK(call["arguments"] == args);
}
