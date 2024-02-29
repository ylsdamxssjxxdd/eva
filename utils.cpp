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

void Widget::load_move()
{
    QTextCursor cursor = ui->state->textCursor();
    QTextCharFormat format;
    QFont font;font.setPointSize(6);format.setFont(font);

    if(load_action % 2 ==0)
    {
        cursor.movePosition(QTextCursor::Start);//移到文本开头
        cursor.movePosition(QTextCursor::Down,QTextCursor::MoveAnchor,movie_dot.at(load_action / 2).x()-2+playlineNumber);//向下移动到指定行
        cursor.movePosition(QTextCursor::Right,QTextCursor::MoveAnchor,movie_dot.at(load_action / 2).y()-1);//向右移动到指定列
        cursor.movePosition(QTextCursor::Left,QTextCursor::KeepAnchor);//选中当前字符
        cursor.removeSelectedText();//删除选中字符
        set_dotcolor(&format,load_action / 2);//设置字体颜色
        cursor.setCharFormat(format);//设置字体
        cursor.insertText("*");//插入字符
    }
    else
    {
        cursor.movePosition(QTextCursor::Start);//移到文本开头
        cursor.movePosition(QTextCursor::Down,QTextCursor::MoveAnchor,movie_dot.at(load_action / 2 + 1).x()-2+playlineNumber);//向下移动到指定行
        cursor.movePosition(QTextCursor::Right,QTextCursor::MoveAnchor,movie_dot.at(load_action / 2 + 1).y()-1);//向右移动到指定列
        cursor.movePosition(QTextCursor::Left,QTextCursor::KeepAnchor);//选中当前字符
        cursor.removeSelectedText();//删除选中字符
        cursor.setCharFormat(format);//设置字体
        cursor.insertText(" ");//插入字符
    }

    load_action ++;
}


void Widget::load_play()
{
    QTextCursor cursor = ui->state->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n");//插个回车
    QTextCharFormat format;
    QFont font;font.setPointSize(6);format.setFont(font);
    load_action = 0;

    //获取当前行数
    playlineNumber = 0;
    QTextDocument *document = ui->state->document();
    for (QTextBlock block = document->begin(); block != document->end(); block = block.next()) 
    {
        ++playlineNumber;
    }
    //qDebug() << "lineNumber: " << playlineNumber;
    
    //展示背景
    for(int i = 0;i<movie_line.size();++i)
    {
        cursor.movePosition(QTextCursor::End);//移到文本开头
        cursor.setCharFormat(format);//设置字体
        cursor.insertText(movie_line.at(i)+"\n");//插入字符
        //恢复字体大小
        if(i==movie_line.size()-1)
        {
            font.setPointSize(9);format.setFont(font);
            cursor.setCharFormat(format);//设置字体
            //cursor.insertText("");    
        }
    }

    //先自动播放动画
    load_pTimer->start(800);
    //向下滑
    load_begin_pTimer->start(100);
}

//设置72个点的字体前景色颜色
void Widget::set_dotcolor(QTextCharFormat *format,int load_action)
{
    if(load_action<4){format->setForeground(movie_color.at(0));}
    else if(load_action<8){format->setForeground(movie_color.at(1));}
    else if(load_action<18){format->setForeground(movie_color.at(2));}
    else if(load_action<28){format->setForeground(movie_color.at(3));}
    else if(load_action<32){format->setForeground(movie_color.at(4));}
    else if(load_action<36){format->setForeground(movie_color.at(5));}
    else if(load_action<40){format->setForeground(movie_color.at(6));}
    else if(load_action<50){format->setForeground(movie_color.at(7));}
    else if(load_action<54){format->setForeground(movie_color.at(8));}
    else if(load_action<58){format->setForeground(movie_color.at(9));}
    else if(load_action<68){format->setForeground(movie_color.at(10));}
    else if(load_action<72){format->setForeground(movie_color.at(11));}
}

//连接动画计时器到时处理
void Widget::load_handleTimeout()
{
    if(load_pTimer->isActive()){load_pTimer->stop();}//控制超时处理函数只会处理一次
    if(load_action<all_fps)
    {
        load_move();//下一帧
    }
    
    //循环播放
    if(load_action<all_fps)
    {
        
        if(is_load)
        {
            load_pTimer->start(10);//延时多少ms后发出timeout()信号
        }
        else
        {
            load_pTimer->start(1100);//延时多少ms后发出timeout()信号
        }
    }
    else if(is_load)
    {
        load_action = 0;//重置动作计数
        all_fps --;//减去补上的最后一帧
        emit load_play_over();
    }

}

