// 主函数和主要槽函数

#include "widget.h"

#include "terminal_pane.h"
#include "ui_widget.h"
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QVBoxLayout>
#include <QtGlobal>
#include "../utils/textspacing.h"

Widget::Widget(QWidget *parent, QString applicationDirPath_)
    : QWidget(parent), ui(new Ui::Widget)
{
    //---------------初始化ui--------------
    ui->setupUi(this);
    if (ui->recordBar)
    {
        connect(ui->recordBar, &RecordBar::nodeClicked, this, &Widget::onRecordClicked);
        connect(ui->recordBar, &RecordBar::nodeDoubleClicked, this, &Widget::onRecordDoubleClicked);
    }
    initTextComponentsMemoryPolicy();
    applicationDirPath = applicationDirPath_;
    skillManager = new SkillManager(this);
    skillManager->setApplicationDir(applicationDirPath);
    skillManager->loadFromDisk();
    connect(skillManager, &SkillManager::skillsChanged, this, &Widget::onSkillsChanged);
    connect(skillManager, &SkillManager::skillImported, this, [this](const QString &id)
            { reflash_state(QString::fromUtf8("ui:skill imported -> ") + id, SUCCESS_SIGNAL); });
    connect(skillManager, &SkillManager::skillOperationFailed, this, [this](const QString &msg)
            { reflash_state("ui:" + msg, WRONG_SIGNAL); });
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

    getWords(":/src/utils/ui_language.json");
    // 记忆进度条：文本“记忆:xx%”，使用第二进度（黄色）表示
    ui->kv_bar->setShowText(QString::fromUtf8("记忆:"));
    ui->kv_bar->setMaximum(100);
    ui->kv_bar->setValue(0);
    ui->kv_bar->setSecondValue(0);
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

    //-------------默认展示内容-------------
    right_menu = nullptr;     // 初始设置输入区右击菜单为空
    ui_font.setPointSize(10); // 将设置控件的字体大小设置为10
    {
        const QFont appFont = QApplication::font();
        globalUiSettings_.fontFamily = appFont.family();
        const int pt = appFont.pointSize();
        globalUiSettings_.fontSizePt = pt > 0 ? pt : globalUiSettings_.fontSizePt;
        globalUiSettings_.themeId = QStringLiteral("unit01");
    }
    EVA_icon = QIcon(":/logo/dark_logo.png");
    QApplication::setWindowIcon(EVA_icon);                                         // 设置应用程序图标
    ui->set->setIcon(QIcon(":/logo/assimp_tools_icon.ico"));                       // 设置设置图标
    ui->reset->setIcon(QIcon(":/logo/sync.ico"));                                  // 设置重置图标
    reflash_state("ui:" + jtr("click load and choose a gguf file"), USUAL_SIGNAL); // 初始提示

    //-------------初始化各种控件-------------
    setApiDialog();   // 设置api选项
    set_DateDialog(); // 设置约定选项
    set_SetDialog();  // 设置设置选项
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
    connect(cpucheck_timer, &QTimer::timeout, this, &Widget::updateCpuStatus);
    cpucheck_timer->start(500); // 多少ms更新一次
    //-------------获取gpu内存信息-------------
    QTimer *gpucheck_timer = new QTimer(this);
    connect(gpucheck_timer, &QTimer::timeout, this, &Widget::updateGpuStatus);
    gpucheck_timer->start(500); // 多少ms更新一次

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

    //-------------音频相关-------------
    audio_timer = new QTimer(this);                                           // 录音定时器
    connect(audio_timer, &QTimer::timeout, this, &Widget::monitorAudioLevel); // 每隔100毫秒刷新一次输入区                                                      // win7就不用检查声音输入了
    if (checkAudio())                                                         // 如果支持音频输入则注册f2快捷键
    {
        // QShortcut *shortcutF2 = new QShortcut(QKeySequence(Qt::Key_F2), this);
        // connect(shortcutF2, &QShortcut::activated, this, &Widget::onShortcutActivated_F2);
        shortcutF2 = new QHotkey(QKeySequence("F2"), true, this);
        connect(shortcutF2, &QHotkey::activated, this, &Widget::onShortcutActivated_F2);
    }
    //----------------本地后端管理（llama-server）------------------
    serverManager = new LocalServerManager(this, applicationDirPath);
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
        // 计划内重启时旧进程的退出：完全忽略，不重置 UI、不停止转轮动画
        if (ignoreNextServerStopped_ || lastServerRestart_) { ignoreNextServerStopped_ = false; return; }
        // 其它情况：后端确实已停止 -> 重置 UI，并停止任何进行中的动画

        ui->state->clear();
        reflash_state("ui: local server stopped", SIGNAL_SIGNAL);
        if (decode_pTimer) decode_pTimer->stop();
        lastServerRestart_ = false;
        is_load = false;
        EVA_title = jtr("current model") + " ";
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        is_run = false;
        unlockButtonsAfterError(); });

    // 应用语言语种，注意不能影响行动纲领（主要流程）
    apply_language(language_flag);

    //----------------设置系统托盘-----------------------
    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
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

    EVA_title = jtr("eva");
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    trayIcon->show();
    // Initialize persistent history store under EVA_TEMP/history
    history_ = new HistoryStore(QDir(applicationDirPath).filePath("EVA_TEMP/history"));

    // 进程退出前，确保停止本地 llama-server，避免残留进程
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]()
            {
        if (serverManager) serverManager->stop(); });
    qDebug() << "widget init over";
}

Widget::~Widget()
{
    if (serverManager) serverManager->stop();
    delete history_;
    delete ui;
    delete cutscreen_dialog;
    delete date_ui;
    delete settings_ui;
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
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    qApp->processEvents(); // 刷新一次以显示对话框

    // 如果后端未在运行，直接退出
    if (!serverManager || !serverManager->isRunning())
    {
        dlg->close();
        dlg->deleteLater();
        event->accept();
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }

    // 在 UI 线程发起非阻塞的优雅停止，避免跨线程调用 QProcess 导致 QWinEventNotifier 警告/卡顿
    event->ignore();
    if (serverManager)
    {
        // 完成后关闭对话框并退出应用
        connect(serverManager, &LocalServerManager::serverStopped, dlg, &QProgressDialog::close);
        connect(serverManager, &LocalServerManager::serverStopped, dlg, &QObject::deleteLater);
        connect(serverManager, &LocalServerManager::serverStopped, qApp, []()
                { QTimer::singleShot(0, qApp, &QCoreApplication::quit); });
        serverManager->stopAsync();
    }
    else
    {
        // 理论上不走到这里，因为上面已判断 running；兜底直接退出
        dlg->close();
        dlg->deleteLater();
        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
    }
}

