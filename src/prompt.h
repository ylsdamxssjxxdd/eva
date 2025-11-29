#pragma once
#include "xconfig.h"

namespace promptx
{
// Numeric ids for prompt entries (kept in sync with resource/language/prompt.ini).
enum PromptEntryId : int
{
    PROMPT_EXTRA_TEMPLATE = 50000,
    PROMPT_ENGINEER_INFO = 50001,
    PROMPT_ENGINEER_SYSTEM = 50002,
    PROMPT_ARCHITECT_INFO = 50003,
    PROMPT_TOOL_ANSWER = 51000,
    PROMPT_TOOL_CALCULATOR = 51001,
    PROMPT_TOOL_CONTROLLER = 51002,
    PROMPT_TOOL_MCP_LIST = 51003,
    PROMPT_TOOL_KNOWLEDGE = 51004,
    PROMPT_TOOL_SD = 51005,
    PROMPT_TOOL_EXECUTE = 51006,
    PROMPT_TOOL_LIST_FILES = 51007,
    PROMPT_TOOL_SEARCH_CONTENT = 51008,
    PROMPT_TOOL_READ_FILE = 51009,
    PROMPT_TOOL_WRITE_FILE = 51010,
    PROMPT_TOOL_REPLACE_IN_FILE = 51011,
    PROMPT_TOOL_EDIT_IN_FILE = 51012,
    PROMPT_TOOL_PTC = 51013,
    PROMPT_TOOL_ENGINEER_PROXY = 51014
};

// Load prompt definitions from prompt.ini (qrc path by default).
bool loadPromptLibrary(const QString &resourcePath = QString());

// Resolve arbitrary prompt ids with fallback to provided text.
QString promptById(int id, const QString &fallback = QString());

// Frequently used prompt blocks.
const QString &extraPromptTemplate();
const QString &engineerInfo();
const QString &engineerSystemInfo();
const QString &architectInfo();

// Built-in tool descriptors with descriptions resolved from prompt.ini.
const TOOLS_INFO &toolAnswer();
const TOOLS_INFO &toolCalculator();
const TOOLS_INFO &toolController();
const TOOLS_INFO &toolMcpList();
const TOOLS_INFO &toolKnowledge();
const TOOLS_INFO &toolStableDiffusion();
const TOOLS_INFO &toolExecuteCommand();
const TOOLS_INFO &toolPtc();
const TOOLS_INFO &toolListFiles();
const TOOLS_INFO &toolSearchContent();
const TOOLS_INFO &toolReadFile();
const TOOLS_INFO &toolWriteFile();
const TOOLS_INFO &toolReplaceInFile();
const TOOLS_INFO &toolEditInFile();
const TOOLS_INFO &toolEngineerProxy();
} // namespace promptx
