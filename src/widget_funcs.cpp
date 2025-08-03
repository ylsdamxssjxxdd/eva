//功能函数
#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//----------------------------------装载相关--------------------------------
//-------------------------------------------------------------------------
//动画播放逻辑
// 1.先展示背景load_play()
// 2.启动load_begin_pTimer动画,向中滑动
// 2.启动load_pTimer动画,连接动画
// 3.启动load_over_pTimer动画,向下滑动
// 4.启动force_unlockload_pTimer,强制解锁
// 5.真正装载完的处理unlockLoad()

// 初始化动画,主要是背景和12条连线
void Widget::init_movie() {
    movie_line << "                        ##                        ";  // 2
    movie_line << "                      #    #                      ";  // 3
    movie_line << "                        ##                        ";  // 4
    movie_line << "     ##                                    ##     ";  // 5
    movie_line << "   #    #                                #    #   ";  // 6
    movie_line << "     ##                                    ##     ";  // 7
    movie_line << "                        ##                        ";  // 8
    movie_line << "                      #    #                      ";  // 9
    movie_line << "                        ##                        ";  // 10
    movie_line << "     ##                                    ##     ";  // 11
    movie_line << "   #    #                                #    #   ";  // 12
    movie_line << "     ##                                    ##     ";  // 13
    movie_line << "                        ##                        ";  // 14
    movie_line << "                      #    #                      ";  // 15
    movie_line << "                        ##                        ";  // 16

    movie_dot << QPointF(5, 12) << QPointF(4, 15) << QPointF(3, 19) << QPointF(2, 22);                                                                                                                   // load_action 1
    movie_dot << QPointF(5, 12) << QPointF(6, 15) << QPointF(7, 19) << QPointF(8, 22);                                                                                                                   // load_action 2
    movie_dot << QPointF(5, 12) << QPointF(6, 13) << QPointF(7, 14) << QPointF(8, 15) << QPointF(9, 16) << QPointF(10, 17) << QPointF(11, 18) << QPointF(12, 19) << QPointF(13, 20) << QPointF(14, 22);  // load_action 3

    movie_dot << QPointF(11, 12) << QPointF(10, 13) << QPointF(9, 14) << QPointF(8, 15) << QPointF(7, 16) << QPointF(6, 17) << QPointF(5, 18) << QPointF(4, 19) << QPointF(3, 20) << QPointF(2, 22);  // load_action 4
    movie_dot << QPointF(11, 12) << QPointF(10, 15) << QPointF(9, 19) << QPointF(8, 22);                                                                                                              // load_action 5
    movie_dot << QPointF(11, 12) << QPointF(12, 15) << QPointF(13, 19) << QPointF(14, 22);                                                                                                            // load_action 6

    movie_dot << QPointF(2, 31) << QPointF(3, 34) << QPointF(4, 38) << QPointF(5, 41);                                                                                                                // load_action 7
    movie_dot << QPointF(2, 31) << QPointF(3, 33) << QPointF(4, 34) << QPointF(5, 35) << QPointF(6, 36) << QPointF(7, 37) << QPointF(8, 38) << QPointF(9, 39) << QPointF(10, 40) << QPointF(11, 41);  // load_action 8

    movie_dot << QPointF(8, 31) << QPointF(7, 34) << QPointF(6, 38) << QPointF(5, 41);    // load_action 9
    movie_dot << QPointF(8, 31) << QPointF(9, 34) << QPointF(10, 38) << QPointF(11, 41);  // load_action 10

    movie_dot << QPointF(14, 31) << QPointF(13, 33) << QPointF(12, 34) << QPointF(11, 35) << QPointF(10, 36) << QPointF(9, 37) << QPointF(8, 38) << QPointF(7, 39) << QPointF(6, 40) << QPointF(5, 41);  // load_action 11
    movie_dot << QPointF(14, 31) << QPointF(13, 34) << QPointF(12, 38) << QPointF(11, 41);                                                                                                               // load_action 12

    //添加颜色
    for (int i = 0; i < 12; ++i) {
        //彩色
        // movie_color <<QColor(QRandomGenerator::global()->bounded(256), QRandomGenerator::global()->bounded(256), QRandomGenerator::global()->bounded(256));
        //黑色
        movie_color << NORMAL_BLACK;
    }

    //设置动画内容字体格式
    movie_format.setFontWeight(QFont::Bold);  // 设置粗体
    movie_font.setPointSize(6);
    // movie_font.setFamily(DEFAULT_FONT);
    movie_format.setFont(movie_font);

    load_pTimer = new QTimer(this);                                                         //连接接动画
    load_begin_pTimer = new QTimer(this);                                                   //向中滑动
    load_over_pTimer = new QTimer(this);                                                    //向下滑动
    force_unlockload_pTimer = new QTimer(this);                                             //强制解锁
    connect(load_pTimer, SIGNAL(timeout()), this, SLOT(load_handleTimeout()));              //设置终止信号触发的槽函数
    connect(load_begin_pTimer, SIGNAL(timeout()), this, SLOT(load_begin_handleTimeout()));  //设置终止信号触发的槽函数
    connect(load_over_pTimer, SIGNAL(timeout()), this, SLOT(load_over_handleTimeout()));    //设置终止信号触发的槽函数
    connect(force_unlockload_pTimer, SIGNAL(timeout()), this, SLOT(unlockLoad()));          //新开一个线程

    decode_pTimer = new QTimer(this);  //启动后,达到规定时间将发射终止信号
    connect(decode_pTimer, SIGNAL(timeout()), this, SLOT(decode_handleTimeout()));  //设置终止信号触发的槽函数
}

//设置72个点的字体前景色颜色
void Widget::set_dotcolor(QTextCharFormat *format, int load_action) {
    if (load_action < 4) {
        format->setForeground(movie_color.at(0));
    } else if (load_action < 8) {
        format->setForeground(movie_color.at(1));
    } else if (load_action < 18) {
        format->setForeground(movie_color.at(2));
    } else if (load_action < 28) {
        format->setForeground(movie_color.at(3));
    } else if (load_action < 32) {
        format->setForeground(movie_color.at(4));
    } else if (load_action < 36) {
        format->setForeground(movie_color.at(5));
    } else if (load_action < 40) {
        format->setForeground(movie_color.at(6));
    } else if (load_action < 50) {
        format->setForeground(movie_color.at(7));
    } else if (load_action < 54) {
        format->setForeground(movie_color.at(8));
    } else if (load_action < 58) {
        format->setForeground(movie_color.at(9));
    } else if (load_action < 68) {
        format->setForeground(movie_color.at(10));
    } else if (load_action < 72) {
        format->setForeground(movie_color.at(11));
    }
}

//连接动画的下一帧
void Widget::load_move() {
    QTextCursor cursor = ui->state->textCursor();

    if (load_action % 2 == 0) {
        cursor.movePosition(QTextCursor::Start);                                                                                  //移到文本开头
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2).x() - 2 + playlineNumber);  //向下移动到指定行
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2).y() - 1);                  //向右移动到指定列
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);                                                          //选中当前字符
        cursor.removeSelectedText();                                                                                              //删除选中字符
        set_dotcolor(&movie_format, load_action / 2);                                                                             //设置字体颜色
        cursor.setCharFormat(movie_format);                                                                                       //设置字体
        cursor.insertText("*");                                                                                                   //插入字符
    } else {
        cursor.movePosition(QTextCursor::Start);                                                                                      //移到文本开头
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2 + 1).x() - 2 + playlineNumber);  //向下移动到指定行
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2 + 1).y() - 1);                  //向右移动到指定列
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);                                                              //选中当前字符
        cursor.removeSelectedText();                                                                                                  //删除选中字符
        cursor.setCharFormat(movie_format);                                                                                           //设置字体
        cursor.insertText(" ");                                                                                                       //插入字符
    }

    load_action++;
}

//开始播放
void Widget::load_play() {
    QTextCursor cursor = ui->state->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n");  //插个回车

    load_action = 0;

    //获取当前行数
    playlineNumber = 0;
    QTextDocument *document = ui->state->document();
    for (QTextBlock block = document->begin(); block != document->end(); block = block.next()) {
        ++playlineNumber;
    }
    // qDebug() << "lineNumber: " << playlineNumber;

    //展示背景
    for (int i = 0; i < movie_line.size(); ++i) {
        cursor.movePosition(QTextCursor::End);       //移到文本开头
        cursor.setCharFormat(movie_format);          //设置字体
        cursor.insertText(movie_line.at(i) + "\n");  //插入字符
    }

    //向下滑
    load_begin_pTimer->start(100);
    //先自动播放动画
    load_pTimer->start(800);
}

//连接动画
void Widget::load_handleTimeout() {
    if (load_pTimer->isActive()) {
        load_pTimer->stop();
    }  //控制超时处理函数只会处理一次
    if (load_action < all_fps) {
        load_move();  //下一帧
    }

    //循环播放
    if (load_action < all_fps) {
        if (is_load) {
            load_pTimer->start(10);  //延时多少ms后发出timeout()信号
        } else {
            load_pTimer->start(1100);  //延时多少ms后发出timeout()信号
        }
    } else if (is_load) {
        load_action = 0;  //重置动作计数
        all_fps--;        //减去补上的最后一帧
        load_over_pTimer->start(100);
    }
}

//滑动到最佳动画位置
void Widget::load_begin_handleTimeout() {
    if (load_begin_pTimer->isActive()) {
        load_begin_pTimer->stop();
    }                                                            //控制超时处理函数只会处理一次
    int currentValue = ui->state->verticalScrollBar()->value();  //当前滑动条位置
    ui->state->verticalScrollBar()->setValue(currentValue + 1);
    // qDebug() << currentValue <<playlineNumber;
    if (currentValue < playlineNumber - 2) {
        load_begin_pTimer->start(100);
    } else {
        load_begin_pTimer->stop();
    }
}

