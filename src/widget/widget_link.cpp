#include "ui_widget.h"
#include "widget.h"
#include "../utils/textparse.h"
#include <QDateTime>
#include <QUrl>
#include <QHostInfo>

namespace
{
QString normalizeLinkEndpoint(const QString &rawEndpoint)
{
    // Remove whitespace, infer scheme when missing, and drop a trailing /v1 to avoid duplicating the version segment
    QString clean = TextParse::removeAllWhitespace(rawEndpoint);
    QUrl url = QUrl::fromUserInput(clean);
    const QString host = url.host();
    const bool isLocal = isLoopbackHost(host);
    const QString scheme = url.scheme().toLower();
    if (scheme.isEmpty())
        url.setScheme(isLocal ? QStringLiteral("http") : QStringLiteral("https"));

    QString path = url.path();
    while (path.endsWith('/') && path.length() > 1)
        path.chop(1);
    if (path.toLower().endsWith(QStringLiteral("/v1")))
    {
        const int slashPos = path.lastIndexOf('/');
        QString basePath = path.left(slashPos);
        if (basePath.isEmpty())
            basePath = QStringLiteral("/");
        url.setPath(basePath);
    }
    return url.toString(QUrl::RemoveFragment);
}
} // namespace

//-------------------------------------------------------------------------
//----------------------------------链接相关--------------------------------
//-------------------------------------------------------------------------

