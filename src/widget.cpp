//主函数和主要槽函数

#include "widget.h"

#include "ui_widget.h"



Widget::Widget(QWidget *parent, QString applicationDirPath_) : QWidget(parent), ui(new Ui::Widget) {
    //---------------初始化ui--------------
    ui->setupUi(this);
    applicationDirPath = applicationDirPath_;
    ui->splitter->setStretchFactor(0, 3);  //设置分隔器中第一个元素初始高度占比为3
    ui->splitter->setStretchFactor(1, 1);  //设置分隔器中第二个元素初始高度占比为1
    
    connect(ui->splitter, &QSplitter::splitterMoved, this, &Widget::onSplitterMoved);
    // QFont font(DEFAULT_FONT);
    // ui->state->setFont(font);                                                                     // 设置state区的字体
    // 注册 发送 快捷键
    shortcutCtrlEnter = new QHotkey(QKeySequence("ctrl+enter"), true, this);
    connect(shortcutCtrlEnter,  &QHotkey::activated, this, &Widget::onShortcutActivated_CTRL_ENTER);
#ifdef BODY_USE_32BIT
    ui->vcore_bar->setVisible(0);
    ui->vram_bar->setVisible(0);
#endif
    //--------------初始化语言--------------
    QLocale locale = QLocale::system();              // 获取系统locale
    QLocale::Language language = locale.language();  // 获取语言
    if (locale.languageToString(language) == "Chinese") {
        language_flag = 0;  //中文
    } else {
        language_flag = 1;  //英文
    }

    getWords(":/src/utils/ui_language.json");
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
    right_menu = nullptr;                                                           //初始设置输入区右击菜单为空
    ui_font.setPointSize(10);                                                       // 将设置控件的字体大小设置为10
    EVA_icon = QIcon(":/logo/dark_logo.png");
    QApplication::setWindowIcon(EVA_icon);                     //设置应用程序图标
    ui->set->setIcon(QIcon(":/logo/assimp_tools_icon.ico"));                        //设置设置图标
    ui->reset->setIcon(QIcon(":/logo/sync.ico"));                                   //设置重置图标
    reflash_state("ui:" + jtr("click load and choose a gguf file"), USUAL_SIGNAL);  //初始提示

    init_movie();  //初始化动画参数

    //-------------初始化各种控件-------------
    setApiDialog();                                      //设置api选项
    set_DateDialog();                                    //设置约定选项
    set_SetDialog();                                     //设置设置选项
    ui_state_init();                                     //初始界面状态
    ui->input->textEdit->setContextMenuPolicy(Qt::NoContextMenu);  //取消右键菜单
    ui->input->installEventFilter(this);                 //安装事件过滤器
    ui->input->textEdit->installEventFilter(this);                 //安装事件过滤器
    ui->load->installEventFilter(this);                  //安装事件过滤器
    api_endpoint_LineEdit->installEventFilter(this);     //安装事件过滤器
    ui->state->setContextMenuPolicy(Qt::NoContextMenu);  //取消右键
    ui->state->installEventFilter(this);                 //安装事件过滤器
    ui->state->setLineWrapMode(QPlainTextEdit::NoWrap);  // 禁用自动换行
    ui->state->setFocus();                               //设为当前焦点
    trayMenu = new QMenu(this);// 托盘菜单

    //-------------获取cpu内存信息-------------
    max_thread = std::thread::hardware_concurrency();
    settings_ui->nthread_slider->setRange(1, max_thread);  //设置线程数滑块的范围
    QTimer *cpucheck_timer = new QTimer(this);
    connect(cpucheck_timer, &QTimer::timeout, this, &Widget::updateCpuStatus);
    cpucheck_timer->start(500);  // 多少ms更新一次
    //-------------获取gpu内存信息-------------
    QTimer *gpucheck_timer = new QTimer(this);
    connect(gpucheck_timer, &QTimer::timeout, this, &Widget::updateGpuStatus);
    gpucheck_timer->start(500);  // 多少ms更新一次

    //-------------输出/状态区滚动条控制-------------
    output_scrollBar = ui->output->verticalScrollBar();
    connect(output_scrollBar, &QScrollBar::valueChanged, this, &Widget::output_scrollBarValueChanged);

    //-------------截图相关-------------
    cutscreen_dialog = new CutScreenDialog(this);
    QObject::connect(cutscreen_dialog, &CutScreenDialog::cut2ui_qimagepath, this, &Widget::recv_qimagepath);  // 传递截取的图像路径
    // QShortcut *shortcutF1 = new QShortcut(QKeySequence(Qt::Key_F1), this);
    // connect(shortcutF1, &QShortcut::activated, this, &Widget::onShortcutActivated_F1);
    shortcutF1 = new QHotkey(QKeySequence("F1"), true, this);
    connect(shortcutF1,  &QHotkey::activated, this, &Widget::onShortcutActivated_F1);

    //-------------音频相关-------------
    audio_timer = new QTimer(this);                                            //录音定时器
    connect(audio_timer, &QTimer::timeout, this, &Widget::monitorAudioLevel);  // 每隔100毫秒刷新一次输入区
#ifndef BODY_USE_32BIT // win7就不用检查声音输入了
    music_player.setMedia(QUrl("qrc:/fly_me_to_the_moon.mp3"));  //设置播放的音乐
    if (checkAudio())                                                          // 如果支持音频输入则注册f2快捷键
    {
        // QShortcut *shortcutF2 = new QShortcut(QKeySequence(Qt::Key_F2), this);
        // connect(shortcutF2, &QShortcut::activated, this, &Widget::onShortcutActivated_F2);
        shortcutF2 = new QHotkey(QKeySequence("F2"), true, this);
        connect(shortcutF2,  &QHotkey::activated, this, &Widget::onShortcutActivated_F2);
    }
#endif
    //----------------第三方进程相关------------------
    server_process = new QProcess(this);                                                                                              // 创建一个QProcess实例用来启动llama-server
    connect(server_process, &QProcess::started, this, &Widget::server_onProcessStarted);                                              //连接开始信号
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Widget::server_onProcessFinished);  //连接结束信号
    
    //应用语言语种，注意不能影响行动纲领（主要流程）
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
            toggleWindowVisibility(this,true);// 显示窗体
        }
    });

    //监视相关
    connect(&monitor_timer, SIGNAL(timeout()), this, SLOT(monitorTime()));          //新开一个线程

    EVA_title = jtr("eva");
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    trayIcon->show();
    qDebug() << "widget init over";
}

