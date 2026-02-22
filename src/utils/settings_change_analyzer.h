#ifndef SETTINGS_CHANGE_ANALYZER_H
#define SETTINGS_CHANGE_ANALYZER_H

#include "xconfig.h"

#include <QString>
#include <QStringList>

// 设置变化分析结果：
// - restartItems：需要重启本地后端才能生效的字段
// - resetItems：仅需重置会话即可生效的字段
// - requiresSessionReset：只要有变化就需要重置上下文，重启后端场景也包含在内
struct SettingsChangeSummary
{
    bool hasAnyChange = false;
    bool requiresBackendRestart = false;
    bool requiresSessionReset = false;
    QStringList restartItems;
    QStringList resetItems;
};

SettingsChangeSummary analyzeSettingsChanges(const SETTINGS &beforeSettings,
                                            const SETTINGS &afterSettings,
                                            const QString &beforePort,
                                            const QString &afterPort,
                                            const QString &beforeDevice,
                                            const QString &afterDevice,
                                            bool backendOverrideDirty,
                                            int knownMaxNgl);

// 将变化项压缩为短文本，避免状态栏过长。
QString compactChangeItems(const QStringList &items, int maxItems = 3);

#endif // SETTINGS_CHANGE_ANALYZER_H