// 用户点击装载按钮处理
void Widget::on_load_clicked()
{
    // reflash_state("ui:" + jtr("clicked load"), SIGNAL_SIGNAL);

    // 弹出模式选择对话框：本地模式 或 链接模式（上下结构、紧凑、无“取消”按钮）
    // Build a minimal modal dialog with vertical buttons to satisfy the UI spec.
    QDialog modeDlg(this);
    modeDlg.setModal(true);
    modeDlg.setWindowTitle(jtr("load"));
    // Remove help button, keep close button; do not add a Cancel control.
    modeDlg.setWindowFlags(modeDlg.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QVBoxLayout *vbox = new QVBoxLayout(&modeDlg);
    vbox->setContentsMargins(12, 12, 12, 12); // compact
    vbox->setSpacing(6);
    QPushButton *localBtn = new QPushButton(jtr("local mode"), &modeDlg);
    QPushButton *linkBtn = new QPushButton(jtr("link mode"), &modeDlg);
    localBtn->setMinimumHeight(50);
    linkBtn->setMinimumHeight(50);
    // Make them expand horizontally but stack vertically
    localBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    linkBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    vbox->addWidget(localBtn);
    vbox->addWidget(linkBtn);
    // Clicking a button ends the dialog with distinct codes
    QObject::connect(localBtn, &QPushButton::clicked, &modeDlg, [&modeDlg]()
                     { modeDlg.done(1); });
    QObject::connect(linkBtn, &QPushButton::clicked, &modeDlg, [&modeDlg]()
                     { modeDlg.done(2); });
    const int ret = modeDlg.exec();

    if (ret == 1)
    {
        // 用户选择本地模式：选择模型并启动本地 llama-server
        currentpath = customOpenfile(currentpath, jtr("load_button_tooltip"), "(*.bin *.gguf)");
        // 允许选择与上次相同的模型路径以便重新装载（如服务器已停止或需要重试）
        if (currentpath == "")
        {
            return; // 路径未选择或与上次相同
        }
        ui_mode = LOCAL_MODE;      // 本地模式 -> 使用本地llama-server + xNet
        historypath = currentpath; // 记录这个路径
        ui_SETTINGS.modelpath = currentpath;
        ui_SETTINGS.mmprojpath = ""; // 清空mmproj模型路径
        ui_SETTINGS.lorapath = "";   // 清空lora模型路径
        is_load = false;
        monitor_timer.stop();
        firstAutoNglEvaluated_ = false; // 新模型：允许重新评估一次是否可全量 offload
        // 启动/重启本地llama-server（内部会根据是否需要重启来切换到“装载中”状态）
        ensureLocalServer();
    }
    else if (ret == 2)
    {
        // 用户选择链接模式：打开链接设置对话框
        ui_state_info = "ui:" + jtr("clicked") + jtr("link") + jtr("set");
        reflash_state(ui_state_info, SIGNAL_SIGNAL);
        // 预填：优先使用已持久化的链接配置，避免本地模式切换时被覆盖
        {
            QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
            settings.setIniCodec("utf-8");
            const QString ep = settings.value("api_endpoint", apis.api_endpoint).toString();
            const QString key = settings.value("api_key", apis.api_key).toString();
            const QString model = settings.value("api_model", apis.api_model).toString();
            api_endpoint_LineEdit->setText(ep);
            api_key_LineEdit->setText(key);
            api_model_LineEdit->setText(model);
        }
        api_dialog->exec(); // 确定后触发 set_api()
    }
    else
    {
        // 用户关闭对话框（未选择） -> 不做任何事
        return;
    }
}

// 模型释放完毕并重新装载
void Widget::recv_freeover_loadlater()
{
    gpu_wait_load = true;
    emit gpu_reflash(); // 强制刷新gpu信息
}

// 装载前动作
void Widget::preLoad()
{
    is_load = false; // 重置is_load标签
    if (ui_state == CHAT_STATE)
    {
        ui->output->clear(); // 清空输出区
    }
    ui->state->clear(); // 清空状态区
    // 清空记录条与记录锚点，避免重新装载后残留旧节点
    recordClear();
    ui_state_loading(); // 装载中界面状态
    // 开始“装载中”转轮动画并计时（复用解码动画作为统一的简单动画）
    wait_play("load model");
    load_timer.start();
    if (is_config)
    {
        QString relativePath = applicationDirPath + "/EVA_TEMP/eva_config.ini";
        QFileInfo fileInfo(relativePath);
        QString absolutePath = fileInfo.absoluteFilePath();
        is_config = false;
        reflash_state("ui:" + jtr("apply_config_mess") + " " + absolutePath, USUAL_SIGNAL);
    }
    reflash_state("ui:" + jtr("model location") + " " + ui_SETTINGS.modelpath, USUAL_SIGNAL);
}

// xBot 装载完成的槽已废弃；改由 onServerReady() 收尾本地动画

// 用户点击发出按钮处理

// Build common endpoint data for a turn
ENDPOINT_DATA Widget::prepareEndpointData()
{
    ENDPOINT_DATA d;
    d.date_prompt = ui_DATES.date_prompt;
    d.stopwords = ui_DATES.extra_stop_words;
    d.is_complete_state = (ui_state == COMPLETE_STATE);
    d.temp = ui_SETTINGS.temp;
    d.repeat = ui_SETTINGS.repeat;
    d.top_k = ui_SETTINGS.top_k;
    d.top_p = ui_SETTINGS.hid_top_p;
    d.n_predict = ui_SETTINGS.hid_npredict;
    d.messagesArray = ui_messagesArray;
    d.id_slot = currentSlotId_;
    return d;
}

// Start a persistent history session if needed (chat mode only)
void Widget::beginSessionIfNeeded()
{
    if (!(history_ && ui_state == CHAT_STATE && history_->sessionId().isEmpty())) return;
    SessionMeta meta;
    meta.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    meta.title = "";
    meta.endpoint = (ui_mode == LINK_MODE) ? (apis.api_endpoint + ((ui_state == CHAT_STATE) ? apis.api_chat_endpoint : apis.api_completion_endpoint))
                                           : (serverManager ? serverManager->endpointBase() : "");
    meta.model = (ui_mode == LINK_MODE) ? apis.api_model : ui_SETTINGS.modelpath;
    meta.system = ui_DATES.date_prompt;
    meta.n_ctx = ui_SETTINGS.nctx;
    meta.slot_id = currentSlotId_;
    meta.startedAt = QDateTime::currentDateTime();
    history_->begin(meta);
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    history_->appendMessage(systemMessage);
}

// Collect text/images/audio inputs from UI (includes monitor frames if enabled)
void Widget::collectUserInputs(InputPack &pack)
{
    pack.text.clear();
    // Only collect user text when we are NOT in a tool loop. The current task
    // is already logged by on_send_clicked(); do not log here to avoid
    // duplicate/misleading "current task" lines.
    if (tool_result.isEmpty())
    {
        pack.text = ui->input->textEdit->toPlainText().toUtf8().data();
        ui->input->textEdit->clear();
    }
    pack.images = ui->input->imageFilePaths();
    if (ui_mode == LOCAL_MODE && ui_state == CHAT_STATE && !monitorFrames_.isEmpty())
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 cutoff = nowMs - qint64(kMonitorKeepSeconds_) * 1000;
        while (!monitorFrames_.isEmpty() && monitorFrames_.front().tsMs < cutoff)
        {
            const QString old = monitorFrames_.front().path;
            monitorFrames_.pop_front();
            QFile f(old);
            if (f.exists()) f.remove();
        }
        for (const auto &mf : monitorFrames_) pack.images.append(mf.path);
    }
    pack.wavs = ui->input->wavFilePaths();
    ui->input->clearThumbnails();
}