//模型装载完毕动画,并解锁按钮
void Widget::load_over_handleTimeout() {
    if (load_over_pTimer->isActive()) {
        load_over_pTimer->stop();
    }  //控制超时处理函数只会处理一次

    int currentValue = ui->state->verticalScrollBar()->value();  //当前滑动条位置
    ui->state->verticalScrollBar()->setValue(currentValue + 1);
    currentValue++;
    //展示完就停止
    if (currentValue <= ui->state->verticalScrollBar()->maximum()) {
        load_over_pTimer->start(100);
    }
    //滚到最下面才解锁按钮,真正装载完毕
    else {
        force_unlockload_pTimer->start(0);  //强制解锁
    }

    if(ui_monitor_frame>0 && ui_state == CHAT_STATE)
    {
        qDebug()<<"开始监视..."<<ui_monitor_frame;
        monitor_timer.start(1000/ui_monitor_frame);
    }
    else
    {
        monitor_timer.stop();
    }
}

// 装载完毕强制预处理
void Widget::unlockLoad() {
    if (ui_SETTINGS.ngl < ui_maxngl) {
        reflash_state("ui:" + jtr("ngl tips"), USUAL_SIGNAL);
    }

    reflash_state("ui:" + jtr("load model") + jtr("over") + " " + QString::number(load_time, 'f', 2) + " s " + jtr("right click and check model log"), SUCCESS_SIGNAL);
    if (ui_SETTINGS.ngl > 0) {
        EVA_icon = QIcon(":/logo/green_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }  // 设置应用程序图标
    else {
        EVA_icon = QIcon(":/logo/blue_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }  // 设置应用程序图标
    EVA_title = jtr("current model") + " " + ui_SETTINGS.modelpath.split("/").last();
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    ui->kv_bar->show_text = jtr("brain");
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(max_thread));
    auto_save_user();  //保存ui配置
    force_unlockload_pTimer->stop();
    is_load_play_over = true;  //标记模型动画已经完成
    ui_state_normal();         //解锁界面
    reflash_output(bot_predecode_content, 0, SYSTEM_BLUE);
    ;  //显示预解码内容
}

// 按日志显示装载进度
void Widget::load_log_play() {
    int load_count = load_percent * all_fps / 100;
    // qDebug() << load_count;
    while (load_count > load_action && ui_SETTINGS.ngl != 0) {
        load_move();
    }
}

//-------------------------------------------------------------------------
//------------------------------文字输出相关--------------------------------
//-------------------------------------------------------------------------
// output和state采用verticalScrollBar()控制滑动条,如果其在底部,有新内容加入将自动下滑,用户上滑后下滑效果取消

//更新输出区,is_while表示从流式输出的token
void Widget::reflash_output(const QString result, bool is_while, QColor color) {
    output_scroll(result, color);
    if (is_while) {
        temp_assistant_history += result;
    }
}

//输出区滚动条事件响应
void Widget::output_scrollBarValueChanged(int value) {
    //如果滑动条在最下面则自动滚动
    int maximumValue = output_scrollBar->maximum();
    if (value == maximumValue) {
        is_stop_output_scroll = 0;
    } else {
        is_stop_output_scroll = 1;
    }
}

//向output末尾添加文本并滚动
void Widget::output_scroll(QString output, QColor color) {
    QTextCursor cursor = ui->output->textCursor();
    QTextCharFormat textFormat;

    textFormat.setForeground(QBrush(color));  // 设置文本颜色
    cursor.movePosition(QTextCursor::End);    //光标移动到末尾
    cursor.mergeCharFormat(textFormat);       // 应用文本格式

    cursor.insertText(output);  //输出

    QTextCharFormat textFormat0;            // 清空文本格式
    cursor.movePosition(QTextCursor::End);  //光标移动到末尾
    cursor.mergeCharFormat(textFormat0);    // 应用文本格式

    if (!is_stop_output_scroll)  //如果停止标签没有启用,则每次输出完自动滚动到最下面
    {
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum());  //滚动条滚动到最下面
    }
}

//更新状态区
void Widget::reflash_state(QString state_string, SIGNAL_STATE state) {
    QTextCharFormat format;  //设置特殊文本颜色
    // QFont font;//字体 设置了字体就不能缩放了
    // font.setPointSize(9);
    // format.setFont(font);
    //过滤回车和换行符
    if (state != MATRIX_SIGNAL) {
        state_string.replace("\n", "\\n");
        state_string.replace("\r", "\\r");
    }

    if (state == USUAL_SIGNAL || state == MATRIX_SIGNAL)  //一般黑色
    {
        format.clearForeground();                 //清除前景颜色
        format.setForeground(NORMAL_BLACK);       //还是黑色吧
        ui->state->setCurrentCharFormat(format);  //设置光标格式
        ui->state->appendPlainText(state_string);
    } else if (state == SUCCESS_SIGNAL)  //正常绿色
    {
        format.setForeground(QColor(0, 200, 0));  // 设置前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式

        ui->state->appendPlainText(state_string);
        format.clearForeground();                 //清除前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
    } else if (state == WRONG_SIGNAL)             //不正常红色
    {
        format.setForeground(QColor(200, 0, 0));  // 设置前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                 //清除前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
    } else if (state == SIGNAL_SIGNAL)            //信号蓝色
    {
        format.setForeground(QColor(0, 0, 200));  // 蓝色设置前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                 //清除前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
    } else if (state == EVA_SIGNAL)               //行为警告
    {
        QFont font = format.font();
        // font.setFamily(DEFAULT_FONT);
        // font.setLetterSpacing(QFont::AbsoluteSpacing, 0); // 设置字母间的绝对间距
        font.setPixelSize(14);
        format.setFont(font);
        format.setFontItalic(true);  // 设置斜体
        // format.setForeground(QColor(128,0,128));    // 紫色设置前景颜色
        format.setForeground(NORMAL_BLACK);       //还是黑色吧
        ui->state->setCurrentCharFormat(format);  //设置光标格式
        //■■■■■■■■■■■■■■
        ui->state->appendPlainText(jtr("cubes"));  //显示

        //中间内容
        format.setFontItalic(false);                              // 取消斜体
        format.setFontWeight(QFont::Black);                       // 设置粗体
        ui->state->setCurrentCharFormat(format);                  //设置光标格式
        ui->state->appendPlainText("          " + state_string);  //显示

        //■■■■■■■■■■■■■■
        format.setFontItalic(true);                // 设置斜体
        format.setFontWeight(QFont::Normal);       // 取消粗体
        ui->state->setCurrentCharFormat(format);   //设置光标格式
        ui->state->appendPlainText(jtr("cubes"));  //显示

        format.setFontWeight(QFont::Normal);      // 取消粗体
        format.setFontItalic(false);              // 取消斜体
        format.clearForeground();                 //清除前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
    } else if (state == TOOL_SIGNAL)              //工具天蓝色
    {
        format.setForeground(TOOL_BLUE);          //天蓝色设置前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                 //清除前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
    } else if (state == SYNC_SIGNAL)              //同步橘黄色
    {
        format.setForeground(LCL_ORANGE);         //天蓝色设置前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                 //清除前景颜色
        ui->state->setCurrentCharFormat(format);  //设置光标格式
    }
}

//-------------------------------------------------------------------------
//-------------------------------响应槽相关---------------------------------
//-------------------------------------------------------------------------

//温度滑块响应
void Widget::temp_change() { settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0)); }
// ngl滑块响应
void Widget::ngl_change() { settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(settings_ui->ngl_slider->value())); }
// nctx滑块响应
void Widget::nctx_change() { settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(settings_ui->nctx_slider->value())); }
// repeat滑块响应
void Widget::repeat_change() { settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0)); }

void Widget::nthread_change() { settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(settings_ui->nthread_slider->value())); }

//补完状态按钮响应
void Widget::complete_change() {
    //选中则禁止约定输入
    if (settings_ui->complete_btn->isChecked()) {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nthread_slider->setEnabled(1);
        settings_ui->nctx_slider->setEnabled(1);

        settings_ui->port_lineEdit->setEnabled(0);
    }
}

//对话状态按钮响应
void Widget::chat_change() {
    if (settings_ui->chat_btn->isChecked()) {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nctx_slider->setEnabled(1);
        settings_ui->nthread_slider->setEnabled(1);

        settings_ui->port_lineEdit->setEnabled(0);
    }
}

//服务状态按钮响应
void Widget::web_change() {
    if (settings_ui->web_btn->isChecked()) {
        settings_ui->sample_box->setEnabled(0);

        settings_ui->port_lineEdit->setEnabled(1);
    }
}

//提示词模板下拉框响应
void Widget::prompt_template_change() {
    if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set1")) {
        date_ui->date_prompt_TextEdit->setEnabled(1);
        date_ui->user_name_LineEdit->setEnabled(1);
        date_ui->model_name_LineEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom1_date_system);
        date_ui->user_name_LineEdit->setText(custom1_user_name);
        date_ui->model_name_LineEdit->setText(custom1_model_name);
    } else if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set2")) {
        date_ui->date_prompt_TextEdit->setEnabled(1);
        date_ui->user_name_LineEdit->setEnabled(1);
        date_ui->model_name_LineEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom2_date_system);
        date_ui->user_name_LineEdit->setText(custom2_user_name);
        date_ui->model_name_LineEdit->setText(custom2_model_name);
    } else {
        date_ui->date_prompt_TextEdit->setPlainText(date_map[date_ui->chattemplate_comboBox->currentText()].date_prompt);
        date_ui->date_prompt_TextEdit->setEnabled(0);
        date_ui->user_name_LineEdit->setText(date_map[date_ui->chattemplate_comboBox->currentText()].user_name);
        date_ui->user_name_LineEdit->setEnabled(0);
        date_ui->model_name_LineEdit->setText(date_map[date_ui->chattemplate_comboBox->currentText()].model_name);
        date_ui->model_name_LineEdit->setEnabled(0);
    }
}

