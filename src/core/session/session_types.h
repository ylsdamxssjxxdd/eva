#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// 会话派发任务类型
enum class ConversationTask
{
    ChatReply,
    Completion,
    ToolLoop,
    Compaction
};

// 会话流程阶段，用于 FlowTracer 打点
enum class FlowPhase
{
    Start,
    Build,
    NetRequest,
    Streaming,
    NetDone,
    ToolParsed,
    ToolStart,
    ToolResult,
    ContinueTurn,
    Finish,
    Cancel
};

// 文档附件描述
struct DocumentAttachment
{
    QString path;
    QString displayName;
    QString markdown;
};

// 一次用户输入的打包结果
struct InputPack
{
    QString text;
    QStringList images;
    QStringList wavs;
    QVector<DocumentAttachment> documents;
};
