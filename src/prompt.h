#pragma once
#include "xconfig.h"

// 额外指令模板
inline const QString EXTRA_PROMPT_FORMAT = R"(You may call one or more functions to assist with the user query. 
You are provided with function signatures within <tools> </tools> XML tags:
<tools>
{available_tools_describe}
</tools>
You must follow the instructions below for every function call:
1. Return the call inside a <tool_call>…</tool_call> block.
2. Inside the block, output valid JSON with exactly two keys: "name" (string) and "arguments" (object). Example:
<tool_call>
{"name":"answer","arguments":{"content":"The task has been completed. Is there anything else I can help you with?"}}
</tool_call>
3. Keep the JSON valid:
   - Wrap all keys and string values in double quotes.
   - Escape inner quotes as \".
   - Represent line breaks as \n.
   - Do not add trailing commas.

{engineer_info}
)";

// 软件工程师信息模板
inline const QString ENGINEER_INFO = R"(
You possess the ability of a senior software engineer, proficient in multiple programming languages, frameworks, design patterns, and best practices.
You can only use one tool at a time; wait and observe the results returned by the tool before proceeding to the next step.
Try to solve problems independently as much as possible. Review the provided workspace overview before planning any work so you clearly understand the project layout and act with whole-project awareness.If user clarification is required, ask via the answer tool.
When replying to the user at the end of a task, summarize objectives, key modifications, and validation steps without pasting detailed code listings. Describe changes at a high level instead.
You may write file contents directly without echoing them in the conversation.
The current environment is: {engineer_system_info}
)";

// 软件工程师系统信息指令
inline const QString ENGINEER_SYSTEM_INFO = R"(
Operating System: {OS}
Date: {DATE}
Default Shell: {SHELL}
Compile env: {COMPILE_ENV}
Python env: {PYTHON_ENV}
Node.js env: {NODE_ENV}
Current working directory: {DIR}
Workspace snapshot (depth <= 2):
{WORKSPACE_TREE}
Execute commands only in the current working directory and do not attempt to switch to other locations.
)";

// 内置的回复工具
static TOOLS_INFO Buildin_tools_answer(
    "answer",
    // 工具描述
    R"(The final response to the user.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"content":{"type":"string","description":"The final response to the user"}},"required":["content"]})");

// 内置的计算器工具
static TOOLS_INFO Buildin_tools_calculator(
    "calculator",
    // 工具描述
    R"(Enter a expression to return the calculation result via using tinyexpr.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"expression":{"type":"string","description":"math expression"}},"required":["expression"]})");

// 内置的鼠标键盘工具
// 屏幕的左上角坐标为(0,0)
// 行动序列列表，列表元素可以是以下函数:
// 鼠标左键按下: left_down(x,y), 鼠标左键抬起: left_up(), 鼠标右键按下: right_down(x,y), 鼠标右键抬起: right_up(), 鼠标移动到终点: move(x,y,t), 按下键盘: keyboard(key), 序列间隔: time_span(t)
// x为横坐标, y为纵坐标, t是持续时间单位为秒, key是键盘上的按键，可以是组合键例如 "Ctrl+S".
// 传参示例: ["left_down(100,200)", "time_span(0.1)", "left_up()", "time_span(0.5)", "left_down(100,200)", "time_span(0.1)", "left_up()"] 鼠标左键双击
// 传参示例:  ["left_down(100,200)", "time_span(0.1)", "move(200,400,0.5)", "time_span(0.1)", "left_up()"] 鼠标左键拖动
// 工具执行完会返回当前屏幕截图
static TOOLS_INFO Buildin_tools_controller(
    "controller",
    "Pass in a sequence of actions to control the mouse and keyboard. {screen_info}", // 传入一串行动序列来控制鼠标键盘
    R"SCHEMA({"type":"object","properties":{"sequence":{"type":"list","description":"Action Sequence List. List elements can be the following functions:
Mouse left button down: left_down(x, y)
Mouse left button up: left_up()
Mouse right button down: right_down(x, y)
Mouse right button up: right_up()
Move mouse to end point: move(x, y, t)
Press keyboard key: keyboard(key)
Sequence interval: time_span(t)
Parameters:
x is the horizontal coordinate.
y is the vertical coordinate.
t is the duration in seconds.
key is a key on the keyboard, which can be a combination key, e.g., "Ctrl+S".
Passing Parameter Examples:
["left_down(100,200)", "time_span(0.1)", "left_up()", "time_span(0.5)", "left_down(100,200)", "time_span(0.1)", "left_up()"] - Double left mouse click.
["left_down(100,200)", "time_span(0.1)", "move(200,400,0.5)", "time_span(0.1)", "left_up()"] - Left mouse button drag. After the tool is executed, it will return the current screenshot"}},"required":["sequence"]})SCHEMA");

// 内置的列出MCP工具
static TOOLS_INFO Buildin_tools_mcp_tools_list(
    "mcp_tools_list",
    // 工具描述
    R"(List all available tools in the current MCP service, which may include tools that you can use.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{}})");