void Widget::chooseLorapath() {
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose lora model"), "(*.bin *.gguf)");

    settings_ui->lora_LineEdit->setText(currentpath);
}

void Widget::chooseMmprojpath() {
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose mmproj model"), "(*.bin *.gguf)");

    settings_ui->mmproj_LineEdit->setText(currentpath);
}

//响应工具选择
void Widget::tool_change() {
    QObject* senderObj = sender(); // gets the object that sent the signal
    
    // 如果是软件工程师则查询python环境
    if (QCheckBox* checkbox = qobject_cast<QCheckBox*>(senderObj)) {
        if (checkbox == date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked()) {
                python_env = checkPython();
                compile_env = checkCompile();
            } 
    }

    // 判断是否挂载了工具
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
    ui_extra_prompt = create_extra_prompt();
}

//-------------------------------------------------------------------------
//--------------------------------设置选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::set_SetDialog() {
    settings_dialog = new QDialog(this);
    settings_dialog->setWindowFlags(settings_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);  //隐藏?按钮
    // settings_dialog->setWindowFlags(settings_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);// 隐藏关闭按钮
    settings_ui = new Ui::Settings_Dialog_Ui;
    settings_ui->setupUi(settings_dialog);

    //温度控制
    settings_ui->temp_slider->setRange(0, 100);  // 设置范围为1到99
    settings_ui->temp_slider->setValue(ui_SETTINGS.temp * 100.0);
    connect(settings_ui->temp_slider, &QSlider::valueChanged, this, &Widget::temp_change);
    //重复惩罚控制
    settings_ui->repeat_slider->setRange(0, 200);  // 设置范围
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
    settings_ui->lora_LineEdit->setContextMenuPolicy(Qt::NoContextMenu);  //取消右键菜单
    settings_ui->lora_LineEdit->installEventFilter(this);
    // load mmproj
    settings_ui->mmproj_LineEdit->setContextMenuPolicy(Qt::NoContextMenu);  //取消右键菜单
    settings_ui->mmproj_LineEdit->installEventFilter(this);
    //补完控制
    connect(settings_ui->complete_btn, &QRadioButton::clicked, this, &Widget::complete_change);
    //多轮对话
    settings_ui->chat_btn->setChecked(1);
    connect(settings_ui->chat_btn, &QRadioButton::clicked, this, &Widget::chat_change);
    //网页服务控制
    QHBoxLayout *layout_H10 = new QHBoxLayout();  //水平布局器
    settings_ui->port_lineEdit->setText(ui_port);
    QIntValidator *validator = new QIntValidator(0, 65535);  //限制端口输入
    settings_ui->port_lineEdit->setValidator(validator);
    connect(settings_ui->web_btn, &QRadioButton::clicked, this, &Widget::web_change);
    //监视帧率设置
    settings_ui->frame_lineEdit->setValidator(new QDoubleValidator(0.0, 1000.0, 8, this));// 只允许输入数字

    connect(settings_ui->confirm, &QPushButton::clicked, this, &Widget::settings_ui_confirm_button_clicked);
    connect(settings_ui->cancel, &QPushButton::clicked, this, &Widget::settings_ui_cancel_button_clicked);

    settings_dialog->setWindowTitle(jtr("set"));
}

// 设置选项卡确认按钮响应
void Widget::settings_ui_confirm_button_clicked() {
    settings_dialog->close();
    set_set();
}

// 设置选项卡取消按钮响应
void Widget::settings_ui_cancel_button_clicked() {
    settings_dialog->close();
    if(!is_load)//如果没有装载模型则装载
    {
        set_set();
    }
}

// 设置用户设置内容
void Widget::set_set() {
    EVA_STATE current_ui_state = ui_state;  //上一次机体的状态
    get_set();  //获取设置中的纸面值

    //如果不是对话模式则禁用约定
    if (ui_state != CHAT_STATE) {
        date_ui->prompt_box->setEnabled(0);
        date_ui->tool_box->setEnabled(0);
    } else {
        date_ui->prompt_box->setEnabled(1);
        date_ui->tool_box->setEnabled(1);
    }

    //从补完模式回来强行预解码
    if(current_ui_state == COMPLETE_STATE && ui_state == CHAT_STATE)
    {
        emit ui2bot_preDecode();
    }

    //发送设置参数给模型
    if(ui_state != SERVER_STATE && ui_mode != LINK_MODE){emit ui2bot_set(ui_SETTINGS, 1);}
    
    // llama-server接管,不需要告知bot约定
    if (ui_state == SERVER_STATE) {
        serverControl();
    } else {
        if (ui_mode == LINK_MODE)  //链接模式不发信号
        {
            on_reset_clicked();
        }
    }
}


//-------------------------------------------------------------------------
//--------------------------------约定选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::set_DateDialog() {
    //初始化约定窗口
    date_dialog = new QDialog(this);
    date_dialog->setWindowTitle(jtr("date"));
    date_dialog->setWindowFlags(date_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);  //隐藏?按钮
    date_ui = new Ui::Date_Dialog_Ui;
    date_ui->setupUi(date_dialog);
    for (const QString &key : date_map.keys()) {
        date_ui->chattemplate_comboBox->addItem(key);
    }
    date_ui->chattemplate_comboBox->addItem(jtr("custom set1"));  //添加自定义模板
    date_ui->chattemplate_comboBox->addItem(jtr("custom set2"));  //添加自定义模板
    date_ui->chattemplate_comboBox->setCurrentText(ui_template);  //默认使用default的提示词模板
    connect(date_ui->chattemplate_comboBox, &QComboBox::currentTextChanged, this, &Widget::prompt_template_change);
    connect(date_ui->confirm_button, &QPushButton::clicked, this, &Widget::date_ui_confirm_button_clicked);
    connect(date_ui->cancel_button, &QPushButton::clicked, this, &Widget::date_ui_cancel_button_clicked);
    connect(date_ui->switch_lan_button, &QPushButton::clicked, this, &Widget::switch_lan_change);
    connect(date_ui->knowledge_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); //点击工具响应
    connect(date_ui->stablediffusion_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); //点击工具响应
    connect(date_ui->calculator_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); //点击工具响应
    connect(date_ui->controller_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); //点击工具响应
    connect(date_ui->MCPtools_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); //点击工具响应
    connect(date_ui->engineer_checkbox, &QCheckBox::stateChanged, this, &Widget::tool_change); //点击工具响应

    if (language_flag == 0) {
        ui_extra_lan = "zh";
    }
    if (language_flag == 1) {
        ui_extra_lan = "en";
    }

    prompt_template_change();  //先应用提示词模板
}

// 约定选项卡确认按钮响应
void Widget::date_ui_confirm_button_clicked() {
    date_dialog->close();
    set_date();
}

// 约定选项卡取消按钮响应
void Widget::date_ui_cancel_button_clicked() {
    date_dialog->close();
    cancel_date();
}

//-------------------------------------------------------------------------
//--------------------------------api选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::setApiDialog() {
    api_dialog = new QDialog();
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);  //隐藏?按钮
    // api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);        //隐藏关闭按钮
    api_dialog->resize(400, 100);                                                              // 设置宽度,高度

    QVBoxLayout *layout = new QVBoxLayout(api_dialog);  //垂直布局器
    // api_endpoint
    QHBoxLayout *layout_H1 = new QHBoxLayout();  //水平布局器
    api_endpoint_label = new QLabel(jtr("api endpoint"),this);
    api_endpoint_label->setFixedWidth(80);
    layout_H1->addWidget(api_endpoint_label);
    api_endpoint_LineEdit = new QLineEdit(this);
    api_endpoint_LineEdit->setPlaceholderText(jtr("input api endpoint"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_endpoint_LineEdit->setText(apis.api_endpoint);
    layout_H1->addWidget(api_endpoint_LineEdit);
    layout->addLayout(layout_H1);  //将布局添加到总布局
    // api_key
    QHBoxLayout *layout_H2 = new QHBoxLayout();  //水平布局器
    api_key_label = new QLabel(jtr("api key"),this);
    api_key_label->setFixedWidth(80);
    layout_H2->addWidget(api_key_label);
    api_key_LineEdit = new QLineEdit(this);
    api_key_LineEdit->setEchoMode(QLineEdit::Password);
    api_key_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_key_LineEdit->setToolTip(jtr("input api key"));
    api_key_LineEdit->setText(apis.api_key);
    layout_H2->addWidget(api_key_LineEdit);
    layout->addLayout(layout_H2);  //将布局添加到总布局
    // api_model
    QHBoxLayout *layout_H3 = new QHBoxLayout();  //水平布局器
    api_model_label = new QLabel(jtr("api model"),this);
    api_model_label->setFixedWidth(80);
    layout_H3->addWidget(api_model_label);
    api_model_LineEdit = new QLineEdit(this);
    api_model_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_model_LineEdit->setToolTip(jtr("input api model"));
    api_model_LineEdit->setText(apis.api_model);
    layout_H3->addWidget(api_model_LineEdit);
    layout->addLayout(layout_H3);  //将布局添加到总布局

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, api_dialog);  // 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_api);
    connect(buttonBox, &QDialogButtonBox::accepted, api_dialog, &QDialog::reject);// 点击确定后直接退出
    connect(buttonBox, &QDialogButtonBox::rejected, api_dialog, &QDialog::reject);
}

//-------------------------------------------------------------------------
//----------------------------------编码动画--------------------------------
//-------------------------------------------------------------------------

