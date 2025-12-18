#include "ui_widget.h"
#include "widget.h"
#include "../utils/textparse.h"
#include "../utils/flowtracer.h"
#include "../utils/openai_compat.h"
#include <QDateTime>
#include <QUrl>
#include <QHostInfo>
#include <QFileInfo>
#include <QTextCharFormat>

namespace
{
QString previewForLog(const QString &text, int limit = 120)
{
    QString trimmed = text;
    trimmed.replace("\n", "\\n");
    trimmed.replace("\r", "\\r");
    if (trimmed.size() > limit)
    {
        trimmed = trimmed.left(limit) + QStringLiteral("…");
    }
    return trimmed;
}

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
    // Collapse连续的斜杠，避免用户多输“////”导致路径异常
    QString collapsed;
    collapsed.reserve(path.size());
    bool prevSlash = false;
    for (const QChar ch : path)
    {
        if (ch == QLatin1Char('/'))
        {
            if (!prevSlash) collapsed.append(ch);
            prevSlash = true;
        }
        else
        {
            collapsed.append(ch);
            prevSlash = false;
        }
    }
    path = collapsed;
    while (path.endsWith('/') && path.length() > 1) path.chop(1);
    const QString lowerPath = path.toLower();
    if (lowerPath.endsWith(QStringLiteral("/v1")))
    {
        const int slashPos = path.lastIndexOf('/');
        QString basePath = path.left(slashPos);
        if (basePath.isEmpty())
            basePath = QStringLiteral("/");
        url.setPath(basePath);
    }
    else
    {
        // 仅去掉尾部斜杠并应用规整后的路径
        if (path.isEmpty()) path = QStringLiteral("/");
        url.setPath(path);
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
    // 根据 base url 自动选择 OpenAI 兼容接口路径风格：
    // - 默认（OpenAI/llama.cpp 等）：base 不含版本号，接口固定为 /v1/...
    // - 火山方舟 Ark：base 自带 /api/v3，接口直接使用 /chat/completions、/models 等
    //   若仍然额外追加 /v1，会被拼成 /api/v3/v1/... 从而请求失败
    {
        const QUrl baseUrl = QUrl::fromUserInput(apis.api_endpoint);
        apis.api_chat_endpoint = OpenAiCompat::chatCompletionsPath(baseUrl);
        apis.api_completion_endpoint = OpenAiCompat::completionsPath(baseUrl);
    }

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
    {
        // 切换链接模式后立即同步评估页上下文上限（未探测到时标记为未知）
        SETTINGS snap = ui_SETTINGS;
        if (ui_mode == LINK_MODE) snap.nctx = (slotCtxMax_ > 0 ? slotCtxMax_ : 0);
        emit ui2expend_settings(snap);
    }
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
    reflash_output_tool_highlight(ui_DATES.date_prompt, themeTextPrimary());
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
            // 允许用户在不重新“装载/确认”的情况下修改端点：
            // 这里需要同步更新各厂商的 OpenAI 兼容路径风格，避免 Ark(/api/v3) 被误拼为 /api/v3/v1/...
            {
                const QUrl baseUrl = QUrl::fromUserInput(apis.api_endpoint);
                apis.api_chat_endpoint = OpenAiCompat::chatCompletionsPath(baseUrl);
                apis.api_completion_endpoint = OpenAiCompat::completionsPath(baseUrl);
            }
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
    QUrl base = QUrl::fromUserInput(apis.api_endpoint);
    if (!base.isValid()) return;
    // 无论是否本机，优先读取 /props 获取实际运行时 n_ctx；若缺失则回退 /v1/models
    fetchPropsContextLimit(true, true);
}

void Widget::fetchModelsContextLimit(bool isLocalEndpoint)
{
    Q_UNUSED(isLocalEndpoint);
    if (ui_mode != LINK_MODE) return;
    QUrl base = QUrl::fromUserInput(apis.api_endpoint);
    if (!base.isValid()) return;
    // /v1/models 是最常见的 OpenAI 兼容路径；但火山方舟 Ark 的 base 已经包含 /api/v3，
    // 因此 models 路径应为 /models（最终拼成 /api/v3/models）
    const QUrl url = OpenAiCompat::joinPath(base, OpenAiCompat::modelsPath(base));

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!apis.api_key.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + apis.api_key.toUtf8());

