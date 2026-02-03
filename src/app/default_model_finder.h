#pragma once

#include <QSettings>
#include <QString>

// 启动时的默认模型发现结果。
struct DefaultModelPaths
{
    QString llmModel;
    QString embeddingModel;
    QString whisperModel;
    QString ttsModel;
    QString sdModel;
    QString sdTemplate;
};

// 默认模型发现器：扫描 EVA_MODELS 并选择“最小体积可用模型”。
class DefaultModelFinder
{
public:
    // 在 EVA_MODELS 目录中自动查找最小模型路径。
    static DefaultModelPaths discover(const QString &modelsRoot);
    // 将发现结果写入配置文件（仅写入非空项）。
    static void applyToSettings(QSettings &settings, const DefaultModelPaths &paths);
};