// Handle normal chat reply turn
void Widget::handleChatReply(ENDPOINT_DATA &data, const InputPack &in)
{
    // user message assembly
    if (in.images.isEmpty())
    {
        QJsonObject roleMessage;
        roleMessage.insert("role", DEFAULT_USER_NAME);
        roleMessage.insert("content", in.text);
        ui_messagesArray.append(roleMessage);
        if (history_) history_->appendMessage(roleMessage);
    }
    else
    {
        QJsonObject message;
        message["role"] = DEFAULT_USER_NAME;
        QJsonArray contentArray;
        if (!in.text.isEmpty())
        {
            QJsonObject textMessage;
            textMessage.insert("type", "text");
            textMessage.insert("text", in.text);
            contentArray.append(textMessage);
        }
        for (int i = 0; i < in.images.size(); ++i)
        {
            QFile imageFile(in.images[i]);
            if (!imageFile.open(QIODevice::ReadOnly))
            {
                qDebug() << "Failed to open image file";
                continue;
            }
            QByteArray imageData = imageFile.readAll();
            QByteArray base64Data = imageData.toBase64();
            QString base64String = QString("data:image/jpeg;base64,") + base64Data;
            QJsonObject imageObject;
            imageObject["type"] = "image_url";
            QJsonObject imageUrlObject;
            imageUrlObject["url"] = base64String;
            imageObject["image_url"] = imageUrlObject;
            contentArray.append(imageObject);
            showImages({in.images[i]});
        }
        message["content"] = contentArray;
        ui_messagesArray.append(message);
        // Persist a history-only copy with local file paths for reliable restoration
        if (history_)
        {
            QJsonObject histMsg = message; // copy
            QJsonArray locals;
            for (const QString &p : in.images)
            {
                // Store absolute path to improve robustness across cwd changes
                locals.append(QFileInfo(p).absoluteFilePath());
            }
            if (!locals.isEmpty()) histMsg.insert("local_images", locals);
            history_->appendMessage(histMsg);
        }
    }
    if (!in.wavs.isEmpty())
    {
        QJsonObject message;
        message["role"] = DEFAULT_USER_NAME;
        QJsonArray contentArray;
        for (int i = 0; i < in.wavs.size(); ++i)
        {
            QString filePath = in.wavs[i];
            QFile audioFile(filePath);
            if (!audioFile.open(QIODevice::ReadOnly))
            {
                qDebug() << "Failed to open audio file:" << filePath;
                continue;
            }
            QByteArray audioData = audioFile.readAll();
            QByteArray base64Data = audioData.toBase64();
            QFileInfo fileInfo(filePath);
            QString extension = fileInfo.suffix().toLower();
            QString mimeType = "audio/mpeg";
            if (extension == "wav")
                mimeType = "audio/wav";
            else if (extension == "ogg")
                mimeType = "audio/ogg";
            else if (extension == "flac")
                mimeType = "audio/flac";
            QString base64String = QString("data:%1;base64,").arg(mimeType) + base64Data;
            QJsonObject audioObject;
            audioObject["type"] = "audio_url";
            QJsonObject audioUrlObject;
            audioUrlObject["url"] = base64String;
            audioObject["audio_url"] = audioUrlObject;
            contentArray.append(audioObject);
            showImages({":/logo/wav.png"});
        }
        if (!contentArray.isEmpty())
        {
            message["content"] = contentArray;
            ui_messagesArray.append(message);
            if (history_) history_->appendMessage(message);
        }
    }
    data.messagesArray = ui_messagesArray;
    // Create record BEFORE printing header/content so docFrom anchors at header line
    int __idx = recordCreate(RecordRole::User);
    appendRoleHeader(QStringLiteral("user"));
    reflash_output(in.text, 0, themeTextPrimary());
    // After content is printed, update record's text and docTo, and link msgIndex
    recordAppendText(__idx, in.text);
    if (!ui_messagesArray.isEmpty()) { recordEntries_[__idx].msgIndex = ui_messagesArray.size() - 1; }
    data.n_predict = ui_SETTINGS.hid_npredict;
    emit ui2net_data(data);
    emit ui2net_push();
    if (ui_mode == LOCAL_MODE && ui_state == CHAT_STATE && !monitorFrames_.isEmpty()) monitorFrames_.clear();
}

// Handle completion mode turn
void Widget::handleCompletion(ENDPOINT_DATA &data)
{
    data.input_prompt = ui->output->toPlainText();
    data.n_predict = ui_SETTINGS.hid_npredict;
    emit ui2net_data(data);
    emit ui2net_push();
}

// Handle tool loop: append tool message and schedule a continue tick
void Widget::handleToolLoop(ENDPOINT_DATA &data)
{
    Q_UNUSED(data);
    toolInvocationActive_ = false;
    QJsonObject roleMessage;
    roleMessage.insert("role", QStringLiteral("tool"));
    roleMessage.insert("content", tool_result);
    ui_messagesArray.append(roleMessage);
    if (history_ && ui_state == CHAT_STATE)
    {
        // Include any pending image paths produced by tools so we can restore them later
        QJsonObject histMsg = roleMessage;
        if (!wait_to_show_images_filepath.isEmpty())
        {
            QJsonArray locals;
            for (const QString &p : wait_to_show_images_filepath)
                locals.append(QFileInfo(p).absoluteFilePath());
            histMsg.insert("local_images", locals);
        }
        history_->appendMessage(histMsg);
    }

    // Create record BEFORE printing header/content so docFrom anchors at header area
    int __idx = recordCreate(RecordRole::Tool);
    appendRoleHeader(QStringLiteral("tool"));
    reflash_output(tool_result, 0, themeStateColor(TOOL_SIGNAL));
    recordAppendText(__idx, tool_result);
    if (!ui_messagesArray.isEmpty()) { recordEntries_[__idx].msgIndex = ui_messagesArray.size() - 1; }

    tool_result = "";
    QTimer::singleShot(100, this, SLOT(tool_testhandleTimeout()));
    is_run = true;
    ui_state_pushing();
}

void Widget::logCurrentTask(ConversationTask task)
{
    Q_UNUSED(task);
    // QString name;
    // switch (task)
    // {
    // case ConversationTask::ChatReply: name = QStringLiteral("chat-reply"); break;
    // case ConversationTask::Completion: name = QStringLiteral("completion"); break;
    // case ConversationTask::ToolLoop: name = QStringLiteral("tool-loop"); break;
    // }
    // // Prefer i18n key if present
    // const QString label = jtr("current task");
    // if (!label.isEmpty())
    //     reflash_state(QStringLiteral("ui:") + label + QStringLiteral(" ") + name, SIGNAL_SIGNAL);
    // else
    //     reflash_state(QStringLiteral("ui:current task ") + name, SIGNAL_SIGNAL);
}

