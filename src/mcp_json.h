#ifndef MCP_JSON_H
#define MCP_JSON_H

#include <string>

#include "thirdparty/nlohmann/json.hpp"

namespace mcp
{
using json = nlohmann::ordered_json;
inline constexpr const char *MCP_VERSION = "2024-11-05";
} // namespace mcp

#endif // MCP_JSON_H