void Widget::encode_play()
{
    encode_LineNumber = ui->state->document()->blockCount() - 1;// 获取那一行的行数
    encode_pTimer->start(500);//延时多少ms后发出timeout()信号
}
void Widget::encode_move()
{
    //控制光标移动到指定行的末尾
    QTextBlock block = ui->state->document()->findBlockByLineNumber(encode_LineNumber);
    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::EndOfBlock);

    if(encode_action % 2 == 0)
    {
        cursor.insertText(".");//插入字符
    }
    else
    {
        cursor.movePosition(QTextCursor::Left,QTextCursor::KeepAnchor);//选中当前字符
        cursor.removeSelectedText();//删除选中字符
    }

    encode_action ++;
}

void Widget::encode_handleTimeout()
{
    if(encode_pTimer->isActive()){encode_pTimer->stop();}//控制超时处理函数只会处理一次
    encode_move();
    encode_pTimer->start(500);//延时多少ms后发出timeout()信号
}

//连接动画播放完毕后处理
void Widget::load_play_finish()
{
    //播放完毕状态区再播放置底动画
    load_over_pTimer->start(100);
}



//滑动到最佳动画位置
void Widget::load_begin_handleTimeout()
{
    if(load_begin_pTimer->isActive()){load_begin_pTimer->stop();}//控制超时处理函数只会处理一次
    int currentValue = ui->state->verticalScrollBar()->value();//当前滑动条位置
    ui->state->verticalScrollBar()->setValue(currentValue+1);
    //qDebug() << currentValue <<playlineNumber;
    if(currentValue < playlineNumber-2)
    {
        load_begin_pTimer->start(100);
    }
    else
    {
        load_begin_pTimer->stop();
    }

}

//模型装载完毕动画,并解锁按钮
void Widget::load_over_handleTimeout()
{
    if(load_over_pTimer->isActive()){load_over_pTimer->stop();}//控制超时处理函数只会处理一次
    
    int currentValue = ui->state->verticalScrollBar()->value();//当前滑动条位置
    ui->state->verticalScrollBar()->setValue(currentValue+1);
    currentValue ++;
    //展示完就停止
    if(currentValue <= ui->state->verticalScrollBar()->maximum())
    {
        load_over_pTimer->start(100);
    }
    //滚到最下面才解锁按钮,真正装载完毕
    else
    {
        keeptimer->stop();
        force_unlockload->start(0);//缓解死锁？
    }
}
//装载完毕强制预处理
void Widget::unlockLoad()
{
    ui_state = "ui:" + wordsObj["load model"].toString() + wordsObj["over"].toString() + " " + QString::number(load_time,'f',2)+" s " + wordsObj["right click and check model log"].toString();reflash_state(ui_state,1);
    if(ui_SETTINGS.ngl>0){QApplication::setWindowIcon(QIcon(":/ui/green_logo.png"));}// 设置应用程序图标
    else{QApplication::setWindowIcon(QIcon(":/ui/blue_logo.png"));}// 设置应用程序图标
    this->setWindowTitle(wordsObj["current model"].toString() + " " + ui_SETTINGS.modelpath.split("/").last());
    ui->kv_bar->message = wordsObj["brain"].toString();
    ui->send->setEnabled(1);
    ui->reset->setEnabled(1);
    ui->date->setEnabled(1);ui->set->setEnabled(1);
    ui->load->setEnabled(1);
    ui->output->setStyleSheet("");//取消文本为透明
    ui->input->setFocus();//设置输入区为焦点

    //设置特殊文本,显示设备支持情况
    QTextCharFormat format;
    format.setForeground(QColor(0,0,200));    // 设置前景颜色
    ui->state->setCurrentCharFormat(format);//设置光标格式
    format.clearForeground();//清除前景颜色
    ui->state->setCurrentCharFormat(format);//设置光标格式
    ui->input->setPlaceholderText(wordsObj["chat or right click to choose question"].toString());
    ui->cpu_bar->setToolTip(wordsObj["nthread/maxthread"].toString()+"  "+QString::number(ui_nthread)+"/"+QString::number(max_thread));
    //如果是对话模式则预推理约定
    if(ui_mode == 0)
    {
        history_prompt = ui_DATES.system_prompt;//同步历史约定内容
        ui_need_predecode = true;
        on_send_clicked();
    }
    force_unlockload->stop();
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
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, date_dialog);// 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_date);
    connect(buttonBox, &QDialogButtonBox::rejected, date_dialog, &QDialog::reject);
    prompt_template_change();//先应用提示词模板
    system_TextEdit->setText(ui_system_prompt);
    date_dialog->setWindowTitle(wordsObj["date"].toString());
}



//输出区滚动条事件响应
void Widget::scrollBarValueChanged(int value)
{
    //如果滑动条在最下面则自动滚动
    int maximumValue = output_scrollBar->maximum();
    if (value == maximumValue)
    {
        is_stop_scroll = 0;
    }
    else
    {
        is_stop_scroll = 1;
    }
}

