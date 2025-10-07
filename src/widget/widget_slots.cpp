#include "ui_widget.h"
#include "widget.h"
#include <QTcpServer>
#include <QHostAddress>

//-------------------------------------------------------------------------
//-------------------------------响应槽相关---------------------------------
//-------------------------------------------------------------------------

//温度滑块响应
void Widget::temp_change()
{
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0));
}
// ngl滑块响应
void Widget::ngl_change()
{
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(settings_ui->ngl_slider->value()));
}
// nctx滑块响应
void Widget::nctx_change()
{
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(settings_ui->nctx_slider->value()));
}
// repeat滑块响应
void Widget::repeat_change()
{
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0));
}

// top_k 滑块响应
void Widget::topk_change()
{
    settings_ui->topk_label->setText(jtr("top_k") + " " + QString::number(settings_ui->topk_slider->value()));
}

// 并发数量滑块响应
void Widget::parallel_change()
{
    settings_ui->parallel_label->setText(jtr("parallel") + " " + QString::number(settings_ui->parallel_slider->value()));
}

void Widget::nthread_change()
{
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(settings_ui->nthread_slider->value()));
}

//补完状态按钮响应
void Widget::complete_change()
{
    //选中则禁止约定输入
    if (settings_ui->complete_btn->isChecked())
    {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nthread_slider->setEnabled(1);
        settings_ui->nctx_slider->setEnabled(1);
        // 端口设置始终可用（服务状态已移除，本地后端自动启动）
        settings_ui->port_lineEdit->setEnabled(1);
    }
}

//对话状态按钮响应
void Widget::chat_change()
{
    if (settings_ui->chat_btn->isChecked())
    {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nctx_slider->setEnabled(1);
        settings_ui->nthread_slider->setEnabled(1);
        settings_ui->port_lineEdit->setEnabled(1);
    }
}

//服务状态按钮响应
void Widget::web_change()
{
    // 服务状态已移除
}

//提示词模板下拉框响应
void Widget::prompt_template_change()
{
    if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set1"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);
        date_ui->user_name_LineEdit->setEnabled(1);
        date_ui->model_name_LineEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom1_date_system);
        date_ui->user_name_LineEdit->setText(custom1_user_name);
        date_ui->model_name_LineEdit->setText(custom1_model_name);
    }
    else if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set2"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);
        date_ui->user_name_LineEdit->setEnabled(1);
        date_ui->model_name_LineEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom2_date_system);
        date_ui->user_name_LineEdit->setText(custom2_user_name);
        date_ui->model_name_LineEdit->setText(custom2_model_name);
    }
    else
    {
        date_ui->date_prompt_TextEdit->setPlainText(date_map[date_ui->chattemplate_comboBox->currentText()].date_prompt);
        date_ui->date_prompt_TextEdit->setEnabled(0);
        date_ui->user_name_LineEdit->setText(date_map[date_ui->chattemplate_comboBox->currentText()].user_name);
        date_ui->user_name_LineEdit->setEnabled(0);
        date_ui->model_name_LineEdit->setText(date_map[date_ui->chattemplate_comboBox->currentText()].model_name);
        date_ui->model_name_LineEdit->setEnabled(0);
    }
}

void Widget::chooseLorapath()
{
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose lora model"), "(*.bin *.gguf)");

    settings_ui->lora_LineEdit->setText(currentpath);
}

void Widget::chooseMmprojpath()
{
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose mmproj model"), "(*.bin *.gguf)");

    settings_ui->mmproj_LineEdit->setText(currentpath);
}

//响应工具选择
void Widget::tool_change()
{
    QObject *senderObj = sender(); // gets the object that sent the signal

    // 如果是软件工程师则查询python环境
    if (QCheckBox *checkbox = qobject_cast<QCheckBox *>(senderObj))
    {
        if (checkbox == date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked())
        {
            python_env = checkPython();
            compile_env = checkCompile();
        }
    }

    // 判断是否挂载了工具
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
    ui_extra_prompt = create_extra_prompt();
}

//用户按下F1键响应
void Widget::onShortcutActivated_F1()
{
    createTempDirectory("./EVA_TEMP");
    cutscreen_dialog->showFullScreen(); //处理截图事件
}

//用户按下F2键响应
void Widget::onShortcutActivated_F2()
{
    if (whisper_model_path == "") //如果还未指定模型路径则先指定
    {
        emit ui2expend_show(WHISPER_WINDOW); //语音增殖界面
    }
    else if (!is_recodering)
    {
        recordAudio(); //开始录音
        is_recodering = true;
    }
    else if (is_recodering)
    {
        stop_recordAudio(); //停止录音
    }
}

