// 主函数和主要槽函数

#include "widget.h"

#include "terminal_pane.h"
#include "toolcall_test_dialog.h"
#include "backendmanagerdialog.h"
#include "ui_widget.h"
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMessageBox>
#include <QSet>
#include <QStringList>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>
#include <exception>
#include "../utils/textspacing.h"
#include "../utils/startuplogger.h"

void toggleWindowVisibility(QWidget *w, bool visible)
{
    if (!w) return;
    if (visible)
    {
        w->setWindowFlags(Qt::Window);
        w->showNormal();
        w->raise();
        w->activateWindow();
    }
    else
    {
        w->hide();
    }
}

Widget::Widget(QWidget *parent, QString applicationDirPath_)
    : QWidget(parent), ui(new Ui::Widget)
{
    QElapsedTimer ctorTimer;
    ctorTimer.start();
    QElapsedTimer sectionTimer;
    sectionTimer.start();
    const auto logStep = [&](const QString &label)
    {
        const qint64 section = sectionTimer.elapsed();
        const qint64 total = ctorTimer.elapsed();
        StartupLogger::log(QStringLiteral("[widget] %1 (%2 ms, total %3 ms)").arg(label).arg(section).arg(total));
        sectionTimer.restart();
    };

    //---------------初始化ui--------------
    ui->setupUi(this);
    if (ui->recordBar)
    {
        connect(ui->recordBar, &RecordBar::nodeClicked, this, &Widget::onRecordClicked);
        connect(ui->recordBar, &RecordBar::nodeDoubleClicked, this, &Widget::onRecordDoubleClicked);
    }
    loadOutputFontFromResource();
    refreshOutputFont();
    initTextComponentsMemoryPolicy();
    applicationDirPath = applicationDirPath_;
    dockerTargetMode_ = loadPersistedDockerMode();
    ui_dockerSandboxEnabled = (dockerTargetMode_ != DockerTargetMode::None);
    skillManager = new SkillManager(this);
    skillManager->setApplicationDir(applicationDirPath);
    connect(skillManager, &SkillManager::skillsChanged, this, &Widget::onSkillsChanged);
    connect(skillManager, &SkillManager::skillImported, this, [this](const QString &id)
            { reflash_state(QString::fromUtf8("ui:skill imported -> ") + id, SUCCESS_SIGNAL); });
    connect(skillManager, &SkillManager::skillOperationFailed, this, [this](const QString &msg)
            { reflash_state("ui:" + msg, WRONG_SIGNAL); });
    // Win7/老环境存在 SIGILL 时允许跳过技能预加载，可通过环境变量强制关闭/开启
    const QString osId = DeviceManager::currentOsId();
    const QString productVer = QSysInfo::productVersion();
    const QString kernelVer = QSysInfo::kernelVersion();
    const bool envDisableSkills = (qEnvironmentVariableIntValue("EVA_DISABLE_SKILLS") != 0);
    const bool envEnableSkills = (qEnvironmentVariableIntValue("EVA_ENABLE_SKILLS") != 0);
    const QOperatingSystemVersion osv = QOperatingSystemVersion::current();
    const bool legacyWin = (osv.majorVersion() > 0 && osv.majorVersion() <= 6) ||
                           productVer.startsWith(QStringLiteral("7")) ||
                           productVer.startsWith(QStringLiteral("6.")) ||
                           kernelVer.startsWith(QStringLiteral("6.")) ||
                           (osId == QStringLiteral("win7"));
    bool disableSkills = envDisableSkills || (legacyWin && !envEnableSkills);
    StartupLogger::log(QStringLiteral("[widget] skills preload gate: osId=%1 productVer=%2 kernel=%3 env_disable=%4 env_enable=%5 legacyWin=%6 -> %7")
                           .arg(osId,
                                productVer,
                                kernelVer,
                                envDisableSkills ? QStringLiteral("1") : QStringLiteral("0"),
                                envEnableSkills ? QStringLiteral("1") : QStringLiteral("0"),
                                legacyWin ? QStringLiteral("1") : QStringLiteral("0"),
                                disableSkills ? QStringLiteral("skip") : QStringLiteral("load")));
    if (disableSkills)
    {
        StartupLogger::log(QStringLiteral("[widget] skip skills preload (win7 or EVA_DISABLE_SKILLS=1)"));
    }
    else
    {
        QTimer::singleShot(0, this, &Widget::loadSkillsAsync);
        logStep(QStringLiteral("技能管理器初始化并安排异步加载"));
    }
    engineerEnvWatcher_.setParent(this);
    connect(&engineerEnvWatcher_, &QFutureWatcher<EngineerEnvSnapshot>::finished, this, &Widget::onEngineerEnvProbeFinished);
    // Default engineer workdir under application directory
    engineerWorkDir = QDir::cleanPath(QDir(applicationDirPath).filePath("EVA_WORK"));
    ui->splitter->setStretchFactor(0, 3); // 设置分隔器中第一个元素初始高度占比为3
    ui->splitter->setStretchFactor(1, 1); // 设置分隔器中第二个元素初始高度占比为1
    enableSplitterHover(ui->splitter);
    if (ui->statusTerminalSplitter)
    {
        ui->statusTerminalSplitter->setStretchFactor(0, 1);
        ui->statusTerminalSplitter->setStretchFactor(1, 0);
        ui->statusTerminalSplitter->setHandleWidth(8);
        ui->statusTerminalSplitter->setCollapsible(0, false);
        ui->statusTerminalSplitter->setCollapsible(1, true);
        enableSplitterHover(ui->statusTerminalSplitter);
        connect(ui->statusTerminalSplitter, &QSplitter::splitterMoved, this, [this](int, int)
                {
            if (!ui->statusTerminalSplitter) return;
            const QList<int> sizes = ui->statusTerminalSplitter->sizes();
            if (sizes.size() < 2) return;
            const int terminalWidth = sizes[1];
            terminalCollapsed_ = terminalWidth < 8;
            if (!terminalCollapsed_)
            {
                terminalAutoExpandSize_ = qMax(240, terminalWidth);
            } });
    }
    if (ui->terminalPane)
    {
        connect(ui->terminalPane, &TerminalPane::interruptRequested, this, &Widget::onTerminalInterruptRequested);
        ui->terminalPane->setManualWorkingDirectory(engineerWorkDir);
        terminalAutoExpandSize_ = qMax(320, ui->terminalPane->sizeHint().width());
    }
    QTimer::singleShot(0, this, &Widget::collapseTerminalPane);
    // QFont font(DEFAULT_FONT);
    // ui->state->setFont(font);                                                                     // 设置state区的字体
    // 注册 发送 快捷键
    shortcutCtrlEnter = new QHotkey(QKeySequence("CTRL+Return"), true, this);
    connect(shortcutCtrlEnter, &QHotkey::activated, this, &Widget::onShortcutActivated_CTRL_ENTER);
    logStep(QStringLiteral("基础 UI 组件初始化完成"));
    //--------------初始化语言--------------
    QLocale locale = QLocale::system();             // 获取系统locale
    QLocale::Language language = locale.language(); // 获取语言
    if (locale.languageToString(language) == "Chinese")
    {
        language_flag = 0; // 中文
    }
    else
    {
        language_flag = 1; // 英文
    }

    StartupLogger::log(QStringLiteral("[widget] 语言资源读取开始"));
    getWords(QStringLiteral(":/language"));
    StartupLogger::log(QStringLiteral("[widget] 语言资源读取完成"));
    // 记忆进度条：文本“记忆:xx%”，使用第二进度（黄色）表示
    ui->kv_bar->setMaximum(100);
    ui->kv_bar->setValue(0);
    ui->kv_bar->setSecondValue(0);
    ui->kv_bar->setShowText(jtr("kv bar label"));
    ui->kv_bar->setToolTip(jtr("kv bar tooltip").arg(0).arg(0));
    updateKvBarUi();
    //-------------初始化约定模板-------------
    ui_date_prompt = DEFAULT_DATE_PROMPT;
    ui_DATES.date_prompt = DEFAULT_DATE_PROMPT;
    ui_DATES.is_load_tool = false;
    date_map.insert("default", ui_DATES);
    EVA_DATES troll;
    troll.date_prompt = jtr("you are a troll please respect any question for user");
    troll.is_load_tool = false;
    date_map.insert(jtr("troll"), troll);
    EVA_DATES ghost;
    ghost.date_prompt = jtr("Mediocre ghost prompt");
    ghost.is_load_tool = false;
    date_map.insert(jtr("Mediocre ghost"), ghost);
    StartupLogger::log(QStringLiteral("[widget] date templates initialized"));

    //-------------默认展示内容-------------
    right_menu = nullptr;     // 初始设置输入区右击菜单为空
    ui_font.setPointSize(10); // 将设置控件的字体大小设置为10
    {
        const QFont appFont = QApplication::font();
        globalUiSettings_.fontFamily = appFont.family();
        if (globalUiSettings_.fontSizePt <= 0)
        {
            globalUiSettings_.fontSizePt = Widget::kDefaultUiFontPt;
        }
        globalUiSettings_.themeId = QStringLiteral("unit01");
    }
    setBaseWindowIcon(QIcon(":/logo/dark_logo.png"));
    ui->set->setIcon(QIcon(":/logo/assimp_tools_icon.ico"));                       // 设置设置图标
    ui->reset->setIcon(QIcon(":/logo/sync.ico"));                                  // 设置重置图标
    reflash_state("ui:" + jtr("click load and choose a gguf file"), USUAL_SIGNAL); // 初始提示

    //-------------初始化各种控件-------------
    // Dialog initialization is the most OS-sensitive block; log each step for Win7 diagnostics.
    StartupLogger::log(QStringLiteral("[widget] setApiDialog begin"));
    setApiDialog();   // 设置api选项
    StartupLogger::log(QStringLiteral("[widget] setApiDialog done"));

    StartupLogger::log(QStringLiteral("[widget] set_DateDialog begin"));
    set_DateDialog(); // 设置约定选项
    StartupLogger::log(QStringLiteral("[widget] set_DateDialog done"));

    StartupLogger::log(QStringLiteral("[widget] set_SetDialog begin"));
    try
    {
        set_SetDialog(); // 设置设置选项
        StartupLogger::log(QStringLiteral("[widget] set_SetDialog done"));
    }
    catch (const std::exception &ex)
    {
        StartupLogger::log(QStringLiteral("[widget] set_SetDialog failed: %1").arg(QString::fromLocal8Bit(ex.what())));
        qWarning() << "set_SetDialog threw" << ex.what();
    }
    catch (...)
    {
        StartupLogger::log(QStringLiteral("[widget] set_SetDialog failed: unknown exception"));
        qWarning() << "set_SetDialog threw unknown exception";
    }
    logStep(QStringLiteral("设置对话框初始化完成"));
    lastServerRestart_ = false;
    ui_state_init();                                              // 初始界面状态
    ui->input->textEdit->setContextMenuPolicy(Qt::NoContextMenu); // 取消右键菜单
    ui->input->installEventFilter(this);                          // 安装事件过滤器
    ui->input->textEdit->installEventFilter(this);                // 安装事件过滤器
    ui->load->installEventFilter(this);                           // 安装事件过滤器
    api_endpoint_LineEdit->installEventFilter(this);              // 安装事件过滤器
    ui->state->setContextMenuPolicy(Qt::NoContextMenu);           // 取消右键
    ui->state->installEventFilter(this);                          // 安装事件过滤器
    ui->state->setLineWrapMode(QPlainTextEdit::NoWrap);           // 禁用自动换行
    ui->state->setFocus();
    TextSpacing::apply(ui->state, 1.25);
    TextSpacing::apply(ui->output, 1.25);
    if (ui->input && ui->input->textEdit) TextSpacing::apply(ui->input->textEdit, 1.25);
    // Setup decode timer for wait animation
    decode_pTimer = new QTimer(this);
    connect(decode_pTimer, &QTimer::timeout, this, &Widget::decode_handleTimeout);
    trayMenu = new QMenu(this); // 托盘菜单

    //-------------获取cpu内存信息-------------
    max_thread = std::thread::hardware_concurrency();
    settings_ui->nthread_slider->setRange(1, max_thread); // 设置线程数滑块的范围
    QTimer *cpucheck_timer = new QTimer(this);
    cpucheck_timer->setInterval(500);
    connect(cpucheck_timer, &QTimer::timeout, this, &Widget::updateCpuStatus);
    QTimer::singleShot(1200, this, [this, cpucheck_timer]()
                       {
                           if (!this->isVisible() && !cpucheck_timer->isActive())
                           {
                               // 仍执行一次刷新，避免延迟导致状态面板为空
                               updateCpuStatus();
                           }
                           if (!cpucheck_timer->isActive()) cpucheck_timer->start();
                       });
    //-------------获取gpu内存信息-------------
    QTimer *gpucheck_timer = new QTimer(this);
    gpucheck_timer->setInterval(500);
    connect(gpucheck_timer, &QTimer::timeout, this, &Widget::updateGpuStatus);
    QTimer::singleShot(1500, this, [this, gpucheck_timer]()
                       {
                           if (!this->isVisible() && !gpucheck_timer->isActive())
                           {
                               updateGpuStatus();
                           }
                           if (!gpucheck_timer->isActive()) gpucheck_timer->start();
                       });

    //-------------输出/状态区滚动条控制-------------
    output_scrollBar = ui->output->verticalScrollBar();
    connect(output_scrollBar, &QScrollBar::valueChanged, this, &Widget::output_scrollBarValueChanged);

    //-------------截图相关-------------
    cutscreen_dialog = new CutScreenDialog(this);
    QObject::connect(cutscreen_dialog, &CutScreenDialog::cut2ui_qimagepath, this, &Widget::recv_qimagepath); // 传递截取的图像路径
    // QShortcut *shortcutF1 = new QShortcut(QKeySequence(Qt::Key_F1), this);
    // connect(shortcutF1, &QShortcut::activated, this, &Widget::onShortcutActivated_F1);
    shortcutF1 = new QHotkey(QKeySequence("F1"), true, this);
    connect(shortcutF1, &QHotkey::activated, this, &Widget::onShortcutActivated_F1);
    shortcutF3 = new QHotkey(QKeySequence("F3"), true, this);
    connect(shortcutF3, &QHotkey::activated, this, &Widget::onShortcutActivated_F3);

    //-------------音频相关-------------
    audio_timer = new QTimer(this);                                           // 录音定时器
    connect(audio_timer, &QTimer::timeout, this, &Widget::monitorAudioLevel); // 每隔100毫秒刷新一次输入区                                                      // win7就不用检查声音输入了
    QTimer::singleShot(0, this, &Widget::initializeAudioSubsystem);
    //----------------本地后端管理（llama-server）------------------
    serverManager = new LocalServerManager(this, applicationDirPath);
    proxyServer_ = new LocalProxyServer(this);
    connect(proxyServer_, &LocalProxyServer::wakeRequested, this, &Widget::onProxyWakeRequested);
    connect(proxyServer_, &LocalProxyServer::externalActivity, this, &Widget::onProxyExternalActivity);
    connect(proxyServer_, &LocalProxyServer::proxyError, this, [this](const QString &msg)
            { reflash_state("ui:proxy " + msg, WRONG_SIGNAL); });
    // 转发 server 输出到模型日志（增殖窗口）而不是主输出区
    connect(serverManager, &LocalServerManager::serverOutput, this, [this](const QString &s)
            { emit ui2expend_llamalog(s); });
    connect(serverManager, &LocalServerManager::serverOutput, this, &Widget::onServerOutput);
    connect(serverManager, &LocalServerManager::serverState, this, &Widget::reflash_state);
    // 后端启动失败 -> 立即停止装载动画并解锁界面
    connect(serverManager, &LocalServerManager::serverStartFailed, this, &Widget::onServerStartFailed);
    connect(serverManager, &LocalServerManager::serverReady, this, &Widget::onServerReady);
    connect(serverManager, &LocalServerManager::serverStopped, this, [this]()
            {
        const bool fallbackEligible = !ignoreNextServerStopped_;
        // 计划内重启时旧进程的退出：完全忽略，不重置 UI、不停止转轮动画
        if (ignoreNextServerStopped_ || lastServerRestart_) { ignoreNextServerStopped_ = false; suppressStateClearOnStop_ = false; return; }
        // 其它情况：后端确实已停止 -> 重置 UI，并停止任何进行中的动画

        backendOnline_ = false;
        lazyWakeInFlight_ = false;
        applyWakeUiLock(false);
        if (proxyServer_) proxyServer_->setBackendAvailable(false);
        const bool wasLazyUnloaded = lazyUnloaded_;
        const bool lazyStop = lazyUnloadPreserveState_ || wasLazyUnloaded;
        cancelLazyUnload(QStringLiteral("server stopped"));
        pendingSendAfterWake_ = false;

        suppressStateClearOnStop_ = false;
        lazyUnloadPreserveState_ = false;
        if (lazyStop)
        {
            reflash_state("ui:" + jtr("auto eject sleeping"), SIGNAL_SIGNAL);
        }
        else
        {
            reflash_state("ui:" + jtr("eva halted"), SIGNAL_SIGNAL);
        }
        if (decode_pTimer) decode_pTimer->stop();
        lastServerRestart_ = false;
        is_load = false;
        EVA_title = jtr("current model") + " ";
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        is_run = false;
        unlockButtonsAfterError();
        if (fallbackEligible && triggerWin7CpuFallback(QStringLiteral("process exit")))
        {
            return;
        } });

    // 应用语言语种，注意不能影响行动纲领（主要流程）
    apply_language(language_flag);

    //----------------设置系统托盘-----------------------
    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    refreshWindowIcon();
    trayIcon->setToolTip(EVA_title);
    trayIcon->setContextMenu(trayMenu);
    // 托盘图标点击事件
    QObject::connect(trayIcon, &QSystemTrayIcon::activated, this, [&](QSystemTrayIcon::ActivationReason reason)
                     {
        if (reason == QSystemTrayIcon::Trigger) //单击
        {
            toggleWindowVisibility(this, true); // 显示窗体
        } });

    // 监视相关
    connect(&monitor_timer, SIGNAL(timeout()), this, SLOT(monitorTime())); // 新开一个线程

    lazyUnloadTimer_ = new QTimer(this);
    lazyUnloadTimer_->setSingleShot(true);
    connect(lazyUnloadTimer_, &QTimer::timeout, this, &Widget::performLazyUnload);
    lazyCountdownTimer_ = new QTimer(this);
    lazyCountdownTimer_->setInterval(1000);
    lazyCountdownTimer_->setSingleShot(false);
    connect(lazyCountdownTimer_, &QTimer::timeout, this, &Widget::updateLazyCountdownLabel);
    updateLazyCountdownLabel();


    EVA_title = jtr("eva");
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    trayIcon->show();
    logStep(QStringLiteral("托盘与语言资源初始化完成"));
    // Initialize persistent history store under EVA_TEMP/history
    history_ = new HistoryStore(QDir(applicationDirPath).filePath("EVA_TEMP/history"));
    logStep(QStringLiteral("历史记录存储初始化完成"));

    // 进程退出前，确保停止本地 llama-server，避免残留进程
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]()
            {
        if (serverManager) serverManager->stop(); });
    StartupLogger::log(QStringLiteral("[widget] 构造函数收尾, 总耗时 %1 ms").arg(ctorTimer.elapsed()));
    qDebug() << "widget init over";
}

