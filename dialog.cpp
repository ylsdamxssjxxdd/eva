//设置界面控件和槽函数

#include "widget.h"
#include "ui_widget.h"
// 初始化动画相关
void Widget::init_move()
{

    movie_line << "                        ##                        ";//2
    movie_line << "                      #    #                      ";//3
    movie_line << "                        ##                        ";//4
    movie_line << "     ##                                    ##     ";//5
    movie_line << "   #    #                                #    #   ";//6
    movie_line << "     ##                                    ##     ";//7
    movie_line << "                        ##                        ";//8
    movie_line << "                      #    #                      ";//9
    movie_line << "                        ##                        ";//10
    movie_line << "     ##                                    ##     ";//11
    movie_line << "   #    #                                #    #   ";//12
    movie_line << "     ##                                    ##     ";//13
    movie_line << "                        ##                        ";//14
    movie_line << "                      #    #                      ";//15
    movie_line << "                        ##                        ";//16

    movie_dot << QPointF(5,12) << QPointF(4,15) << QPointF(3,19) << QPointF(2,22);//load_action 1
    movie_dot << QPointF(5,12) << QPointF(6,15) << QPointF(7,19) << QPointF(8,22);//load_action 2
    movie_dot << QPointF(5,12) << QPointF(6,13) << QPointF(7,14) << QPointF(8,15)  << QPointF(9,16) << QPointF(10,17) << QPointF(11,18) << QPointF(12,19) << QPointF(13,20) << QPointF(14,22);//load_action 3

    movie_dot << QPointF(11,12) << QPointF(10,13) << QPointF(9,14) << QPointF(8,15) << QPointF(7,16) << QPointF(6,17) << QPointF(5,18) << QPointF(4,19) << QPointF(3,20) << QPointF(2,22);//load_action 4
    movie_dot << QPointF(11,12) << QPointF(10,15) << QPointF(9,19) << QPointF(8,22);//load_action 5
    movie_dot << QPointF(11,12) << QPointF(12,15) << QPointF(13,19) << QPointF(14,22);//load_action 6

    movie_dot << QPointF(2,31) << QPointF(3,34) << QPointF(4,38) << QPointF(5,41);//load_action 7
    movie_dot << QPointF(2,31) << QPointF(3,33) << QPointF(4,34) << QPointF(5,35) << QPointF(6,36) << QPointF(7,37) << QPointF(8,38) << QPointF(9,39) << QPointF(10,40) << QPointF(11,41);//load_action 8

    movie_dot << QPointF(8,31) << QPointF(7,34) << QPointF(6,38) << QPointF(5,41);//load_action 9
    movie_dot << QPointF(8,31) << QPointF(9,34) << QPointF(10,38) << QPointF(11,41);//load_action 10

    movie_dot << QPointF(14,31) << QPointF(13,33) << QPointF(12,34) << QPointF(11,35) << QPointF(10,36) << QPointF(9,37) << QPointF(8,38) << QPointF(7,39) << QPointF(6,40) << QPointF(5,41);//load_action 11
    movie_dot << QPointF(14,31) << QPointF(13,34) << QPointF(12,38) << QPointF(11,41);//load_action 12

    //添加颜色
    for(int i=0;i<12;++i)
    {
        //彩色
        //movie_color <<QColor(QRandomGenerator::global()->bounded(256), QRandomGenerator::global()->bounded(256), QRandomGenerator::global()->bounded(256));
        //黑色
        movie_color <<QColor(0, 0, 0);
    }


    load_pTimer = new QTimer(this);//启动后,达到规定时间将发射终止信号
    load_begin_pTimer = new QTimer(this);//启动后,达到规定时间将发射终止信号
    load_over_pTimer = new QTimer(this);//启动后,达到规定时间将发射终止信号
    encode_pTimer = new QTimer(this);//启动后,达到规定时间将发射终止信号

    connect(load_pTimer, SIGNAL(timeout()), this, SLOT(load_handleTimeout()));//设置终止信号触发的槽函数
    connect(load_over_pTimer, SIGNAL(timeout()), this, SLOT(load_over_handleTimeout()));//设置终止信号触发的槽函数
    connect(load_begin_pTimer, SIGNAL(timeout()), this, SLOT(load_begin_handleTimeout()));//设置终止信号触发的槽函数
    connect(this, &Widget::load_play_over, this, &Widget::load_play_finish);
    connect(encode_pTimer, SIGNAL(timeout()), this, SLOT(encode_handleTimeout()));//设置终止信号触发的槽函数
}