//用户按下CTRL+ENTER键响应
void Widget::onShortcutActivated_CTRL_ENTER()
{
    ui->send->click();
}

//接收传来的图像
void Widget::recv_qimagepath(QString cut_imagepath_)
{
    reflash_state("ui:" + jtr("cut image success"), USUAL_SIGNAL);
    ui->input->addFileThumbnail(cut_imagepath_);
}

// 服务模式已移除

// bot将模型参数传递给ui
void Widget::recv_params(MODEL_PARAMS p)
{
    ui_n_ctx_train = p.n_ctx_train;
    settings_ui->nctx_slider->setMaximum(p.n_ctx_train); // 没有拓展4倍,因为批解码时还是会失败
    ui_maxngl = p.max_ngl;                               // gpu负载层数是n_layer+1
    settings_ui->ngl_slider->setMaximum(ui_maxngl);
    if (ui_SETTINGS.ngl == 999)
    {
        ui_SETTINGS.ngl = ui_maxngl;
    } //及时修正999值
}

//接收缓存量
void Widget::recv_kv(float percent, int ctx_size)
{
    Q_UNUSED(percent);
    Q_UNUSED(ctx_size);

}

// 播放装载动画的槽已废弃；直接在 preLoad() 中调用 load_play()

//更新gpu内存使用率
void Widget::recv_gpu_status(float vmem, float vramp, float vcore, float vfree_)
{
    vfree = vfree_; //剩余显存
    ui->vcore_bar->setValue(vcore);
    //取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if (is_first_getvram)
    {
        is_first_getvram = false;
        first_vramp = vramp;
        ui->vram_bar->setValue(first_vramp);
    }
    ui->vram_bar->setSecondValue(vramp - first_vramp);

    if (gpu_wait_load)
    {
        gpu_wait_load = false;
#ifdef BODY_USE_GPU
        // 以文件体积近似估计显存占用：若模型大小低于当前可用显存的95%，则尝试全量 offload（ngl=999）
        QFileInfo fileInfo(ui_SETTINGS.modelpath);   // 模型文件大小
        QFileInfo fileInfo2(ui_SETTINGS.mmprojpath); // mmproj 文件大小（可为空）
        const int modelsize_MB = fileInfo.size() / 1024 / 1024 + fileInfo2.size() / 1024 / 1024;
        const double limit = 0.95 * vfree; // 95% 当前可用显存
        if (modelsize_MB > 0 && vfree > 0 && modelsize_MB <= limit)
        {
            ui_SETTINGS.ngl = 999; // 初次装载：尽可能全量 offload
        }
        else
        {
            ui_SETTINGS.ngl = 0; // 不足则先走纯CPU/少量 offload
        }
#endif
        // 应用新设置并按需重启本地服务
        if (ui_mode == LOCAL_MODE) ensureLocalServer();
    }
}

//传递cpu信息
void Widget::recv_cpu_status(double cpuload, double memload)
{
    ui->cpu_bar->setValue(cpuload);
    //取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if (is_first_getmem)
    {
        first_memp = memload;
        ui->mem_bar->setValue(first_memp);
        is_first_getmem = false;
    }
    ui->mem_bar->setSecondValue(memload - first_memp);
    // ui->mem_bar->setValue(physMemUsedPercent-(model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    // ui->mem_bar->setSecondValue((model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
}

//事件过滤器,鼠标跟踪效果不好要在各种控件单独实现
bool Widget::eventFilter(QObject *obj, QEvent *event)
{
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->input && event->type() == QEvent::ContextMenu && ui_state == CHAT_STATE)
    {
        QContextMenuEvent *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        // 显示菜单
        right_menu->exec(contextMenuEvent->globalPos());
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->lora_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseLorapath();
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->mmproj_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseMmprojpath();
        return true;
    }
    // 取消通过右击装载按钮进入链接模式的逻辑（改为点击装载后弹模式选择）
    //响应已安装控件上的鼠标右击事件
    if (obj == api_endpoint_LineEdit && event->type() == QEvent::ContextMenu)
    {
        QString api_endpoint = "http://" + getFirstNonLoopbackIPv4Address() + ":8080";
        api_endpoint_LineEdit->setText(api_endpoint);
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->state && event->type() == QEvent::ContextMenu)
    {
        emit ui2expend_show(PREV_WINDOW); // 1是模型信息页
        return true;
    }

    return QObject::eventFilter(obj, event);
}

//传递模型预解码的内容
void Widget::recv_predecode(QString bot_predecode_content_)
{
    bot_predecode_content = bot_predecode_content_;
}

//接收whisper解码后的结果
void Widget::recv_speechdecode_over(QString result)
{
    ui_state_normal();
    ui->input->textEdit->append(result);
    // ui->send->click();//尝试一次发送
}

