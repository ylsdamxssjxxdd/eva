#pragma once

#include <QObject>
#include <QString>

class Widget;

// 工具流控制器：负责工具调用解析、工具回环与工具结果回注。
class ToolFlowController : public QObject
{
    Q_OBJECT
public:
    explicit ToolFlowController(Widget *owner);

    void recvPushover();
    void recvToolCalls(const QString &payload);
    void recvToolPushover(QString toolResult);

private:
    Widget *w_ = nullptr; // 不拥有，仅用于访问 UI 与状态
};