    auto *nam = new QNetworkAccessManager(this);
    QNetworkReply *rp = nam->get(req);
    connect(rp, &QNetworkReply::finished, this, [this, nam, rp]()
            {
        rp->deleteLater();
        nam->deleteLater();
        if (rp->error() != QNetworkReply::NoError)
        {
            return;
        }
        const QByteArray body = rp->readAll();
        QJsonParseError perr{};
        QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        if (perr.error != QJsonParseError::NoError)
        {
            return;
        }
        int maxCtx = -1;
        int firstCtx = -1;
        QString firstAlias;
        QStringList dbgLines;
        auto tryPick = [&](const QJsonObject &o) {
            // Try common fields from various providers
            const char *keys[] = {"max_model_len","context_length","max_input_tokens","max_context_length","max_input_length","prompt_token_limit","input_token_limit"};
            for (auto k : keys) {
                if (o.contains(k)) { int v = o.value(k).toInt(-1); if (v > 0) return v; }
            }
            // Nested meta fields (llama.cpp returns meta.n_ctx_train)
            if (o.contains("meta") && o.value("meta").isObject()) {
                const QJsonObject meta = o.value("meta").toObject();
                const char *mkeys[] = {"n_ctx_train","n_ctx","context_length","max_context_length"};
                for (auto k : mkeys) {
                    int v = meta.value(k).toInt(-1);
                    if (v > 0) return v;
                }
            }
            // Some providers expose details.context_length
            if (o.contains("details") && o.value("details").isObject()) {
                const QJsonObject det = o.value("details").toObject();
                int v = det.value("context_length").toInt(-1);
                if (v > 0) return v;
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
        auto baseName = [](QString s) {
            QFileInfo fi(s);
            QString name = fi.fileName();
            if (name.isEmpty()) name = s;
            if (name.endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) name.chop(5);
            return name;
        };
        auto matchModel = [&](const QString &idRaw) {
            if (apis.api_model.isEmpty()) return false;
            QString id = idRaw;
            if (id.isEmpty()) return false;
            if (id == apis.api_model) return true;
            // accept provider-prefixed ids like provider:model
            if (id.endsWith(":" + apis.api_model) || id.endsWith("/" + apis.api_model)) return true;
            const QString idBase = baseName(id);
            const QString targetBase = baseName(apis.api_model);
            if (!idBase.isEmpty() && idBase == targetBase) return true;
            if (id.contains(targetBase) || targetBase.contains(idBase)) return true;
            return false;
        };
        auto updateAlias = [&](const QString &alias) {
            if (!alias.isEmpty() && alias != apis.api_model) {
                applyDiscoveredAlias(alias, QStringLiteral("v1/models"));
            }
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
                    const QString altModel = m.value("model").toString();
                    const QString name = m.value("name").toString();
                    const int ctxCandidate = tryPick(m);
                    if (firstAlias.isEmpty())
                    {
                        firstAlias = !mid.isEmpty() ? mid : (!altModel.isEmpty() ? altModel : name);
                        firstCtx = ctxCandidate;
                    }
                    dbgLines << QStringLiteral("[data] id=%1 model=%2 name=%3 ctx=%4")
                                    .arg(mid, altModel, name, QString::number(ctxCandidate));
                    if ((!mid.isEmpty() && matchModel(mid)) || (!altModel.isEmpty() && matchModel(altModel)) || (!name.isEmpty() && matchModel(name)))
                    {
                        const QString alias = !mid.isEmpty() ? mid : (!altModel.isEmpty() ? altModel : name);
                        updateAlias(alias);
                        maxCtx = ctxCandidate;
                        if (maxCtx > 0) break;
                    }
                }
            }
            // Some providers might return "models" array (llama-server legacy) or a single object
            if (maxCtx <= 0 && root.contains("models") && root.value("models").isArray())
            {
                const QJsonArray arr = root.value("models").toArray();
                for (const auto &v : arr)
                {
                    if (!v.isObject()) continue;
                    const QJsonObject m = v.toObject();
                    const QString mid = m.value("model").toString();
                    const QString name = m.value("name").toString();
                    const int ctxCandidate = tryPick(m);
                    if (firstAlias.isEmpty())
                    {
                        firstAlias = !mid.isEmpty() ? mid : name;
                        firstCtx = ctxCandidate;
                    }
                    dbgLines << QStringLiteral("[models] id=%1 name=%2 ctx=%3")
                                    .arg(mid, name, QString::number(ctxCandidate));
                    if ((!mid.isEmpty() && matchModel(mid)) || (!name.isEmpty() && matchModel(name)))
                    {
                        const QString alias = !mid.isEmpty() ? mid : name;
                        updateAlias(alias);
                        maxCtx = ctxCandidate;
                        if (maxCtx > 0) break;
                    }
                }
            }
            // Some providers might return a single object for the model
            if (maxCtx <= 0)
            {
                maxCtx = tryPick(root);
                dbgLines << QStringLiteral("[root] ctx=%1").arg(maxCtx);
                if (firstAlias.isEmpty() && root.contains("id")) firstAlias = root.value("id").toString();
                if (firstAlias.isEmpty() && root.contains("model")) firstAlias = root.value("model").toString();
                if (firstAlias.isEmpty() && root.contains("name")) firstAlias = root.value("name").toString();
                if (firstCtx <= 0) firstCtx = maxCtx;
            }
        }
        // Fallback: if没有匹配到但只有一个候选，直接采用首个模型的别名与上下文
        if (maxCtx <= 0 && !firstAlias.isEmpty() && firstCtx > 0)
        {
            updateAlias(firstAlias);
            maxCtx = firstCtx;
            dbgLines << QStringLiteral("[fallback-first] alias=%1 ctx=%2").arg(firstAlias).arg(firstCtx);
        }
        if (maxCtx > 0)
        {
            slotCtxMax_ = maxCtx;
            enforcePredictLimit();
            updateKvBarUi();
            // Notify Expend (evaluation tab) to refresh displayed n_ctx
            SETTINGS snap = ui_SETTINGS;
            if (ui_mode == LINK_MODE) snap.nctx = (slotCtxMax_ > 0 ? slotCtxMax_ : 0);
            emit ui2expend_settings(snap);
            const QString log = QStringLiteral("net:n_ctx via /v1/models = %1").arg(maxCtx);
            FlowTracer::log(FlowChannel::Net, dbgLines.join(QStringLiteral(" | ")), 0);
            FlowTracer::log(FlowChannel::Net, log, 0);
        }
    });
}