//接收模型路径
void Widget::recv_whisper_modelpath(QString modelpath)
{
    whisper_model_path = modelpath;
}

// Ensure local llama.cpp server is running with current settings
void Widget::ensureLocalServer()
{
    if (!serverManager) return;
    // 首次装载前：根据当前可用显存粗略评估是否可全量 offload（ngl=999）
#ifdef BODY_USE_GPU
    if (!firstAutoNglEvaluated_ && !serverManager->isRunning())
    {
        firstAutoNglEvaluated_ = true; // 只评估一次
        QFileInfo fileInfo(ui_SETTINGS.modelpath);
        QFileInfo fileInfo2(ui_SETTINGS.mmprojpath);
        const int modelsize_MB = fileInfo.size() / 1024 / 1024 + fileInfo2.size() / 1024 / 1024;
        if (modelsize_MB > 0 && vfree > 0)
        {
            const double limit = 0.95 * vfree; // 95% 当前可用显存
            if (modelsize_MB <= limit)
            {
                ui_SETTINGS.ngl = 999; // 先尝试全量 offload；装载后再按 n_layer+1 修正显示
                if (settings_ui && settings_ui->ngl_slider)
                {
                    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
                    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl));
                }
            }
        }
        else if (modelsize_MB > 0 && vfree <= 0)
        {
            // 无法拿到可用显存数据：延后到下一次 GPU 刷新回调中处理
            gpu_wait_load = true;
        }
    }
#endif
    // Determine bind host and a usable port before starting server.
    auto pickFreePort = []() -> QString {
        QTcpServer s;
        // Ask OS for any free IPv4 port
        if (s.listen(QHostAddress::AnyIPv4, 0)) {
            const quint16 p = s.serverPort();
            s.close();
            return QString::number(p);
        }
        return QString(DEFAULT_SERVER_PORT);
    };
    auto isPortFree = [](quint16 port, const QHostAddress &addr) -> bool {
        QTcpServer s;
        const bool ok = s.listen(addr, port);
        if (ok) s.close();
        return ok;
    };

    QString bindHost = "0.0.0.0"; // default: expose to LAN
    QString chosenPort = ui_port.trimmed();

    if (chosenPort.isEmpty()) {
        // If user cleared the port, bind only to localhost with a random port
        bindHost = "127.0.0.1";
        chosenPort = pickFreePort();
        // keep ui_port empty to indicate no exposure
        if (settings_ui && settings_ui->port_lineEdit) {
            settings_ui->port_lineEdit->setPlaceholderText("blank = localhost only (random port)");
        }
        reflash_state("ui:port cleared -> bind 127.0.0.1", SIGNAL_SIGNAL);
    } else {
        bool ok = false;
        const quint16 portNum = chosenPort.toUShort(&ok);
        if (!ok || portNum == 0 || !isPortFree(portNum, QHostAddress(QHostAddress::AnyIPv4))) {
            const QString newPort = pickFreePort();
            if (newPort != chosenPort) {
                reflash_state("ui:port in use, switch to " + newPort, SIGNAL_SIGNAL);
                chosenPort = newPort;
                ui_port = newPort;
                if (settings_ui && settings_ui->port_lineEdit) {
                    settings_ui->port_lineEdit->setText(newPort);
                }
            }
        }
    }


    // 同步配置到本地后端管理器
    serverManager->setSettings(ui_SETTINGS);
    serverManager->setHost(bindHost);
    serverManager->setPort(chosenPort);
    serverManager->setModelPath(ui_SETTINGS.modelpath);
    serverManager->setMmprojPath(ui_SETTINGS.mmprojpath);
    serverManager->setLoraPath(ui_SETTINGS.lorapath);

    // 判断是否需要重启，若需要则切到装载中并中止当前网络请求
    lastServerRestart_ = serverManager->needsRestart();
    const bool hadOld = serverManager->isRunning();
    // Fresh start or planned restart -> next "all slots are idle" is just baseline; suppress one speed line
    if (lastServerRestart_ || !hadOld) suppressNextAllIdle_ = true;
    ignoreNextServerStopped_ = lastServerRestart_ && hadOld;
    if (lastServerRestart_)
    {
        // 标记为未装载并进入装载中状态；这会禁用发送等控件
        preLoad();
        emit ui2net_stop(true); // 停止可能仍在进行的 SSE 回复
    }

    // 确保后端进程状态符合当前设置
    serverManager->ensureRunning();

    // 立即将端点切换到本地（避免还连向旧端点）
    apis.api_endpoint = serverManager->endpointBase();
    apis.api_key = "";
    apis.api_model = "default";
    emit ui2net_apis(apis);
}

