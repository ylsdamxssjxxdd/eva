#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//--------------------------------设置选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::set_SetDialog()
{
    settings_dialog = new QDialog(this);
    settings_dialog->setWindowFlags(settings_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint); //隐藏?按钮
    // settings_dialog->setWindowFlags(settings_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);// 隐藏关闭按钮
    settings_ui = new Ui::Settings_Dialog_Ui;
    settings_ui->setupUi(settings_dialog);

    // 推理设备下拉：根据当前目录中可用后端动态填充
    {
        settings_ui->device_comboBox->clear();
        settings_ui->device_comboBox->addItem("auto");
        const QStringList backs = DeviceManager::availableBackends();
        for (const QString &b : backs)
        {
            settings_ui->device_comboBox->addItem(b);
        }
        // 初始值沿用进程内选择（默认 auto），避免在首次打开前就触发重启
        int idx = settings_ui->device_comboBox->findText(DeviceManager::userChoice());
        if (idx >= 0) settings_ui->device_comboBox->setCurrentIndex(idx);

        // helper：刷新设备相关 UI（auto 时在“推理设备”文本后追加提示；CPU 禁用 gpu 层数）
        auto refreshDeviceUI = [this]() {
            const QString sel = settings_ui->device_comboBox->currentText().trimmed().toLower();
            const bool isCpu = (sel == QLatin1String("cpu")||sel == QLatin1String("opencl"));
            const bool isAuto = (sel == QLatin1String("auto"));

            // 当选择 CPU 时，禁止选择 gpu 负载层数
            settings_ui->ngl_slider->setEnabled(!isCpu);

            // 在“推理设备”标签文本后附加当前 auto 的实际后端
            if (deviceLabelBaseText.isEmpty())
            {
                deviceLabelBaseText = settings_ui->device_label->text();
            }
            if (isAuto)
            {
                // 计算 auto 实际生效的后端（按 cuda>vulkan>opencl>cpu 顺序）
                const QString eff = DeviceManager::effectiveBackend();
                settings_ui->device_label->setText(deviceLabelBaseText + QString(" (%1)").arg(eff));
            }
            else
            {
                settings_ui->device_label->setText(deviceLabelBaseText);
            }
        };

        // 监听选择变化并初始化一次
        connect(settings_ui->device_comboBox, &QComboBox::currentTextChanged, this, [=](const QString &) {
            refreshDeviceUI();
        });
        refreshDeviceUI();
    }

    //温度控制
    settings_ui->temp_slider->setRange(0, 100); // 设置范围为1到99
    settings_ui->temp_slider->setValue(ui_SETTINGS.temp * 100.0);
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0));
    connect(settings_ui->temp_slider, &QSlider::valueChanged, this, &Widget::temp_change);
    //重复惩罚控制
    settings_ui->repeat_slider->setRange(0, 200); // 设置范围
    settings_ui->repeat_slider->setValue(ui_SETTINGS.repeat * 100.0);
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0));
    connect(settings_ui->repeat_slider, &QSlider::valueChanged, this, &Widget::repeat_change);
    // TOP_K 控制（采样）
    settings_ui->topk_slider->setRange(0, 200);
    settings_ui->topk_slider->setValue(ui_SETTINGS.top_k);
    settings_ui->topk_label->setText(jtr("top_k") + " " + QString::number(settings_ui->topk_slider->value()));
    connect(settings_ui->topk_slider, &QSlider::valueChanged, this, &Widget::topk_change);
    // TOP_P 控制（采样）
    settings_ui->topp_slider->setRange(0, 100);
    settings_ui->topp_slider->setValue(ui_SETTINGS.hid_top_p * 100.0);
    settings_ui->topp_label->setText("TOP_P " + QString::number(settings_ui->topp_slider->value() / 100.0));
    // 提示：核采样阈值（nucleus sampling）范围 0.00–1.00
    settings_ui->topp_label->setToolTip(QString::fromUtf8("核采样阈值（top_p），范围 0.00–1.00；当前：%1")
                                            .arg(QString::number(settings_ui->topp_slider->value() / 100.0, 'f', 2)));
    connect(settings_ui->topp_slider, &QSlider::valueChanged, this, &Widget::topp_change);
    //加速支持
    settings_ui->ngl_slider->setRange(0, 99);
    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(settings_ui->ngl_slider->value()));
    connect(settings_ui->ngl_slider, &QSlider::valueChanged, this, &Widget::ngl_change);
    // cpu线程数设置
    settings_ui->nthread_slider->setValue(ui_SETTINGS.nthread);
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(settings_ui->nthread_slider->value()));
    connect(settings_ui->nthread_slider, &QSlider::valueChanged, this, &Widget::nthread_change);
    // ctx length 记忆容量
    settings_ui->nctx_slider->setRange(128, 32768);
    settings_ui->nctx_slider->setValue(ui_SETTINGS.nctx);
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(settings_ui->nctx_slider->value()));
    connect(settings_ui->nctx_slider, &QSlider::valueChanged, this, &Widget::nctx_change);
    // 并发数量（llama-server --parallel）
    settings_ui->parallel_slider->setValue(ui_SETTINGS.hid_parallel);
    settings_ui->parallel_label->setText(jtr("parallel") + " " + QString::number(settings_ui->parallel_slider->value()));
    connect(settings_ui->parallel_slider, &QSlider::valueChanged, this, &Widget::parallel_change);
    // load lora
    settings_ui->lora_LineEdit->setContextMenuPolicy(Qt::NoContextMenu); //取消右键菜单
    settings_ui->lora_LineEdit->installEventFilter(this);
    // load mmproj
    settings_ui->mmproj_LineEdit->setContextMenuPolicy(Qt::NoContextMenu); //取消右键菜单
    settings_ui->mmproj_LineEdit->installEventFilter(this);
    //补完控制
    connect(settings_ui->complete_btn, &QRadioButton::clicked, this, &Widget::complete_change);
    //多轮对话
    settings_ui->chat_btn->setChecked(1);
    connect(settings_ui->chat_btn, &QRadioButton::clicked, this, &Widget::chat_change);
    //网页服务控制（服务状态已移除，仅保留端口）
    QHBoxLayout *layout_H10 = new QHBoxLayout(); //水平布局器
    settings_ui->port_lineEdit->setText(ui_port);
    QIntValidator *validator = new QIntValidator(0, 65535); //限制端口输入
    settings_ui->port_lineEdit->setValidator(validator);
    settings_ui->port_lineEdit->setPlaceholderText("blank = localhost only (random port)");
    // web_btn 已从 UI 移除
    //监视帧率设置
    settings_ui->frame_lineEdit->setValidator(new QDoubleValidator(0.0, 1000.0, 8, this)); // 只允许输入数字

    connect(settings_ui->confirm, &QPushButton::clicked, this, &Widget::settings_ui_confirm_button_clicked);
    connect(settings_ui->cancel, &QPushButton::clicked, this, &Widget::settings_ui_cancel_button_clicked);

    settings_dialog->setWindowTitle(jtr("set"));

}

