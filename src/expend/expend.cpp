#include "expend.h"
#include "ui_expend.h"
#include "../xnet.h" // ensure xNet is known as QObject-derived for invokeMethod

#include <QThread>
#include <QDir>
#include <QTextCursor>
#include <QScrollBar>
#include <QProcessEnvironment>
#include "../utils/evallogedit.h"
#include "../utils/introanimedit.h"
#include "../utils/neuronlogedit.h"

// Minimal, robust reimplementation of Expend core wiring.
// This file was reconstructed to fix unresolved symbols and restore
// process lifecycles after corruption.

Expend::Expend(QWidget *parent, QString applicationDirPath_)
    : QWidget(parent), ui(new Ui::Expend)
{
    ui->setupUi(this);
    applicationDirPath = applicationDirPath_;

    // Ensure a dedicated temp dir for TTS outputs under EVA_TEMP to avoid touching working dir
    outettsDir = QDir(applicationDirPath).filePath("EVA_TEMP/tts");

    // Basic UI tweaks (non-critical if object names evolve)
    if (ui->sd_prompt_textEdit) ui->sd_prompt_textEdit->setContextMenuPolicy(Qt::NoContextMenu);
    if (ui->sd_log) ui->sd_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    if (ui->modellog_card) ui->modellog_card->setLineWrapMode(QPlainTextEdit::NoWrap);

    // Track tab changes to (optionally) toggle heavy animations
    if (ui->tabWidget)
        connect(ui->tabWidget, &QTabWidget::currentChanged, this, &Expend::onTabCurrentChanged);

    // External processes
    server_process = new QProcess(this);
    connect(server_process, &QProcess::started, this, &Expend::server_onProcessStarted);
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::server_onProcessFinished);

    quantize_process = new QProcess(this);
    connect(quantize_process, &QProcess::started, this, &Expend::quantize_onProcessStarted);
    connect(quantize_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::quantize_onProcessFinished);

    whisper_process = new QProcess(this);
    connect(whisper_process, &QProcess::started, this, &Expend::whisper_onProcessStarted);
    connect(whisper_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::whisper_onProcessFinished);

    sd_process = new QProcess(this);
    connect(sd_process, &QProcess::started, this, &Expend::sd_onProcessStarted);
    connect(sd_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::sd_onProcessFinished);

    outetts_process = new QProcess(this);
    connect(outetts_process, &QProcess::started, this, &Expend::outetts_onProcessStarted);
    connect(outetts_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Expend::outetts_onProcessFinished);

    // Shutdown housekeeping on app exit
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        shutdownEvalWorker();
        stopEmbeddingServer(true);
    });

    // Speech playback
    speech_player = new QMediaPlayer;
    connect(speech_player, &QMediaPlayer::mediaStatusChanged, this, &Expend::speech_player_over);

    // Periodic timers for TTS stream/playback
    connect(&speechTimer, &QTimer::timeout, this, &Expend::speech_process);
    connect(&speechPlayTimer, &QTimer::timeout, this, &Expend::speech_play_process);
    speechTimer.start(500);
    speechPlayTimer.start(500);

    // Ensure temp dir exists and load persisted config
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    readConfig();
}

Expend::~Expend()
{
    // Ensure eval worker is stopped before destruction
    shutdownEvalWorker();

    // Best-effort terminate child processes
    auto killProc = [](QProcess *p) {
        if (!p) return;
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(150);
        }
    };
    killProc(sd_process);
    killProc(whisper_process);
    killProc(quantize_process);

    delete ui;
}

// Forward minimal logs to the Model Info log view
void Expend::recv_llama_log(QString log)
{
    if (!ui || !ui->modellog_card) return;
    QTextCursor cursor = ui->modellog_card->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    cursor.insertText(log);
    ui->modellog_card->setTextCursor(cursor);
}

// Stop knowledge embedding server (llama-server embedding mode)
void Expend::stopEmbeddingServer(bool force)
{
    if (!server_process) return;
#ifdef _WIN32
    auto winKillTree = [](qint64 pid, bool forceKill) {
        if (pid <= 0) return;
        QStringList args{ "/PID", QString::number(pid), "/T" };
        if (forceKill) args << "/F";
        QProcess::execute("taskkill", args);
    };
#endif
    if (server_process->state() == QProcess::Running) {
        if (force) {
#ifdef _WIN32
            winKillTree(server_process->processId(), true);
#else
            QProcess::execute("kill", {"-KILL", QString::number(server_process->processId())});
#endif
            server_process->waitForFinished(100);
        } else {
            server_process->terminate();
            if (!server_process->waitForFinished(1500)) {
                server_process->kill();
                server_process->waitForFinished(300);
            }
        }
    }
}

// Window state tweaks (optional)
void Expend::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) setWindowFlags(Qt::Tool);
    }
    QWidget::changeEvent(event);
}

// Animation toggles are safe no-ops if custom widgets are absent
void Expend::onTabCurrentChanged(int)
{
    updateModelInfoAnim();
}

