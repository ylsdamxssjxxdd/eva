// Model evaluation tab: orchestrates one-click benchmarks against current endpoint
#include "expend.h"

#include "../prompt.h"
#include "../utils/devicemanager.h"
#include "../xnet.h"
#include "../xtool.h"

#include "ui_expend.h"

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

    ui->eval_mode_value->setText(eval_mode == LOCAL_MODE ? QStringLiteral("本地模式") : QStringLiteral("链接模式"));
    ui->eval_endpoint_value->setText(eval_apis.api_endpoint.isEmpty() ? QStringLiteral("-") : eval_apis.api_endpoint);
    // Model: local shows file name if set; link shows provider model id
    QString modelStr = eval_mode == LOCAL_MODE ? eval_settings.modelpath : eval_apis.api_model;
    if (modelStr.isEmpty()) modelStr = QStringLiteral("-");
    ui->eval_model_value->setText(modelStr);
    // Device and key runtime toggles (best-effort)
    ui->eval_device_value->setText(DeviceManager::effectiveBackend());
    ui->eval_nctx_value->setText(eval_settings.nctx > 0 ? QString::number(eval_settings.nctx) : QStringLiteral("-"));
    ui->eval_threads_value->setText(eval_settings.nthread > 0 ? QString::number(eval_settings.nthread) : QStringLiteral("-"));
}

void Expend::evalResetUi()
{
    // Initialize merged table: [指标/步骤 | 状态 | 用时(s) | 值 | 说明]
    if (!ui->eval_table) return;
    ui->eval_table->clearContents();
    ui->eval_table->setRowCount(5);
    ui->eval_table->setColumnCount(5);
    QStringList headers;
    headers << QStringLiteral("指标/步骤") << QStringLiteral("状态") << QStringLiteral("用时(s)")
            << QStringLiteral("值") << QStringLiteral("说明");
    ui->eval_table->setHorizontalHeaderLabels(headers);
    // Init rows with default status (5 steps)
    evalSetTable(0, QStringLiteral("首次响应(ms)"), QStringLiteral("-"), QStringLiteral("1024 字符上文，测首 token 时延"));
    evalSetTable(1, QStringLiteral("生成速度(tok/s)"), QStringLiteral("-"), QStringLiteral("生成 1024 字符，排除思考时间"));
    evalSetTable(2, QStringLiteral("常识问答(%)"), QStringLiteral("-"), QStringLiteral("5 题单选，A-D"));
    evalSetTable(3, QStringLiteral("逻辑推理(%)"), QStringLiteral("-"), QStringLiteral("5 题单选，A-D（较难）"));
    evalSetTable(4, QStringLiteral("工具调用(0-100)"), QStringLiteral("-"), QStringLiteral("6 项：计算器/文生图/知识库/工程师/MCP/鼠键"));
    for (int r = 0; r < 5; ++r) evalSetStatus(r, QStringLiteral("待开始"));

    ui->eval_log_plainTextEdit->clear();
    // Progress bar (units recomputed by start)
    if (ui->eval_progressBar) { ui->eval_progressBar->setMaximum(stepsUnitsTotal); ui->eval_progressBar->setValue(0); }
    // Reset chart if present
    if (ui->eval_bar_chart) ui->eval_bar_chart->setScores(-1, -1, -1, -1, -1, -1);
}

