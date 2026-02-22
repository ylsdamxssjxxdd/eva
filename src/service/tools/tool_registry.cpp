#include "service/tools/tool_registry.h"

#include <QJsonArray>

namespace
{
bool useChinesePrompt(int languageFlag)
{
    return languageFlag == EVA_LANG_ZH;
}

void applyCapabilityMetadata(QVector<ToolRegistry::Entry> &entries)
{
    auto applyOne = [&entries](const QString &name, int schemaVersion, int timeoutMs, bool highRisk)
    {
        for (auto &entry : entries)
        {
            if (entry.name != name) continue;
            entry.schemaVersion = schemaVersion;
            entry.timeoutMs = timeoutMs;
            entry.highRisk = highRisk;
            return;
        }
    };

    // 统一维护工具执行元信息，便于后续做限流、超时与风险提醒。
    applyOne(QStringLiteral("answer"), 1, 10000, false);
    applyOne(QStringLiteral("calculator"), 1, 10000, false);
    applyOne(QStringLiteral("controller"), 1, 90000, true);
    applyOne(QStringLiteral("monitor"), 1, 60000, false);
    applyOne(QStringLiteral("knowledge"), 1, 30000, false);
    applyOne(QStringLiteral("stablediffusion"), 1, 180000, false);
    applyOne(QStringLiteral("execute_command"), 1, 180000, true);
    applyOne(QStringLiteral("ptc"), 1, 180000, true);
    applyOne(QStringLiteral("list_files"), 1, 30000, false);
    applyOne(QStringLiteral("search_content"), 1, 60000, false);
    applyOne(QStringLiteral("read_file"), 1, 30000, false);
    applyOne(QStringLiteral("write_file"), 1, 30000, true);
    applyOne(QStringLiteral("replace_in_file"), 1, 30000, true);
    applyOne(QStringLiteral("edit_in_file"), 1, 30000, true);
    applyOne(QStringLiteral("system_engineer_proxy"), 1, 120000, false);
    applyOne(QStringLiteral("skill_call"), 1, 30000, false);
    applyOne(QStringLiteral("schedule_task"), 1, 30000, false);
}

QJsonObject buildCapabilityObject(const ToolRegistry::Entry &entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), entry.name);
    object.insert(QStringLiteral("schema"), entry.schema);
    object.insert(QStringLiteral("schema_version"), entry.schemaVersion);
    object.insert(QStringLiteral("timeout_ms"), entry.timeoutMs);
    object.insert(QStringLiteral("high_risk"), entry.highRisk);
    object.insert(QStringLiteral("description"),
                  entry.cache.description.isEmpty() ? entry.fallbackEn : entry.cache.description);
    return object;
}