// Fallback: GET /props from llama.cpp tools/server to obtain runtime n_ctx
void Widget::fetchPropsContextLimit(bool allowLinkMode, bool fallbackModels)
{
    if (ui_mode != LOCAL_MODE && !allowLinkMode) return;
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
    connect(rp, &QNetworkReply::finished, this, [this, nam, rp, allowLinkMode, fallbackModels]()
            {
        rp->deleteLater();
        nam->deleteLater();
        bool gotCtx = false;
        auto fallback = [&]() {
            if (fallbackModels && !gotCtx)
            {
                fetchModelsContextLimit(true);
            }
        };
        if (rp->error() == QNetworkReply::NoError)
        {
            const QByteArray body = rp->readAll();
            QJsonParseError perr{};
            QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
            if (perr.error == QJsonParseError::NoError && doc.isObject())
            {
                const QJsonObject root = doc.object();
                const QString alias = root.value(QStringLiteral("model_alias")).toString();
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
                if (!alias.isEmpty() && alias != apis.api_model)
                {
                    applyDiscoveredAlias(alias, QStringLiteral("props"));
                }
                if (nctx > 0)
                {
                    gotCtx = true;
                    applyDiscoveredContext(nctx, QStringLiteral("props"));
                }
                else
                {
                    // FlowTracer::log(FlowChannel::Net,
                    //                 QStringLiteral("net:/props missing n_ctx, body=%1").arg(QString::fromUtf8(body)),
                    //                 0);
                }
            }
            else
            {
                // FlowTracer::log(FlowChannel::Net,
                //                 QStringLiteral("net:/props parse error=%1 body=%2")
                //                     .arg(perr.errorString(), QString::fromUtf8(body)),
                //                 0);
            }
        }
        else
        {
            // FlowTracer::log(FlowChannel::Net,
            //                 QStringLiteral("net:/props http fail=%1").arg(rp->error()),
            //                 0);
        }
        fallback();
    });
}

