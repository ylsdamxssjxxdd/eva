#include "prompt.h"

#include <QFile>
#include <QTextStream>
#include <QVector>

namespace
{
// 当前提示词语种（默认英文；界面切换时由 UI 主动刷新）
int currentPromptLanguage = EVA_LANG_EN;

bool useChinesePrompt(int languageFlag)
{
    return languageFlag == EVA_LANG_ZH;
}

// 从资源文件读取提示词文本，失败则使用兜底内容
QString readPromptResource(const QString &path, const QString &fallback)
{
    if (path.trimmed().isEmpty()) return fallback;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return fallback;
    }
    QTextStream in(&file);
    in.setCodec("utf-8");
    QString text = in.readAll();
    file.close();
    text = text.trimmed();
    return text.isEmpty() ? fallback : text;
}

QString &currentSystemPrompt()
{
    static QString value = QStringLiteral(DEFAULT_DATE_PROMPT);
    return value;
}

const QString &defaultExtraPromptEn()
{
    // 对齐参考项目 wunder 的工具提示词结构（英文版）
    static const QString value = QStringLiteral(
        "Tool signatures are provided inside the <tools> </tools> XML tag:\n"
        "<tools>\n"
        "{available_tools_describe}\n"
        "</tools>\n"
        "Each tool call must follow these rules:\n"
        "1. Wrap the call in a <tool_call>...</tool_call> block.\n"
        "2. Output valid JSON with only two keys: \"name\" (string) and \"arguments\" (object). Example:\n"
        "<tool_call>\n"
        "{\"name\":\"answer\",\"arguments\":{\"content\":\"Task is complete. How else can I help?\"}}\n"
        "</tool_call>\n"
        "\n"
        "Tool results will be returned as a user message prefixed with \"tool_response: \".\n"
        "\n"
        "{engineer_info}");
    return value;
}

const QString &defaultExtraPromptZh()
{
    // 对齐参考项目 wunder 的工具提示词结构（中文版）
    static const QString value = QStringLiteral(
        "工具签名在 <tools> </tools> XML 标签内提供：\n"
        "<tools>\n"
        "{available_tools_describe}\n"
        "</tools>\n"
        "每次工具调用都必须遵循以下要求：\n"
        "1. 将调用内容放在 <tool_call>...</tool_call> 块中返回。\n"
        "2. 在块内输出有效 JSON，且仅包含两个键：\"name\"(字符串) 和 \"arguments\"(对象)。示例：\n"
        "<tool_call>\n"
        "{\"name\":\"answer\",\"arguments\":{\"content\":\"任务已完成，还有什么我可以帮忙的吗？\"}}\n"
        "</tool_call>\n"
        "\n"
        "工具执行结果会作为以 \"tool_response: \" 前缀的 user 消息返回。\n"
        "\n"
        "{engineer_info}");
    return value;
}

QString &currentExtraPrompt()
{
    static QString value = defaultExtraPromptEn();
    return value;
}

const QString &defaultEngineerInfoEn()
{
    // 参照 wunder 工程师提示词结构（英文版）
    static const QString value = QStringLiteral(
        "Goal: complete the user's task accurately with minimal chatter.\n"
        "- Do not end the response or call \"answer\" until the task is complete.\n"
        "- Before editing files, prefer batch use of read_file/list_files.\n"
        "- Prefer `ptc` when a workflow spans multiple CLI commands, fragile parsing, or structured edits; use it first and fall back to ad-hoc shell only when a one-liner truly suffices.\n"
        "- Keep every response concise; unless explicitly requested, avoid logs or long code blocks.\n"
        "- Call only one tool at a time, and proceed step by step.\n"
        "- For long-running tasks, leave progress traces and deliver stable output; you may use schedule_task to set reminders or recurring jobs.\n"
        "- If instructions are unclear, ask for clarification and avoid hallucinating details.\n"
        "- When the plan board tool is enabled, start with a concise plan using it and keep it updated as you execute.\n"
        "{engineer_system_info}");
    return value;
}