// When local server is ready, switch UI to xNet over local endpoint
void Widget::onServerReady(const QString &endpoint)
{
    // 配置本地端点；统一由动画收尾逻辑 unlockLoad() 设置标题/图标/状态
    apis.api_endpoint = endpoint;
    apis.api_key = "";
    apis.api_model = "default";
    emit ui2net_apis(apis);

    // 完成装载动画：记录耗时，补帧并快速播完剩余动画，最后 unlockLoad()
    load_time = load_timer.isValid() ? (load_timer.nsecsElapsed() / 1e9) : 0.0;
    ui_mode = LOCAL_MODE;

    ui->output->clear();
    ui_messagesArray = QJsonArray();
    {
        QJsonObject systemMessage;
        systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
        systemMessage.insert("content", ui_DATES.date_prompt);
        ui_messagesArray.append(systemMessage);
        if (history_ && ui_state == CHAT_STATE)
        {
            SessionMeta meta;
            meta.id = QString::number(QDateTime::currentMSecsSinceEpoch());
            meta.title = "";
            meta.endpoint = endpoint;
            meta.model = ui_SETTINGS.modelpath;
            meta.system = ui_DATES.date_prompt;
            meta.n_ctx = ui_SETTINGS.nctx;
            meta.slot_id = -1;
            meta.startedAt = QDateTime::currentDateTime();
            history_->begin(meta);
            history_->appendMessage(systemMessage);
            currentSlotId_ = -1;
        }
    }
    bot_predecode_content = ui_DATES.date_prompt; // 使用系统指令作为“预解码内容”展示
    is_load = true;
    // After fresh load, the first "all slots are idle" is an idle baseline -> ignore once
    lastServerRestart_ = false; // 一次重启流程结束
    all_fps++;                  // 补上最后一帧，表示上下文也创建了
    if (load_pTimer)
    {
        load_pTimer->stop();    // 停止动画，但保留 load_action
        load_pTimer->start(10); // 快速播放完剩下的动画
    }
    else
    {
        // 兜底：没有动画定时器则直接解锁
        unlockLoad();
    }
}