//向state末尾添加文本并滚动
void Widget::state_scroll()
{
    ui->state->appendPlainText(ui_state);
    //如果预解码完毕则强制见底
    if(ui_state.contains(wordsObj["system calling"].toString() + wordsObj["predecode"].toString()))
    {
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum());//滚动条滚动到最下面
    }
}
//向output末尾添加文本并滚动
void Widget::output_scroll(QColor color)
{
    QTextCursor cursor = ui->output->textCursor();
    QTextCharFormat textFormat;

    textFormat.setForeground(QBrush(color)); // 设置文本颜色
    cursor.movePosition(QTextCursor::End);//光标移动到末尾
    cursor.mergeCharFormat(textFormat);   // 应用文本格式

    cursor.insertText(ui_output);//输出

    QTextCharFormat textFormat0;// 清空文本格式
    cursor.movePosition(QTextCursor::End);//光标移动到末尾
    cursor.mergeCharFormat(textFormat0);   // 应用文本格式

    if(!is_stop_scroll)//如果停止标签没有启用,则每次输出完自动滚动到最下面
    {
       ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum());//滚动条滚动到最下面
    }

}
//根据标签改变ui控件的状态
void Widget::ui_change()
{
    if(ui_mode == 1)
    {
        //补完模式关闭输入区
        ui->input->clear();
        ui->input->setPlaceholderText(wordsObj["please change the text"].toString());
        ui->input->setStyleSheet("background-color: gray;");//设置背景为灰色
        ui->input->setReadOnly(1);
        ui->send->setText(wordsObj["complete"].toString());
        ui->output->setReadOnly(0);
        ui->send->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return));//恢复快捷键
        ui->output->setFocus();//设置输出区为焦点
    }
    else if(ui_mode == 0)
    {
        //对话模式则清空输出区
        ui->output->clear();
        ui->input->setPlaceholderText(wordsObj["chat or right click to choose question"].toString());
        ui->input->setStyleSheet("background-color: white;");
        ui->input->setReadOnly(0);
        ui->send->setText(wordsObj["send"].toString());
        ui->output->setReadOnly(1);
        ui->send->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Return));//恢复快捷键
        ui->input->setFocus();//设置输入区为焦点
    }
}


//温度滑块响应
void Widget::temp_change()
{
    temp_label->setText(wordsObj["temperature"].toString()+" "+ QString::number(temp_slider->value()/100.0));
}
//ngl滑块响应
void Widget::ngl_change()
{
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    ngl_label->setText("gpu "+ wordsObj["offload"].toString() + " " + QString::number(ngl_slider->value()));
#endif
}
//batch滑块响应
void Widget::batch_change()
{
    batch_label->setText(wordsObj["batch size"].toString() + " " + QString::number(batch_slider->value()));
}
// nctx滑块响应
void Widget::nctx_change()
{
    nctx_label->setText(wordsObj["ctx"].toString()+wordsObj["length"].toString()+" "+ QString::number(nctx_slider->value()));
    
}
//repeat滑块响应
void Widget::repeat_change()
{

    repeat_label->setText(wordsObj["repeat"].toString()+" "+ QString::number(repeat_slider->value()/100.0));
}

void Widget::npredict_change()
{
    npredict_label->setText(wordsObj["npredict"].toString() + " " + QString::number(npredict_slider->value()));
}

void Widget::nthread_change()
{
    nthread_label->setText("cpu " + wordsObj["thread"].toString() + " " + QString::number(nthread_slider->value()));
}