const QString &defaultEngineerInfoZh()
{
    // 参照 wunder 工程师提示词结构（中文版）
    static const QString value = QStringLiteral(
        "目标：用最少闲聊和步骤准确完成用户的任务。\n"
        "- 未完成任务不得结束回复或调用“answer”。\n"
        "- 编辑文件前优先批量 read_file/list_files。\n"
        "- 复杂流程优先使用 `ptc`（多条命令、易出错解析、结构化改动等），能用 ptc 就先用，只有一行命令确实足够时才用零散 shell。\n"
        "- 每次回复保持简洁；除非明确要求，不输出日志或长代码。\n"
        "- 每次只能调用一个工具，一步一步完成用户的任务。\n"
        "- 长时间运行任务需要分段留痕，稳定输出；可以使用 schedule_task 设置提醒或周期任务。\n"
        "- 遇到不明确的指令时，优先请求澄清，避免空想虚构。\n"
        "- 当启用“计划面板”工具时，先用它给出简洁计划，并在执行过程中持续更新状态。\n"
        "{engineer_system_info}");
    return value;
}

QString &currentEngineerInfo()
{
    static QString value = defaultEngineerInfoEn();
    return value;
}

const QString &defaultEngineerSystemInfoEn()
{
    // 参照 wunder 工程师环境摘要（英文版）
    static const QString value = QStringLiteral(
        "OS: {OS}\n"
        "Date: {DATE}\n"
        "Your current working directory (all commands run from here by default): {DIR}\n"
        "Workspace (max 2 levels):\n"
        "{WORKSPACE_TREE}\n"
        "All commands must stay within the working directory and its subdirectories.");
    return value;
}

const QString &defaultEngineerSystemInfoZh()
{
    // 参照 wunder 工程师环境摘要（中文版）
    static const QString value = QStringLiteral(
        "操作系统：{OS}\n"
        "日期：{DATE}\n"
        "你当前所在工作目录，所有命令默认在此路径执行：{DIR}\n"
        "工作区（最多 2 层）：\n"
        "{WORKSPACE_TREE}\n"
        "所有命令仅限当前工作目录及其子目录内执行。");
    return value;
}

QString &currentEngineerSystemInfo()
{
    static QString value = defaultEngineerSystemInfoEn();
    return value;
}

const QString &defaultArchitectInfoEn()
{
    static const QString value = QStringLiteral(
        "You are EVA's system architect; you never run commands or edit code yourself.\n"
        "- Dispatch work via system_engineer_proxy; decide when to reuse engineer_id (preserve memory) or start fresh.\n"
        "- Each request must state objectives, constraints, and acceptance criteria; after tool results, synthesize decisions and risks instead of relaying raw logs.");
    return value;
}

const QString &defaultArchitectInfoZh()
{
    static const QString value = QStringLiteral(
        "你是 EVA 的系统架构师；你不会亲自运行命令或修改代码。\n"
        "- 通过 system_engineer_proxy 下发任务，决定是否复用 engineer_id（保留记忆）或重新开始。\n"
        "- 每次请求必须明确目标、约束与验收标准；拿到工具结果后需要综合决策与风险，而不是直接转述原始日志。");
    return value;
}

QString &currentArchitectInfo()
{
    static QString value = defaultArchitectInfoEn();
    return value;
}

void applyPromptLanguage(int languageFlag);

struct ToolTemplate
{
    promptx::PromptEntryId descriptionId;
    const char *name;
    const char *schema;
    QString fallbackEn;
    QString fallbackZh;
    TOOLS_INFO cache{};
};