// 应用api设置
void Widget::set_api()
{
    // 纯请求式：不再使用本地嵌入模型进程（xbot）
    is_load = false;  // 重置
    historypath = ""; // 重置
    linkProfile_ = LinkProfile::Api;
    controlAwaitingHello_ = false;

    // 获取设置值
    // Sanitize endpoint/key/model: strip whitespace, normalize scheme, strip trailing /v1
    QString clean_endpoint = normalizeLinkEndpoint(api_endpoint_LineEdit->text());
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
    setBaseWindowIcon(QIcon(":/logo/dark_logo.png"));

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
    recordAppendText(__idx, ui_DATES.date_prompt);
    lastSystemRecordIndex_ = __idx;
    // 重置对话消息并注入系统指令
    ui_messagesArray = QJsonArray();
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    ui_messagesArray.append(systemMessage);
    recordEntries_[__idx].msgIndex = 0;
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
        QString clean_endpoint = normalizeLinkEndpoint(api_endpoint_LineEdit->text());
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

//-------------------------------------------------------------------------
//---------------------------机体控制/镜像-----------------------------------
//-------------------------------------------------------------------------

bool Widget::isControllerActive() const
{
    return linkProfile_ == LinkProfile::Control && controlChannel_ && controlClient_.state == ControlChannel::ControllerState::Connected && !controlAwaitingHello_;
}

bool Widget::isHostControlled() const
{
    return controlChannel_ && controlHost_.active;
}

void Widget::setupControlChannel()
{
    if (controlChannel_) return;
    controlChannel_ = new ControlChannel(this);
    connect(controlChannel_, &ControlChannel::hostClientChanged, this, &Widget::handleControlHostClientChanged);
    connect(controlChannel_, &ControlChannel::hostCommandArrived, this, &Widget::handleControlHostCommand);
    connect(controlChannel_, &ControlChannel::controllerEventArrived, this, &Widget::handleControlControllerEvent);
    connect(controlChannel_, &ControlChannel::controllerStateChanged, this, &Widget::handleControlControllerState);
}

void Widget::setControlHostEnabled(bool enabled)
{
    if (enabled)
    {
        setupControlChannel();
        if (!controlChannel_)
        {
            reflash_state(jtr("control listen fail"), WRONG_SIGNAL);
            return;
        }
        if (controlHostAllowed_)
        {
            // Already hosting; nothing to change
            return;
        }
        if (controlChannel_->startHost(static_cast<quint16>(DEFAULT_CONTROL_PORT)))
        {
            controlHostAllowed_ = true;
            reflash_state(jtr("control listen ok").arg(QString::number(DEFAULT_CONTROL_PORT)), SIGNAL_SIGNAL);
        }
        else
        {
            controlHostAllowed_ = false;
            reflash_state(jtr("control listen fail"), WRONG_SIGNAL);
        }
        return;
    }

    if (!controlHostAllowed_)
    {
        return;
    }
    controlHostAllowed_ = false;
    if (!controlChannel_) return;
    if (isHostControlled())
    {
        QJsonObject bye;
        bye.insert(QStringLiteral("type"), QStringLiteral("released"));
        controlChannel_->sendToController(bye);
        controlHost_.active = false;
        controlHost_.peer.clear();
    }
    controlChannel_->stopHost();
}

QJsonObject Widget::buildControlSnapshot() const
{
    QJsonObject snap;
    if (ui && ui->output) snap.insert(QStringLiteral("output"), ui->output->toPlainText());
    if (ui && ui->state) snap.insert(QStringLiteral("state_log"), ui->state->toPlainText());

    int cap = slotCtxMax_ > 0 ? slotCtxMax_ : (ui_SETTINGS.nctx > 0 ? ui_SETTINGS.nctx : DEFAULT_NCTX);
    if (cap <= 0) cap = DEFAULT_NCTX;
    int used = qMax(0, kvUsed_);
    if (used > cap) used = cap;
    int percent = cap > 0 ? int(qRound(100.0 * double(used) / double(cap))) : 0;
    if (used > 0 && percent == 0) percent = 1;

    snap.insert(QStringLiteral("kv_used"), used);
    snap.insert(QStringLiteral("kv_cap"), cap);
    snap.insert(QStringLiteral("kv_percent"), percent);
    snap.insert(QStringLiteral("ui_state"), ui_state == CHAT_STATE ? QStringLiteral("chat") : QStringLiteral("complete"));
    snap.insert(QStringLiteral("is_run"), is_run);
    snap.insert(QStringLiteral("title"), windowTitle());
    snap.insert(QStringLiteral("mode"), ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local"));
    return snap;
}

void Widget::broadcastControlSnapshot()
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("snapshot"));
    payload.insert(QStringLiteral("snapshot"), buildControlSnapshot());
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlOutput(const QString &result, bool isStream, const QColor &color)
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("output"));
    payload.insert(QStringLiteral("text"), result);
    payload.insert(QStringLiteral("stream"), isStream);
    payload.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlState(const QString &stateString, SIGNAL_STATE level)
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("state_log"));
    payload.insert(QStringLiteral("text"), stateString);
    payload.insert(QStringLiteral("level"), static_cast<int>(level));
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlKv(int used, int cap, int percent)
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("kv"));
    payload.insert(QStringLiteral("used"), used);
    payload.insert(QStringLiteral("cap"), cap);
    payload.insert(QStringLiteral("percent"), percent);
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlUiPhase(const QString &phase)
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("ui_state"));
    payload.insert(QStringLiteral("phase"), phase);
    payload.insert(QStringLiteral("is_run"), is_run);
    payload.insert(QStringLiteral("state"), ui_state == CHAT_STATE ? QStringLiteral("chat") : QStringLiteral("complete"));
    controlChannel_->sendToController(payload);
}

void Widget::handleControlHostClientChanged(bool connected, const QString &reason)
{
    Q_UNUSED(reason);
    if (!connected)
    {
        controlHost_.active = false;
        controlHost_.peer.clear();
        broadcastControlState(jtr("control disconnected"), SIGNAL_SIGNAL);
        return;
    }
    controlHost_.peer = controlChannel_ ? controlChannel_->hostPeer() : QString();
}

