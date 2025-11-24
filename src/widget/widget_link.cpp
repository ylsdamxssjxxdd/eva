#include "ui_widget.h"
#include "widget.h"
#include "../utils/textparse.h"
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
    QString clean_endpoint = TextParse::removeAllWhitespace(api_endpoint_LineEdit->text());
    // Normalize scheme: prefer https for public hosts; http for localhost/LAN when scheme missing
    {
        QUrl u = QUrl::fromUserInput(clean_endpoint);
        const QString host = u.host();
        QString scheme = u.scheme().toLower();
        const bool isLocal = isLoopbackHost(host);
        if (scheme.isEmpty())
        {
            u.setScheme(isLocal ? "http" : "https");
        }
        clean_endpoint = u.toString(QUrl::RemoveFragment);
    }
    const QString clean_key = TextParse::removeAllWhitespace(api_key_LineEdit->text());
    const QString clean_model = TextParse::removeAllWhitespace(api_model_LineEdit->text());
    // Reflect cleaned values in UI
    api_endpoint_LineEdit->setText(clean_endpoint);
    api_key_LineEdit->setText(clean_key);
    api_model_LineEdit->setText(clean_model);
    apis.api_endpoint = clean_endpoint;
    apis.api_key = clean_key;
    apis.api_model = clean_model;
    apis.is_local_backend = false;

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
    // Broadcast to Expend (evaluation tab) as well
    emit ui2expend_apis(apis);
    emit ui2expend_mode(ui_mode);
    // Reset LINK-mode memory/state since endpoint/key/model changed
    // Reset KV counters when switching to LINK mode to avoid leaking local state
    kvTokensTurn_ = 0;
    kvPromptTokensTurn_ = 0;
    kvUsed_ = 0;
    kvUsedBeforeTurn_ = 0;
    kvStreamedTurn_ = 0;
    lastReasoningTokens_ = 0;
    turnActive_ = false;
    sawPromptPast_ = false;
    sawFinalPast_ = false;
    currentSlotId_ = -1;
    slotCtxMax_ = 0;
    enforcePredictLimit();
    updateKvBarUi();
    fetchRemoteContextLimit();
    flushPendingStream();
    ui->output->clear();
    // Reset record bar to avoid residual nodes when switching to LINK mode
    recordClear();
    // Create record BEFORE printing header/content so docFrom anchors at header area
    int __idx = recordCreate(RecordRole::System);
    appendRoleHeader(QStringLiteral("system"));
    reflash_output(ui_DATES.date_prompt, 0, themeTextPrimary());
    {
        recordAppendText(__idx, ui_DATES.date_prompt);
        if (!ui_messagesArray.isEmpty())
        {
            int mi = ui_messagesArray.size() - 1;
            recordEntries_[__idx].msgIndex = mi;
        }
    }
    // 重置对话消息并注入系统指令
    ui_messagesArray = QJsonArray();
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
    // Ensure latest LINK apis before pushing (users may edit endpoint/key/model after linking)
    if (ui_mode == LINK_MODE)
    {
        QString clean_endpoint = TextParse::removeAllWhitespace(api_endpoint_LineEdit->text());
        // Normalize scheme for remote hosts
        {
            QUrl u = QUrl::fromUserInput(clean_endpoint);
            const QString host = u.host();
            QString scheme = u.scheme().toLower();
            const bool isLocal = isLoopbackHost(host);
            if (scheme.isEmpty())
                u.setScheme(isLocal ? "http" : "https");
            clean_endpoint = u.toString(QUrl::RemoveFragment);
        }
        const QString clean_key = TextParse::removeAllWhitespace(api_key_LineEdit->text());
        const QString clean_model = TextParse::removeAllWhitespace(api_model_LineEdit->text());
        if (clean_endpoint != apis.api_endpoint || clean_key != apis.api_key || clean_model != apis.api_model)
        {
            apis.api_endpoint = clean_endpoint;
            apis.api_key = clean_key;
            apis.api_model = clean_model;
            apis.is_local_backend = false;
            emit ui2net_apis(apis);
        }
    }
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
        if (settings_dialog && settings_dialog->isVisible())
        {
            applySettingsDialogSizing();
        }
    }
}