// 设置设置选项
void Widget::set_SetDialog()
{
    set_dialog = new QDialog;
    set_dialog->setWindowFlags(set_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);//隐藏?按钮
    set_dialog->resize(150, 200); // 设置宽度,高度

    QVBoxLayout *layout = new QVBoxLayout(set_dialog);//总垂直布局器

    //------------采样设置---------------
    sample_box = new QGroupBox(wordsObj["sample set"].toString());//采样设置区域
    QVBoxLayout *samlpe_layout = new QVBoxLayout();//采样设置垂直布局器

    //温度控制
    QHBoxLayout *layout_H1 = new QHBoxLayout();//水平布局器
    temp_label = new QLabel(wordsObj["temperature"].toString()+" " + QString::number(ui_SETTINGS.temp));
    temp_label->setToolTip(wordsObj["The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"].toString());
    temp_label->setMinimumWidth(100);
    layout_H1->addWidget(temp_label);
    temp_slider = new QSlider(Qt::Horizontal);
    temp_slider->setRange(0, 99); // 设置范围为0到99
    temp_slider->setMinimumWidth(150);
    temp_slider->setValue(ui_SETTINGS.temp*100.0);
    temp_slider->setToolTip(wordsObj["The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"].toString());
    connect(temp_slider, &QSlider::valueChanged, this, &Widget::temp_change);
    layout_H1->addWidget(temp_slider);
    samlpe_layout->addLayout(layout_H1);
    
    //重复惩罚控制
    QHBoxLayout *layout_H4 = new QHBoxLayout();//水平布局器
    repeat_label = new QLabel(wordsObj["repeat"].toString() + " " + QString::number(ui_SETTINGS.repeat));
    repeat_label->setToolTip(wordsObj["Reduce the probability of the model outputting synonymous words"].toString());
    repeat_label->setMinimumWidth(100);
    layout_H4->addWidget(repeat_label);
    repeat_slider = new QSlider(Qt::Horizontal);
    repeat_slider->setRange(0, 200); // 设置范围
    repeat_slider->setValue(ui_SETTINGS.repeat*100.0);
    repeat_slider->setToolTip(wordsObj["Reduce the probability of the model outputting synonymous words"].toString());
    connect(repeat_slider, &QSlider::valueChanged, this, &Widget::repeat_change);
    layout_H4->addWidget(repeat_slider);
    samlpe_layout->addLayout(layout_H4);

    //最大输出长度设置
    QHBoxLayout *layout_H15 = new QHBoxLayout();//水平布局器
    npredict_label = new QLabel(wordsObj["npredict"].toString() + " " + QString::number(ui_SETTINGS.npredict));
    npredict_label->setToolTip(wordsObj["one predict model can output max token number"].toString());npredict_label->setMinimumWidth(100);
    layout_H15->addWidget(npredict_label);
    npredict_slider = new QSlider(Qt::Horizontal);
    npredict_slider->setRange(1, 8192); // 设置范围
    npredict_slider->setValue(ui_SETTINGS.npredict);
    npredict_slider->setToolTip(wordsObj["one predict model can output max token number"].toString());
    connect(npredict_slider, &QSlider::valueChanged, this, &Widget::npredict_change);
    layout_H15->addWidget(npredict_slider);
    samlpe_layout->addLayout(layout_H15);

    sample_box->setLayout(samlpe_layout);
    layout->addWidget(sample_box);

    //------------解码设置---------------
    decode_box = new QGroupBox(wordsObj["decode set"].toString());//解码设置区域
    QVBoxLayout *decode_layout = new QVBoxLayout();//解码设置垂直布局器

#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    //加速支持
    QHBoxLayout *layout_H2 = new QHBoxLayout();//水平布局器
    ngl_label = new QLabel("gpu " + wordsObj["offload"].toString() + QString::number(ui_SETTINGS.ngl));
    ngl_label->setToolTip(wordsObj["put some model paragram to gpu and reload model"].toString());ngl_label->setMinimumWidth(100);
    layout_H2->addWidget(ngl_label);
    ngl_slider = new QSlider(Qt::Horizontal);
    ngl_slider->setRange(0,99);
    ngl_slider->setValue(ui_SETTINGS.ngl);
    ngl_slider->setToolTip(wordsObj["put some model paragram to gpu and reload model"].toString());
    layout_H2->addWidget(ngl_slider);
    decode_layout->addLayout(layout_H2);//将布局添加到总布局
    connect(ngl_slider, &QSlider::valueChanged, this, &Widget::ngl_change);
#endif
    //cpu线程数设置
    QHBoxLayout *layout_H16 = new QHBoxLayout();//水平布局器
    nthread_label = new QLabel("cpu " + wordsObj["thread"].toString() + " " + QString::number(ui_nthread));
    nthread_label->setToolTip(wordsObj["not big better"].toString());
    nthread_label->setMinimumWidth(100);
    layout_H16->addWidget(nthread_label);
    nthread_slider = new QSlider(Qt::Horizontal);
    nthread_slider->setToolTip(wordsObj["not big better"].toString());
    nthread_slider->setRange(1,max_thread);
    nthread_slider->setValue(ui_nthread);
    layout_H16->addWidget(nthread_slider);
    decode_layout->addLayout(layout_H16);//将布局添加到总布局
    connect(nthread_slider, &QSlider::valueChanged, this, &Widget::nthread_change);
    //ctx length
    QHBoxLayout *layout_H3 = new QHBoxLayout();//水平布局器
    nctx_label = new QLabel(wordsObj["ctx"].toString()+wordsObj["length"].toString()+" " + QString::number(ui_SETTINGS.nctx));
    nctx_label->setToolTip(wordsObj["memory capacity"].toString() + "," + wordsObj["not big better"].toString());nctx_label->setMinimumWidth(100);
    layout_H3->addWidget(nctx_label);
    nctx_slider = new QSlider(Qt::Horizontal);
    nctx_slider->setRange(128,32768);
    nctx_slider->setValue(ui_SETTINGS.nctx);
    nctx_slider->setToolTip(wordsObj["memory capacityl"].toString() + "," + wordsObj["not big better"].toString());
    layout_H3->addWidget(nctx_slider);
    decode_layout->addLayout(layout_H3);//将布局添加到总布局
    connect(nctx_slider, &QSlider::valueChanged, this, &Widget::nctx_change);
    //batch size
    QHBoxLayout *layout_H13 = new QHBoxLayout();//水平布局器
    batch_label = new QLabel(wordsObj["batch size"].toString() + " " + QString::number(ui_SETTINGS.batch));
    batch_label->setToolTip(wordsObj["batch size mean"].toString());batch_label->setMinimumWidth(100);
    layout_H13->addWidget(batch_label);
    batch_slider = new QSlider(Qt::Horizontal);
    batch_slider->setRange(1,2048);
    batch_slider->setValue(ui_SETTINGS.batch);
    batch_slider->setToolTip(wordsObj["batch size mean"].toString());
    layout_H13->addWidget(batch_slider);
    decode_layout->addLayout(layout_H13);//将布局添加到总布局
    connect(batch_slider, &QSlider::valueChanged, this, &Widget::batch_change);

    //load lora
    QHBoxLayout *layout_H12 = new QHBoxLayout();//水平布局器
    lora_label = new QLabel(wordsObj["load lora"].toString());
    lora_LineEdit = new QLineEdit();
    lora_LineEdit->setPlaceholderText(wordsObj["right click and choose lora"].toString());
    layout_H12->addWidget(lora_label);
    layout_H12->addWidget(lora_LineEdit);
    decode_layout->addLayout(layout_H12);//将布局添加到总布局
    lora_LineEdit->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    lora_LineEdit->installEventFilter(this);

    //load mmproj
    QHBoxLayout *layout_H17 = new QHBoxLayout();//水平布局器
    mmproj_label = new QLabel(wordsObj["load mmproj"].toString());
    mmproj_LineEdit = new QLineEdit();
    mmproj_LineEdit->setPlaceholderText(wordsObj["right click and choose mmproj"].toString());
    layout_H17->addWidget(mmproj_label);
    layout_H17->addWidget(mmproj_LineEdit);
    decode_layout->addLayout(layout_H17);//将布局添加到总布局
    mmproj_LineEdit->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    mmproj_LineEdit->installEventFilter(this);

    decode_box->setLayout(decode_layout);
    layout->addWidget(decode_box);

    //------------模式设置---------------
    mode_box = new QGroupBox(wordsObj["mode set"].toString());//模式设置区域
    QVBoxLayout *mode_layout = new QVBoxLayout();//模式设置垂直布局器
    

    //补完控制
    complete_btn = new QRadioButton(wordsObj["complete mode"].toString());
    mode_layout->addWidget(complete_btn);
    connect(complete_btn, &QRadioButton::clicked, this, &Widget::complete_change);
    //多轮对话
    chat_btn = new QRadioButton(wordsObj["chat mode"].toString());
    mode_layout->addWidget(chat_btn);
    chat_btn->setChecked(1);
    connect(chat_btn, &QRadioButton::clicked, this, &Widget::chat_change);
    //网页服务控制
    QHBoxLayout *layout_H10 = new QHBoxLayout();//水平布局器
    web_btn = new QRadioButton(wordsObj["server mode"].toString());
    layout_H10->addWidget(web_btn);
    port_label = new QLabel(wordsObj["port"].toString());
    layout_H10->addWidget(port_label);
    port_lineEdit = new QLineEdit();
    port_lineEdit->setText(ui_port);
    QIntValidator *validator = new QIntValidator(0, 65535);//限制端口输入
    port_lineEdit->setValidator(validator);
    layout_H10->addWidget(port_lineEdit);
    mode_layout->addLayout(layout_H10);//将布局添加到总布局
    connect(web_btn, &QRadioButton::clicked, this, &Widget::web_change);

    mode_box->setLayout(mode_layout);
    layout->addWidget(mode_box);

#ifdef BODY_USE_32BIT
    lora_label->setVisible(0);
    lora_LineEdit->setVisible(0);
#endif
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    lora_label->setVisible(0);
    lora_LineEdit->setVisible(0);
#endif

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, date_dialog);// 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_set);
    connect(buttonBox, &QDialogButtonBox::rejected, set_dialog, &QDialog::reject);
    set_dialog->setWindowTitle(wordsObj["set"].toString());

}