Widget::~Widget()
{
    if (serverManager) serverManager->stop();
    if (proxyServer_) proxyServer_->stop();
    delete history_;
    delete ui;
    delete cutscreen_dialog;
    delete date_ui;
    delete settings_ui;
}

void Widget::setBaseWindowIcon(const QIcon &icon)
{
    EVA_icon = icon;
    refreshWindowIcon();
}

void Widget::refreshWindowIcon()
{
    const QIcon engineerIcon(QStringLiteral(":/logo/User.ico"));
    const bool engineerActive = isEngineerToolActive();
    const QIcon targetIcon = engineerActive ? engineerIcon : EVA_icon;
    QApplication::setWindowIcon(targetIcon);
    if (trayIcon) trayIcon->setIcon(targetIcon);
}

bool Widget::isEngineerToolActive() const
{
    if (date_ui && date_ui->engineer_checkbox) return date_ui->engineer_checkbox->isChecked();
    return ui_engineer_ischecked;
}

bool Widget::shouldApplySandboxNow() const
{
    return !(date_dialog && date_dialog->isVisible());
}

void Widget::loadSkillsAsync()
{
    // 在部分 Win7/老旧环境上解析技能目录可能触发非法指令，提供再次兜底的运行时跳过
    const bool envDisableSkills = (qEnvironmentVariableIntValue("EVA_DISABLE_SKILLS") != 0);
    const bool legacyWin = DeviceManager::currentOsId() == QStringLiteral("win7") ||
                           QSysInfo::productVersion().startsWith(QStringLiteral("7")) ||
                           QSysInfo::productVersion().startsWith(QStringLiteral("6.")) ||
                           QSysInfo::kernelVersion().startsWith(QStringLiteral("6."));
    if (envDisableSkills || legacyWin)
    {
        StartupLogger::log(QStringLiteral("[widget] loadSkillsAsync skipped at runtime (env=%1 legacyWin=%2)")
                               .arg(envDisableSkills ? QStringLiteral("1") : QStringLiteral("0"))
                               .arg(legacyWin ? QStringLiteral("1") : QStringLiteral("0")));
        return;
    }
    StartupLogger::log(QStringLiteral("[widget] loadSkillsAsync enter"));
    if (!skillManager) return;
    QElapsedTimer timer;
    timer.start();
    const bool ok = skillManager->loadFromDisk();
    StartupLogger::log(QStringLiteral("[widget] 技能目录扫描完成 (%1 ms, %2)")
                           .arg(timer.elapsed())
                           .arg(ok ? QStringLiteral("成功") : QStringLiteral("失败")));
}