// Probe remote /v1/models to determine max context for current model (LINK mode)
void Widget::fetchRemoteContextLimit()
{
    if (ui_mode != LINK_MODE) return;
    // Build URL: base endpoint + /v1/models
    QUrl base = QUrl::fromUserInput(apis.api_endpoint);
    if (!base.isValid()) return;
    const bool isLocalEndpoint = (ui_mode == LOCAL_MODE);
    QUrl url(base);
    QString path = url.path();
    if (!path.endsWith('/')) path += '/';
    path += QLatin1String("v1/models");
    url.setPath(path);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!apis.api_key.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + apis.api_key.toUtf8());

    auto *nam = new QNetworkAccessManager(this);
    QNetworkReply *rp = nam->get(req);
    connect(rp, &QNetworkReply::finished, this, [this, nam, rp, isLocalEndpoint]()
            {
        rp->deleteLater();
        nam->deleteLater();
        if (rp->error() != QNetworkReply::NoError)
        {
            // leave slotCtxMax_ unchanged on error
            if (isLocalEndpoint) fetchPropsContextLimit();
            return;
        }
        const QByteArray body = rp->readAll();
        QJsonParseError perr{};
        QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error != QJsonParseError::NoError)
        {
            if (isLocalEndpoint) fetchPropsContextLimit();
            return;
        }
        int maxCtx = -1;
        auto tryPick = [&](const QJsonObject &o) {
            // Try common fields from various providers
            const char *keys[] = {"max_model_len","context_length","max_input_tokens","max_context_length","max_input_length","prompt_token_limit","input_token_limit"};
            for (auto k : keys) {
                if (o.contains(k)) { int v = o.value(k).toInt(-1); if (v > 0) return v; }
            }
            // Also look under nested objects
            if (o.contains("capabilities") && o.value("capabilities").isObject()) {
                const QJsonObject cap = o.value("capabilities").toObject();
                const int v = cap.value("context_length").toInt(-1); if (v > 0) return v;
            }
            if (o.contains("limits") && o.value("limits").isObject()) {
                const QJsonObject lim = o.value("limits").toObject();
                const int v = lim.value("max_input_tokens").toInt(-1); if (v > 0) return v;
            }
            return -1;
        };
        auto matchModel = [&](const QString &id) {
            if (id == apis.api_model) return true;
            // accept provider-prefixed ids like provider:model
            return id.endsWith(":" + apis.api_model) || id.endsWith("/" + apis.api_model);
        };
        if (doc.isObject())
        {
            const QJsonObject root = doc.object();
            if (root.contains("data") && root.value("data").isArray())
            {
                const QJsonArray arr = root.value("data").toArray();
                for (const auto &v : arr)
                {
                    if (!v.isObject()) continue;
                    const QJsonObject m = v.toObject();
                    const QString mid = m.value("id").toString();
                    if (!mid.isEmpty() && matchModel(mid))
                    {
                        maxCtx = tryPick(m);
                        if (maxCtx > 0) break;
                    }
                }
            }
            // Some providers might return a single object for the model
            if (maxCtx <= 0)
            {
                maxCtx = tryPick(root);
            }
        }
        if (maxCtx > 0)
        {
            slotCtxMax_ = maxCtx;
            enforcePredictLimit();
            updateKvBarUi();
            // Notify Expend (evaluation tab) to refresh displayed n_ctx
            SETTINGS snap = ui_SETTINGS;
            if (ui_mode == LINK_MODE && slotCtxMax_ > 0) snap.nctx = slotCtxMax_;
            emit ui2expend_settings(snap);
        }
        else
        {
            // Fallback: try llama.cpp tools/server props API
            if (isLocalEndpoint) fetchPropsContextLimit();
        } });
}

// Fallback: GET /props from llama.cpp tools/server to obtain global n_ctx
void Widget::fetchPropsContextLimit()
{
    if (ui_mode != LOCAL_MODE) return;
    QUrl base = QUrl::fromUserInput(apis.api_endpoint);
    if (!base.isValid()) return;
    QUrl url(base);
    QString path = url.path();
    if (!path.endsWith('/')) path += '/';
    path += QLatin1String("props");
    url.setPath(path);

    QNetworkRequest req(url);
    if (!apis.api_key.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + apis.api_key.toUtf8());

    auto *nam = new QNetworkAccessManager(this);
    QNetworkReply *rp = nam->get(req);
    connect(rp, &QNetworkReply::finished, this, [this, nam, rp]()
            {
        rp->deleteLater();
        nam->deleteLater();
        if (rp->error() != QNetworkReply::NoError) return;
        const QByteArray body = rp->readAll();
        QJsonParseError perr{};
        QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error != QJsonParseError::NoError) return;
        if (!doc.isObject()) return;
        const QJsonObject root = doc.object();
        int nctx = -1;
        if (root.contains("default_generation_settings") && root.value("default_generation_settings").isObject())
        {
            const QJsonObject dgs = root.value("default_generation_settings").toObject();
            nctx = dgs.value("n_ctx").toInt(-1);
            if (nctx <= 0 && dgs.contains("params") && dgs.value("params").isObject())
            {
                const QJsonObject params = dgs.value("params").toObject();
                nctx = params.value("n_ctx").toInt(nctx);
            }
        }
        if (nctx > 0)
        {
            slotCtxMax_ = nctx;
            enforcePredictLimit();
            updateKvBarUi();
            reflash_state(QString("net:ctx via /props = %1").arg(nctx), SIGNAL_SIGNAL);
            // Notify Expend to refresh displayed n_ctx
            SETTINGS snap = ui_SETTINGS;
            if (ui_mode == LINK_MODE && slotCtxMax_ > 0) snap.nctx = slotCtxMax_;
            emit ui2expend_settings(snap);
        } });
}
