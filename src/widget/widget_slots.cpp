#include "ui_widget.h"
#include "widget.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTcpServer>
#include "../utils/startuplogger.h"

//-------------------------------------------------------------------------
//-------------------------------响应槽相关---------------------------------
//-------------------------------------------------------------------------

// 温度滑块响应
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

// top_p 滑块响应
void Widget::topp_change()
{
    const double val = settings_ui->topp_slider->value() / 100.0;
    settings_ui->topp_label->setText("TOP_P " + QString::number(val));
    settings_ui->topp_label->setToolTip(QString::fromUtf8("核采样阈值（top_p），范围 0.00–1.00；当前：%1")
                                            .arg(QString::number(val, 'f', 2)));
}

// 并发数量滑块响应
void Widget::npredict_change()
{
    const int value = settings_ui->npredict_spin->value();
    const QString text = jtr("npredict") + " " + QString::number(value);
    settings_ui->npredict_label->setText(text);
    settings_ui->npredict_label->setToolTip(text);
    settings_ui->npredict_spin->setToolTip(text);
}

void Widget::parallel_change()
{
    settings_ui->parallel_label->setText(jtr("parallel") + " " + QString::number(settings_ui->parallel_slider->value()));
}

void Widget::nthread_change()
{
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(settings_ui->nthread_slider->value()));
}

// 补完状态按钮响应
void Widget::complete_change()
{
    // 选中则禁止约定输入
    if (settings_ui->complete_btn->isChecked())
    {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nthread_slider->setEnabled(1);
        settings_ui->nctx_slider->setEnabled(1);
        // 端口设置始终可用（服务状态已移除，本地后端自动启动）
        settings_ui->port_lineEdit->setEnabled(1);
    }
}

// 对话状态按钮响应
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

// 服务状态按钮响应
void Widget::web_change()
{
    // 服务状态已移除
}

// 提示词模板下拉框响应
void Widget::prompt_template_change()
{
    if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set1"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom1_date_system);
    }
    else if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set2"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom2_date_system);
    }
    else
    {
        date_ui->date_prompt_TextEdit->setPlainText(date_map[date_ui->chattemplate_comboBox->currentText()].date_prompt);
        date_ui->date_prompt_TextEdit->setEnabled(0);
    }
}

void Widget::chooseLorapath()
{
    // 用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose lora model"), "(*.bin *.gguf)");

    settings_ui->lora_LineEdit->setText(currentpath);
}

void Widget::chooseMmprojpath()
{
    // 用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose mmproj model"), "(*.bin *.gguf)");

    settings_ui->mmproj_LineEdit->setText(currentpath);
}