// 设置选项卡确认按钮响应
void Widget::settings_ui_confirm_button_clicked()
{
    // 先读取对话框中的值，与打开对话框时的快照对比；完全一致则不做任何动作
    // 注意：get_set() 会把控件值写入 ui_SETTINGS 与 ui_port
    get_set();

    auto eq_str = [](const QString &a, const QString &b) { return a == b; };
    auto eq_ngl = [&](int a, int b) {
        // 视 999 与 (n_layer+1) 为等价（已知服务端最大层数时）
        if (a == b) return true;
        if (ui_maxngl > 0) {
            if ((a == 999 && b == ui_maxngl) || (b == 999 && a == ui_maxngl)) return true;
        }
        return false;
    };
    auto eq = [&](const SETTINGS &A, const SETTINGS &B) {
        // 影响 llama-server 启动参数的设置项（保持与 LocalServerManager::buildArgs 一致）
        if (A.modelpath != B.modelpath) return false;
        if (A.mmprojpath != B.mmprojpath) return false;
        if (A.lorapath != B.lorapath) return false;
        if (A.nctx != B.nctx) return false;
        if (!eq_ngl(A.ngl, B.ngl)) return false;
        if (A.nthread != B.nthread) return false;
        if (A.hid_batch != B.hid_batch) return false;
        if (A.hid_parallel != B.hid_parallel) return false;
        if (A.hid_use_mmap != B.hid_use_mmap) return false;
        if (A.hid_use_mlock != B.hid_use_mlock) return false;
        if (A.hid_flash_attn != B.hid_flash_attn) return false;
        // 推理设备（切换后需要重启后端）
        if (ui_device_backend != device_snapshot_) return false;
        // 其他仅影响采样/推理流程的设置项（不触发后端重启）也一并比较；若都未变，则完全不处理
        if (A.temp != B.temp) return false;
        if (A.repeat != B.repeat) return false;
        if (A.top_k != B.top_k) return false;
        if (A.hid_top_p != B.hid_top_p) return false;
        if (A.hid_npredict != B.hid_npredict) return false;
        if (A.hid_n_ubatch != B.hid_n_ubatch) return false;
        if (A.hid_special != B.hid_special) return false;
        if (A.complete_mode != B.complete_mode) return false;
        return true;
    };

    const bool sameSettings = eq(ui_SETTINGS, settings_snapshot_) && eq_str(ui_port, port_snapshot_);

    // 仅比较会触发后端重启的设置项
    auto eq_server = [&](const SETTINGS &A, const SETTINGS &B) {
        if (A.modelpath != B.modelpath) return false;
        if (A.mmprojpath != B.mmprojpath) return false;
        if (A.lorapath != B.lorapath) return false;
        if (A.nctx != B.nctx) return false;
        if (!eq_ngl(A.ngl, B.ngl)) return false;
        if (A.nthread != B.nthread) return false;
        if (A.hid_batch != B.hid_batch) return false;
        if (A.hid_parallel != B.hid_parallel) return false;
        if (A.hid_use_mmap != B.hid_use_mmap) return false;
        if (A.hid_use_mlock != B.hid_use_mlock) return false;
        if (A.hid_flash_attn != B.hid_flash_attn) return false;
        // 设备切换也需要重启后端
        if (ui_device_backend != device_snapshot_) return false;
        return true;
    };
    const bool sameServer = eq_server(ui_SETTINGS, settings_snapshot_) && eq_str(ui_port, port_snapshot_);

    settings_dialog->close();
    auto_save_user(); // persist settings to eva_config.ini
    // 监视帧率无需重启后端；实时应用
    updateMonitorTimer();
    if (sameSettings)
    {
        // 未发生任何变化：不重启、不重置
        return;
    }
    if (sameServer)
    {
        // 仅采样/推理相关变化：不重启后端，仅重置上下文应用参数
        if (ui_mode == LOCAL_MODE || ui_mode == LINK_MODE)
        {
            on_reset_clicked();
        }
        return;
    }
    // 有影响后端的变化：必要时重启后端并在未重启的情况下重置
    if (ui_mode == LOCAL_MODE)
    {
        ensureLocalServer();
        if (!lastServerRestart_)
        {
            on_reset_clicked();
        }
    }
    else
    {
        on_reset_clicked();
    }
}

// 设置选项卡取消按钮响应
void Widget::settings_ui_cancel_button_clicked()
{
    settings_dialog->close();
    if (!is_load) //如果没有装载模型则装载
    {
        set_set();
    }
}

// 设置用户设置内容
void Widget::set_set()
{
    EVA_STATE current_ui_state = ui_state; //上一次机体的状态
    get_set();                             //获取设置中的纸面值

    //如果不是对话模式则禁用约定
    if (ui_state != CHAT_STATE)
    {
        date_ui->prompt_box->setEnabled(0);
        date_ui->tool_box->setEnabled(0);
    }
    else
    {
        date_ui->prompt_box->setEnabled(1);
        date_ui->tool_box->setEnabled(1);
    }

    // 本地模式：按需重启 llama-server（内部会切至装载中并更新端点）；
    // 若无需重启（仅采样参数变化），则简单重置对话上下文。
    if (ui_mode == LOCAL_MODE)
    {
        ensureLocalServer();
        if (!lastServerRestart_)
        {
            on_reset_clicked();
        }
        // 若发生了重启，等待 onServerReady() 回调恢复 UI 和上下文
    }
    else if (ui_mode == LINK_MODE)
    {
        on_reset_clicked();
    }
}