QVector<ToolRegistry::Entry> &mutableEntries()
{
    static QVector<ToolRegistry::Entry> entries = {
        {promptx::PROMPT_TOOL_ANSWER,
         QStringLiteral("answer"),
         QStringLiteral(R"({"type":"object","properties":{"content":{"type":"string","description":"The final response to the user"}},"required":["content"]})"),
         QStringLiteral("The final response to the user."),
         QStringLiteral("对用户的最终回复内容。")},
        {promptx::PROMPT_TOOL_CALCULATOR,
         QStringLiteral("calculator"),
         QStringLiteral(R"({"type":"object","properties":{"expression":{"type":"string","description":"math expression"}},"required":["expression"]})"),
         QStringLiteral("Enter a expression to return the calculation result via using tinyexpr."),
         QStringLiteral("输入数学表达式，使用 tinyexpr 返回计算结果。")},
        {promptx::PROMPT_TOOL_CONTROLLER,
         QStringLiteral("controller"),
         QStringLiteral(R"({"type":"object","properties":{"bbox":{"anyOf":[{"type":"array","items":{"type":"integer"},"minItems":4,"maxItems":4},{"type":"array","items":{"type":"integer"},"minItems":2,"maxItems":2}]},"action":{"type":"string","enum":["left_click","left_double_click","right_click","middle_click","left_hold","right_hold","middle_hold","left_release","right_release","middle_release","scroll_down","scroll_up","press_key","type_text","delay","move_mouse","drag_drop"]},"description":{"type":"string"},"key":{"type":"string"},"text":{"type":"string"},"delay_ms":{"type":"integer","minimum":0},"duration_ms":{"type":"integer","minimum":0},"scroll_steps":{"type":"integer","minimum":1},"to_bbox":{"anyOf":[{"type":"array","items":{"type":"integer"},"minItems":4,"maxItems":4},{"type":"array","items":{"type":"integer"},"minItems":2,"maxItems":2}]}},"required":["bbox","action","description"],"additionalProperties":false})"),
         QStringLiteral(
             "Desktop controller:\n"
             "- Use only when the user explicitly requests desktop actions or grants permission; otherwise ask.\n"
             "- `bbox` is in normalized screenshot space {controller_norm_x}x{controller_norm_y} (origin top-left), format: [x1,y1,x2,y2] or [cx,cy]. One call = one atomic action; multi-step = multiple rounds using the latest screenshot.\n"
             "- For `type_text`, EVA auto left-clicks the bbox center to focus before typing `text` (so you usually don't need a separate left_click). Never click/type in EVA's chat input box. press_key uses `key`; delay=`delay_ms`, scroll_steps optional, drag_drop uses `to_bbox` (same format as bbox)."),
         QStringLiteral(
             "桌面控制器：\n"
             "- 仅在用户明确要求桌面操作或授权时使用，否则先询问。\n"
             "- `bbox` 使用归一化截图坐标 {controller_norm_x}x{controller_norm_y}（左上角为原点），格式：[x1,y1,x2,y2] 或 [cx,cy]。一次调用=一个原子动作，多步操作需多轮并以上次截图为准。\n"
             "- `type_text` 会自动先点击 bbox 中心聚焦再输入 `text`（一般无需单独 left_click）。不要在 EVA 聊天输入框点击/输入。press_key 用 `key`，delay=`delay_ms`，scroll_steps 可选；drag_drop 使用 `to_bbox`（同 bbox 格式）。")},
        {promptx::PROMPT_TOOL_MCP_LIST,
         QStringLiteral("mcp_tools_list"),
         QStringLiteral(R"({"type":"object","properties":{}})"),
         QStringLiteral("List all available tools in the current MCP service, which may include tools that you can use."),
         QStringLiteral("列出当前 MCP 服务可用的所有工具，可能包含你可使用的工具。")},
        {promptx::PROMPT_TOOL_KNOWLEDGE,
         QStringLiteral("knowledge"),
         QStringLiteral(R"({"type":"object","properties":{"content":{"type":"string","description":"Keywords to be queried"}},"required":["content"]})"),
         QStringLiteral("Ask a question to the knowledge base, the more detailed the question, the better. The knowledge base will return three text segments with the highest similarity to the question. knowledge database describe: {embeddingdb describe}"),
         QStringLiteral("向知识库提问，问题越详细越好。知识库会返回与问题相似度最高的三个文本段。知识库描述：{embeddingdb describe}")},
        {promptx::PROMPT_TOOL_SD,
         QStringLiteral("stablediffusion"),
         QStringLiteral(R"({"type":"object","properties":{"prompt":{"type":"string","description":"Describe the image you want to draw."}},"required":["prompt"]})"),
         QStringLiteral("Describe the image you want to draw with a paragraph of English text. The tool will send the text to the drawing model and then return the drawn image, making sure to input English. You can add modifiers or phrases to improve the quality of the image before the text and separate them with commas."),
         QStringLiteral("用一段英文文本描述你想绘制的图像。工具会把文本发送到绘图模型并返回生成的图像，请确保输入英文。你可以在文本前添加修饰词或短语并用逗号分隔以提升质量。")},
        {promptx::PROMPT_TOOL_EXECUTE,
         QStringLiteral("execute_command"),
         QStringLiteral(R"({"type":"object","properties":{"content":{"type":"string","description":"CLI commands"}},"required":["content"]})"),
         QStringLiteral("Request to execute CLI commands on the system. Use this command when you need to perform system operations or run specific commands to complete any step of a user task. You must adjust the commands according to the user's system. Prioritize executing complex CLI commands over creating executable scripts, as they are more flexible and easier to run. The command will be executed in the current working directory."),
         QStringLiteral("请求在系统上执行 CLI 命令。当你需要完成任务步骤时使用。必须根据用户系统调整命令。优先执行复杂 CLI 命令而不是创建可执行脚本，因为更灵活更易运行。命令将在当前工作目录执行。")},
        {promptx::PROMPT_TOOL_PTC,
         QStringLiteral("ptc"),
         QStringLiteral(R"({"type":"object","properties":{"filename":{"type":"string","description":"Python file name, e.g. helper.py."},"workdir":{"type":"string","description":"Working directory relative to the engineer workspace. Use \".\" for the workspace root."},"content":{"type":"string","description":"Full Python source code that should run inside the target workdir."}},"required":["filename","workdir","content"]})"),
         QStringLiteral("Programmatic Tool Calling lets you write a Python helper when CLI commands become unwieldy. Provide the file name, working directory (relative to the engineer workspace), and full script content. EVA saves the file under ptc_temp, runs it immediately, and returns stdout/stderr so you can chain additional steps."),
         QStringLiteral("编程式工具调用：当 CLI 命令繁琐时，用 Python 编写助手脚本。提供文件名、工作目录（相对工程师工作区，“.” 表示根），以及完整脚本内容。EVA 会将文件保存到 ptc_temp 并立即运行，返回 stdout/stderr 以便继续。")},
        {promptx::PROMPT_TOOL_LIST_FILES,
         QStringLiteral("list_files"),
         QStringLiteral(R"({"type":"object","properties":{"path":{"type":"string","description":"Optional directory to list (relative to the engineer working directory). Leave blank to use the current working directory."}}})"),
         QStringLiteral("List all immediate subfolders and files under a directory. If no path is provided, default to the engineer working directory. Paths are resolved relative to the engineer working directory and output stays compact, one entry per line."),
         QStringLiteral("列出目录下所有直接子文件夹和文件。未提供 path 时默认工程师工作目录。路径相对工程师工作目录解析，输出紧凑，每行一项。")},
        {promptx::PROMPT_TOOL_SEARCH_CONTENT,
         QStringLiteral("search_content"),
         QStringLiteral(R"({"type":"object","properties":{"query":{"type":"string","description":"The text to search for (case-insensitive literal)."},"path":{"type":"string","description":"Optional directory to limit the search to, relative to the engineer working directory."},"file_pattern":{"type":"string","description":"Optional glob to filter files, e.g. *.cpp or src/**.ts"}},"required":["query"]})"),
         QStringLiteral("Search all text files under the engineer working directory for a query string (case-insensitive). Returns lines in the form <path>:<line>:<content>."),
         QStringLiteral("在工程师工作目录下搜索文本（不区分大小写）。返回格式为 <path>:<line>:<content>。")},
        {promptx::PROMPT_TOOL_READ_FILE,
         QStringLiteral("read_file"),
         QStringLiteral(R"({"type":"object","properties":{"files":{"type":"array","description":"List of files to read. Each entry may include optional line ranges.","items":{"type":"object","properties":{"path":{"type":"string","description":"Path to read, relative to the engineer workspace."},"start_line":{"type":"integer","minimum":1,"description":"Optional start line (inclusive)."},"end_line":{"type":"integer","minimum":1,"description":"Optional end line (inclusive)."},"line_ranges":{"type":"array","description":"Optional explicit ranges.","items":{"type":"array","items":[{"type":"integer","minimum":1},{"type":"integer","minimum":1}]}}},"required":["path"]}},"path":{"type":"string","description":"Legacy single-file path."},"start_line":{"type":"integer","minimum":1,"description":"Legacy start line (inclusive)."},"end_line":{"type":"integer","minimum":1,"description":"Legacy end line (inclusive)."}},"anyOf":[{"required":["files"]},{"required":["path"]}]})"),
         QStringLiteral("Request to read the content of a file in a specified path, used when you need to check the content of an existing file, such as analyzing code, reviewing text files, or extracting information from a configuration file. You can specify start and end line numbers to read only a portion of the file. Maximum 200 lines can be read at once."),
         QStringLiteral("读取指定路径文件内容，用于检查现有文件（如分析代码、审阅文本或提取配置）。可指定起止行号，单次最多读取 200 行。")},
        {promptx::PROMPT_TOOL_WRITE_FILE,
         QStringLiteral("write_file"),
         QStringLiteral(R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to write"},"content":{"type":"string","description":"The file content"}},"required":["path","content"]})"),
         QStringLiteral("Request to write content to a file at the specified path. If the file exists, it will be overwritten by the provided content. If the file does not exist, it will be created. This tool will automatically create any directory required for writing files."),
         QStringLiteral("将内容写入指定路径。若文件存在则覆盖，不存在则创建。会自动创建所需目录。")},
        {promptx::PROMPT_TOOL_REPLACE_IN_FILE,
         QStringLiteral("replace_in_file"),
         QStringLiteral(R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"old_string":{"type":"string","description":"The exact literal text to be replaced (include surrounding context for uniqueness)."},"new_string":{"type":"string","description":"The exact literal text that will replace old_string."},"expected_replacements":{"type":"number","description":"Number of expected occurrences to replace. Defaults to 1 if omitted.","minimum":1}},"required":["path","old_string","new_string"]})"),
         QStringLiteral("Request to replace text inside an existing file. By default, exactly one occurrence of old_string will be replaced. If you need to replace multiple identical occurrences, set expected_replacements to the number of occurrences you expect. For a file of unknown size, initially limit the reading to the first 200 lines, and then continue reading from there based on the situation."),
         QStringLiteral("在现有文件中替换文本。默认仅替换一次；若需替换多处相同内容，请设置 expected_replacements。对未知大小文件，先读前 200 行再按需继续。")},
        {promptx::PROMPT_TOOL_EDIT_IN_FILE,
         QStringLiteral("edit_in_file"),
         QStringLiteral(R"({"type":"object","properties":{"path":{"type":"string","description":"The file path which you want to edit."},"edits":{"type":"array","description":"Ordered list of edit operations.","items":{"type":"object","properties":{"action":{"type":"string","enum":["replace","insert_before","insert_after","delete"],"description":"Type of edit to apply."},"start_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit begins."},"end_line":{"type":"integer","minimum":1,"description":"1-based line number where the edit ends (inclusive). Required for replace and delete."},"new_content":{"type":"string","description":"Replacement or insertion content. Use actual line breaks inside the string for multi-line updates."}},"required":["action","start_line"],"additionalProperties":false}},"ensure_newline_at_eof":{"type":"boolean","description":"Ensure the file ends with a trailing newline when true."}},"required":["path","edits"]})"),
         QStringLiteral("Apply structured line-based edits to an existing text file. Each edit is applied from bottom to top to keep line numbers stable. Provide concise context to avoid unintended changes."),
         QStringLiteral("对现有文本文件应用结构化按行编辑。每条编辑从下往上应用以保持行号稳定。提供简洁上下文避免误改。")},
        {promptx::PROMPT_TOOL_ENGINEER_PROXY,
         QStringLiteral("system_engineer_proxy"),
         QStringLiteral(R"({"type":"object","properties":{"engineer_id":{"type":"string","description":"Engineer identifier. Reuse to keep prior context or set a new id to start fresh."},"task":{"type":"string","description":"Task description, including objectives, context, and acceptance criteria."}},"required":["engineer_id","task"]})"),
         QStringLiteral("Escalate a task to the resident system engineer. Arguments include engineer_id (string) and task (string). The task should describe the desired outcome, context, and constraints. The engineer replies with a summary of the work (<=200 characters). Use the same engineer_id to reuse prior memory; send a new id to start with a fresh engineer."),
         QStringLiteral("将任务升级给系统工程师。参数含 engineer_id 与 task。task 应描述目标、上下文与约束。工程师返回 <=200 字摘要。复用 engineer_id 可保留记忆；使用新 id 则从头开始。")},
        {promptx::PROMPT_TOOL_MONITOR,
         QStringLiteral("monitor"),
         QStringLiteral(R"({"type":"object","properties":{"wait_ms":{"type":"integer","minimum":0,"maximum":30000},"note":{"type":"string"}},"required":["wait_ms"],"additionalProperties":false})"),
         QStringLiteral(
             "Desktop monitor:\n"
             "- Use only when you need to watch the screen and wait for a UI change.\n"
             "- One call waits `wait_ms` ms, then you will receive a fresh screenshot.\n"
             "- After each screenshot: if not ready, call monitor again (use backoff); if ready, stop monitoring and call controller.\n"
             "- Do not loop forever: stop after reasonable attempts/time and ask the user."),
         QStringLiteral(
             "桌面监视器：\n"
             "- 仅在需要观察屏幕变化时使用。\n"
             "- 每次调用等待 `wait_ms` 毫秒，然后获得新的截图。\n"
             "- 每次截图后：未就绪则再次调用 monitor（可退避），就绪则停止监视并调用 controller。\n"
             "- 不要无限循环：合理次数/时间后停止并询问用户。")},
        {promptx::PROMPT_TOOL_SKILL_CALL,
         QStringLiteral("skill_call"),
         QStringLiteral(R"({"type":"object","properties":{"name":{"type":"string","description":"Skill name to load."}},"required":["name"]})"),
         QStringLiteral("Return full SKILL.md and directory tree for the specified skill name."),
         QStringLiteral("根据技能名称返回完整 SKILL.md 及技能目录结构。")},
        {promptx::PROMPT_TOOL_SCHEDULE_TASK,
         QStringLiteral("schedule_task"),
         QStringLiteral(R"({"type":"object","properties":{"action":{"type":"string","enum":["add","update","remove","enable","disable","get","list","run"]},"job":{"type":"object","properties":{"job_id":{"type":"string"},"name":{"type":"string"},"schedule":{"type":"object","properties":{"kind":{"type":"string","enum":["at","every","cron"]},"at":{"type":"string"},"every_ms":{"type":"integer","minimum":1000},"cron":{"type":"string"},"tz":{"type":"string"}},"required":["kind"]},"session":{"type":"string","enum":["main","isolated"]},"payload":{"type":"object","properties":{"message":{"type":"string"}}},"deliver":{"type":"object"},"enabled":{"type":"boolean"},"delete_after_run":{"type":"boolean"},"dedupe_key":{"type":"string"}}}},"required":["action"]})"),
         QStringLiteral("Manage scheduled jobs. Use action add/update/remove/enable/disable/get/list/run. For add/update, provide job.schedule and payload.message."),
         QStringLiteral("定时任务管理：action 支持 add/update/remove/enable/disable/get/list/run。add/update 需提供 job.schedule 与 payload.message。")},
    };
    static bool metadataApplied = false;
    if (!metadataApplied)
    {
        metadataApplied = true;
        applyCapabilityMetadata(entries);
    }
    return entries;
}

void rebuildToolEntries(int languageFlag)
{
    auto &defs = mutableEntries();
    for (auto &tpl : defs)
    {
        const bool zh = useChinesePrompt(languageFlag);
        const QString desc = (zh && !tpl.fallbackZh.isEmpty()) ? tpl.fallbackZh : tpl.fallbackEn;
        tpl.cache = TOOLS_INFO(tpl.name, desc, tpl.schema);
    }
}
} // namespace

void ToolRegistry::setLanguage(int languageFlag)
{
    rebuildToolEntries(languageFlag);
}

const QVector<ToolRegistry::Entry> &ToolRegistry::entries()
{
    return mutableEntries();
}

const ToolRegistry::Entry &ToolRegistry::entryAt(int index)
{
    auto &defs = mutableEntries();
    static Entry empty{promptx::PROMPT_TOOL_ANSWER, QString(), QString(), QString(), QString(), 1, 120000, false, TOOLS_INFO()};
    if (index < 0 || index >= defs.size()) return empty;
    return defs[index];
}

const TOOLS_INFO &ToolRegistry::toolByIndex(int index)
{
    return entryAt(index).cache;
}

const TOOLS_INFO *ToolRegistry::findByPromptId(int id)
{
    auto &defs = mutableEntries();
    for (auto &tpl : defs)
    {
        if (tpl.descriptionId == id) return &tpl.cache;
    }
    return nullptr;
}

QJsonObject ToolRegistry::capabilityByName(const QString &toolName)
{
    const QString expected = toolName.trimmed().toLower();
    if (expected.isEmpty()) return QJsonObject();

    const auto &defs = mutableEntries();
    for (const auto &entry : defs)
    {
        if (entry.name.toLower() != expected) continue;
        return buildCapabilityObject(entry);
    }
    return QJsonObject();
}

QJsonObject ToolRegistry::capabilityManifest()
{
    const auto &defs = mutableEntries();
    QJsonArray tools;
    for (const auto &entry : defs)
    {
        tools.append(buildCapabilityObject(entry));
    }

    QJsonObject manifest;
    manifest.insert(QStringLiteral("manifest_version"), 1);
    manifest.insert(QStringLiteral("tool_count"), defs.size());
    manifest.insert(QStringLiteral("tools"), tools);
    return manifest;
}