void Widget::initializeAudioSubsystem()
{
    StartupLogger::log(QStringLiteral("[widget] initializeAudioSubsystem enter"));
    QElapsedTimer timer;
    timer.start();
    const bool audioReady = checkAudio();
    StartupLogger::log(QStringLiteral("[widget] 音频子系统探测%1 (%2 ms)")
                           .arg(audioReady ? QStringLiteral("成功") : QStringLiteral("跳过"))
                           .arg(timer.elapsed()));
    if (!audioReady)
    {
        return;
    }
    if (!shortcutF2)
    {
        shortcutF2 = new QHotkey(QKeySequence("F2"), true, this);
        connect(shortcutF2, &QHotkey::activated, this, &Widget::onShortcutActivated_F2);
    }
}

bool Widget::ensureGlobalSettingsDialog()
{
    if (globalDialog_) return true;
    if (!settings_ui || !settings_dialog || !settings_ui->verticalLayout_4) return false;

    QElapsedTimer timer;
    timer.start();

    globalDialog_ = new QDialog(this);
    globalDialog_->setAttribute(Qt::WA_DeleteOnClose, false);
    globalDialog_->setWindowFlag(Qt::WindowStaysOnTopHint, true);
    globalDialog_->setModal(true);

    QVBoxLayout *dialogLayout = new QVBoxLayout(globalDialog_);
    dialogLayout->setContentsMargins(12, 12, 12, 12);
    dialogLayout->setSpacing(8);

    globalSettingsPanel_ = new QFrame(globalDialog_);
    globalSettingsPanel_->setObjectName(QStringLiteral("globalSettingsPanel"));
    globalSettingsPanel_->setFrameShape(QFrame::StyledPanel);
    globalSettingsPanel_->setFrameShadow(QFrame::Raised);
    globalSettingsPanel_->setMinimumWidth(0);
    globalSettingsPanel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QVBoxLayout *panelLayout = new QVBoxLayout(globalSettingsPanel_);
    panelLayout->setContentsMargins(12, 16, 12, 16);
    panelLayout->setSpacing(12);

    auto installLabel = [&](QLabel *&target, int weight)
    {
        target = new QLabel(globalSettingsPanel_);
        target->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        target->setStyleSheet(QStringLiteral("font-weight:%1;").arg(weight));
        panelLayout->addWidget(target);
    };

    installLabel(globalPanelTitleLabel_, 600);
    panelLayout->addSpacing(4);

    installLabel(globalFontLabel_, 500);
    globalFontCombo_ = new QFontComboBox(globalSettingsPanel_);
    globalFontCombo_->setEditable(false);
    globalFontCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panelLayout->addWidget(globalFontCombo_);

    installLabel(globalFontSizeLabel_, 500);
    globalFontSizeSpin_ = new QSpinBox(globalSettingsPanel_);
    globalFontSizeSpin_->setRange(8, 48);
    globalFontSizeSpin_->setAccelerated(true);
    panelLayout->addWidget(globalFontSizeSpin_);

    panelLayout->addSpacing(8);
    installLabel(globalOutputFontLabel_, 500);
    globalOutputFontCombo_ = new QFontComboBox(globalSettingsPanel_);
    globalOutputFontCombo_->setEditable(false);
    globalOutputFontCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panelLayout->addWidget(globalOutputFontCombo_);

    installLabel(globalOutputFontSizeLabel_, 500);
    globalOutputFontSizeSpin_ = new QSpinBox(globalSettingsPanel_);
    globalOutputFontSizeSpin_->setRange(8, 48);
    globalOutputFontSizeSpin_->setAccelerated(true);
    panelLayout->addWidget(globalOutputFontSizeSpin_);

    installLabel(globalThemeLabel_, 500);
    globalThemeCombo_ = new QComboBox(globalSettingsPanel_);
    struct ThemeMeta
    {
        const char *id;
    };
    const ThemeMeta themes[] = {
        {"unit01"},
        {"unit00"},
        {"unit02"},
        {"unit03"}};
    for (const ThemeMeta &meta : themes)
    {
        globalThemeCombo_->addItem(QString(), QString::fromLatin1(meta.id));
    }
    panelLayout->addWidget(globalThemeCombo_);
    panelLayout->addStretch(1);

    dialogLayout->addWidget(globalSettingsPanel_);
    globalDialog_->setLayout(dialogLayout);

    connect(globalFontCombo_, &QFontComboBox::currentFontChanged, this, &Widget::handleGlobalFontFamilyChanged);
    connect(globalFontSizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &Widget::handleGlobalFontSizeChanged);
    connect(globalOutputFontCombo_, &QFontComboBox::currentFontChanged, this, &Widget::handleOutputFontFamilyChanged);
    connect(globalOutputFontSizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &Widget::handleOutputFontSizeChanged);
    connect(globalThemeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Widget::handleGlobalThemeChanged);

    updateGlobalSettingsTranslations();
    syncGlobalSettingsPanelControls();

    StartupLogger::log(QStringLiteral("[widget] 全局设置面板构建完成 (%1 ms)").arg(timer.elapsed()));

    return true;
}