//更新输出区,is_while表示从流式输出的token
void Widget::reflash_output(const QString &result,bool is_while, QColor color)
{
    ui_output = result;
    if(is_test && is_while)//现在要知道是模型输出的答案还是预编码完成的结果,要将预编码完成的结果排除
    {
        test_count++;//已经加一了
        QString result_ = result;
        //答对，remove(' ')移除答案中的空格
        if(result_.remove(' ') == test_list_answer.at(test_question_index.at(0)))//
        {
            test_score++;output_scroll(Qt::green);
            ui_state = "ui:"+ QString::number(test_count) + " " +wordsObj["answer right"].toString() + " " + wordsObj["right answer"].toString() + test_list_answer.at(test_question_index.at(0));reflash_state(ui_state,1);
        }
        //答错
        else
        {
            output_scroll(Qt::red);
            ui_state = "ui:"+ QString::number(test_count) + " " + wordsObj["answer error"].toString() + " " + wordsObj["right answer"].toString() + test_list_answer.at(test_question_index.at(0));reflash_state(ui_state,2);
        }
        float acc = test_score / test_count * 100.0;//回答准确率
        test_question_index.removeAt(0);//回答完毕删除开头的第一个问题
        if(is_api){this->setWindowTitle(wordsObj["test"].toString() + QString::number(test_count) +"/"+ QString::number(test_list_question.size())+ "   " + wordsObj["accurate"].toString() +QString::number(acc,'f',1) + "% " + "   "+wordsObj["current api"].toString() + " " + current_api);}
        else{this->setWindowTitle(wordsObj["test"].toString() + QString::number(test_count) +"/"+ QString::number(test_list_question.size())+ "   " + wordsObj["accurate"].toString() +QString::number(acc,'f',1) + "% " + "   "+ ui_SETTINGS.modelpath.split("/").last());} 

        //每20次题加一次引导题
        if(int(test_count) % 20 == 0)
        {
            if(!is_api){emit ui2bot_help_input();}
            else{api_addhelpinput();}
            ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,3);
        }
    }
    else
    {
        //正常输出
        ui_output = result;
        output_scroll(color);
    }
    if(is_while){temp_assistant_history += result;}
    
}
//更新状态区
void Widget::reflash_state(const QString &state_,int state_num)
{
    QTextCharFormat format;//设置特殊文本颜色
    ui_state = state_;
    ui_state.remove("\n");ui_state.remove("\r");//过滤回车和换行符
    if(state_num==0)//一般黑色
    {
        format.clearForeground();//清除前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
        state_scroll();
    }
    else if(state_num==1)//正常绿色
    {
        format.setForeground(QColor(0,200,0));    // 设置前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
        state_scroll();
        format.clearForeground();//清除前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
    }
    else if(state_num==2)//不正常红色
    {
        format.setForeground(QColor(200,0,0));    // 设置前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
        state_scroll();
        format.clearForeground();//清除前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
    }
    else if(state_num==3)//信号蓝色
    {
        format.setForeground(QColor(0,0,200));    // 红色设置前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
        state_scroll();
        format.clearForeground();//清除前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
    }
    else if(state_num==4)//行为紫色
    {
        format.setForeground(QColor(128,0,128));    // 红色设置前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
        state_scroll();
        format.clearForeground();//清除前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
    }
    else if(state_num==5)//工具橘黄色
    {
        format.setForeground(QColor(255, 165, 0));    // 红色设置前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
        state_scroll();
        format.clearForeground();//清除前景颜色
        ui->state->setCurrentCharFormat(format);//设置光标格式
    }
    


}
//补完按钮响应
void Widget::complete_change()
{
    //选中则禁止约定输入
    if(complete_btn->isChecked())
    {
        // prompt_comboBox->setStyleSheet("color: transparent;");//设置文本为透明
        // system_TextEdit->setStyleSheet("color: transparent;");//设置文本为透明
        // input_pfx_LineEdit->setStyleSheet("color: transparent;");//设置文本为透明
        // input_sfx_LineEdit->setStyleSheet("color: transparent;");//设置文本为透明
        
        npredict_slider->setEnabled(1);
        nthread_slider->setEnabled(1);
        temp_slider->setEnabled(1);
        repeat_slider->setEnabled(1);
        nctx_slider->setEnabled(1);
        port_lineEdit->setEnabled(0);

        // prompt_comboBox->setEnabled(0);
        // system_TextEdit->setEnabled(0);
        // input_pfx_LineEdit->setEnabled(0);
        // input_sfx_LineEdit->setEnabled(0);
        // tool_box->setEnabled(0);

    }
}
//对话按钮响应
void Widget::chat_change()
{
    if(chat_btn->isChecked())
    {
        // prompt_comboBox->setStyleSheet("");//取消文本为透明
        // system_TextEdit->setStyleSheet("");//取消文本为透明
        // input_pfx_LineEdit->setStyleSheet("");//取消文本为透明
        // input_sfx_LineEdit->setStyleSheet("");//取消文本为透明
        // prompt_comboBox->setEnabled(1);
        // if(prompt_comboBox->currentText() == wordsObj["custom set"].toString())
        // {
        //     system_TextEdit->setEnabled(1);
        //     input_pfx_LineEdit->setEnabled(1);
        //     input_sfx_LineEdit->setEnabled(1);
        // }
        // tool_box->setEnabled(1);//挂载工具区域

        temp_slider->setEnabled(1);
        repeat_slider->setEnabled(1);
        nctx_slider->setEnabled(1);
        npredict_slider->setEnabled(1);
        nthread_slider->setEnabled(1);

        port_lineEdit->setEnabled(0);

    }
}

