#include "service/net/net_client.h"

#include "xnet.h"

NetClient::NetClient(QObject *parent)
    : QObject(parent)
{
}

NetClient::~NetClient()
{
    if (net_)
    {
        net_->deleteLater();
        net_ = nullptr;
    }
}

void NetClient::ensureNet()
{
    if (net_)
        return;
    net_ = new xNet();
    net_->moveToThread(thread());

    // 将 xNet 信号转发到 NetClient
    connect(net_, &xNet::net2ui_tool_calls, this, &NetClient::net2ui_tool_calls);
    connect(net_, &xNet::net2ui_state, this, &NetClient::net2ui_state);
    connect(net_, &xNet::net2ui_output, this, &NetClient::net2ui_output);
    connect(net_, &xNet::net2ui_pushover, this, &NetClient::net2ui_pushover);
    connect(net_, &xNet::net2ui_kv_tokens, this, &NetClient::net2ui_kv_tokens);
    connect(net_, &xNet::net2ui_prompt_baseline, this, &NetClient::net2ui_prompt_baseline);
    connect(net_, &xNet::net2ui_slot_id, this, &NetClient::net2ui_slot_id);
    connect(net_, &xNet::net2ui_reasoning_tokens, this, &NetClient::net2ui_reasoning_tokens);
    connect(net_, &xNet::net2ui_speeds, this, &NetClient::net2ui_speeds);
    connect(net_, &xNet::net2ui_turn_counters, this, &NetClient::net2ui_turn_counters);
}

void NetClient::send(const RequestSnapshot &snapshot)
{
    ensureNet();
    net_->recv_apis(snapshot.apis);
    net_->recv_data(snapshot.endpoint);
    net_->recv_language(snapshot.languageFlag);
    net_->recv_turn(snapshot.turnId);
    net_->wordsObj = snapshot.wordsObj; // 语言资源仅在主线程更新后快照传入
    net_->run();
}

void NetClient::stop(bool stop)
{
    ensureNet();
    net_->recv_stop(stop);
}
