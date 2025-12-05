// Model evaluation tab: orchestrates one-click benchmarks against current endpoint
#include "expend.h"

#include "../prompt.h"
#include "../utils/devicemanager.h"
#include "../utils/flowprogressbar.h"
#include "../xnet.h"
#include "../xtool.h"

#include "ui_expend.h"
#include <QAbstractItemView>
#include <QFileInfo>
#include <QHeaderView>
#include <QStyle>
#include <QVariant>

// Simple helper: strip think markers from a chunk
static inline QString stripThink(const QString &s)
{
    QString t = s;
    t.replace(QString(DEFAULT_THINK_BEGIN), QString());
    t.replace(QString(DEFAULT_THINK_END), QString());
    return t;
}

void Expend::ensureEvalNet()
{
    if (evalNet) return;
    // Register meta-types once (harmless if repeated)
    qRegisterMetaType<APIS>("APIS");
    qRegisterMetaType<ENDPOINT_DATA>("ENDPOINT_DATA");
    qRegisterMetaType<SETTINGS>("SETTINGS");
    qRegisterMetaType<EVA_MODE>("EVA_MODE");

    evalNet = new xNet();
    evalThread = new QThread(this);
    evalNet->moveToThread(evalThread);
    // Ensure worker is destroyed in its own thread context when the thread exits
    connect(evalThread, &QThread::finished, evalNet, &QObject::deleteLater);
    evalThread->start();

    // Wire signals into this window (only for eval tab)
    connect(evalNet, &xNet::net2ui_output, this, &Expend::onEvalOutput, Qt::QueuedConnection);
    connect(evalNet, &xNet::net2ui_state, this, &Expend::onEvalState, Qt::QueuedConnection);
    connect(evalNet, &xNet::net2ui_pushover, this, &Expend::onEvalPushover, Qt::QueuedConnection);
    connect(evalNet, &xNet::net2ui_speeds, this, &Expend::onEvalSpeeds, Qt::QueuedConnection);
}

void Expend::updateEvalInfoUi()
{
    if (!ui) return;
    if (!ui->eval_info_groupBox) return;

    ui->eval_mode_value->setText(eval_mode == LOCAL_MODE ? jtr("local mode") : jtr("link mode"));
    ui->eval_endpoint_value->setText(eval_apis.api_endpoint.isEmpty() ? QStringLiteral("-") : eval_apis.api_endpoint);
    // Model: local shows only file name (not full path); link shows provider model id
    QString modelStr;
    if (eval_mode == LOCAL_MODE)
    {
        const QString p = eval_settings.modelpath.trimmed();
        if (!p.isEmpty())
        {
            modelStr = QFileInfo(p).fileName();
            if (modelStr.isEmpty())
            {
                QString t = p;
                t.replace("\\", "/");
                const int k = t.lastIndexOf('/');
                modelStr = (k >= 0 ? t.mid(k + 1) : p);
            }
        }
    }
    else
    {
        modelStr = eval_apis.api_model;
    }
    if (modelStr.isEmpty()) modelStr = QStringLiteral("-");
    // 模型名称过长时保持单行省略，避免撑开“当前信息”区域
    if (ui->eval_model_value)
    {
        ui->eval_model_value->setContentText(modelStr);
        ui->eval_model_value->setToolTip(modelStr == QStringLiteral("-") ? QString() : modelStr);
    }
    // Device and key runtime toggles (best-effort)
    ui->eval_device_value->setText(DeviceManager::effectiveBackend());
    // n_ctx: for LINK mode, Widget side passes the discovered maximum context via settings.nctx
    ui->eval_nctx_value->setText(eval_settings.nctx > 0 ? QString::number(eval_settings.nctx) : QStringLiteral("-"));
    ui->eval_threads_value->setText(eval_settings.nthread > 0 ? QString::number(eval_settings.nthread) : QStringLiteral("-"));
}

void Expend::evalResetUi()
{
    // Initialize merged table: [指标/步骤 | 状态 | 用时(s) | 值]
    if (!ui->eval_table) return;
    ui->eval_table->clearContents();
    ui->eval_table->setRowCount(5);
    ui->eval_table->setColumnCount(4);
    QStringList headers;
    headers << jtr("metric/step") << jtr("state") << jtr("elapsed(s)") << jtr("value");
    ui->eval_table->setHorizontalHeaderLabels(headers);
    // Make the table auto-fit the available area
    ui->eval_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->eval_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->eval_table->verticalHeader()->setVisible(false);
    ui->eval_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->eval_table->setSelectionMode(QAbstractItemView::NoSelection);
    ui->eval_table->setFocusPolicy(Qt::NoFocus);
    ui->eval_table->setAlternatingRowColors(true);
    ui->eval_table->setShowGrid(false);
    ui->eval_table->setWordWrap(false);
    // Init rows with default status (5 steps)
    evalSetTable(0, jtr("first token"), QStringLiteral("-"));
    evalSetTable(1, jtr("gen speed"), QStringLiteral("-"));
    evalSetTable(2, jtr("common qa"), QStringLiteral("-"));
    evalSetTable(3, jtr("logic"), QStringLiteral("-"));
    evalSetTable(4, jtr("tool call"), QStringLiteral("-"));
    for (int r = 0; r < 5; ++r) evalSetStatus(r, jtr("pending"));

    ui->eval_log_plainTextEdit->clear();
    // Progress bar (units recomputed by start); put text inside the bar
    if (ui->eval_progressBar)
    {
        ui->eval_progressBar->setMaximum(stepsUnitsTotal);
        ui->eval_progressBar->setValue(0);
        ui->eval_progressBar->setTextVisible(true);
        ui->eval_progressBar->setFormat(jtr("progress steps"));
        ui->eval_progressBar->setMinimumHeight(22);
        // Blue-themed, glossy progress bar with subtle depth
        // Scoped to this widget by setting the stylesheet on the instance.
        ui->eval_progressBar->setStyleSheet(QStringLiteral(
            "QProgressBar {\n"
            "  text-align: center;\n"
            "  color: #E8F2FF;\n"
            "  min-height: 22px;\n"
            "  border: none;\n"
            "  border-radius: 5px;\n"
            "  background: QLinearGradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0b1e33, stop:1 #0f2b4a);\n"
            "}\n"
            "QProgressBar::chunk {\n"
            "  margin: 1px;\n"
            "  border-radius: 9px;\n"
            "  border: 1px solid rgba(255,255,255,20);\n"
            "  background: QLinearGradient(x1:0, y1:0, x2:0, y2:1,\n"
            "                               stop:0 #6fc1ff,\n"
            "                               stop:0.45 #489ef7,\n"
            "                               stop:0.55 #2e7fd6,\n"
            "                               stop:1 #1e62b0);\n"
            "}\n"));
    }
    updateEvalSummary(true);
}