//网页服务响应
void Widget::web_change()
{
    if(web_btn->isChecked())
    {
        // prompt_comboBox->setStyleSheet("color: transparent;");//设置文本为透明
        // system_TextEdit->setStyleSheet("color: transparent;");//设置文本为透明
        // input_pfx_LineEdit->setStyleSheet("color: transparent;");//设置文本为透明
        // input_sfx_LineEdit->setStyleSheet("color: transparent;");//设置文本为透明

        // prompt_comboBox->setEnabled(0);
        // system_TextEdit->setEnabled(0);
        // input_pfx_LineEdit->setEnabled(0);
        // input_sfx_LineEdit->setEnabled(0);

        // tool_box->setEnabled(0);//挂载工具区域

        temp_slider->setEnabled(0);
        npredict_slider->setEnabled(0);
        repeat_slider->setEnabled(0);

        port_lineEdit->setEnabled(1);
    }
}
//获取本机第一个ip地址
QString Widget::getFirstNonLoopbackIPv4Address() {
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (int i = 0; i < list.count(); i++) {
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol) {
            return list[i].toString();
        }
    }
    return QString();
}

//第三方程序开始
void Widget::onProcessStarted()
{
    if(ui_SETTINGS.ngl==0){QApplication::setWindowIcon(QIcon(":/ui/connection-point-blue.png"));}
    else{QApplication::setWindowIcon(QIcon(":/ui/connection-point-green.png"));}
    ipAddress = getFirstNonLoopbackIPv4Address();
    ui_state = "ui:server"+wordsObj["oning"].toString()+"...";reflash_state(ui_state,3);
}
//第三方程序结束
void Widget::onProcessFinished()
{
    QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
    ui_state = "ui:server"+wordsObj["off"].toString();reflash_state(ui_state,3);
    ui_output = "\nserver"+wordsObj["shut down"].toString();output_scroll();
}

//提示词模板下拉框响应
void Widget::prompt_template_change()
{
    if(prompt_comboBox->currentText() == wordsObj["custom set"].toString())
    {
        system_TextEdit->setEnabled(1);
        input_pfx_LineEdit->setEnabled(1);
        input_sfx_LineEdit->setEnabled(1);
    }
    else
    {
        system_TextEdit->setText(date_map[prompt_comboBox->currentText()].system_prompt);
        system_TextEdit->setEnabled(0);
        input_pfx_LineEdit->setText(date_map[prompt_comboBox->currentText()].input_pfx);
        input_pfx_LineEdit->setEnabled(0);
        input_sfx_LineEdit->setText(date_map[prompt_comboBox->currentText()].input_sfx);
        input_sfx_LineEdit->setEnabled(0);
    }
}

// 构建测试问题
void Widget::makeTestQuestion(QString dirPath)
{
    getAllFiles(dirPath);
    for(int i=0;i<filePathList.size();++i)
    {
        QString fileName = filePathList.at(i);
        readCsvFile(fileName);
    }
}
//清空题库
void Widget::clearQuestionlist()
{
    filePathList.clear();
    test_list_answer.clear();
    test_list_question.clear();
}

void Widget::readCsvFile(const QString &fileName)
{
    QFile file(fileName);
    QString questiontitle = wordsObj["question type"].toString() + ":" + fileName.split("/").last().split(".").at(0) + "\n\n";
    if (!file.open(QIODevice::ReadOnly)) 
    {
        qDebug() << file.errorString();
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    // 读取并忽略标题行
    QString headerLine = in.readLine();

    // 读取文件的每一行
    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 使用制表符分割每一行的内容
        QStringList fields = line.split(",");

        
        // 确保每行有足够的列
        if(fields.size() >= 7)
        {
            // 输出题目和答案
            // qDebug() << "id:" << fields.at(0).trimmed();
            // qDebug() << "Question:" << fields.at(1).trimmed();
            // qDebug() << "A:" << fields.at(2).trimmed();
            // qDebug() << "B:" << fields.at(3).trimmed();
            // qDebug() << "C:" << fields.at(4).trimmed();
            // qDebug() << "D:" << fields.at(5).trimmed();
            // qDebug() << "Answer:" << fields.at(6).trimmed();
            test_list_question<<questiontitle +fields.at(1).trimmed()+"\n\n"+"A:" + fields.at(2).trimmed()+"\n"+"B:"+fields.at(3).trimmed()+"\n"+"C:"+fields.at(4).trimmed()+"\n"+"D:"+fields.at(5).trimmed()+"\n";
            test_list_answer<<fields.at(6).trimmed();
        }
    }

    file.close();
}


void Widget::makeTestIndex()
{
    test_question_index.clear();
    for(int i =0;i<test_list_question.size();++i)
    //for(int i =0;i<3;++i)
    {
        test_question_index<<i;
    }
    //std::random_shuffle(test_question_index.begin(), test_question_index.end());//随机打乱顺序
}
//遍历文件
void Widget::getAllFiles(const QString&floderPath)
{
    QDir folder(floderPath);
    folder.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList entries = folder.entryInfoList();

    for(const QFileInfo&entry : entries)
    {
        if(entry.isDir())
        {
            childPathList.append(entry.filePath());
        }
        else if(entry.isFile())
        {
            filePathList.append(entry.filePath());
        }
    }
}

