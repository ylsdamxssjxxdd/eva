//主函数和主要槽函数

#include "widget.h"

#include "ui_widget.h"
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>

Widget::Widget(QWidget *parent, QString applicationDirPath_)
    : QWidget(parent), ui(new Ui::Widget)
{
    //---------------初始化ui--------------
    ui->setupUi(this);
    initTextComponentsMemoryPolicy();
    applicationDirPath = applicationDirPath_;
    // Default engineer workdir under application directory
    engineerWorkDir = QDir::cleanPath(QDir(applicationDirPath).filePath("EVA_WORK"));
    ui->splitter->setStretchFactor(0, 3); //设置分隔器中第一个元素初始高度占比为3
    ui->splitter->setStretchFactor(1, 1); //设置分隔器中第二个元素初始高度占比为1

    connect(ui->splitter, &QSplitter::splitterMoved, this, &Widget::onSplitterMoved);
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
        language_flag = 0; //中文
    }
    else
    {
        language_flag = 1; //英文
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
    ui_DATES.user_name = DEFAULT_USER_NAME;
    ui_DATES.model_name = DEFAULT_MODEL_NAME;
    ui_DATES.is_load_tool = false;
    date_map.insert("default", ui_DATES);
    EVA_DATES troll;
    troll.date_prompt = jtr("you are a troll please respect any question for user");
    troll.user_name = jtr("user");
    troll.model_name = jtr("troll");
    troll.is_load_tool = false;
    date_map.insert(jtr("troll"), troll);
    EVA_DATES ghost;
    ghost.date_prompt = jtr("Mediocre ghost prompt");
    ghost.user_name = jtr("user");
    ghost.model_name = jtr("Mediocre ghost");
    ghost.is_load_tool = false;
    date_map.insert(jtr("Mediocre ghost"), ghost);

    //-------------默认展示内容-------------
    right_menu = nullptr;     //初始设置输入区右击菜单为空
    ui_font.setPointSize(10); // 将设置控件的字体大小设置为10
    EVA_icon = QIcon(":/logo/dark_logo.png");
    QApplication::setWindowIcon(EVA_icon);                                         //设置应用程序图标
    ui->set->setIcon(QIcon(":/logo/assimp_tools_icon.ico"));                       //设置设置图标
    ui->reset->setIcon(QIcon(":/logo/sync.ico"));                                  //设置重置图标
    reflash_state("ui:" + jtr("click load and choose a gguf file"), USUAL_SIGNAL); //初始提示

    init_movie(); //初始化动画参数

    //-------------初始化各种控件-------------
    setApiDialog();                                               //设置api选项
    set_DateDialog();                                             //设置约定选项
    set_SetDialog();                                              //设置设置选项
    lastServerRestart_ = false;
    ui_state_init();                                              //初始界面状态
    ui->input->textEdit->setContextMenuPolicy(Qt::NoContextMenu); //取消右键菜单
    ui->input->installEventFilter(this);                          //安装事件过滤器
    ui->input->textEdit->installEventFilter(this);                //安装事件过滤器
    ui->load->installEventFilter(this);                           //安装事件过滤器
    api_endpoint_LineEdit->installEventFilter(this);              //安装事件过滤器
    ui->state->setContextMenuPolicy(Qt::NoContextMenu);           //取消右键
    ui->state->installEventFilter(this);                          //安装事件过滤器
    ui->state->setLineWrapMode(QPlainTextEdit::NoWrap);           // 禁用自动换行
    ui->state->setFocus();                                        //设为当前焦点
    trayMenu = new QMenu(this);                                   // 托盘菜单

    //-------------获取cpu内存信息-------------
    max_thread = std::thread::hardware_concurrency();
    settings_ui->nthread_slider->setRange(1, max_thread); //设置线程数滑块的范围
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
    audio_timer = new QTimer(this);                                           //录音定时器
    connect(audio_timer, &QTimer::timeout, this, &Widget::monitorAudioLevel); // 每隔100毫秒刷新一次输入区                                                      // win7就不用检查声音输入了
    music_player.setMedia(QUrl("qrc:/fly_me_to_the_moon.mp3")); //设置播放的音乐
    if (checkAudio())                                           // 如果支持音频输入则注册f2快捷键
    {
        // QShortcut *shortcutF2 = new QShortcut(QKeySequence(Qt::Key_F2), this);
        // connect(shortcutF2, &QShortcut::activated, this, &Widget::onShortcutActivated_F2);
        shortcutF2 = new QHotkey(QKeySequence("F2"), true, this);
        connect(shortcutF2, &QHotkey::activated, this, &Widget::onShortcutActivated_F2);
    }
    //----------------本地后端管理（llama-server）------------------
    serverManager = new LocalServerManager(this, applicationDirPath);
    // 转发 server 输出到模型日志（增殖窗口）而不是主输出区
    connect(serverManager, &LocalServerManager::serverOutput, this, [this](const QString &s) { emit ui2expend_llamalog(s); });
    connect(serverManager, &LocalServerManager::serverOutput, this, &Widget::onServerOutput);
    connect(serverManager, &LocalServerManager::serverState, this, &Widget::reflash_state);
    // 后端启动失败 -> 立即停止装载动画并解锁界面
    connect(serverManager, &LocalServerManager::serverStartFailed, this, &Widget::onServerStartFailed);
    connect(serverManager, &LocalServerManager::serverReady, this, &Widget::onServerReady);
    connect(serverManager, &LocalServerManager::serverStopped, this, [this]() {
        // 计划内重启时旧进程的退出：完全忽略，不重置 UI、不停止转轮动画
        if (ignoreNextServerStopped_ || lastServerRestart_) { ignoreNextServerStopped_ = false; return; }
        // 其它情况：后端确实已停止 -> 重置 UI，并停止任何进行中的动画

        ui->state->clear();
        reflash_state("ui: local server stopped", SIGNAL_SIGNAL);
        if (load_begin_pTimer) load_begin_pTimer->stop();
        if (load_pTimer) load_pTimer->stop();
        if (load_over_pTimer) load_over_pTimer->stop();
        if (decode_pTimer) decode_pTimer->stop();
        if (force_unlockload_pTimer) force_unlockload_pTimer->stop();
        lastServerRestart_ = false;
        is_load = false;
        load_action = 0;
        EVA_title = jtr("current model") + " ";
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        unlockButtonsAfterError();
    });

    //应用语言语种，注意不能影响行动纲领（主要流程）
    apply_language(language_flag);

    //----------------设置系统托盘-----------------------
    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    trayIcon->setToolTip(EVA_title);
    trayIcon->setContextMenu(trayMenu);
    // 托盘图标点击事件
    QObject::connect(trayIcon, &QSystemTrayIcon::activated, this, [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) //单击
        {
            toggleWindowVisibility(this, true); // 显示窗体
        }
    });

    //监视相关
    connect(&monitor_timer, SIGNAL(timeout()), this, SLOT(monitorTime())); //新开一个线程

    EVA_title = jtr("eva");
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    trayIcon->show();
    // Initialize persistent history store under EVA_TEMP/history
    history_ = new HistoryStore(QDir(applicationDirPath).filePath("EVA_TEMP/history"));

    // 进程退出前，确保停止本地 llama-server，避免残留进程
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        if (serverManager) serverManager->stop();
    });
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
            & ~Qt::WindowCloseButtonHint                                       // 移除关闭按钮
        );
    dlg->setWindowTitle("quit");
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    qApp->processEvents();                  // 刷新一次以显示对话框

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
        connect(serverManager, &LocalServerManager::serverStopped, qApp, []() {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        });
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