void Widget::applyDiscoveredAlias(const QString &alias, const QString &sourceTag)
{
    if (alias.isEmpty() || alias == apis.api_model) return;
    apis.api_model = alias;
    api_model_LineEdit->setText(alias);
    emit ui2net_apis(apis);
    emit ui2expend_apis(apis);
    FlowTracer::log(FlowChannel::Net,
                    QStringLiteral("net:model via %1 = %2").arg(sourceTag, alias),
                    0);
}

void Widget::applyDiscoveredContext(int nctx, const QString &sourceTag)
{
    if (nctx <= 0) return;
    slotCtxMax_ = nctx;
    enforcePredictLimit();
    updateKvBarUi();
    // Notify Expend (evaluation tab) with latest effective n_ctx
    SETTINGS snap = ui_SETTINGS;
    if (ui_mode == LINK_MODE) snap.nctx = (slotCtxMax_ > 0 ? slotCtxMax_ : 0);
    emit ui2expend_settings(snap);
    FlowTracer::log(FlowChannel::Net,
                    QStringLiteral("net:n_ctx via %1 = %2").arg(sourceTag).arg(nctx),
                    0);
}

int Widget::resolvedContextLimitForUi() const
{
    // 优先使用从 /props 或 /v1/models 探测到的有效 n_ctx；LINK 模式下未探测到时返回 0 表示未知
    if (slotCtxMax_ > 0) return slotCtxMax_;
    if (ui_mode == LINK_MODE) return 0;
    if (ui_SETTINGS.nctx > 0) return ui_SETTINGS.nctx;
    return DEFAULT_NCTX;
}

QString Widget::resolvedContextLabelForUi() const
{
    const int cap = resolvedContextLimitForUi();
    return (cap > 0) ? QString::number(cap) : QStringLiteral("未知");
}

QString Widget::resolvedModelLabelForUi() const
{
    // LINK 模式下优先展示用户填写/探测到的模型名，否则用“未知”占位，避免误用本地默认模型名
    if (ui_mode == LINK_MODE)
    {
        const QString linkModel = apis.api_model.trimmed();
        if (!linkModel.isEmpty()) return linkModel;
        return QStringLiteral("未知");
    }
    QString modelLabel = QFileInfo(ui_SETTINGS.modelpath).fileName();
    if (modelLabel.isEmpty()) modelLabel = jtr("unknown model");
    return modelLabel;
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

    const int cap = resolvedContextLimitForUi();
    const bool capKnown = cap > 0;
    int used = qMax(0, kvUsed_);
    if (capKnown && used > cap) used = cap;
    int percent = (capKnown && cap > 0) ? int(qRound(100.0 * double(used) / double(cap))) : 0;
    if (capKnown && used > 0 && percent == 0) percent = 1;

    const QString modelLabel = resolvedModelLabelForUi();

    snap.insert(QStringLiteral("kv_used"), used);
    snap.insert(QStringLiteral("kv_cap"), cap);
    snap.insert(QStringLiteral("kv_percent"), percent);
    snap.insert(QStringLiteral("ui_state"), ui_state == CHAT_STATE ? QStringLiteral("chat") : QStringLiteral("complete"));
    snap.insert(QStringLiteral("is_run"), is_run);
    snap.insert(QStringLiteral("title"), windowTitle());
    snap.insert(QStringLiteral("mode"), ui_mode == LINK_MODE ? QStringLiteral("link") : QStringLiteral("local"));
    snap.insert(QStringLiteral("model_name"), modelLabel);
    snap.insert(QStringLiteral("endpoint"), current_api);
    snap.insert(QStringLiteral("monitor"), buildControlMonitor());
    snap.insert(QStringLiteral("records"), buildControlRecords());
    return snap;
}

QJsonObject Widget::buildControlMonitor() const
{
    QJsonObject mon;
    if (ui && ui->cpu_bar)
    {
        mon.insert(QStringLiteral("cpu"), ui->cpu_bar->value());
        mon.insert(QStringLiteral("cpu2"), ui->cpu_bar->m_secondValue);
    }
    if (ui && ui->mem_bar)
    {
        mon.insert(QStringLiteral("mem"), ui->mem_bar->value());
        mon.insert(QStringLiteral("mem2"), ui->mem_bar->m_secondValue);
    }
    if (ui && ui->vram_bar)
    {
        mon.insert(QStringLiteral("vram"), ui->vram_bar->value());
        mon.insert(QStringLiteral("vram2"), ui->vram_bar->m_secondValue);
    }
    if (ui && ui->vcore_bar)
    {
        mon.insert(QStringLiteral("vcore"), ui->vcore_bar->value());
    }
    return mon;
}