void Widget::on_send_clicked()
{
    // Reset headers and kv tracker
    turnThinkHeaderPrinted_ = false;
    turnAssistantHeaderPrinted_ = false;
    turnThinkActive_ = false;
    sawPromptPast_ = false;
    sawFinalPast_ = false;
    // reflash_state("ui:" + jtr("clicked send"), SIGNAL_SIGNAL);
    turnActive_ = true;
    kvUsedBeforeTurn_ = kvUsed_;
    // Fresh turn: clear prompt/output trackers before new network call
    kvTokensTurn_ = 0;
    kvPromptTokensTurn_ = 0;
    kvStreamedTurn_ = 0;
    // if (ui_mode == LINK_MODE)
    // {
    //     reflash_state(QStringLiteral("link:turn begin used_before=%1 total_used=%2")
    //                       .arg(kvUsedBeforeTurn_)
    //                       .arg(kvUsed_));
    // }

    emit ui2net_stop(0);
    ENDPOINT_DATA data = prepareEndpointData();

    if (ui_state == CHAT_STATE) beginSessionIfNeeded();

    if (!tool_result.isEmpty())
    {
        currentTask_ = ConversationTask::ToolLoop;
        logCurrentTask(currentTask_);
        handleToolLoop(data);
        return;
    }

    if (ui_state == CHAT_STATE)
    {
        currentTask_ = ConversationTask::ChatReply;
        logCurrentTask(currentTask_);
        InputPack in;
        collectUserInputs(in);
        handleChatReply(data, in);
    }
    else
    {
        currentTask_ = ConversationTask::Completion;
        logCurrentTask(currentTask_);
        handleCompletion(data);
    }

    is_run = true;
    ui_state_pushing();
}
void Widget::recv_pushover()
{
    // Separate all reasoning (<think>...</think>) blocks from final content; capture both roles
    QString finalText = temp_assistant_history;
    const QString tBegin = QString(DEFAULT_THINK_BEGIN);
    const QString tEnd = QString(DEFAULT_THINK_END);
    QStringList reasonings;
    int searchPos = 0;
    while (true)
    {
        int s = finalText.indexOf(tBegin, searchPos);
        if (s == -1) break;
        int e = finalText.indexOf(tEnd, s + tBegin.size());
        if (e == -1) break; // unmatched tail -> leave as is
        const int rStart = s + tBegin.size();
        reasonings << finalText.mid(rStart, e - rStart);
        // remove the whole <think>...</think> segment from finalText
        finalText.remove(s, (e + tEnd.size()) - s);
        searchPos = s; // continue scanning from removal point
    }
    const QString reasoningText = reasonings.join("");
    // Append think and assistant messages to UI array/history
    if (!reasoningText.isEmpty())
    {
        QJsonObject thinkMsg;
        thinkMsg.insert("role", QStringLiteral("think"));
        thinkMsg.insert("content", reasoningText);
        ui_messagesArray.append(thinkMsg);
        if (history_ && ui_state == CHAT_STATE) history_->appendMessage(thinkMsg);
        if (currentThinkIndex_ >= 0) { recordEntries_[currentThinkIndex_].msgIndex = ui_messagesArray.size() - 1; }
    }
    QJsonObject roleMessage;
    roleMessage.insert("role", DEFAULT_MODEL_NAME);
    roleMessage.insert("content", finalText);
    ui_messagesArray.append(roleMessage);
    if (history_ && ui_state == CHAT_STATE)
    {
        history_->appendMessage(roleMessage);
    }
    if (currentAssistantIndex_ >= 0)
    {
        recordEntries_[currentAssistantIndex_].msgIndex = ui_messagesArray.size() - 1;
    }
    currentThinkIndex_ = -1;
    currentAssistantIndex_ = -1;
    temp_assistant_history = "";

    if (ui_state == COMPLETE_STATE) // 补完模式的回答只输出一次
    {
        normal_finish_pushover();
        on_reset_clicked(); // 自动重置
    }
    else
    {
        // 工具链开关开启时，尝试解析工具 JSON
        if (is_load_tool)
        {
            QString tool_str = ui_messagesArray.last().toObject().value("content").toString();
            tools_call = XMLparser(tool_str);
            if (tools_call.empty())
            {
                normal_finish_pushover();
            }
            else
            {
                if (tools_call.contains("name") && tools_call.contains("arguments"))
                {
                    QString tools_name = QString::fromStdString(tools_call.value("name", ""));
                    reflash_state("ui:" + jtr("clicked") + " " + tools_name, SIGNAL_SIGNAL);
                    // 工具层面指出结束
                    if (tools_name == "answer" || tools_name == "response")
                    {
                        normal_finish_pushover();
                    }
                    else
                    {
                        // Before entering tool loop, correct LINK memory by subtracting this turn's reasoning tokens
                        if (ui_mode == LINK_MODE && lastReasoningTokens_ > 0)
                        {
                            // Reasoning text is not sent back in LINK mode, so exclude it from memory usage
                            kvUsed_ = qMax(0, kvUsed_ - lastReasoningTokens_);
                            kvStreamedTurn_ = qMax(0, kvStreamedTurn_ - lastReasoningTokens_);
                            updateKvBarUi();
                            lastReasoningTokens_ = 0;
                        }
                        toolInvocationActive_ = true;
                        emit ui2tool_exec(tools_call);
                        // use tool; decoding remains paused
                    }
                }
            }
        }
        else
        {
            normal_finish_pushover();
        }
    }
}

// 正常情况处理推理完毕
void Widget::normal_finish_pushover()
{
    turnThinkActive_ = false;
    // Reset per-turn header flags
    is_run = false;
    ui_state_normal(); // 待机界面状态
    // LINK mode: final correction of memory by excluding this turn's reasoning tokens
    if (ui_mode == LINK_MODE && lastReasoningTokens_ > 0)
    {
        kvUsed_ = qMax(0, kvUsed_ - lastReasoningTokens_);
        kvStreamedTurn_ = qMax(0, kvStreamedTurn_ - lastReasoningTokens_);
        kvTokensTurn_ = kvPromptTokensTurn_ + qMax(0, kvStreamedTurn_);
        updateKvBarUi();
        lastReasoningTokens_ = 0;
    }
    if (ui_mode == LINK_MODE)
    {
        kvTokensTurn_ = kvPromptTokensTurn_ + qMax(0, kvStreamedTurn_);
        // reflash_state(QStringLiteral("link:turn complete prompt=%1 stream=%2 turn=%3 used=%4 used_before=%5")
        //                   .arg(kvPromptTokensTurn_)
        //                   .arg(kvStreamedTurn_)
        //                   .arg(kvTokensTurn_)
        //                   .arg(kvUsed_)
        //                   .arg(kvUsedBeforeTurn_));
    }
    decode_finish();
    if (!wait_to_show_images_filepath.isEmpty())
    {
        showImages(wait_to_show_images_filepath);
        wait_to_show_images_filepath.clear();
    }
}

// 处理tool推理完毕的槽
void Widget::recv_toolpushover(QString tool_result_)
{
    toolInvocationActive_ = false;
    if (tool_result_.contains("<ylsdamxssjxxdd:showdraw>")) // 有图像要显示的情况
    {
        wait_to_show_images_filepath.append(tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1]); // 文生图后待显示图像的图像路径
        tool_result = "stablediffusion " + jtr("call successful, image save at") + " " + tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1];
    }
    else
    {
        tool_result = tool_result_;
        tool_result = truncateString(tool_result, MAX_INPUT); // 超出最大输入的部分截断
    }

    on_send_clicked(); // 触发发送继续预测下一个词
}

void Widget::collapseTerminalPane()
{
    if (!ui->statusTerminalSplitter || !ui->terminalPane) return;
    QList<int> sizes;
    sizes << 1 << 0;
    ui->statusTerminalSplitter->setSizes(sizes);
    terminalCollapsed_ = true;
}

void Widget::ensureTerminalPaneVisible()
{
    if (!ui->statusTerminalSplitter || !ui->terminalPane) return;
    if (!terminalCollapsed_) return;
    const int available = ui->statusTerminalSplitter->size().width();
    int desired = qMax(terminalAutoExpandSize_, 240);
    if (available > 0)
    {
        const int maxAllowed = qMax(240, available - 160);
        desired = qMin(desired, maxAllowed);
        if (desired <= 0) desired = qMax(available / 2, 240);
    }
    else
    {
        desired = qMax(desired, 320);
    }
    int left = (available > 0) ? qMax(available - desired, 240) : desired * 2;
    if (left <= 0) left = desired;
    QList<int> sizes;
    sizes << left << desired;
    ui->statusTerminalSplitter->setSizes(sizes);
    terminalAutoExpandSize_ = qMax(240, desired);
    terminalCollapsed_ = false;
}

void Widget::toolCommandStarted(const QString &command, const QString &workingDir)
{
    ensureTerminalPaneVisible();
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalStart(command, workingDir);
    }
}

void Widget::toolCommandStdout(const QString &chunk)
{
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalStdout(chunk);
    }
}

void Widget::toolCommandStderr(const QString &chunk)
{
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalStderr(chunk);
    }
}

void Widget::toolCommandFinished(int exitCode, bool interrupted)
{
    if (ui->terminalPane)
    {
        ui->terminalPane->handleExternalFinished(exitCode, interrupted);
    }
}

void Widget::onTerminalInterruptRequested()
{
    emit ui2tool_interruptCommand();
}