void Widget::decode_play() {
    decode_pTimer->start(100);  //延时多少ms后发出timeout()信号
}
void Widget::decode_move() {
    int decode_LineNumber = ui->state->document()->blockCount() - 1;  // 获取最后一行的行数
    //如果在新的行数上播放动画,则先删除上一次解码动画的残余部分
    if (currnet_LineNumber != decode_LineNumber) {
        QTextBlock currnet_block = ui->state->document()->findBlockByLineNumber(currnet_LineNumber);  //取上次最后一行
        QTextCursor currnet_cursor(currnet_block);                                                    //取游标
        currnet_cursor.movePosition(QTextCursor::EndOfBlock);                                         //游标移动到末尾
        currnet_cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);                      //选中当前字符
        if (currnet_cursor.selectedText() == "\\" || currnet_cursor.selectedText() == "/" || currnet_cursor.selectedText() == "-" || currnet_cursor.selectedText() == "|") {
            currnet_cursor.removeSelectedText();  //删除选中字符
        }
        currnet_LineNumber = decode_LineNumber;
    }

    QTextBlock block = ui->state->document()->findBlockByLineNumber(decode_LineNumber);  //取最后一行
    QTextCursor cursor(block);                                                           //取游标
    cursor.movePosition(QTextCursor::EndOfBlock);                                        //游标移动到末尾

    //四帧动画
    if (decode_action % 8 == 0) {
        cursor.insertText("|");  //插入字符
    } else if (decode_action % 8 == 2) {
        cursor.insertText("/");  //插入字符
    } else if (decode_action % 8 == 4) {
        cursor.insertText("-");  //插入字符
    } else if (decode_action % 8 == 6) {
        cursor.insertText("\\");  //插入字符
    } else {
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);  //选中当前字符
        if (cursor.selectedText() == "|" || cursor.selectedText() == "\\" || cursor.selectedText() == "/" || cursor.selectedText() == "-") {
            cursor.removeSelectedText();  //删除选中字符
        }
    }

    decode_action++;
}

void Widget::decode_handleTimeout() {
    if (decode_pTimer->isActive()) {
        decode_pTimer->stop();
    }  //控制超时处理函数只会处理一次
    decode_move();
    decode_pTimer->start(100);  //延时多少ms后发出timeout()信号
}

//-------------------------------------------------------------------------
//----------------------------------链接相关--------------------------------
//-------------------------------------------------------------------------

//应用api设置
void Widget::set_api() {
    emit ui2bot_free(0);  //释放原来的模型
    is_load = false;// 重置
    historypath = "";// 重置

    //获取设置值
    apis.api_endpoint = api_endpoint_LineEdit->text();
    apis.api_key = api_key_LineEdit->text();
    apis.api_model = api_model_LineEdit->text();

    ui_mode = LINK_MODE;  //按照链接模式的行为来
    reflash_state("ui:" + jtr("eva link"), EVA_SIGNAL);
    if (ui_state == CHAT_STATE) {
        current_api = apis.api_endpoint + apis.api_chat_endpoint;
    } else {
        current_api = apis.api_endpoint + apis.api_completion_endpoint;
    }
    EVA_title = jtr("current api") + " " + current_api;
    reflash_state("ui:" + EVA_title, USUAL_SIGNAL);
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    EVA_icon = QIcon(":/logo/dark_logo.png");
    QApplication::setWindowIcon(EVA_icon);  //设置应用程序图标
    trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    ui->kv_bar->setToolTip("");

    emit ui2net_apis(apis);
    ui->output->clear();
    reflash_output(ui_DATES.date_prompt, 0, SYSTEM_BLUE);
    //构造系统指令
    QJsonObject systemMessage;
    systemMessage.insert("role", DEFAULT_SYSTEM_NAME);
    systemMessage.insert("content", ui_DATES.date_prompt);
    ui_messagesArray.append(systemMessage);
    ui_state_normal();
    auto_save_user();
}

//链接模式下工具返回结果时延迟发送
void Widget::tool_testhandleTimeout() {
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
    data.repeat = ui_SETTINGS.repeat;
    data.n_predict = ui_SETTINGS.hid_npredict;
    data.messagesArray = ui_messagesArray;

    emit ui2net_data(data);
    emit ui2net_push();
}

void Widget::send_testhandleTimeout() { on_send_clicked(); }

//链接模式切换时某些控件可见状态
void Widget::change_api_dialog(bool enable) {
    settings_ui->repeat_label->setVisible(enable);
    settings_ui->repeat_slider->setVisible(enable);
    settings_ui->nctx_label->setVisible(enable);
    settings_ui->nctx_slider->setVisible(enable);
    settings_ui->nthread_label->setVisible(enable);
    settings_ui->nthread_slider->setVisible(enable);
    settings_ui->mmproj_label->setVisible(enable);
    settings_ui->mmproj_LineEdit->setVisible(enable);
    settings_ui->ngl_label->setVisible(enable);
    settings_ui->ngl_slider->setVisible(enable);
    settings_ui->lora_label->setVisible(enable);
    settings_ui->lora_LineEdit->setVisible(enable);
    settings_ui->port_label->setVisible(enable);
    settings_ui->port_lineEdit->setVisible(enable);
    settings_ui->web_btn->setVisible(enable);
    settings_ui->frame_label->setVisible(enable);
    settings_ui->frame_lineEdit->setVisible(enable);
}

//-------------------------------------------------------------------------
//----------------------------------界面状态--------------------------------
//-------------------------------------------------------------------------
//按钮的可用和可视状态控制
//最终要归到下面的几种状态来

//初始界面状态
void Widget::ui_state_init() {
    ui->load->setEnabled(1);   //装载按钮
    ui->date->setEnabled(0);   //约定按钮
    ui->set->setEnabled(0);    //设置按钮
    ui->reset->setEnabled(0);  //重置按钮
    ui->send->setEnabled(0);   //发送按钮
    ui->output->setReadOnly(1);
}

// 装载中界面状态
void Widget::ui_state_loading() {
    ui->send->setEnabled(0);   //发送按钮
    ui->reset->setEnabled(0);  //重置按钮
    ui->date->setEnabled(0);   //约定按钮
    ui->set->setEnabled(0);    //设置按钮
    ui->load->setEnabled(0);   //装载按钮
    ui->input->textEdit->setFocus();     //设置输入区为焦点
}

//推理中界面状态
void Widget::ui_state_pushing() {
    decode_play();  //开启推理动画
    ui->load->setEnabled(0);
    ui->date->setEnabled(0);
    ui->set->setEnabled(0);
    ui->reset->setEnabled(1);
    ui->send->setEnabled(0);
}

//服务中界面状态
void Widget::ui_state_servering() {
    ui->output->clear();
    ui->load->setEnabled(0);
    ui->date->setEnabled(0);
    ui->set->setEnabled(1);
    ui->reset->setEnabled(0);
    ui->input->setVisible(0);
    ui->send->setVisible(0);

}

//待机界面状态
void Widget::ui_state_normal() {
    if (is_run)  //如果是模型正在运行的状态的话
    {
        ui->reset->setEnabled(1);
        ui->input->textEdit->setEnabled(1);
        ui->input->textEdit->setReadOnly(0);
        ui->send->setEnabled(0);
        ui->input->textEdit->setPlaceholderText(jtr("chat or right click to choose question"));
        ui->input->textEdit->setStyleSheet("background-color: white;");
        return;
    }

    decode_pTimer->stop();  //停止解码动画
    if (ui_state == CHAT_STATE) {
        ui->input->setVisible(1);
        ui->send->setVisible(1);

        ui->load->setEnabled(1);
        if (is_load || ui_mode == LINK_MODE) {
            ui->reset->setEnabled(1);
            ui->send->setEnabled(1);
        }
        if(is_load || ui_mode == LINK_MODE)
        {
            ui->date->setEnabled(1);
            ui->set->setEnabled(1);
        }
        ui->input->setVisible(1);
        ui->send->setVisible(1);

        ui->input->textEdit->setPlaceholderText(jtr("chat or right click to choose question"));
        ui->input->textEdit->setStyleSheet("background-color: white;");
        ui->input->textEdit->setReadOnly(0);
        ui->input->textEdit->setFocus();  //设置输入区为焦点
        ui->send->setText(jtr("send"));

        ui->output->setReadOnly(1);

    } else if (ui_state == COMPLETE_STATE) {
        ui->load->setEnabled(1);

        if (is_load || ui_mode == LINK_MODE) {
            ui->reset->setEnabled(1);
            ui->send->setEnabled(1);
        }
        ui->date->setEnabled(1);
        ui->set->setEnabled(1);
        ui->input->setVisible(1);
        ui->send->setVisible(1);

        ui->input->textEdit->clear();
        ui->input->textEdit->setPlaceholderText(jtr("Please modify any text above"));
        ui->input->textEdit->setStyleSheet("background-color: rgba(255, 165, 0, 127);");  //设置背景为橘黄色
        ui->input->textEdit->setReadOnly(1);
        ui->send->setText(jtr("complete"));

        ui->output->setReadOnly(0);
        ui->output->setFocus();  //设置输出区为焦点
    } else if (ui_state == SERVER_STATE) {
        ui->set->setEnabled(1);
    }
    if (ui_mode == LINK_MODE) {
        change_api_dialog(0);
    }  //链接模式不要解码设置
    else {
        change_api_dialog(1);
    }
}

