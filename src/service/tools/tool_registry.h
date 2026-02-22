#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include "prompt.h"
#include "xconfig.h"

#include <QJsonObject>
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
        int schemaVersion = 1;  // 工具参数 schema 版本
        int timeoutMs = 120000; // 建议超时（毫秒）
        bool highRisk = false;  // 高风险工具（如命令执行/桌面控制）
        TOOLS_INFO cache{};
    };

    static void setLanguage(int languageFlag);
    static const QVector<Entry> &entries();
    static const Entry &entryAt(int index);
    static const TOOLS_INFO &toolByIndex(int index);
    static const TOOLS_INFO *findByPromptId(int id);
    static QJsonObject capabilityByName(const QString &toolName);
    static QJsonObject capabilityManifest();
};

#endif // TOOL_REGISTRY_H
