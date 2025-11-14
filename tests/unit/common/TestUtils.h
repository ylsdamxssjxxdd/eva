#pragma once

#include <string>

#include "mcp_json.h"

namespace eva::test
{

inline mcp::json makeToolCall(const std::string &name, const mcp::json &arguments)
{
    mcp::json call = mcp::json::object();
    call["name"] = name;
    call["arguments"] = arguments;
    return call;
}

} // namespace eva::test