// Toggle heavy animations according to current tab & visibility
void Expend::updateModelInfoAnim()
{
    if (!ui || !ui->tabWidget) return;
    const bool onIntroTab = (ui->tabWidget->currentIndex() == window_map[INTRODUCTION_WINDOW]);
    const bool onModelTab = (ui->tabWidget->currentIndex() == window_map[MODELINFO_WINDOW]);
    const bool onEvalTab = (ui->tabWidget->currentIndex() == window_map[MODELEVAL_WINDOW]);

    const bool runIntro = this->isVisible() && onIntroTab;
    const bool runModel = this->isVisible() && onModelTab;
    const bool runEval = this->isVisible() && onEvalTab;

    if (auto neu = qobject_cast<NeuronLogEdit *>(ui->modellog_card)) neu->setActive(runModel);
    if (auto intro = qobject_cast<IntroAnimEdit *>(ui->info_card)) intro->setActive(runIntro);
    if (auto evalLog = qobject_cast<EvalLogEdit *>(ui->eval_log_plainTextEdit)) evalLog->setActive(runEval);
}

// Language-aware text resolver
QString Expend::jtr(QString customstr)
{
    QJsonArray arr = wordsObj[customstr].toArray();
    if (language_flag >= 0 && language_flag < arr.size())
        return arr.at(language_flag).toString();
    // Fallbacks
    if (wordsObj.contains(customstr) && wordsObj[customstr].isString())
        return wordsObj[customstr].toString();
    return customstr;
}

// Pass-through event filter; specific widgets can re-install richer filters elsewhere
bool Expend::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    Q_UNUSED(event);
    return QWidget::eventFilter(obj, event);
}

// Persist a subset of settings on close to keep behavior consistent across runs
void Expend::closeEvent(QCloseEvent *event)
{
    // Save current SD template and basic paths
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    // Legacy inline SD path fields removed

    // Save advanced popup config as well
    settings.setValue("sd_adv_model_path", sd_run_config_.modelPath);
    settings.setValue("sd_adv_vae_path", sd_run_config_.vaePath);
    settings.setValue("sd_adv_clip_l_path", sd_run_config_.clipLPath);
    settings.setValue("sd_adv_clip_g_path", sd_run_config_.clipGPath);
    settings.setValue("sd_adv_clip_vision_path", sd_run_config_.clipVisionPath);
    settings.setValue("sd_adv_t5xxl_path", sd_run_config_.t5xxlPath);
    settings.setValue("sd_adv_qwen2vl_path", sd_run_config_.qwen2vlPath);

    settings.setValue("sd_adv_qwen2vl_vision_path", sd_run_config_.qwen2vlVisionPath);
    settings.setValue("sd_adv_lora_dir", sd_run_config_.loraDirPath);
    settings.setValue("sd_adv_taesd_path", sd_run_config_.taesdPath);
    settings.setValue("sd_adv_upscale_model", sd_run_config_.upscaleModelPath);
    settings.setValue("sd_adv_control_net", sd_run_config_.controlNetPath);
    settings.setValue("sd_adv_control_img", sd_run_config_.controlImagePath);
    settings.setValue("sd_adv_width", sd_run_config_.width);
    settings.setValue("sd_adv_height", sd_run_config_.height);
    settings.setValue("sd_adv_sampler", sd_run_config_.sampler);
    settings.setValue("sd_adv_scheduler", sd_run_config_.scheduler);
    settings.setValue("sd_adv_steps", sd_run_config_.steps);
    settings.setValue("sd_adv_cfg", sd_run_config_.cfgScale);
    settings.setValue("sd_adv_clip_skip", sd_run_config_.clipSkip);
    settings.setValue("sd_adv_batch", sd_run_config_.batchCount);
    settings.setValue("sd_adv_seed", sd_run_config_.seed);
    settings.setValue("sd_adv_strength", sd_run_config_.strength);
    settings.setValue("sd_adv_guidance", sd_run_config_.guidance);
    settings.setValue("sd_adv_rng", sd_run_config_.rng);
    settings.setValue("sd_adv_flow_shift_en", sd_run_config_.flowShiftEnabled);
    settings.setValue("sd_adv_flow_shift", sd_run_config_.flowShift);
    settings.setValue("sd_adv_offload_cpu", sd_run_config_.offloadToCpu);
    settings.setValue("sd_adv_clip_cpu", sd_run_config_.clipOnCpu);
    settings.setValue("sd_adv_vae_cpu", sd_run_config_.vaeOnCpu);
    settings.setValue("sd_adv_control_cpu", sd_run_config_.controlNetOnCpu);
    settings.setValue("sd_adv_diff_fa", sd_run_config_.diffusionFA);
    settings.setValue("sd_adv_vae_tiling", sd_run_config_.vaeTiling);
    settings.setValue("sd_adv_vae_tile_x", sd_run_config_.vaeTileX);
    settings.setValue("sd_adv_vae_tile_y", sd_run_config_.vaeTileY);
    settings.setValue("sd_adv_vae_tile_overlap", sd_run_config_.vaeTileOverlap);
    settings.setValue("sd_adv_modify", sd_run_config_.modifyPrompt);
    settings.setValue("sd_adv_negative", sd_run_config_.negativePrompt);
    settings.sync();

    event->accept();
}

bool Expend::createTempDirectory(const QString &path)
{
    QDir dir;
    if (dir.exists(path)) return true;
    return dir.mkpath(path);
}

void Expend::shutdownEvalWorker()
{
    if (evalNet)
        QMetaObject::invokeMethod(evalNet, "recv_stop", Qt::QueuedConnection, Q_ARG(bool, true));
    evalRunning = false;
    if (evalThread && evalThread->isRunning()) {
        evalThread->quit();
        if (!evalThread->wait(2000)) {
            evalThread->terminate();
            evalThread->wait(200);
        }
    }
}