// 响应工具选择
void Widget::tool_change()
{
    QObject *senderObj = sender(); // gets the object that sent the signal

    // 如果是软件工程师则查询python环境
    if (QCheckBox *checkbox = qobject_cast<QCheckBox *>(senderObj))
    {
        if (checkbox == date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked())
        {
            triggerEngineerEnvRefresh(true);
            // If a work dir was chosen previously, reuse it silently
            const QString fallback = QDir(applicationDirPath).filePath("EVA_WORK");
            const QString current = engineerWorkDir.isEmpty() ? fallback : engineerWorkDir;
            if (engineerWorkDir.isEmpty())
            {
                // Prompt only when not set before (or path missing)
                QString picked = QFileDialog::getExistingDirectory(this, jtr("choose work dir"), current,
                                                                   QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                if (!picked.isEmpty())
                {
                    setEngineerWorkDir(picked);
                    // Persist immediately to avoid losing selection on crash
                    auto_save_user();
                }
                else // user canceled and not set previously -> stick to default silently
                {
                    setEngineerWorkDir(current);
                    auto_save_user();
                }
            }
            else
            {
                // Already determined -> just propagate to tool to ensure it's in sync
                emit ui2tool_workdir(engineerWorkDir);
            }

            // 显示“工程师工作目录”行（在约定对话框）并更新显示
            if (date_ui->date_engineer_workdir_LineEdit)
            {
                date_ui->date_engineer_workdir_label->setVisible(true);
                date_ui->date_engineer_workdir_LineEdit->setVisible(true);
                date_ui->date_engineer_workdir_browse->setVisible(true);
                date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
            }
        }
    }

    // 取消工程师挂载时隐藏该行
    if (QCheckBox *checkbox2 = qobject_cast<QCheckBox *>(senderObj))
    {
        if (checkbox2 == date_ui->engineer_checkbox && !date_ui->engineer_checkbox->isChecked())
        {
            if (date_ui->date_engineer_workdir_LineEdit)
            {
                date_ui->date_engineer_workdir_label->setVisible(false);
                date_ui->date_engineer_workdir_LineEdit->setVisible(false);
                date_ui->date_engineer_workdir_browse->setVisible(false);
            }
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

// 用户按下F1键响应
void Widget::onShortcutActivated_F1()
{
    createTempDirectory("./EVA_TEMP");
    cutscreen_dialog->showFullScreen(); // 处理截图事件
}

// 用户按下F2键响应
void Widget::onShortcutActivated_F2()
{
    if (whisper_model_path == "") // 如果还未指定模型路径则先指定
    {
        emit ui2expend_show(WHISPER_WINDOW); // 语音增殖界面
    }
    else if (!is_recodering)
    {
        recordAudio(); // 开始录音
        is_recodering = true;
    }
    else if (is_recodering)
    {
        stop_recordAudio(); // 停止录音
    }
}

// 用户按下CTRL+ENTER键响应
void Widget::onShortcutActivated_CTRL_ENTER()
{
    ui->send->click();
}

// 接收传来的图像
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
    if (settings_ui && settings_ui->ngl_slider)
    {
        settings_ui->ngl_slider->setMaximum(ui_maxngl);
    }
    const bool shouldAdoptMax = (ui_SETTINGS.ngl == 999 && ui_maxngl > 0);
    const bool shouldClamp = (ui_maxngl > 0 && ui_SETTINGS.ngl > ui_maxngl);
    if (shouldAdoptMax || shouldClamp)
    {
        ui_SETTINGS.ngl = ui_maxngl;
    }
    if (settings_ui && settings_ui->ngl_slider)
    {
        const int sliderValue = qBound(settings_ui->ngl_slider->minimum(), ui_SETTINGS.ngl, settings_ui->ngl_slider->maximum());
        if (sliderValue != settings_ui->ngl_slider->value()) settings_ui->ngl_slider->setValue(sliderValue);
        settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(sliderValue));
    } // 确保 UI 展示真实层数
}

// 接收缓存量
void Widget::recv_kv(float percent, int ctx_size)
{
    Q_UNUSED(percent);
    Q_UNUSED(ctx_size);
}

// 装载动画相关的槽已移除；现统一使用等待动画 wait_play()

// 更新gpu内存使用率
void Widget::recv_gpu_status(float vmem, float vramp, float vcore, float vfree_)
{
    Q_UNUSED(vmem);
    vfree = vfree_; // 剩余显存
    ui->vcore_bar->setValue(vcore);
    // 取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if (is_first_getvram)
    {
        is_first_getvram = false;
        first_vramp = vramp;
        ui->vram_bar->setValue(first_vramp);
    }
    ui->vram_bar->setSecondValue(vramp - first_vramp);

    if (gpu_wait_load)
    {
        gpu_wait_load = false;                       // 以文件体积近似估计显存占用：若模型大小低于当前可用显存的95%，则尝试全量 offload（ngl=999）
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
        // 应用新设置并按需重启本地服务
        if (ui_mode == LOCAL_MODE) ensureLocalServer();
    }
}

// 传递cpu信息
void Widget::recv_cpu_status(double cpuload, double memload)
{
    ui->cpu_bar->setValue(cpuload);
    // 取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
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

// 事件过滤器,鼠标跟踪效果不好要在各种控件单独实现
bool Widget::eventFilter(QObject *obj, QEvent *event)
{
    // 响应已安装控件上的鼠标右击事件
    if (obj == ui->input && event->type() == QEvent::ContextMenu && ui_state == CHAT_STATE)
    {
        QContextMenuEvent *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        // 显示菜单
        right_menu->exec(contextMenuEvent->globalPos());
        return true;
    }
    // 响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->lora_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseLorapath();
        return true;
    }
    // 响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->mmproj_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseMmprojpath();
        return true;
    }
    // 取消通过右击装载按钮进入链接模式的逻辑（改为点击装载后弹模式选择）
    // 响应已安装控件上的鼠标右击事件
    if (obj == api_endpoint_LineEdit && event->type() == QEvent::ContextMenu)
    {
        QString api_endpoint = "http://" + getFirstNonLoopbackIPv4Address() + ":8080";
        api_endpoint_LineEdit->setText(api_endpoint);
        return true;
    }
    // 响应已安装控件上的鼠标右击事件
    if (obj == ui->state && event->type() == QEvent::ContextMenu)
    {
        emit ui2expend_show(PREV_WINDOW); // 1是模型信息页
        return true;
    }

    return QObject::eventFilter(obj, event);
}

// 传递模型预解码的内容
void Widget::recv_predecode(QString bot_predecode_content_)
{
    bot_predecode_content = bot_predecode_content_;
}

// 接收whisper解码后的结果
void Widget::recv_speechdecode_over(QString result)
{
    ui_state_normal();
    ui->input->textEdit->append(result);
    // ui->send->click();//尝试一次发送
}

// 接收模型路径
void Widget::recv_whisper_modelpath(QString modelpath)
{
    whisper_model_path = modelpath;
}

// Ensure local llama.cpp server is running with current settings
void Widget::ensureLocalServer(bool lazyWake)
{
    if (!serverManager) return;

    StartupLogger::log(QStringLiteral("ensureLocalServer start (lazy=%1)").arg(lazyWake));
    QElapsedTimer ensureTimer;
    ensureTimer.start();

    cancelLazyUnload(QStringLiteral("ensureLocalServer entry"));
    if (lazyWake) lazyWakeInFlight_ = true;

    if (!firstAutoNglEvaluated_ && !serverManager->isRunning())
    {
        firstAutoNglEvaluated_ = true;
        QFileInfo fileInfo(ui_SETTINGS.modelpath);
        QFileInfo fileInfo2(ui_SETTINGS.mmprojpath);
        const int modelsize_MB = fileInfo.size() / 1024 / 1024 + fileInfo2.size() / 1024 / 1024;
        if (modelsize_MB > 0 && vfree > 0)
        {
            const double limit = 0.95 * vfree;
            if (modelsize_MB <= limit)
            {
                ui_SETTINGS.ngl = 999;
                if (settings_ui && settings_ui->ngl_slider)
                {
                    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
                    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl));
                }
            }
        }
        else if (modelsize_MB > 0 && vfree <= 0)
        {
            gpu_wait_load = true;
        }
    }

    const QString originalUserPort = ui_port.trimmed();
    ui_port = originalUserPort;
    portConflictDetected_ = false;
    enum class PortFallbackReason
    {
        None,
        Invalid,
        Busy
    };
    PortFallbackReason fallbackReason = PortFallbackReason::None;

    QString frontendHost = QStringLiteral("0.0.0.0");
    QString chosenPort = originalUserPort;
    bool appliedFallbackPort = false;

    if (!portFallbackInFlight_)
    {
        forcedPortOverride_.clear();
    }
    if (!forcedPortOverride_.isEmpty())
    {
        chosenPort = forcedPortOverride_;
        appliedFallbackPort = true;
        fallbackReason = PortFallbackReason::Busy;
    }

    if (chosenPort.isEmpty())
    {
        frontendHost = QStringLiteral("127.0.0.1");
        QString fallback = pickFreeTcpPort();
        if (!fallback.isEmpty()) chosenPort = fallback;
        if (chosenPort.isEmpty()) chosenPort = QString(DEFAULT_SERVER_PORT);
        ui_port.clear();
        lastPortConflictPreferred_.clear();
        lastPortConflictFallback_.clear();
        if (settings_ui && settings_ui->port_lineEdit)
        {
            settings_ui->port_lineEdit->setPlaceholderText("blank = localhost only (random port)");
            settings_ui->port_lineEdit->setToolTip(QString());
        }
        reflash_state(QStringLiteral("ui:port cleared -> bind 127.0.0.1:%1").arg(chosenPort), SIGNAL_SIGNAL);
        appliedFallbackPort = true;
    }
    else
    {
        bool ok = false;
        const quint16 portNum = chosenPort.toUShort(&ok);
        if (!ok || portNum == 0)
        {
            frontendHost = QStringLiteral("127.0.0.1");
            QString fallback = pickFreeTcpPort();
            if (!fallback.isEmpty()) chosenPort = fallback;
            appliedFallbackPort = true;
            fallbackReason = PortFallbackReason::Invalid;
            lastPortConflictPreferred_.clear();
            lastPortConflictFallback_.clear();
            reflash_state(QStringLiteral("ui:invalid port -> bind 127.0.0.1:%1").arg(chosenPort), SIGNAL_SIGNAL);
            if (settings_ui && settings_ui->port_lineEdit)
            {
                settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(chosenPort));
                settings_ui->port_lineEdit->setToolTip(jtr("invalid port fallback body").arg(chosenPort));
            }
        }
    }

    if (appliedFallbackPort)
    {
        ui_port = originalUserPort;
        if (settings_ui && settings_ui->port_lineEdit)
        {
            QSignalBlocker blocker(settings_ui->port_lineEdit);
            settings_ui->port_lineEdit->setText(originalUserPort);
        }
        if (fallbackReason == PortFallbackReason::Invalid && settings_ui && settings_ui->port_lineEdit)
        {
            settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(chosenPort));
            settings_ui->port_lineEdit->setToolTip(jtr("invalid port fallback body").arg(chosenPort));
        }
        portFallbackInFlight_ = false;
    }
    else if (!originalUserPort.isEmpty())
    {
        ui_port = originalUserPort;
        if (settings_ui && settings_ui->port_lineEdit)
        {
            settings_ui->port_lineEdit->setPlaceholderText(QString());
            settings_ui->port_lineEdit->setToolTip(QString());
        }
        lastPortConflictPreferred_.clear();
        lastPortConflictFallback_.clear();
    }

    forcedPortOverride_.clear();
    portFallbackInFlight_ = false;

    QString proxyError;
    if (!ensureProxyListening(frontendHost, chosenPort, &proxyError))
    {
        if (!proxyError.isEmpty())
        {
            reflash_state(QStringLiteral("ui:proxy %1").arg(proxyError), WRONG_SIGNAL);
        }
        if (!portFallbackInFlight_)
        {
            portConflictDetected_ = true;
            initiatePortFallback();
        }
        lazyWakeInFlight_ = false;
        return;
    }

    activeServerHost_ = frontendHost;
    activeServerPort_ = chosenPort;

    QString backendPort = activeBackendPort_;
    const bool backendRunning = serverManager->isRunning();
    if (backendPort.isEmpty() || backendPort == chosenPort || !backendRunning)
    {
        QString candidate;
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            candidate = pickFreeTcpPort(QHostAddress::LocalHost);
            if (!candidate.isEmpty() && candidate != chosenPort)
                break;
        }
        if (candidate.isEmpty() || candidate == chosenPort)
        {
            bool ok = false;
            const quint16 frontNum = chosenPort.toUShort(&ok);
            quint16 alt = ok ? quint16((frontNum + 1) % 65535) : 0;
            if (alt == 0 || alt == frontNum) alt = 9000;
            candidate = QString::number(alt);
        }
        backendPort = candidate;
    }
    activeBackendPort_ = backendPort;
    updateProxyBackend(backendListenHost_, activeBackendPort_);

    serverManager->setSettings(ui_SETTINGS);
    serverManager->setHost(backendListenHost_);
    serverManager->setPort(activeBackendPort_);
    serverManager->setModelPath(ui_SETTINGS.modelpath);
    serverManager->setMmprojPath(ui_SETTINGS.mmprojpath);
    serverManager->setLoraPath(ui_SETTINGS.lorapath);

    lastServerRestart_ = serverManager->needsRestart();
    const bool hadOld = backendRunning;
    ignoreNextServerStopped_ = lastServerRestart_ && hadOld;
    if (lastServerRestart_)
    {
        backendOnline_ = false;
        if (proxyServer_) proxyServer_->setBackendAvailable(false);
        if (!lazyWake)
        {
            preLoad();
            emit ui2net_stop(true);
        }
    }

    serverManager->ensureRunning();
    StartupLogger::log(QStringLiteral("ensureLocalServer ensureRunning done (%1 ms)").arg(ensureTimer.elapsed()));

    backendOnline_ = serverManager->isRunning() && !lastServerRestart_;
    if (proxyServer_) proxyServer_->setBackendAvailable(backendOnline_);
    if (!lazyWake && backendOnline_) markBackendActivity();

    if (!lastServerRestart_ && backendRunning) lazyWakeInFlight_ = false;

    apis.api_endpoint = serverManager->endpointBase();
    apis.api_key = "";
    apis.api_model = "default";
    emit ui2net_apis(apis);
    emit ui2expend_apis(apis);
    emit ui2expend_mode(ui_mode);
    updateLazyCountdownLabel();
}
QString Widget::pickFreeTcpPort(const QHostAddress &addr) const
{
    QTcpServer server;
    if (server.listen(addr, 0))
    {
        const quint16 port = server.serverPort();
        server.close();
        return QString::number(port);
    }
    return QString();
}

