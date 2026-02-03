#include "config_migrator.h"

#include <QDebug>
#include <QLocale>

#include "xconfig.h"

// 迁移策略：
// 1) 将旧版数值/百分比键迁移为字符串键（temp_str/repeat_str/hid_top_p_str）。
// 2) 应用默认工具配置迁移（tool_call_mode/engineer 等）。
void ConfigMigrator::migrate(QSettings &settings)
{
    // Migrate legacy numeric/percent keys -> string keys, then remove legacy keys.
    // 注意：不能在旧键不存在时用“默认值”覆盖 string 键，否则会导致 temp/repeat 等持久化每次启动都被重置。
    {
        auto parseDoubleC = [](const QString &text, bool &ok) -> double
        {
            return QLocale::c().toDouble(text, &ok);
        };

        // top_p：优先从 hid_top_p 迁移，其次从 hid_top_p_percent 迁移；若 hid_top_p_str 已合法则不覆盖。
        {
            bool okStr = false;
            const QString s = settings.value("hid_top_p_str", "").toString().trimmed();
            const double strv = s.isEmpty() ? -1.0 : parseDoubleC(s, okStr);
            const bool strValid = okStr && (strv > 0.0 && strv <= 1.0);

            if (settings.contains("hid_top_p"))
            {
                const double num = settings.value("hid_top_p").toDouble();
                const bool numValid = (num > 0.0 && num <= 1.0);
                if (numValid && (!strValid || qAbs(num - strv) > 0.0005))
                {
                    settings.setValue("hid_top_p_str", QString::number(num, 'f', 6));
                }
            }
            else if (!strValid && settings.contains("hid_top_p_percent"))
            {
                const int perc = settings.value("hid_top_p_percent").toInt();
                const double v = qBound(0, perc, 100) / 100.0;
                if (v > 0.0 && v <= 1.0)
                {
                    settings.setValue("hid_top_p_str", QString::number(v, 'f', 6));
                }
            }
            settings.remove("hid_top_p");
            settings.remove("hid_top_p_percent");
        }

        // temp：UI 统一按 0~2 映射；旧版 percent 兼容 0~200（=> 0.0~2.0）。
        {
            bool okStr = false;
            const QString ts = settings.value("temp_str", "").toString().trimmed();
            const double strv = ts.isEmpty() ? -1.0 : parseDoubleC(ts, okStr);
            const bool strValid = okStr && (strv >= 0.0 && strv <= 2.0);

            if (settings.contains("temp"))
            {
                const double num = settings.value("temp").toDouble();
                const bool numValid = (num >= 0.0 && num <= 2.0);
                if (numValid && (!strValid || qAbs(num - strv) > 0.0005))
                {
                    settings.setValue("temp_str", QString::number(num, 'f', 6));
                }
            }
            else if (!strValid && settings.contains("temp_percent"))
            {
                const int perc = settings.value("temp_percent").toInt();
                const double v = qBound(0, perc, 200) / 100.0;
                if (v >= 0.0 && v <= 2.0)
                {
                    settings.setValue("temp_str", QString::number(v, 'f', 6));
                }
            }
            settings.remove("temp");
            settings.remove("temp_percent");
        }

        // repeat：旧版 percent 兼容 0~200（=> 0.0~2.0）。
        {
            bool okStr = false;
            const QString rs = settings.value("repeat_str", "").toString().trimmed();
            const double strv = rs.isEmpty() ? -1.0 : parseDoubleC(rs, okStr);
            const bool strValid = okStr && (strv >= 0.0 && strv <= 2.0);

            if (settings.contains("repeat"))
            {
                const double num = settings.value("repeat").toDouble();
                const bool numValid = (num >= 0.0 && num <= 2.0);
                if (numValid && (!strValid || qAbs(num - strv) > 0.0005))
                {
                    settings.setValue("repeat_str", QString::number(num, 'f', 6));
                }
            }
            else if (!strValid && settings.contains("repeat_percent"))
            {
                const int perc = settings.value("repeat_percent").toInt();
                const double v = qBound(0, perc, 200) / 100.0;
                if (v >= 0.0 && v <= 2.0)
                {
                    settings.setValue("repeat_str", QString::number(v, 'f', 6));
                }
            }
            settings.remove("repeat");
            settings.remove("repeat_percent");
        }

        settings.sync();
    }

    // 默认工具配置迁移：确保新版默认 tool_call + 工程师工具自动挂载
    {
        const QString migrateKey = QStringLiteral("defaults_migrated_20260203");
        if (!settings.value(migrateKey, false).toBool())
        {
            bool updated = false;
            const int storedToolCall = settings.value("tool_call_mode", DEFAULT_TOOL_CALL_MODE).toInt();
            if (storedToolCall != DEFAULT_TOOL_CALL_MODE)
            {
                settings.setValue("tool_call_mode", DEFAULT_TOOL_CALL_MODE);
                updated = true;
            }
            const bool storedEngineer = settings.value("engineer_checkbox", DEFAULT_ENGINEER_ENABLED).toBool();
            if (storedEngineer != DEFAULT_ENGINEER_ENABLED)
            {
                settings.setValue("engineer_checkbox", DEFAULT_ENGINEER_ENABLED);
                updated = true;
            }
            QStringList enabledTools = settings.value("enabled_tools").toStringList();
            if (DEFAULT_ENGINEER_ENABLED && !enabledTools.contains(QStringLiteral("engineer")))
            {
                enabledTools << QStringLiteral("engineer");
                settings.setValue("enabled_tools", enabledTools);
                updated = true;
            }
            if (updated)
            {
                settings.setValue(migrateKey, true);
                settings.sync();
            }
        }
    }
}