// 停止完毕的后处理
void Widget::recv_stopover()
{
    if (ui_state == COMPLETE_STATE)
    {
        ui->reset->click();
    } // 补完模式终止后需要重置
}

// 重置完毕的后处理
void Widget::recv_resetover()
{
    if (ui_SETTINGS.ngl == 0)
    {
        EVA_icon = QIcon(":/logo/blue_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    } // 恢复
    else
    {
        EVA_icon = QIcon(":/logo/green_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    } // 恢复
    reflash_state("ui:" + jtr("reset ok"), SUCCESS_SIGNAL);

    updateMonitorTimer();
}

// 设置参数改变,重载模型
void Widget::recv_reload()
{
    preLoad(); // 装载前动作
}

// bot发信号请求ui触发reset,针对约定
void Widget::recv_datereset()
{
    // 打印约定的系统指令
    ui_state_info = "···········" + jtr("date") + "···········";
    reflash_state(ui_state_info, USUAL_SIGNAL);
    if (ui_state == COMPLETE_STATE)
    {
        reflash_state("· " + jtr("complete mode") + jtr("on") + " ", USUAL_SIGNAL);
    }
    else
    {
        reflash_state("· " + jtr("system calling") + " " + date_ui->date_prompt_TextEdit->toPlainText() + ui_extra_prompt, USUAL_SIGNAL);
        // //展示额外停止标志
        // QString stop_str;
        // stop_str = jtr("extra stop words") + " ";
        // // stop_str += bot_chat.input_prefix + " ";
        // for (int i = 0; i < ui_DATES.extra_stop_words.size(); ++i) {
        //     stop_str += ui_DATES.extra_stop_words.at(i) + " ";
        // }

        // reflash_state("· " + stop_str + " ", USUAL_SIGNAL);
    }
    reflash_state("···········" + jtr("date") + "···········", USUAL_SIGNAL);
    auto_save_user(); // 保存ui配置

    ui->reset->click();
}

// bot发信号请求ui触发reset,针对设置
void Widget::recv_setreset()
{
    // 打印设置内容
    reflash_state("···········" + jtr("set") + "···········", USUAL_SIGNAL);

    reflash_state("· " + jtr("temperature") + " " + QString::number(ui_SETTINGS.temp), USUAL_SIGNAL);
    reflash_state("· " + jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat), USUAL_SIGNAL);
    reflash_state("· " + jtr("npredict") + " " + QString::number(ui_SETTINGS.hid_npredict), USUAL_SIGNAL);
    reflash_state("· gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl), USUAL_SIGNAL);
    reflash_state("· cpu" + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread), USUAL_SIGNAL);
    reflash_state("· " + jtr("ctx") + jtr("length") + " " + QString::number(ui_SETTINGS.nctx), USUAL_SIGNAL);
    reflash_state("· " + jtr("batch size") + " " + QString::number(ui_SETTINGS.hid_batch), USUAL_SIGNAL);

    if (ui_SETTINGS.lorapath != "")
    {
        reflash_state("ui:" + jtr("load lora") + " " + ui_SETTINGS.lorapath, USUAL_SIGNAL);
    }
    if (ui_SETTINGS.mmprojpath != "")
    {
        reflash_state("ui:" + jtr("load mmproj") + " " + ui_SETTINGS.mmprojpath, USUAL_SIGNAL);
    }
    if (ui_state == CHAT_STATE)
    {
        reflash_state("· " + jtr("chat mode"), USUAL_SIGNAL);
    }
    else if (ui_state == COMPLETE_STATE)
    {
        reflash_state("· " + jtr("complete mode"), USUAL_SIGNAL);
    }

    // 展示额外停止标志
    //  if (ui_state == CHAT_STATE) {
    //      QString stop_str;
    //      stop_str = jtr("extra stop words") + " ";
    //      for (int i = 0; i < ui_DATES.extra_stop_words.size(); ++i) {
    //          stop_str += ui_DATES.extra_stop_words.at(i) + " ";
    //      }
    //      reflash_state("· " + stop_str + " ", USUAL_SIGNAL);
    //  }

    reflash_state("···········" + jtr("set") + "···········", USUAL_SIGNAL);
    auto_save_user(); // 保存ui配置

    ui->reset->click();
}

// 用户点击重置按钮的处理,重置模型以及对话,并设置约定的参数
void Widget::on_reset_clicked()
{
    if (toolInvocationActive_)
    {
        emit ui2tool_cancelActive();
        toolInvocationActive_ = false;
        tool_result.clear();
        turnActive_ = false;
        is_run = false;
        decode_finish();
        ui_state_normal();
        reflash_state("ui:tool cancelled", SIGNAL_SIGNAL);
        return;
    }

    emit ui2tool_cancelActive();
    wait_to_show_images_filepath.clear(); // 清空待显示图像
    emit ui2expend_resettts();            // 清空待读列表
    tool_result = "";                     // 清空工具结果
    // 如果模型正在推理就改变模型的停止标签
    if (is_run)
    {
        reflash_state("ui:" + jtr("clicked") + jtr("shut down"), SIGNAL_SIGNAL);
        emit ui2net_stop(1);
        // 传递推理停止信号,模型停止后会再次触发on_reset_clicked()
        return;
    }

    if (date_ui)
    {
        // Rebuild system prompt so workspace snapshot reflects current workspace state.
        ui_extra_prompt = create_extra_prompt();
        get_date();
    }

    // reflash_state("ui:" + jtr("clicked reset"), SIGNAL_SIGNAL);
    // Reset all per-turn token counters now that the session is cleared
    kvTokensTurn_ = 0;
    kvPromptTokensTurn_ = 0;
    // reset KV/speed tracking and progress bar
    kvUsed_ = 0;
    kvUsedBeforeTurn_ = 0;
    kvStreamedTurn_ = 0;
    turnActive_ = false;
    updateKvBarUi();
    currentSlotId_ = -1; // new conversation -> no slot yet
    // Reset output safely. Replacing the QTextDocument drops any cached
    // resources/undo stack without risking double-deletes.
    // Note: QTextEdit takes ownership of the previous document and will
    // delete it; do not manually delete the old one here.
    if (ui_state == CHAT_STATE) resetOutputDocument();
    ui_state_normal();
    recordClear(); // 待机界面状态

    // 请求式统一处理（本地/远端）
    ui_messagesArray = QJsonArray(); // 清空
    // 构造系统指令
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    ui_messagesArray.append(systemMessage);
    if (ui_state == CHAT_STATE)
    {
        // Create record BEFORE header so gotoRecord can place role name at top
        int __idx = recordCreate(RecordRole::System);
        appendRoleHeader(QStringLiteral("system"));
        reflash_output(ui_DATES.date_prompt, 0, themeTextPrimary());
        recordAppendText(__idx, ui_DATES.date_prompt);
        if (!ui_messagesArray.isEmpty())
        {
            int mi = ui_messagesArray.size() - 1;
            recordEntries_[__idx].msgIndex = mi;
        }
    }

    // Do not record reset into history; clear current session only
    if (history_) history_->clearCurrent();

    if (ui_mode == LINK_MODE)
    {
        // 远端模式：显示当前端点
        current_api = (ui_state == CHAT_STATE) ? (apis.api_endpoint + apis.api_chat_endpoint)
                                               : (apis.api_endpoint + apis.api_completion_endpoint);
        EVA_icon = QIcon(":/logo/dark_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon);
        EVA_title = jtr("current api") + " " + current_api;
        reflash_state(QString("ui:") + EVA_title, USUAL_SIGNAL);
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
    }
    else // LOCAL_MODE：显示当前模型，保持本地装载表现
    {
        QString modelName = ui_SETTINGS.modelpath.split("/").last();
        EVA_title = jtr("current model") + " " + modelName;
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        if (ui_SETTINGS.ngl == 0)
        {
            EVA_icon = QIcon(":/logo/blue_logo.png");
        }
        else
        {
            EVA_icon = QIcon(":/logo/green_logo.png");
        }
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon);
    }
    return;
}

// 用户点击约定按钮处理
void Widget::on_date_clicked()
{
    // reflash_state("ui:" + jtr("clicked date"), SIGNAL_SIGNAL);

    // 展示最近一次设置值
    date_ui->chattemplate_comboBox->setCurrentText(ui_template); // 默认使用default的提示词模板
    date_ui->date_prompt_TextEdit->setPlainText(ui_date_prompt);

    date_ui->calculator_checkbox->setChecked(ui_calculator_ischecked);
    date_ui->knowledge_checkbox->setChecked(ui_knowledge_ischecked);
    date_ui->stablediffusion_checkbox->setChecked(ui_stablediffusion_ischecked);
    date_ui->controller_checkbox->setChecked(ui_controller_ischecked);
    date_ui->MCPtools_checkbox->setChecked(ui_MCPtools_ischecked);
    date_ui->engineer_checkbox->setChecked(ui_engineer_ischecked);
    if (date_ui->date_engineer_workdir_LineEdit)
    {
        date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
        const bool vis = date_ui->engineer_checkbox->isChecked();
        date_ui->date_engineer_workdir_label->setVisible(vis);
        date_ui->date_engineer_workdir_LineEdit->setVisible(vis);
        date_ui->date_engineer_workdir_browse->setVisible(vis);
        updateSkillVisibility(vis);
        if (vis) refreshSkillsUI();
    }

    date_ui->switch_lan_button->setText(ui_extra_lan);

    date_dialog->exec();
}

// 应用用户设置的约定内容
void Widget::set_date()
{
    // 如果用户在“约定”对话框中修改了工程师工作目录，点击“确定”后立即生效
    // 仅当已勾选“软件工程师”工具时才向 xTool 下发（未勾选时仅保存值供下次使用）
    if (date_ui && date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked() && date_ui->date_engineer_workdir_LineEdit)
    {
        const QString typed = date_ui->date_engineer_workdir_LineEdit->text().trimmed();
        if (!typed.isEmpty())
        {
            const QString norm = QDir::cleanPath(typed);
            if (norm != engineerWorkDir)
            {
                // setEngineerWorkDir 会更新成员变量、同步到 xTool，并刷新行编辑显示
                setEngineerWorkDir(norm);
            }
        }
    }

    // 同步其余“约定”参数到内存
    get_date(); // 获取约定中的纸面值
    updateSkillVisibility(ui_engineer_ischecked);
    if (ui_engineer_ischecked) refreshSkillsUI();

    // 约定变化后统一重置对话上下文（本地/远端一致）并持久化
    auto_save_user(); // persist date settings

    on_reset_clicked();

    date_dialog->close();
}

// 用户取消约定
void Widget::cancel_date()
{
    // 还原工具选择
    date_ui->calculator_checkbox->setChecked(ui_calculator_ischecked);
    date_ui->controller_checkbox->setChecked(ui_controller_ischecked);
    date_ui->knowledge_checkbox->setChecked(ui_knowledge_ischecked);
    date_ui->stablediffusion_checkbox->setChecked(ui_stablediffusion_ischecked);
    date_ui->MCPtools_checkbox->setChecked(ui_MCPtools_ischecked);
    date_ui->engineer_checkbox->setChecked(ui_engineer_ischecked);
    if (date_ui->date_engineer_workdir_LineEdit)
    {
        date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
        const bool vis = date_ui->engineer_checkbox->isChecked();
        date_ui->date_engineer_workdir_label->setVisible(vis);
        date_ui->date_engineer_workdir_LineEdit->setVisible(vis);
        date_ui->date_engineer_workdir_browse->setVisible(vis);
        updateSkillVisibility(vis);
        if (vis) refreshSkillsUI();
    }
    date_ui->switch_lan_button->setText(ui_extra_lan);
    // 复原语言
    if (ui_extra_lan == "zh")
    {
        language_flag = 0;
    }
    else if (ui_extra_lan == "en")
    {
        language_flag = 1;
    }
    apply_language(language_flag);
    emit ui2tool_language(language_flag);
    emit ui2net_language(language_flag);
    emit ui2expend_language(language_flag);
    // 重新判断是否挂载了工具
    if (date_ui->calculator_checkbox->isChecked() || date_ui->engineer_checkbox->isChecked() || date_ui->MCPtools_checkbox->isChecked() || date_ui->knowledge_checkbox->isChecked() || date_ui->controller_checkbox->isChecked() || date_ui->stablediffusion_checkbox->isChecked())
    {
        if (is_load_tool == false)
        {
            reflash_state("ui:" + jtr("enable output parser"), SIGNAL_SIGNAL);
        }
        is_load_tool = true;
    }
    else
    {
        if (is_load_tool == true)
        {
            reflash_state("ui:" + jtr("disable output parser"), SIGNAL_SIGNAL);
        }
        is_load_tool = false;
    }
}

// 用户点击设置按钮响应
void Widget::on_set_clicked()
{
    reflash_state("ui:" + jtr("clicked") + jtr("set"), SIGNAL_SIGNAL);
    if (ui_state == CHAT_STATE)
    {
        settings_ui->chat_btn->setChecked(1), chat_change();
    }
    else if (ui_state == COMPLETE_STATE)
    {
        settings_ui->complete_btn->setChecked(1), complete_change();
    }
    // 服务模式已移除
    // 展示最近一次设置值
    settings_ui->temp_slider->setValue(qRound(ui_SETTINGS.temp * 100.0));
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0));
    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
    settings_ui->nctx_slider->setValue(ui_SETTINGS.nctx);
    settings_ui->repeat_slider->setValue(qRound(ui_SETTINGS.repeat * 100.0));
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0));
    // Ensure top-k/top-p sliders reflect last confirmed settings on every open
    settings_ui->topk_slider->setValue(ui_SETTINGS.top_k);
    settings_ui->topp_slider->setValue(qRound(ui_SETTINGS.hid_top_p * 100.0));
    {
        const double val = settings_ui->topp_slider->value() / 100.0;
        settings_ui->topp_label->setText("TOP_P " + QString::number(val));
        settings_ui->topp_label->setToolTip(QString::fromUtf8("核采样阈值（top_p），范围 0.00–1.00；当前：%1").arg(QString::number(val, 'f', 2)));
    }
    settings_ui->lora_LineEdit->setText(ui_SETTINGS.lorapath);
    settings_ui->mmproj_LineEdit->setText(ui_SETTINGS.mmprojpath);
    settings_ui->nthread_slider->setValue(ui_SETTINGS.nthread);
    settings_ui->port_lineEdit->setText(ui_port);
    // 打开设置时记录当前设置快照，用于确认时判断是否有修改
    settings_snapshot_ = ui_SETTINGS;
    port_snapshot_ = ui_port;
    device_snapshot_ = settings_ui->device_comboBox->currentText().trimmed().toLower();
    // Ensure device label reflects current auto->(effective) preview before showing the dialog
    refreshDeviceBackendUI();
    applySettingsDialogSizing();
    settings_dialog->exec();
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