void Widget::chooseLorapath()
{
    //用户选择模型位置
    QString lora_path = QFileDialog::getOpenFileName(this,"choose lora model",ui_SETTINGS.modelpath);
    lora_LineEdit->setText(lora_path);
}
void Widget::chooseMmprojpath()
{
    //用户选择模型位置
    QString mmproj_path = QFileDialog::getOpenFileName(this,"choose mmproj model",ui_SETTINGS.modelpath);
    mmproj_LineEdit->setText(mmproj_path);

}
//api模式切换时控件可见状态
void Widget::change_api_dialog(bool enable)
{
    repeat_label->setVisible(enable);repeat_slider->setVisible(enable);
    nctx_label->setVisible(enable);nctx_slider->setVisible(enable);
    nthread_label->setVisible(enable);nthread_slider->setVisible(enable);
    batch_label->setVisible(enable);batch_slider->setVisible(enable);
    mmproj_label->setVisible(enable);mmproj_LineEdit->setVisible(enable);
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    ngl_label->setVisible(enable);ngl_slider->setVisible(enable);
#else
    lora_label->setVisible(enable);lora_LineEdit->setVisible(enable);
#endif
    port_label->setVisible(enable);port_lineEdit->setVisible(enable);web_btn->setVisible(enable);
}

//构建附加指令
QString Widget::create_extra_prompt()
{
    QString extra_prompt_;
    if(switch_lan_button->text()=="zh")
    {
        extra_prompt_ = wordsObj["head_extra_prompt_zh"].toString();
        if(is_load_tool)
        {
            //头
            if(calculator_checkbox->isChecked()){extra_prompt_ += tool_map["calculator"].func_describe_zh + "\n";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += tool_map["cmd"].func_describe_zh + "\n";}
            if(search_checkbox->isChecked()){extra_prompt_ += tool_map["search"].func_describe_zh + "\n";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ += tool_map["knowledge"].func_describe_zh + "\n";}
            if(positron_checkbox->isChecked()){extra_prompt_ += tool_map["positron"].func_describe_zh + "\n";}
            if(llm_checkbox->isChecked()){extra_prompt_ += tool_map["llm"].func_describe_zh + "\n";}
            //中
            extra_prompt_ +=wordsObj["middle_extra_prompt_zh"].toString();
            if(calculator_checkbox->isChecked()){extra_prompt_ += "\"calculator\" ";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += "\"cmd\" ";}
            if(search_checkbox->isChecked()){extra_prompt_ += "\"search\" ";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ +="\"knowledge\" ";}
            if(positron_checkbox->isChecked()){extra_prompt_ +="\"positron\" ";}
            if(llm_checkbox->isChecked()){extra_prompt_ +="\"llm\" ";}
            //尾
            extra_prompt_ += wordsObj["tail_extra_prompt_zh"].toString();
        }
        else{extra_prompt_ = "";}
        return extra_prompt_;
    }
    else if(switch_lan_button->text()=="en")
    {
        extra_prompt_ = wordsObj["head_extra_prompt_en"].toString();
        if(is_load_tool)
        {
            //头
            if(calculator_checkbox->isChecked()){extra_prompt_ += tool_map["calculator"].func_describe_en + "\n";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += tool_map["cmd"].func_describe_en + "\n";}
            if(search_checkbox->isChecked()){extra_prompt_ += tool_map["search"].func_describe_en + "\n";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ += tool_map["knowledge"].func_describe_en + "\n";}
            if(positron_checkbox->isChecked()){extra_prompt_ += tool_map["positron"].func_describe_en + "\n";}
            if(llm_checkbox->isChecked()){extra_prompt_ += tool_map["llm"].func_describe_en + "\n";}
            //中
            extra_prompt_ +=wordsObj["middle_extra_prompt_en"].toString();
            if(calculator_checkbox->isChecked()){extra_prompt_ += "\"calculator\" ";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += "\"cmd\" ";}
            if(search_checkbox->isChecked()){extra_prompt_ += "\"search\" ";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ +="\"knowledge\" ";}
            if(positron_checkbox->isChecked()){extra_prompt_ +="\"positron\" ";}
            if(llm_checkbox->isChecked()){extra_prompt_ +="\"llm\" ";}
            //尾
            extra_prompt_ += wordsObj["tail_extra_prompt_en"].toString();
        }
        else{extra_prompt_ = "";}
        
    }
    return extra_prompt_;
    
}

