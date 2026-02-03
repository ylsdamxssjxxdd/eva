#pragma once

#include <QObject>
#include <QString>

#include "core/session/session_types.h"
#include "xconfig.h"

class Widget;

// 会话控制器：负责会话编排、消息组织、压缩与发送流程。
class SessionController : public QObject
{
    Q_OBJECT
public:
    explicit SessionController(Widget *owner);
    
    // 会话与请求构建
    ENDPOINT_DATA prepareEndpointData();
    void beginSessionIfNeeded();

    // 输入与消息构建
    bool buildDocumentAttachment(const QString &path, DocumentAttachment &attachment);
    QString formatDocumentPayload(const DocumentAttachment &doc) const;
    QString describeDocumentList(const QVector<DocumentAttachment> &docs) const;
    void collectUserInputs(InputPack &pack, bool attachControllerFrame);

    // 会话流
    void handleChatReply(ENDPOINT_DATA &data, const InputPack &in);
    void handleCompletion(ENDPOINT_DATA &data);
    void handleToolLoop(ENDPOINT_DATA &data);
    void logCurrentTask(ConversationTask task);
    void startTurnFlow(ConversationTask task, bool continuingTool);
    void finishTurnFlow(const QString &reason, bool success);
    void ensureSystemHeader(const QString &systemText);

    // 上下文压缩
    bool shouldTriggerCompaction() const;
    bool startCompactionIfNeeded(const InputPack &pendingInput);
    void startCompactionRun(const QString &reason);
    QString extractMessageTextForCompaction(const QJsonObject &msg) const;
    QString buildCompactionSourceText(int fromIndex, int toIndex) const;
    bool appendCompactionSummaryFile(const QJsonObject &summaryObj) const;
    void applyCompactionSummary(const QString &summaryText);
    void handleCompactionReply(const QString &summaryText, const QString &reasoningText);
    void resumeSendAfterCompaction();

private:
    Widget *w_ = nullptr; // 不拥有，仅用于访问 UI 与状态
};