QJsonArray Widget::buildControlRecords() const
{
    QJsonArray arr;
    for (const RecordEntry &e : recordEntries_)
    {
        QJsonObject obj;
        obj.insert(QStringLiteral("role"), static_cast<int>(e.role));
        obj.insert(QStringLiteral("text"), e.text);
        if (e.role == RecordRole::Tool && !e.toolName.isEmpty())
        {
            obj.insert(QStringLiteral("tool"), e.toolName);
        }
        if (e.msgIndex >= 0)
        {
            obj.insert(QStringLiteral("msg_index"), e.msgIndex);
        }
        arr.append(obj);
    }
    return arr;
}

void Widget::broadcastControlSnapshot()
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("snapshot"));
    payload.insert(QStringLiteral("snapshot"), buildControlSnapshot());
    controlChannel_->sendToController(payload);
    const int recordCount = recordEntries_.size();
    const int outputLen = (ui && ui->output) ? ui->output->toPlainText().size() : 0;
    const int stateLen = (ui && ui->state) ? ui->state->toPlainText().size() : 0;
    FlowTracer::log(FlowChannel::Session,
                    QStringLiteral("[control] host snapshot push records=%1 output=%2 state=%3")
                        .arg(recordCount)
                        .arg(outputLen)
                        .arg(stateLen),
                    activeTurnId_);
}

void Widget::broadcastControlMonitor()
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("monitor"));
    payload.insert(QStringLiteral("monitor"), buildControlMonitor());
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlRecordClear()
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("record_clear"));
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlRecordAdd(RecordRole role, const QString &toolName)
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("record_add"));
    payload.insert(QStringLiteral("role"), static_cast<int>(role));
    if (!toolName.isEmpty()) payload.insert(QStringLiteral("tool"), toolName);
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlRecordUpdate(int index, const QString &deltaText)
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("record_update"));
    payload.insert(QStringLiteral("index"), index);
    payload.insert(QStringLiteral("delta"), deltaText);
    controlChannel_->sendToController(payload);
}

void Widget::broadcastControlOutput(const QString &result, bool isStream, const QColor &color, const QString &roleHint, int thinkActiveFlag)
{
    if (!isHostControlled()) return;
    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("output"));
    payload.insert(QStringLiteral("text"), result);
    payload.insert(QStringLiteral("stream"), isStream);
    payload.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
    if (!roleHint.isEmpty()) payload.insert(QStringLiteral("role"), roleHint);
    if (thinkActiveFlag >= 0) payload.insert(QStringLiteral("think_active"), thinkActiveFlag);
    controlChannel_->sendToController(payload);
    // FlowTracer::log(FlowChannel::Session,
    //                 QStringLiteral("[control] host stream role=%1 stream=%2 think=%3 text=%4")
    //                     .arg(roleHint.isEmpty() ? QStringLiteral("-") : roleHint)
    //                     .arg(isStream ? QStringLiteral("yes") : QStringLiteral("no"))
    //                     .arg(thinkActiveFlag)
    //                     .arg(previewForLog(result)),
    //                 activeTurnId_);
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

void Widget::appendControlStateLog(const QString &text, SIGNAL_STATE level, const QString &prefix, bool mirrorToModelInfo)
{
    QString line = text;
    if (level != MATRIX_SIGNAL)
    {
        line.replace("\n", "\\n");
        line.replace("\r", "\\r");
    }
    const QString composed = prefix.isEmpty() ? line : prefix + QStringLiteral(" ") + line;
    QTextCharFormat fmt;
    fmt.setForeground(themeStateColor(level));
    appendStateLine(composed, fmt);
    if (mirrorToModelInfo)
    {
        logControlInfoToModelInfo(composed);
    }
}