//录音界面状态
void Widget::ui_state_recoding() {
    if (audio_time == 0) {
        ui->load->setEnabled(0);
        ui->date->setEnabled(0);
        ui->set->setEnabled(0);
        ui->reset->setEnabled(0);
        ui->send->setEnabled(0);
        ui->input->textEdit->setFocus();
        ui->input->textEdit->clear();
        ui->input->textEdit->setStyleSheet("background-color: rgba(144, 238, 144, 127);");  //透明绿色
        ui->input->textEdit->setReadOnly(1);
        ui->input->textEdit->setFocus();  //设置输入区为焦点
        ui->input->textEdit->setPlaceholderText(jtr("recoding") + "... " + jtr("push f2 to stop"));
    } else {
        ui->input->textEdit->setPlaceholderText(jtr("recoding") + "... " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s " + jtr("push f2 to stop"));
    }
}

//添加右击问题
void Widget::create_right_menu() {
    QDate currentDate = QDate::currentDate();  //历史中的今天
    QString dateString = currentDate.toString("M" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    //---------------创建一般问题菜单--------------
    if (right_menu != nullptr) {
        delete right_menu;
    }
    right_menu = new QMenu(this);
    for (int i = 1; i < 14; ++i) {
        QString question;

        if (i == 4) {
            question = jtr(QString("Q%1").arg(i)).replace("{today}", dateString);
        }  //历史中的今天
        else {
            question = jtr(QString("Q%1").arg(i));
        }
        QAction *action = right_menu->addAction(question);

        connect(action, &QAction::triggered, this, [=]() { ui->input->textEdit->setPlainText(question); });
    }
    //------------创建自动化问题菜单-------------
    //上传图像
    QAction *action14 = right_menu->addAction(jtr("Q14"));
    connect(action14, &QAction::triggered, this, [=]() {

        //用户选择图片
        QStringList paths = QFileDialog::getOpenFileNames(nullptr, jtr("Q14"), currentpath, "(*.png *.jpg *.bmp)");
        ui->input->addFiles(paths);
    });
}

//添加托盘右击事件
void Widget::create_tray_right_menu()
{ 
    trayMenu->clear();
    QAction *showAction_shortcut = trayMenu->addAction(jtr("shortcut"));
    QAction *blank1 = trayMenu->addAction("");//占位符
    QAction *blank2 = trayMenu->addAction("");//占位符，目的是把截图顶出去，用户点击后才会隐藏
    blank1->setEnabled(false);
    blank2->setEnabled(false);
    trayMenu->addSeparator();// 添加分割线
    QAction *showAction_widget = trayMenu->addAction(jtr("show widget"));
    QAction *showAction_expend = trayMenu->addAction(jtr("show expend"));
    trayMenu->addSeparator();// 添加分割线
    QAction *exitAction = trayMenu->addAction(jtr("quit"));
    QObject::connect(showAction_widget, &QAction::triggered, this, [&]() 
    {
        toggleWindowVisibility(this,true);// 显示窗体
    });
    QObject::connect(showAction_expend, &QAction::triggered, this, [&]() 
    {
        emit ui2expend_show(PREV_WINDOW);
    });
    QObject::connect(showAction_shortcut, &QAction::triggered, this, [&]() 
    {
        trayMenu->hide();
        QTimer::singleShot(100,[this](){onShortcutActivated_F1();});// 触发截图
    });
    QObject::connect(exitAction, &QAction::triggered, QApplication::quit);// 退出程序
}

//获取设置中的纸面值
void Widget::get_set() {
    ui_SETTINGS.temp = settings_ui->temp_slider->value() / 100.0;
    ui_SETTINGS.repeat = settings_ui->repeat_slider->value() / 100.0;

    ui_SETTINGS.nthread = settings_ui->nthread_slider->value();
    ui_SETTINGS.nctx = settings_ui->nctx_slider->value();    //获取nctx滑块的值
    ui_SETTINGS.ngl = settings_ui->ngl_slider->value();      //获取ngl滑块的值

    ui_SETTINGS.lorapath = settings_ui->lora_LineEdit->text();
    ui_SETTINGS.mmprojpath = settings_ui->mmproj_LineEdit->text();

    ui_SETTINGS.complete_mode = settings_ui->complete_btn->isChecked();
    ui_monitor_frame = settings_ui->frame_lineEdit->text().toDouble();

    if (settings_ui->chat_btn->isChecked()) {
        ui_state = CHAT_STATE;

    } 
    else if (settings_ui->complete_btn->isChecked()) {
        ui_state = COMPLETE_STATE;

    }
    else if (settings_ui->web_btn->isChecked()) {
        ui_state = SERVER_STATE;
    }

    ui_port = settings_ui->port_lineEdit->text();
}

//获取约定中的纸面值
void Widget::get_date() {
    ui_date_prompt = date_ui->date_prompt_TextEdit->toPlainText();
    //合并附加指令
    if (ui_extra_prompt != "") {
        ui_DATES.date_prompt = ui_date_prompt + "\n\n" + ui_extra_prompt;
    } else {
        ui_DATES.date_prompt = ui_date_prompt;
    }

    ui_DATES.user_name = date_ui->user_name_LineEdit->text();
    ui_DATES.model_name = date_ui->model_name_LineEdit->text();

    ui_DATES.is_load_tool = is_load_tool;
    ui_template = date_ui->chattemplate_comboBox->currentText();
    ui_extra_lan = date_ui->switch_lan_button->text();

    ui_calculator_ischecked = date_ui->calculator_checkbox->isChecked();
    ui_engineer_ischecked = date_ui->engineer_checkbox->isChecked();
    ui_MCPtools_ischecked = date_ui->MCPtools_checkbox->isChecked();
    ui_knowledge_ischecked = date_ui->knowledge_checkbox->isChecked();
    ui_controller_ischecked = date_ui->controller_checkbox->isChecked();
    ui_stablediffusion_ischecked = date_ui->stablediffusion_checkbox->isChecked();

    //记录自定义模板
    if (ui_template == jtr("custom set1")) {
        custom1_date_system = ui_date_prompt;
        custom1_user_name = ui_DATES.user_name;
        custom1_model_name = ui_DATES.model_name;
    } else if (ui_template == jtr("custom set2")) {
        custom2_date_system = ui_date_prompt;
        custom2_user_name = ui_DATES.user_name;
        custom2_model_name = ui_DATES.model_name;
    }

    //添加额外停止标志
    addStopwords();
}

//手搓输出解析器，提取可能的xml，目前只支持一个参数
mcp::json Widget::XMLparser(QString text)
{
    if(text.contains("</think>"))
    {
        text = text.split("</think>")[1];//移除思考标签前面的所有内容
    }
    mcp::json toolsarg;// 提取出的工具名和参数
    // 匹配<tool></tool>之间的内容
    QRegularExpression toolRegex("<([^>]+)>(.*)</\\1>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch toolMatch = toolRegex.match(text);
    if (toolMatch.hasMatch()) 
    {
        QString toolContent = toolMatch.captured(2);
        qDebug() << "toolContent:" << toolContent;
        try
        {
            toolsarg = mcp::json::parse(toolContent.toStdString());
        }
        catch(const std::exception& e)
        {
            qCritical() << "tool JSON parse error:" << e.what();
        }
    } 
    else 
    {
        qDebug() << "no tool matched";
    }

    return toolsarg;
}

//构建额外指令
QString Widget::create_extra_prompt() {
    QString extra_prompt_;//额外指令
    QString available_tools_describe;//工具名和描述
    QString engineer_info;//软件工程师信息
    extra_prompt_ = EXTRA_PROMPT_FORMAT;
    extra_prompt_.replace("{OBSERVATION_STOPWORD}",DEFAULT_OBSERVATION_STOPWORD);
    if (is_load_tool) {
        available_tools_describe += Buildin_tools_answer.text + "\n\n";
        // qDebug()<< MCP_TOOLS_INFO_LIST.size();
        if (date_ui->MCPtools_checkbox->isChecked()) {
            available_tools_describe += Buildin_tools_mcp_tools_list.text + "\n\n";
        }
        if (date_ui->calculator_checkbox->isChecked()) {
            available_tools_describe += Buildin_tools_calculator.text + "\n\n";
        }
        if (date_ui->knowledge_checkbox->isChecked()) {
            available_tools_describe += Buildin_tools_knowledge.text.replace("{embeddingdb describe}", embeddingdb_describe) + "\n\n";
        }
        if (date_ui->stablediffusion_checkbox->isChecked()) {
            available_tools_describe += Buildin_tools_stablediffusion.text + "\n\n";
        }
        if (date_ui->controller_checkbox->isChecked()) {
            screen_info = create_screen_info();//构建屏幕信息
            available_tools_describe += Buildin_tools_controller.text.replace("{screen_info}", screen_info) + "\n\n";
        }
        if (date_ui->engineer_checkbox->isChecked()) {
            available_tools_describe += Buildin_tools_execute_command.text + "\n\n";
            available_tools_describe += Buildin_tools_read_file.text + "\n\n";
            available_tools_describe += Buildin_tools_write_file.text + "\n\n";
            available_tools_describe += Buildin_tools_edit_file.text + "\n\n";
            // 这里添加更多工程师的工具
            engineer_info = create_engineer_info();//构建工程师信息
        }
        extra_prompt_.replace("{available_tools_describe}",available_tools_describe);//替换相应内容
        extra_prompt_.replace("{engineer_info}",engineer_info);//替换相应内容
    } else {
        extra_prompt_ = "";//没有挂载工具则为空
    }
    return extra_prompt_;
}

QString Widget::truncateString(const QString& str, int maxLength) {
    if (str.size() <= maxLength) {
        return str;
    }
    
    // 使用QTextStream来处理多字节字符
    QTextStream stream(const_cast<QString*>(&str), QIODevice::ReadOnly);
    stream.setCodec("UTF-8");
    
    // 找到开始截取的位置
    int startIndex = str.size() - maxLength;
    
    // 确保不截断多字节字符
    stream.seek(startIndex);
    
    return QString(stream.readAll());
}

//获取环境中的python版本以及库信息
QString Widget::checkPython()
{
    QString result;

    // Initialize the python executable name
    QString pythonExecutable = DEFAULT_PYTHON;

    QProcess process;
    QStringList shellArgs;
    shellArgs << CMDGUID << pythonExecutable + " --version";
    process.start(shell, shellArgs);
    process.waitForFinished();

    QString versionOutput = process.readAllStandardOutput();
    QString versionError = process.readAllStandardError();
    QString versionInfo = versionOutput + versionError;

    // If still not found, return an error message
    if (versionInfo.isEmpty()) {
        result += "Python interpreter not found in the system environment.\n";
        return result;
    } else {
        QStringList shellArgs2;
#ifdef Q_OS_WIN
        shellArgs2 << CMDGUID << pythonExecutable << "-c" << "import sys; print(sys.executable)";
#elif defined(Q_OS_LINUX)
        // 注意linux下不能有两个-c
        shellArgs2 << CMDGUID << pythonExecutable + "-c \"import sys; print(sys.executable)\"";
#endif
        process.start(shell, shellArgs2);
        process.waitForFinished();
        QString python_absolutePath = process.readAllStandardOutput().trimmed();
        result += versionInfo + " " + python_absolutePath + "\n";
    }

    // // 获取安装库信息
    // QStringList shellArgs3;
    // shellArgs3 << CMDGUID << pythonExecutable + " -m pip list";
    // process.start(shell, shellArgs3);
    // process.waitForFinished();

    // QString pipOutput = process.readAllStandardOutput();
    // QString pipError = process.readAllStandardError();
    // QString pipList = pipOutput + pipError;

    // // 不想展示的库
    // QStringList excludeLibraries = {
    //     "anyio", "appdirs", "archspec", "arrow", "astroid", "astropy", "astropy-iers-data", "asttokens", "async-lru", "attrs", "autopep8", "Babel", "black", "bleach", "bokeh", "botocore", "Bottleneck",
    //     "aiohttp", "aioitertools", "altair", "anaconda-anon-usage", "anaconda-catalogs", "anaconda-client", "anaconda-cloud-auth", "anaconda-navigator", "anaconda-project",
    //     "aiobotocore", "aiohappyeyeballs", "aiosignal", "alabaster", "annotated-types", "argon2-cffi", "argon2-cffi-bindings", "atomicwrites", "Automat", "bcrypt", "binaryornot", 
    //     "blinker", "boltons", "Brotli", "cachetools", "cffi", "chardet", "charset-normalizer", "colorama", "cssselect", "cycler", "cytoolz", "decorator", "defusedxml", "diff-match-patch", 
    //     "distro", "docstring-to-markdown", "et-xmlfile", "executing", "frozenlist", "frozendict", "h11", "HeapDict", "hyperlink", "idna", "imagecodecs", "imagesize", "importlib-metadata", 
    //     "incremental", "iniconfig", "intervaltree", "ipython-genutils", "jaraco.classes", "jmespath", "jsonpointer", "jsonpatch", "json5", "lazy-object-proxy", "lazy_loader", "locket",
    //     "mdit-py-plugins", "mdurl", "mkl_fft", "mkl_random", "mkl-service", "more-itertools", "mpmath", "multidict", "multipledispatch", "mypy-extensions", "navigator-updater", "nest-asyncio", "overrides", 
    //     "pkce", "platformdirs", "pluggy", "pure-eval", "py-cpuinfo", "pyasn1", "pyasn1-modules", "pycosat", "pyct", "pydocstyle", "pyerfa", "pylint-venv", "pyls-spyder", "pytoolconfig", "pyviz_comms", 
    //     "pywin32-ctypes", "pywinpty", "queuelib", "referencing", "rfc3339-validator", "rfc3986-validator", "rope", "rpds-py", "ruamel.yaml.clib", "ruamel-yaml-conda", "safetensors", "semver",
    //     "service-identity", "sip", "smmap", "sniffio", "snowballstemmer", "sortedcontainers", "stack-data", "tblib", "tenacity", "text-unidecode", "textdistance", "threadpoolctl", "three-merge", "tinycss2", "tldextract", 
    //     "tomli", "toolz", "truststore", "twisted-iocpsupport", "ua-parser", "ua-parser-builtins", "uc-micro-py", "unicodedata2", "user-agents", "w3lib", "whatthepatch", "windows-curses", "zict", "zope.interface", "zstandard"
    // };
    
    // // If pip output is empty, pip may not be installed
    // if (pipList.contains("No module named pip")) {
    //     result += "pip is not installed for the detected Python interpreter.\n";
    // } else if (pipList.isEmpty()) {
    //     result += "No Python libraries installed or unable to retrieve the list.\n";
    // } else {
    //     // Extract package names from the pip list output
    //     QStringList lines = pipList.split('\n');
    //     QStringList packageNames;

    //     // Skip the first line (header) and process the rest
    //     for (int i = 2; i < lines.size(); ++i) {  // Start from the second line
    //         QString line = lines[i].trimmed();
    //         if (line.isEmpty()) continue;  // Skip empty lines

    //         QStringList parts = line.split(QRegExp("\\s+"));  // Split by whitespace
    //         if (parts.size() > 0) {
    //             QString packageName = parts[0];
    //             if (!excludeLibraries.contains(packageName)) {  // Exclude foundational libraries
    //                 packageNames.append(packageName);
    //             }
    //         }
    //     }

    //     // Join the package names into a single string, separated by spaces
    //     result += "Part Installed Python Libraries: " + truncateString(packageNames.join(" "),MAX_INPUT);
    // }

    return result;
}

QString Widget::checkCompile() {
    QString compilerInfo;
    QProcess process;
    // Windows平台的编译器检查
#ifdef Q_OS_WIN
    // 尝试检查 MinGW
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "g++ --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty()) {
            compilerInfo += "MinGW version: ";
            QStringList lines = output.split('\n');
            QString versionLine = lines.first();
            QRegExp regExp("\\s*\\(.*\\)");  // 使用正则表达式去除括号中的内容 匹配括号及其中的内容
            versionLine = versionLine.replace(regExp, "");
            compilerInfo += versionLine.trimmed();  // 去除前后的空格
            compilerInfo += "\n";
        }
    }

    // 检查 MSVC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "cl /Bv";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QByteArray output = process.readAllStandardOutput();
        output += process.readAllStandardError();
        if (!output.isEmpty()) {
            compilerInfo += "MSVC version: ";
            QString outputStr = QString::fromLocal8Bit(output);
            QStringList lines = outputStr.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }
    }

    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty()) {
            compilerInfo += "Clang version: ";
            QStringList lines = output.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }
    }