void Widget::announcePortBusy(const QString &requestedPort, const QString &alternativePort)
{
    if (alternativePort.isEmpty())
    {
        reflash_state(jtr("ui port busy none").arg(requestedPort), WRONG_SIGNAL);
        return;
    }

    const bool repeated = (lastPortConflictPreferred_ == requestedPort && lastPortConflictFallback_ == alternativePort);
    lastPortConflictPreferred_ = requestedPort;
    lastPortConflictFallback_ = alternativePort;

    const QString stateLine = jtr("ui port busy switched").arg(requestedPort, alternativePort);
    reflash_state(stateLine, WRONG_SIGNAL);

    const QString dialogTitle = jtr("port conflict title");
    const QString dialogText = jtr("port conflict body").arg(requestedPort, alternativePort);

    if (settings_ui && settings_ui->port_lineEdit)
    {
        settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(alternativePort));
        settings_ui->port_lineEdit->setToolTip(dialogText);
    }

    if (!repeated)
    {
        QTimer::singleShot(0, this, [this, dialogTitle, dialogText]()
                           { QMessageBox::warning(this, dialogTitle, dialogText); });
    }
}

void Widget::initiatePortFallback()
{
    if (portFallbackInFlight_) return;

    const QString preferred = ui_port.trimmed();
    if (preferred.isEmpty()) return;

    QString fallback = pickFreeTcpPort();
    if (fallback.isEmpty() || fallback == preferred)
    {
        reflash_state(jtr("ui port busy none").arg(preferred), WRONG_SIGNAL);
        return;
    }

    portConflictDetected_ = false;
    portFallbackInFlight_ = true;
    forcedPortOverride_ = fallback;

    announcePortBusy(preferred, fallback);

    if (settings_ui && settings_ui->port_lineEdit)
    {
        settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(fallback));
        settings_ui->port_lineEdit->setToolTip(jtr("port conflict body").arg(preferred, fallback));
    }

    if (proxyServer_) proxyServer_->stop();
    if (serverManager)
    {
        ignoreNextServerStopped_ = true;
        serverManager->stopAsync();
    }

    QTimer::singleShot(150, this, [this]()
                       { ensureLocalServer(); });
}
bool Widget::ensureProxyListening(const QString &host, const QString &port, QString *errorMessage)
{
    if (!proxyServer_) return true;
    bool ok = false;
    const quint16 value = port.toUShort(&ok);
    if (!ok || value == 0)
    {
        if (errorMessage) *errorMessage = QStringLiteral("invalid proxy port -> %1").arg(port);
        return false;
    }
    if (proxyServer_->isListening() && proxyServer_->listenHost() == host && proxyServer_->listenPort() == value)
    {
        return true;
    }
    if (!proxyServer_->start(host, value, errorMessage))
    {
        return false;
    }
    return true;
}

