#include "prompt.h"

#include "utils/simpleini.h"
#include <QHash>
#include <QVector>
#include <QVector>

namespace
{
QHash<int, QString> g_promptMap;

const QString &defaultExtraPrompt()
{
    static const QString value = QStringLiteral(
        "You may call one or more functions to assist with the user query.\n"
        "You are provided with function signatures within <tools> </tools> XML tags:\n"
        "<tools>\n"
        "{available_tools_describe}\n"
        "</tools>\n"
        "You must follow the instructions below for every function call:\n"
        "1. Return the call inside a <tool_call>…</tool_call> block.\n"
        "2. Inside the block, output valid JSON with exactly two keys: \"name\" (string) and \"arguments\" (object). Example:\n"
        "<tool_call>\n"
        "{\"name\":\"answer\",\"arguments\":{\"content\":\"The task has been completed. Is there anything else I can help you with?\"}}\n"
        "</tool_call>\n"
        "3. Keep the JSON valid:\n"
        "   - Wrap all keys and string values in double quotes.\n"
        "   - Escape inner quotes as \\\".\n"
        "   - Represent line breaks as \\n.\n"
        "   - Do not add trailing commas.\n"
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
        "You possess the ability of a senior software engineer, proficient in multiple programming languages, frameworks, design patterns, and best practices.\n"
        "You can only use one tool at a time; wait and observe the results returned by the tool before proceeding to the next step.\n"
        "Try to solve problems independently as much as possible. Review the provided workspace overview before planning any work so you clearly understand the project layout and act with whole-project awareness.If user clarification is required, ask via the answer tool.\n"
        "Whenever a problem looks complex or requires multiple steps, prefer invoking the programmatic_tool_calling (ptc) tool to author a Python helper instead of chaining brittle shell commands.\n"
        "When replying to the user at the end of a task, summarize objectives, key modifications, and validation steps without pasting detailed code listings. Describe changes at a high level instead.\n"
        "You may write file contents directly without echoing them in the conversation.\n"
        "The current environment is: {engineer_system_info}");
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
        "Operating System: {OS}\n"
        "Date: {DATE}\n"
        "Default Shell: {SHELL}\n"
        "Compile env: {COMPILE_ENV}\n"
        "Python env: {PYTHON_ENV}\n"
        "Node.js env: {NODE_ENV}\n"
        "Current working directory: {DIR}\n"
        "Workspace snapshot (depth <= 2):\n"
        "{WORKSPACE_TREE}\n"
        "Execute commands only in the current working directory and do not attempt to switch to other locations.");
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
        "You serve as EVA's System Architect. Focus on scoping work, writing crisp requirements, and sequencing plans. "
        "Never run shell commands yourself; instead, invoke the system_engineer tool whenever code, files, or shells must be touched. "
        "Reuse an engineer_id to keep the same engineer and its remaining memory, or create a new id when you need a fresh engineer. "
        "Provide enough context (objectives, constraints, acceptance criteria) for the engineer to act autonomously. "
        "Always wait for the engineer's reply before taking the next step. When the commander requests a status update, summarize decisions and surface unresolved risks. "
        "Environment snapshot:\n{engineer_system_info}");
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
    TOOLS_INFO cache;
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
["left_down(100,200)", "time_span(0.1)", "move(200,400,0.5)", "time_span(0.1)", "left_up()"] - Left mouse button drag. After the tool is executed, it will return the current screenshot"}},"required":["sequence"]})SCHEMA",
         QStringLiteral("Pass in a sequence of actions to control the mouse and keyboard. {screen_info}")},
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
         R"({"type":"object","properties":{"prompt":{"type":"string","description":"Describe the image you want to draw with a paragraph of English text"}},"required":["prompt"]})",
         QStringLiteral("Describe the image you want to draw with a paragraph of English text. The tool will send the text to the drawing model and then return the drawn image, making sure to input English. You can add modifiers or phrases to improve the quality of the image before the text and separate them with commas.")},
        {promptx::PROMPT_TOOL_EXECUTE,
         "execute_command",
         R"({"type":"object","properties":{"content":{"type":"string","description":"CLI commands"}},"required":["content"]})",
         QStringLiteral("Request to execute CLI commands on the system. Use this command when you need to perform system operations or run specific commands to complete any step of a user task. You must adjust the commands according to the user's system. Prioritize executing complex CLI commands over creating executable scripts, as they are more flexible and easier to run. The command will be executed in the current working directory.")},
        {promptx::PROMPT_TOOL_PTC,
         "ptc",
         R"({"type":"object","properties":{"filename":{"type":"string","description":"Python file name, e.g. helper.py."},"workdir":{"type":"string","description":"Working directory relative to the engineer workspace. Use \".\" for the workspace root."},"content":{"type":"string","description":"Full Python source code that should run inside the target workdir."}},"required":["filename","workdir","content"]})",
         QStringLiteral("When CLI commands become unwieldy, author a Python helper script via programmatic_tool_calling. Provide the filename, the working directory (relative to the engineer workspace), and the script content. EVA stores the file under ptc_temp and executes it immediately, returning stdout/stderr." )},
        {promptx::PROMPT_TOOL_LIST_FILES,
         "list_files",
         R"({"type":"object","properties":{"path":{"type":"string","description":"Optional directory to list (relative to the engineer working directory). Leave blank to use the current working directory."}}})",
         QStringLiteral("List all immediate subfolders and files under a directory. If no path is provided, default to the engineer working directory. Paths are resolved relative to the engineer working directory and output stays compact, one entry per line.")},
        {promptx::PROMPT_TOOL_SEARCH_CONTENT,
         "search_content",
         R"({"type":"object","properties":{"query":{"type":"string","description":"The text to search for."}},"required":["query"]})",
         QStringLiteral("Search all text files under the engineer working directory for a query string (case-insensitive). Returns lines in the form <path>:<line>:<content>.")},
        {promptx::PROMPT_TOOL_READ_FILE,
         "read_file",
         R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to read"},"start_line":{"type":"integer","description":"The starting line number (1-based index). Optional, default is 1","minimum":1},"end_line":{"type":"integer","description":"The ending line number (1-based index). Optional, maximum 200 lines from start_line","minimum":1}},"required":["path"]})",
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
         QStringLiteral("Escalate implementation work to the resident system engineer. Provide an engineer_id (reuse an old one to keep its memory) and a detailed task. The engineer replies with remaining memory and a <=200-character summary.")},
    };
    return templates;
}