// 内置的知识库工具
static TOOLS_INFO Buildin_tools_knowledge(
    "knowledge",
    // 工具描述
    R"(Ask a question to the knowledge base, the more detailed the question, the better. The knowledge base will return three text segments with the highest similarity to the question. knowledge database describe: {embeddingdb describe})",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"content":{"type":"string","description":"Keywords to be queried"}},"required":["content"]})");

// 内置的文生图工具
static TOOLS_INFO Buildin_tools_stablediffusion(
    "stablediffusion",
    // 工具描述
    R"(Describe the image you want to draw with a paragraph of English text. The tool will send the text to the drawing model and then return the drawn image, making sure to input English. You can add modifiers or phrases to improve the quality of the image before the text and separate them with commas.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"prompt":{"type":"string","description":"Describe the image you want to draw with a paragraph of English text"}},"required":["prompt"]})");

// 内置的工程师-命令行工具
static TOOLS_INFO Buildin_tools_execute_command(
    "execute_command",
    // 工具描述
    R"(Request to execute CLI commands on the system. Use this command when you need to perform system operations or run specific commands to complete any step of a user task. You must adjust the commands according to the user's system. Prioritize executing complex CLI commands over creating executable scripts, as they are more flexible and easier to run. The command will be executed in the current working directory.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"content":{"type":"string","description":"CLI commands"}},"required":["content"]})");

// 内置的工程师-列出目录文件工具
static TOOLS_INFO Buildin_tools_list_files(
    "list_files",
    // 工具描述
    R"(List all immediate subfolders and files under a directory. If no path is provided, default to the engineer working directory. Paths are resolved relative to the engineer working directory and output stays compact, one entry per line.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"path":{"type":"string","description":"Optional directory to list (relative to the engineer working directory). Leave blank to use the current working directory."}}})");

// 内置的工程师-搜索文件内容工具
static TOOLS_INFO Buildin_tools_search_content(
    "search_content",
    // 工具描述
    R"(Search all text files under the engineer working directory for a query string (case-insensitive). Returns lines in the form <path>:<line>:<content>.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"query":{"type":"string","description":"The text to search for."}},"required":["query"]})");

// 内置的工程师-读文件工具
static TOOLS_INFO Buildin_tools_read_file(
    "read_file",
    // 工具描述
    R"(Request to read the content of a file in a specified path, used when you need to check the content of an existing file, such as analyzing code, reviewing text files, or extracting information from a configuration file. You can specify start and end line numbers to read only a portion of the file. Maximum 200 lines can be read at once.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to read"},"start_line":{"type":"integer","description":"The starting line number (1-based index). Optional, default is 1","minimum":1},"end_line":{"type":"integer","description":"The ending line number (1-based index). Optional, maximum 200 lines from start_line","minimum":1}},"required":["path"]})");

// 内置的工程师-写文件工具
static TOOLS_INFO Buildin_tools_write_file(
    "write_file",
    // 工具描述
    R"(Request to write content to a file at the specified path. If the file exists, it will be overwritten by the provided content. If the file does not exist, it will be created. This tool will automatically create any directory required for writing files.)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to write"},"content":{"type":"string","description":"The file content"}},"required":["path","content"]})");

// ──────────────────────────────────────────────────────────────
// 内置的工程师-编辑文件工具
// ──────────────────────────────────────────────────────────────
static TOOLS_INFO Buildin_tools_replace_in_file(
    "replace_in_file",
    // 工具描述
    R"(Request to replace text inside an existing file.  
By default, exactly one occurrence of old_string will be replaced.  
If you need to replace multiple identical occurrences, set expected_replacements to the
number of occurrences you expect.
For a file of unknown size, initially limit the reading to the first 200 lines, and then continue reading from there based on the situation.
)",
    // JSON-Schema，用于验证调用参数
    R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"old_string":{"type":"string","description":"The exact literal text to be replaced (include surrounding context for uniqueness)."},"new_string":{"type":"string","description":"The exact literal text that will replace old_string."},"expected_replacements":{"type":"number","description":"Number of expected occurrences to replace. Defaults to 1 if omitted.","minimum":1}},"required":["path","old_string","new_string"]})");

// Engineer tool - structured edit helper
static TOOLS_INFO Buildin_tools_edit_in_file(
    "edit_in_file",
    // 工具说明
    R"(Apply structured line-based edits to an existing text file.  
Each edit is applied from bottom to top to keep line numbers stable.  
Provide concise context to avoid unintended changes.)",
    // JSON-Schema参数校验及使用说明
    R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"edits":{"type":"array","description":"Ordered list of edit operations.","items":{"type":"object","properties":{"action":{"type":"string","enum":["replace","insert_before","insert_after","delete"],"description":"Type of edit to apply."},"start_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit begins."},"end_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit ends (inclusive). Required for replace and delete."},"new_content":{"type":"string","description":"Replacement or insertion content. Use actual line breaks inside the string for multi-line updates."}},"required":["action","start_line"],"additionalProperties":false}},"ensure_newline_at_eof":{"type":"boolean","description":"Ensure the file ends with a trailing newline when true."}},"required":["path","edits"]})");
