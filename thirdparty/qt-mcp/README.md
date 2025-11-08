# Qt MCP Client (Qt 5)

This module reimplements the core pieces of [`cpp-mcp`](https://github.com/hkr04/cpp-mcp) with Qt 5 primitives so that Qt applications can talk to MCP servers via SSE or stdio transports. The library stays close to the JSON-RPC flows used by the reference repo: `initialize → initialized → tools/list → tools/call`.

## Features
- Shared `ServerConfig` loader that understands the MCP config JSON used by Claude Desktop / MCP CLI.
- `qmcp::SseClient` backed by `QNetworkAccessManager` and a streaming parser for `text/event-stream`.
- `qmcp::StdioClient` backed by `QProcess`, using newline-delimited JSON just like the original repo.
- Example console app (`qtmcp_example`) that loads `test.json`, connects to every active server, lists available tools, and optionally calls a sample tool defined in the config.

## Build

```bash
cmake -S qt-mcp -B qt-mcp/build -DCMAKE_PREFIX_PATH=<path-to-Qt5>
cmake --build qt-mcp/build --config Release
```

Replace `<path-to-Qt5>` with the folder that contains `Qt5Config.cmake` (for example `C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5` on Windows).

## Example

1. Update [`test.json`](../test.json) with your servers and (optionally) per-server `exampleToolCall`.
2. Run the example (make sure the target MCP servers are reachable, and the stdio server command is available on your PATH):

```bash
cd qt-mcp/build
ctest --show-only=json-v1 # optional sanity check
./qtmcp_example --config ../../test.json
```

The example prints the discovered tools for each server and attempts to run a sample tool call when enough metadata is provided.

### Config shape

Each entry under `mcpServers` can include an `exampleToolCall` block:

```jsonc
{
  "mcpServers": {
    "EverythingServer": {
      "type": "stdio",
      "command": "npx",
      "args": ["-y", "@modelcontextprotocol/server-everything"],
      "env": { "MCP_DEBUG": "1" },
      "exampleToolCall": {
        "name": "echo",
        "arguments": { "text": "Qt MCP example" }
      }
    }
  }
}
```

For SSE servers you can also provide custom HTTP headers (for example `Authorization: Bearer ...`), which are automatically attached to both the SSE stream and JSON-RPC POSTs.