//链接模式的发送处理
void Widget::api_send_clicked_slove()
{
    // 注：联机模式也加前后缀
    QString input;

    // Begin a new turn: reset KV/speed trackers
    turnActive_ = true;
    kvUsedBeforeTurn_ = kvUsed_;
    kvStreamedTurn_ = 0;
    lastPromptTps_ = -1.0;
    lastGenTps_ = -1.0;
    sawPromptTps_ = false;
    sawGenTps_ = false;
    turnTimer_.restart();

    emit ui2net_stop(0);
    ENDPOINT_DATA data;
    data.date_prompt = ui_DATES.date_prompt;
    data.input_pfx = ui_DATES.user_name;
    data.input_sfx = ui_DATES.model_name;
    data.stopwords = ui_DATES.extra_stop_words;
    data.is_complete_state = (ui_state == COMPLETE_STATE);
    data.temp = ui_SETTINGS.temp;
    data.repeat = ui_SETTINGS.repeat;
    data.top_k = ui_SETTINGS.top_k;
    data.n_predict = ui_SETTINGS.hid_npredict;
    data.messagesArray = ui_messagesArray;
    data.id_slot = currentSlotId_;

    if (tool_result == "")
    {
        input = ui->input->textEdit->toPlainText().toUtf8().data();
        ui->input->textEdit->clear();
    }

    QStringList images_filepath = ui->input->imageFilePaths(); // 获取图像列表
    QStringList wavs_filepath = ui->input->wavFilePaths();     // 获取音频列表
    ui->input->clearThumbnails();                              // 清空发送区缩略图

    if (ui_state == CHAT_STATE)
    {
        // ensure persistent history session is created only when sending, not on reset
        if (history_ && history_->sessionId().isEmpty()) {
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
        //----------------------- 处理工具消息 ----------------------------
        if (tool_result != "")
        {
            // 目前通过 user 角色推给 net
            QJsonObject roleMessage;
            roleMessage.insert("role", DEFAULT_USER_NAME);
            roleMessage.insert("content", "tool_response: " + tool_result);
            ui_messagesArray.append(roleMessage);
            if (history_ && ui_state == CHAT_STATE) history_->appendMessage(roleMessage);
            reflash_output(QString(DEFAULT_SPLITER) + DEFAULT_USER_NAME + DEFAULT_SPLITER + "tool_response: " + tool_result + DEFAULT_SPLITER + ui_DATES.model_name + DEFAULT_SPLITER, 0, TOOL_BLUE);

            tool_result = "";
            QTimer::singleShot(100, this, SLOT(tool_testhandleTimeout()));
            is_run = true;
            ui_state_pushing();
            return;
        }
        //----------------------- 处理用户消息 ----------------------------
        else
        {
            if (images_filepath.isEmpty())
            {
                QJsonObject roleMessage;
                roleMessage.insert("role", DEFAULT_USER_NAME);
                roleMessage.insert("content", input);
                ui_messagesArray.append(roleMessage);
                if (history_) history_->appendMessage(roleMessage);
            }
            else // 有图片的用户消息
            {
                QJsonObject message;
                message["role"] = DEFAULT_USER_NAME;
                QJsonArray contentArray;

                // 先添加用户文本
                if (!input.isEmpty())
                {
                    QJsonObject textMessage;
                    textMessage.insert("type", "text");
                    textMessage.insert("text", input);
                    contentArray.append(textMessage);
                }

                // 再添加图像信息
                for (int i = 0; i < images_filepath.size(); ++i)
                {
                    QFile imageFile(images_filepath[i]);
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
                    showImages({images_filepath[i]}); // 展示图片
                }

                message["content"] = contentArray;
                ui_messagesArray.append(message);
                if (history_) history_->appendMessage(message);
            }

            // 可选：添加音频（保持现有 UI 结构，xnet 侧会转换为 input_audio）
            if (!wavs_filepath.isEmpty())
            {
                QJsonObject message;
                message["role"] = DEFAULT_USER_NAME;
                QJsonArray contentArray;

                for (int i = 0; i < wavs_filepath.size(); ++i)
                {
                    QString filePath = wavs_filepath[i];
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
                    QString mimeType = "audio/mpeg"; // 默认MP3
                    if (extension == "wav")
                    {
                        mimeType = "audio/wav";
                    }
                    else if (extension == "ogg")
                    {
                        mimeType = "audio/ogg";
                    }
                    else if (extension == "flac")
                    {
                        mimeType = "audio/flac";
                    }

                    QString base64String = QString("data:%1;base64,").arg(mimeType) + base64Data;

                    QJsonObject audioObject;
                    audioObject["type"] = "audio_url"; // 仍用 audio_url，xnet 里转换为 input_audio
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
            reflash_output(QString(DEFAULT_SPLITER) + ui_DATES.user_name + DEFAULT_SPLITER, 0, SYSTEM_BLUE);  //前缀蓝色
            reflash_output(input, 0, NORMAL_BLACK);                                                           //正文黑色
            reflash_output(QString(DEFAULT_SPLITER) + ui_DATES.model_name + DEFAULT_SPLITER, 0, SYSTEM_BLUE); //后缀蓝色
            data.n_predict = ui_SETTINGS.hid_npredict;
            emit ui2net_data(data);
        }
    }
    else if (ui_state == COMPLETE_STATE) // 直接把 output 上的文本作为提示词
    {
        data.input_prompt = ui->output->toPlainText();
        data.n_predict = ui_SETTINGS.hid_npredict;
        emit ui2net_data(data);
    }

    is_run = true; // 模型运行标记
    ui_state_pushing();
    // carry over tokens from previous turn before starting a new one
    if (kvTokensTurn_ > 0) { kvTokensAccum_ += kvTokensTurn_; kvTokensTurn_ = 0; }
    emit ui2net_push();
}
//传递知识库的描述
void Widget::recv_embeddingdb_describe(QString describe)
{
    embeddingdb_describe = describe;
}

//传递控制信息
void Widget::recv_controller(int num)
{
    QString result;
    if (num == 1) //最大化主窗口
    {
        setWindowState(windowState() | Qt::WindowMaximized); //设置窗口最大化
        result = jtr("main window") + jtr("maximized");
    }
    else if (num == 2) //最小化主窗口
    {
        this->showMinimized();
        result = jtr("main window") + jtr("minimized");
    }
    else if (num == 3) //主窗口置顶
    {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        show();
        result = jtr("main window") + jtr("topped");
    }
    else if (num == 4) //取消主窗口置顶
    {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show();
        result = jtr("main window") + jtr("topped canceled");
    }
    else if (num == 5) //关闭主窗口
    {
        this->close();
        result = jtr("main window") + jtr("closed");
    }
    else if (num == 6) //播放音乐
    {
        music_player.play();
        result = jtr("music") + jtr("started playing");
    }
    else if (num == 7) //关闭音乐
    {
        music_player.stop();
        result = jtr("music") + jtr("stopped playback");
    }
    else if (num == 8) //打开增殖窗口
    {
        emit ui2expend_show(PREV_WINDOW);
        result = jtr("expend window") + jtr("opened");
    }
    else if (num == 9) //关闭增殖窗口
    {
        emit ui2expend_show(NO_WINDOW);
        result = jtr("expend window") + jtr("closed");
    }
    else
    {
        result = jtr("The number passed in does not have a corresponding action");
    }
    emit recv_controller_over(result);
}

//分割器被用户拉动时响应
void Widget::onSplitterMoved(int pos, int index) {}

// 根据language.json和language_flag中找到对应的文字
QString Widget::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}

// 检测音频支持
bool Widget::checkAudio()
{
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
    if (availableDevices.isEmpty())
    {
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
void Widget::recv_chat_format(EVA_CHATS_TEMPLATE chats)
{
    bot_chat = chats;
}

// 正在预解码
void Widget::recv_predecoding()
{
    ui_state_pushing();
}

// 完成预解码
void Widget::recv_predecoding_over()
{
    ui_state_normal();
}

// Initialize memory policy for text widgets: disable undo, cap logs
void Widget::initTextComponentsMemoryPolicy()
{
    // Output area (QTextEdit): no undo stack to avoid growth on streaming inserts
    ui->output->setUndoRedoEnabled(false);

    // State log (QPlainTextEdit): disable undo and cap block count
    ui->state->setUndoRedoEnabled(false);
    ui->state->setMaximumBlockCount(5000); // prevent unbounded log growth
}

#include <QTextDocument>

#include <QFont>
// Replace output document to drop undo stack and cached resources (images)
void Widget::resetOutputDocument()
{
    // Preserve existing visual settings (font, tab stops) before swapping the doc.
    const QFont prevFont = ui->output->font();
    const qreal prevTabStop = ui->output->tabStopDistance();

    // Create a fresh document and hand ownership to the widget.
    // Qt will destroy the previous document for us; avoid manual delete.
    QTextDocument *doc = new QTextDocument(ui->output);
    doc->setUndoRedoEnabled(false);
    doc->setDefaultFont(prevFont); // keep the same font after reset
    ui->output->setDocument(doc);

    // Reapply widget-level settings that the new document doesn't carry.
    ui->output->setFont(prevFont);
    ui->output->setTabStopDistance(prevTabStop);
}

// Replace state document to drop undo stack quickly
void Widget::resetStateDocument()
{
    // Same as above: let Qt own and clean up the previous document.
    QTextDocument *doc = new QTextDocument(ui->state);
    doc->setUndoRedoEnabled(false);
    ui->state->setDocument(doc);
}

// Refresh kv progress bar based on current counters
void Widget::updateKvBarUi()
{
    // determine capacity: prefer n_ctx_slot from server; fallback to UI nctx
    int cap = slotCtxMax_ > 0 ? slotCtxMax_ : (ui_SETTINGS.nctx > 0 ? ui_SETTINGS.nctx : DEFAULT_NCTX);
    if (cap <= 0) cap = DEFAULT_NCTX;
    int used = qMax(0, kvUsed_);
    int percent = cap > 0 ? int(qRound(100.0 * double(used) / double(cap))) : 0;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    // Use second (yellow) segment to indicate memory; first (blue) set to 0
    ui->kv_bar->setValue(0);
    ui->kv_bar->setSecondValue(percent);
    ui->kv_bar->setShowText(QString::fromUtf8("记忆:"));
    ui->kv_bar->setCenterText(""); // use show_text + auto % rendering
    ui->kv_bar->setToolTip(QString::fromUtf8("上下文缓存量 %1 / %2 token").arg(used).arg(cap));
}

// Update kv from llama.cpp server timings/stream (usedTokens = prompt_n + streamed chunks)
void Widget::recv_kv_from_net(int usedTokens)
{
    // Approximate KV usage accumulation during streaming tokens from xNet.
    // In LINK mode, we don't have local server logs; use this as fallback.
    if (!turnActive_) return;
    kvStreamedTurn_ = qMax(0, usedTokens); // count observed streamed chunks as tokens
    // For both local and link, keep a running approximation; local will be corrected by server logs later
    kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
    updateKvBarUi();
}

// server-assigned slot id -> persist and reuse for KV cache efficiency
void Widget::onSlotAssigned(int slotId)
{
    if (slotId < 0) return;
    if (currentSlotId_ == slotId) return;
    currentSlotId_ = slotId;
    if (history_) history_->updateSlotId(slotId);
    reflash_state(QString("net:slot id=%1").arg(slotId), SIGNAL_SIGNAL);
}

// Capture the approximate tokens generated within <think> this turn
void Widget::recv_reasoning_tokens(int tokens)
{
    lastReasoningTokens_ = qMax(0, tokens);
}

// Parse llama-server output lines to capture n_ctx value for verification
void Widget::onServerOutput(const QString &line)
{
    // 0) Track turn lifecycle heuristics
    // Start/resume turn timer when server prints a new prompt line
    if (line.contains("new prompt") || line.contains("launch_slot_"))
    {
        turnActive_ = true;
        kvUsedBeforeTurn_ = kvUsed_;
        kvStreamedTurn_ = 0;
        lastPromptTps_ = -1.0;
        lastGenTps_ = -1.0;
        sawPromptTps_ = false;
        sawGenTps_ = false;
        sawFinalPast_ = false;
        turnTimer_.restart();
    }

    // 1) capture n_ctx for verification
    // 0) capture n_ctx_train from print_info and clamp UI nctx slider max accordingly
    static QRegularExpression reCtxTrain("print_info\\s*:\\s*n_ctx_train\\s*=\\s*(\\d+)");
    for (auto it = reCtxTrain.globalMatch(line); it.hasNext();)
    {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int train = m.captured(1).toInt(&ok);
        if (ok && train > 0)
        {
            if (ui_n_ctx_train != train)
            {
                ui_n_ctx_train = train;
                if (settings_ui && settings_ui->nctx_slider)
                {
                    const int curMax = settings_ui->nctx_slider->maximum();
                    if (curMax != train) settings_ui->nctx_slider->setMaximum(train);
                    // 仅更新最大值，不强制改动当前值；避免用户未修改的情况下导致“看似变化”
                }
            }
        }
    }

    static QRegularExpression reCtx("llama_context\\s*:\\s*n_ctx\\s*=\\s*(\\d+)");
    for (auto it = reCtx.globalMatch(line); it.hasNext();)
    {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int v = m.captured(1).toInt(&ok);
        if (ok && v > 0)
        {
            server_nctx_ = v;
            if (server_nctx_ != ui_SETTINGS.nctx)
            {
                reflash_state("ui:server n_ctx=" + QString::number(server_nctx_) + ", ui n_ctx=" + QString::number(ui_SETTINGS.nctx), SIGNAL_SIGNAL);
            }
        }
    }

    // 1.2) capture per-slot capacity n_ctx_slot
    static QRegularExpression reCtxSlot("n_ctx_slot\\s*=\\s*(\\d+)");
    for (auto it = reCtxSlot.globalMatch(line); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        bool ok = false; const int v = m.captured(1).toInt(&ok);
        if (ok && v > 0) { slotCtxMax_ = v; updateKvBarUi(); }
    }

    // 1.3) kv cache rm [hit, end) -> use left number to correct current memory right away
    static QRegularExpression reKvRm("kv cache rm\\s*\\[\\s*(\\d+)");
    QRegularExpressionMatch mRm = reKvRm.match(line);
    if (mRm.hasMatch()) {
        bool ok=false; int hit = mRm.captured(1).toInt(&ok);
        if (ok) { kvUsed_ = qMax(0, hit); updateKvBarUi(); }
    }

    // 1.5) capture n_layer and clamp GPU offload slider max to n_layer + 1
    static QRegularExpression reLayer("print_info\\s*:\\s*n_layer\\s*=\\s*(\\d+)");
    for (auto it = reLayer.globalMatch(line); it.hasNext();)
    {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int layers = m.captured(1).toInt(&ok);
        if (ok && layers > 0)
        {
            const int maxngl = layers + 1; // llama.cpp convention
            if (ui_maxngl != maxngl)
            {
                ui_maxngl = maxngl;
                if (settings_ui && settings_ui->ngl_slider)
                {
                    const int curMax = settings_ui->ngl_slider->maximum();
                    if (curMax != maxngl) settings_ui->ngl_slider->setMaximum(maxngl);
                    // 不强制覆盖 ui_SETTINGS.ngl；仅更新显示，保持 999 与 maxngl 的等价关系由确认时判断
                    int curVal = settings_ui->ngl_slider->value();
                    if (curVal > maxngl) curVal = maxngl; // slider 可能会被系统自动夹紧
                    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(curVal));
                    // reflash_state("ui:max ngl = " + QString::number(maxngl), SIGNAL_SIGNAL);
                }
            }
        }
    }

    // 2) Chat format
    static QRegularExpression reFmt("Chat\\s+format\\s*:\\s*(.+)");
    QRegularExpressionMatch mFmt = reFmt.match(line);
    if (mFmt.hasMatch())
    {
        const QString fmt = mFmt.captured(1).trimmed();
        // reflash_state("srv: Chat format: " + fmt, USUAL_SIGNAL);
    }

    // 3) prompt eval / eval time speeds and total tokens
    static QRegularExpression rePrompt(
        "prompt\\s*(?:eval\\s*)?time\\s*=\\s*([0-9.]+)\\s*ms\\s*/\\s*(\\d+)\\s*tokens\\s*\\(.*?,\\s*([0-9.]+)\\s*tokens per second\\)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression rePromptAlt(
        "prompt\\s*(?:processing\\s*)?time\\s*=\\s*([0-9.]+)\\s*ms.*?([0-9.]+)\\s*tokens per second",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reGen(
        "^\\s*eval\\s+time\\s*=\\s*([0-9.]+)\\s*ms\\s*/\\s*(\\d+)\\s*tokens\\s*\\(.*?,\\s*([0-9.]+)\\s*tokens per second\\)",
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression reTotal("total\\s+time\\s*=\\s*([0-9.]+)\\s*ms\\s*/\\s*(\\d+)\\s*tokens");

    QRegularExpressionMatch mPrompt = rePrompt.match(line);
    if (mPrompt.hasMatch()) {
        // tokens per second for prompt processing is group 3
        lastPromptTps_ = mPrompt.captured(3).toDouble();
        sawPromptTps_ = true;
    } else {
        QRegularExpressionMatch m1b = rePromptAlt.match(line);
        if (m1b.hasMatch()) {
            lastPromptTps_ = m1b.captured(2).toDouble();
            sawPromptTps_ = true;
        }
    }

    QRegularExpressionMatch mGen = reGen.match(line);
    if (mGen.hasMatch()) {
        lastGenTps_ = mGen.captured(3).toDouble();
        sawGenTps_ = true;
    }

    QRegularExpressionMatch m3 = reTotal.match(line);
    if (m3.hasMatch())
    {
        const int totalTokens = m3.captured(2).toInt();
        // Use total tokens to correct current slot KV usage for this turn
        // 优先级：如果已看到 stop processing 的 n_past，则不再用 total 覆盖
        kvStreamedTurn_ = totalTokens; // better approximation than chunk count
        if (!sawFinalPast_)
        {
            kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
            updateKvBarUi();
        }
    }

    // 4) prompt done / progress / stop processing -> correct kvUsed_ from n_past immediately
    static QRegularExpression rePromptDone("prompt\\s+done,\\s*n_past\\s*=\\s*(\\d+)");
    static QRegularExpression reProgress("prompt\\s+processing\\s+progress,\\s*n_past\\s*=\\s*(\\d+)");
    static QRegularExpression reStop("stop\\s+processing.*n_past\\s*=\\s*(\\d+)");
    QRegularExpressionMatch mPD = rePromptDone.match(line);
    if (mPD.hasMatch()) {
        bool ok=false; int past = mPD.captured(1).toInt(&ok);
        if (ok) { kvUsed_ = qMax(0, past); updateKvBarUi(); }
    }
    QRegularExpressionMatch mProg = reProgress.match(line);
    if (mProg.hasMatch()) {
        bool ok=false; int past = mProg.captured(1).toInt(&ok);
        if (ok) { kvUsed_ = qMax(0, past); updateKvBarUi(); }
    }
    QRegularExpressionMatch mStop = reStop.match(line);
    if (mStop.hasMatch()) {
        bool ok=false; int past = mStop.captured(1).toInt(&ok);
        if (ok) { kvUsed_ = qMax(0, past); sawFinalPast_ = true; updateKvBarUi(); }
    }
    // 5) all slots idle -> finalize speeds for this turn
    if (line.contains("all slots are idle"))
    {
        // Suppress the very first idle baseline after (re)start to avoid a fake "速度"行闪烁
        if (suppressNextAllIdle_) { suppressNextAllIdle_ = false; return; }
        turnActive_ = false;
        double promptTps = sawPromptTps_ ? lastPromptTps_ : -1.0;
        double genTps = sawGenTps_ ? lastGenTps_ : -1.0;
        if (genTps <= 0.0) {
            // fallback: use streamed token count divided by turn time (seconds)
            const double secs = turnTimer_.isValid() ? (turnTimer_.nsecsElapsed() / 1e9) : 0.0;
            if (secs > 0.0 && kvStreamedTurn_ > 0) genTps = double(kvStreamedTurn_) / secs;
        }
        // report one-line combined speeds with prefix "ui:"
        const QString genStr = (genTps > 0.0) ? (QString::number(genTps, 'f', 1) + " tokens/s") : QString::fromUtf8("--");
        const QString promptStr = (promptTps > 0.0) ? (QString::number(promptTps, 'f', 1) + " tokens/s") : QString::fromUtf8("--");
        reflash_state(QString::fromUtf8("ui:") + jtr("single decode") + " " + genStr  + " " + jtr("batch decode")  + " " + (ui_mode == LOCAL_MODE ? promptStr : QString::fromUtf8("--")), SUCCESS_SIGNAL);
    }
}