#endif

    // Linux平台的编译器检查
#ifdef Q_OS_LINUX
    // 检查 GCC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "gcc --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty()) {
            compilerInfo += "GCC version: ";
            QStringList lines = output.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }

    }

    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty()) {
            compilerInfo += "Clang version: ";
            QStringList lines = output.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }
    }

#endif

    // 如果没有找到任何编译器信息
    if (compilerInfo.isEmpty()) {
        compilerInfo = "No compiler detected.\n";
    }

    return compilerInfo;
}

QString Widget::create_engineer_info()
{
    QString engineer_info_ = ENGINEER_INFO;
    QString engineer_system_info_ = ENGINEER_SYSTEM_INFO;
    QDate currentDate = QDate::currentDate();  //今天日期
    QString dateString = currentDate.toString("yyyy" + QString(" ") + jtr("year") + QString(" ") + "M" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    engineer_system_info_.replace("{OS}", USEROS);
    engineer_system_info_.replace("{DATE}", dateString);
    engineer_system_info_.replace("{SHELL}", shell);
    engineer_system_info_.replace("{COMPILE_ENV}", compile_env);
    engineer_system_info_.replace("{PYTHON_ENV}", python_env);
    engineer_system_info_.replace("{DIR}", applicationDirPath + "/EVA_WORK");

    engineer_info_.replace("{engineer_system_info}", engineer_system_info_);
    return engineer_info_;
}

//添加额外停止标志，本地模式时在xbot.cpp里已经现若同时包含"<|" 和 "|>"也停止
void Widget::addStopwords() {
    ui_DATES.extra_stop_words.clear();  //重置额外停止标志

    if (ui_DATES.is_load_tool)  //如果挂载了工具则增加额外停止标志
    {
        // ui_DATES.extra_stop_words << DEFAULT_OBSERVATION_STOPWORD;//在后端已经处理了
    }
}

//获取本机第一个ip地址 排除以.1结尾的地址 如果只有一个.1结尾的则保留它
QString Widget::getFirstNonLoopbackIPv4Address() {
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    QString ipWithDot1; // 用于存储以.1结尾的IP地址

    for (int i = 0; i < list.count(); i++) {
        QString ip = list[i].toString();
        // 排除回环地址和非IPv4地址
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol) {
            if (ip.endsWith(".1")) {
                ipWithDot1 = ip; // 记录以.1结尾的IP地址
            } else {
                return ip; // 返回第一个不以.1结尾的IP地址
            }
        }
    }

    // 如果没有找到不以.1结尾的IP地址，则返回以.1结尾的IP地址
    if (!ipWithDot1.isEmpty()) {
        return ipWithDot1;
    }

    return QString(); // 如果没有找到任何符合条件的IP地址，返回空字符串
}