Widget::~Widget() {
    server_process->kill();  //有点问题
    delete ui;
    delete cutscreen_dialog;
    delete date_ui;
    delete settings_ui;

}
// 窗口状态变化处理
void Widget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            setWindowFlags(Qt::Tool); // 隐藏任务栏条目
            trayIcon->showMessage(jtr("eva hide"),"",EVA_icon,1000);
        }
    }
}

//关闭事件
void Widget::closeEvent(QCloseEvent *event) {
    QApplication::quit();// 关闭主窗口就退出程序
}

//用户点击装载按钮处理
void Widget::on_load_clicked() {
    reflash_state("ui:" + jtr("clicked load"), SIGNAL_SIGNAL);

    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("load_button_tooltip"), "(*.bin *.gguf)");

    if (currentpath == "" || currentpath == historypath) {
        return;
    }  //如果路径没选好或者模型路径是一样的，则不操作

    ui_mode = LOCAL_MODE;       //只要点击装载有东西就不再是链接模式
    historypath = currentpath;  // 记录这个路径，方便下次对比
    ui_SETTINGS.modelpath = currentpath;
    ui_SETTINGS.mmprojpath = "";  // 清空mmproj模型路径
    ui_SETTINGS.lorapath = "";    // 清空lora模型路径
    is_load = false;
    //先释放旧的模型和上下文
    emit ui2bot_free(1);  // 1表示重载
    monitor_timer.stop();
}

//模型释放完毕并重新装载
void Widget::recv_freeover_loadlater() {
    gpu_wait_load = true;
    emit gpu_reflash();  //强制刷新gpu信息
}

// 装载前动作
void Widget::preLoad() {
    is_load = false;  //重置is_load标签
    is_load_play_over = false;
    if (ui_state == CHAT_STATE) {
        ui->output->clear();  //清空输出区
    }
    ui->state->clear();  //清空状态区
    ui_state_loading();  //装载中界面状态
    if (is_config) {
        QString relativePath = applicationDirPath + "/EVA_TEMP/eva_config.ini";
        QFileInfo fileInfo(relativePath);
        QString absolutePath = fileInfo.absoluteFilePath();
        is_config = false;
        reflash_state("ui:" + jtr("apply_config_mess") + " " + absolutePath, USUAL_SIGNAL);
    }
    reflash_state("ui:" + jtr("model location") + " " + ui_SETTINGS.modelpath, USUAL_SIGNAL);
    emit ui2bot_loadmodel(ui_SETTINGS.modelpath);  //开始装载模型
}

//完成加载模型
void Widget::recv_loadover(bool ok_, float load_time_) {
    if (ok_) {
        load_time = load_time_;
        is_load = true;          //标记模型已装载
        all_fps++;               //补上最后一帧,表示上下文也创建了
        load_pTimer->stop();     //停止动画,但是动作计数load_action保留
        load_pTimer->start(10);  //快速播放完剩下的动画,播放完再做一些后续动作
        
    } else {
        ui->state->clear();
        load_begin_pTimer->stop();  //停止动画
        load_pTimer->stop();        //停止动画
        is_load = false;            //标记模型未装载
        load_action = 0;
        EVA_title = jtr("current model") + " ";
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        ui_state_init();
    }
}

//用户点击发出按钮处理
void Widget::on_send_clicked() {
    if (ui_state == SERVER_STATE) {
        return;
    }
    reflash_state("ui:" + jtr("clicked send"), SIGNAL_SIGNAL);

    EVA_INPUTS inputs;// 待构造的输入消息
    QString text_content;// 文本内容
    QStringList images_filepath;// 图像内容
    QStringList wavs_filepath;// 音频内容

    //链接模式的处理
    if (ui_mode == LINK_MODE) {
        api_send_clicked_slove();
        return;
    }

    //如果是对话模式,主要流程就是构建text_content,发送text_content,然后触发推理
    if (ui_state == CHAT_STATE) {
        if (tool_result == "") {
            text_content = ui->input->textEdit->toPlainText().toUtf8().data();
            ui->input->textEdit->clear();  // 获取用户输入
            images_filepath = ui->input->imageFilePaths();
            wavs_filepath = ui->input->wavFilePaths();
            // qDebug()<<"wavs_filepath"<<wavs_filepath;
            // qDebug()<<images_filepath;
            ui->input->clearThumbnails();
        }
        //如果挂载了鼠标键盘工具，则每次发送时附带一张屏幕截图
        if(ui_controller_ischecked)
        {
            QString imgfilePath = saveScreen();
            images_filepath.append(imgfilePath);
        }
        //如果工具返回的结果不为空，则认为输入源是观察者
        if (tool_result != "") {
            
            text_content = tool_result;
            tool_result = "";
            inputs = {EVA_ROLE_OBSERVATION,text_content,images_filepath};

        } else {
            inputs = {EVA_ROLE_USER,text_content,images_filepath,wavs_filepath};
        }
    } 
    else if (ui_state == COMPLETE_STATE) 
    {
        text_content = ui->output->toPlainText().toUtf8().data();  //直接用output上的文本进行推理
        inputs = {EVA_ROLE_USER,text_content};                        //传递用户输入
    }
    // qDebug()<<text_content;
    is_run = true;                //模型正在运行标签
    ui_state_pushing();           //推理中界面状态
    emit ui2bot_predict(inputs);  //开始推理
}