void Expend::evalSetTable(int row, const QString &name, const QString &val, const QString &desc)
{
    auto setItem = [&](int r, int c, const QString &text) {
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
    setItem(row, 4, desc);
    // Colorize value cell by metric threshold if possible
    setValueColor(row, name, val.toDouble(), name);
}

void Expend::evalSetStatus(int row, const QString &status)
{
    if (!ui || !ui->eval_table) return;
    QTableWidgetItem *it = ui->eval_table->item(row, 1);
    if (!it) { it = new QTableWidgetItem(); it->setFlags(it->flags() & ~Qt::ItemIsEditable); ui->eval_table->setItem(row, 1, it); }
    it->setText(status);
}

void Expend::evalSetElapsed(int row, double seconds)
{
    if (!ui || !ui->eval_table) return;
    QTableWidgetItem *it = ui->eval_table->item(row, 2);
    if (!it) { it = new QTableWidgetItem(); it->setFlags(it->flags() & ~Qt::ItemIsEditable); ui->eval_table->setItem(row, 2, it); }
    if (seconds >= 0) it->setText(QString::number(seconds, 'f', 2));
    else it->setText(QString());
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
    d.stopwords = QStringList();
    d.id_slot = -1;
    return d;
}

QJsonArray Expend::makeMsgs(const QString &sys, const QString &user)
{
    QJsonArray arr;
    QJsonObject s; s.insert("role", DEFAULT_SYSTEM_NAME); s.insert("content", sys);
    QJsonObject u; u.insert("role", DEFAULT_USER_NAME);   u.insert("content", user);
    arr.append(s);
    arr.append(u);
    return arr;
}

void Expend::on_eval_start_pushButton_clicked()
{
    if (evalRunning)
    {
        evalLog(QStringLiteral("eval: 已在运行"));
        return;
    }
    ensureEvalNet();
    // Best-effort: fill endpoint for LOCAL_MODE if missing
    if (eval_apis.api_endpoint.trimmed().isEmpty())
    {
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        const QString port = settings.value("port", DEFAULT_SERVER_PORT).toString();
        eval_apis.api_endpoint = QString("http://127.0.0.1:%1").arg(port);
    }
    // Plan fine-grained steps for progress (latency + gen + qa + logic + tools)
    // Initialize tool test cases once per run
    toolCases_.clear();
    toolCases_.push_back({QStringLiteral("calculator"), QStringLiteral("Use the calculator tool to compute 13*17+29 and return the result."), QStringLiteral("计算器: 13*17+29")});
    toolCases_.push_back({QStringLiteral("stablediffusion"), QStringLiteral("Draw an image of 'a cute orange cat sitting on a desk' using the stablediffusion tool."), QStringLiteral("文生图: cat image")});
    toolCases_.push_back({QStringLiteral("knowledge"), QStringLiteral("Query the knowledge base with the keyword 'EVA BACKEND layout' using the knowledge tool."), QStringLiteral("知识库: EVA BACKEND layout")});
    toolCases_.push_back({QStringLiteral("execute_command"), QStringLiteral("List files in the current directory using the execute_command tool (use an appropriate command for this OS)."), QStringLiteral("工程师: 列目录")});
    toolCases_.push_back({QStringLiteral("mcp_tools_list"), QStringLiteral("List available MCP tools using the mcp_tools_list tool."), QStringLiteral("MCP: 列出工具")});
    toolCases_.push_back({QStringLiteral("controller"), QStringLiteral("Simulate a simple double-click at (100,200) using the controller tool."), QStringLiteral("鼠键: 双击(100,200)")});

    stepsUnitsTotal = 1 /*latency*/ + 1 /*gen*/ + qaPlanned_ /*qa*/ + logicPlanned_ /*logic*/ + toolCases_.size() /*tools*/;
    evalResetUi();
    updateEvalInfoUi();
    evalRunning = true;
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
    // reset per-run indices
    qaIndex_ = 0; qaCorrect_ = 0;
    logicIndex_ = 0; logicCorrect_ = 0;
    toolIndex_ = 0; toolCorrect_ = 0;
    genCounting_ = false; genStartNsRel_ = 0;
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

void Expend::on_eval_stop_pushButton_clicked()
{
    if (!evalRunning) return;
    // Abort active eval request if any
    if (evalNet)
    {
        QMetaObject::invokeMethod(evalNet, "recv_stop", Qt::QueuedConnection, Q_ARG(bool, true));
    }
    evalRunning = false;
    evalLog(QStringLiteral("eval: 已停止"));
}

void Expend::evalNext()
{
    if (!evalRunning) return;
    switch (evalStep)
    {
    case 0: runLatencyTest(); break;
    case 1: runGenSpeedTest(); break;
    case 2: runQATest(); break;        // Common-sense MC
    case 3: runLogicTest(); break;     // Logical MC
    case 4: runToolcallTest(); break;  // Tools multi-cases
    default: evalFinish(); break;
    }
}

void Expend::runLatencyTest()
{
    evalLog(QStringLiteral("[1/5] 首次响应：发送 1024 个 'A' 字符，测首 token 时延"));
    evalSetStatus(0, QStringLiteral("进行中"));
    ENDPOINT_DATA d = makeBaseData(0.2, 16);
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
    evalLog(QStringLiteral("[2/5] 生成速度：生成 1024 个字符（排除<think>时间）"));
    evalSetStatus(1, QStringLiteral("进行中"));
    ENDPOINT_DATA d = makeBaseData(0.0, 1024);
    const QString ask = QStringLiteral("请输出恰好 1024 个小写字母 a，不要包含空格、换行或其他字符。仅输出 a 字符串。Strictly output 1024 'a'.");
    d.messagesArray = makeMsgs(QStringLiteral("You are a helpful assistant. Reply strictly as asked."), ask);
    evalFirstToken = false;
    evalAccum.clear();
    evalTimer.restart();
    stepTimer.restart();
    genCounting_ = false; genStartNsRel_ = 0;
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
        qaPairs_.push_back({QStringLiteral("常识问答1：法国的首都是哪一座城市?\nA) 柏林\nB) 巴黎\nC) 马德里\nD) 罗马\n仅输出 A/B/C/D。"), QStringLiteral("b")});
        qaPairs_.push_back({QStringLiteral("常识问答2：水的化学式是?\nA) CO2\nB) H2O\nC) O2\nD) NaCl\n仅输出 A/B/C/D。"), QStringLiteral("b")});
        qaPairs_.push_back({QStringLiteral("常识问答3：地球绕太阳一周大约需要?\nA) 24 小时\nB) 7 天\nC) 365 天\nD) 12 小时\n仅输出 A/B/C/D。"), QStringLiteral("c")});
        qaPairs_.push_back({QStringLiteral("常识问答4：'傲慢与偏见'的作者是谁?\nA) 奥斯汀\nB) 雪莱\nC) 狄更斯\nD) 莎士比亚\n仅输出 A/B/C/D。"), QStringLiteral("a")});
        qaPairs_.push_back({QStringLiteral("常识问答5：太阳从哪个方向升起?\nA) 南\nB) 北\nC) 西\nD) 东\n仅输出 A/B/C/D。"), QStringLiteral("d")});
        qaIndex_ = 0; qaCorrect_ = 0;
        evalLog(QStringLiteral("[3/5] 常识问答 (5 题，单选)"));
    }
    if (qaIndex_ >= qaPairs_.size())
    {
        // Finalize score
        m_qaScore = (qaPairs_.isEmpty() ? 0.0 : (100.0 * double(qaCorrect_) / double(qaPairs_.size())));
        evalSetTable(2, QStringLiteral("常识问答(%)"), QString::number(m_qaScore, 'f', 1), QStringLiteral("命中率"));
        evalStep++;
        evalNext();
        return;
    }
    // One question per turn
    const auto &p = qaPairs_[qaIndex_];
    if (qaIndex_ == 0) evalSetStatus(2, QStringLiteral("进行中 0/") + QString::number(qaPlanned_));
    ENDPOINT_DATA d = makeBaseData(0.1, 64);
    d.messagesArray = makeMsgs(QStringLiteral("You are a concise assistant. Reply with a single letter A/B/C/D only."), p.first);
    evalFirstToken = false;
    evalAccum.clear();
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
        logicPairs_.push_back({QStringLiteral("逻辑推理1：数列 2, 6, 12, 20, ? 的下一个数是?\nA) 28\nB) 30\nC) 32\nD) 34\n仅输出 A/B/C/D。"), QStringLiteral("b")}); // pattern +4,+6,+8,+10
        logicPairs_.push_back({QStringLiteral("逻辑推理2：一个正六边形的对角线条数为?\nA) 6\nB) 9\nC) 12\nD) 15\n仅输出 A/B/C/D。"), QStringLiteral("c")}); // n(n-3)/2 = 9 ? Wait hexagon: 6*3/2=9; but options; correct is B=9. Fix to b
        logicPairs_.last().second = QStringLiteral("b");
        logicPairs_.push_back({QStringLiteral("逻辑推理3：在1到100的整数中，数字9出现的次数是?\nA) 18\nB) 19\nC) 20\nD) 21\n仅输出 A/B/C/D。"), QStringLiteral("c")}); // 20
        logicPairs_.push_back({QStringLiteral("逻辑推理4：一个两位数，十位与个位之和为9，且该数是9的倍数，该数是?\nA) 18\nB) 27\nC) 45\nD) 54\n仅输出 A/B/C/D。"), QStringLiteral("d")}); // 54 (6+? actually 5+4=9 and 54 divisible by 9)
        logicPairs_.push_back({QStringLiteral("逻辑推理5：四个连着的整数乘积加一，一定是?\nA) 合数\nB) 质数\nC) 完全平方数\nD) 不能确定\n仅输出 A/B/C/D。"), QStringLiteral("b")}); // often prime? Actually n(n+1)(n+2)(n+3)+1 is not guaranteed prime; Known for n=1 gives 25 not prime. So D is correct. Fix to D.
        logicPairs_.last().second = QStringLiteral("d");
        logicIndex_ = 0; logicCorrect_ = 0;
        evalLog(QStringLiteral("[4/5] 逻辑推理 (5 题，单选)"));
    }
    if (logicIndex_ >= logicPairs_.size())
    {
        m_logicScore = (logicPairs_.isEmpty() ? 0.0 : (100.0 * double(logicCorrect_) / double(logicPairs_.size())));
        evalSetTable(3, QStringLiteral("逻辑推理(%)"), QString::number(m_logicScore, 'f', 1), QStringLiteral("命中率"));
        evalStep++;
        evalNext();
        return;
    }
    const auto &p = logicPairs_[logicIndex_];
    if (logicIndex_ == 0) evalSetStatus(3, QStringLiteral("进行中 0/") + QString::number(logicPlanned_));
    ENDPOINT_DATA d = makeBaseData(0.1, 64);
    d.messagesArray = makeMsgs(QStringLiteral("You are a concise assistant. Reply with a single letter A/B/C/D only."), p.first);
    evalFirstToken = false;
    evalAccum.clear();
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
        evalLog(QStringLiteral("[5/5] 工具调用能力 (6 项)"));
        evalSetStatus(4, QStringLiteral("进行中 0/") + QString::number(toolCases_.size()));
    }
    if (toolIndex_ >= toolCases_.size())
    {
        // Finalize tool score
        const int total = qMax(1, toolCases_.size());
        m_toolScore = 100.0 * double(toolCorrect_) / double(total);
        evalSetTable(4, QStringLiteral("工具调用(0-100)"), QString::number(m_toolScore, 'f', 0), QStringLiteral("六项综合"));
        evalSetStatus(4, QStringLiteral("完成"));
        evalSetElapsed(4, stepTimer.nsecsElapsed()/1e9);
        evalStep++;
        evalNext();
        return;
    }
    // Prepare one tool case
    const ToolCase &tc = toolCases_[toolIndex_];
    QString toolsDesc = Buildin_tools_answer.text + QString("\n\n")
                        + Buildin_tools_calculator.text + QString("\n\n")
                        + Buildin_tools_stablediffusion.text + QString("\n\n")
                        + Buildin_tools_knowledge.text + QString("\n\n")
                        + Buildin_tools_execute_command.text + QString("\n\n")
                        + Buildin_tools_controller.text + QString("\n\n")
                        + Buildin_tools_mcp_tools_list.text;
    QString sys = EXTRA_PROMPT_FORMAT;
    sys.replace("{available_tools_describe}", toolsDesc);
    sys.replace("{engineer_info}", QString());
    const QString task = tc.user + QStringLiteral(" Strictly output exactly one <tool_call> JSON and stop.");

    ENDPOINT_DATA d = makeBaseData(0.2, 64);
    d.messagesArray = makeMsgs(sys, task);
    evalFirstToken = false;
    evalAccum.clear();
    if (toolIndex_ == 0) stepTimer.restart();
    evalTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::evalFinish()
{
    // Weighted overall score per spec: 10% TTFB, 20% Gen, 20% Common QA, 20% Logic, 30% Tools
    auto scoreTTFB = [&](double ms) {
        if (ms < 0) return 0.0; if (ms <= 500.0) return 100.0; if (ms >= 10000.0) return 0.0; return (10000.0 - ms) * 100.0 / (10000.0 - 500.0);
    };
    auto scoreGen = [&](double tokps) {
        if (tokps < 0) return 0.0; if (tokps >= 100.0) return 100.0; if (tokps <= 0.0) return 0.0; return tokps; /* linear to 100 */ };
    const double s_ttfb = scoreTTFB(m_firstTokenMs);
    const double s_gen  = scoreGen(m_genTokPerSec);
    const double s_qa   = qMax(0.0, m_qaScore);
    const double s_log  = qMax(0.0, m_logicScore);
    const double s_tool = qMax(0.0, m_toolScore);
    m_syncRate = std::max(0.0, std::min(100.0, 0.10 * s_ttfb + 0.20 * s_gen + 0.20 * s_qa + 0.20 * s_log + 0.30 * s_tool));
    // Do not show overall score in the table; only log it and reflect on bar chart
    evalLog(QStringLiteral("评估完成。综合(10/20/20/20/30)= ") + QString::number(m_syncRate, 'f', 1));
    evalRunning = false;
    updateScoreBars();
}

