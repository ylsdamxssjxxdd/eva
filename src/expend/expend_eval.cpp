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
    ui->eval_table->setRowCount(6);
    ui->eval_table->setColumnCount(5);
    QStringList headers;
    headers << QStringLiteral("指标/步骤") << QStringLiteral("状态") << QStringLiteral("用时(s)")
            << QStringLiteral("值") << QStringLiteral("说明");
    ui->eval_table->setHorizontalHeaderLabels(headers);
    // Init rows with default status
    evalSetTable(0, QStringLiteral("首次响应(ms)"), QStringLiteral("-"), QStringLiteral("固定上文长度测 TTFB"));
    evalSetTable(1, QStringLiteral("上文处理(tok/s)"), QStringLiteral("-"), QStringLiteral("服务器报告"));
    evalSetTable(2, QStringLiteral("生成速度(tok/s)"), QStringLiteral("-"), QStringLiteral("服务器报告或估算"));
    evalSetTable(3, QStringLiteral("常识问答(%)"), QStringLiteral("-"), QStringLiteral("命中率"));
    evalSetTable(4, QStringLiteral("工具调用"), QStringLiteral("-"), QStringLiteral("calculator") );
    evalSetTable(5, QStringLiteral("同步率(%)"), QStringLiteral("-"), QStringLiteral("正确项/总项") );
    for (int r = 0; r < 6; ++r) evalSetStatus(r, QStringLiteral("待开始"));

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
    // Plan fine-grained steps for progress
    stepsUnitsTotal = 1 /*latency*/ + 1 /*gen*/ + qaPlanned_ /*qa*/ + 1 /*tool*/;
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
    m_toolScore = -1.0;
    m_syncRate = -1.0;
    evalUpdateProgress();
    evalNext();
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
    case 2: runQATest(); break;
    case 3: runToolcallTest(); break;
    default: evalFinish(); break;
    }
}