void Widget::updateProxyBackend(const QString &backendHost, const QString &backendPort)
{
    if (!proxyServer_) return;
    bool ok = false;
    const quint16 value = backendPort.toUShort(&ok);
    proxyServer_->setBackendEndpoint(backendHost, ok ? value : 0);
    proxyServer_->setBackendAvailable(ok && backendOnline_);
}

void Widget::onProxyWakeRequested()
{
    if (!lazyUnloadEnabled()) return;
    if (lazyWakeInFlight_) return;
    if (backendOnline_ && serverManager && serverManager->isRunning()) return;
    reflash_state(QStringLiteral("ui:proxy wake request"), SIGNAL_SIGNAL);
    ensureLocalServer(true);
}

void Widget::onProxyExternalActivity()
{
    markBackendActivity();
    cancelLazyUnload(QStringLiteral("proxy activity"));
}

void Widget::markBackendActivity()
{
    if (!lazyUnloadEnabled()) return;
    if (!idleSince_.isValid())
        idleSince_.start();
    else
        idleSince_.restart();
    updateLazyCountdownLabel();
}

void Widget::scheduleLazyUnload()
{
    if (!lazyUnloadEnabled()) return;
    if (!serverManager || !serverManager->isRunning()) return;
    if (turnActive_ || toolInvocationActive_) return;
    if (!lazyUnloadTimer_) return;
    lazyUnloaded_ = false;
    if (!idleSince_.isValid())
        idleSince_.start();
    else
        idleSince_.restart();
    lazyUnloadTimer_->start(lazyUnloadMs_);
    updateLazyCountdownLabel();
}

void Widget::cancelLazyUnload(const QString &reason)
{
    Q_UNUSED(reason);
    if (!lazyUnloadTimer_) return;
    if (lazyUnloadTimer_->isActive()) lazyUnloadTimer_->stop();
    if (lazyCountdownTimer_ && lazyCountdownTimer_->isActive()) lazyCountdownTimer_->stop();
    lazyUnloaded_ = false;
    idleSince_ = QElapsedTimer();
    updateLazyCountdownLabel();
}

void Widget::performLazyUnload()
{
    performLazyUnloadInternal(false);
}

void Widget::performLazyUnloadInternal(bool forced)
{
    if (!forced && !lazyUnloadEnabled()) return;
    pendingSendAfterWake_ = false;
    if (lazyUnloadTimer_ && lazyUnloadTimer_->isActive()) lazyUnloadTimer_->stop();
    if (lazyCountdownTimer_ && lazyCountdownTimer_->isActive()) lazyCountdownTimer_->stop();
    if (!forced && (turnActive_ || toolInvocationActive_))
    {
        scheduleLazyUnload();
        updateLazyCountdownLabel();
        return;
    }
    if (forced && turnActive_)
    {
        emit ui2net_stop(true);
        turnActive_ = false;
        is_run = false;
    }
    if (forced && toolInvocationActive_)
    {
        emit ui2tool_cancelActive();
        toolInvocationActive_ = false;
    }
    lazyUnloaded_ = true;
    lazyWakeInFlight_ = false;
    backendOnline_ = false;
    if (proxyServer_) proxyServer_->setBackendAvailable(false);
    reflash_state(QStringLiteral("ui:lazy unload -> stop llama-server"), SIGNAL_SIGNAL);
    if (serverManager && serverManager->isRunning())
    {
        serverManager->stopAsync();
    }
    idleSince_ = QElapsedTimer();
    updateLazyCountdownLabel();
}