// ---------------- evalNet signal handlers ----------------

void Expend::onEvalOutput(const QString &text, bool streaming, QColor)
{
    Q_UNUSED(streaming);
    if (!evalRunning) return;
    const QString chunk = stripThink(text);
    if (!evalFirstToken && !chunk.trimmed().isEmpty())
    {
        evalFirstToken = true;
        const double ms = evalTimer.isValid() ? (evalTimer.nsecsElapsed() / 1e6) : -1.0;
        if (evalStep == 0)
        {
            m_firstTokenMs = ms;
            evalSetTable(0, QStringLiteral("首次响应(ms)"), QString::number(m_firstTokenMs, 'f', 1));
            updateScoreBars();
        }
        else if (evalStep == 1)
        {
            // Start counting gen speed excluding any preceding <think>
            genCounting_ = true;
            genStartNsRel_ = stepTimer.nsecsElapsed();
        }
        // Log per-step ttfb briefly
        evalLog(QStringLiteral("ttfb: ") + QString::number(ms, 'f', 1) + QStringLiteral(" ms"));
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
        if (evalStep == 0) r = 0; else if (evalStep == 1) r = 1; else if (evalStep == 2) r = 2; else if (evalStep == 3) r = 3; else /*evalStep>=4*/ r = 4;
        evalSetStatus(r, QStringLiteral("错误"));
        evalSetElapsed(r, stepTimer.nsecsElapsed()/1e9);
        evalRunning = false;
        evalFinish();
    }
}

