#pragma once

#include <QString>

#include "app_context.h"

// 应用启动装配器：集中处理环境变量、资源加载、目录准备与默认模型发现。
class AppBootstrap
{
public:
    // 早期环境变量设置（必须在 QApplication 创建前执行）。
    static void applyEarlyEnv();

    // Linux 静态构建输入法与库路径设置（可在 QApplication 前后调用）。
    static void applyLinuxRuntimeEnv();

    // 构建 AppContext（路径、目录与后端探测快照）。
    static AppContext buildContext();

    // 确保 EVA_TEMP 目录存在。
    static void ensureTempDir(const AppContext &ctx);

    // 注册 Sarasa 字体资源（font_out.rcc）。
    static void registerSarasaFontResource(const QString &runtimeDir);

    // Linux 下创建 .desktop 快捷方式。
    static void createLinuxDesktopShortcut(const QString &appPath);

    // 读取主题样式表。
    static QString loadStylesheet();

    // 若无配置文件，则按 EVA_MODELS 自动发现默认模型并写入。
    static void ensureDefaultConfig(const AppContext &ctx);

    // 填充后端探测快照信息（仅用于日志排障）。
    static void snapshotBackendProbe(AppContext &ctx);
};