//用户点击装载按钮处理
void Widget::on_load_clicked()
{
    reflash_state("ui:" + jtr("clicked load"), SIGNAL_SIGNAL);

    // 弹出模式选择对话框：本地模式 或 链接模式
    QMessageBox box(this);
    box.setWindowTitle(jtr("load"));
    box.setText(jtr("load") + ": " + jtr("local mode") + " / " + jtr("link mode"));
    QPushButton *localBtn = box.addButton(jtr("local mode"), QMessageBox::AcceptRole);
    QPushButton *linkBtn = box.addButton(jtr("link mode"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == localBtn)
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
    else if (box.clickedButton() == linkBtn)
    {
        // 用户选择链接模式：打开链接设置对话框
        ui_state_info = "ui:" + jtr("clicked") + jtr("link") + jtr("set");
        reflash_state(ui_state_info, SIGNAL_SIGNAL);
        // 预填当前值
        api_endpoint_LineEdit->setText(apis.api_endpoint);
        api_key_LineEdit->setText(apis.api_key);
        api_model_LineEdit->setText(apis.api_model);
        api_dialog->exec(); // 确定后触发 set_api()
    }
    else
    {
        // 取消 -> 不做任何事
        return;
    }
}

//模型释放完毕并重新装载
void Widget::recv_freeover_loadlater()
{
    gpu_wait_load = true;
    emit gpu_reflash(); //强制刷新gpu信息
}

// 装载前动作
void Widget::preLoad()
{
    is_load = false; //重置is_load标签
    is_load_play_over = false;
    if (ui_state == CHAT_STATE)
    {
        ui->output->clear(); //清空输出区
    }
    ui->state->clear(); //清空状态区
    ui_state_loading(); //装载中界面状态
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

//用户点击发出按钮处理
void Widget::on_send_clicked()
{
    reflash_state("ui:" + jtr("clicked send"), SIGNAL_SIGNAL);

    EVA_INPUTS inputs;           // 待构造的输入消息
    QString text_content;        // 文本内容
    QStringList images_filepath; // 图像内容
    QStringList wavs_filepath;   // 音频内容

    // 统一通过 xNet 发送（本地和远端都是请求式）
    api_send_clicked_slove();
    return;

    //如果是对话模式,主要流程就是构建text_content,发送text_content,然后触发推理
    if (ui_state == CHAT_STATE)
    {
        if (tool_result == "")
        {
            text_content = ui->input->textEdit->toPlainText().toUtf8().data();
            ui->input->textEdit->clear(); // 获取用户输入
            images_filepath = ui->input->imageFilePaths();
            wavs_filepath = ui->input->wavFilePaths();
            // qDebug()<<"wavs_filepath"<<wavs_filepath;
            // qDebug()<<images_filepath;
            ui->input->clearThumbnails();
        }
        //如果挂载了鼠标键盘工具，则每次发送时附带一张屏幕截图
        if (ui_controller_ischecked)
        {
            QString imgfilePath = saveScreen();
            images_filepath.append(imgfilePath);
        }
        //如果工具返回的结果不为空，则认为输入源是观察者
        if (tool_result != "")
        {

            text_content = tool_result;
            tool_result = "";
            inputs = {EVA_ROLE_OBSERVATION, text_content, images_filepath};
        }
        else
        {
            inputs = {EVA_ROLE_USER, text_content, images_filepath, wavs_filepath};
        }
    }
    else if (ui_state == COMPLETE_STATE)
    {
        text_content = ui->output->toPlainText().toUtf8().data(); //直接用output上的文本进行推理
        inputs = {EVA_ROLE_USER, text_content};                   //传递用户输入
    }
    // qDebug()<<text_content;
    is_run = true;      //模型正在运行标签
    ui_state_pushing(); //推理中界面状态
    // xbot 已弃用
}

//模型输出完毕的后处理
void Widget::recv_pushover()
{
    // Finalize speed display for LINK mode (remote server): single-line output
    if (ui_mode == LINK_MODE)
    {
        turnActive_ = false;
        double genTps = -1.0;
        const double secs = turnTimer_.isValid() ? (turnTimer_.nsecsElapsed() / 1e9) : 0.0;
        if (secs > 0.0 && kvStreamedTurn_ > 0)
        {
            genTps = double(kvStreamedTurn_) / secs;
        }
        const QString genStr = (genTps > 0.0) ? (QString::number(genTps, 'f', 2) + " tokens/s") : QString::fromUtf8("--");
        const QString promptStr = QString::fromUtf8("--"); // 链接模式不显示上文处理速度
        reflash_state(QString::fromUtf8("ui: 文字生成 ") + genStr + QString::fromUtf8("  上文处理 ") + promptStr, USUAL_SIGNAL);
    }

    // Separate reasoning (<think>...</think>) from final content; don't add reasoning to messagesArray
    QString reasoningText;
    QString finalText = temp_assistant_history;
    const QString tBegin = QString(DEFAULT_THINK_BEGIN);
    const QString tEnd = QString(DEFAULT_THINK_END);
    int endIdx = finalText.indexOf(tEnd);
    if (endIdx != -1)
    {
        int startIdx = finalText.lastIndexOf(tBegin, endIdx);
        if (startIdx != -1)
        {
            const int rStart = startIdx + tBegin.size();
            reasoningText = finalText.mid(rStart, endIdx - rStart);
            // remove the whole <think>...</think> segment from finalText
            finalText.remove(startIdx, (endIdx + tEnd.size()) - startIdx);
        }
    }
    QJsonObject roleMessage;
    roleMessage.insert("role", DEFAULT_MODEL_NAME);
    roleMessage.insert("content", finalText);
    ui_messagesArray.append(roleMessage);
    // history: store reasoning separately for future display/search
    if (history_ && ui_state == CHAT_STATE)
    {
        QJsonObject hist = roleMessage;
        if (!reasoningText.isEmpty()) hist.insert("reasoning", reasoningText);
        history_->appendMessage(hist);
    }
    temp_assistant_history = "";

    if (ui_state == COMPLETE_STATE) //补完模式的话额外重置一下
    {
        normal_finish_pushover();
        on_reset_clicked(); //触发重置
    }
    else
    {
        //如果挂载了工具,则尝试提取里面的json
        if (is_load_tool)
        {
            // qDebug()<<ui_messagesArray.last().first;
            QString tool_str = ui_messagesArray.last().toObject().value("content").toString(); //移除think标签;

            tools_call = XMLparser(tool_str); //取巧预解码的系统指令故意不让解析出
            if (tools_call.empty())
            {
                normal_finish_pushover();
            }
            else
            {
                if (tools_call.contains("name") && tools_call.contains("arguments")) //要包含这两个字段才能调用工具
                {
                    QString tools_name = QString::fromStdString(tools_call.value("name", ""));
                    reflash_state("ui:" + jtr("clicked") + " " + tools_name, SIGNAL_SIGNAL);
                    //包含以下字段则停止调用
                    if (tools_name == "answer" || tools_name == "response")
                    {
                        normal_finish_pushover();
                    }
                    //正常调用情况
                    else
                    {
                        // accumulate current-turn tokens before launching tool (exclude reasoning)
                        if (kvTokensTurn_ > 0)
                        {
                            const int adjustedTurn = qMax(0, kvTokensTurn_ - lastReasoningTokens_);
                            kvTokensAccum_ += adjustedTurn;
                            kvTokensTurn_ = 0;
                            lastReasoningTokens_ = 0;
                        }
                        emit ui2tool_exec(tools_call); //调用tool
                        //使用工具时解码动画不停
                    }
                }
            }
        }

        //正常结束
        else
        {
            normal_finish_pushover();
        }
    }
}

//正常情况处理推理完毕
void Widget::normal_finish_pushover()
{
    is_run = false;
    ui_state_normal(); //待机界面状态
    // integrate this-turn tokens into conversation accumulation
    if (kvTokensTurn_ > 0)
    {
        const int adjustedTurn = qMax(0, kvTokensTurn_ - lastReasoningTokens_);
        kvTokensAccum_ += adjustedTurn;
        kvTokensTurn_ = 0;
        lastReasoningTokens_ = 0;
    }
    decode_finish();
    if (!wait_to_show_images_filepath.isEmpty())
    {
        showImages(wait_to_show_images_filepath);
        wait_to_show_images_filepath.clear();
    }
}

//处理tool推理完毕的槽
void Widget::recv_toolpushover(QString tool_result_)
{
    if (tool_result_.contains("<ylsdamxssjxxdd:showdraw>")) //有图像要显示的情况
    {
        wait_to_show_images_filepath.append(tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1]); //文生图后待显示图像的图像路径
        tool_result = "stablediffusion " + jtr("call successful, image save at") + " " + tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1];
    }
    else
    {
        tool_result = tool_result_;
        tool_result = truncateString(tool_result, MAX_INPUT); //超出最大输入的部分截断
    }

    on_send_clicked(); //触发发送继续预测下一个词
}

