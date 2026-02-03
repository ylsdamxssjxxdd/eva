#pragma once

#include <QSettings>

// 配置迁移器：负责将旧版配置键迁移到新版格式，避免每次启动被默认值覆盖。
class ConfigMigrator
{
public:
    // 执行所有已知迁移；必要时写回 settings。
    static void migrate(QSettings &settings);
};