// 从SQLite加载并还原历史会话
void Widget::restoreSessionById(const QString &sessionId)
{
    if (!history_) return;
    recordClear();
    SessionMeta meta;
    QJsonArray msgs;
    if (!history_->loadSession(sessionId, meta, msgs))
    {
        reflash_state(jtr("history db error"), WRONG_SIGNAL);
        return;
    }
    if (!history_->resume(sessionId))
    {
        reflash_state(jtr("history db error"), WRONG_SIGNAL);
    }

    ui->output->clear();
    ui_messagesArray = QJsonArray();

    // Helper to map role string to UI color and record role
    auto roleToRecord = [](const QString &r) -> RecordRole
    {
        if (r == QLatin1String("system")) return RecordRole::System;
        if (r == QLatin1String("user")) return RecordRole::User;
        if (r == QLatin1String("assistant")) return RecordRole::Assistant;
        if (r == QLatin1String("think")) return RecordRole::Think;
        if (r == QLatin1String("tool")) return RecordRole::Tool;
        return RecordRole::User;
    };
    auto roleToColor = [&](const QString &r) -> QColor
    {
        if (r == QLatin1String("think")) return themeThinkColor();
        if (r == QLatin1String("tool")) return themeStateColor(TOOL_SIGNAL);
        return themeTextPrimary();
    };

    for (const auto &v : msgs)
    {
        QJsonObject m = v.toObject();
        const QString role = m.value("role").toString();
        const QJsonValue contentVal = m.value("content");

        // Build displayable text from content (string or multimodal array)
        QString displayText;
        if (contentVal.isArray())
        {
            const QJsonArray parts = contentVal.toArray();
            for (const auto &pv : parts)
            {
                if (!pv.isObject()) continue;
                const QJsonObject po = pv.toObject();
                const QString type = po.value("type").toString();
                if (type == QLatin1String("text"))
                    displayText += po.value("text").toString();
            }
        }
        else
        {
            QString s = contentVal.isString() ? contentVal.toString() : contentVal.toVariant().toString();
            s.replace(QString(DEFAULT_THINK_BEGIN), QString());
            s.replace(QString(DEFAULT_THINK_END), QString());
            displayText = s;
        }

        // Create record entry and print role header + content
        const int recIdx = recordCreate(roleToRecord(role));
        appendRoleHeader(role);
        reflash_output(displayText, 0, roleToColor(role));
        recordAppendText(recIdx, displayText);

        // Append sanitized message back to UI memory (remove local-only metadata)
        QJsonObject uiMsg = m;
        uiMsg.remove("local_images");
        ui_messagesArray.append(uiMsg);
        recordEntries_[recIdx].msgIndex = ui_messagesArray.size() - 1;

        // Restore any local images recorded in history
        QStringList localPaths;
        const QJsonValue localsVal = m.value("local_images");
        if (localsVal.isArray())
        {
            for (const auto &lv : localsVal.toArray())
            {
                if (lv.isString()) localPaths << lv.toString();
            }
        }
        if (!localPaths.isEmpty())
        {
            QStringList showable;
            for (const QString &p : localPaths)
            {
                if (QFileInfo::exists(p))
                {
                    showable << p;
                }
                else
                {
                    // Warn user that the original image file is missing
                    reflash_state(QStringLiteral("ui: missing image file -> ") + p, WRONG_SIGNAL);
                    // Also print a visible placeholder into the transcript
                    output_scroll(p + QStringLiteral(" (missing)\n"));
                }
            }
            if (!showable.isEmpty()) showImages(showable);
        }
    }

    if (!meta.title.isEmpty())
        reflash_state("ui:" + jtr("loaded session") + " " + meta.title, SUCCESS_SIGNAL);
    else
        reflash_state("ui:" + jtr("loaded session"), SUCCESS_SIGNAL);

    int resumeSlot = -1;
    if (ui_mode == LINK_MODE)
    {
        const QString ep = (ui_state == CHAT_STATE) ? (apis.api_endpoint + apis.api_chat_endpoint)
                                                    : (apis.api_endpoint + apis.api_completion_endpoint);
        if (meta.endpoint == ep) resumeSlot = meta.slot_id;
    }
    else
    {
        const QString ep = serverManager ? serverManager->endpointBase() : QString();
        if (!ep.isEmpty() && meta.endpoint == ep) resumeSlot = meta.slot_id;
    }
    currentSlotId_ = (resumeSlot >= 0) ? resumeSlot : -1;
}

