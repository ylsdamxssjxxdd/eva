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

    //温度控制
    settings_ui->temp_slider->setRange(0, 100); // 设置范围为1到99
    settings_ui->temp_slider->setValue(ui_SETTINGS.temp * 100.0);
    connect(settings_ui->temp_slider, &QSlider::valueChanged, this, &Widget::temp_change);
    //重复惩罚控制
    settings_ui->repeat_slider->setRange(0, 200); // 设置范围
    settings_ui->repeat_slider->setValue(ui_SETTINGS.repeat * 100.0);
    connect(settings_ui->repeat_slider, &QSlider::valueChanged, this, &Widget::repeat_change);
    //加速支持
    settings_ui->ngl_slider->setRange(0, 99);
    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
    connect(settings_ui->ngl_slider, &QSlider::valueChanged, this, &Widget::ngl_change);
    // cpu线程数设置
    settings_ui->nthread_slider->setValue(ui_SETTINGS.nthread);
    connect(settings_ui->nthread_slider, &QSlider::valueChanged, this, &Widget::nthread_change);
    // ctx length 记忆容量
    settings_ui->nctx_slider->setRange(128, 32768);
    settings_ui->nctx_slider->setValue(ui_SETTINGS.nctx);
    connect(settings_ui->nctx_slider, &QSlider::valueChanged, this, &Widget::nctx_change);
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
    //网页服务控制
    QHBoxLayout *layout_H10 = new QHBoxLayout(); //水平布局器
    settings_ui->port_lineEdit->setText(ui_port);
    QIntValidator *validator = new QIntValidator(0, 65535); //限制端口输入
    settings_ui->port_lineEdit->setValidator(validator);
    connect(settings_ui->web_btn, &QRadioButton::clicked, this, &Widget::web_change);
    //监视帧率设置
    settings_ui->frame_lineEdit->setValidator(new QDoubleValidator(0.0, 1000.0, 8, this)); // 只允许输入数字

    connect(settings_ui->confirm, &QPushButton::clicked, this, &Widget::settings_ui_confirm_button_clicked);
    connect(settings_ui->cancel, &QPushButton::clicked, this, &Widget::settings_ui_cancel_button_clicked);

    settings_dialog->setWindowTitle(jtr("set"));
}

// 设置选项卡确认按钮响应
void Widget::settings_ui_confirm_button_clicked()
{
    settings_dialog->close();
    set_set();
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

    //从补完模式回来强行预解码
    if (current_ui_state == COMPLETE_STATE && ui_state == CHAT_STATE)
    {
        emit ui2bot_preDecode();
    }

    //发送设置参数给模型
    if (ui_state != SERVER_STATE && ui_mode != LINK_MODE) { emit ui2bot_set(ui_SETTINGS, 1); }

    // llama-server接管,不需要告知bot约定
    if (ui_state == SERVER_STATE)
    {
        serverControl();
    }
    else
    {
        if (ui_mode == LINK_MODE) //链接模式不发信号
        {
            on_reset_clicked();
        }
    }
}