void Widget::logControlInfoToModelInfo(const QString &line)
{
    const QString withBreak = line.endsWith(QChar('\n')) ? line : line + QStringLiteral("\n");
    emit ui2expend_llamalog(withBreak);
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

void Widget::applyControlMonitor(const QJsonObject &mon)
{
    if (mon.isEmpty()) return;
    if (!ui) return;
    if (ui->cpu_bar)
    {
        ui->cpu_bar->setValue(mon.value(QStringLiteral("cpu")).toInt(ui->cpu_bar->value()));
        ui->cpu_bar->setSecondValue(mon.value(QStringLiteral("cpu2")).toInt(ui->cpu_bar->m_secondValue));
    }
    if (ui->mem_bar)
    {
        ui->mem_bar->setValue(mon.value(QStringLiteral("mem")).toInt(ui->mem_bar->value()));
        ui->mem_bar->setSecondValue(mon.value(QStringLiteral("mem2")).toInt(ui->mem_bar->m_secondValue));
    }
    if (ui->vram_bar)
    {
        ui->vram_bar->setValue(mon.value(QStringLiteral("vram")).toInt(ui->vram_bar->value()));
        ui->vram_bar->setSecondValue(mon.value(QStringLiteral("vram2")).toInt(ui->vram_bar->m_secondValue));
    }
    if (ui->vcore_bar)
    {
        ui->vcore_bar->setValue(mon.value(QStringLiteral("vcore")).toInt(ui->vcore_bar->value()));
    }
}

void Widget::applyControlRecordClear()
{
    recordClear();
}

void Widget::applyControlRecordAdd(RecordRole role, const QString &toolName)
{
    if (role == RecordRole::Tool) lastToolCallName_ = toolName;
    recordCreate(role, toolName);
}

void Widget::applyControlRecordUpdate(int index, const QString &deltaText)
{
    recordAppendText(index, deltaText);
}

void Widget::handleControlHostClientChanged(bool connected, const QString &reason)
{
    Q_UNUSED(reason);
    if (!connected)
    {
        controlHost_.active = false;
        controlHost_.peer.clear();
        appendControlStateLog(jtr("control disconnected"), SIGNAL_SIGNAL, jtr("control peer prefix"), true);
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
        const QString modeLabel = (ui_mode == LINK_MODE) ? QStringLiteral("链接") : QStringLiteral("本地");
        const QString stateLabel = (ui_state == CHAT_STATE) ? QStringLiteral("对话") : QStringLiteral("补完");
        const QString runLabel = is_run ? QStringLiteral("推理中") : QStringLiteral("空闲");
        const QString modelLabel = resolvedModelLabelForUi();
        const int cap = resolvedContextLimitForUi();
        const bool capKnown = cap > 0;
        int used = qMax(0, kvUsed_);
        if (capKnown && cap > 0 && used > cap) used = cap;
        const int percent = (capKnown && cap > 0) ? int(qRound(100.0 * double(used) / double(cap))) : 0;
        const QString capLabel = resolvedContextLabelForUi();
        const QString percentLabel = capKnown ? QString::number(percent) : QStringLiteral("-");
        QString infoLine = QStringLiteral("控制端 %1 已接入 | 模式:%2 | 状态:%3 | 运行:%4 | 模型:%5 | KV:%6/%7(%8%)")
                               .arg(controlHost_.peer.isEmpty() ? QStringLiteral("-") : controlHost_.peer)
                               .arg(modeLabel)
                               .arg(stateLabel)
                               .arg(runLabel)
                               .arg(modelLabel)
                               .arg(used)
                               .arg(capLabel)
                               .arg(percentLabel);
        if (!current_api.isEmpty()) infoLine += QStringLiteral(" | 端点:") + current_api;
        appendControlStateLog(infoLine, SIGNAL_SIGNAL, jtr("control peer prefix"), true);
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
        const QString modeLabel = (snap.value(QStringLiteral("mode")).toString() == QStringLiteral("link")) ? QStringLiteral("链接") : QStringLiteral("本地");
        const QString stateLabel = (snap.value(QStringLiteral("ui_state")).toString() == QStringLiteral("complete")) ? QStringLiteral("补完") : QStringLiteral("对话");
        const QString runLabel = snap.value(QStringLiteral("is_run")).toBool(false) ? QStringLiteral("推理中") : QStringLiteral("空闲");
        const int cap = snap.value(QStringLiteral("kv_cap")).toInt(0);
        bool capKnown = cap > 0;
        int used = snap.value(QStringLiteral("kv_used")).toInt(0);
        if (capKnown && cap > 0 && used > cap) used = cap;
        const int percent = (capKnown && cap > 0) ? int(qRound(100.0 * double(used) / double(cap))) : 0;
        QString modelLabel = snap.value(QStringLiteral("model_name")).toString();
        if (modelLabel.isEmpty()) modelLabel = resolvedModelLabelForUi();
        if (modelLabel.isEmpty()) modelLabel = QStringLiteral("-");
        const QString capLabel = capKnown ? QString::number(cap) : QStringLiteral("未知");
        const QString percentLabel = capKnown ? QString::number(percent) : QStringLiteral("-");
        const QString endpoint = snap.value(QStringLiteral("endpoint")).toString();
        QString infoLine = QStringLiteral("目标 %1 | 模式:%2 | 状态:%3 | 运行:%4 | 模型:%5 | KV:%6/%7(%8%)")
                               .arg(controlClient_.peer.isEmpty() ? QStringLiteral("-") : controlClient_.peer)
                               .arg(modeLabel)
                               .arg(stateLabel)
                               .arg(runLabel)
                               .arg(modelLabel)
                               .arg(used)
                               .arg(capLabel)
                               .arg(percentLabel);
        if (!endpoint.isEmpty()) infoLine += QStringLiteral(" | 端点:") + endpoint;
        appendControlStateLog(infoLine, SIGNAL_SIGNAL, jtr("control peer prefix"), true);
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
        const QString role = payload.value(QStringLiteral("role")).toString();
        const int thinkFlag = payload.value(QStringLiteral("think_active")).toInt(-1);
        if (!role.isEmpty()) controlStreamRole_ = role;
        FlowTracer::log(FlowChannel::Session,
                        QStringLiteral("[control] controller recv role=%1 stream=%2 think=%3 text=%4")
                            .arg(role.isEmpty() ? QStringLiteral("-") : role)
                            .arg(stream ? QStringLiteral("yes") : QStringLiteral("no"))
                            .arg(thinkFlag)
                            .arg(previewForLog(text)),
                        activeTurnId_);
        // Mirror host output verbatim to avoid re-parsing <think> on controller side
        if (stream) flushPendingStream();
        QString plain = text;
        plain.replace(QString(DEFAULT_THINK_BEGIN), QString());
        plain.replace(QString(DEFAULT_THINK_END), QString());
        output_scroll(plain, c);
        if (role == QStringLiteral("think"))
        {
            controlThinkActive_ = (thinkFlag != 0);
        }
        else if (!role.isEmpty() || thinkFlag == 0)
        {
            controlThinkActive_ = false;
        }
        return;
    }
    if (type == QStringLiteral("state_log"))
    {
        const QString text = payload.value(QStringLiteral("text")).toString();
        const int lv = payload.value(QStringLiteral("level")).toInt(static_cast<int>(USUAL_SIGNAL));
        appendControlStateLog(text, static_cast<SIGNAL_STATE>(lv), jtr("control peer prefix"));
        return;
    }
    if (type == QStringLiteral("kv"))
    {
        kvUsed_ = payload.value(QStringLiteral("used")).toInt(kvUsed_);
        slotCtxMax_ = payload.value(QStringLiteral("cap")).toInt(slotCtxMax_);
        updateKvBarUi();
        return;
    }
    if (type == QStringLiteral("monitor"))
    {
        const QJsonObject mon = payload.value(QStringLiteral("monitor")).toObject();
        applyControlMonitor(mon);
        return;
    }
    if (type == QStringLiteral("record_clear"))
    {
        applyControlRecordClear();
        return;
    }
    if (type == QStringLiteral("record_add"))
    {
        const int roleInt = payload.value(QStringLiteral("role")).toInt(static_cast<int>(RecordRole::System));
        const QString toolName = payload.value(QStringLiteral("tool")).toString();
        applyControlRecordAdd(static_cast<RecordRole>(roleInt), toolName);
        return;
    }
    if (type == QStringLiteral("record_update"))
    {
        const int idx = payload.value(QStringLiteral("index")).toInt(-1);
        const QString delta = payload.value(QStringLiteral("delta")).toString();
        applyControlRecordUpdate(idx, delta);
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
        appendControlStateLog(jtr("control disconnected"), SIGNAL_SIGNAL, jtr("control peer prefix"), true);
        reflash_state(jtr("control disconnected"), SIGNAL_SIGNAL);
        releaseControl(false);
        return;
    }
}

void Widget::handleControlControllerState(ControlChannel::ControllerState state, const QString &reason)
{
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
        if (linkProfile_ == LinkProfile::Control && !reason.isEmpty())
        {
            if (reason == QStringLiteral("refused"))
            {
                reflash_state(jtr("control refused disabled"), WRONG_SIGNAL);
            }
            else
            {
                reflash_state(jtr("control refused generic"), WRONG_SIGNAL);
            }
        }
        if (linkProfile_ == LinkProfile::Control) releaseControl(false);
    }
}