// Receive final per-turn speeds from xNet timings and print a single UI line
void Widget::recv_net_speeds(double promptPerSec, double genPerSec)
{
    const bool haveGen = genPerSec > 0.0;
    const bool havePrompt = promptPerSec > 0.0;
    if (!haveGen && !havePrompt) return; // 没有就不打印
    const QString genStr = haveGen ? (QString::number(genPerSec, 'f', 1) + " tokens/s") : QString::fromUtf8("--");
    const QString promptStr = havePrompt ? (QString::number(promptPerSec, 'f', 1) + " tokens/s") : QString::fromUtf8("--");
    reflash_state(QString::fromUtf8("ui:") + jtr("single decode") + " " + genStr + " " + jtr("batch decode") + " " + promptStr, SUCCESS_SIGNAL);
}

// ===== Record Bar helpers and slots =====
int Widget::recordCreate(RecordRole role)
{
    RecordEntry e;
    e.role = role;
    e.docFrom = outputDocEnd();
    e.docTo = e.docFrom;
    e.text.clear();
    e.msgIndex = -1;
    recordEntries_.push_back(e);
    const int idx = recordEntries_.size() - 1;
    if (ui->recordBar) ui->recordBar->addNode(chipColorForRole(role), QString());
    return idx;
}

void Widget::recordAppendText(int index, const QString &text)
{
    if (index < 0 || index >= recordEntries_.size()) return;
    recordEntries_[index].text += text;
    recordEntries_[index].docTo = outputDocEnd();
    QString tip = recordEntries_[index].text;
    if (tip.size() > 600) tip = tip.left(600) + "...";
    if (ui->recordBar) ui->recordBar->updateNode(index, tip);
}

void Widget::recordClear()
{
    recordEntries_.clear();
    currentThinkIndex_ = -1;
    currentAssistantIndex_ = -1;
    if (ui->recordBar) ui->recordBar->clearNodes();
}

void Widget::gotoRecord(int index)
{
    if (index < 0 || index >= recordEntries_.size()) return;
    if (ui->recordBar) ui->recordBar->setSelectedIndex(index);
    const auto &e = recordEntries_[index];
    QTextDocument *doc = ui->output->document();
    const int end = outputDocEnd();

    // Normalize start: skip any leading blank lines so the role header is the first visible line
    int from = qBound(0, e.docFrom, end);
    while (from < end && doc->characterAt(from) == QChar('\n')) ++from;

    // Place caret at header start and ensure it's visible
    QTextCursor c(doc);
    c.setPosition(from);
    ui->output->setTextCursor(c);
    ui->output->ensureCursorVisible();

    // Align header line to the very top of the viewport
    if (QScrollBar *vs = ui->output->verticalScrollBar())
    {
        const QRect r = ui->output->cursorRect(c);
        const int target = qBound(0, vs->maximum(), vs->value() + r.top());
        vs->setValue(target);
    }

    ui->output->setFocus();
}

void Widget::refreshSkillsUI()
{
    if (!date_ui || !date_ui->skills_list || !skillManager) return;
    date_ui->skills_list->setSkills(skillManager->skills());
}

void Widget::rebuildSkillPrompts()
{
    ui_extra_prompt = create_extra_prompt();
    if (date_ui) get_date();
}

void Widget::updateSkillVisibility(bool engineerEnabled)
{
    if (!date_ui) return;
    if (date_ui->skills_box) date_ui->skills_box->setVisible(engineerEnabled);
    if (engineerEnabled)
    {
        refreshSkillsUI();
    }
    rebuildSkillPrompts();
}

void Widget::onSkillsChanged()
{
    refreshSkillsUI();
    rebuildSkillPrompts();
    if (date_ui) auto_save_user();
}