//模型输出完毕的后处理
void Widget::recv_pushover() {
    QJsonObject roleMessage;
    roleMessage.insert("role",DEFAULT_MODEL_NAME);
    roleMessage.insert("content",temp_assistant_history);
    ui_messagesArray.append(roleMessage);
    temp_assistant_history = "";

    if (ui_state == COMPLETE_STATE)  //补完模式的话额外重置一下
    {
        normal_finish_pushover();
        on_reset_clicked();  //触发重置
    } else {
        //如果挂载了工具,则尝试提取里面的json
        if (is_load_tool) 
        {
            // qDebug()<<ui_messagesArray.last().first;
            QString tool_str = ui_messagesArray.last().toObject().value("content").toString();//移除think标签;
            
            tools_call = XMLparser(tool_str);  //取巧预解码的系统指令故意不让解析出
            if (tools_call.empty()) 
            {
                normal_finish_pushover();
            } 
            else
            {
                if(tools_call.contains("name") && tools_call.contains("arguments") )//要包含这两个字段才能调用工具
                {
                    QString tools_name = QString::fromStdString(tools_call.value("name", ""));
                    reflash_state("ui:" + jtr("clicked") + " " + tools_name, SIGNAL_SIGNAL);
                    //包含以下字段则停止调用
                    if (tools_name =="answer" || tools_name=="response") 
                    {
                        normal_finish_pushover();
                    }
                    //正常调用情况
                    else {
                        emit ui2tool_exec(tools_call);  //调用tool
                        //使用工具时解码动画不停
                    }
                }
                

            }

        }

        //正常结束
        else {
            normal_finish_pushover();
        }
    }
}

//正常情况处理推理完毕
void Widget::normal_finish_pushover() {
    is_run = false;
    ui_state_normal();  //待机界面状态
    decode_pTimer->stop();
    decode_action = 0;
    if (!wait_to_show_images_filepath.isEmpty()) {
        showImages(wait_to_show_images_filepath);
        wait_to_show_images_filepath.clear();
    }
}

//处理tool推理完毕的槽
void Widget::recv_toolpushover(QString tool_result_) {
    if (tool_result_.contains("<ylsdamxssjxxdd:showdraw>"))  //有图像要显示的情况
    {
        wait_to_show_images_filepath.append(tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1]);  //文生图后待显示图像的图像路径
        tool_result = "stablediffusion " + jtr("call successful, image save at") + " " + tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1];
    } else {
        tool_result = tool_result_;
        tool_result = truncateString(tool_result, MAX_INPUT);//超出最大输入的部分截断
    }

    on_send_clicked();  //触发发送继续预测下一个词
}

//停止完毕的后处理
void Widget::recv_stopover() {
    if (ui_state == COMPLETE_STATE) {
        ui->reset->click();
    }  //补完模式终止后需要重置
}

//模型达到最大上下文的后处理
void Widget::recv_arrivemaxctx(bool predecode) {
    EVA_icon = QIcon(":/logo/red_logo.png");
    QApplication::setWindowIcon(EVA_icon);// 设置应用程序图标
    trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    // if(predecode){history_prompt = "";}//取巧使下一次重置触发预解码
}