//停止完毕的后处理
void Widget::recv_stopover()
{
    if (ui_state == COMPLETE_STATE)
    {
        ui->reset->click();
    } //补完模式终止后需要重置
}

//模型达到最大上下文的后处理
void Widget::recv_arrivemaxctx(bool predecode)
{
    EVA_icon = QIcon(":/logo/red_logo.png");
    QApplication::setWindowIcon(EVA_icon); // 设置应用程序图标
    trayIcon->setIcon(EVA_icon);           // 设置系统托盘图标
    // if(predecode){history_prompt = "";}//取巧使下一次重置触发预解码
}

//重置完毕的后处理
void Widget::recv_resetover()
{
    if (ui_SETTINGS.ngl == 0)
    {
        EVA_icon = QIcon(":/logo/blue_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }                                //恢复
    else
    {
        EVA_icon = QIcon(":/logo/green_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }                                //恢复
    reflash_state("ui:" + jtr("reset ok"), SUCCESS_SIGNAL);

    updateMonitorTimer();
}

//设置参数改变,重载模型
void Widget::recv_reload()
{
    preLoad(); //装载前动作
}

// bot发信号请求ui触发reset,针对约定
void Widget::recv_datereset()
{
    //打印约定的系统指令
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
    auto_save_user(); //保存ui配置

    ui->reset->click();
}

// bot发信号请求ui触发reset,针对设置
void Widget::recv_setreset()
{
    //打印设置内容
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

    //展示额外停止标志
    // if (ui_state == CHAT_STATE) {
    //     QString stop_str;
    //     stop_str = jtr("extra stop words") + " ";
    //     for (int i = 0; i < ui_DATES.extra_stop_words.size(); ++i) {
    //         stop_str += ui_DATES.extra_stop_words.at(i) + " ";
    //     }
    //     reflash_state("· " + stop_str + " ", USUAL_SIGNAL);
    // }

    reflash_state("···········" + jtr("set") + "···········", USUAL_SIGNAL);
    auto_save_user(); //保存ui配置

    ui->reset->click();
}

//用户点击重置按钮的处理,重置模型以及对话,并设置约定的参数
void Widget::on_reset_clicked()
{
    wait_to_show_images_filepath.clear(); //清空待显示图像
    emit ui2expend_resettts();            //清空待读列表
    tool_result = "";                     //清空工具结果
    //如果模型正在推理就改变模型的停止标签
    if (is_run)
    {
        reflash_state("ui:" + jtr("clicked") + jtr("shut down"), SIGNAL_SIGNAL);
        emit ui2net_stop(1);
        // 传递推理停止信号,模型停止后会再次触发on_reset_clicked()
        return;
    }

    reflash_state("ui:" + jtr("clicked reset"), SIGNAL_SIGNAL);
    kvTokensAccum_ = 0;
    kvTokensTurn_ = 0;   // reset conversation kv tokens
    // reset KV/speed tracking and progress bar
    kvUsed_ = 0;
    kvUsedBeforeTurn_ = 0;
    kvStreamedTurn_ = 0;
    lastPromptTps_ = -1.0;
    lastGenTps_ = -1.0;
    sawPromptTps_ = false;
    sawGenTps_ = false;
    turnActive_ = false;
    updateKvBarUi();
    currentSlotId_ = -1; // new conversation -> no slot yet
    // Reset output safely. Replacing the QTextDocument drops any cached
    // resources/undo stack without risking double-deletes.
    // Note: QTextEdit takes ownership of the previous document and will
    // delete it; do not manually delete the old one here.
    if (ui_state == CHAT_STATE) resetOutputDocument();
    ui_state_normal(); //待机界面状态

    // 请求式统一处理（本地/远端）
    ui_messagesArray = QJsonArray(); //清空
    //构造系统指令
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    ui_messagesArray.append(systemMessage);
    if (ui_state == CHAT_STATE)
    {
        reflash_output(ui_DATES.date_prompt, 0, SYSTEM_BLUE);
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

//用户点击约定按钮处理
void Widget::on_date_clicked()
{
    reflash_state("ui:" + jtr("clicked date"), SIGNAL_SIGNAL);

    //展示最近一次设置值
    date_ui->chattemplate_comboBox->setCurrentText(ui_template); //默认使用default的提示词模板
    date_ui->date_prompt_TextEdit->setPlainText(ui_date_prompt);
    date_ui->user_name_LineEdit->setText(ui_DATES.user_name);
    date_ui->model_name_LineEdit->setText(ui_DATES.model_name);

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
    }

    date_ui->switch_lan_button->setText(ui_extra_lan);

    date_dialog->exec();
}

//应用用户设置的约定内容
void Widget::set_date()
{
    // 如果用户在“约定”对话框中修改了工程师工作目录，点击“确定”后立即生效
    // 仅当已勾选“软件工程师”工具时才向 xTool 下发（未勾选时仅保存值供下次使用）
    if (date_ui && date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked()
        && date_ui->date_engineer_workdir_LineEdit)
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
    get_date(); //获取约定中的纸面值

    // 约定变化后统一重置对话上下文（本地/远端一致）并持久化
    auto_save_user(); // persist date settings

    on_reset_clicked();

    date_dialog->close();
}

//用户取消约定
void Widget::cancel_date()
{
    //还原工具选择
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
    }
    date_ui->switch_lan_button->setText(ui_extra_lan);
    //复原语言
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

//用户点击设置按钮响应
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
    //展示最近一次设置值
    settings_ui->temp_slider->setValue(ui_SETTINGS.temp * 100);
    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
    settings_ui->nctx_slider->setValue(ui_SETTINGS.nctx);
    settings_ui->repeat_slider->setValue(ui_SETTINGS.repeat * 100.00);
    settings_ui->lora_LineEdit->setText(ui_SETTINGS.lorapath);
    settings_ui->mmproj_LineEdit->setText(ui_SETTINGS.mmprojpath);
    settings_ui->nthread_slider->setValue(ui_SETTINGS.nthread);
    settings_ui->port_lineEdit->setText(ui_port);
    // 打开设置时记录当前设置快照，用于确认时判断是否有修改
    settings_snapshot_ = ui_SETTINGS;
    port_snapshot_ = ui_port;
    device_snapshot_ = settings_ui->device_comboBox->currentText().trimmed().toLower();
    settings_dialog->exec();
}

// 从SQLite加载并还原历史会话
void Widget::restoreSessionById(const QString &sessionId)
{
    if (!history_) return;
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

    for (const auto &v : msgs)
    {
        const QJsonObject m = v.toObject();
        const QString role = m.value("role").toString();
        const QString content = m.value("content").toString();
        if (role == QString(DEFAULT_SYSTEM_NAME))
        {
            reflash_output(content, 0, SYSTEM_BLUE);
            QJsonObject o; o.insert("role", DEFAULT_SYSTEM_NAME); o.insert("content", content);
            ui_messagesArray.append(o);
        }
        else if (role == QStringLiteral("user"))
        {
            reflash_output(QString(DEFAULT_SPLITER) + ui_DATES.user_name + DEFAULT_SPLITER, 0, SYSTEM_BLUE);
            reflash_output(content, 0, NORMAL_BLACK);
            reflash_output(QString(DEFAULT_SPLITER) + ui_DATES.model_name + DEFAULT_SPLITER, 0, SYSTEM_BLUE);
            QJsonObject o; o.insert("role", "user"); o.insert("content", content);
            ui_messagesArray.append(o);
        }
        else if (role == QString(DEFAULT_MODEL_NAME))
        {
            const QString reasoning = m.value("reasoning").toString();
            if (!reasoning.isEmpty())
            {
                reflash_output(QString(DEFAULT_THINK_BEGIN) + reasoning + QString(DEFAULT_THINK_END), 0, THINK_GRAY);
            }
            reflash_output(content, 0, NORMAL_BLACK);
            QJsonObject o; o.insert("role", DEFAULT_MODEL_NAME); o.insert("content", content);
            ui_messagesArray.append(o);
        }
        else
        {
            reflash_output(content, 0, NORMAL_BLACK);
            QJsonObject o; o.insert("role", role); o.insert("content", content);
            ui_messagesArray.append(o);
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