//第三方程序开始
void Widget::server_onProcessStarted() {
    if (ui_SETTINGS.ngl == 0) {
        EVA_icon = QIcon(":/logo/connection-point-blue.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    } else {
        EVA_icon = QIcon(":/logo/connection-point-green.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }
    ipAddress = getFirstNonLoopbackIPv4Address();
    reflash_state("ui:server " + jtr("oning"), SIGNAL_SIGNAL);
}

//第三方程序结束
void Widget::server_onProcessFinished() {
    if (ui_state == SERVER_STATE) {
        ui_state_info = "ui:" + jtr("old") + "server " + jtr("off");
        reflash_state(ui_state_info, SIGNAL_SIGNAL);
    } else {
        EVA_icon = QIcon(":/logo/dark_logo.png");
        QApplication::setWindowIcon(EVA_icon);  //设置应用程序图标
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
        reflash_state("ui:server" + jtr("off"), SIGNAL_SIGNAL);
        ui_output = "\nserver" + jtr("shut down");
        output_scroll(ui_output);
    }
}

// llama-bench进程结束响应
void Widget::bench_onProcessFinished() { qDebug() << "llama-bench进程结束响应"; }

//显示文件名和图像
void Widget::showImages(QStringList images_filepath) {
    for (int i = 0; i < images_filepath.size(); ++i) {
        QString imagepath = images_filepath[i];
        QString ui_output = imagepath + "\n";
        if(ui_output!=":/logo/wav.png"){output_scroll(ui_output);}

        // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
        QImage image(imagepath);
        int originalWidth = image.width() / devicePixelRatioF();
        int originalHeight = image.height() / devicePixelRatioF();

        QTextCursor cursor(ui->output->textCursor());
        cursor.movePosition(QTextCursor::End);

        QTextImageFormat imageFormat;
        imageFormat.setWidth(originalWidth/2);    // 设置图片的宽度
        imageFormat.setHeight(originalHeight/2);  // 设置图片的高度
        imageFormat.setName(imagepath);         // 图片资源路径

        cursor.insertImage(imageFormat);
        output_scroll("\n");
        //滚动到底部展示
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum());  //滚动条滚动到最下面
    }
}

//开始录音
void Widget::recordAudio() {
    reflash_state("ui:" + jtr("recoding") + "... ");
    ui_state_recoding();

    audioRecorder.record();   // 在这之前检查是否可用
    audio_timer->start(100);  // 每隔100毫秒刷新一次输入区
}

// 每隔100毫秒刷新一次监视录音
void Widget::monitorAudioLevel() {
    audio_time += 100;
    ui_state_recoding();  //更新输入区
}

//停止录音
void Widget::stop_recordAudio() {
    QString wav_path = applicationDirPath + "/EVA_TEMP/" + QString("EVA_") + ".wav";
    is_recodering = false;
    audioRecorder.stop();
    audio_timer->stop();
    reflash_state("ui:" + jtr("recoding over") + " " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s");
    audio_time = 0;
    //将录制的wav文件重采样为16khz音频文件
#ifdef _WIN32
    QTextCodec *code = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    std::string wav_path_c = code->fromUnicode(wav_path).data();
#elif __linux__
    std::string wav_path_c = wav_path.toStdString();
#endif
    resampleWav(wav_path_c, wav_path_c);
    emit ui2expend_speechdecode(wav_path, "txt");  //传一个wav文件开始解码
}

//更新gpu内存使用率
void Widget::updateGpuStatus() { emit gpu_reflash(); }

//更新cpu内存使用率
void Widget::updateCpuStatus() { emit cpu_reflash(); }

//拯救中文
void Widget::getWords(QString json_file_path) {
    QFile jfile(json_file_path);
    if (!jfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for reading.";
        return;
    }

    QTextStream in(&jfile);
    in.setCodec("UTF-8");  // 确保使用UTF-8编码读取文件
    QString data = in.readAll();
    jfile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = doc.object();
    wordsObj = jsonObj["words"].toObject();
}

//切换额外指令的语言
void Widget::switch_lan_change() {
    if (date_ui->switch_lan_button->text() == "zh") {
        language_flag = 1;
        date_ui->switch_lan_button->setText("en");

    } else if (date_ui->switch_lan_button->text() == "en") {
        language_flag = 0;
        date_ui->switch_lan_button->setText("zh");
    }

    apply_language(language_flag);
    ui_extra_prompt = create_extra_prompt();
    emit ui2bot_language(language_flag);
    emit ui2tool_language(language_flag);
    emit ui2net_language(language_flag);
    emit ui2expend_language(language_flag);
}
//改变语种相关
void Widget::apply_language(int language_flag_) {
    //主界面语种
    ui->load->setText(jtr("load"));
    ui->load->setToolTip(jtr("load_button_tooltip"));
    ui->date->setText(jtr("date"));
    ui->set->setToolTip(jtr("set"));
    ui->reset->setToolTip(jtr("reset"));
    ui->send->setText(jtr("send"));
    ui->send->setToolTip(jtr("send_tooltip"));
    cutscreen_dialog->initAction(jtr("save cut image"), jtr("svae screen image"));
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(std::thread::hardware_concurrency()));
    ui->mem_bar->setShowText(jtr("mem"));    //进度条里面的文本,强制重绘
    ui->vram_bar->setShowText(jtr("vram"));  //进度条里面的文本,强制重绘
    ui->kv_bar->setShowText(jtr("brain"));   //进度条里面的文本,强制重绘
    ui->cpu_bar->show_text = "cpu ";           //进度条里面的文本
    ui->vcore_bar->show_text = "gpu ";         //进度条里面的文本
    //输入区右击菜单语种
    create_right_menu();  //添加右击问题
    create_tray_right_menu();
    // api设置语种
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_endpoint_label->setText(jtr("api endpoint"));
    api_endpoint_LineEdit->setPlaceholderText(jtr("input api endpoint"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_key_label->setText(jtr("api key"));
    api_key_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_key_LineEdit->setToolTip(jtr("input api key"));
    api_model_label->setText(jtr("api model"));
    api_model_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_model_LineEdit->setToolTip(jtr("input api model"));
    //约定选项语种
    date_ui->prompt_box->setTitle(jtr("character"));  //提示词模板设置区域
    date_ui->chattemplate_label->setText(jtr("chat template"));
    date_ui->chattemplate_label->setToolTip(jtr("chattemplate_label_tooltip"));
    date_ui->chattemplate_comboBox->setToolTip(jtr("chattemplate_label_tooltip"));
    date_ui->date_prompt_label->setText(jtr("date prompt"));
    date_ui->date_prompt_label->setToolTip(jtr("date_prompt_label_tooltip"));
    date_ui->date_prompt_TextEdit->setToolTip(jtr("date_prompt_label_tooltip"));
    date_ui->user_name_label->setText(jtr("user name"));
    date_ui->user_name_label->setToolTip(jtr("user_name_label_tooltip"));
    date_ui->user_name_LineEdit->setToolTip(jtr("user_name_label_tooltip"));
    date_ui->model_name_label->setText(jtr("model name"));
    date_ui->model_name_label->setToolTip(jtr("model_name_label_tooltip"));
    date_ui->model_name_LineEdit->setToolTip(jtr("model_name_label_tooltip"));
    date_ui->tool_box->setTitle(jtr("mount") + jtr("tool"));
    date_ui->calculator_checkbox->setText(jtr("calculator"));
    date_ui->calculator_checkbox->setToolTip(jtr("calculator_checkbox_tooltip"));
    date_ui->engineer_checkbox->setText(jtr("engineer"));
    date_ui->engineer_checkbox->setToolTip(jtr("engineer_checkbox_tooltip"));
    date_ui->controller_checkbox->setText(jtr("controller"));
    date_ui->controller_checkbox->setToolTip(jtr("controller_checkbox_tooltip"));
    date_ui->knowledge_checkbox->setText(jtr("knowledge"));
    date_ui->knowledge_checkbox->setToolTip(jtr("knowledge_checkbox_tooltip"));
    date_ui->MCPtools_checkbox->setText(jtr("MCPtools"));
    date_ui->MCPtools_checkbox->setToolTip(jtr("MCPtools_checkbox_tooltip"));
    date_ui->stablediffusion_checkbox->setText(jtr("stablediffusion"));
    date_ui->stablediffusion_checkbox->setToolTip(jtr("stablediffusion_checkbox_tooltip"));
    date_ui->switch_lan_button->setToolTip(jtr("switch_lan_button_tooltip"));
    date_ui->confirm_button->setText(jtr("ok"));
    date_ui->cancel_button->setText(jtr("cancel"));
    date_dialog->setWindowTitle(jtr("date"));
    //设置选项语种
    settings_ui->sample_box->setTitle(jtr("sample set"));  //采样设置区域
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(ui_SETTINGS.temp));
    settings_ui->temp_label->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    settings_ui->temp_slider->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat));
    settings_ui->repeat_label->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    settings_ui->repeat_slider->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    settings_ui->decode_box->setTitle(jtr("decode set"));  //解码设置区域
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl));
    settings_ui->ngl_label->setToolTip(jtr("put some model paragram to gpu and reload model"));
    settings_ui->ngl_slider->setToolTip(jtr("put some model paragram to gpu and reload model"));
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread));
    settings_ui->nthread_label->setToolTip(jtr("not big better"));
    settings_ui->nthread_slider->setToolTip(jtr("not big better"));
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(ui_SETTINGS.nctx));
    settings_ui->nctx_label->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    settings_ui->nctx_slider->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    settings_ui->lora_label->setText(jtr("load lora"));
    settings_ui->lora_label->setToolTip(jtr("lora_label_tooltip"));
    settings_ui->lora_LineEdit->setToolTip(jtr("lora_label_tooltip"));
    settings_ui->lora_LineEdit->setPlaceholderText(jtr("right click and choose lora"));
    settings_ui->mmproj_label->setText(jtr("load mmproj"));
    settings_ui->mmproj_label->setToolTip(jtr("mmproj_label_tooltip"));
    settings_ui->mmproj_LineEdit->setToolTip(jtr("mmproj_label_tooltip"));
    settings_ui->mmproj_LineEdit->setPlaceholderText(jtr("right click and choose mmproj"));
    settings_ui->mode_box->setTitle(jtr("state set"));  //状态设置区域
    settings_ui->complete_btn->setText(jtr("complete state"));
    settings_ui->complete_btn->setToolTip(jtr("complete_btn_tooltip"));
    settings_ui->chat_btn->setText(jtr("chat state"));
    settings_ui->chat_btn->setToolTip(jtr("chat_btn_tooltip"));
    settings_ui->web_btn->setText(jtr("server state"));
    settings_ui->web_btn->setToolTip(jtr("web_btn_tooltip"));
    settings_ui->port_label->setText(jtr("port"));
    settings_ui->port_label->setToolTip(jtr("port_label_tooltip"));
    settings_ui->port_lineEdit->setToolTip(jtr("port_label_tooltip"));
    settings_ui->frame_label->setText(jtr("frame"));
    settings_ui->frame_label->setToolTip(jtr("frame_label_tooltip"));
    settings_ui->frame_lineEdit->setToolTip(jtr("frame_label_tooltip"));
    settings_ui->confirm->setText(jtr("ok"));
    settings_ui->cancel->setText(jtr("cancel"));
    settings_dialog->setWindowTitle(jtr("set"));
}

