#include "ui_widget.h"
#include "widget.h"
#include <QDateTime>
#include <QUrl>

//-------------------------------------------------------------------------
//----------------------------------链接相关--------------------------------
//-------------------------------------------------------------------------

// 应用api设置
void Widget::set_api()
{
    // 纯请求式：不再使用本地嵌入模型进程（xbot）
    is_load = false;  // 重置
    historypath = ""; // 重置

    // 获取设置值
    // Sanitize endpoint/key/model: strip all whitespace to avoid mistakes
    auto sanitize = [](const QString &s)
    { QString out = s; out.replace(QRegularExpression(" +"), ""); return out; };
    QString clean_endpoint = sanitize(api_endpoint_LineEdit->text());
    // Normalize scheme: prefer https for public hosts; http for localhost/LAN when scheme missing
    {
        QUrl u = QUrl::fromUserInput(clean_endpoint);
        QString host = u.host().toLower();
        QString scheme = u.scheme().toLower();
        const bool isLocal = host.isEmpty() || host == "localhost" || host == "127.0.0.1" || host.startsWith("192.") || host.startsWith("10.") || host.startsWith("172.");
        if (scheme.isEmpty())
        {
            u.setScheme(isLocal ? "http" : "https");
        }
        else if (scheme == "http" && !isLocal)
        {
            u.setScheme("https");
        }
        clean_endpoint = u.toString(QUrl::RemoveFragment);
    }
    const QString clean_key = sanitize(api_key_LineEdit->text());
    const QString clean_model = sanitize(api_model_LineEdit->text());
    // Reflect cleaned values in UI
    api_endpoint_LineEdit->setText(clean_endpoint);
    api_key_LineEdit->setText(clean_key);
    api_model_LineEdit->setText(clean_model);
    apis.api_endpoint = clean_endpoint;
    apis.api_key = clean_key;
    apis.api_model = clean_model;

    // 切换为链接模式
    ui_mode = LINK_MODE; // 按照链接模式的行为来
    // 进入链接模式后：
    // 1) 终止当前的流式请求（若有）
    emit ui2net_stop(true);
    // 2) 停止本地 llama.cpp server 后端，避免占用资源/混淆来源
    if (serverManager && serverManager->isRunning())
    {
        serverManager->stop();
        // reflash_state("ui:backend stopped", SIGNAL_SIGNAL);
    }
    reflash_state("ui:" + jtr("eva link"), EVA_SIGNAL);
    if (ui_state == CHAT_STATE)
    {
        current_api = apis.api_endpoint + apis.api_chat_endpoint;
    }
    else
    {
        current_api = apis.api_endpoint + apis.api_completion_endpoint;
    }
    EVA_title = jtr("current api") + " " + current_api;
    reflash_state("ui:" + EVA_title, USUAL_SIGNAL);
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    EVA_icon = QIcon(":/logo/dark_logo.png");
    QApplication::setWindowIcon(EVA_icon); // 设置应用程序图标
    trayIcon->setIcon(EVA_icon);           // 设置系统托盘图标

    emit ui2net_apis(apis);
    ui->output->clear();
    // Reset record bar to avoid residual nodes when switching to LINK mode
    recordClear();
    // Create record BEFORE printing header/content so docFrom anchors at header area
    int __idx = recordCreate(RecordRole::System);
    appendRoleHeader(QStringLiteral("system"));
    reflash_output(ui_DATES.date_prompt, 0, NORMAL_BLACK);
    {
        recordAppendText(__idx, ui_DATES.date_prompt);
        if (!ui_messagesArray.isEmpty())
        {
            int mi = ui_messagesArray.size() - 1;
            recordEntries_[__idx].msgIndex = mi;
        }
    }
    // 构造系统指令
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    ui_messagesArray.append(systemMessage);
    // start a new persistent history session in LINK mode
    if (history_)
    {
        SessionMeta meta;
        meta.id = QString::number(QDateTime::currentMSecsSinceEpoch());
        meta.title = "";
        meta.endpoint = current_api;
        meta.model = apis.api_model;
        meta.system = ui_DATES.date_prompt;
        meta.n_ctx = ui_SETTINGS.nctx;
        meta.slot_id = -1;
        meta.startedAt = QDateTime::currentDateTime();
        history_->begin(meta);
        history_->appendMessage(systemMessage);
        currentSlotId_ = -1;
    }
    ui_state_normal();
    auto_save_user();
}

// 链接模式下工具返回结果时延迟发送
void Widget::tool_testhandleTimeout()
{
    ENDPOINT_DATA data;
    data.date_prompt = ui_DATES.date_prompt;
    data.stopwords = ui_DATES.extra_stop_words;
    if (ui_state == COMPLETE_STATE)
    {
        data.is_complete_state = true;
    }
    else
    {
        data.is_complete_state = false;
    }
    data.temp = ui_SETTINGS.temp;
    data.repeat = ui_SETTINGS.repeat;
    data.top_k = ui_SETTINGS.top_k;
    data.top_p = ui_SETTINGS.hid_top_p;
    data.n_predict = ui_SETTINGS.hid_npredict;
    data.messagesArray = ui_messagesArray;
    data.id_slot = currentSlotId_;

    emit ui2net_data(data);
    emit ui2net_push();
}

void Widget::send_testhandleTimeout()
{
    on_send_clicked();
}

// 链接模式切换时某些控件可见状态
void Widget::change_api_dialog(bool enable)
{
    // 链接模式隐藏“后端设置”整组；保留“采样设置”和“状态设置”
    // enable==0 -> LINK_MODE: backend controls hidden; enable==1 -> LOCAL_MODE: show backend controls
    if (settings_ui && settings_ui->backend_box)
    {
        settings_ui->backend_box->setVisible(enable);
    }
}
