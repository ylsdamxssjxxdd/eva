#pragma once
#include "xconfig.h"

// 额外指令模板
inline const QString EXTRA_PROMPT_FORMAT = R"(You may call one or more functions to assist with the user query. 
You are provided with function signatures within <tools> </tools> XML tags:
<tools>
{available_tools_describe}
</tools>
For each function call, return a json object with function name and arguments within <tool_call> </tool_call> XML tags:
<tool_call>
{"name":"<function-name>","arguments":<args-json-object>}
</tool_call>
Example:
<tool_call>
{"name":"answer","arguments":{"content":"The task has been completed. Is there anything else I can help you with?"}}
</tool_call>

{engineer_info}
)";

// 软件工程师信息模板
inline const QString ENGINEER_INFO = R"(
You possess the ability of a senior software engineer, proficient in multiple programming languages, frameworks, design patterns, and best practices. 
You can only use one tool at a time, wait and observe the results returned by the tool before proceeding to the next step. 
Try to solve problems independently as much as possible. If there is a need for user response, please use the answer tool directly to inquire. 
The content that needs to be written to the file does not need to be displayed and can be written directly. 
The current environment is: {engineer_system_info}
)";

// 软件工程师系统信息指令
inline const QString ENGINEER_SYSTEM_INFO = R"(
Operating System: {OS}
Date: {DATE}
Default Shell: {SHELL}
Compile env: {COMPILE_ENV}
Python env: {PYTHON_ENV}
Current working directory: {DIR}
Execute commands only in the current working directory and do not attempt to switch to other locations.
)";

// 内置的回复工具
static TOOLS_INFO Buildin_tools_answer(
    "answer",
    // 工具描述
    R"(The final response to the user.)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "content": {
            "type": "string",
            "description": "The final response to the user"
            }
        },
        "required": ["content"]
})");

// 内置的计算器工具
static TOOLS_INFO Buildin_tools_calculator(
    "calculator",
    // 工具描述
    R"(Enter a expression to return the calculation result via using tinyexpr.)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "expression": {
            "type": "string",
            "description": "math expression"
            }
        },
        "required": ["expression"]
})");

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
    "{"
    "\"type\":\"object\","
    "\"properties\":{"
    "\"sequence\":{"
    "\"type\":\"list\","
    "\"description\":\"Action Sequence List. List elements can be the following functions:\nMouse left button down: left_down(x, y)\nMouse left button up: left_up()\nMouse right button down: right_down(x, y)\nMouse right button up: right_up()\nMove mouse to end point: move(x, y, t)\nPress keyboard key: keyboard(key)\nSequence interval: time_span(t)\nParameters:\nx is the horizontal coordinate.\ny is the vertical coordinate.\nt is the duration in seconds.\nkey is a key on the keyboard, which can be a combination key, e.g., \"Ctrl+S\".\nPassing Parameter Examples:\n[\"left_down(100,200)\", \"time_span(0.1)\", \"left_up()\", \"time_span(0.5)\", \"left_down(100,200)\",\"time_span(0.1)\", \"left_up()\"] - Double left mouse click.\n[\"left_down(100,200)\", \"time_span(0.1)\", \"move(200,400,0.5)\", \"time_span(0.1)\", \"left_up()\"] - Left mouse button drag. After the tool is executed, it will return the current screenshot\""
    "}"
    "},"
    "\"required\":[\"sequence\"]\n"
    "}");

// 内置的列出MCP工具
static TOOLS_INFO Buildin_tools_mcp_tools_list(
    "mcp_tools_list",
    // 工具描述
    R"(List all available tools in the current MCP service, which may include tools that you can use.)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {}
})");

// 内置的知识库工具
static TOOLS_INFO Buildin_tools_knowledge(
    "knowledge",
    // 工具描述
    R"(Ask a question to the knowledge base, the more detailed the question, the better. The knowledge base will return three text segments with the highest similarity to the question. knowledge database describe: {embeddingdb describe})",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "content": {
            "type": "string",
            "description": "Keywords to be queried"
            }
        },
        "required": ["content"]
})");

// 内置的文生图工具
static TOOLS_INFO Buildin_tools_stablediffusion(
    "stablediffusion",
    // 工具描述
    R"(Describe the image you want to draw with a paragraph of English text. The tool will send the text to the drawing model and then return the drawn image, making sure to input English. You can add modifiers or phrases to improve the quality of the image before the text and separate them with commas.)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "prompt": {
            "type": "string",
            "description": "Describe the image you want to draw with a paragraph of English text"
            }
        },
        "required": ["prompt"]
})");

// 内置的工程师-命令行工具
static TOOLS_INFO Buildin_tools_execute_command(
    "execute_command",
    // 工具描述
    R"(Request to execute CLI commands on the system. Use this command when you need to perform system operations or run specific commands to complete any step of a user task. You must adjust the commands according to the user's system. Prioritize executing complex CLI commands over creating executable scripts, as they are more flexible and easier to run. The command will be executed in the current working directory.)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "content": {
            "type": "string",
            "description": "CLI commands"
            }
        },
        "required": ["content"]
})");

// 内置的工程师-读文件工具
static TOOLS_INFO Buildin_tools_read_file(
    "read_file",
    // 工具描述
    R"(Request to read the content of a file in a specified path, used when you need to check the content of an existing file, such as analyzing code, reviewing text files, or extracting information from a configuration file. You can specify start and end line numbers to read only a portion of the file. Maximum 200 lines can be read at once.)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "path": {
            "type": "string",
            "description": "The file path which you want to read"
            },
            "start_line": {
            "type": "integer",
            "description": "The starting line number (1-based index). Optional, default is 1",
            "minimum": 1
            },
            "end_line": {
            "type": "integer",
            "description": "The ending line number (1-based index). Optional, maximum 200 lines from start_line",
            "minimum": 1
            }
        },
        "required": ["path"]
})");

// 内置的工程师-写文件工具
static TOOLS_INFO Buildin_tools_write_file(
    "write_file",
    // 工具描述
    R"(Request to write content to a file at the specified path. If the file exists, it will be overwritten by the provided content. If the file does not exist, it will be created. This tool will automatically create any directory required for writing files.)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "path": {
            "type": "string",
            "description": "The file path which you want to write"
            },
            "content": {
            "type": "string",
            "description": "The file content"
            }
        },
        "required": ["path", "content"]
})");

// ──────────────────────────────────────────────────────────────
// 内置的工程师-编辑文件工具
// ──────────────────────────────────────────────────────────────
static TOOLS_INFO Buildin_tools_edit_file(
    "edit_file",
    // 工具描述
    R"(Request to replace text inside an existing file.  
By default, exactly one occurrence of old_string will be replaced.  
If you need to replace multiple identical occurrences, set expected_replacements to the
number of occurrences you expect.
For a file of unknown size, initially limit the reading to the first 200 lines, and then continue reading from there based on the situation.
)",
    // JSON-Schema，用于验证调用参数
    R"({
        "type": "object",
        "properties": {
            "path": {
            "type": "string",
            "description": "The file path which you want to edit."
            },
            "old_string": {
            "type": "string",
            "description": "The exact literal text to be replaced (include surrounding context for uniqueness)."
            },
            "new_string": {
            "type": "string",
            "description": "The exact literal text that will replace old_string."
            },
            "expected_replacements": {
            "type": "number",
            "description": "Number of expected occurrences to replace. Defaults to 1 if omitted.",
            "minimum": 1
            }
        },
        "required": ["path", "old_string", "new_string"]
})");