ToolCallTestDialog *Widget::ensureToolCallTestDialog()
{
    if (!toolCallTestDialog_)
    {
        toolCallTestDialog_ = new ToolCallTestDialog(this);
        connect(toolCallTestDialog_, &ToolCallTestDialog::testRequested, this, &Widget::handleToolCallTestRequest);
        toolCallTestDialog_->setTranslator([this](const QString &key, const QString &fallback) -> QString {
            const QString translated = jtr(key);
            return translated.isEmpty() ? fallback : translated;
        });
    }
    return toolCallTestDialog_;
}

void Widget::handleToolCallTestRequest(const QString &inputText)
{
    ToolCallTestDialog *dialog = ensureToolCallTestDialog();
    if (!dialog) return;

    const auto toolcallTr = [&](const QString &key, const QString &fallback) -> QString {
        const QString text = jtr(key);
        return text.isEmpty() ? fallback : text;
    };

    QStringList logLines;
    logLines << toolcallTr(QStringLiteral("toolcall log length"), QStringLiteral("Original input length: %1 chars")).arg(inputText.size());

    const QString trimmed = inputText.trimmed();
    if (trimmed.isEmpty())
    {
        dialog->displayReport(logLines, QString(),
                              toolcallTr(QStringLiteral("toolcall log no content"), QStringLiteral("No parseable content.")), false);
        return;
    }

    mcp::json parsed = XMLparser(trimmed, &logLines);
    const bool success = !parsed.empty();

    QString jsonDump;
    QString summary;
    if (success)
    {
        jsonDump = QString::fromStdString(parsed.dump(2));
        summary = formatToolCallSummary(parsed);
    }
    else
    {
        summary = toolcallTr(QStringLiteral("toolcall log invalid format"),
                             QStringLiteral("No valid tool call found; check <tool_call> tags and JSON structure."));
    }

    dialog->displayReport(logLines, jsonDump, summary, success);
}