void Widget::handleControlHostCommand(const QJsonObject &payload)
{
    const QString type = payload.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("hello"))
    {
        if (controlHost_.active)
        {
            QJsonObject busy;
            busy.insert(QStringLiteral("type"), QStringLiteral("reject"));
            busy.insert(QStringLiteral("reason"), QStringLiteral("busy"));
            controlChannel_->sendToController(busy);
            return;
        }
        controlHost_.active = true;
        controlHost_.peer = controlChannel_ ? controlChannel_->hostPeer() : QString();
        reflash_state(jtr("control connected").arg(controlHost_.peer), SIGNAL_SIGNAL);
        QJsonObject ack;
        ack.insert(QStringLiteral("type"), QStringLiteral("hello_ack"));
        ack.insert(QStringLiteral("snapshot"), buildControlSnapshot());
        ack.insert(QStringLiteral("peer"), QHostInfo::localHostName());
        controlChannel_->sendToController(ack);
        return;
    }
    if (!isHostControlled()) return;
    if (type != QStringLiteral("command")) return;
    const QString name = payload.value(QStringLiteral("name")).toString();
    if (name == QStringLiteral("release"))
    {
        controlHost_.active = false;
        reflash_state(jtr("control host exit"), SIGNAL_SIGNAL);
        QJsonObject bye;
        bye.insert(QStringLiteral("type"), QStringLiteral("released"));
        controlChannel_->sendToController(bye);
        return;
    }
    if (name == QStringLiteral("stop"))
    {
        if (is_run || turnActive_ || toolInvocationActive_)
        {
            reflash_state(jtr("control stop"), SIGNAL_SIGNAL);
            emit ui2net_stop(true);
            emit ui2tool_cancelActive();
        }
        return;
    }
    if (name == QStringLiteral("reset"))
    {
        reflash_state(jtr("control reset"), SIGNAL_SIGNAL);
        on_reset_clicked();
        broadcastControlSnapshot();
        return;
    }
    if (name == QStringLiteral("send"))
    {
        if (is_run || turnActive_)
        {
            QJsonObject warn;
            warn.insert(QStringLiteral("type"), QStringLiteral("state_log"));
            warn.insert(QStringLiteral("text"), jtr("control command blocked"));
            warn.insert(QStringLiteral("level"), static_cast<int>(WRONG_SIGNAL));
            controlChannel_->sendToController(warn);
            return;
        }
        const QString text = payload.value(QStringLiteral("text")).toString();
        if (text.trimmed().isEmpty())
        {
            QJsonObject warn;
            warn.insert(QStringLiteral("type"), QStringLiteral("state_log"));
            warn.insert(QStringLiteral("text"), jtr("control send missing"));
            warn.insert(QStringLiteral("level"), static_cast<int>(WRONG_SIGNAL));
            controlChannel_->sendToController(warn);
            return;
        }
        struct DraftBackup
        {
            QString text;
            QStringList attachments;
        };
        DraftBackup backup;
        if (ui && ui->input && ui->input->textEdit)
        {
            backup.text = ui->input->textEdit->toPlainText();
            QStringList paths = ui->input->imageFilePaths();
            paths.append(ui->input->documentFilePaths());
            paths.append(ui->input->wavFilePaths());
            backup.attachments = paths;
        }
        if (ui && ui->input && ui->input->textEdit)
        {
            ui->input->textEdit->setPlainText(text);
            ui->input->clearThumbnails();
            on_send_clicked();
            if (!backup.text.isEmpty() || !backup.attachments.isEmpty())
            {
                ui->input->textEdit->setPlainText(backup.text);
                ui->input->clearThumbnails();
                if (!backup.attachments.isEmpty())
                    ui->input->addFiles(backup.attachments);
            }
        }
        return;
    }
}