void Widget::onSkillDropRequested(const QStringList &paths)
{
    if (!skillManager) return;
    for (const QString &path : paths)
    {
        const QString displayName = QFileInfo(path).fileName();
        reflash_state(QString::fromUtf8("ui:skill import queued -> ") + displayName, SIGNAL_SIGNAL);
        skillManager->importSkillArchiveAsync(path);
    }
}

void Widget::onSkillToggleRequested(const QString &skillId, bool enabled)
{
    if (!skillManager || skillId.isEmpty()) return;
    skillManager->setSkillEnabled(skillId, enabled);
}

void Widget::onSkillRemoveRequested(const QString &skillId)
{
    if (!skillManager || skillId.isEmpty()) return;
    QString error;
    if (!skillManager->removeSkill(skillId, &error))
    {
        const QString msg = error.isEmpty() ? QStringLiteral("failed to remove skill -> ") + skillId : error;
        reflash_state(QString::fromUtf8("ui:") + msg, WRONG_SIGNAL);
    }
    else
    {
        reflash_state(QString::fromUtf8("ui:skill removed -> ") + skillId, SIGNAL_SIGNAL);
    }
    auto_save_user();
}

void Widget::restoreSkillSelection(const QStringList &skills)
{
    if (!skillManager) return;
    QSet<QString> enabled;
    for (const QString &id : skills)
    {
        enabled.insert(id.trimmed());
    }
    skillManager->restoreEnabledSet(enabled);
    if (date_ui && date_ui->engineer_checkbox)
    {
        updateSkillVisibility(date_ui->engineer_checkbox->isChecked());
    }
    else
    {
        rebuildSkillPrompts();
    }
}

void Widget::replaceOutputRangeColored(int from, int to, const QString &text, QColor color)
{
    QTextCursor c(ui->output->document());
    const int endBound = outputDocEnd();
    from = qBound(0, from, endBound);
    to = qBound(0, to, endBound);
    if (to < from) std::swap(to, from);
    c.setPosition(from);
    c.setPosition(to, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    QTextCharFormat fmt;
    fmt.setForeground(QBrush(color));
    c.mergeCharFormat(fmt);
    c.insertText(text);
    QTextCharFormat fmt0;
    c.mergeCharFormat(fmt0);
}

void Widget::onRecordClicked(int index)
{
    gotoRecord(index);
}

void Widget::onRecordDoubleClicked(int index)
{
    if (index < 0 || index >= recordEntries_.size()) return;
    if (ui->recordBar) ui->recordBar->setSelectedIndex(index);
    auto &e = recordEntries_[index];
    QDialog dlg(this);
    dlg.setWindowTitle(jtr("edit"));
    dlg.setModal(true);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QTextEdit *ed = new QTextEdit(&dlg);
    ed->setPlainText(e.text);
    ed->setMinimumSize(QSize(480, 280));
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(ed);
    lay->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString newText = ed->toPlainText();
    if (newText == e.text) return;

    // Compute content range to replace (preserve role header line)
    QTextDocument *doc = ui->output->document();
    const int docEnd = outputDocEnd();
    int contentFrom = qBound(0, e.docFrom, docEnd);

    // Skip leading blank line inserted before header (if any)
    while (contentFrom < docEnd && doc->characterAt(contentFrom) == QChar('\n')) ++contentFrom;

    auto roleName = [](RecordRole r) -> QString
    {
        switch (r)
        {
        case RecordRole::System: return QStringLiteral("system");
        case RecordRole::User: return QStringLiteral("user");
        case RecordRole::Assistant: return QStringLiteral("assistant");
        case RecordRole::Think: return QStringLiteral("think");
        case RecordRole::Tool: return QStringLiteral("tool");
        }
        return QStringLiteral("user");
    };

    const QString header = roleName(e.role);
    // If header is present right after contentFrom, skip "header\n"
    if (contentFrom + header.size() + 1 <= docEnd)
    {
        bool headerMatch = true;
        for (int i = 0; i < header.size() && contentFrom + i < docEnd; ++i)
        {
            if (doc->characterAt(contentFrom + i) != header.at(i))
            {
                headerMatch = false;
                break;
            }
        }
        if (headerMatch && (contentFrom + header.size() < docEnd) && doc->characterAt(contentFrom + header.size()) == QChar('\n'))
        {
            contentFrom += header.size() + 1;
        }
    }

    const int oldContentTo = qBound(contentFrom, e.docTo, docEnd);
    // Replace only the content region, keep coloring by role
    replaceOutputRangeColored(contentFrom, oldContentTo, newText, textColorForRole(e.role));

    // Update current entry and cascade position delta to following entries
    const int newEnd = contentFrom + newText.size();
    const int delta = newEnd - oldContentTo;
    e.text = newText;
    e.docTo = newEnd;
    for (int i = index + 1; i < recordEntries_.size(); ++i)
    {
        recordEntries_[i].docFrom += delta;
        recordEntries_[i].docTo += delta;
    }

    QString tip = newText;
    if (tip.size() > 600) tip = tip.left(600) + "...";
    if (ui->recordBar) ui->recordBar->updateNode(index, tip);

    // Ensure role header line exists after editing (guard against accidental deletion)
    {
        QTextDocument *doc = ui->output->document();
        const int docEnd2 = outputDocEnd();
        int s = qBound(0, recordEntries_[index].docFrom, docEnd2);
        // Skip any leading blank lines before header
        while (s < docEnd2 && doc->characterAt(s) == QChar('\n')) ++s;

        const auto roleName = [](RecordRole r) -> QString
        {
            switch (r)
            {
            case RecordRole::System: return QStringLiteral("system");
            case RecordRole::User: return QStringLiteral("user");
            case RecordRole::Assistant: return QStringLiteral("assistant");
            case RecordRole::Think: return QStringLiteral("think");
            case RecordRole::Tool: return QStringLiteral("tool");
            }
            return QStringLiteral("user");
        };
        const QString header = roleName(e.role);

        bool headerOk = false;
        if (s + header.size() + 1 <= docEnd2)
        {
            headerOk = true;
            for (int i = 0; i < header.size(); ++i)
            {
                if (doc->characterAt(s + i) != header.at(i))
                {
                    headerOk = false;
                    break;
                }
            }
            if (!(headerOk && doc->characterAt(s + header.size()) == QChar('\n')))
            {
                headerOk = false;
            }
        }

        if (!headerOk)
        {
            // Insert header + newline at position s with role color
            QTextCursor ic(doc);
            ic.setPosition(s);
            QTextCharFormat headerFmt;
            headerFmt.setForeground(QBrush(chipColorForRole(e.role)));
            ic.mergeCharFormat(headerFmt);
            ic.insertText(header + QString(DEFAULT_SPLITER));

            // Adjust current and subsequent record ranges by the inserted length
            const int ins = header.size() + 1;
            e.docTo += ins;
            for (int i = index + 1; i < recordEntries_.size(); ++i)
            {
                recordEntries_[i].docFrom += ins;
                recordEntries_[i].docTo += ins;
            }
        }
    }

    // Update in-memory message content and persist to history
    if (e.msgIndex >= 0 && e.msgIndex < ui_messagesArray.size())
    {
        QJsonObject m = ui_messagesArray[e.msgIndex].toObject();
        m.insert("content", newText);
        ui_messagesArray[e.msgIndex] = m;
        if (history_ && !history_->sessionId().isEmpty()) { history_->rewriteAllMessages(ui_messagesArray); }
    }
}

int Widget::outputDocEnd() const
{
    QTextCursor c(ui->output->document());
    c.movePosition(QTextCursor::End);
    return c.position();
}