void Widget::applyControlSnapshot(const QJsonObject &snap)
{
    if (!ui) return;
    controlThinkActive_ = false;
    controlStreamRole_.clear();
    recordClear();
    if (streamFlushTimer_ && streamFlushTimer_->isActive()) streamFlushTimer_->stop();
    streamPending_.clear();
    streamPendingChars_ = 0;
    temp_assistant_history.clear();
    pendingAssistantHeaderReset_ = false;
    turnThinkActive_ = false;
    turnThinkHeaderPrinted_ = false;
    turnAssistantHeaderPrinted_ = false;
    currentThinkIndex_ = -1;
    currentAssistantIndex_ = -1;
    is_stop_output_scroll = false;
    if (ui->output) resetOutputDocument();
    if (ui->state) resetStateDocument();

    const auto renderRoleLabel = [&](RecordRole role) -> QString
    {
        switch (role)
        {
        case RecordRole::System: return QStringLiteral("system");
        case RecordRole::User: return QStringLiteral("user");
        case RecordRole::Assistant: return QStringLiteral("assistant");
        case RecordRole::Think: return QStringLiteral("think");
        case RecordRole::Tool: return QStringLiteral("tool");
        }
        return QString();
    };
    const auto renderColor = [&](RecordRole role) -> QColor
    {
        if (role == RecordRole::Think) return themeThinkColor();
        if (role == RecordRole::Tool) return themeStateColor(TOOL_SIGNAL);
        return themeTextPrimary();
    };

    bool renderedFromRecords = false;
    const QJsonArray recs = snap.value(QStringLiteral("records")).toArray();
    if (ui->output && !recs.isEmpty())
    {
        for (const auto &v : recs)
        {
            if (!v.isObject()) continue;
            const QJsonObject ro = v.toObject();
            const int roleInt = ro.value(QStringLiteral("role")).toInt(static_cast<int>(RecordRole::System));
            const QString text = ro.value(QStringLiteral("text")).toString();
            const QString toolName = ro.value(QStringLiteral("tool")).toString();
            const int msgIndex = ro.value(QStringLiteral("msg_index")).toInt(-1);
            const RecordRole role = static_cast<RecordRole>(roleInt);
            const QString header = renderRoleLabel(role);
            const int idx = recordCreate(role, toolName);
            if (!header.isEmpty()) appendRoleHeader(header);
            if (!text.isEmpty()) reflash_output(text, 0, renderColor(role));
            recordAppendText(idx, text);
            recordEntries_[idx].msgIndex = msgIndex;
            if (role == RecordRole::System) lastSystemRecordIndex_ = idx;
            renderedFromRecords = true;
        }
    }
    if (!renderedFromRecords && ui->output)
    {
        ui->output->setPlainText(snap.value(QStringLiteral("output")).toString());
    }
    if (ui->state) ui->state->setPlainText(snap.value(QStringLiteral("state_log")).toString());
    kvUsed_ = snap.value(QStringLiteral("kv_used")).toInt(kvUsed_);
    slotCtxMax_ = snap.value(QStringLiteral("kv_cap")).toInt(slotCtxMax_);
    updateKvBarUi();
    if (renderedFromRecords && ui->recordBar)
    {
        for (int i = 0; i < recordEntries_.size(); ++i)
        {
            QString tip = recordEntries_[i].text;
            if (tip.size() > 600) tip = tip.left(600) + "...";
            ui->recordBar->updateNode(i, tip);
        }
    }

    applyControlMonitor(snap.value(QStringLiteral("monitor")).toObject());
    FlowTracer::log(FlowChannel::Session,
                    QStringLiteral("[control] controller snapshot applied records=%1 rendered_from_records=%2 output=%3")
                        .arg(recs.size())
                        .arg(renderedFromRecords ? 1 : 0)
                        .arg((ui && ui->output) ? ui->output->toPlainText().size() : 0),
                    activeTurnId_);
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
    blockLocalMonitor_ = isControllerActive();
    const QString dateLabel = jtr("date");
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
            ui->set->setText(QString());
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