void Widget::handleControlControllerEvent(const QJsonObject &payload)
{
    const QString type = payload.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("reject"))
    {
        reflash_state(jtr("control busy"), WRONG_SIGNAL);
        releaseControl(false);
        return;
    }
    if (type == QStringLiteral("hello_ack") || type == QStringLiteral("snapshot"))
    {
        controlAwaitingHello_ = false;
        if (payload.contains(QStringLiteral("peer"))) controlClient_.peer = payload.value(QStringLiteral("peer")).toString();
        const QJsonObject snap = payload.value(QStringLiteral("snapshot")).toObject();
        applyControlSnapshot(snap);
        reflash_state(jtr("control snapshot applied"), SIGNAL_SIGNAL);
        applyControlUiLock();
        return;
    }
    if (!isControllerActive()) return;
    if (type == QStringLiteral("output"))
    {
        const QString text = payload.value(QStringLiteral("text")).toString();
        const bool stream = payload.value(QStringLiteral("stream")).toBool();
        QColor c(themeTextPrimary());
        const QString cstr = payload.value(QStringLiteral("color")).toString();
        if (!cstr.isEmpty())
        {
            QColor parsed(cstr);
            if (parsed.isValid()) c = parsed;
        }
        reflash_output(text, stream, c);
        return;
    }
    if (type == QStringLiteral("state_log"))
    {
        const QString text = payload.value(QStringLiteral("text")).toString();
        const int lv = payload.value(QStringLiteral("level")).toInt(static_cast<int>(USUAL_SIGNAL));
        reflash_state(text, static_cast<SIGNAL_STATE>(lv));
        return;
    }
    if (type == QStringLiteral("kv"))
    {
        kvUsed_ = payload.value(QStringLiteral("used")).toInt(kvUsed_);
        slotCtxMax_ = payload.value(QStringLiteral("cap")).toInt(slotCtxMax_);
        updateKvBarUi();
        return;
    }
    if (type == QStringLiteral("ui_state"))
    {
        controlClient_.remoteRunning = payload.value(QStringLiteral("is_run")).toBool(controlClient_.remoteRunning);
        const QString stateStr = payload.value(QStringLiteral("state")).toString();
        controlClient_.remoteUiState = (stateStr == QStringLiteral("complete")) ? COMPLETE_STATE : CHAT_STATE;
        applyControlUiLock();
        return;
    }
    if (type == QStringLiteral("released"))
    {
        reflash_state(jtr("control disconnected"), SIGNAL_SIGNAL);
        releaseControl(false);
        return;
    }
}

void Widget::handleControlControllerState(ControlChannel::ControllerState state, const QString &reason)
{
    Q_UNUSED(reason);
    controlClient_.state = state;
    if (state == ControlChannel::ControllerState::Connected)
    {
        controlAwaitingHello_ = true;
        QJsonObject hello;
        hello.insert(QStringLiteral("type"), QStringLiteral("hello"));
        hello.insert(QStringLiteral("token"), controlToken_);
        hello.insert(QStringLiteral("peer"), QHostInfo::localHostName());
        if (controlChannel_) controlChannel_->sendToHost(hello);
    }
    else if (state == ControlChannel::ControllerState::Idle)
    {
        if (linkProfile_ == LinkProfile::Control) releaseControl(false);
    }
}

void Widget::applyControlSnapshot(const QJsonObject &snap)
{
    if (!ui) return;
    if (ui->output) resetOutputDocument();
    if (ui->state) resetStateDocument();
    if (ui->output) ui->output->setPlainText(snap.value(QStringLiteral("output")).toString());
    if (ui->state) ui->state->setPlainText(snap.value(QStringLiteral("state_log")).toString());
    kvUsed_ = snap.value(QStringLiteral("kv_used")).toInt(kvUsed_);
    slotCtxMax_ = snap.value(QStringLiteral("kv_cap")).toInt(slotCtxMax_);
    updateKvBarUi();
    const QString stateStr = snap.value(QStringLiteral("ui_state")).toString();
    controlClient_.remoteUiState = (stateStr == QStringLiteral("complete")) ? COMPLETE_STATE : CHAT_STATE;
    controlClient_.remoteRunning = snap.value(QStringLiteral("is_run")).toBool(false);
    const QString title = snap.value(QStringLiteral("title")).toString();
    if (!title.isEmpty())
    {
        this->setWindowTitle(title);
        trayIcon->setToolTip(title);
    }
}

