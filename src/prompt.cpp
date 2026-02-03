#include "prompt.h"
#include "service/tools/tool_registry.h"

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

QString &currentWunderSystemPrompt()
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
        "\n");
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
        "\n");
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

void applyPromptLanguage(int languageFlag)
{
    currentPromptLanguage = languageFlag;
    // 系统提示词优先从资源文件加载，避免将大段提示词硬编码进 C++ 源文件
    const QString fallback = QStringLiteral(DEFAULT_DATE_PROMPT);
    const QString systemPath = useChinesePrompt(languageFlag)
                                   ? QStringLiteral(DEFAULT_SYSTEM_PROMPT_ZH_RESOURCE)
                                   : QStringLiteral(DEFAULT_SYSTEM_PROMPT_EN_RESOURCE);
    currentSystemPrompt() = readPromptResource(systemPath, fallback);
    const QString wunderPath = useChinesePrompt(languageFlag)
                                   ? QStringLiteral(WUNDER_SYSTEM_PROMPT_ZH_RESOURCE)
                                   : QStringLiteral(WUNDER_SYSTEM_PROMPT_EN_RESOURCE);
    currentWunderSystemPrompt() = readPromptResource(wunderPath, currentSystemPrompt());
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
    ToolRegistry::setLanguage(languageFlag);
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
    if (const TOOLS_INFO *tool = ToolRegistry::findByPromptId(id))
    {
        return tool->description;
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

const QString &wunderSystemPromptTemplate()
{
    return currentWunderSystemPrompt();
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
    return ToolRegistry::toolByIndex(0);
}

const TOOLS_INFO &toolCalculator()
{
    return ToolRegistry::toolByIndex(1);
}

const TOOLS_INFO &toolController()
{
    return ToolRegistry::toolByIndex(2);
}

const TOOLS_INFO &toolMcpList()
{
    return ToolRegistry::toolByIndex(3);
}

const TOOLS_INFO &toolKnowledge()
{
    return ToolRegistry::toolByIndex(4);
}

const TOOLS_INFO &toolStableDiffusion()
{
    return ToolRegistry::toolByIndex(5);
}

const TOOLS_INFO &toolExecuteCommand()
{
    return ToolRegistry::toolByIndex(6);
}

const TOOLS_INFO &toolPtc()
{
    return ToolRegistry::toolByIndex(7);
}

const TOOLS_INFO &toolListFiles()
{
    return ToolRegistry::toolByIndex(8);
}

const TOOLS_INFO &toolSearchContent()
{
    return ToolRegistry::toolByIndex(9);
}

const TOOLS_INFO &toolReadFile()
{
    return ToolRegistry::toolByIndex(10);
}

const TOOLS_INFO &toolWriteFile()
{
    return ToolRegistry::toolByIndex(11);
}

const TOOLS_INFO &toolReplaceInFile()
{
    return ToolRegistry::toolByIndex(12);
}

const TOOLS_INFO &toolEditInFile()
{
    return ToolRegistry::toolByIndex(13);
}

const TOOLS_INFO &toolEngineerProxy()
{
    return ToolRegistry::toolByIndex(14);
}

const TOOLS_INFO &toolMonitor()
{
    return ToolRegistry::toolByIndex(15);
}

const TOOLS_INFO &toolSkillCall()
{
    return ToolRegistry::toolByIndex(16);
}

const TOOLS_INFO &toolScheduleTask()
{
    return ToolRegistry::toolByIndex(17);
}
} // namespace promptx