//输出解析器，提取JSON
QStringList Widget::matchJSON(QString text)
{
    QStringList func_arg_list;
    // 使用正则表达式来定位JSON部分
    // 使用QRegularExpression查找JSON字符串
    // 正则表达式解释：
    // \s* 可能的空白符
    // \{ 开始的大括号
    // (?:.|\n)*? 非贪婪匹配任意字符包括换行符
    // \} 结束的大括号
    QRegularExpression re("\\{(?:.|\\s)*?\\}");
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch()) {
        // 提取JSON字符串
        QString jsonString = match.captured(0);
        // 将JSON字符串转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8());
        // 检查解析是否成功
        if (!jsonDoc.isNull()) {
            if (jsonDoc.isObject()) {
                // 获取QJsonObject并操作它
                QJsonObject jsonObj = jsonDoc.object();
                QString action = jsonObj["action"].toString();
                
                QString action_input;
                // 检查action_input的类型并相应处理
                QJsonValue actionInputValue = jsonObj["action_input"];
                if (actionInputValue.isString()) 
                {
                    // 如果是字符串，则获取字符串
                    action_input = actionInputValue.toString();
                    
                } 
                else if (actionInputValue.isDouble()) {
                    // 如果是数字，则获取数字
                    action_input = QString::number(actionInputValue.toDouble());
                }
                func_arg_list << action;
                func_arg_list << action_input;
                qDebug() << "action:" << action<< "action_input:" << action_input;
                reflash_state("ui:" + wordsObj["json detect"].toString() + " action:" + action + " action_input:" + action_input,0);
            } else {
                reflash_state("ui:" + wordsObj["no json detect"].toString() + " JSON document is not an object",0);
            }
        } else {
            reflash_state("ui:" + wordsObj["no json detect"].toString() + " Invalid JSON...",0);
        }
    } else {
        reflash_state("ui:" + wordsObj["no json detect"].toString(),0);
    }
    return func_arg_list;
}

// 按日志显示装载进度
void Widget::load_log_play()
{
    int load_count = load_percent*all_fps/100;
    //qDebug() << load_count;
    while(load_count>load_action && ui_SETTINGS.ngl!=0)
    {
        load_move();
    }
    
}
double Widget::CalculateCPULoad()
{
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        // 获取系统时间失败
        return -1;
    }

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;

    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;

    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    // Convert previous FILETIME values to ULARGE_INTEGER.
    ULARGE_INTEGER prevIdle, prevKernel, prevUser;
    prevIdle.LowPart = preidleTime.dwLowDateTime;
    prevIdle.HighPart = preidleTime.dwHighDateTime;

    prevKernel.LowPart = prekernelTime.dwLowDateTime;
    prevKernel.HighPart = prekernelTime.dwHighDateTime;

    prevUser.LowPart = preuserTime.dwLowDateTime;
    prevUser.HighPart = preuserTime.dwHighDateTime;

    // Calculate the differences between the previous and current times.
    ULARGE_INTEGER sysIdle, sysKernel, sysUser;
    sysIdle.QuadPart = idle.QuadPart - prevIdle.QuadPart;
    sysKernel.QuadPart = kernel.QuadPart - prevKernel.QuadPart;
    sysUser.QuadPart = user.QuadPart - prevUser.QuadPart;

    // Update the stored times for the next calculation.
    preidleTime = idleTime;
    prekernelTime = kernelTime;
    preuserTime = userTime;

    // Avoid division by zero.
    if (sysKernel.QuadPart + sysUser.QuadPart == 0) {
        return 0;
    }

    // Calculate the CPU load as a percentage.
    return (sysKernel.QuadPart + sysUser.QuadPart - sysIdle.QuadPart) * 100.0 / (sysKernel.QuadPart + sysUser.QuadPart);
}