void Widget::applyControlUiLock()
{
    if (!ui) return;
    const QString dateLabel = jtr("date");
    const QString setLabel = jtr("set");
    const QString releaseLabel = jtr("control release");
    if (isControllerActive())
    {
        const bool canSend = !controlClient_.remoteRunning;
        ui->send->setEnabled(canSend);
        ui->reset->setEnabled(true);
        // Merge load/date/set into a single “解除” control to match controller UI spec
        if (ui->load) ui->load->setVisible(false);
        if (ui->set) ui->set->setVisible(false);
        if (ui->date)
        {
            ui->date->setVisible(true);
            ui->date->setText(releaseLabel);
            ui->date->setToolTip(releaseLabel);
        }
        ui->date->setEnabled(true);
        ui->set->setEnabled(true);
        ui->load->setEnabled(true);
        if (ui->input && ui->input->textEdit) ui->input->textEdit->setReadOnly(false);
    }
    else
    {
        if (ui->load) ui->load->setVisible(true);
        if (ui->set) ui->set->setVisible(true);
        if (ui->date)
        {
            ui->date->setVisible(true);
            ui->date->setText(dateLabel);
            ui->date->setToolTip(dateLabel);
        }
        if (ui->set)
        {
            ui->set->setText(setLabel);
            ui->set->setToolTip(jtr("set"));
        }
    }
}

void Widget::beginControlLink()
{
    const int tabIndex = (linkTabWidget) ? linkTabWidget->currentIndex() : 0;
    if (tabIndex == 0)
    {
        linkProfile_ = LinkProfile::Api;
        controlAwaitingHello_ = false;
        set_api();
        return;
    }
    linkProfile_ = LinkProfile::Control;
    const QString rawHost = control_host_LineEdit ? control_host_LineEdit->text() : QString();
    const QString host = TextParse::removeAllWhitespace(rawHost);
    const QString portText = control_port_LineEdit ? TextParse::removeAllWhitespace(control_port_LineEdit->text()) : QString();
    bool ok = false;
    const int port = portText.toInt(&ok);
    if (host.isEmpty())
    {
        reflash_state(jtr("control invalid host"), WRONG_SIGNAL);
        return;
    }
    if (!ok || port <= 0 || port > 65535)
    {
        reflash_state(jtr("control invalid port"), WRONG_SIGNAL);
        return;
    }
    setupControlChannel();
    if (!controlChannel_)
    {
        reflash_state(jtr("control listen fail"), WRONG_SIGNAL);
        return;
    }
    controlTargetHost_ = host;
    controlTargetPort_ = static_cast<quint16>(port);
    controlToken_ = control_token_LineEdit ? control_token_LineEdit->text() : QString();
    ui_mode = LINK_MODE;
    controlClient_.remoteRunning = false;
    controlClient_.remoteUiState = ui_state;
    controlAwaitingHello_ = true;
    if (controlChannel_) controlChannel_->connectToHost(controlTargetHost_, controlTargetPort_);
    reflash_state(jtr("control connect").arg(QStringLiteral("%1:%2").arg(controlTargetHost_).arg(controlTargetPort_)), SIGNAL_SIGNAL);
    ui_state_normal();
}

void Widget::releaseControl(bool notifyRemote)
{
    if (isControllerActive() && controlChannel_ && notifyRemote)
    {
        QJsonObject bye;
        bye.insert(QStringLiteral("type"), QStringLiteral("command"));
        bye.insert(QStringLiteral("name"), QStringLiteral("release"));
        controlChannel_->sendToHost(bye);
    }
    controlClient_.state = ControlChannel::ControllerState::Idle;
    controlClient_.peer.clear();
    controlClient_.remoteRunning = false;
    controlAwaitingHello_ = false;
    linkProfile_ = LinkProfile::Api;
    if (controlChannel_) controlChannel_->disconnectFromHost();
    ui_state_normal();
    applyControlUiLock();
}