// 设置约定选项
void Widget::set_DateDialog()
{
    date_dialog = new QDialog;
    date_dialog->setWindowFlags(date_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);//隐藏?按钮
    date_dialog->setWindowFlags(date_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);//隐藏关闭按钮
    date_dialog->resize(150, 200); // 设置宽度,高度
    QVBoxLayout *layout = new QVBoxLayout(date_dialog);//垂直布局器
    //layout->setSizeConstraint(QLayout::SetFixedSize);//使得自动调整紧凑布局
    //------------提示词模板设置---------------
    prompt_box = new QGroupBox(wordsObj["prompt"].toString() + wordsObj["template"].toString());//提示词模板设置区域
    QVBoxLayout *prompt_layout = new QVBoxLayout();//提示词模板设置垂直布局器

    //预设模板
    QHBoxLayout *layout_H9 = new QHBoxLayout();//水平布局器
    prompt_label = new QLabel(wordsObj["chat"].toString() + wordsObj["template"].toString());
    layout_H9->addWidget(prompt_label);
    prompt_comboBox = new QComboBox();
    prompt_comboBox->setMinimumWidth(200);
    for (const QString &key : date_map.keys())
    {
        prompt_comboBox->addItem(key);
    }
    prompt_comboBox->addItem(wordsObj["custom set"].toString());
    prompt_comboBox->setCurrentText(ui_template);//默认使用qwen的提示词模板
    connect(prompt_comboBox, &QComboBox::currentTextChanged, this, &Widget::prompt_template_change);
    layout_H9->addWidget(prompt_comboBox);
    prompt_layout->addLayout(layout_H9);//将布局添加到总布局
    //系统指令
    QHBoxLayout *layout_H11 = new QHBoxLayout();//水平布局器
    system_label = new QLabel(wordsObj["system calling"].toString());
    system_label->setToolTip(wordsObj["will predecode"].toString());
    layout_H11->addWidget(system_label);
    system_TextEdit = new QTextEdit();
    // 设置样式表
    // system_TextEdit->setStyleSheet("QTextEdit {"
    //                     "border: 1px solid black;"   // 边框宽度为1px, 颜色为黑色
    //                     "border-radius: 5px;"        // 边框圆角为5px
    //                     "padding: 1px;"              // 内边距为1px
    //                     "}");
    layout_H11->addWidget(system_TextEdit);
    prompt_layout->addLayout(layout_H11);//将布局添加到总布局
    //输入前缀设置
    QHBoxLayout *layout_H5 = new QHBoxLayout();//水平布局器
    input_pfx_label = new QLabel(wordsObj["user name"].toString());
    input_pfx_label->setToolTip(wordsObj["pfx"].toString());
    layout_H5->addWidget(input_pfx_label);
    input_pfx_LineEdit = new QLineEdit();
    input_pfx_LineEdit->setText(ui_DATES.input_pfx);
    layout_H5->addWidget(input_pfx_LineEdit);
    prompt_layout->addLayout(layout_H5);//将布局添加到总布局
    //输入后缀设置
    QHBoxLayout *layout_H6 = new QHBoxLayout();//水平布局器
    input_sfx_label = new QLabel(wordsObj["bot name"].toString());
    input_sfx_label->setToolTip(wordsObj["sfx"].toString());
    layout_H6->addWidget(input_sfx_label);
    input_sfx_LineEdit = new QLineEdit();
    input_sfx_LineEdit->setText(ui_DATES.input_sfx);
    layout_H6->addWidget(input_sfx_LineEdit);
    prompt_layout->addLayout(layout_H6);//将布局添加到垂直布局

    prompt_box->setLayout(prompt_layout);
    layout->addWidget(prompt_box);

    //------------工具设置---------------
    tool_box = new QGroupBox(wordsObj["mount"].toString() + wordsObj["tool"].toString());//提示词模板设置区域
    QVBoxLayout *tool_layout = new QVBoxLayout();//提示词模板设置垂直布局器
    //可用工具
    QHBoxLayout *layout_H44 = new QHBoxLayout();//水平布局器
    calculator_checkbox = new QCheckBox(wordsObj["calculator"].toString());
    cmd_checkbox = new QCheckBox(wordsObj["cmd"].toString());
    layout_H44->addWidget(calculator_checkbox);
    layout_H44->addWidget(cmd_checkbox);
    tool_layout->addLayout(layout_H44);//将布局添加到垂直布局

    QHBoxLayout *layout_H45 = new QHBoxLayout();//水平布局器
    search_checkbox = new QCheckBox(wordsObj["search"].toString());
    knowledge_checkbox = new QCheckBox(wordsObj["knowledge"].toString());
    layout_H45->addWidget(search_checkbox);
    layout_H45->addWidget(knowledge_checkbox);
    tool_layout->addLayout(layout_H45);//将布局添加到垂直布局

    QHBoxLayout *layout_H46 = new QHBoxLayout();//水平布局器
    positron_checkbox = new QCheckBox(wordsObj["positron"].toString());
    llm_checkbox = new QCheckBox(wordsObj["llm"].toString());
    layout_H46->addWidget(positron_checkbox);
    layout_H46->addWidget(llm_checkbox);
    tool_layout->addLayout(layout_H46);//将布局添加到垂直布局

    connect(calculator_checkbox, &QCheckBox::stateChanged, this, &Widget::calculator_change);
    connect(cmd_checkbox, &QCheckBox::stateChanged, this, &Widget::cmd_change);
    connect(search_checkbox, &QCheckBox::stateChanged, this, &Widget::search_change);
    connect(knowledge_checkbox, &QCheckBox::stateChanged, this, &Widget::knowledge_change);
    connect(positron_checkbox, &QCheckBox::stateChanged, this, &Widget::positron_change);
    connect(llm_checkbox, &QCheckBox::stateChanged, this, &Widget::llm_change);

    //附加指令
    QHBoxLayout *layout_H55 = new QHBoxLayout();//水平布局器
    extra_label = new QLabel(wordsObj["extra calling"].toString());
    layout_H55->addWidget(extra_label);
    switch_lan_button = new QPushButton(ui_extra_lan);
    switch_lan_button->setMinimumWidth(200);
    layout_H55->addWidget(switch_lan_button);
    extra_TextEdit = new QTextEdit();
    extra_TextEdit->setPlaceholderText(wordsObj["extra calling tooltip"].toString());
    // 设置样式表
    // extra_TextEdit->setStyleSheet("QTextEdit {"
    //                     "border: 1px solid black;"   // 边框宽度为1px, 颜色为黑色
    //                     "border-radius: 5px;"        // 边框圆角为5px
    //                     "padding: 1px;"              // 内边距为1px
    //                     "}");
    tool_layout->addLayout(layout_H55);//将布局添加到总布局
    tool_layout->addWidget(extra_TextEdit);

    tool_box->setLayout(tool_layout);
    layout->addWidget(tool_box);
    connect(switch_lan_button, &QPushButton::clicked, this, &Widget::switch_lan_change);
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, date_dialog);// 创建 QDialogButtonBox 用于确定按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_date);
    connect(buttonBox, &QDialogButtonBox::rejected, date_dialog, &QDialog::reject);
    prompt_template_change();//先应用提示词模板
    system_TextEdit->setText(ui_system_prompt);
    date_dialog->setWindowTitle(wordsObj["date"].toString());
}