//更新cpu内存使用率
void Widget::updateStatus()
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
    double physMemUsedPercent = (physMemUsed * 100.0) / totalPhysMem;// 计算内存使用率
    double cpuLoad = CalculateCPULoad();// 计算cpu使用率
    ui->cpu_bar->setValue(cpuLoad);
    //取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if(is_first_getmem){first_memp = physMemUsedPercent;ui->mem_bar->setValue(first_memp);is_first_getmem=false;}
    ui->mem_bar->setSecondValue(physMemUsedPercent - first_memp);
    //ui->mem_bar->setValue(physMemUsedPercent-(model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    //ui->mem_bar->setSecondValue((model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    
}
//拯救中文
void Widget::getWords()
{
    QFile jfile(":/chinese.json");
    if (!jfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for reading.";
        return;
    }

    QTextStream in(&jfile);
    in.setCodec("UTF-8"); // 确保使用UTF-8编码读取文件
    QString data = in.readAll();
    jfile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = doc.object();
    wordsObj = jsonObj["words"].toObject();
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

//应用api设置
void Widget::set_api()
{
    if(api_ip_LineEdit->text().contains("0.0") || api_ip_LineEdit->text().split(".").size()<3 || api_ip_LineEdit->text() == "0.0.0.0" || api_port_LineEdit->text()==""){ui_state = "ui:api wrong";reflash_state(ui_state,2);return;}
    ui_state = "ui:"+wordsObj["detecting"].toString()+"api...";reflash_state(ui_state,3);
    emit ui2bot_free();is_load = false;
    if(ui_mode == 0){ui->output->clear();}
    apis.api_ip = api_ip_LineEdit->text();
    apis.api_port = api_port_LineEdit->text();
    apis.api_chat_endpoint = api_chat_LineEdit->text();
    apis.api_complete_endpoint = api_complete_LineEdit->text();
    apis.is_cache = api_is_cache->isChecked();
    startConnection(apis.api_ip,apis.api_port.toInt());//检测ip是否通畅
    api_dialog->setDisabled(1);
}

void Widget::startConnection(const QString &ip, int port) {
    // socket should be a member variable or should be managed to ensure its lifetime
    // during the asynchronous operation
    QTcpSocket *socket = new QTcpSocket(this); 
    connect(socket, &QTcpSocket::connected, this, &Widget::onConnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &Widget::onError);
    socket->connectToHost(ip, port);
    
}

void Widget::keepConnection()
{
    //if(keeptimer->isActive()){keeptimer->stop();}//控制超时处理函数只会处理一次
    keeptime.restart();
    QTcpSocket *socket = new QTcpSocket(this); 
    connect(socket, &QTcpSocket::connected, this, &Widget::keep_onConnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &Widget::keep_onError);
    socket->connectToHost(apis.api_ip, apis.api_port.toInt());
    
}

void Widget::api_addhelpinput()
{
    ui_user_history << wordsObj["H1"].toString();
    ui_assistant_history << wordsObj["A1"].toString();
    ui_user_history << wordsObj["H2"].toString();
    ui_assistant_history << wordsObj["A2"].toString();
    reflash_output("\n" + ui_DATES.input_pfx + ":\n" + wordsObj["H1"].toString() + "\n" + ui_DATES.input_sfx + ":\n" + wordsObj["A1"].toString(),0,Qt::black);
    reflash_output("\n" + ui_DATES.input_pfx + ":\n" + wordsObj["H2"].toString() + "\n" + ui_DATES.input_sfx + ":\n" + wordsObj["A2"].toString(),0,Qt::black);
}

// 连接成功
void Widget::onConnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket) {
        socket->disconnectFromHost();
        // Handle successful connection
    }
    is_api = true;
    reflash_state("ui:" + wordsObj["eva link"].toString(),4);
    if(ui_mode == 0){current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_chat_endpoint;}
    else{current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_complete_endpoint;}
    ui_state = "ui:"+wordsObj["current api"].toString() + " " + current_api;reflash_state(ui_state,0);
    this->setWindowTitle(wordsObj["current api"].toString() + " " + current_api);
    QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
    ui->kv_bar->message = wordsObj["delay"].toString();ui->kv_bar->setToolTip("");
    
    emit ui2net_apis(apis);
    reflash_output(ui_DATES.system_prompt,0,Qt::black);
    ui->date->setEnabled(1);ui->set->setEnabled(1);
    ui->reset->setEnabled(1);
    ui->send->setEnabled(1);
    api_dialog->setDisabled(0);
    api_dialog->close();

    keeptimer->start(3000);//每多少秒测一次延迟，频率太高会让服务端爆炸
}
//连接失败
void Widget::onError(QAbstractSocket::SocketError socketError) {
    // Handle the error
    is_api = false;
    ui_state = "ui:api"+wordsObj["port"].toString()+wordsObj["blocked"].toString();reflash_state(ui_state,2);
    this->setWindowTitle(wordsObj["eva"].toString());
    ui->date->setEnabled(0);ui->set->setEnabled(0);
    ui->reset->setEnabled(0);
    ui->send->setEnabled(0);
    api_dialog->setDisabled(0);
    api_dialog->close();
}
void Widget::send_testhandleTimeout()
{
    on_send_clicked();
}
//每多少秒测一次延迟,回应时间/keeptest*100为延迟量
void Widget::keep_onConnected()
{
    float percent = keeptime.nsecsElapsed()/1000000000.0 / keeptesttime;
    //qDebug() << keeptime.nsecsElapsed()/1000000000.0<<keeptesttime<<percent;
    if(percent < 1 && percent >0){percent = 1;}
    ui->kv_bar->setSecondValue(percent);
}
//每多少秒测一次延迟,回应时间/keeptest*100为延迟量
void Widget::keep_onError(QAbstractSocket::SocketError socketError)
{
    //qDebug() << socketError;
    if(socketError!=QAbstractSocket::RemoteHostClosedError){ui->kv_bar->setSecondValue(100);}
    
}
