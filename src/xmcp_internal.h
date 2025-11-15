#ifndef EVA_XMCP_INTERNAL_H
#define EVA_XMCP_INTERNAL_H

#include "mcp_json.h"
#include "xconfig.h"

#include <QSet>
#include <QString>

namespace eva::mcp
{
// Synchronize MCP_TOOLS_INFO_LIST with the latest tool metadata returned by services.
// When enabledFilter is not null, only services present in the filter are kept.
// Returns true if the global list was modified (layouts, descriptions, schemas).
bool syncSelectedMcpTools(const ::mcp::json &allTools, const QSet<QString> *enabledFilter = nullptr);

// Sanitize tool metadata in-place: strips schema metadata that tends to create noisy prompts.
::mcp::json sanitizeToolsInfo(::mcp::json tools);
} // namespace eva::mcp

#endif // EVA_XMCP_INTERNAL_H