void Expend::onEvalSpeeds(double prompt_per_s, double gen_per_s)
{
    if (!evalRunning) return;
    if (prompt_per_s > 0)
    {
        m_promptTokPerSec = prompt_per_s;
        // no explicit row for prompt speed in the new rubric
    }
    if (gen_per_s > 0)
    {
        m_genTokPerSec = gen_per_s;
        evalSetTable(1, QStringLiteral("生成速度(tok/s)"), QString::number(m_genTokPerSec, 'f', 1));
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
        // Nothing else; rely on server-reported speeds if any
        // Log model's actual answer
        evalLog(QStringLiteral("[首次响应] 模型回答：\n") + evalAccum);
        // Mark step 1 done
        evalSetStatus(0, QStringLiteral("完成"));
        evalSetElapsed(0, stepTimer.nsecsElapsed()/1e9);
        stepsDone++;
        evalUpdateProgress();
        updateScoreBars();
        evalStep++;
        evalNext();
        break;
    case 1:
    {
        // Derive char/s as a fallback
        double tSec = evalTimer.isValid() ? (evalTimer.nsecsElapsed() / 1e9) : -1.0;
        // Exclude think time: use relative start captured at first non-think output
        if (genCounting_ && stepTimer.isValid())
        {
            const qint64 ns = stepTimer.nsecsElapsed();
            if (ns > genStartNsRel_) tSec = double(ns - genStartNsRel_) / 1e9;
        }
        if (tSec > 0)
        {
            const int n = evalAccum.size();
            const double cps = double(n) / tSec;
            m_genCharsPerSec = cps;
            // If server did not report token-speed, keep a hint in desc
            QString desc = (m_genTokPerSec > 0) ? QStringLiteral("服务器报告") : QStringLiteral("估算: ") + QString::number(cps, 'f', 1) + QStringLiteral(" chars/s");
            evalSetTable(1, QStringLiteral("生成速度(tok/s)"), (m_genTokPerSec > 0 ? QString::number(m_genTokPerSec, 'f', 1) : QStringLiteral("-")), desc);
            // If no token speed, derive a rough estimate from chars/s to drive the bar
            if (m_genTokPerSec <= 0 && cps > 0)
            {
                const double estTokPerSec = cps / 4.0; // heuristic: ~4 chars per token
                m_genTokPerSec = estTokPerSec;
            }
            updateScoreBars();
        }
        // Log actual generated content
        evalLog(QStringLiteral("[生成速度] 生成内容(长度=") + QString::number(evalAccum.size()) + QStringLiteral(")：\n") + evalAccum);
        evalSetStatus(1, QStringLiteral("完成"));
        evalSetElapsed(1, stepTimer.nsecsElapsed()/1e9);
        stepsDone++;
        evalUpdateProgress();
        evalStep++;
        evalNext();
        break;
    }
    case 2:
    {
        // Judge this QA item (MC A-D)
        const QString ansRaw = evalAccum.trimmed();
        const QChar pick = parseMCAnswer(ansRaw);
        const QString expect = qaPairs_[qaIndex_].second;
        const QString question = qaPairs_[qaIndex_].first;
        const bool ok = (!expect.isEmpty() && !pick.isNull() && QString(pick).toLower() == expect);
        if (ok) qaCorrect_++;
        // Log this Q/A clearly
        evalLog(QStringLiteral("[常识问答] 题目(") + QString::number(qaIndex_ + 1) + ")：\n" + question +
                QStringLiteral("\n标准答案：") + expect.toUpper() +
                QStringLiteral("\n模型回答：") + (pick.isNull()? ansRaw : QString(pick).toUpper()) +
                QStringLiteral("\n判定：") + (ok ? QStringLiteral("正确") : QStringLiteral("错误")));
        qaIndex_++;
        stepsDone++; // count each QA as a progress unit
        evalSetStatus(2, QStringLiteral("进行中 ") + QString::number(std::min(qaIndex_, qaPlanned_)) + "/" + QString::number(qaPlanned_));
        evalUpdateProgress();
        // Next QA or finish QA stage
        runQATest();
        if (qaIndex_ >= qaPairs_.size()) {
            evalSetStatus(2, QStringLiteral("完成"));
            evalSetElapsed(2, stepTimer.nsecsElapsed()/1e9);
            updateScoreBars();
        }
        break;
    }
    case 3:
    {
        // Judge logic MC
        const QString ansRaw = evalAccum.trimmed();
        const QChar pick = parseMCAnswer(ansRaw);
        const QString expect = logicPairs_[logicIndex_].second;
        const QString question = logicPairs_[logicIndex_].first;
        const bool ok = (!expect.isEmpty() && !pick.isNull() && QString(pick).toLower() == expect);
        if (ok) logicCorrect_++;
        evalLog(QStringLiteral("[逻辑推理] 题目(") + QString::number(logicIndex_ + 1) + ")：\n" + question +
                QStringLiteral("\n标准答案：") + expect.toUpper() +
                QStringLiteral("\n模型回答：") + (pick.isNull()? ansRaw : QString(pick).toUpper()) +
                QStringLiteral("\n判定：") + (ok ? QStringLiteral("正确") : QStringLiteral("错误")));
        logicIndex_++;
        stepsDone++;
        evalSetStatus(3, QStringLiteral("进行中 ") + QString::number(std::min(logicIndex_, logicPlanned_)) + "/" + QString::number(logicPlanned_));
        evalUpdateProgress();
        runLogicTest();
        if (logicIndex_ >= logicPairs_.size()) {
            evalSetStatus(3, QStringLiteral("完成"));
            evalSetElapsed(3, stepTimer.nsecsElapsed()/1e9);
            updateScoreBars();
        }
        break;
    }
    case 4:
    {
        // Tools: evaluate current case then proceed to next
        const QString all = evalAccum;
        int s = all.indexOf("<tool_call>");
        int e = all.indexOf("</tool_call>");
        bool ok = false;
        QString jsonStr;
        if (s >= 0 && e > s)
        {
            jsonStr = all.mid(s + 11, e - (s + 11)).trimmed();
            try {
                mcp::json call = mcp::json::parse(jsonStr.toStdString());
                const QString name = QString::fromStdString(get_string_safely(call, "name"));
                ok = (name == toolCases_[toolIndex_].name);
            } catch (...) { ok = false; }
        }
        toolCorrect_ += ok ? 1 : 0;
        evalLog(QStringLiteral("[工具调用]") + QString(" (%1/%2) ").arg(toolIndex_+1).arg(toolCases_.size()) + toolCases_[toolIndex_].desc +
                QStringLiteral("\n模型输出：\n") + (jsonStr.isEmpty()? all : jsonStr) +
                QStringLiteral("\n判定：") + (ok ? QStringLiteral("正确工具") : QStringLiteral("错误或缺失")));
        // Update partial tool score to refresh bar chart
        const int total = qMax(1, (int)toolCases_.size());
        m_toolScore = 100.0 * double(toolCorrect_) / double(total);
        updateScoreBars();
        toolIndex_++;
        stepsDone++;
        evalSetStatus(4, QStringLiteral("进行中 ") + QString::number(std::min(toolIndex_, toolCases_.size())) + "/" + QString::number(toolCases_.size()));
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
    if (ui->eval_progressBar) { ui->eval_progressBar->setMaximum(stepsUnitsTotal); ui->eval_progressBar->setValue(stepsDone); }
}

void Expend::updateScoreBars()
{
    // Update compact chart in score group (6 bars: TTFB / Gen / QA / Logic / Tools / Overall)
    auto scoreTTFB = [&](double ms) {
        if (ms < 0) return 0.0; if (ms <= 500) return 100.0; if (ms >= 10000) return 0.0; return (10000.0 - ms) * 100.0 / (10000.0 - 500.0); };
    auto scoreGen = [&](double tps) { if (tps < 0) return 0.0; if (tps >= 100.0) return 100.0; if (tps <= 0) return 0.0; return tps; };
    const double s1 = (m_firstTokenMs >= 0 ? scoreTTFB(m_firstTokenMs) : -1);
    const double s2 = (m_genTokPerSec >= 0 ? scoreGen(m_genTokPerSec) : (m_genCharsPerSec > 0 ? scoreGen(m_genCharsPerSec/4.0) : -1));
    const double s3 = (m_qaScore >= 0 ? m_qaScore : -1);
    const double s4 = (m_logicScore >= 0 ? m_logicScore : -1);
    const double s5 = (m_toolScore >= 0 ? m_toolScore : -1);
    const double s6 = (m_syncRate >= 0 ? m_syncRate : -1);
    if (ui->eval_bar_chart) ui->eval_bar_chart->setScores(s1, s2, s3, s4, s5, s6);
}

void Expend::setValueColor(int row, const QString &nameKey, double val, const QString &metric)
{
    // Apply color thresholds to the "值" cell
    if (!ui || !ui->eval_table) return;
    QTableWidgetItem *cell = ui->eval_table->item(row, 3);
    if (!cell) return;
    auto setBg = [&](const QColor &c){ cell->setBackground(QBrush(c)); };
    const QColor good(180, 255, 180);
    const QColor warn(255, 235, 150);
    const QColor bad(255, 180, 180);
    QString key = metric;
    if (key.contains("首次响应"))
    {
        if (m_firstTokenMs < 0) return;
        if (m_firstTokenMs <= 500) setBg(good);
        else if (m_firstTokenMs <= 2000) setBg(warn);
        else setBg(bad);
        return;
    }
    if (key.contains("生成速度"))
    {
        if (val <= 0) return;
        if (val >= 100) setBg(good);
        else if (val >= 60) setBg(warn);
        else setBg(bad);
        return;
    }
    if (key.contains("常识问答"))
    {
        if (val < 0) return;
        if (val >= 80) setBg(good);
        else if (val >= 60) setBg(warn);
        else setBg(bad);
        return;
    }
    if (key.contains("逻辑推理"))
    {
        if (val < 0) return;
        if (val >= 80) setBg(good);
        else if (val >= 60) setBg(warn);
        else setBg(bad);
        return;
    }
    if (key.contains("工具调用"))
    {
        if (val < 0) return;
        if (val >= 90) setBg(good);
        else if (val >= 50) setBg(warn);
        else setBg(bad);
        return;
    }
}