//重置完毕的后处理
void Widget::recv_resetover() {
    if (ui_SETTINGS.ngl == 0) {
        EVA_icon = QIcon(":/logo/blue_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }  //恢复
    else {
        EVA_icon = QIcon(":/logo/green_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }  //恢复
    reflash_state("ui:" + jtr("reset ok"), SUCCESS_SIGNAL);

    if(ui_monitor_frame>0 && ui_state == CHAT_STATE)
    {
        qDebug()<<"开始监视..."<<ui_monitor_frame;
        monitor_timer.start(1000/ui_monitor_frame);
        EVA_icon = QIcon(":/logo/jimu.png"); // 千年积木
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }
    else
    {
        monitor_timer.stop();
    }

}

//设置参数改变,重载模型
void Widget::recv_reload() {
    preLoad();  //装载前动作
}

// bot发信号请求ui触发reset,针对约定
void Widget::recv_datereset() {
    //打印约定的系统指令
    ui_state_info = "···········" + jtr("date") + "···········";
    reflash_state(ui_state_info, USUAL_SIGNAL);
    if (ui_state == COMPLETE_STATE) {
        reflash_state("· " + jtr("complete mode") + jtr("on") + " ", USUAL_SIGNAL);
    } else {
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
    auto_save_user();  //保存ui配置

    ui->reset->click();
}

// bot发信号请求ui触发reset,针对设置
void Widget::recv_setreset() {
    //打印设置内容
    reflash_state("···········" + jtr("set") + "···········", USUAL_SIGNAL);

    reflash_state("· " + jtr("temperature") + " " + QString::number(ui_SETTINGS.temp), USUAL_SIGNAL);
    reflash_state("· " + jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat), USUAL_SIGNAL);
    reflash_state("· " + jtr("npredict") + " " + QString::number(ui_SETTINGS.hid_npredict), USUAL_SIGNAL);
    reflash_state("· gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl), USUAL_SIGNAL);
    reflash_state("· cpu" + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread), USUAL_SIGNAL);
    reflash_state("· " + jtr("ctx") + jtr("length") + " " + QString::number(ui_SETTINGS.nctx), USUAL_SIGNAL);
    reflash_state("· " + jtr("batch size") + " " + QString::number(ui_SETTINGS.hid_batch), USUAL_SIGNAL);

    if (ui_SETTINGS.lorapath != "") {
        reflash_state("ui:" + jtr("load lora") + " " + ui_SETTINGS.lorapath, USUAL_SIGNAL);
    }
    if (ui_SETTINGS.mmprojpath != "") {
        reflash_state("ui:" + jtr("load mmproj") + " " + ui_SETTINGS.mmprojpath, USUAL_SIGNAL);
    }
    if (ui_state == CHAT_STATE) {
        reflash_state("· " + jtr("chat mode"), USUAL_SIGNAL);
    } else if (ui_state == COMPLETE_STATE) {
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
    auto_save_user();  //保存ui配置

    ui->reset->click();
}

//用户点击重置按钮的处理,重置模型以及对话,并设置约定的参数
void Widget::on_reset_clicked() {
    wait_to_show_images_filepath.clear();    //清空待显示图像
    emit ui2expend_resettts();  //清空待读列表
    tool_result = "";//清空工具结果
    //如果模型正在推理就改变模型的停止标签
    if (is_run) 
    {
        reflash_state("ui:" + jtr("clicked") + jtr("shut down"), SIGNAL_SIGNAL);
        if (ui_mode == LINK_MODE) {
            emit ui2net_stop(1);
        } else {
            emit ui2bot_stop();
            qDebug()<<"emit ui2bot_stop()";
        }  //传递推理停止信号,模型停止后会再次触发on_reset_clicked()
        return;
    }

    reflash_state("ui:" + jtr("clicked reset"), SIGNAL_SIGNAL);

    if (ui_state == CHAT_STATE) {
        ui->output->clear();
    }
    ui_state_normal();  //待机界面状态

    //如果是链接模式就简单处理
    if (ui_mode == LINK_MODE) {
        ui_messagesArray = QJsonArray();//清空
        //构造系统指令
        QJsonObject systemMessage;
        systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
        systemMessage.insert("content", ui_DATES.date_prompt);
        ui_messagesArray.append(systemMessage);
        if (ui_state == CHAT_STATE) {
            reflash_output(ui_DATES.date_prompt, 0, SYSTEM_BLUE);
            current_api = apis.api_endpoint + apis.api_chat_endpoint;
        } else {
            current_api = apis.api_endpoint + apis.api_completion_endpoint;
        }
        EVA_icon = QIcon(":/logo/dark_logo.png");
        QApplication::setWindowIcon(EVA_icon);  //设置应用程序图标
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
        EVA_title = jtr("current api") + " " + current_api;
        reflash_state(QString("ui:") + EVA_title, USUAL_SIGNAL);
        this->setWindowTitle(EVA_title);
        trayIcon->setToolTip(EVA_title);
        return;
    }
    EVA_title = jtr("current model") + " " + ui_SETTINGS.modelpath.split("/").last();
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    
    emit ui2bot_reset();  //传递重置信号,清空kv缓存,并预解码约定指令
}

//用户点击约定按钮处理
void Widget::on_date_clicked() {
    reflash_state("ui:" + jtr("clicked date"), SIGNAL_SIGNAL);

    //展示最近一次设置值
    date_ui->chattemplate_comboBox->setCurrentText(ui_template);  //默认使用default的提示词模板
    date_ui->date_prompt_TextEdit->setPlainText(ui_date_prompt);
    date_ui->user_name_LineEdit->setText(ui_DATES.user_name);
    date_ui->model_name_LineEdit->setText(ui_DATES.model_name);

    date_ui->calculator_checkbox->setChecked(ui_calculator_ischecked);
    date_ui->knowledge_checkbox->setChecked(ui_knowledge_ischecked);
    date_ui->stablediffusion_checkbox->setChecked(ui_stablediffusion_ischecked);
    date_ui->controller_checkbox->setChecked(ui_controller_ischecked);
    date_ui->MCPtools_checkbox->setChecked(ui_MCPtools_ischecked);
    date_ui->engineer_checkbox->setChecked(ui_engineer_ischecked);

    date_ui->switch_lan_button->setText(ui_extra_lan);

    date_dialog->exec();
}

//应用用户设置的约定内容
void Widget::set_date() {
    get_date();  //获取约定中的纸面值

    if (ui_mode == LINK_MODE) {
        on_reset_clicked();
    }  //如果是链接模式就重置一下

    date_dialog->close();
    emit ui2bot_date(ui_DATES);
}

//用户取消约定
void Widget::cancel_date() {
    //还原工具选择
    date_ui->calculator_checkbox->setChecked(ui_calculator_ischecked);
    date_ui->controller_checkbox->setChecked(ui_controller_ischecked);
    date_ui->knowledge_checkbox->setChecked(ui_knowledge_ischecked);
    date_ui->stablediffusion_checkbox->setChecked(ui_stablediffusion_ischecked);
    date_ui->MCPtools_checkbox->setChecked(ui_MCPtools_ischecked);
    date_ui->engineer_checkbox->setChecked(ui_engineer_ischecked);
    date_ui->switch_lan_button->setText(ui_extra_lan);
    //复原语言
    if (ui_extra_lan == "zh") {
        language_flag = 0;
    } else if (ui_extra_lan == "en") {
        language_flag = 1;
    }
    apply_language(language_flag);
    emit ui2bot_language(language_flag);
    emit ui2tool_language(language_flag);
    emit ui2net_language(language_flag);
    emit ui2expend_language(language_flag);
    // 重新判断是否挂载了工具
    if (date_ui->calculator_checkbox->isChecked() || date_ui->engineer_checkbox->isChecked() || date_ui->MCPtools_checkbox->isChecked() || date_ui->knowledge_checkbox->isChecked() || date_ui->controller_checkbox->isChecked() || date_ui->stablediffusion_checkbox->isChecked()) {
        if (is_load_tool == false) {
            reflash_state("ui:" + jtr("enable output parser"), SIGNAL_SIGNAL);
        }
        is_load_tool = true;
    } else {
        if (is_load_tool == true) {
            reflash_state("ui:" + jtr("disable output parser"), SIGNAL_SIGNAL);
        }
        is_load_tool = false;
    }
}

//用户点击设置按钮响应
void Widget::on_set_clicked() {
    server_process->kill();
    reflash_state("ui:" + jtr("clicked") + jtr("set"), SIGNAL_SIGNAL);
    if (ui_state == CHAT_STATE) {
        settings_ui->chat_btn->setChecked(1), chat_change();
    } else if (ui_state == COMPLETE_STATE) {
        settings_ui->complete_btn->setChecked(1), complete_change();
    } else if (ui_state == SERVER_STATE) {
        settings_ui->web_btn->setChecked(1), web_change();
    }
    //展示最近一次设置值
    settings_ui->temp_slider->setValue(ui_SETTINGS.temp * 100);
    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
    settings_ui->nctx_slider->setValue(ui_SETTINGS.nctx);
    settings_ui->repeat_slider->setValue(ui_SETTINGS.repeat * 100.00);
    settings_ui->lora_LineEdit->setText(ui_SETTINGS.lorapath);
    settings_ui->mmproj_LineEdit->setText(ui_SETTINGS.mmprojpath);
    settings_ui->nthread_slider->setValue(ui_SETTINGS.nthread);
    settings_ui->port_lineEdit->setText(ui_port);
    settings_dialog->exec();
}

//用户按下F1键响应
void Widget::onShortcutActivated_F1() {
    createTempDirectory("./EVA_TEMP");
    cutscreen_dialog->showFullScreen();  //处理截图事件
}

//用户按下F2键响应
void Widget::onShortcutActivated_F2() {
    if (whisper_model_path == "")  //如果还未指定模型路径则先指定
    {
        emit ui2expend_show(WHISPER_WINDOW);  //语音增殖界面
    } else if (!is_recodering) {
        recordAudio();  //开始录音
        is_recodering = true;
    } else if (is_recodering) {
        stop_recordAudio();  //停止录音
    }
}

//用户按下CTRL+ENTER键响应
void Widget::onShortcutActivated_CTRL_ENTER() { ui->send->click(); }

//接收传来的图像
void Widget::recv_qimagepath(QString cut_imagepath_) {
    reflash_state("ui:" + jtr("cut image success"), USUAL_SIGNAL);
    ui->input->addFileThumbnail(cut_imagepath_);
}

// llama-server接管
void Widget::serverControl() {
    ui_state_servering();  //服务中界面状态
    EVA_title = jtr("current model") + " " + ui_SETTINGS.modelpath.split("/").last();
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    if (is_config) {
        QString relativePath = applicationDirPath + "/EVA_TEMP/eva_config.ini";
        QFileInfo fileInfo(relativePath);
        QString absolutePath = fileInfo.absoluteFilePath();
        is_config = false;
        reflash_state("ui:" + jtr("apply_config_mess") + " " + absolutePath, USUAL_SIGNAL);
    }

    //如果还没有选择模型路径
    if (ui_SETTINGS.modelpath == "") {
        currentpath = customOpenfile(currentpath, jtr("load_button_tooltip"), "(*.bin *.gguf)");
        ui_SETTINGS.modelpath = currentpath;
    }
    if (ui_SETTINGS.modelpath == "") {
        return;
    }

    emit ui2bot_free(0);
    is_load = false;

#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/llama-server") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#else
    QString localPath = QString("./llama-server") + SFX_NAME;
    QString program = localPath;  // 设置要运行的exe文件的路径
#endif

    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << ui_SETTINGS.modelpath;
    arguments << "--host"
              << "0.0.0.0";                                            //暴露本机ip
    arguments << "--port" << ui_port;                                  //服务端口
    arguments << "-c" << QString::number(ui_SETTINGS.nctx);            //使用最近一次应用的nctx作为服务的上下文长度
    arguments << "-ngl" << QString::number(ui_SETTINGS.ngl);           //使用最近一次应用的ngl作为服务的gpu负载
    arguments << "--threads" << QString::number(ui_SETTINGS.nthread);  //使用线程
    arguments << "-b" << QString::number(ui_SETTINGS.hid_batch);           //批大小
    arguments << "--jinja"; // 使用jinja引擎支持工具调用
    arguments << "--verbose-prompt"; // 每次对话时打印出提示词
    // arguments << "-lv" << QString::number(1); // 打印日志等级
    // arguments << "-v"; // 打印全部日志，会导致速度变慢
    arguments << "--parallel" << QString::number(ui_SETTINGS.hid_parallel);
    // arguments << "--log-disable";                                      //不要日志
    if(ui_SETTINGS.hid_flash_attn){arguments << "-fa";}// 开启flash attention加速
    if (ui_SETTINGS.lorapath != "") {
        arguments << "--no-mmap";//挂载lora不能开启mmp
        arguments << "--lora" << ui_SETTINGS.lorapath;
    }  
    else {if(!ui_SETTINGS.hid_use_mmap){arguments << "--no-mmap";}}

    // 支持视觉
    if (ui_SETTINGS.mmprojpath != "") {arguments << "--mmproj" << ui_SETTINGS.mmprojpath;}

    // 开始运行程序
    server_process->start(program, arguments);

    setWindowState(windowState() | Qt::WindowMaximized);  //设置窗口最大化
    reflash_state(jtr("eva expend"), EVA_SIGNAL);

    //连接信号和槽,获取程序的输出
    connect(server_process, &QProcess::readyReadStandardOutput, [=]() {
        ui_output = server_process->readAllStandardOutput();
        // qDebug()<<"readyReadStandardOutput"<<ui_output;
        output_scroll(ui_output);
    });
    connect(server_process, &QProcess::readyReadStandardError, [=]() {
        ui_output = server_process->readAllStandardError();
        // qDebug()<<"readyReadStandardError"<<ui_output;
        //启动成功的标志
        if (ui_output.contains(SERVER_START)) {
            ui_output += QString(DEFAULT_SPLITER) + jtr("api endpoint") + "   " + " http://" + ipAddress + ":" + ui_port;
            ui_output += QString(DEFAULT_SPLITER) + jtr("model") + jtr("name") + "   " + "default" + QString(DEFAULT_SPLITER);
            ui_state_info = "ui:server " + jtr("on") + jtr("success") + "," + jtr("browser at") + ipAddress + ":" + ui_port;
            auto_save_user();  //保存ui配置
            reflash_state(ui_state_info, SUCCESS_SIGNAL);
        }

        output_scroll(ui_output);
    });
}

// bot将模型参数传递给ui
void Widget::recv_params(MODEL_PARAMS p) {
    ui_n_ctx_train = p.n_ctx_train;
    settings_ui->nctx_slider->setMaximum(p.n_ctx_train);  // 没有拓展4倍,因为批解码时还是会失败
    ui_maxngl = p.max_ngl;                   // gpu负载层数是n_layer+1
    settings_ui->ngl_slider->setMaximum(ui_maxngl);
    if (ui_SETTINGS.ngl == 999) {
        ui_SETTINGS.ngl = ui_maxngl;
    }  //及时修正999值
}

//接收缓存量
void Widget::recv_kv(float percent, int ctx_size) {
    if (percent > 0 && percent < 1) {
        percent = 1;
    }
    ui->kv_bar->setSecondValue(percent);
    ui->kv_bar->setToolTip(jtr("kv cache") + " " + QString::number(ctx_size) + " token");
}

//播放装载动画
void Widget::recv_play() {
    load_play();  //开始播放动画
}

//更新gpu内存使用率
void Widget::recv_gpu_status(float vmem, float vramp, float vcore, float vfree_) {
    vfree = vfree_;  //剩余显存
    ui->vcore_bar->setValue(vcore);
    //取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if (is_first_getvram) {
        is_first_getvram = false;
        first_vramp = vramp;
        ui->vram_bar->setValue(first_vramp);
    }
    ui->vram_bar->setSecondValue(vramp - first_vramp);

    if (gpu_wait_load) {
        gpu_wait_load = false;
#ifdef BODY_USE_GPU
        int modelsize_MB;
        QFileInfo fileInfo(ui_SETTINGS.modelpath);    //获取模型文件大小
        QFileInfo fileInfo2(ui_SETTINGS.mmprojpath);  //获取mmproj文件大小
        modelsize_MB = fileInfo.size() / 1024 / 1024 + fileInfo2.size() / 1024 / 1024;
        // qDebug()<<vfree<<modelsize_MB * 1.2;

        if (vfree > modelsize_MB * 1.2) {
            ui_SETTINGS.ngl = 999;
        } else {
            ui_SETTINGS.ngl = 0;
        }
#endif
        //发送设置参数给bot
        emit ui2bot_set(ui_SETTINGS, 1);  //设置应用完会触发preLoad
    }
}

//传递cpu信息
void Widget::recv_cpu_status(double cpuload, double memload) {
    ui->cpu_bar->setValue(cpuload);
    //取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if (is_first_getmem) {
        first_memp = memload;
        ui->mem_bar->setValue(first_memp);
        is_first_getmem = false;
    }
    ui->mem_bar->setSecondValue(memload - first_memp);
    // ui->mem_bar->setValue(physMemUsedPercent-(model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    // ui->mem_bar->setSecondValue((model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
}

//事件过滤器,鼠标跟踪效果不好要在各种控件单独实现
bool Widget::eventFilter(QObject *obj, QEvent *event) {
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->input && event->type() == QEvent::ContextMenu && ui_state == CHAT_STATE) {
        QContextMenuEvent *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        // 显示菜单
        right_menu->exec(contextMenuEvent->globalPos());
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->lora_LineEdit && event->type() == QEvent::ContextMenu) {
        chooseLorapath();
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->mmproj_LineEdit && event->type() == QEvent::ContextMenu) {
        chooseMmprojpath();
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->load && ui->load->isEnabled() && event->type() == QEvent::ContextMenu) {
        ui_state_info = "ui:" + jtr("clicked") + jtr("link") + jtr("set");
        reflash_state(ui_state_info, SIGNAL_SIGNAL);
        //设置当前值
        api_endpoint_LineEdit->setText(apis.api_endpoint);
        api_key_LineEdit->setText(apis.api_key);
        api_model_LineEdit->setText(apis.api_model);
        api_dialog->exec();
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == api_endpoint_LineEdit && event->type() == QEvent::ContextMenu) {
        QString api_endpoint = "http://" + getFirstNonLoopbackIPv4Address() + ":8080";
        api_endpoint_LineEdit->setText(api_endpoint);
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->state && event->type() == QEvent::ContextMenu) {
        emit ui2expend_show(PREV_WINDOW);  // 1是模型信息页
        return true;
    }

    return QObject::eventFilter(obj, event);
}

//传递模型预解码的内容
void Widget::recv_predecode(QString bot_predecode_content_) { bot_predecode_content = bot_predecode_content_; }

//接收whisper解码后的结果
void Widget::recv_speechdecode_over(QString result) {
    ui_state_normal();
    ui->input->textEdit->append(result);
    // ui->send->click();//尝试一次发送
}

//接收模型路径
void Widget::recv_whisper_modelpath(QString modelpath) { whisper_model_path = modelpath; }

//链接模式的发送处理
void Widget::api_send_clicked_slove() {
    //注意链接模式不发送前后缀
    QString input;
    
    emit ui2net_stop(0);
    ENDPOINT_DATA data;
    data.date_prompt = ui_DATES.date_prompt;
    data.input_pfx = ui_DATES.user_name;
    data.input_sfx = ui_DATES.model_name;
    data.stopwords = ui_DATES.extra_stop_words;
    if (ui_state == COMPLETE_STATE) {
        data.is_complete_state = true;
    } else {
        data.is_complete_state = false;
    }
    data.temp = ui_SETTINGS.temp;
    data.n_predict = ui_SETTINGS.hid_npredict;
    data.repeat = ui_SETTINGS.repeat;
    data.messagesArray = ui_messagesArray;

    if (tool_result == "") {
        input = ui->input->textEdit->toPlainText().toUtf8().data();
        ui->input->textEdit->clear();
    }

    QStringList images_filepath = ui->input->imageFilePaths();// 获取图像列表
    QStringList wavs_filepath = ui->input->wavFilePaths();// 获取音频列表
    ui->input->clearThumbnails();// 清空缩率图区

    
    if (ui_state == CHAT_STATE) {
        //-----------------------构造工具消息----------------------------
        if (tool_result != "") {
            //目前通过user这个角色给net
            QJsonObject roleMessage;
            roleMessage.insert("role",DEFAULT_USER_NAME);
            roleMessage.insert("content","tool: " + tool_result);
            ui_messagesArray.append(roleMessage);
            reflash_output(QString(DEFAULT_SPLITER) + DEFAULT_USER_NAME + DEFAULT_SPLITER + "tool: " + tool_result + DEFAULT_SPLITER + ui_DATES.model_name + DEFAULT_SPLITER, 0, TOOL_BLUE);  //天蓝色表示工具返回结果

            tool_result = "";

            QTimer::singleShot(100, this, SLOT(tool_testhandleTimeout()));  //链接模式不能立即发送
            is_run = true;                                                  //模型正在运行标签
            ui_state_pushing();
            return;
        } 
        //-----------------------构造用户消息----------------------------
        else {
            // 暂时不加截图，因为模型如果不支持视觉会直接报错
            // if(ui_controller_ischecked)
            // {
            //     QString imgfilePath = saveScreen();
            //     images_filepath.append(imgfilePath);
            // }

            if(images_filepath.isEmpty())
            {
                QJsonObject roleMessage;
                roleMessage.insert("role",DEFAULT_USER_NAME);
                roleMessage.insert("content",input);
                ui_messagesArray.append(roleMessage);
            }
            else // 有图片的情况
            {
                QJsonObject message;
                message["role"] = DEFAULT_USER_NAME;
                QJsonArray contentArray;
                // 添加图像消息
                for(int i = 0;i<images_filepath.size();++i)
                {
                    //读取图像文件并转换为 Base64
                    QFile imageFile(images_filepath[i]);
                    if (!imageFile.open(QIODevice::ReadOnly)) {qDebug() << "Failed to open image file";}
                    QByteArray imageData = imageFile.readAll();
                    QByteArray base64Data = imageData.toBase64();
                    QString base64String = QString("data:image/jpeg;base64,") + base64Data;

                    QJsonObject imageObject;
                    imageObject["type"] = "image_url";
                    QJsonObject imageUrlObject;
                    imageUrlObject["url"] = base64String;
                    imageObject["image_url"] = imageUrlObject;
                    contentArray.append(imageObject);
                    showImages({images_filepath[i]});// 展示图片
                }
                // 添加用户消息
                QJsonObject textMessage;
                textMessage.insert("type","text");
                textMessage.insert("text",input);
                contentArray.append(textMessage);
                message["content"] = contentArray;
                ui_messagesArray.append(message);
            }

            if(!wavs_filepath.isEmpty())//有音频的情况
            {
                QJsonObject message;
                message["role"] = DEFAULT_USER_NAME;
                QJsonArray contentArray;
                // 添加音频消息
                for(int i = 0; i < images_filepath.size(); ++i)
                {
                    QString filePath = images_filepath[i];
                    QFile audioFile(filePath);
                    if (!audioFile.open(QIODevice::ReadOnly)) {
                        qDebug() << "Failed to open audio file:" << filePath;
                        continue; // 跳过失败文件
                    }
                    
                    QByteArray audioData = audioFile.readAll();
                    QByteArray base64Data = audioData.toBase64();

                    // 根据扩展名确定MIME类型
                    QFileInfo fileInfo(filePath);
                    QString extension = fileInfo.suffix().toLower();
                    QString mimeType = "audio/mpeg"; // 默认MP3
                    
                    if (extension == "wav") {
                        mimeType = "audio/wav";
                    } else if (extension == "ogg") {
                        mimeType = "audio/ogg";
                    } else if (extension == "flac") {
                        mimeType = "audio/flac";
                    }
                    // 其他格式可继续扩展

                    QString base64String = QString("data:%1;base64,").arg(mimeType) + base64Data;

                    QJsonObject audioObject;
                    audioObject["type"] = "audio_url";  // 类型改为audio_url
                    
                    QJsonObject audioUrlObject;
                    audioUrlObject["url"] = base64String;
                    
                    audioObject["audio_url"] = audioUrlObject;  // 键名改为audio_url
                    contentArray.append(audioObject);
                    showImages({":/logo/wav.png"});// 展示图片
                }
            }

            data.messagesArray = ui_messagesArray;
            reflash_output(QString(DEFAULT_SPLITER) + ui_DATES.user_name + DEFAULT_SPLITER, 0, SYSTEM_BLUE);   //前后缀用蓝色
            reflash_output(input, 0, NORMAL_BLACK);                                                            //输入用黑色
            reflash_output(QString(DEFAULT_SPLITER) + ui_DATES.model_name + DEFAULT_SPLITER, 0, SYSTEM_BLUE);  //前后缀用蓝色
            data.n_predict = ui_SETTINGS.hid_npredict;
            emit ui2net_data(data);
        }
    } 
    else if (ui_state == COMPLETE_STATE)  //直接用output上的文本进行推理
    {
        data.input_prompt = ui->output->toPlainText();
        data.n_predict = ui_SETTINGS.hid_npredict;
        emit ui2net_data(data);
    }

    is_run = true;  //模型正在运行标签
    ui_state_pushing();
    emit ui2net_push();
}
//传递知识库的描述
void Widget::recv_embeddingdb_describe(QString describe) { embeddingdb_describe = describe; }

//传递控制信息
void Widget::recv_controller(int num) {
    QString result;
    if (num == 1)  //最大化主窗口
    {
        setWindowState(windowState() | Qt::WindowMaximized);  //设置窗口最大化
        result = jtr("main window") + jtr("maximized");
    } else if (num == 2)  //最小化主窗口
    {
        this->showMinimized();
        result = jtr("main window") + jtr("minimized");
    } else if (num == 3)  //主窗口置顶
    {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        show();
        result = jtr("main window") + jtr("topped");
    } else if (num == 4)  //取消主窗口置顶
    {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show();
        result = jtr("main window") + jtr("topped canceled");
    } else if (num == 5)  //关闭主窗口
    {
        this->close();
        result = jtr("main window") + jtr("closed");
    } else if (num == 6)  //播放音乐
    {
        music_player.play();
        result = jtr("music") + jtr("started playing");
    } else if (num == 7)  //关闭音乐
    {
        music_player.stop();
        result = jtr("music") + jtr("stopped playback");
    } else if (num == 8)  //打开增殖窗口
    {
        emit ui2expend_show(PREV_WINDOW);
        result = jtr("expend window") + jtr("opened");
    } else if (num == 9)  //关闭增殖窗口
    {
        emit ui2expend_show(NO_WINDOW);
        result = jtr("expend window") + jtr("closed");
    } else {
        result = jtr("The number passed in does not have a corresponding action");
    }
    emit recv_controller_over(result);
}

//分割器被用户拉动时响应
void Widget::onSplitterMoved(int pos, int index) {}

// 根据language.json和language_flag中找到对应的文字
QString Widget::jtr(QString customstr) { return wordsObj[customstr].toArray()[language_flag].toString(); }

// 检测音频支持
bool Widget::checkAudio() {
    // 设置编码器
    audioSettings.setCodec("audio/x-raw");
    audioSettings.setSampleRate(44100);
    audioSettings.setBitRate(128000);
    audioSettings.setChannelCount(2);
    audioSettings.setQuality(QMultimedia::HighQuality);
    // 设置音频编码器参数
    audioRecorder.setEncodingSettings(audioSettings);
    // 设置容器格式
    audioRecorder.setContainerFormat("audio/x-wav");
    // 设置音频输出位置
    audioRecorder.setOutputLocation(QUrl::fromLocalFile(applicationDirPath + "/EVA_TEMP/" + QString("EVA_") + ".wav"));

    // // 打印出音频支持情况
    // // 获取本机支持的音频编码器和解码器
    // QStringList supportedCodecs = audioRecorder.supportedAudioCodecs();
    // QStringList supportedContainers = audioRecorder.supportedContainers();
    // qDebug() << "Supported audio codecs:" << supportedCodecs;
    // qDebug() << "Supported container formats:" << supportedContainers;
    // // 获取实际的编码器设置
    // QAudioEncoderSettings actualSettings = audioRecorder.audioSettings();
    // qDebug() << "Actual Codec:" << actualSettings.codec();
    // qDebug() << "Actual Sample Rate:" << actualSettings.sampleRate() << "Hz";
    // qDebug() << "Actual Bit Rate:" << actualSettings.bitRate() << "bps";
    // qDebug() << "Actual Channel Count:" << actualSettings.channelCount();
    // qDebug() << "Actual Quality:" << actualSettings.quality();
    // qDebug() << "Actual Encoding Mode:" << actualSettings.encodingMode();

    // 获取可用的音频输入设备列表
    QList<QAudioDeviceInfo> availableDevices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (availableDevices.isEmpty()) {
        qDebug() << "No audio input devices available.";
        return false;
    }

    // qDebug() << "Available Audio Input Devices:";
    // for (const QAudioDeviceInfo &deviceInfo : availableDevices) {
    //     qDebug() << "    Device Name:" << deviceInfo.deviceName();
    //     qDebug() << "    Supported Codecs:";
    //     for (const QString &codecName : deviceInfo.supportedCodecs()) {
    //         qDebug() << "        " << codecName;
    //     }
    //     qDebug() << "    Supported Sample Rates:";
    //     for (int sampleRate : deviceInfo.supportedSampleRates()) {
    //         qDebug() << "        " << sampleRate;
    //     }
    //     qDebug() << "    -------------------------------------";
    // }

    return true;
}

//传递格式化后的对话内容
void Widget::recv_chat_format(EVA_CHATS_TEMPLATE chats) { bot_chat = chats; }

// 正在预解码
void Widget::recv_predecoding() { ui_state_pushing(); }

// 完成预解码
void Widget::recv_predecoding_over() { ui_state_normal(); }