bool Widget::lazyUnloadEnabled() const
{
    return proxyServer_ && lazyUnloadTimer_ && lazyUnloadMs_ > 0 && ui_mode == LOCAL_MODE;
}

void Widget::updateLazyCountdownLabel()
{
    if (!settings_ui || !settings_ui->lazy_countdown_value_label) return;

    QString text;
    if (lazyUnloadMs_ <= 0)
    {
        text = QStringLiteral("未启用");
    }
    else if (!backendOnline_ || lazyUnloaded_)
    {
        text = QStringLiteral("已卸载");
    }
    else if (!lazyUnloadTimer_ || !lazyUnloadTimer_->isActive())
    {
        text = QStringLiteral("待命");
    }
    else
    {
        int remaining = lazyUnloadTimer_->remainingTime();
        if (remaining < 0) remaining = 0;
        const int totalSeconds = (remaining + 999) / 1000;
        const int hours = totalSeconds / 3600;
        const int minutes = (totalSeconds % 3600) / 60;
        const int seconds = totalSeconds % 60;
        if (hours > 0)
        {
            text = QStringLiteral("%1:%2:%3")
                       .arg(hours, 2, 10, QLatin1Char('0'))
                       .arg(minutes, 2, 10, QLatin1Char('0'))
                       .arg(seconds, 2, 10, QLatin1Char('0'));
        }
        else
        {
            text = QStringLiteral("%1:%2")
                       .arg(minutes, 2, 10, QLatin1Char('0'))
                       .arg(seconds, 2, 10, QLatin1Char('0'));
        }
    }

    settings_ui->lazy_countdown_value_label->setText(text);

    if (lazyCountdownTimer_)
    {
        if (lazyUnloadTimer_ && lazyUnloadTimer_->isActive() && lazyUnloadMs_ > 0 && backendOnline_)
        {
            if (!lazyCountdownTimer_->isActive()) lazyCountdownTimer_->start();
        }
        else
        {
            lazyCountdownTimer_->stop();
        }
    }
}

void Widget::onLazyUnloadNowClicked()
{
    if (ui_mode != LOCAL_MODE)
    {
        reflash_state(QStringLiteral("ui:惰性卸载仅在本地模式启用"), WRONG_SIGNAL);
        return;
    }
    if (!serverManager || !serverManager->isRunning())
    {
        reflash_state(QStringLiteral("ui:本地后端已停止"), SIGNAL_SIGNAL);
        cancelLazyUnload(QStringLiteral("manual unload"));
        return;
    }
    reflash_state(QStringLiteral("ui:立即卸载触发"), SIGNAL_SIGNAL);
    performLazyUnloadInternal(true);
}


void Widget::onServerReady(const QString &endpoint)
{
    backendOnline_ = true;
    lazyUnloaded_ = false;
    lazyWakeInFlight_ = false;
    cancelLazyUnload(QStringLiteral("backend ready"));
    markBackendActivity();
    updateProxyBackend(backendListenHost_, activeBackendPort_);
    if (proxyServer_) proxyServer_->setBackendAvailable(true);

    scheduleLazyUnload();
    updateLazyCountdownLabel();

    if (pendingSendAfterWake_)
    {
        pendingSendAfterWake_ = false;
        QTimer::singleShot(0, this, [this]() { on_send_clicked(); });
    }

    // 配置本地端点；统一由动画收尾逻辑 unlockLoad() 设置标题/图标/状态
    apis.api_endpoint = endpoint;
    apis.api_key = "";
    apis.api_model = "default";
    emit ui2net_apis(apis);
    emit ui2expend_apis(apis);
    emit ui2expend_mode(ui_mode);

    // 完成装载：记录耗时，统一用简单转轮动画（decode_*）收尾，然后解锁 UI
    load_time = load_timer.isValid() ? (load_timer.nsecsElapsed() / 1e9) : 0.0;
    ui_mode = LOCAL_MODE;

    flushPendingStream();
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
    // Track the backend that actually came up and align UI hints/fallback logic.
    const QString resolvedBackend = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server"));
    const QString previousRuntime = runtimeDeviceBackend_;
    if (!resolvedBackend.isEmpty())
    {
        runtimeDeviceBackend_ = resolvedBackend;
    }
    const bool runtimeChanged = (!resolvedBackend.isEmpty() && resolvedBackend != previousRuntime);
    // Sync settings device combobox with actually resolved backend if user chose an explicit device.
    const QString userSel = DeviceManager::userChoice();
    if (!userSel.isEmpty() && userSel != QLatin1String("auto"))
    {
        if (!resolvedBackend.isEmpty() && resolvedBackend != userSel)
        {
            if (settings_ui && settings_ui->device_comboBox)
            {
                int idx = settings_ui->device_comboBox->findText(resolvedBackend);
                if (idx < 0)
                {
                    settings_ui->device_comboBox->addItem(resolvedBackend);
                    idx = settings_ui->device_comboBox->findText(resolvedBackend);
                }
                if (idx >= 0)
                {
                    settings_ui->device_comboBox->setCurrentIndex(idx);
                }
            }
            ui_device_backend = resolvedBackend;
            DeviceManager::setUserChoice(resolvedBackend);
            auto_save_user(); // persist corrected device selection
            reflash_state(QStringLiteral("ui:device fallback -> ") + resolvedBackend, SIGNAL_SIGNAL);
        }
    }
    else if (userSel == QLatin1String("auto") && runtimeChanged)
    {
        reflash_state(QStringLiteral("ui:device resolved -> %1").arg(resolvedBackend), SIGNAL_SIGNAL);
    }
    refreshDeviceBackendUI();
    // Complete load animation and finalize spinner state.
    decode_finish();
    if (!activeServerPort_.isEmpty())
    {
        QString displayHost = activeServerHost_;
        if (displayHost.isEmpty() || displayHost == QStringLiteral("0.0.0.0"))
        {
            displayHost = QStringLiteral("127.0.0.1");
        }
        const QString url = QStringLiteral("http://%1:%2").arg(displayHost, activeServerPort_);
        QString lanHint;
        if (activeServerHost_ == QStringLiteral("0.0.0.0"))
        {
            const QString lanIp = getFirstNonLoopbackIPv4Address();
            if (!lanIp.isEmpty())
            {
                lanHint = QStringLiteral(" / http://%1:%2").arg(lanIp, activeServerPort_);
            }
        }
        reflash_state("ui:" + jtr("local endpoint ready") + QStringLiteral(" -> %1%2").arg(url, lanHint), SUCCESS_SIGNAL);
    }
    // 直接解锁界面（不再补帧播放复杂装载动画）
    unlockLoad();
    // 刚装载完成：若已设置监视帧率，则启动监视
    updateMonitorTimer();
}

