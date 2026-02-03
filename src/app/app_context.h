#pragma once

#include <QString>
#include <QStringList>

// 应用启动阶段需要的上下文信息（只读快照）。
// 约定：AppContext 在应用启动后构建完成，后续仅作为只读配置传递。
struct AppContext
{
    QString appDir;          // 可执行程序所在目录
    QString appPath;         // 可执行程序完整路径
    QString tempDir;         // EVA_TEMP 绝对路径
    QString modelsDir;       // EVA_MODELS 绝对路径
    QString backendDir;      // EVA_BACKEND 绝对路径
    QString skillsDir;       // EVA_SKILLS 绝对路径
    QString workDir;         // EVA_WORK 绝对路径

    // 运行环境信息（用于日志与后端探测）
    QString osId;            // e.g. win/linux
    QString archId;          // e.g. x86_64
    QString deviceChoice;    // 用户选择的后端（auto/cuda/...）
    QString effectiveBackend;// 实际解析到的后端
    QString resolvedDevice;  // 解析到的设备信息

    // 后端探测路径快照（排障用）
    QStringList backendRoots;
    QStringList backendProbed;
    QStringList backendAvailable;
};