void Expend::runLatencyTest()
{
    evalLog(QStringLiteral("[1/4] 测试首次响应延迟与上文处理速度"));
    // Mark step 1 running
    evalSetStatus(0, QStringLiteral("进行中"));
    ENDPOINT_DATA d = makeBaseData(0.2, 16);
    const QString bigUser = QString(2000, QLatin1Char('A')) + QStringLiteral("\n请回答 ok");
    d.messagesArray = makeMsgs(QStringLiteral("You are a helpful assistant."), bigUser);
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
    evalLog(QStringLiteral("[2/4] 测试文字生成速度"));
    evalSetStatus(2, QStringLiteral("进行中"));
    ENDPOINT_DATA d = makeBaseData(0.0, 512);
    const QString ask = QStringLiteral("请输出恰好 512 个小写字母 a，不要包含空格、换行或其他字符。仅输出 a 字符串。");
    d.messagesArray = makeMsgs(QStringLiteral("You are a helpful assistant. Reply strictly as asked."), ask);
    evalFirstToken = false;
    evalAccum.clear();
    evalTimer.restart();
    stepTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::runQATest()
{
    if (qaPairs_.isEmpty())
    {
        // Initialize a tiny common-sense set
        qaPairs_.clear();
        qaPairs_.push_back({QStringLiteral("What is the capital of France? Respond in one word."), QStringLiteral("paris")});
        qaPairs_.push_back({QStringLiteral("2+2*3 = ? Respond with a number only."), QStringLiteral("8")});
        qaPairs_.push_back({QStringLiteral("Who wrote 'Pride and Prejudice'?"), QStringLiteral("austen")});
        qaPairs_.push_back({QStringLiteral("Does the Sun rise in the east or west? Answer east/west."), QStringLiteral("east")});
        qaPairs_.push_back({QStringLiteral("What is the chemical formula of water?"), QStringLiteral("h2o")});
        qaIndex_ = 0;
        qaCorrect_ = 0;
        evalLog(QStringLiteral("[3/4] 常识问答 (5 题)"));
    }
    if (qaIndex_ >= qaPairs_.size())
    {
        // Finalize score
        m_qaScore = (qaPairs_.isEmpty() ? 0.0 : (100.0 * double(qaCorrect_) / double(qaPairs_.size())));
        evalSetTable(3, QStringLiteral("常识问答(%)"), QString::number(m_qaScore, 'f', 1), QStringLiteral("命中率"));
        evalStep++;
        evalNext();
        return;
    }
    // One question per turn
    const auto &p = qaPairs_[qaIndex_];
    if (qaIndex_ == 0) {
        evalSetStatus(3, QStringLiteral("进行中 0/") + QString::number(qaPlanned_));
    }
    ENDPOINT_DATA d = makeBaseData(0.1, 64);
    d.messagesArray = makeMsgs(QStringLiteral("You are a concise assistant. Reply briefly."), p.first);
    evalFirstToken = false;
    evalAccum.clear();
    evalTimer.restart();
    if (qaIndex_ == 0) stepTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::runToolcallTest()
{
    evalLog(QStringLiteral("[4/4] 工具调用能力 (calculator)"));
    evalSetStatus(4, QStringLiteral("进行中"));
    // Build a minimal tool schema prompt: answer + calculator
    QString toolsDesc = Buildin_tools_answer.text + QString("\n\n") + Buildin_tools_calculator.text;
    QString sys = EXTRA_PROMPT_FORMAT;
    sys.replace("{available_tools_describe}", toolsDesc);
    sys.replace("{engineer_info}", QString()); // no engineer tools here
    const QString task = QStringLiteral("Use the 'calculator' tool to compute 13*17+29 and return only the final result as a number."
                                      " Strictly output a <tool_call> JSON for the call, then stop.");

    ENDPOINT_DATA d = makeBaseData(0.1, 64);
    d.messagesArray = makeMsgs(sys, task);
    evalFirstToken = false;
    evalAccum.clear();
    evalTimer.restart();
    stepTimer.restart();
    QMetaObject::invokeMethod(evalNet, "recv_apis", Qt::QueuedConnection, Q_ARG(APIS, eval_apis));
    QMetaObject::invokeMethod(evalNet, "recv_data", Qt::QueuedConnection, Q_ARG(ENDPOINT_DATA, d));
    QMetaObject::invokeMethod(evalNet, "run", Qt::QueuedConnection);
}

void Expend::evalFinish()
{
    // Compute sync rate = correct items (QA + tool pass) / total judged items
    const int qaTotal = qaPairs_.size();
    int correct = qaCorrect_;
    if (m_toolScore >= 90.0) correct += 1; // tool considered pass when >=90
    const int denom = (qaTotal > 0 ? qaTotal : qaPlanned_) + 1; // +1 for tool
    if (denom > 0)
        m_syncRate = std::max(0.0, std::min(100.0, 100.0 * double(correct) / double(denom)));
    else
        m_syncRate = 0.0;
    evalSetTable(5, QStringLiteral("同步率(%)"), QString::number(m_syncRate, 'f', 1), QString());
    evalLog(QStringLiteral("评估完成"));
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
        if (evalStep == 0) r = 0; else if (evalStep == 1) r = 2; else if (evalStep == 2) r = 3; else if (evalStep == 3) r = 4; else r = 5;
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
        evalSetTable(1, QStringLiteral("上文处理(tok/s)"), QString::number(m_promptTokPerSec, 'f', 1));
        updateScoreBars();
    }
    if (gen_per_s > 0)
    {
        m_genTokPerSec = gen_per_s;
        evalSetTable(2, QStringLiteral("生成速度(tok/s)"), QString::number(m_genTokPerSec, 'f', 1));
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
        evalLog(QStringLiteral("[首次响应/上文速度] 模型回答：\n") + evalAccum);
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
        const double tSec = evalTimer.isValid() ? (evalTimer.nsecsElapsed() / 1e9) : -1.0;
        if (tSec > 0)
        {
            const int n = evalAccum.size();
            const double cps = double(n) / tSec;
            m_genCharsPerSec = cps;
            // If server did not report token-speed, keep a hint in desc
            QString desc = (m_genTokPerSec > 0) ? QStringLiteral("服务器报告") : QStringLiteral("估算: ") + QString::number(cps, 'f', 1) + QStringLiteral(" chars/s");
            evalSetTable(2, QStringLiteral("生成速度(tok/s)"), (m_genTokPerSec > 0 ? QString::number(m_genTokPerSec, 'f', 1) : QStringLiteral("-")), desc);
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
        evalSetStatus(2, QStringLiteral("完成"));
        evalSetElapsed(2, stepTimer.nsecsElapsed()/1e9);
        stepsDone++;
        evalUpdateProgress();
        evalStep++;
        evalNext();
        break;
    }
    case 2:
    {
        // Judge this QA item
        const QString ansRaw = evalAccum.trimmed();
        const QString ans = ansRaw.toLower();
        const QString expect = qaPairs_[qaIndex_].second;
        const QString question = qaPairs_[qaIndex_].first;
        const bool ok = (!expect.isEmpty() && ans.contains(expect));
        if (ok) qaCorrect_++;
        // Log this Q/A clearly
        evalLog(QStringLiteral("[常识问答] 题目(") + QString::number(qaIndex_ + 1) + ")：" + question +
                QStringLiteral("\n标准答案：") + expect +
                QStringLiteral("\n模型回答：\n") + ansRaw +
                QStringLiteral("\n判定：") + (ok ? QStringLiteral("正确") : QStringLiteral("错误")));
        qaIndex_++;
        stepsDone++; // count each QA as a progress unit
        evalSetStatus(3, QStringLiteral("进行中 ") + QString::number(std::min(qaIndex_, qaPlanned_)) + "/" + QString::number(qaPlanned_));
        evalUpdateProgress();
        // Next QA or finish QA stage
        runQATest();
        if (qaIndex_ >= qaPairs_.size()) {
            evalSetStatus(3, QStringLiteral("完成"));
            evalSetElapsed(3, stepTimer.nsecsElapsed()/1e9);
            updateScoreBars();
        }
        break;
    }
    case 3:
    {
        // Extract tool_call JSON and evaluate execution using xTool
        m_toolScore = 0.0;
        // Find first <tool_call>...</tool_call>
        const QString all = evalAccum;
        int s = all.indexOf("<tool_call>");
        int e = all.indexOf("</tool_call>");
        if (s >= 0 && e > s)
        {
            const QString jsonStr = all.mid(s + 11, e - (s + 11)).trimmed();
            evalLog(QStringLiteral("[工具调用] 模型返回工具请求 JSON：\n") + jsonStr);
            try
            {
                mcp::json call = mcp::json::parse(jsonStr.toStdString());
                const std::string name = get_string_safely(call, "name");
                if (name == std::string("calculator"))
                {
                    xTool tool(applicationDirPath);
                    // Capture result via a one-shot connection
                    QString result;
                    QEventLoop loop;
                    QObject::connect(&tool, &xTool::tool2ui_pushover, &loop, [&](const QString &r) { result = r; loop.quit(); });
                    tool.Exec(call);
                    // Some tools emit synchronously; ensure loop finishes quickly
                    QTimer::singleShot(100, &loop, [&]() { if (loop.isRunning()) loop.quit(); });
                    loop.exec();
                    // Expect 13*17+29 = 250
                    const bool ok = result.contains("250");
                    m_toolScore = ok ? 100.0 : 50.0; // half credit for structured call
                    evalLog(QStringLiteral("[工具调用] 标准答案：250\n工具返回：\n") + result +
                            QStringLiteral("\n判定：") + (ok ? QStringLiteral("正确") : QStringLiteral("错误")));
                }
                else
                {
                    m_toolScore = 20.0; // wrong tool
                    evalLog(QStringLiteral("[工具调用] 错误：调用了错误的工具 name=") + QString::fromStdString(name));
                }
            }
            catch (...)
            {
                m_toolScore = 0.0;
                evalLog(QStringLiteral("[工具调用] 错误：解析 <tool_call> JSON 失败"));
            }
        }
        else
        {
            m_toolScore = 0.0;
            evalLog(QStringLiteral("[工具调用] 错误：未找到 <tool_call> 标签"));
        }
        evalSetTable(4, QStringLiteral("工具调用"), QString::number(m_toolScore, 'f', 0), QStringLiteral("calculator"));
        evalSetStatus(4, QStringLiteral("完成"));
        evalSetElapsed(4, stepTimer.nsecsElapsed()/1e9);
        stepsDone++;
        evalUpdateProgress();
        updateScoreBars();
        evalStep++;
        evalNext();
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
    // Update compact chart in score group
    auto scoreTTFB = [&](double ms) {
        if (ms < 0) return 0.0; if (ms <= 600) return 100.0; if (ms <= 1200) return 85.0; if (ms <= 2500) return 70.0; if (ms <= 4000) return 50.0; return 30.0; };
    auto scoreTokps = [&](double tps) {
        if (tps < 0) return 0.0; if (tps >= 250) return 100.0; if (tps >= 150) return 85.0; if (tps >= 90) return 70.0; if (tps >= 50) return 50.0; return 30.0; };
    const double s1 = (m_firstTokenMs >= 0 ? scoreTTFB(m_firstTokenMs) : -1);
    const double s2 = (m_promptTokPerSec >= 0 ? scoreTokps(m_promptTokPerSec) : -1);
    const double s3 = (m_genTokPerSec >= 0 ? scoreTokps(m_genTokPerSec) : -1);
    const double s4 = (m_qaScore >= 0 ? m_qaScore : -1);
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
        if (m_firstTokenMs <= 1000) setBg(good);
        else if (m_firstTokenMs <= 2500) setBg(warn);
        else setBg(bad);
        return;
    }
    if (key.contains("上文处理") || key.contains("生成速度"))
    {
        if (val <= 0) return;
        if (val >= 150) setBg(good);
        else if (val >= 70) setBg(warn);
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
    if (key.contains("工具调用"))
    {
        if (val < 0) return;
        if (val >= 90) setBg(good);
        else if (val >= 50) setBg(warn);
        else setBg(bad);
        return;
    }
}