// 链接模式的发送处理
// 传递知识库的描述
void Widget::recv_embeddingdb_describe(QString describe)
{
    embeddingdb_describe = describe;
}

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
    audioSettings.setSampleRate(16000);
    audioSettings.setBitRate(128000);
    audioSettings.setChannelCount(1);
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

// 传递格式化后的对话内容
void Widget::recv_chat_format(EVA_CHATS_TEMPLATE chats)
{
    bot_chat = chats;
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
    flushPendingStream();
    is_stop_output_scroll = false;
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
    // Prefer server-reported n_ctx_slot; fallback to UI nctx
    int cap = slotCtxMax_ > 0 ? slotCtxMax_ : (ui_SETTINGS.nctx > 0 ? ui_SETTINGS.nctx : DEFAULT_NCTX);
    if (cap <= 0) cap = DEFAULT_NCTX;

    int used = qMax(0, kvUsed_);
    if (used > cap) used = cap;

    // Convert used/cap to percent in a single (orange) segment
    int percent = cap > 0 ? int(qRound(100.0 * double(used) / double(cap))) : 0;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    // Visual minimum: if any memory, show at least 1%
    if (used > 0 && percent == 0) percent = 1;

    // Force progress range 0..100; use second segment only (orange)
    if (ui->kv_bar->maximum() != 100 || ui->kv_bar->minimum() != 0) ui->kv_bar->setRange(0, 100);
    ui->kv_bar->setValue(0);
    ui->kv_bar->setSecondValue(percent);
    ui->kv_bar->setShowText(QString::fromUtf8("记忆:"));
    ui->kv_bar->setCenterText("");
    ui->kv_bar->setToolTip(QString::fromUtf8("上下文缓存量 %1 / %2 token").arg(used).arg(cap));
}
// Update kv from llama.cpp server timings/stream (usedTokens = prompt_n + streamed chunks)

// Set prompt baseline tokens (LINK mode) from usage in SSE
void Widget::recv_prompt_baseline(int tokens)
{
    if (tokens < 0) return;
    const int promptTokens = qMax(0, tokens);
    const int previousPrompt = kvPromptTokensTurn_;
    const int previousBaseline = kvUsedBeforeTurn_;
    // Always treat provider usage as the absolute prompt baseline for this turn.
    kvPromptTokensTurn_ = promptTokens;
    kvUsedBeforeTurn_ = promptTokens;
    if (!turnActive_) turnActive_ = true;
    kvTokensTurn_ = kvPromptTokensTurn_ + qMax(0, kvStreamedTurn_);
    kvUsed_ = kvPromptTokensTurn_ + qMax(0, kvStreamedTurn_);
    sawPromptPast_ = true;
    // if (ui_mode == LINK_MODE)
    // {
    //     const int deltaPrompt = kvPromptTokensTurn_ - previousPrompt;
    //     const int deltaContext = kvUsedBeforeTurn_ - previousBaseline;
    //     const QString tag = (deltaPrompt == 0) ? QStringLiteral("link:prompt usage (repeat)")
    //                                            : QStringLiteral("link:prompt usage");
    //     reflash_state(QStringLiteral("%1 prompt=%2 delta_prompt=%3 delta_ctx=%4 stream=%5 turn=%6 used=%7 used_before=%8")
    //                       .arg(tag)
    //                       .arg(kvPromptTokensTurn_)
    //                       .arg(deltaPrompt)
    //                       .arg(deltaContext)
    //                       .arg(kvStreamedTurn_)
    //                       .arg(kvTokensTurn_)
    //                       .arg(kvUsed_)
    //                       .arg(kvUsedBeforeTurn_));
    // }
    updateKvBarUi();
}

void Widget::recv_turn_counters(int cacheTokens, int promptTokens, int predictedTokens)
{
    const int cache = qMax(0, cacheTokens);
    const int prompt = qMax(0, promptTokens);
    const int generated = qMax(0, predictedTokens);
    const int total = cache + prompt + generated;
    if (total <= 0) return;
    kvPromptTokensTurn_ = prompt;
    kvStreamedTurn_ = generated;
    kvTokensTurn_ = prompt + generated;
    kvUsedBeforeTurn_ = cache + prompt;
    kvUsed_ = total;
    if (!turnActive_) turnActive_ = true;
    sawPromptPast_ = true;
    // if (ui_mode == LINK_MODE)
    // {
    //     reflash_state(QStringLiteral("link:timings cache=%1 prompt=%2 generated=%3 total=%4")
    //                       .arg(cache)
    //                       .arg(prompt)
    //                       .arg(generated)
    //                       .arg(total));
    // }
    updateKvBarUi();
}