QString resolvePrompt(int id, const QString &fallback)
{
    return g_promptMap.value(id, fallback);
}

void rebuildToolEntries()
{
    auto &defs = toolTemplates();
    for (auto &tpl : defs)
    {
        const QString description = resolvePrompt(tpl.descriptionId, tpl.fallback);
        tpl.cache = TOOLS_INFO(QString::fromUtf8(tpl.name), description, QString::fromUtf8(tpl.schema));
    }
}
} // namespace

namespace promptx
{
QString promptById(int id, const QString &fallback)
{
    return resolvePrompt(id, fallback);
}

bool loadPromptLibrary(const QString &resourcePath)
{
    const QString path = resourcePath.isEmpty() ? QStringLiteral(":/language/prompt.ini") : resourcePath;
    const auto map = simpleini::parseFile(path);
    if (map.isEmpty())
    {
        g_promptMap.clear();
        currentExtraPrompt() = defaultExtraPrompt();
        currentEngineerInfo() = defaultEngineerInfo();
        currentEngineerSystemInfo() = defaultEngineerSystemInfo();
        rebuildToolEntries();
        return false;
    }
    g_promptMap.clear();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
    {
        bool ok = false;
        const int id = it.key().toInt(&ok);
        if (!ok) continue;
        g_promptMap.insert(id, it.value());
    }
    currentExtraPrompt() = resolvePrompt(PROMPT_EXTRA_TEMPLATE, defaultExtraPrompt());
    currentEngineerInfo() = resolvePrompt(PROMPT_ENGINEER_INFO, defaultEngineerInfo());
    currentEngineerSystemInfo() = resolvePrompt(PROMPT_ENGINEER_SYSTEM, defaultEngineerSystemInfo());
    currentArchitectInfo() = resolvePrompt(PROMPT_ARCHITECT_INFO, defaultArchitectInfo());
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
} // namespace promptx