void Expend::evalSetTable(int row, const QString &name, const QString &val, const QString &desc)
{
    Q_UNUSED(desc);
    auto setItem = [&](int r, int c, const QString &text)
    {
        QTableWidgetItem *it = ui->eval_table->item(r, c);
        if (!it)
        {
            it = new QTableWidgetItem();
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            ui->eval_table->setItem(r, c, it);
        }
        it->setText(text);
    };
    setItem(row, 0, name);
    setItem(row, 3, val);
    // Colorize value cell by metric threshold if possible
    /* no color fill for value column by request */
}

void Expend::evalSetStatus(int row, const QString &status)
{
    if (!ui || !ui->eval_table) return;
    QTableWidgetItem *it = ui->eval_table->item(row, 1);
    if (!it)
    {
        it = new QTableWidgetItem();
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        ui->eval_table->setItem(row, 1, it);
    }
    it->setText(status);
}

void Expend::evalSetElapsed(int row, double seconds)
{
    if (!ui || !ui->eval_table) return;
    QTableWidgetItem *it = ui->eval_table->item(row, 2);
    if (!it)
    {
        it = new QTableWidgetItem();
        it->setFlags(it->flags() & ~Qt::ItemIsEditable);
        ui->eval_table->setItem(row, 2, it);
    }
    if (seconds >= 0)
        it->setText(QString::number(seconds, 'f', 2));
    else
        it->setText(QString());
}

void Expend::updateEvalButtonState()
{
    if (!ui || !ui->eval_start_pushButton) return;
    const bool running = evalRunning;
    ui->eval_start_pushButton->setText(running ? jtr("stop evaluate") : jtr("evaluate"));
    ui->eval_start_pushButton->setEnabled(true);
}

void Expend::evalLog(const QString &line)
{
    if (!ui || !ui->eval_log_plainTextEdit) return;
    ui->eval_log_plainTextEdit->appendPlainText(line);
}

ENDPOINT_DATA Expend::makeBaseData(double temp, int npredict)
{
    ENDPOINT_DATA d{};
    d.is_complete_state = false;
    d.temp = float(temp);
    d.repeat = eval_settings.repeat;
    d.top_k = eval_settings.top_k;
    d.top_p = eval_settings.hid_top_p;
    d.n_predict = npredict;
    d.reasoning_effort = sanitizeReasoningEffort(eval_settings.reasoning_effort);
    d.stopwords = QStringList();
    d.id_slot = -1;
    return d;
}

QJsonArray Expend::makeMsgs(const QString &sys, const QString &user)
{
    QJsonArray arr;
    QJsonObject s;
    s.insert("role", DEFAULT_SYSTEM_NAME);
    s.insert("content", sys);
    QJsonObject u;
    u.insert("role", DEFAULT_USER_NAME);
    u.insert("content", user);
    arr.append(s);
    arr.append(u);
    return arr;
}

void Expend::on_eval_start_pushButton_clicked()
{
    // Toggle evaluation: click again while running to abort the current task.
    if (evalRunning)
    {
        if (ui && ui->eval_progressBar)
        {
            if (auto fp = qobject_cast<FlowProgressBar *>(ui->eval_progressBar)) fp->setFlowing(false);
        }
        if (evalNet)
        {
            QMetaObject::invokeMethod(evalNet, "recv_stop", Qt::QueuedConnection, Q_ARG(bool, true));
        }
        evalRunning = false;
        evalLog(QStringLiteral("eval: ") + jtr("stopped"));
        updateEvalButtonState();
        return;
    }

    if (ui && ui->eval_progressBar)
    {
        if (auto fp = qobject_cast<FlowProgressBar *>(ui->eval_progressBar)) fp->setFlowing(true);
    }
    ensureEvalNet();
    if (eval_apis.api_endpoint.trimmed().isEmpty())
    {
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        const QString port = settings.value("port", DEFAULT_SERVER_PORT).toString();
        eval_apis.api_endpoint = QString("http://127.0.0.1:%1").arg(port);
        eval_apis.is_local_backend = true;
    }
    toolCases_.clear();
    // Construct evaluation tasks for each tool (two natural prompts per tool).
    toolCases_.push_back({QStringLiteral("calculator"), jtr("tc_desc_calculator"), jtr("tc_tag_calculator")});
    toolCases_.push_back({QStringLiteral("calculator"), jtr("tc_desc_calculator_2"), jtr("tc_tag_calculator_2")});
    toolCases_.push_back({QStringLiteral("stablediffusion"), jtr("tc_desc_sd"), jtr("tc_tag_sd")});
    toolCases_.push_back({QStringLiteral("stablediffusion"), jtr("tc_desc_sd_2"), jtr("tc_tag_sd_2")});
    toolCases_.push_back({QStringLiteral("knowledge"), jtr("tc_desc_knowledge"), jtr("tc_tag_knowledge")});
    toolCases_.push_back({QStringLiteral("knowledge"), jtr("tc_desc_knowledge_2"), jtr("tc_tag_knowledge_2")});
    toolCases_.push_back({QStringLiteral("execute_command"), jtr("tc_desc_exec"), jtr("tc_tag_exec")});
    toolCases_.push_back({QStringLiteral("execute_command"), jtr("tc_desc_exec_2"), jtr("tc_tag_exec_2")});
    toolCases_.push_back({QStringLiteral("mcp_tools_list"), jtr("tc_desc_mcp"), jtr("tc_tag_mcp")});
    toolCases_.push_back({QStringLiteral("mcp_tools_list"), jtr("tc_desc_mcp_2"), jtr("tc_tag_mcp_2")});
    toolCases_.push_back({QStringLiteral("controller"), jtr("tc_desc_controller"), jtr("tc_tag_controller")});
    toolCases_.push_back({QStringLiteral("controller"), jtr("tc_desc_controller_2"), jtr("tc_tag_controller_2")});

    stepsUnitsTotal = 1 /*latency*/ + genPlanned_ /*gen (multi-run)*/ + qaPlanned_ /*qa*/ + logicPlanned_ /*logic*/ + toolCases_.size() /*tools*/;
    evalResetUi();
    updateEvalInfoUi();
    evalRunning = true;
    updateEvalButtonState();
    evalStep = 0;
    stepsDone = 0;
    stepTimer.invalidate();
    m_firstTokenMs = -1.0;
    m_promptTokPerSec = -1.0;
    m_genTokPerSec = -1.0;
    m_genCharsPerSec = -1.0;
    m_qaScore = -1.0;
    m_logicScore = -1.0;
    m_toolScore = -1.0;
    m_syncRate = -1.0;
    updateEvalSummary(true);
    genRunIndex_ = 0;
    genTokPerSecSum_ = 0.0;
    qaIndex_ = 0;
    qaCorrect_ = 0;
    logicIndex_ = 0;
    logicCorrect_ = 0;
    toolIndex_ = 0;
    toolCorrect_ = 0;
    genCounting_ = false;
    genStartNsRel_ = 0;
    evalUpdateProgress();
    evalNext();
}