QVector<ToolTemplate> &toolTemplates()
{
    static QVector<ToolTemplate> templates = {
        {promptx::PROMPT_TOOL_ANSWER,
         "answer",
         R"({"type":"object","properties":{"content":{"type":"string","description":"The final response to the user"}},"required":["content"]})",
         QStringLiteral("The final response to the user."),
         QStringLiteral("对用户的最终回复内容。")},
        {promptx::PROMPT_TOOL_CALCULATOR,
         "calculator",
         R"({"type":"object","properties":{"expression":{"type":"string","description":"math expression"}},"required":["expression"]})",
         QStringLiteral("Enter a expression to return the calculation result via using tinyexpr."),
         QStringLiteral("输入数学表达式，使用 tinyexpr 返回计算结果。")},
        {promptx::PROMPT_TOOL_CONTROLLER,
         "controller",
         // 桌面控制器的 arguments Schema 会直接拼进系统提示词；使用最小化、单行 JSON，避免提示词被无意义换行/描述拉长。
         R"({"type":"object","properties":{"bbox":{"anyOf":[{"type":"array","items":{"type":"integer"},"minItems":4,"maxItems":4},{"type":"array","items":{"type":"integer"},"minItems":2,"maxItems":2}]},"action":{"type":"string","enum":["left_click","left_double_click","right_click","middle_click","left_hold","right_hold","middle_hold","left_release","right_release","middle_release","scroll_down","scroll_up","press_key","type_text","delay","move_mouse","drag_drop"]},"description":{"type":"string"},"key":{"type":"string"},"text":{"type":"string"},"delay_ms":{"type":"integer","minimum":0},"duration_ms":{"type":"integer","minimum":0},"scroll_steps":{"type":"integer","minimum":1},"to_bbox":{"anyOf":[{"type":"array","items":{"type":"integer"},"minItems":4,"maxItems":4},{"type":"array","items":{"type":"integer"},"minItems":2,"maxItems":2}]}},"required":["bbox","action","description"],"additionalProperties":false})",
         QStringLiteral(
             "Desktop controller:\n"
             "- Use only when the user explicitly requests desktop actions or grants permission; otherwise ask.\n"
             "- `bbox` is in normalized screenshot space {controller_norm_x}x{controller_norm_y} (origin top-left), format: [x1,y1,x2,y2] or [cx,cy]. One call = one atomic action; multi-step = multiple rounds using the latest screenshot.\n"
             "- For `type_text`, EVA auto left-clicks the bbox center to focus before typing `text` (so you usually don't need a separate left_click). Never click/type in EVA's chat input box. press_key uses `key`; delay=`delay_ms`, scroll_steps optional, drag_drop uses `to_bbox` (same format as bbox)."),
         QStringLiteral(
             "桌面控制器：\n"
             "- 仅在用户明确要求桌面操作或授权时使用，否则先询问。\n"
             "- `bbox` 使用归一化截图坐标 {controller_norm_x}x{controller_norm_y}（左上角为原点），格式：[x1,y1,x2,y2] 或 [cx,cy]。一次调用=一个原子动作；多步操作需多轮，并以最新截图为准。\n"
             "- 对 `type_text`，EVA 会先自动左键点击 bbox 中心以聚焦再输入 `text`（通常无需单独 left_click）。不要在 EVA 的聊天输入框内点击/输入。press_key 使用 `key`；delay=`delay_ms`；scroll_steps 可选；drag_drop 使用 `to_bbox`（同 bbox 格式）。")},
        {promptx::PROMPT_TOOL_MCP_LIST,
         "mcp_tools_list",
         R"({"type":"object","properties":{}})",
         QStringLiteral("List all available tools in the current MCP service, which may include tools that you can use."),
         QStringLiteral("列出当前 MCP 服务可用的所有工具，可能包含你可以使用的工具。")},
        {promptx::PROMPT_TOOL_KNOWLEDGE,
         "knowledge",
         R"({"type":"object","properties":{"content":{"type":"string","description":"Keywords to be queried"}},"required":["content"]})",
         QStringLiteral("Ask a question to the knowledge base, the more detailed the question, the better. The knowledge base will return three text segments with the highest similarity to the question. knowledge database describe: {embeddingdb describe}"),
         QStringLiteral("向知识库提问，问题越详细越好。知识库会返回与问题相似度最高的三个文本段。知识库描述：{embeddingdb describe}")},
        {promptx::PROMPT_TOOL_SD,
         "stablediffusion",
         R"({"type":"object","properties":{"prompt":{"type":"string","description":"Describe the image you want to draw."}},"required":["prompt"]})",
         QStringLiteral("Describe the image you want to draw with a paragraph of English text. The tool will send the text to the drawing model and then return the drawn image, making sure to input English. You can add modifiers or phrases to improve the quality of the image before the text and separate them with commas."),
         QStringLiteral("用一段英文文本描述你想绘制的图像。工具会把文本发送到绘图模型并返回生成的图像，请确保输入英文。你可以在文本前添加修饰语或短语并用逗号分隔以提升质量。")},
        {promptx::PROMPT_TOOL_EXECUTE,
         "execute_command",
         R"({"type":"object","properties":{"content":{"type":"string","description":"CLI commands"}},"required":["content"]})",
         QStringLiteral("Request to execute CLI commands on the system. Use this command when you need to perform system operations or run specific commands to complete any step of a user task. You must adjust the commands according to the user's system. Prioritize executing complex CLI commands over creating executable scripts, as they are more flexible and easier to run. The command will be executed in the current working directory."),
         QStringLiteral("请求在系统上执行 CLI 命令。当你需要完成任务步骤时使用。必须根据用户系统调整命令。优先执行复杂 CLI 命令而不是创建可执行脚本，因为更灵活且更易运行。命令将在当前工作目录执行。")},
        {promptx::PROMPT_TOOL_PTC,
         "ptc",
         R"({"type":"object","properties":{"filename":{"type":"string","description":"Python file name, e.g. helper.py."},"workdir":{"type":"string","description":"Working directory relative to the engineer workspace. Use \".\" for the workspace root."},"content":{"type":"string","description":"Full Python source code that should run inside the target workdir."}},"required":["filename","workdir","content"]})",
         QStringLiteral("Programmatic Tool Calling lets you write a Python helper when CLI commands become unwieldy. Provide the file name, working directory (relative to the engineer workspace), and full script content. EVA saves the file under ptc_temp, runs it immediately, and returns stdout/stderr so you can chain additional steps."),
         QStringLiteral("编程式工具调用：当 CLI 命令繁琐时，用 Python 编写助手脚本。提供文件名、工作目录（相对工程师工作区，\".\" 表示根），以及完整脚本内容。EVA 会将文件保存到 ptc_temp 并立即运行，返回 stdout/stderr 以便继续后续步骤。")},
        {promptx::PROMPT_TOOL_LIST_FILES,
         "list_files",
         R"({"type":"object","properties":{"path":{"type":"string","description":"Optional directory to list (relative to the engineer working directory). Leave blank to use the current working directory."}}})",
         QStringLiteral("List all immediate subfolders and files under a directory. If no path is provided, default to the engineer working directory. Paths are resolved relative to the engineer working directory and output stays compact, one entry per line."),
         QStringLiteral("列出目录下所有直接子文件夹和文件。未提供 path 时默认工程师工作目录。路径相对工程师工作目录解析，输出保持紧凑，每行一个条目。")},
        {promptx::PROMPT_TOOL_SEARCH_CONTENT,
         "search_content",
         R"({"type":"object","properties":{"query":{"type":"string","description":"The text to search for (case-insensitive literal)."},"path":{"type":"string","description":"Optional directory to limit the search to, relative to the engineer working directory."},"file_pattern":{"type":"string","description":"Optional glob to filter files, e.g. *.cpp or src/**.ts"}},"required":["query"]})",
         QStringLiteral("Search all text files under the engineer working directory for a query string (case-insensitive). Returns lines in the form <path>:<line>:<content>."),
         QStringLiteral("在工程师工作目录下搜索查询字符串（不区分大小写）。返回格式为 <path>:<line>:<content>。")},
        {promptx::PROMPT_TOOL_READ_FILE,
         "read_file",
         R"({"type":"object","properties":{"files":{"type":"array","description":"List of files to read. Each entry may include optional line ranges.","items":{"type":"object","properties":{"path":{"type":"string","description":"Path to read, relative to the engineer workspace."},"start_line":{"type":"integer","minimum":1,"description":"Optional start line (inclusive)."},"end_line":{"type":"integer","minimum":1,"description":"Optional end line (inclusive)."},"line_ranges":{"type":"array","description":"Optional explicit ranges.","items":{"type":"array","items":[{"type":"integer","minimum":1},{"type":"integer","minimum":1}]}}},"required":["path"]}},"path":{"type":"string","description":"Legacy single-file path."},"start_line":{"type":"integer","minimum":1,"description":"Legacy start line (inclusive)."},"end_line":{"type":"integer","minimum":1,"description":"Legacy end line (inclusive)."}},"anyOf":[{"required":["files"]},{"required":["path"]}]})",
         QStringLiteral("Request to read the content of a file in a specified path, used when you need to check the content of an existing file, such as analyzing code, reviewing text files, or extracting information from a configuration file. You can specify start and end line numbers to read only a portion of the file. Maximum 200 lines can be read at once."),
         QStringLiteral("读取指定路径文件内容，用于检查现有文件（如分析代码、审阅文本或提取配置）。可指定起止行号，单次最多读取 200 行。")},
        {promptx::PROMPT_TOOL_WRITE_FILE,
         "write_file",
         R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to write"},"content":{"type":"string","description":"The file content"}},"required":["path","content"]})",
         QStringLiteral("Request to write content to a file at the specified path. If the file exists, it will be overwritten by the provided content. If the file does not exist, it will be created. This tool will automatically create any directory required for writing files."),
         QStringLiteral("将内容写入指定路径。若文件存在则覆盖；不存在则创建。会自动创建所需目录。")},
        {promptx::PROMPT_TOOL_REPLACE_IN_FILE,
         "replace_in_file",
         R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"old_string":{"type":"string","description":"The exact literal text to be replaced (include surrounding context for uniqueness)."},"new_string":{"type":"string","description":"The exact literal text that will replace old_string."},"expected_replacements":{"type":"number","description":"Number of expected occurrences to replace. Defaults to 1 if omitted.","minimum":1}},"required":["path","old_string","new_string"]})",
         QStringLiteral("Request to replace text inside an existing file. By default, exactly one occurrence of old_string will be replaced. If you need to replace multiple identical occurrences, set expected_replacements to the number of occurrences you expect. For a file of unknown size, initially limit the reading to the first 200 lines, and then continue reading from there based on the situation."),
         QStringLiteral("在现有文件中替换文本。默认仅替换一次。如果需要替换多处相同内容，设置 expected_replacements 为预期次数。对于未知大小文件，先读取前 200 行再按需继续。")},
        {promptx::PROMPT_TOOL_EDIT_IN_FILE,
         "edit_in_file",
         R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"edits":{"type":"array","description":"Ordered list of edit operations.","items":{"type":"object","properties":{"action":{"type":"string","enum":["replace","insert_before","insert_after","delete"],"description":"Type of edit to apply."},"start_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit begins."},"end_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit ends (inclusive). Required for replace and delete."},"new_content":{"type":"string","description":"Replacement or insertion content. Use actual line breaks inside the string for multi-line updates."}},"required":["action","start_line"],"additionalProperties":false}},"ensure_newline_at_eof":{"type":"boolean","description":"Ensure the file ends with a trailing newline when true."}},"required":["path","edits"]})",
         QStringLiteral("Apply structured line-based edits to an existing text file. Each edit is applied from bottom to top to keep line numbers stable. Provide concise context to avoid unintended changes."),
         QStringLiteral("对现有文本文件应用结构化按行编辑。每个编辑从下往上应用以保持行号稳定。提供简洁上下文以避免误改。")},
        {promptx::PROMPT_TOOL_ENGINEER_PROXY,
          "system_engineer_proxy",
          R"({"type":"object","properties":{"engineer_id":{"type":"string","description":"Engineer identifier. Reuse to keep prior context or set a new id to start fresh."},"task":{"type":"string","description":"Task description, including objectives, context, and acceptance criteria."}},"required":["engineer_id","task"]})",
          QStringLiteral("Escalate a task to the resident system engineer. Arguments include engineer_id (string) and task (string). The task should describe the desired outcome, context, and constraints. The engineer replies with a summary of the work (<=200 characters). Use the same engineer_id to reuse prior memory; send a new id to start with a fresh engineer."),
          QStringLiteral("将任务升级给系统工程师。参数包含 engineer_id（字符串）和 task（字符串）。task 应描述目标、上下文与约束。工程师返回 <=200 字摘要。复用 engineer_id 可保留记忆；使用新 id 则从头开始。")},
        {promptx::PROMPT_TOOL_MONITOR,
         "monitor",
         // 监视器工具的作用是“等待 + 截图”形成观测回路；同样使用最小化 schema，减少提示词注入长度。
         R"({"type":"object","properties":{"wait_ms":{"type":"integer","minimum":0,"maximum":30000},"note":{"type":"string"}},"required":["wait_ms"],"additionalProperties":false})",
         QStringLiteral(
             "Desktop monitor:\n"
             "- Use only when you need to watch the screen and wait for a UI change.\n"
             "- One call waits `wait_ms` ms, then you will receive a fresh screenshot.\n"
             "- After each screenshot: if not ready, call monitor again (use backoff); if ready, stop monitoring and call controller.\n"
             "- Do not loop forever: stop after reasonable attempts/time and ask the user."),
         QStringLiteral(
             "桌面监视器：\n"
             "- 仅在需要观察屏幕变化时使用。\n"
             "- 每次调用等待 `wait_ms` 毫秒，然后你会收到新的截图。\n"
             "- 每次截图后：若未就绪则再次调用 monitor（可退避）；就绪则停止监视并调用 controller。\n"
             "- 不要无限循环：合理次数/时间后停止并向用户询问。")},
        {promptx::PROMPT_TOOL_SKILL_CALL,
         "skill_call",
         R"({"type":"object","properties":{"name":{"type":"string","description":"Skill name to load."}},"required":["name"]})",
         QStringLiteral("Return full SKILL.md and directory tree for the specified skill name."),
         QStringLiteral("根据技能名称返回完整 SKILL.md 及技能目录结构。")},
        {promptx::PROMPT_TOOL_SCHEDULE_TASK,
         "schedule_task",
         R"({"type":"object","properties":{"action":{"type":"string","enum":["add","update","remove","enable","disable","get","list","run"]},"job":{"type":"object","properties":{"job_id":{"type":"string"},"name":{"type":"string"},"schedule":{"type":"object","properties":{"kind":{"type":"string","enum":["at","every","cron"]},"at":{"type":"string"},"every_ms":{"type":"integer","minimum":1000},"cron":{"type":"string"},"tz":{"type":"string"}},"required":["kind"]},"session":{"type":"string","enum":["main","isolated"]},"payload":{"type":"object","properties":{"message":{"type":"string"}}},"deliver":{"type":"object"},"enabled":{"type":"boolean"},"delete_after_run":{"type":"boolean"},"dedupe_key":{"type":"string"}}}},"required":["action"]})",
         QStringLiteral("Manage scheduled jobs. Use action add/update/remove/enable/disable/get/list/run. For add/update, provide job.schedule and payload.message."),
         QStringLiteral("定时任务管理：action 支持 add/update/remove/enable/disable/get/list/run。add/update 需提供 job.schedule 与 payload.message。")},
    };
    return templates;
}

void rebuildToolEntries(int languageFlag)
{
    auto &defs = toolTemplates();
    for (auto &tpl : defs)
    {
        const bool zh = useChinesePrompt(languageFlag);
        const QString desc = (zh && !tpl.fallbackZh.isEmpty()) ? tpl.fallbackZh : tpl.fallbackEn;
        tpl.cache = TOOLS_INFO(QString::fromUtf8(tpl.name), desc, QString::fromUtf8(tpl.schema));
    }
}

void applyPromptLanguage(int languageFlag)
{
    currentPromptLanguage = languageFlag;
    // 系统提示词优先从资源文件加载，避免将大段提示词硬编码进 C++ 源文件
    const QString fallback = QStringLiteral(DEFAULT_DATE_PROMPT);
    const QString systemPath = useChinesePrompt(languageFlag)
                                   ? QStringLiteral(DEFAULT_SYSTEM_PROMPT_ZH_RESOURCE)
                                   : QStringLiteral(DEFAULT_SYSTEM_PROMPT_EN_RESOURCE);
    currentSystemPrompt() = readPromptResource(systemPath, fallback);
    if (useChinesePrompt(languageFlag))
    {
        currentExtraPrompt() = defaultExtraPromptZh();
        currentEngineerInfo() = defaultEngineerInfoZh();
        currentEngineerSystemInfo() = defaultEngineerSystemInfoZh();
        currentArchitectInfo() = defaultArchitectInfoZh();
    }
    else
    {
        currentExtraPrompt() = defaultExtraPromptEn();
        currentEngineerInfo() = defaultEngineerInfoEn();
        currentEngineerSystemInfo() = defaultEngineerSystemInfoEn();
        currentArchitectInfo() = defaultArchitectInfoEn();
    }
    rebuildToolEntries(languageFlag);
}
} // namespace

namespace promptx
{
QString promptById(int id, const QString &fallback)
{
    switch (id)
    {
    case PROMPT_SYSTEM_TEMPLATE:
        return currentSystemPrompt();
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
        if (tpl.descriptionId == id) return tpl.cache.description;
    }
    return fallback;
}

bool loadPromptLibrary(const QString &resourcePath)
{
    (void)resourcePath;
    applyPromptLanguage(currentPromptLanguage);
    return true;
}

void setPromptLanguage(int languageFlag)
{
    applyPromptLanguage(languageFlag);
}

const QString &extraPromptTemplate()
{
    return currentExtraPrompt();
}

const QString &systemPromptTemplate()
{
    return currentSystemPrompt();
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

const TOOLS_INFO &toolSkillCall()
{
    return toolTemplates()[16].cache;
}

const TOOLS_INFO &toolScheduleTask()
{
    return toolTemplates()[17].cache;
}
} // namespace promptx
