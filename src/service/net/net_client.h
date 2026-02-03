#pragma once

#include <QObject>

#include "service/net/request_snapshot.h"

class xNet;

// 网络客户端：封装 xNet，使用 RequestSnapshot 进行一次性发送。
class NetClient : public QObject
{
    Q_OBJECT
public:
    explicit NetClient(QObject *parent = nullptr);
    ~NetClient() override;

public slots:
    void send(const RequestSnapshot &snapshot);
    void stop(bool stop);

signals:
    void net2ui_tool_calls(const QString &payload);
    void net2ui_state(const QString &state_string, SIGNAL_STATE state = USUAL_SIGNAL);
    void net2ui_output(const QString &result, bool is_while = 1, QColor color = QColor(0, 0, 0));
    void net2ui_pushover();
    void net2ui_kv_tokens(int usedTokens);
    void net2ui_prompt_baseline(int promptTokens);
    void net2ui_slot_id(int slotId);
    void net2ui_reasoning_tokens(int count);
    void net2ui_speeds(double prompt_per_second, double predicted_per_second);
    void net2ui_turn_counters(int cacheTokens, int promptTokens, int predictedTokens);

private:
    void ensureNet();

    xNet *net_ = nullptr;
};