QChar Expend::parseMCAnswer(const QString &ans)
{
    // Accept formats like A, a, 'A)', '选A', '答案：C', etc.
    const QString s = ans.trimmed();
    if (s.isEmpty()) return QChar();
    for (const QChar &c : {QChar('A'), QChar('B'), QChar('C'), QChar('D'), QChar('a'), QChar('b'), QChar('c'), QChar('d')})
    {
        if (s.contains(c, Qt::CaseInsensitive)) return c.toUpper();
    }
    return QChar();
}

void Expend::evalNext()
{
    if (!evalRunning) return;
    switch (evalStep)
    {
    case 0: runLatencyTest(); break;
    case 1: runGenSpeedTest(); break;
    case 2: runQATest(); break;       // Common-sense MC
    case 3: runLogicTest(); break;    // Logical MC
    case 4: runToolcallTest(); break; // Tools multi-cases
    default: evalFinish(); break;
    }
}

void Expend::runLatencyTest()
{
    // Step header (no leading blank for the very first step)
    evalLog(jtr("latency intro"));
    evalSetStatus(0, jtr("in progress"));
    ENDPOINT_DATA d = makeBaseData(0.2, 1);
    const QString bigUser = QString(1024, QLatin1Char('A'));
    d.messagesArray = makeMsgs(QStringLiteral("You are a helpful assistant. Reply briefly."), bigUser);
    // Prepare and fire
    evalFirstToken = false;
    evalAccum.clear();
    evalTimer.restart();
    stepTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::runGenSpeedTest()
{
    // Two-run measurement: show progress like i/N
    if (genRunIndex_ == 0)
    {
        evalLog(QString());
        evalLog(jtr("gen intro"));
    }
    evalSetStatus(1, jtr("in progress") + " " + QString::number(genRunIndex_) + "/" + QString::number(genPlanned_));
    // Note: per-run counters are reset after timers restart below to ensure
    // a clean boundary between runs (see duplicates after stepTimer.restart()).
    ENDPOINT_DATA d = makeBaseData(0.0, 1024);
    const QString ask = jtr("gen essay prompt");
    d.messagesArray = makeMsgs(QStringLiteral("You are a helpful assistant."), ask);
    evalFirstToken = false;
    evalAccum.clear();
    evalAccumRaw_.clear();
    evalReasoning_.clear();
    evalAnswer_.clear();
    evalThinkMode_ = false;
    evalTimer.restart();
    stepTimer.restart();
    genCounting_ = false;
    genStartNsRel_ = 0;
    // Reset token speed holders for this run
    m_genTokPerSec = -1.0;
    m_genCharsPerSec = -1.0;
    m_genStreamChunks = 0;
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::runQATest()
{
    if (qaPairs_.isEmpty())
    {
        // Initialize common-sense MC (A-D). second is expected option in lower case
        qaPairs_.clear();
        qaPairs_.push_back({jtr("qa1"), QStringLiteral("b")});
        qaPairs_.push_back({jtr("qa2"), QStringLiteral("b")});
        qaPairs_.push_back({jtr("qa3"), QStringLiteral("c")});
        qaPairs_.push_back({jtr("qa4"), QStringLiteral("a")});
        qaPairs_.push_back({jtr("qa5"), QStringLiteral("d")});
        qaIndex_ = 0;
        qaCorrect_ = 0;
        // Blank line to visually separate previous step
        evalLog(QString());
        evalLog(jtr("qa intro"));
    }
    if (qaIndex_ >= qaPairs_.size())
    {
        // Finalize score
        m_qaScore = (qaPairs_.isEmpty() ? 0.0 : (100.0 * double(qaCorrect_) / double(qaPairs_.size())));
        evalSetTable(2, jtr("common qa"), QString::number(m_qaScore, 'f', 1), jtr("hit rate"));
        evalStep++;
        evalNext();
        return;
    }
    // One question per turn
    const auto &p = qaPairs_[qaIndex_];
    if (qaIndex_ == 0) evalSetStatus(2, jtr("in progress") + QStringLiteral(" 0/") + QString::number(qaPlanned_));
    // Print question first for QA
    evalLog(QStringLiteral("[") + jtr("common qa") + QStringLiteral("] ") + jtr("question") + QStringLiteral("(") + QString::number(qaIndex_ + 1) + "/" + QString::number(qaPlanned_) + QStringLiteral(")\n") + p.first);
    ENDPOINT_DATA d = makeBaseData(0.1, 64);
    d.messagesArray = makeMsgs(QStringLiteral("You are a concise assistant. Reply with a single letter A/B/C/D only."), p.first);
    evalFirstToken = false;
    // Reset per-turn accumulators
    evalAccum.clear();
    evalAccumRaw_.clear();
    evalReasoning_.clear();
    evalAnswer_.clear();
    evalThinkMode_ = false;
    evalTimer.restart();
    if (qaIndex_ == 0) stepTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::runLogicTest()
{
    if (logicPairs_.isEmpty())
    {
        // Initialize 5 harder MC questions (Olympiad-style, simplified)
        logicPairs_.clear();
        logicPairs_.push_back({jtr("logic1"), QStringLiteral("b")});
        logicPairs_.push_back({jtr("logic2"), QStringLiteral("b")});
        logicPairs_.push_back({jtr("logic3"), QStringLiteral("c")});
        logicPairs_.push_back({jtr("logic4"), QStringLiteral("d")});
        logicPairs_.push_back({jtr("logic5"), QStringLiteral("c")});
        logicIndex_ = 0;
        logicCorrect_ = 0;
        // Blank line to visually separate previous step
        evalLog(QString());
        evalLog(jtr("logic intro"));
    }
    if (logicIndex_ >= logicPairs_.size())
    {
        m_logicScore = (logicPairs_.isEmpty() ? 0.0 : (100.0 * double(logicCorrect_) / double(logicPairs_.size())));
        evalSetTable(3, jtr("logic"), QString::number(m_logicScore, 'f', 1), jtr("hit rate"));
        evalStep++;
        evalNext();
        return;
    }
    const auto &p = logicPairs_[logicIndex_];
    if (logicIndex_ == 0) evalSetStatus(3, jtr("in progress") + QStringLiteral(" 0/") + QString::number(logicPlanned_));
    // Print question first for Logic
    evalLog(QStringLiteral("[") + jtr("logic") + QStringLiteral("] ") + jtr("question") + QStringLiteral("(") + QString::number(logicIndex_ + 1) + "/" + QString::number(logicPlanned_) + QStringLiteral(")\n") + p.first);
    ENDPOINT_DATA d = makeBaseData(0.1, 64);
    d.messagesArray = makeMsgs(QStringLiteral("You are a concise assistant. Reply with a single letter A/B/C/D only."), p.first);
    evalFirstToken = false;
    // Reset per-turn accumulators for logic question
    evalAccum.clear();
    evalAccumRaw_.clear();
    evalReasoning_.clear();
    evalAnswer_.clear();
    evalThinkMode_ = false;
    evalTimer.restart();
    if (logicIndex_ == 0) stepTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::runToolcallTest()
{
    if (toolIndex_ == 0)
    {
        // Blank line to visually separate previous step
        evalLog(QString());
        evalLog(jtr("toolcall intro") + QStringLiteral(" (") + QString::number(toolCases_.size()) + QStringLiteral(" ") + jtr("items") + QStringLiteral(")"));
        evalSetStatus(4, jtr("in progress") + QStringLiteral(" 0/") + QString::number(toolCases_.size()));
    }
    if (toolIndex_ >= toolCases_.size())
    {
        // Finalize tool score
        const int total = qMax(1, toolCases_.size());
        m_toolScore = 100.0 * double(toolCorrect_) / double(total);
        // Use a generic description consistent with QA/Logic
        evalSetTable(4, jtr("tool call"), QString::number(m_toolScore, 'f', 0), jtr("hit rate"));
        evalSetStatus(4, jtr("completed"));
        evalSetElapsed(4, stepTimer.nsecsElapsed() / 1e9);
        evalStep++;
        evalNext();
        return;
    }
    // Prepare one tool case
    const ToolCase &tc = toolCases_[toolIndex_];
    QString toolsDesc = promptx::toolAnswer().text + QString("\n\n") + promptx::toolCalculator().text + QString("\n\n") + promptx::toolStableDiffusion().text + QString("\n\n") + promptx::toolKnowledge().text + QString("\n\n") + promptx::toolExecuteCommand().text + QString("\n\n") + promptx::toolController().text + QString("\n\n") + promptx::toolMcpList().text;
    // Print tool case header and task before model output
    evalLog(QStringLiteral("[") + jtr("tool call") + QStringLiteral("] ") + QString("(%1/%2) ").arg(toolIndex_ + 1).arg(toolCases_.size()) + tc.desc + QStringLiteral("\n") + jtr("task") + QStringLiteral("\n") + tc.user);
        QString sys = promptx::extraPromptTemplate();
    sys.replace("{available_tools_describe}", toolsDesc);
    sys.replace("{engineer_info}", QString());
    const QString task = tc.user + QStringLiteral(" Strictly output exactly one <tool_call> JSON and stop.");

    ENDPOINT_DATA d = makeBaseData(0.2, 640);
    d.messagesArray = makeMsgs(sys, task);
    evalFirstToken = false;
    // Reset per-turn accumulators for tool case
    evalAccum.clear();
    evalAccumRaw_.clear();
    evalReasoning_.clear();
    evalAnswer_.clear();
    evalThinkMode_ = false;
    evalStreamSeen_ = false;
    if (toolIndex_ == 0) stepTimer.restart();
    evalTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::evalFinish()
{ // Stop progress animation on finish
    if (ui && ui->eval_progressBar)
    {
        if (auto fp = qobject_cast<FlowProgressBar *>(ui->eval_progressBar)) fp->setFlowing(false);
    }
    // Weighted overall score per spec: 10% TTFB, 20% Gen, 20% Common QA, 20% Logic, 30% Tools
    auto scoreTTFB = [&](double ms)
    {
        if (ms < 0) return 0.0;
        if (ms <= 500.0) return 100.0;
        if (ms >= 10000.0) return 0.0;
        return (10000.0 - ms) * 100.0 / (10000.0 - 500.0);
    };
    auto scoreGen = [&](double tokps) -> double
    {
        if (tokps < 0) return 0.0;
        if (tokps >= 100.0) return 100.0;
        if (tokps <= 0.0) return 0.0;
        return tokps;
    };
    const double s_ttfb = scoreTTFB(m_firstTokenMs);
    const double s_gen = scoreGen(m_genTokPerSec);
    const double s_qa = qMax(0.0, m_qaScore);
    const double s_log = qMax(0.0, m_logicScore);
    const double s_tool = qMax(0.0, m_toolScore);
    m_syncRate = std::max(0.0, std::min(100.0, 0.10 * s_ttfb + 0.20 * s_gen + 0.20 * s_qa + 0.20 * s_log + 0.30 * s_tool));
    // Do not show overall score in the table; only log it and reflect on bar chart
    // Insert spacing before the final summary for better visual grouping
    evalLog(QString());
    evalLog(jtr("evaluation finished") + QStringLiteral(" ") + jtr("sync rate") + QStringLiteral(" = ") + QString::number(m_syncRate, 'f', 1));
    evalRunning = false;
    updateScoreBars();
    updateEvalButtonState();
}

// ---------------- evalNet signal handlers ----------------

void Expend::onEvalOutput(const QString &text, bool streaming, QColor)
{
    Q_UNUSED(streaming);
    if (!evalRunning) return;
    if (!text.isEmpty()) evalStreamSeen_ = true;
    const bool rawHasContent = !text.trimmed().isEmpty();

    // Capture raw stream and split into reasoning/output segments for debugging
    // We keep a lightweight state machine here to accumulate content inside/outside <think>
    // so that QA can print the model's thought process and final output separately.
    {
        const QString begin = QString(DEFAULT_THINK_BEGIN);
        const QString end = QString(DEFAULT_THINK_END);
        QString s = text;
        if (!s.isEmpty())
            evalAccumRaw_ += s; // keep raw stream for reference
        while (!s.isEmpty())
        {
            if (evalThinkMode_)
            {
                const int e = s.indexOf(end);
                if (e >= 0)
                {
                    // Append content up to </think>, then switch to normal mode
                    evalReasoning_ += s.left(e);
                    s.remove(0, e + end.size());
                    evalThinkMode_ = false;
                }
                else
                {
                    // Entire chunk belongs to reasoning
                    evalReasoning_ += s;
                    s.clear();
                }
            }
            else
            {
                const int b = s.indexOf(begin);
                if (b >= 0)
                {
                    // Append normal content before <think>, then enter think mode
                    evalAnswer_ += s.left(b);
                    s.remove(0, b + begin.size());
                    evalThinkMode_ = true;
                }
                else
                {
                    // Entire chunk is normal content
                    evalAnswer_ += s;
                    s.clear();
                }
            }
        }
        // For generation speed, start counting when ANY output arrives (including <think>)
        if (evalStep == 1 && !genCounting_ && !text.trimmed().isEmpty())
        {
            genCounting_ = true;
            genStartNsRel_ = stepTimer.nsecsElapsed();
        }
    }

    const QString chunk = stripThink(text);
    // Count only chunks with actual content (exclude pure <think>/</think> markers and whitespace)
        if (evalStep == 1 && !chunk.trimmed().isEmpty())
            ++m_genStreamChunks;
    if (!evalFirstToken && rawHasContent)
    {
        evalFirstToken = true;
        const double ms = evalTimer.isValid() ? (evalTimer.nsecsElapsed() / 1e6) : -1.0;
        if (evalStep == 0)
        {
            m_firstTokenMs = ms;
            // Display score (0-100) in the "值" column for TTFB
            auto scoreTTFB = [&](double t_ms)
            {
                if (t_ms < 0) return 0.0;
                if (t_ms <= 500.0) return 100.0;
                if (t_ms >= 10000.0) return 0.0;
                return (10000.0 - t_ms) * 100.0 / (10000.0 - 500.0);
            };
            const double s = scoreTTFB(m_firstTokenMs);
            evalSetTable(0, jtr("first token"), QString::number(s, 'f', 0));
            updateScoreBars();
            // Immediately stop the request after first token to measure TTFB only
            if (evalNet)
            {
                QMetaObject::invokeMethod(evalNet, "recv_stop", Qt::QueuedConnection, Q_ARG(bool, true));
            }
        }
        else if (evalStep == 1)
        {
            // Start counting gen speed on first output token (including <think>)
            if (!genCounting_ && !text.trimmed().isEmpty())
            {
                genCounting_ = true;
                genStartNsRel_ = stepTimer.nsecsElapsed();
            }
        }
        // Log per-step ttfb briefly
        evalLog(jtr("ttfb") + ": " + QString::number(ms, 'f', 1) + QStringLiteral(" ms"));
    }
    evalAccum += chunk;
}

void Expend::onEvalState(const QString &line, SIGNAL_STATE st)
{
    Q_UNUSED(st);
    if (!evalRunning) return;
    // Reduce noise; only log important errors or http codes
    if (line.startsWith("net:http") || line.startsWith("net:"))
        evalLog(line);
    if (st == WRONG_SIGNAL)
    {
        // Mark current step as failed
        // Map evalStep -> row index in merged table
        int r = 0;
        if (evalStep == 0)
            r = 0;
        else if (evalStep == 1)
            r = 1;
        else if (evalStep == 2)
            r = 2;
        else if (evalStep == 3)
            r = 3;
        else /*evalStep>=4*/
            r = 4;
        evalSetStatus(r, jtr("error"));
        evalSetElapsed(r, stepTimer.nsecsElapsed() / 1e9);
        evalRunning = false;
        evalFinish();
    }
}

void Expend::onEvalSpeeds(double prompt_per_s, double gen_per_s)
{
    if (!evalRunning) return;
    // Only update generation-related speeds during the generation stage.
    // Otherwise later stages (QA/Logic/Tools) would keep changing the Gen bar
    // causing visual flicker until the final summary fixes it.
    if (prompt_per_s > 0 && evalStep == 1)
    {
        m_promptTokPerSec = prompt_per_s;
        // no explicit row for prompt speed in the new rubric
    }
    if (gen_per_s > 0 && evalStep == 1)
    {
        m_genTokPerSec = gen_per_s;
        {
            double __score = (m_genTokPerSec <= 0 ? 0.0 : (m_genTokPerSec >= 100.0 ? 100.0 : m_genTokPerSec));
            evalSetTable(1, jtr("gen speed"), QString::number(__score, 'f', 0));
        }
        updateScoreBars();
    }
}

void Expend::onEvalPushover()
{
    if (!evalRunning) return;
    // Close out current step
    switch (evalStep)
    {
    case 0:
        if (m_firstTokenMs < 0 && evalStreamSeen_)
        {
            m_firstTokenMs = 0.0;
            evalFirstToken = true;
            evalSetTable(0, jtr("first token"), QString::number(100.0, 'f', 0));
            updateScoreBars();
        }
        // Nothing else; rely on server-reported speeds if any
        // Log model's actual answer
        evalLog(QStringLiteral("[") + jtr("first token") + QStringLiteral("] ") + jtr("model answer") + QStringLiteral(":\n") + evalAccum);
        // Visual separation after this step's output
        evalLog(QString());
        // Mark step 1 done
        evalSetStatus(0, jtr("completed"));
        // 用时列单位为 s；首次响应以 ms 计，需转换
        if (m_firstTokenMs >= 0)
            evalSetElapsed(0, m_firstTokenMs / 1000.0);
        else
            evalSetElapsed(0, stepTimer.nsecsElapsed() / 1e9);
        stepsDone++;
        evalUpdateProgress();
        updateScoreBars();
        evalStep++;
        evalNext();
        break;
    case 1:
    {
        // Derive tok/s; prefer server-reported. If missing, estimate from chars using heuristic.
        double tSec = evalTimer.isValid() ? (evalTimer.nsecsElapsed() / 1e9) : -1.0;
        bool tokEstimated = false; // whether token/s is estimated from chars/s
        // Exclude think time: use relative start captured at first non-think output
        if (genCounting_ && stepTimer.isValid())
        {
            const qint64 ns = stepTimer.nsecsElapsed();
            if (ns > genStartNsRel_) tSec = double(ns - genStartNsRel_) / 1e9;
        }
        if (tSec > 0)
        {
            // Prefer server-reported tok/s. If missing, estimate by counting streamed chunks.
            if (m_genTokPerSec <= 0 && m_genStreamChunks > 0)
            {
                const double estTokPerSec = double(m_genStreamChunks) / tSec; // 1 chunk ~= 1 token
                m_genTokPerSec = estTokPerSec;
                tokEstimated = true;
            }
            // After estimation or server report, compute and show score (0..100)
            double __score = (m_genTokPerSec <= 0 ? 0.0 : (m_genTokPerSec >= 100.0 ? 100.0 : m_genTokPerSec));
            evalSetTable(1, jtr("gen speed"), QString::number(__score, 'f', 0));
            updateScoreBars();

            // Per-run token speed log (no chars/s or length)
            QString speedLine = QStringLiteral("[") + jtr("gen speed") + QStringLiteral("] ") + jtr("speed") + QStringLiteral(": ");
            if (m_genTokPerSec > 0)
            {
                speedLine += QString::number(m_genTokPerSec, 'f', 1) + QStringLiteral(" tok/s");
                speedLine += QStringLiteral("(") + (tokEstimated ? jtr("estimated") : jtr("server reported/real-time")) + QStringLiteral(")");
            }
            if (tSec > 0)
            {
                speedLine += QStringLiteral("; ") + jtr("elapsed") + " " + QString::number(tSec, 'f', 2) + QStringLiteral(" s");
            }
            evalLog(speedLine);
        }
        // Log generated content without length postfix
        evalLog(QStringLiteral("[") + jtr("gen speed") + QStringLiteral("] ") + jtr("generated content") + QStringLiteral(":\n") + evalAccum);
        // Add a blank line after the run's output when this stage will finish
        if (genRunIndex_ + 1 >= genPlanned_) evalLog(QString());

        // Accumulate per-run token speed and advance single run
        const double currTok = m_genTokPerSec;
        if (currTok > 0) genTokPerSecSum_ += currTok;
        genRunIndex_++;
        stepsDone++;
        evalUpdateProgress();

        // If more runs planned, schedule next run; otherwise finalize this stage
        if (genRunIndex_ < genPlanned_)
        {
            evalSetStatus(1, jtr("in progress") + " " + QString::number(genRunIndex_) + "/" + QString::number(genPlanned_));
            // Start next run immediately
            runGenSpeedTest();
        }
        else
        {
            // Compute average tok/s across runs and update score once more
            const double avgTok = (genTokPerSecSum_ > 0 ? (genTokPerSecSum_ / double(genPlanned_)) : m_genTokPerSec);
            if (avgTok > 0)
            {
                m_genTokPerSec = avgTok;
                double __score = (m_genTokPerSec >= 100.0 ? 100.0 : m_genTokPerSec);
                evalSetTable(1, jtr("gen speed"), QString::number(__score, 'f', 0));
                evalLog(QStringLiteral("[") + jtr("gen speed") + QStringLiteral("] ") + jtr("average speed") + QStringLiteral(": ") + QString::number(m_genTokPerSec, 'f', 1) + QStringLiteral(" tok/s"));
                updateScoreBars();
            }
            evalSetStatus(1, jtr("completed"));
            evalSetElapsed(1, stepTimer.nsecsElapsed() / 1e9);
            evalStep++;
            evalNext();
        }
        break;
    }
    case 2:
    {
        // Judge this QA item (MC A-D). Prefer outside-<think> text as the final answer.
        const QString ansVisible = evalAnswer_.trimmed();
        const QString ansRaw = (ansVisible.isEmpty() ? evalAccum.trimmed() : ansVisible);
        const QChar pick = parseMCAnswer(ansRaw);
        const QString expect = qaPairs_[qaIndex_].second;
        const QString question = qaPairs_[qaIndex_].first;
        const bool ok = (!expect.isEmpty() && !pick.isNull() && QString(pick).toLower() == expect);
        if (ok) qaCorrect_++;
        // Log result only (question already printed when the step started)
        const QString thinkText = evalReasoning_.trimmed();
        const QString outText = ansVisible.trimmed();
        QString logLine;
        logLine += jtr("answer key") + QStringLiteral(": ") + expect.toUpper();
        if (!thinkText.isEmpty())
            logLine += QStringLiteral("\n") + jtr("reasoning") + QStringLiteral(":\n") + thinkText;
        logLine += QStringLiteral("\n") + jtr("output") + QStringLiteral(": ") + (outText.isEmpty() ? evalAccum.trimmed() : outText);
        logLine += QStringLiteral("\n") + jtr("model pick") + QStringLiteral(": ") + (pick.isNull() ? jtr("unrecognized") : QString(pick).toUpper());
        logLine += QStringLiteral("\n") + jtr("verdict") + QStringLiteral(": ") + (ok ? jtr("correct") : jtr("wrong"));
        evalLog(logLine);
        // Space between QA items for readability
        evalLog(QString());
        qaIndex_++;
        stepsDone++; // count each QA as a progress unit
        // IMPORTANT: if this stage just finished, set elapsed BEFORE triggering next stage
        const bool finished = (qaIndex_ >= qaPairs_.size());
        if (finished)
        {
            evalSetStatus(2, jtr("completed"));
            evalSetElapsed(2, stepTimer.nsecsElapsed() / 1e9);
        }
        else
        {
            evalSetStatus(2, jtr("in progress") + QStringLiteral(" ") + QString::number(std::min(qaIndex_, qaPlanned_)) + "/" + QString::number(qaPlanned_));
        }
        evalUpdateProgress();
        // Next QA or finish QA stage
        runQATest();
        if (finished) updateScoreBars();
        break;
    }
    case 3:
    {
        // Judge logic MC, prefer outside-<think> as final output
        const QString ansVisible = evalAnswer_.trimmed();
        const QString ansRaw = (ansVisible.isEmpty() ? evalAccum.trimmed() : ansVisible);
        const QChar pick = parseMCAnswer(ansRaw);
        const QString expect = logicPairs_[logicIndex_].second;
        const QString question = logicPairs_[logicIndex_].first;
        const bool ok = (!expect.isEmpty() && !pick.isNull() && QString(pick).toLower() == expect);
        if (ok) logicCorrect_++;
        // Log result only (question already printed when the step started)
        const QString thinkText = evalReasoning_.trimmed();
        const QString outText = ansVisible.trimmed();
        QString logLine;
        logLine += jtr("answer key") + QStringLiteral(": ") + expect.toUpper();
        if (!thinkText.isEmpty())
            logLine += QStringLiteral("\n") + jtr("reasoning") + QStringLiteral(":\n") + thinkText;
        logLine += QStringLiteral("\n") + jtr("output") + QStringLiteral(": ") + (outText.isEmpty() ? evalAccum.trimmed() : outText);
        logLine += QStringLiteral("\n") + jtr("model pick") + QStringLiteral(": ") + (pick.isNull() ? jtr("unrecognized") : QString(pick).toUpper());
        logLine += QStringLiteral("\n") + jtr("verdict") + QStringLiteral(": ") + (ok ? jtr("correct") : jtr("wrong"));
        evalLog(logLine);
        // Space between Logic items for readability
        evalLog(QString());
        logicIndex_++;
        stepsDone++;
        // As with QA, compute elapsed BEFORE runLogicTest() may advance and restart the timer
        const bool finished = (logicIndex_ >= logicPairs_.size());
        if (finished)
        {
            evalSetStatus(3, jtr("completed"));
            evalSetElapsed(3, stepTimer.nsecsElapsed() / 1e9);
        }
        else
        {
            evalSetStatus(3, jtr("in progress") + QStringLiteral(" ") + QString::number(std::min(logicIndex_, logicPlanned_)) + "/" + QString::number(logicPlanned_));
        }
        evalUpdateProgress();
        runLogicTest();
        if (finished) updateScoreBars();
        break;
    }
    case 4:
    {
        // Tools: evaluate current case then proceed to next
        // Prefer outside-<think> text when extracting the tool_call JSON
        const QString all = (evalAnswer_.trimmed().isEmpty() ? evalAccum : evalAnswer_);
        int s = all.indexOf("<tool_call>");
        int e = all.indexOf("</tool_call>");
        bool ok = false;
        QString jsonStr;
        if (s >= 0 && e > s)
        {
            jsonStr = all.mid(s + 11, e - (s + 11)).trimmed();
            try
            {
                mcp::json call = mcp::json::parse(jsonStr.toStdString());
                const QString name = QString::fromStdString(get_string_safely(call, "name"));
                ok = (name == toolCases_[toolIndex_].name);
            }
            catch (...)
            {
                ok = false;
            }
        }
        toolCorrect_ += ok ? 1 : 0;
        // Log result only (case header already printed before sending the task)
        QString tlog;
        const QString thinkText = evalReasoning_.trimmed();
        const QString outText = (evalAnswer_.trimmed().isEmpty() ? all : evalAnswer_.trimmed());
        if (!thinkText.isEmpty()) tlog += jtr("reasoning") + QStringLiteral(":\n") + thinkText + QStringLiteral("\n");
        tlog += jtr("output") + QStringLiteral(":\n") + outText;
        if (!jsonStr.isEmpty()) tlog += QStringLiteral("\n") + jtr("parsed tool_call json") + QStringLiteral(":\n") + jsonStr;
        tlog += QStringLiteral("\n") + jtr("verdict") + QStringLiteral(": ") + (ok ? jtr("correct tool") : jtr("wrong or missing"));
        evalLog(tlog);
        // Space between Tool-call items for readability
        evalLog(QString());
        // Update partial tool score to refresh bar chart
        const int total = qMax(1, (int)toolCases_.size());
        m_toolScore = 100.0 * double(toolCorrect_) / double(total);
        updateScoreBars();
        toolIndex_++;
        stepsDone++;
        evalSetStatus(4, jtr("in progress") + QStringLiteral(" ") + QString::number(std::min(toolIndex_, toolCases_.size())) + "/" + QString::number(toolCases_.size()));
        evalUpdateProgress();
        runToolcallTest();
        break;
    }
    default:
        evalFinish();
        break;
    }
}

void Expend::evalInitTable()
{
    // nothing else at the moment (table rows are created in evalResetUi)
}

void Expend::evalUpdateProgress()
{
    if (ui->eval_progressBar)
    {
        ui->eval_progressBar->setMaximum(stepsUnitsTotal);
        ui->eval_progressBar->setValue(stepsDone);
    }
}

void Expend::updateScoreBars()
{
    // Update compact chart in score group (6 bars: TTFB / Gen / QA / Logic / Tools / Overall)
    auto scoreTTFB = [&](double ms) -> double
    {
        if (ms < 0) return 0.0;
        if (ms <= 500) return 100.0;
        if (ms >= 10000) return 0.0;
        return (10000.0 - ms) * 100.0 / (10000.0 - 500.0);
    };
    auto scoreGen = [&](double tps) -> double
    {
        if (tps < 0) return 0.0;
        if (tps >= 100.0) return 100.0;
        if (tps <= 0) return 0.0;
        return tps;
    };
    updateEvalSummary();
}

void Expend::updateEvalSummary(bool resetOnly)
{
    if (!ui) return;
    auto polish = [](QLabel *label, const QString &text, const char *state)
    {
        if (!label) return;
        label->setText(text);
        if (state)
            label->setProperty("state", QString::fromLatin1(state));
        else
            label->setProperty("state", QVariant());
        if (auto style = label->style())
        {
            style->unpolish(label);
            style->polish(label);
        }
        label->update();
    };

    if (resetOnly)
    {
        polish(ui->eval_summary_ttfb_value, QStringLiteral("-"), nullptr);
        polish(ui->eval_summary_gen_value, QStringLiteral("-"), nullptr);
        polish(ui->eval_summary_qa_value, QStringLiteral("-"), nullptr);
        polish(ui->eval_summary_logic_value, QStringLiteral("-"), nullptr);
        polish(ui->eval_summary_tool_value, QStringLiteral("-"), nullptr);
        polish(ui->eval_summary_sync_value, QStringLiteral("-"), nullptr);
        return;
    }

    auto fmtMs = [](double ms) -> QString
    {
        if (ms < 0) return QStringLiteral("-");
        if (ms < 1000.0)
            return QString::number(ms, 'f', ms < 100.0 ? 1 : 0) + QStringLiteral(" ms");
        return QString::number(ms / 1000.0, 'f', 2) + QStringLiteral(" s");
    };
    auto fmtSpeed = [&](double tokPerSec, double charPerSec) -> QString
    {
        if (tokPerSec > 0.0)
        {
            const int precision = tokPerSec >= 100.0 ? 0 : 1;
            return QString::number(tokPerSec, 'f', precision) + QStringLiteral(" tok/s");
        }
        if (charPerSec > 0.0)
        {
            const int precision = charPerSec >= 400.0 ? 0 : 1;
            return QString::number(charPerSec, 'f', precision) + QStringLiteral(" char/s");
        }
        return QStringLiteral("-");
    };
    auto fmtPercentage = [](double val) -> QString
    {
        if (val < 0) return QStringLiteral("-");
        return QString::number(val, 'f', 0) + QStringLiteral("%");
    };
    auto classifyTtfb = [](double ms) -> const char *
    {
        if (ms < 0) return nullptr;
        if (ms <= 500.0) return "good";
        if (ms <= 2000.0) return "warn";
        return "bad";
    };
    auto classifySpeed = [](double tokPerSec) -> const char *
    {
        if (tokPerSec < 0) return nullptr;
        if (tokPerSec >= 100.0) return "good";
        if (tokPerSec >= 60.0) return "warn";
        return "bad";
    };
    auto classifyPercent = [](double val, double good, double warn) -> const char *
    {
        if (val < 0) return nullptr;
        if (val >= good) return "good";
        if (val >= warn) return "warn";
        return "bad";
    };

    const double effectiveGenSpeed = (m_genTokPerSec > 0.0 ? m_genTokPerSec : (m_genCharsPerSec > 0.0 ? (m_genCharsPerSec / 4.0) : -1.0));

    polish(ui->eval_summary_ttfb_value, fmtMs(m_firstTokenMs), classifyTtfb(m_firstTokenMs));
    polish(ui->eval_summary_gen_value, fmtSpeed(m_genTokPerSec, m_genCharsPerSec), classifySpeed(effectiveGenSpeed));
    polish(ui->eval_summary_qa_value, fmtPercentage(m_qaScore), classifyPercent(m_qaScore, 80.0, 60.0));
    polish(ui->eval_summary_logic_value, fmtPercentage(m_logicScore), classifyPercent(m_logicScore, 80.0, 60.0));
    polish(ui->eval_summary_tool_value, fmtPercentage(m_toolScore), classifyPercent(m_toolScore, 90.0, 50.0));
    polish(ui->eval_summary_sync_value, fmtPercentage(m_syncRate), classifyPercent(m_syncRate, 85.0, 65.0));
}

void Expend::setValueColor(int row, const QString &nameKey, double val, const QString &metric)
{
    Q_UNUSED(nameKey);
    // Apply color thresholds to the "值" cell
    if (!ui || !ui->eval_table) return;
    QTableWidgetItem *cell = ui->eval_table->item(row, 3);
    if (!cell) return;
    auto setBg = [&](const QColor &c)
    { cell->setBackground(QBrush(c)); };
    const QColor good(180, 255, 180);
    const QColor warn(255, 235, 150);
    const QColor bad(255, 180, 180);
    QString key = metric;
    if (key.contains("首次响应") || key.contains(jtr("first token")))
    {
        if (m_firstTokenMs < 0) return;
        if (m_firstTokenMs <= 500)
            setBg(good);
        else if (m_firstTokenMs <= 2000)
            setBg(warn);
        else
            setBg(bad);
        return;
    }
    if (key.contains("生成速度") || key.contains(jtr("gen speed")))
    {
        if (val <= 0) return;
        if (val >= 100)
            setBg(good);
        else if (val >= 60)
            setBg(warn);
        else
            setBg(bad);
        return;
    }
    if (key.contains("常识问答") || key.contains(jtr("common qa")))
    {
        if (val < 0) return;
        if (val >= 80)
            setBg(good);
        else if (val >= 60)
            setBg(warn);
        else
            setBg(bad);
        return;
    }
    if (key.contains("逻辑推理") || key.contains(jtr("logic")))
    {
        if (val < 0) return;
        if (val >= 80)
            setBg(good);
        else if (val >= 60)
            setBg(warn);
        else
            setBg(bad);
        return;
    }
    if (key.contains("工具调用") || key.contains(jtr("tool call")))
    {
        if (val < 0) return;
        if (val >= 90)
            setBg(good);
        else if (val >= 50)
            setBg(warn);
        else
            setBg(bad);
        return;
    }
}