//创建临时文件夹EVA_TEMP
bool Widget::createTempDirectory(const QString &path) {
    QDir dir;
    // 检查路径是否存在
    if (dir.exists(path)) {
        return false;
    } else {
        // 尝试创建目录
        if (dir.mkpath(path)) {
            return true;
        } else {
            return false;
        }
    }
}

// 打开文件夹
QString Widget::customOpenfile(QString dirpath, QString describe, QString format) {
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);
    return filepath;
}

//语音朗读相关 文转声相关

//每次约定和设置后都保存配置到本地
void Widget::auto_save_user() {
    //--------------保存当前用户配置---------------
    // 创建 QSettings 对象，指定配置文件的名称和格式

    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    settings.setValue("ui_mode", ui_mode);  //机体模式
    settings.setValue("ui_state", ui_state);  //机体状态
    settings.setValue("shell", shell);  //shell路径
    settings.setValue("python", pythonExecutable);  //python版本
    //保存设置参数
    settings.setValue("modelpath", ui_SETTINGS.modelpath);  //模型路径
    settings.setValue("temp", ui_SETTINGS.temp);            //温度
    settings.setValue("repeat", ui_SETTINGS.repeat);        //惩罚系数
    settings.setValue("ngl", ui_SETTINGS.ngl);              // gpu负载层数
    settings.setValue("nthread", ui_SETTINGS.nthread);      // cpu线程数
    settings.setValue("nctx", ui_SETTINGS.nctx);
    settings.setValue("mmprojpath", ui_SETTINGS.mmprojpath);  //视觉
    settings.setValue("lorapath", ui_SETTINGS.lorapath);      // lora
    settings.setValue("monitor_frame", ui_monitor_frame);      // 监视帧率
    
    //保存隐藏设置
    settings.setValue("hid_npredict", ui_SETTINGS.hid_npredict);    //最大输出长度
    settings.setValue("hid_special", ui_SETTINGS.hid_special);
    settings.setValue("hid_top_p", ui_SETTINGS.hid_top_p);
    settings.setValue("hid_batch", ui_SETTINGS.hid_batch);
    settings.setValue("hid_n_ubatch", ui_SETTINGS.hid_n_ubatch);
    settings.setValue("hid_use_mmap", ui_SETTINGS.hid_use_mmap);
    settings.setValue("hid_use_mlock", ui_SETTINGS.hid_use_mlock);
    settings.setValue("hid_flash_attn", ui_SETTINGS.hid_flash_attn);
    settings.setValue("hid_parallel", ui_SETTINGS.hid_parallel);
    settings.setValue("port", ui_port);                       //服务端口
    //保存约定参数
    settings.setValue("chattemplate", date_ui->chattemplate_comboBox->currentText());               //对话模板
    settings.setValue("calculator_checkbox", date_ui->calculator_checkbox->isChecked());            //计算器工具
    settings.setValue("knowledge_checkbox", date_ui->knowledge_checkbox->isChecked());              // knowledge工具
    settings.setValue("controller_checkbox", date_ui->controller_checkbox->isChecked());            // controller工具
    settings.setValue("stablediffusion_checkbox", date_ui->stablediffusion_checkbox->isChecked());  //计算器工具
    settings.setValue("engineer_checkbox", date_ui->engineer_checkbox->isChecked());                // engineer工具
    settings.setValue("MCPtools_checkbox", date_ui->MCPtools_checkbox->isChecked());              // MCPtools工具
    settings.setValue("extra_lan", ui_extra_lan);                                                   //额外指令语种

    //保存自定义的约定模板
    settings.setValue("custom1_date_system", custom1_date_system);
    settings.setValue("custom1_user_name", custom1_user_name);
    settings.setValue("custom1_model_name", custom1_model_name);
    settings.setValue("custom2_date_system", custom2_date_system);
    settings.setValue("custom2_user_name", custom2_user_name);
    settings.setValue("custom2_model_name", custom2_model_name);

    //保存api参数
    settings.setValue("api_endpoint", apis.api_endpoint);
    settings.setValue("api_key", apis.api_key);
    settings.setValue("api_model", apis.api_model);

    reflash_state("ui:" + jtr("save_config_mess"), USUAL_SIGNAL);
}

//监视时间到
void Widget::monitorTime()
{
    // 这些情况不处理
    if(!is_load || is_run || ui_state!=CHAT_STATE || ui_mode != LOCAL_MODE || is_monitor)
    {return ;}
    is_monitor = true;
    QString filePath = saveScreen();
    emit ui2bot_monitor_filepath(filePath);//给模型发信号，能处理就处理
}

// 保存当前屏幕截图
QString Widget::saveScreen()
{
    QScreen *screen = QApplication::primaryScreen();
    
    // 获取屏幕几何信息
    QRect screenGeometry = screen->geometry();
    qreal devicePixelRatio = screen->devicePixelRatio();
    
    // qDebug() << "逻辑尺寸:" << screenGeometry.width() << screenGeometry.height();
    // qDebug() << "缩放比例:" << devicePixelRatio;
    
    // 直接使用 grabWindow 获取完整屏幕截图（会自动处理DPI）
    QPixmap m_screenPicture = screen->grabWindow(0);
    
    // qDebug() << "截图实际尺寸:" << m_screenPicture.width() << m_screenPicture.height();
    
    // 获取鼠标位置（使用逻辑坐标，不需要手动缩放）
    QPoint cursorPos = QCursor::pos();
    
    // 将逻辑坐标转换为截图中的物理坐标
    cursorPos.setX(cursorPos.x() * devicePixelRatio);
    cursorPos.setY(cursorPos.y() * devicePixelRatio);
    
    // 创建光标图标
    QPixmap cursorPixmap;
    
    // 尝试获取当前光标
    if (QApplication::overrideCursor()) {
        cursorPixmap = QApplication::overrideCursor()->pixmap();
    }
    
    // 如果没有获取到光标，创建默认箭头光标
    if (cursorPixmap.isNull()) {
        // 光标大小按DPI缩放
        int baseSize = 16;
        int cursorSize = baseSize * devicePixelRatio;
        
        cursorPixmap = QPixmap(cursorSize, cursorSize);
        cursorPixmap.fill(Qt::transparent);
        cursorPixmap.setDevicePixelRatio(devicePixelRatio);
        
        QPainter cursorPainter(&cursorPixmap);
        cursorPainter.setRenderHint(QPainter::Antialiasing);
        cursorPainter.setPen(QPen(Qt::black, 1));
        cursorPainter.setBrush(Qt::white);
        
        // 绘制箭头（使用逻辑坐标，QPainter会自动处理缩放）
        QPolygonF arrow;
        arrow << QPointF(0, 0) << QPointF(0, 10) << QPointF(3, 7) 
              << QPointF(7, 11) << QPointF(9, 9) 
              << QPointF(5, 5) << QPointF(10, 0);
        
        cursorPainter.drawPolygon(arrow);
        cursorPainter.end();
    } else {
        // 如果获取到了系统光标，确保它有正确的DPI设置
        cursorPixmap.setDevicePixelRatio(devicePixelRatio);
    }
    
    // 将光标绘制到截图上
    QPainter painter(&m_screenPicture);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制光标时，考虑到cursorPixmap可能已经有devicePixelRatio设置
    // 所以绘制位置需要调整
    QPoint drawPos = cursorPos;
    if (cursorPixmap.devicePixelRatio() > 1.0) {
        // 如果光标pixmap已经设置了devicePixelRatio，绘制位置需要相应调整
        drawPos.setX(cursorPos.x() / devicePixelRatio);
        drawPos.setY(cursorPos.y() / devicePixelRatio);
    }
    
    painter.drawPixmap(drawPos, cursorPixmap);
    painter.end();
    
    QImage image = m_screenPicture.toImage();
    
    // 逐步缩小图片直到尺寸 <= 1920x1080
    while (image.width() > 1920 || image.height() > 1080) {
        // 计算缩放比例，保持宽高比
        qreal scaleRatio = qMin(1920.0 / image.width(), 1080.0 / image.height());
        int newWidth = image.width() * scaleRatio;
        int newHeight = image.height() * scaleRatio;
        
        image = image.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
    QString filePath = QDir::currentPath() + "/EVA_TEMP/screen_cut/" + timestamp + ".png";
    createTempDirectory(QDir::currentPath() + "/EVA_TEMP/screen_cut");
    image.save(filePath);
    return filePath;
}


void Widget::recv_monitor_decode_ok()
{
    is_monitor = false;//解锁
}

//构建屏幕信息
QString Widget::create_screen_info()
{
    // 屏幕左上角坐标为(0,0) 右下角坐标为(x,y)
    QString info;
    QScreen *screen = QApplication::primaryScreen();
    
    // 使用物理像素尺寸，而不是逻辑像素
    QRect screenGeometry = screen->geometry();
    qreal devicePixelRatio = screen->devicePixelRatio();
    
    // 计算实际的物理像素尺寸
    int physicalWidth = screenGeometry.width() * devicePixelRatio;
    int physicalHeight = screenGeometry.height() * devicePixelRatio;

    info = QString("The coordinates of the top left corner of the screen are (0,0) and the coordinates of the bottom right corner are (%1, %2)")
           .arg(physicalWidth).arg(physicalHeight);

    return info;
}