QString Widget::formatToolCallSummary(const mcp::json &payload) const
{
    if (payload.empty())
    {
        return QString();
    }

    const auto toolcallTr = [&](const QString &key, const QString &fallback) -> QString {
        const QString textValue = const_cast<Widget *>(this)->jtr(key);
        return textValue.isEmpty() ? fallback : textValue;
    };

    QStringList lines;
    if (payload.contains("tool_call_id"))
    {
        const mcp::json &idNode = payload.at("tool_call_id");
        const QString idValue = idNode.is_string() ? QString::fromStdString(idNode.get<std::string>())
                                                   : QString::fromStdString(idNode.dump());
        lines << toolcallTr(QStringLiteral("toolcall summary id"), QStringLiteral("tool_call_id: %1")).arg(idValue);
    }

    QString toolName;
    if (payload.contains("name"))
    {
        const mcp::json &nameNode = payload.at("name");
        toolName = nameNode.is_string() ? QString::fromStdString(nameNode.get<std::string>())
                                        : QString::fromStdString(nameNode.dump());
    }
    else
    {
        toolName = toolcallTr(QStringLiteral("toolcall summary missing placeholder"), QStringLiteral("<missing>"));
    }
    lines << toolcallTr(QStringLiteral("toolcall summary name"), QStringLiteral("Tool name: %1")).arg(toolName);

    if (payload.contains("arguments"))
    {
        const mcp::json &args = payload.at("arguments");
        if (args.is_null())
        {
            lines << toolcallTr(QStringLiteral("toolcall summary args null"), QStringLiteral("arguments: null"));
        }
        else if (args.is_object())
        {
            QStringList argLines;
            for (auto it = args.cbegin(); it != args.cend(); ++it)
            {
                const QString key = QString::fromStdString(it.key());
                const QString value = QString::fromStdString(it.value().dump());
                argLines << toolcallTr(QStringLiteral("toolcall summary arg kv"), QStringLiteral("%1 = %2")).arg(key, value);
            }
            if (argLines.isEmpty())
            {
                lines << toolcallTr(QStringLiteral("toolcall summary args empty object"), QStringLiteral("arguments: <empty object>"));
            }
            else
            {
                lines << toolcallTr(QStringLiteral("toolcall summary args detail"), QStringLiteral("Arguments:"));
                lines << argLines;
            }
        }
        else
        {
            lines << toolcallTr(QStringLiteral("toolcall summary args generic"), QStringLiteral("arguments: %1"))
                         .arg(QString::fromStdString(args.dump()));
        }
    }
    else
    {
        lines << toolcallTr(QStringLiteral("toolcall summary args missing"), QStringLiteral("arguments: <missing>"));
    }

    return lines.join(QStringLiteral("\n"));
}

