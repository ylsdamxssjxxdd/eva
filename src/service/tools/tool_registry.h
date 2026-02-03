#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include "prompt.h"
#include "xconfig.h"

#include <QVector>

// 工具注册表：集中维护 tool schema 与多语言描述
class ToolRegistry
{
  public:
    struct Entry
    {
        promptx::PromptEntryId descriptionId;
        QString name;
        QString schema;
        QString fallbackEn;
        QString fallbackZh;
        TOOLS_INFO cache{};
    };

    static void setLanguage(int languageFlag);
    static const QVector<Entry> &entries();
    static const Entry &entryAt(int index);
    static const TOOLS_INFO &toolByIndex(int index);
    static const TOOLS_INFO *findByPromptId(int id);
};

#endif // TOOL_REGISTRY_H