//初始化设置api选项
void Widget::setApiDialog()
{
    api_dialog = new QDialog();
    api_dialog->setWindowTitle("api" + wordsObj["set"].toString());
    api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);//隐藏?按钮
    api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);//隐藏关闭按钮
    api_dialog->resize(250, 200); // 设置宽度为400像素,高度为200像素

    QVBoxLayout *layout = new QVBoxLayout(api_dialog);//垂直布局器

    QHBoxLayout *layout_H1 = new QHBoxLayout();//水平布局器
    api_ip_label = new QLabel("api " + wordsObj["address"].toString());
    layout_H1->addWidget(api_ip_label);
    api_ip_LineEdit = new QLineEdit();
    api_ip_LineEdit->setPlaceholderText(wordsObj["input server ip"].toString());
    api_ip_LineEdit->setText(apis.api_ip);
    QRegExp ipRegex("^((25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$");// IPv4的正则表达式限制
    QRegExpValidator *validator_ipv4 = new QRegExpValidator(ipRegex, api_ip_LineEdit);
    api_ip_LineEdit->setValidator(validator_ipv4);
    layout_H1->addWidget(api_ip_LineEdit);
    layout->addLayout(layout_H1);//将布局添加到总布局

    QHBoxLayout *layout_H2 = new QHBoxLayout();//水平布局器
    api_port_label = new QLabel("api " + wordsObj["port"].toString());
    layout_H2->addWidget(api_port_label);
    api_port_LineEdit = new QLineEdit();
    api_port_LineEdit->setText(apis.api_port);
    QIntValidator *validator_port = new QIntValidator(0, 65535);//限制端口输入
    api_port_LineEdit->setValidator(validator_port);
    layout_H2->addWidget(api_port_LineEdit);
    layout->addLayout(layout_H2);//将布局添加到总布局

    QHBoxLayout *layout_H3 = new QHBoxLayout();//水平布局器
    api_chat_label = new QLabel(wordsObj["chat"].toString()+wordsObj["endpoint"].toString());
    layout_H3->addWidget(api_chat_label);
    api_chat_LineEdit = new QLineEdit();
    api_chat_LineEdit->setText(apis.api_chat_endpoint);
    layout_H3->addWidget(api_chat_LineEdit);
    layout->addLayout(layout_H3);//将布局添加到总布局

    QHBoxLayout *layout_H4 = new QHBoxLayout();//水平布局器
    api_complete_label = new QLabel(wordsObj["complete"].toString()+wordsObj["endpoint"].toString());
    layout_H4->addWidget(api_complete_label);
    api_complete_LineEdit = new QLineEdit();
    api_complete_LineEdit->setText(apis.api_complete_endpoint);
    layout_H4->addWidget(api_complete_LineEdit);
    layout->addLayout(layout_H4);//将布局添加到总布局

    QHBoxLayout *layout_H5 = new QHBoxLayout();//水平布局器
    api_is_cache = new QCheckBox(wordsObj["cache ctx"].toString());
    //不显示,因为缓存上下文只作用补完模式,意义不大
    // layout_H5->addWidget(api_is_cache);
    // layout->addLayout(layout_H5);//将布局添加到总布局

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, api_dialog);// 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_api);
    connect(buttonBox, &QDialogButtonBox::rejected, api_dialog, &QDialog::reject);

}
//选用计算器工具
void Widget::calculator_change()
{
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
        //ui_calculator_ischecked = calculator_checkbox->isChecked();
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}
//选用系统终端工具
void Widget::cmd_change()
{
    
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
        //ui_cmd_ischecked = cmd_checkbox->isChecked();
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::search_change()
{
    
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
        //ui_search_ischecked = search_checkbox->isChecked();
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::knowledge_change()
{
    
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
        //ui_knowledge_ischecked = knowledge_checkbox->isChecked();
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::positron_change()
{
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
        //ui_positron_ischecked = positron_checkbox->isChecked();
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::llm_change()
{
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
        //ui_llm_ischecked = llm_checkbox->isChecked();
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}