// 窗口状态变化处理
void Widget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        if (isMinimized())
        {
            setWindowFlags(Qt::Tool); // 隐藏任务栏条目
            trayIcon->showMessage(jtr("eva hide"), "", EVA_icon, 1000);
        }
    }
}

// 关闭事件：显示一个“无限循环”的进度对话框，等待后台服务优雅停止
void Widget::closeEvent(QCloseEvent *event)
{
    // 防止重复触发
    if (isShuttingDown_)
    {
        event->ignore();
        return;
    }

    isShuttingDown_ = true;

    // 停止一切可能唤醒后端或继续转发的通道，避免退出阶段再次拉起后端
    if (proxyServer_) proxyServer_->stop();
    if (controlChannel_)
    {
        if (isControllerActive()) releaseControl(true);
        controlChannel_->stopHost();
        controlChannel_->disconnectFromHost();
    }

    // 构建一个无确定进度（0,0）的进度对话框，立刻显示，应用级模态
    QProgressDialog *dlg = new QProgressDialog(this);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setMinimumDuration(0);
    dlg->setRange(0, 0);           // 设置为无限循环进度条
    dlg->setCancelButton(nullptr); // 不显示取消按钮
    // 隐藏问号按钮和关闭按钮
    dlg->setWindowFlags(
        dlg->windowFlags() & ~Qt::WindowContextHelpButtonHint // 移除问号按钮
        & ~Qt::WindowCloseButtonHint                          // 移除关闭按钮
    );
    dlg->setWindowTitle("quit");
    dlg->setLabelText(jtr("quit stopping backend"));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    qApp->processEvents(); // 刷新一次以显示对话框

    const bool stopDocker = shouldShutdownDockerOnExit();
    auto finalizeQuit = [this, dlg, stopDocker]() {
        if (stopDocker)
        {
            dlg->setLabelText(jtr("quit stopping docker"));
            qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
            QEventLoop dockerLoop;
            QMetaObject::Connection conn = connect(this, &Widget::dockerShutdownFinished, &dockerLoop, &QEventLoop::quit);
            emit ui2tool_shutdownDocker();
            dockerLoop.exec(QEventLoop::ExcludeUserInputEvents);
            disconnect(conn);
        }
        dlg->close();
        dlg->deleteLater();
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    };

    // 服务未在运行，直接退出
    if (!serverManager || !serverManager->isRunning())
    {
        finalizeQuit();
        event->accept();
        return;
    }

    // 在 UI 线程等待后端停止，防止退出阶段跨线程清理
    event->ignore();
    if (serverManager)
    {
        connect(serverManager, &LocalServerManager::serverStopped, this, [finalizeQuit]()
                { finalizeQuit(); });
        serverManager->stopAsync();
    }
    else
    {
        finalizeQuit();
    }
}