void Widget::recv_kv_from_net(int usedTokens)
{
    const int previousStream = kvStreamedTurn_;
    const int newStream = qMax(0, usedTokens);
    // Approximate KV usage accumulation during streaming tokens from xNet.
    if (ui_mode == LINK_MODE)
    {
        // In LINK mode, we may not have server logs; accept updates regardless of turnActive_
        if (!turnActive_) turnActive_ = true;
        kvStreamedTurn_ = newStream;
        const int delta = kvStreamedTurn_ - previousStream;
        kvTokensTurn_ = kvPromptTokensTurn_ + kvStreamedTurn_;
        kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
        // if (delta != 0)
        // {
        //     reflash_state(QStringLiteral("link:stream update stream=%1 delta=%2 prompt=%3 turn=%4 used=%5")
        //                       .arg(kvStreamedTurn_)
        //                       .arg(delta)
        //                       .arg(kvPromptTokensTurn_)
        //                       .arg(kvTokensTurn_)
        //                       .arg(kvUsed_));
        // }
        updateKvBarUi();
        return;
    }
    // LOCAL mode
    if (!turnActive_) return;
    kvStreamedTurn_ = newStream;
    kvTokensTurn_ = kvPromptTokensTurn_ + kvStreamedTurn_;
    // apply only after server reported prompt baseline (prompt done)
    if (!sawPromptPast_) return;
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
    const QString trimmedLine = line.trimmed();
    if (lazyUnloadEnabled())
    {
        markBackendActivity();
        cancelLazyUnload(QStringLiteral("log activity"));
        const QString lowerLine = trimmedLine.toLower();
        if (lowerLine.contains(QStringLiteral("all slots are idle")) || lowerLine.contains(QStringLiteral("no pending work")) || lowerLine.contains(QStringLiteral("all clients are idle")))
        {
            scheduleLazyUnload();
        }
    }


    // Detect fatal/failed patterns in llama.cpp server logs and unlock UI promptly
    {
        const QString l = line.toLower();
        if (!portFallbackInFlight_ && !ui_port.isEmpty() && activeServerPort_ == ui_port)
        {
            static const QStringList conflictKeys = {
                QStringLiteral("address already in use"),
                QStringLiteral("eaddrinuse"),
                QStringLiteral("failed to bind"),
                QStringLiteral("could not bind"),
                QStringLiteral("bind error"),
                QStringLiteral("listen(): address in use"),
                QStringLiteral("bind(): address")
            };
            for (const QString &key : conflictKeys)
            {
                if (l.contains(key))
                {
                    portConflictDetected_ = true;
                    initiatePortFallback();
                    return;
                }
            }
        }
        static const QStringList badKeys = {
            QStringLiteral(" failed"), QStringLiteral("fatal"), QStringLiteral("segmentation fault"),
            QStringLiteral("assertion failed"), QStringLiteral("error:")};
        bool hit = false;
        for (const QString &k : badKeys)
        {
            if (l.contains(k))
            {
                hit = true;
                break;
            }
        }
        // Filter out benign phrases like "no error" if present
        if (hit && !l.contains("no error"))
        {
            reflash_state(QString::fromUtf8("ui:后端异常输出，已解锁控件"), WRONG_SIGNAL);
            if (decode_pTimer && decode_pTimer->isActive()) decode_fail();
            is_run = false;
            unlockButtonsAfterError();
        }
    }
    // 0) Track turn lifecycle heuristics
    if (line.contains("new prompt") || line.contains("launch_slot_"))
    {
        markBackendActivity();
        cancelLazyUnload(QStringLiteral("turn begin"));
        turnActive_ = true;
        kvUsedBeforeTurn_ = kvUsed_;
        kvTokensTurn_ = 0;
        kvPromptTokensTurn_ = 0;
        kvStreamedTurn_ = 0;
        sawFinalPast_ = false;
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
    for (auto it = reCtxSlot.globalMatch(line); it.hasNext();)
    {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int v = m.captured(1).toInt(&ok);
        if (ok && v > 0)
        {
            slotCtxMax_ = v;
            updateKvBarUi();
            // Notify Expend (evaluation tab) to refresh displayed n_ctx in LINK mode
            SETTINGS snap = ui_SETTINGS;
            if (ui_mode == LINK_MODE && slotCtxMax_ > 0) snap.nctx = slotCtxMax_;
            emit ui2expend_settings(snap);
        }
    }

    // 1.3) kv cache rm [hit, end) -> use left number to correct current memory right away
    static QRegularExpression reKvRm("kv cache rm\\s*\\[\\s*(\\d+)");
    QRegularExpressionMatch mRm = reKvRm.match(line);
    if (mRm.hasMatch())
    {
        bool ok = false;
        int hit = mRm.captured(1).toInt(&ok);
        if (ok)
        {
            kvUsed_ = qMax(0, hit);
            updateKvBarUi();
        }
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
            const bool shouldAdoptMax = (maxngl > 0 && (ui_SETTINGS.ngl == 999 || ui_SETTINGS.ngl > maxngl));
            if (ui_maxngl != maxngl)
            {
                ui_maxngl = maxngl;
                if (settings_ui && settings_ui->ngl_slider)
                {
                    const int curMax = settings_ui->ngl_slider->maximum();
                    if (curMax != maxngl) settings_ui->ngl_slider->setMaximum(maxngl);
                }
            }
            if (shouldAdoptMax)
            {
                ui_SETTINGS.ngl = maxngl;
            }
            if (settings_ui && settings_ui->ngl_slider)
            {
                int curVal = settings_ui->ngl_slider->value();
                if (shouldAdoptMax)
                {
                    curVal = ui_SETTINGS.ngl;
                }
                else if (maxngl > 0 && curVal > maxngl)
                {
                    curVal = maxngl;
                }
                if (curVal != settings_ui->ngl_slider->value()) settings_ui->ngl_slider->setValue(curVal);
                settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(curVal));
                // reflash_state("ui:max ngl = " + QString::number(maxngl), SIGNAL_SIGNAL);
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
    // 3) total tokens (for KV correction)
    static QRegularExpression reTotal("total\\s+time\\s*=\\s*([0-9.]+)\\s*ms\\s*/\\s*(\\d+)\\s*tokens");
    QRegularExpressionMatch m3 = reTotal.match(line);
    if (m3.hasMatch())
    {
        const int totalTokens = m3.captured(2).toInt();
        // Use total tokens to correct current slot KV usage for this turn
        // 本地模式：提示完成后（sawPromptPast_）才基于基线增量更新；避免回跳到上一轮
        kvStreamedTurn_ = totalTokens;
        if (ui_mode == LINK_MODE)
        {
            kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
            updateKvBarUi();
        }
        else if (sawPromptPast_ && !sawFinalPast_)
        {
            kvUsed_ = qMax(0, kvUsedBeforeTurn_ + kvStreamedTurn_);
            updateKvBarUi();
        }
    }

    // 4) prompt done /stop processing -> correct kvUsed_ from n_past immediately
    static QRegularExpression rePromptDone("prompt\\s+done,\\s*n_past\\s*=\\s*(\\d+)");
    static QRegularExpression reStop("stop\\s+processing.*n_past\\s*=\\s*(\\d+)");
    QRegularExpressionMatch mPD = rePromptDone.match(line);
    if (mPD.hasMatch())
    {
        bool ok = false;
        int past = mPD.captured(1).toInt(&ok);
        if (ok)
        {
            kvUsed_ = qMax(0, past);
            kvUsedBeforeTurn_ = qMax(0, past);
            kvStreamedTurn_ = 0;
            sawPromptPast_ = true;
            updateKvBarUi();
            markBackendActivity();
            cancelLazyUnload(QStringLiteral("prompt done"));
        }
    }
    QRegularExpressionMatch mStop = reStop.match(line);
    if (mStop.hasMatch())
    {
        bool ok = false;
        int past = mStop.captured(1).toInt(&ok);
        if (ok)
        {
            kvUsed_ = qMax(0, past);
            sawFinalPast_ = true;
            updateKvBarUi();
            markBackendActivity();
            scheduleLazyUnload();
        }
    }
    // qDebug()<< "Server log:" << line;
}

// 后端启动失败：停止装载动画并解锁按钮，便于用户更换模型或后端
void Widget::onServerStartFailed(const QString &reason)
{
    backendOnline_ = false;
    lazyWakeInFlight_ = false;
    if (proxyServer_) proxyServer_->setBackendAvailable(false);
    cancelLazyUnload(QStringLiteral("backend start failed"));
    pendingSendAfterWake_ = false;

    Q_UNUSED(reason);
    if (!portFallbackInFlight_ && portConflictDetected_ && !ui_port.isEmpty() && activeServerPort_ == ui_port)
    {
        initiatePortFallback();
        return;
    }
    portConflictDetected_ = false;
    const QString selectedBackend = DeviceManager::userChoice();
    const QString resolvedBackend = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server"));
    const QString attemptedBackend = resolvedBackend.isEmpty() ? selectedBackend : resolvedBackend;
    if (!attemptedBackend.isEmpty())
    {
        QString statusLine;
        if (!selectedBackend.isEmpty() && selectedBackend != attemptedBackend)
        {
            statusLine = QStringLiteral("ui:backend start failure (%1 -> %2)").arg(selectedBackend, attemptedBackend);
        }
        else
        {
            statusLine = QStringLiteral("ui:backend start failure -> %1").arg(attemptedBackend);
        }
        reflash_state(statusLine, WRONG_SIGNAL);
    }
    refreshDeviceBackendUI();
    // 停止任何进行中的动画/计时
    if (decode_pTimer) decode_pTimer->stop();
    // 用失败标志收尾“装载中”转轮行
    decode_fail();

    // 清理装载状态，避免后续 serverStopped 被忽略
    lastServerRestart_ = false;
    ignoreNextServerStopped_ = true; // 紧随其后的 serverStopped 属于同一次失败，忽略之
    is_load = false;

    // 解锁界面，允许用户立即调整设置或重新装载
    is_run = false;
    unlockButtonsAfterError();
    // 明确允许打开“设置/约定”以便更换后端/设备/提示词
    if (ui && ui->set) ui->set->setEnabled(true);
    if (ui && ui->date) ui->date->setEnabled(true);
}

// Finalize UI after a successful model load
void Widget::unlockLoad()
{
    if (ui_SETTINGS.ngl < ui_maxngl)
    {
        reflash_state("ui:" + jtr("ngl tips"), USUAL_SIGNAL);
    }

    reflash_state("ui:" + jtr("load model") + jtr("over") + " " + QString::number(load_time, 'f', 2) + " s " + jtr("right click and check model log"), SUCCESS_SIGNAL);
    if (ui_SETTINGS.ngl > 0)
    {
        EVA_icon = QIcon(":/logo/green_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon);
    }
    else
    {
        EVA_icon = QIcon(":/logo/blue_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon);
    }
    EVA_title = jtr("current model") + " " + ui_SETTINGS.modelpath.split("/").last();
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(max_thread));
    auto_save_user();
    ui_state_normal();
    // Record a system header and show system prompt as pre-decode content
    int __idx = recordCreate(RecordRole::System);
    appendRoleHeader(QStringLiteral("system"));
    reflash_output(bot_predecode_content, 0, themeTextPrimary());
    recordAppendText(__idx, bot_predecode_content);
}


