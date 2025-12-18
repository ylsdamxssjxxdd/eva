#include "prompt.h"

#include <QVector>

namespace
{
const QString &defaultExtraPrompt()
{
    static const QString value = QStringLiteral(
        "You may call one or more functions to assist with the user query.\n"
        "You are provided with function signatures within <tools> </tools> XML tags:\n"
        "<tools>\n"
        "{available_tools_describe}</tools>\n"
        "You must follow the instructions below for every function call:\n"
        "1. Return the call inside a <tool_call>...</tool_call> block.\n"
        "2. Inside the block, output valid JSON with exactly two keys: \"name\" (string) and \"arguments\" (object). Example:\n"
        "<tool_call>\n"
        "{\"name\":\"answer\",\"arguments\":{\"content\":\"The task has been completed. Is there anything else I can help you with?\"}}\n"
        "</tool_call>\n"
        "\n"
        "{engineer_info}");
    return value;
}

QString &currentExtraPrompt()
{
    static QString value = defaultExtraPrompt();
    return value;
}

const QString &defaultEngineerInfo()
{
    static const QString value = QStringLiteral(
        "Role: EVA execution engineer. Goal: finish the commander's task with minimal chatter.\n"
        "- If context is missing, ask succinctly via answer; avoid small talk.\n"
        "- Prefer batched read_file/list_files before editing; writes may overwrite, ensure trailing newline if needed.\n"
        "- Default to ptc whenever a workflow spans multiple CLI commands, fragile parsing, or structured edits; reach for ptc first and fall back to ad-hoc shell only when a one-liner truly suffices.\n"
        "- Keep every reply concise: <=3 bullet points, no logs or long code unless explicitly requested.\n"
        "- Finish with a change summary and validation steps, not code dumps.\n"
        "Current environment: {engineer_system_info}");
    return value;
}

QString &currentEngineerInfo()
{
    static QString value = defaultEngineerInfo();
    return value;
}

const QString &defaultEngineerSystemInfo()
{
    static const QString value = QStringLiteral(
        "OS: {OS}\n"
        "Date: {DATE}\n"
        "Shell: {SHELL}\n"
        "Compile/Python/Node: {COMPILE_ENV} / {PYTHON_ENV} / {NODE_ENV}\n"
        "Workdir: {DIR}\n"
        "Workspace (<=2 levels):\n"
        "{WORKSPACE_TREE}\n"
        "All commands are confined to the current working directory and its subdirectories.");
    return value;
}

QString &currentEngineerSystemInfo()
{
    static QString value = defaultEngineerSystemInfo();
    return value;
}

const QString &defaultArchitectInfo()
{
    static const QString value = QStringLiteral(
        "You are EVA's system architect; you never run commands or edit code yourself.\n"
        "- Dispatch work via system_engineer_proxy; decide when to reuse engineer_id (preserve memory) or start fresh.\n"
        "- Each request must state objectives, constraints, and acceptance criteria; after tool results, synthesize decisions and risks instead of relaying raw logs.");
    return value;
}

QString &currentArchitectInfo()
{
    static QString value = defaultArchitectInfo();
    return value;
}

struct ToolTemplate
{
    promptx::PromptEntryId descriptionId;
    const char *name;
    const char *schema;
    QString fallback;
    TOOLS_INFO cache{};
};

QVector<ToolTemplate> &toolTemplates()
{
    static QVector<ToolTemplate> templates = {
        {promptx::PROMPT_TOOL_ANSWER,
         "answer",
         R"({"type":"object","properties":{"content":{"type":"string","description":"The final response to the user"}},"required":["content"]})",
         QStringLiteral("The final response to the user.")},
        {promptx::PROMPT_TOOL_CALCULATOR,
         "calculator",
         R"({"type":"object","properties":{"expression":{"type":"string","description":"math expression"}},"required":["expression"]})",
         QStringLiteral("Enter a expression to return the calculation result via using tinyexpr.")},
        {promptx::PROMPT_TOOL_CONTROLLER,
         "controller",
         // 桌面控制器的 arguments Schema 会直接拼进系统提示词；使用最小化、单行 JSON，避免提示词被无意义换行/描述拉长。
         R"({"type":"object","properties":{"bbox":{"anyOf":[{"type":"array","items":{"type":"integer"},"minItems":4,"maxItems":4},{"type":"array","items":{"type":"integer"},"minItems":2,"maxItems":2}]},"action":{"type":"string","enum":["left_click","left_double_click","right_click","middle_click","left_hold","right_hold","middle_hold","left_release","right_release","middle_release","scroll_down","scroll_up","press_key","type_text","delay","move_mouse","drag_drop"]},"description":{"type":"string"},"key":{"type":"string"},"text":{"type":"string"},"delay_ms":{"type":"integer","minimum":0},"duration_ms":{"type":"integer","minimum":0},"scroll_steps":{"type":"integer","minimum":1},"to_bbox":{"anyOf":[{"type":"array","items":{"type":"integer"},"minItems":4,"maxItems":4},{"type":"array","items":{"type":"integer"},"minItems":2,"maxItems":2}]}},"required":["bbox","action","description"],"additionalProperties":false})",
          QStringLiteral(
              "Desktop controller:\n"
              "- Use only when the user explicitly requests desktop actions or grants permission; otherwise ask.\n"
              "- `bbox` is in normalized screenshot space {controller_norm_x}x{controller_norm_y} (origin top-left), format: [x1,y1,x2,y2] or [cx,cy]. One call = one atomic action; multi-step = multiple rounds using the latest screenshot.\n"
             "- For `type_text`, EVA auto left-clicks the bbox center to focus before typing `text` (so you usually don't need a separate left_click). Never click/type in EVA's chat input box. press_key uses `key`; delay=`delay_ms`, scroll_steps optional, drag_drop uses `to_bbox` (same format as bbox).")},
         {promptx::PROMPT_TOOL_MCP_LIST,
          "mcp_tools_list",
          R"({"type":"object","properties":{}})",
          QStringLiteral("List all available tools in the current MCP service, which may include tools that you can use.")},
        {promptx::PROMPT_TOOL_KNOWLEDGE,
         "knowledge",
         R"({"type":"object","properties":{"content":{"type":"string","description":"Keywords to be queried"}},"required":["content"]})",
         QStringLiteral("Ask a question to the knowledge base, the more detailed the question, the better. The knowledge base will return three text segments with the highest similarity to the question. knowledge database describe: {embeddingdb describe}")},
        {promptx::PROMPT_TOOL_SD,
         "stablediffusion",
         R"({"type":"object","properties":{"prompt":{"type":"string","description":"Describe the image you want to draw."}},"required":["prompt"]})",
         QStringLiteral("Describe the image you want to draw with a paragraph of English text. The tool will send the text to the drawing model and then return the drawn image, making sure to input English. You can add modifiers or phrases to improve the quality of the image before the text and separate them with commas.")},
        {promptx::PROMPT_TOOL_EXECUTE,
         "execute_command",
         R"({"type":"object","properties":{"content":{"type":"string","description":"CLI commands"}},"required":["content"]})",
         QStringLiteral("Request to execute CLI commands on the system. Use this command when you need to perform system operations or run specific commands to complete any step of a user task. You must adjust the commands according to the user's system. Prioritize executing complex CLI commands over creating executable scripts, as they are more flexible and easier to run. The command will be executed in the current working directory.")},
        {promptx::PROMPT_TOOL_PTC,
         "ptc",
         R"({"type":"object","properties":{"filename":{"type":"string","description":"Python file name, e.g. helper.py."},"workdir":{"type":"string","description":"Working directory relative to the engineer workspace. Use \".\" for the workspace root."},"content":{"type":"string","description":"Full Python source code that should run inside the target workdir."}},"required":["filename","workdir","content"]})",
         QStringLiteral("Programmatic Tool Calling lets you write a Python helper when CLI commands become unwieldy. Provide the file name, working directory (relative to the engineer workspace), and full script content. EVA saves the file under ptc_temp, runs it immediately, and returns stdout/stderr so you can chain additional steps.")},
        {promptx::PROMPT_TOOL_LIST_FILES,
         "list_files",
         R"({"type":"object","properties":{"path":{"type":"string","description":"Optional directory to list (relative to the engineer working directory). Leave blank to use the current working directory."}}})",
         QStringLiteral("List all immediate subfolders and files under a directory. If no path is provided, default to the engineer working directory. Paths are resolved relative to the engineer working directory and output stays compact, one entry per line.")},
        {promptx::PROMPT_TOOL_SEARCH_CONTENT,
         "search_content",
         R"({"type":"object","properties":{"query":{"type":"string","description":"The text to search for (case-insensitive literal)."},"path":{"type":"string","description":"Optional directory to limit the search to, relative to the engineer working directory."},"file_pattern":{"type":"string","description":"Optional glob to filter files, e.g. *.cpp or src/**.ts"}},"required":["query"]})",
         QStringLiteral("Search all text files under the engineer working directory for a query string (case-insensitive). Returns lines in the form <path>:<line>:<content>.")},
        {promptx::PROMPT_TOOL_READ_FILE,
         "read_file",
         R"({"type":"object","properties":{"files":{"type":"array","description":"List of files to read. Each entry may include optional line ranges.","items":{"type":"object","properties":{"path":{"type":"string","description":"Path to read, relative to the engineer workspace."},"start_line":{"type":"integer","minimum":1,"description":"Optional start line (inclusive)."},"end_line":{"type":"integer","minimum":1,"description":"Optional end line (inclusive)."},"line_ranges":{"type":"array","description":"Optional explicit ranges.","items":{"type":"array","items":[{"type":"integer","minimum":1},{"type":"integer","minimum":1}]}}},"required":["path"]}},"path":{"type":"string","description":"Legacy single-file path."},"start_line":{"type":"integer","minimum":1,"description":"Legacy start line (inclusive)."},"end_line":{"type":"integer","minimum":1,"description":"Legacy end line (inclusive)."}},"anyOf":[{"required":["files"]},{"required":["path"]}]})",
         QStringLiteral("Request to read the content of a file in a specified path, used when you need to check the content of an existing file, such as analyzing code, reviewing text files, or extracting information from a configuration file. You can specify start and end line numbers to read only a portion of the file. Maximum 200 lines can be read at once.")},
        {promptx::PROMPT_TOOL_WRITE_FILE,
         "write_file",
         R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to write"},"content":{"type":"string","description":"The file content"}},"required":["path","content"]})",
         QStringLiteral("Request to write content to a file at the specified path. If the file exists, it will be overwritten by the provided content. If the file does not exist, it will be created. This tool will automatically create any directory required for writing files.")},
        {promptx::PROMPT_TOOL_REPLACE_IN_FILE,
         "replace_in_file",
         R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"old_string":{"type":"string","description":"The exact literal text to be replaced (include surrounding context for uniqueness)."},"new_string":{"type":"string","description":"The exact literal text that will replace old_string."},"expected_replacements":{"type":"number","description":"Number of expected occurrences to replace. Defaults to 1 if omitted.","minimum":1}},"required":["path","old_string","new_string"]})",
         QStringLiteral("Request to replace text inside an existing file. By default, exactly one occurrence of old_string will be replaced. If you need to replace multiple identical occurrences, set expected_replacements to the number of occurrences you expect. For a file of unknown size, initially limit the reading to the first 200 lines, and then continue reading from there based on the situation.")},
        {promptx::PROMPT_TOOL_EDIT_IN_FILE,
         "edit_in_file",
         R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"edits":{"type":"array","description":"Ordered list of edit operations.","items":{"type":"object","properties":{"action":{"type":"string","enum":["replace","insert_before","insert_after","delete"],"description":"Type of edit to apply."},"start_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit begins."},"end_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit ends (inclusive). Required for replace and delete."},"new_content":{"type":"string","description":"Replacement or insertion content. Use actual line breaks inside the string for multi-line updates."}},"required":["action","start_line"],"additionalProperties":false}},"ensure_newline_at_eof":{"type":"boolean","description":"Ensure the file ends with a trailing newline when true."}},"required":["path","edits"]})",
         QStringLiteral("Apply structured line-based edits to an existing text file. Each edit is applied from bottom to top to keep line numbers stable. Provide concise context to avoid unintended changes.")},
         {promptx::PROMPT_TOOL_ENGINEER_PROXY,
          "system_engineer_proxy",
          R"({"type":"object","properties":{"engineer_id":{"type":"string","description":"Engineer identifier. Reuse to keep prior context or set a new id to start fresh."},"task":{"type":"string","description":"Task description, including objectives, context, and acceptance criteria."}},"required":["engineer_id","task"]})",
          QStringLiteral("Escalate a task to the resident system engineer. Arguments include engineer_id (string) and task (string). The task should describe the desired outcome, context, and constraints. The engineer replies with a summary of the work (<=200 characters). Use the same engineer_id to reuse prior memory; send a new id to start with a fresh engineer.")},
        {promptx::PROMPT_TOOL_MONITOR,
         "monitor",
         // 监视器工具的作用是“等待 + 截图”形成观测回路；同样使用最小化 schema，减少提示词注入长度。
         R"({"type":"object","properties":{"wait_ms":{"type":"integer","minimum":0,"maximum":30000},"note":{"type":"string"}},"required":["wait_ms"],"additionalProperties":false})",
         QStringLiteral(
             "Desktop monitor:\n"
             "- Use only when you need to watch the screen and wait for a UI change.\n"
             "- One call waits `wait_ms` ms, then you will receive a fresh screenshot.\n"
             "- After each screenshot: if not ready, call monitor again (use backoff); if ready, stop monitoring and call controller.\n"
             "- Do not loop forever: stop after reasonable attempts/time and ask the user.")},
    };
    return templates;
}

void rebuildToolEntries()
{
    auto &defs = toolTemplates();
    for (auto &tpl : defs)
    {
        tpl.cache = TOOLS_INFO(QString::fromUtf8(tpl.name), tpl.fallback, QString::fromUtf8(tpl.schema));
    }
}
} // namespace

namespace promptx
{
QString promptById(int id, const QString &fallback)
{
    switch (id)
    {
    case PROMPT_EXTRA_TEMPLATE:
        return currentExtraPrompt();
    case PROMPT_ENGINEER_INFO:
        return currentEngineerInfo();
    case PROMPT_ENGINEER_SYSTEM:
        return currentEngineerSystemInfo();
    case PROMPT_ARCHITECT_INFO:
        return currentArchitectInfo();
    default:
        break;
    }
    for (const auto &tpl : toolTemplates())
    {
        if (tpl.descriptionId == id) return tpl.fallback;
    }
    return fallback;
}

bool loadPromptLibrary(const QString &resourcePath)
{
    (void)resourcePath;
    currentExtraPrompt() = defaultExtraPrompt();
    currentEngineerInfo() = defaultEngineerInfo();
    currentEngineerSystemInfo() = defaultEngineerSystemInfo();
    currentArchitectInfo() = defaultArchitectInfo();
    rebuildToolEntries();
    return true;
}

const QString &extraPromptTemplate()
{
    return currentExtraPrompt();
}

const QString &engineerInfo()
{
    return currentEngineerInfo();
}

const QString &engineerSystemInfo()
{
    return currentEngineerSystemInfo();
}

const QString &architectInfo()
{
    return currentArchitectInfo();
}

const TOOLS_INFO &toolAnswer()
{
    return toolTemplates()[0].cache;
}

const TOOLS_INFO &toolCalculator()
{
    return toolTemplates()[1].cache;
}

const TOOLS_INFO &toolController()
{
    return toolTemplates()[2].cache;
}

const TOOLS_INFO &toolMcpList()
{
    return toolTemplates()[3].cache;
}

const TOOLS_INFO &toolKnowledge()
{
    return toolTemplates()[4].cache;
}

const TOOLS_INFO &toolStableDiffusion()
{
    return toolTemplates()[5].cache;
}

const TOOLS_INFO &toolExecuteCommand()
{
    return toolTemplates()[6].cache;
}

const TOOLS_INFO &toolPtc()
{
    return toolTemplates()[7].cache;
}

const TOOLS_INFO &toolListFiles()
{
    return toolTemplates()[8].cache;
}

const TOOLS_INFO &toolSearchContent()
{
    return toolTemplates()[9].cache;
}

const TOOLS_INFO &toolReadFile()
{
    return toolTemplates()[10].cache;
}

const TOOLS_INFO &toolWriteFile()
{
    return toolTemplates()[11].cache;
}

const TOOLS_INFO &toolReplaceInFile()
{
    return toolTemplates()[12].cache;
}

const TOOLS_INFO &toolEditInFile()
{
    return toolTemplates()[13].cache;
}

const TOOLS_INFO &toolEngineerProxy()
{
    return toolTemplates()[14].cache;
}

const TOOLS_INFO &toolMonitor()
{
    return toolTemplates()[15].cache;
}
} // namespace promptx