bool Widget::shouldShutdownDockerOnExit() const
{
    return ui_engineer_ischecked && ui_dockerSandboxEnabled;
}

void Widget::onDockerShutdownCompleted()
{
    emit dockerShutdownFinished();
}

void Widget::enableSplitterHover(QSplitter *splitter)
{
    if (!splitter) return;
    splitter->setAttribute(Qt::WA_Hover, true);
    splitter->setMouseTracking(true);
    const int handleCount = splitter->count();
    for (int i = 0; i <= handleCount; ++i)
    {
        if (QSplitterHandle *handle = splitter->handle(i))
        {
            handle->setAttribute(Qt::WA_Hover, true);
            handle->setMouseTracking(true);
            handle->update();
        }
    }
}

BackendManagerDialog *Widget::ensureBackendManagerDialog()
{
    QWidget *host = (settings_dialog && settings_dialog->isVisible()) ? static_cast<QWidget *>(settings_dialog) : static_cast<QWidget *>(this);
    if (!backendManagerDialog_)
    {
        auto translator = [this](const QString &key) -> QString
        {
            return this->jtr(key);
        };
        auto provider = [this]() -> QMap<QString, QString>
        {
            return this->currentOverrideMapForUi();
        };
        auto setter = [this](const QString &roleId, const QString &path)
        {
            ensurePendingOverridesInitialized();
            pendingBackendOverrides_.insert(roleId, path);
            backendOverrideDirty_ = true;
        };
        auto clearer = [this](const QString &roleId)
        {
            ensurePendingOverridesInitialized();
            pendingBackendOverrides_.remove(roleId);
            backendOverrideDirty_ = true;
        };
        backendManagerDialog_ = new BackendManagerDialog(translator, provider, setter, clearer, host);
        backendManagerDialog_->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        connect(backendManagerDialog_, &BackendManagerDialog::overridesChanged, this, &Widget::onBackendOverridesChanged);
    }
    else if (backendManagerDialog_->parentWidget() != host)
    {
        backendManagerDialog_->setParent(host);
    }
    const bool modalToSettings = (host == settings_dialog && settings_dialog);
    backendManagerDialog_->setModal(modalToSettings);
    backendManagerDialog_->setWindowModality(modalToSettings ? Qt::WindowModal : Qt::NonModal);
    backendManagerDialog_->refresh();
    return backendManagerDialog_;
}

void Widget::openBackendManagerDialog(const QString &roleId)
{
    BackendManagerDialog *dlg = ensureBackendManagerDialog();
    if (!roleId.isEmpty())
    {
        dlg->focusRole(roleId);
    }
    QTimer::singleShot(0, dlg, [dlg]()
                       {
                           dlg->show();
                           dlg->raise();
                           dlg->activateWindow();
                       });
}

void Widget::onBackendOverridesChanged()
{
    backendOverrideDirty_ = true;
    syncBackendOverrideState();
    refreshDeviceBackendUI();
    reflash_state(QStringLiteral("ui:backend override updated"), SIGNAL_SIGNAL);
}

