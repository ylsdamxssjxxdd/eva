#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Centralized helpers to build OpenAI-compatible chat messages and keep
// prompt flow aligned with llama.cpp server chat templates.
// Note: This only reshapes data; actual chat templating/rendering is handled
// by the server (remote mode) or llama.cpp common chat templates (local mode).
namespace promptx
{

// Return an OpenAI-compatible messages array built from UI history, ensuring:
// - system message is present (injects provided systemPrompt when missing)
// - assistant "reasoning" (<think>...</think>) from past turns is stripped
// - multimodal parts (image_url / input_audio) are preserved
QJsonArray buildOaiChatMessages(const QJsonArray &uiMessages,
                                const QString &systemPrompt,
                                const QString &systemRole,
                                const QString &userRole,
                                const QString &asstRole,
                                const QString &toolRole);

// Backward-compatible overload: 5-parameter wrapper calls 6-parameter version with tool role = "tool"
inline QJsonArray buildOaiChatMessages(const QJsonArray &uiMessages,
                                const QString &systemPrompt,
                                const QString &systemRole,
                                const QString &userRole,
                                const QString &asstRole)
{
    return buildOaiChatMessages(uiMessages, systemPrompt, systemRole, userRole, asstRole, QStringLiteral("tool"));
}

} // namespace promptx