void Widget::syncBackendOverrideState()
{
    if (!settings_ui || !settings_ui->device_comboBox) return;
    const QString customText = QStringLiteral("custom");
    const QMap<QString, QString> overrides = currentOverrideMapForUi();
    const QString inferenceRole = QStringLiteral("llama-server-main");
    const bool hasInferenceOverride = overrides.contains(inferenceRole);
    int customIndex = settings_ui->device_comboBox->findText(customText);
    if (hasInferenceOverride && customIndex < 0)
    {
        settings_ui->device_comboBox->addItem(customText);
        customIndex = settings_ui->device_comboBox->findText(customText);
    }
    const QString currentText = settings_ui->device_comboBox->currentText().trimmed().toLower();
    if (hasInferenceOverride)
    {
        if (customIndex < 0) return;
        if (currentText != customText && currentText != QString())
        {
            lastDeviceBeforeCustom_ = currentText;
        }
        {
            QSignalBlocker blocker(settings_ui->device_comboBox);
            settings_ui->device_comboBox->setCurrentIndex(customIndex);
        }
        ui_device_backend = customText;
        DeviceManager::setUserChoice(customText);
    }
    else
    {
        if (currentText == customText)
        {
            QString restore = lastDeviceBeforeCustom_;
            if (restore.isEmpty())
            {
                restore = QStringLiteral("auto");
            }
            int idx = settings_ui->device_comboBox->findText(restore);
            if (idx < 0)
            {
                idx = settings_ui->device_comboBox->findText(QStringLiteral("auto"));
            }
            if (idx >= 0)
            {
                QSignalBlocker blocker(settings_ui->device_comboBox);
                settings_ui->device_comboBox->setCurrentIndex(idx);
            }
            ui_device_backend = settings_ui->device_comboBox->currentText().trimmed().toLower();
            DeviceManager::setUserChoice(ui_device_backend);
        }
        if (customIndex >= 0 && settings_ui->device_comboBox->currentText().trimmed().toLower() != customText)
        {
            QSignalBlocker blocker(settings_ui->device_comboBox);
            settings_ui->device_comboBox->removeItem(customIndex);
        }
    }
    refreshDeviceBackendUI();
}

void Widget::onDeviceComboTextChanged(const QString &text)
{
    Q_UNUSED(text);
    if (!settings_ui || !settings_ui->device_comboBox) return;
    const QString customText = QStringLiteral("custom");
    const QString inferenceRole = QStringLiteral("llama-server-main");
    const QString nextChoice = settings_ui->device_comboBox->currentText().trimmed().toLower();
    const bool dialogVisible = settings_dialog && settings_dialog->isVisible();
    const QMap<QString, QString> overrides = currentOverrideMapForUi();
    const bool hasInferenceOverride = overrides.contains(inferenceRole);

    if (nextChoice == customText)
    {
        if (!hasInferenceOverride)
        {
            QString restore = lastDeviceBeforeCustom_;
            if (restore.isEmpty()) restore = QStringLiteral("auto");
            int idx = settings_ui->device_comboBox->findText(restore);
            if (idx < 0) idx = settings_ui->device_comboBox->findText(QStringLiteral("auto"));
            if (idx >= 0)
            {
                QSignalBlocker blocker(settings_ui->device_comboBox);
                settings_ui->device_comboBox->setCurrentIndex(idx);
            }
            refreshDeviceBackendUI();
            return;
        }
        ui_device_backend = customText;
        DeviceManager::setUserChoice(customText);
        refreshDeviceBackendUI();
        return;
    }

    if (!nextChoice.isEmpty())
    {
        lastDeviceBeforeCustom_ = nextChoice;
    }
    ui_device_backend = nextChoice;
    DeviceManager::setUserChoice(ui_device_backend);

    if (hasInferenceOverride)
    {
        if (dialogVisible)
        {
            ensurePendingOverridesInitialized();
            bool removed = pendingBackendOverrides_.remove(inferenceRole) > 0;
            if (!removed && pendingBackendOverrides_.isEmpty() && backendOverrideSnapshot_.contains(inferenceRole))
            {
                pendingBackendOverrides_ = backendOverrideSnapshot_;
                removed = pendingBackendOverrides_.remove(inferenceRole) > 0;
            }
            if (removed)
            {
                backendOverrideDirty_ = true;
            }
        }
        else
        {
            DeviceManager::clearProgramOverride(inferenceRole);
            backendOverrideSnapshot_ = DeviceManager::programOverrides();
        }
        syncBackendOverrideState();
    }
    refreshDeviceBackendUI();
}

QMap<QString, QString> Widget::currentOverrideMapForUi() const
{
    if (settings_dialog && settings_dialog->isVisible())
    {
        if (backendOverrideDirty_ || !pendingBackendOverrides_.isEmpty()) return pendingBackendOverrides_;
        return backendOverrideSnapshot_;
    }
    return DeviceManager::programOverrides();
}

void Widget::ensurePendingOverridesInitialized()
{
    if (!(settings_dialog && settings_dialog->isVisible())) return;
    if (backendOverrideDirty_) return;
    if (pendingBackendOverrides_.isEmpty())
    {
        pendingBackendOverrides_ = backendOverrideSnapshot_;
    }
}

void Widget::commitPendingBackendOverrides()
{
    DeviceManager::clearProgramOverrides();
    for (auto it = pendingBackendOverrides_.constBegin(); it != pendingBackendOverrides_.constEnd(); ++it)
    {
        DeviceManager::setProgramOverride(it.key(), it.value());
    }
    backendOverrideSnapshot_ = pendingBackendOverrides_;
    pendingBackendOverrides_.clear();
    backendOverrideDirty_ = false;
}
