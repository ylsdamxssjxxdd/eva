//设置界面控件和槽函数

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
    movie_font.setFamily(DEFAULT_FONT);
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
    keeptimer = new QTimer(this);      //持续检测延迟
    connect(keeptimer, SIGNAL(timeout()), this, SLOT(keepConnection()));
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
        keeptimer->stop();
        force_unlockload_pTimer->start(0);  //强制解锁
    }
}

// 装载完毕强制预处理
void Widget::unlockLoad() {

    if (ui_SETTINGS.ngl < ui_maxngl) {
        reflash_state("ui:" + jtr("ngl tips"), USUAL_SIGNAL);
    }

    reflash_state("ui:" + jtr("load model") + jtr("over") + " " + QString::number(load_time, 'f', 2) + " s " + jtr("right click and check model log"), SUCCESS_SIGNAL);
    if (ui_SETTINGS.ngl > 0) {
        QApplication::setWindowIcon(QIcon(":/logo/green_logo.png"));
    }  // 设置应用程序图标
    else {
        QApplication::setWindowIcon(QIcon(":/logo/blue_logo.png"));
    }  // 设置应用程序图标
    this->setWindowTitle(jtr("current model") + " " + ui_SETTINGS.modelpath.split("/").last());
    ui->kv_bar->show_text = jtr("brain");
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(max_thread));
    auto_save_user();  //保存ui配置
    //如果是对话模式则预解码约定
    if (ui_state == CHAT_STATE) {
        history_prompt = ui_DATES.date_prompt;  //同步历史约定内容
        ui_need_predecode = true;
        on_send_clicked();
    }
    force_unlockload_pTimer->stop();
    is_load_play_over = true;  //标记模型动画已经完成
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
    if (is_test && is_while)  //现在要知道是模型输出的答案还是预编码完成的结果,要将预编码完成的结果排除
    {
        test_count++;  //已经加一了
        QString result_ = result;
        //答对，remove(' ')移除答案中的空格
        if (result_.remove(' ') == test_list_answer.at(test_question_index.at(0))) {
            test_score++;
            output_scroll(result + "\n", Qt::green);
            ui_state_info = "ui:" + QString::number(test_count) + " " + jtr("answer right") + " " + jtr("right answer") + test_list_answer.at(test_question_index.at(0));
            reflash_state(ui_state_info, SUCCESS_SIGNAL);
        }
        //答错
        else {
            output_scroll(result + "\n", Qt::red);
            ui_state_info = "ui:" + QString::number(test_count) + " " + jtr("answer error") + " " + jtr("right answer") + test_list_answer.at(test_question_index.at(0));
            reflash_state(ui_state_info, WRONG_SIGNAL);
        }
        float acc = test_score / test_count * 100.0;  //回答准确率
        test_question_index.removeAt(0);              //回答完毕删除开头的第一个问题
        if (ui_mode == LINK_MODE) {
            this->setWindowTitle(jtr("test") + QString::number(test_count) + "/" + QString::number(test_list_question.size()) + "   " + jtr("accurate") + QString::number(acc, 'f', 1) + "% " + "   " + jtr("current api") + " " + current_api);
        } else {
            this->setWindowTitle(jtr("test") + QString::number(test_count) + "/" + QString::number(test_list_question.size()) + "   " + jtr("accurate") + QString::number(acc, 'f', 1) + "% " + "   " + ui_SETTINGS.modelpath.split("/").last());
        }

        //每20次题加一次引导题
        if (int(test_count) % 20 == 0) {
            help_input = true;
            ui_state_info = "ui:" + jtr("add help question");
            reflash_state(ui_state_info, SIGNAL_SIGNAL);
        }
    } else {
        //正常输出
        output_scroll(result, color);
    }

    if (is_while) {
        temp_assistant_history += result;
        //添加待朗读的文字
        if (!is_test) {
            temp_speech += result;// 累计输出的文本
            //如果积累到包含 叹号/分号/顿号/逗号/句号/问号/冒号 时分段并等待朗读
            // QRegularExpression re("[！；、，。？：!;,?:]");
            QRegularExpression re("[！；、，。？：!;,?:]|\\.\\s");//新增对小数点后跟空格的捕获，但是如果模型输出带空格的字符将会分割异常，待修复
            QRegularExpressionMatch match = re.match(temp_speech);
            if (match.hasMatch()) {
                // qDebug()<< temp_speech;
                wait_speech_list << temp_speech;
                temp_speech = "";
            }
        }

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
        font.setFamily(DEFAULT_FONT);
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
void Widget::temp_change() { temp_label->setText(jtr("temperature") + " " + QString::number(temp_slider->value() / 100.0)); }
// ngl滑块响应
void Widget::ngl_change() {

    ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ngl_slider->value()));

}
// batch滑块响应
void Widget::batch_change() { batch_label->setText(jtr("batch size") + " " + QString::number(batch_slider->value())); }
// nctx滑块响应
void Widget::nctx_change() { nctx_label->setText(jtr("brain size") + " " + QString::number(nctx_slider->value())); }
// repeat滑块响应
void Widget::repeat_change() { repeat_label->setText(jtr("repeat") + " " + QString::number(repeat_slider->value() / 100.0)); }

void Widget::npredict_change() { npredict_label->setText(jtr("npredict") + " " + QString::number(npredict_slider->value())); }

void Widget::nthread_change() { nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(nthread_slider->value())); }

//补完状态按钮响应
void Widget::complete_change() {
    //选中则禁止约定输入
    if (complete_btn->isChecked()) {
        sample_box->setEnabled(1);

        nthread_slider->setEnabled(1);
        nctx_slider->setEnabled(1);

        port_lineEdit->setEnabled(0);
    }
}

//对话状态按钮响应
void Widget::chat_change() {
    if (chat_btn->isChecked()) {
        sample_box->setEnabled(1);

        nctx_slider->setEnabled(1);
        nthread_slider->setEnabled(1);

        port_lineEdit->setEnabled(0);
    }
}

//服务状态按钮响应
void Widget::web_change() {
    if (web_btn->isChecked()) {
        sample_box->setEnabled(0);

        port_lineEdit->setEnabled(1);
    }
}

//提示词模板下拉框响应
void Widget::prompt_template_change() {
    if (chattemplate_comboBox->currentText() == jtr("custom set1")) {
        date_prompt_TextEdit->setEnabled(1);
        user_name_LineEdit->setEnabled(1);
        model_name_LineEdit->setEnabled(1);

        date_prompt_TextEdit->setText(custom1_date_system);
        user_name_LineEdit->setText(custom1_user_name);
        model_name_LineEdit->setText(custom1_model_name);
    } else if (chattemplate_comboBox->currentText() == jtr("custom set2")) {
        date_prompt_TextEdit->setEnabled(1);
        user_name_LineEdit->setEnabled(1);
        model_name_LineEdit->setEnabled(1);

        date_prompt_TextEdit->setText(custom2_date_system);
        user_name_LineEdit->setText(custom2_user_name);
        model_name_LineEdit->setText(custom2_model_name);
    } else {
        date_prompt_TextEdit->setText(date_map[chattemplate_comboBox->currentText()].date_prompt);
        date_prompt_TextEdit->setEnabled(0);
        user_name_LineEdit->setText(date_map[chattemplate_comboBox->currentText()].user_name);
        user_name_LineEdit->setEnabled(0);
        model_name_LineEdit->setText(date_map[chattemplate_comboBox->currentText()].model_name);
        model_name_LineEdit->setEnabled(0);
    }
}

void Widget::chooseLorapath() {
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose lora model"), "(*.bin *.gguf)");

    lora_LineEdit->setText(currentpath);
}

void Widget::chooseMmprojpath() {
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose mmproj model"), "(*.bin *.gguf)");

    mmproj_LineEdit->setText(currentpath);
}

//响应工具选择
void Widget::tool_change() {
    // 判断是否挂载了工具
    if (calculator_checkbox->isChecked() || terminal_checkbox->isChecked() || toolguy_checkbox->isChecked() || knowledge_checkbox->isChecked() || controller_checkbox->isChecked() || stablediffusion_checkbox->isChecked() || interpreter_checkbox->isChecked()) {
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
    extra_TextEdit->setText(create_extra_prompt());
}

//选用计算器工具
void Widget::calculator_change() { tool_change(); }

//选用代码解释器工具
void Widget::interpreter_change() { tool_change(); }

//选用系统终端工具
void Widget::terminal_change() { tool_change(); }

void Widget::toolguy_change() { tool_change(); }

void Widget::knowledge_change() { tool_change(); }

void Widget::controller_change() { tool_change(); }

void Widget::stablediffusion_change() { tool_change(); }

//-------------------------------------------------------------------------
//--------------------------------设置设置选项------------------------------
//-------------------------------------------------------------------------
void Widget::set_SetDialog() {
    set_dialog = new QDialog(this);
    set_dialog->setWindowFlags(set_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);  //隐藏?按钮
    set_dialog->resize(150, 200);                                                              // 设置宽度,高度

    QFile file(":/QSS-master/Ubuntu.qss");  //加载皮肤
    file.open(QFile::ReadOnly);
    QString stylesheet = tr(file.readAll());
    set_dialog->setStyleSheet(stylesheet);
    file.close();

    QVBoxLayout *layout = new QVBoxLayout(set_dialog);  //总垂直布局器
    layout->setSpacing(6);                              // 设置布局中子项之间的间隔为0
    layout->setContentsMargins(3, 3, 3, 3);             // 设置布局的边缘间隔为0 (左, 上, 右, 下)
    //------------采样设置---------------
    sample_box = new QGroupBox(jtr("sample set"));  //采样设置区域
    sample_box->setFont(ui_font);
    QVBoxLayout *samlpe_layout = new QVBoxLayout();  //采样设置垂直布局器

    //温度控制
    QHBoxLayout *layout_H1 = new QHBoxLayout();  //水平布局器
    temp_label = new QLabel(jtr("temperature") + " " + QString::number(ui_SETTINGS.temp));
    temp_label->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    temp_label->setMinimumWidth(100);
    layout_H1->addWidget(temp_label);
    temp_slider = new QSlider(Qt::Horizontal);
    temp_slider->setRange(0, 99);  // 设置范围为1到99
    temp_slider->setMinimumWidth(150);
    temp_slider->setValue(ui_SETTINGS.temp * 100.0);
    temp_slider->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    connect(temp_slider, &QSlider::valueChanged, this, &Widget::temp_change);
    layout_H1->addWidget(temp_slider);
    samlpe_layout->addLayout(layout_H1);

    //重复惩罚控制
    QHBoxLayout *layout_H4 = new QHBoxLayout();  //水平布局器
    repeat_label = new QLabel(jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat));
    repeat_label->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    repeat_label->setMinimumWidth(100);
    layout_H4->addWidget(repeat_label);
    repeat_slider = new QSlider(Qt::Horizontal);
    repeat_slider->setRange(0, 200);  // 设置范围
    repeat_slider->setValue(ui_SETTINGS.repeat * 100.0);
    repeat_slider->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    connect(repeat_slider, &QSlider::valueChanged, this, &Widget::repeat_change);
    layout_H4->addWidget(repeat_slider);
    samlpe_layout->addLayout(layout_H4);

    //最大输出长度设置
    QHBoxLayout *layout_H15 = new QHBoxLayout();  //水平布局器
    npredict_label = new QLabel(jtr("npredict") + " " + QString::number(ui_SETTINGS.npredict));
    npredict_label->setToolTip(jtr("The maximum number of tokens that the model can output in a single prediction process"));
    npredict_label->setMinimumWidth(100);
    layout_H15->addWidget(npredict_label);
    npredict_slider = new QSlider(Qt::Horizontal);
    npredict_slider->setRange(1, 8192);  // 设置范围
    npredict_slider->setValue(ui_SETTINGS.npredict);
    npredict_slider->setToolTip(jtr("The maximum number of tokens that the model can output in a single prediction process"));
    connect(npredict_slider, &QSlider::valueChanged, this, &Widget::npredict_change);
    layout_H15->addWidget(npredict_slider);
    samlpe_layout->addLayout(layout_H15);

    sample_box->setLayout(samlpe_layout);
    layout->addWidget(sample_box);

    //------------解码设置---------------
    decode_box = new QGroupBox(jtr("decode set"));  //解码设置区域
    decode_box->setFont(ui_font);
    QVBoxLayout *decode_layout = new QVBoxLayout();  //解码设置垂直布局器

    //加速支持
    QHBoxLayout *layout_H2 = new QHBoxLayout();  //水平布局器
    ngl_label = new QLabel("gpu " + jtr("offload") + QString::number(ui_SETTINGS.ngl));
    ngl_label->setToolTip(jtr("put some model paragram to gpu and reload model"));
    ngl_label->setMinimumWidth(100);
    layout_H2->addWidget(ngl_label);
    ngl_slider = new QSlider(Qt::Horizontal);
    ngl_slider->setRange(0, 99);
    ngl_slider->setValue(ui_SETTINGS.ngl);
    ngl_slider->setToolTip(jtr("put some model paragram to gpu and reload model"));
    layout_H2->addWidget(ngl_slider);
    decode_layout->addLayout(layout_H2);  //将布局添加到总布局
    connect(ngl_slider, &QSlider::valueChanged, this, &Widget::ngl_change);

    // cpu线程数设置
    QHBoxLayout *layout_H16 = new QHBoxLayout();  //水平布局器
    nthread_label = new QLabel("cpu " + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread));
    nthread_label->setToolTip(jtr("not big better"));
    nthread_label->setMinimumWidth(100);
    layout_H16->addWidget(nthread_label);
    nthread_slider = new QSlider(Qt::Horizontal);
    nthread_slider->setToolTip(jtr("not big better"));

    nthread_slider->setValue(ui_SETTINGS.nthread);
    layout_H16->addWidget(nthread_slider);
    decode_layout->addLayout(layout_H16);  //将布局添加到总布局
    connect(nthread_slider, &QSlider::valueChanged, this, &Widget::nthread_change);
    // ctx length 记忆容量
    QHBoxLayout *layout_H3 = new QHBoxLayout();  //水平布局器
    nctx_label = new QLabel(jtr("brain size") + " " + QString::number(ui_SETTINGS.nctx));
    nctx_label->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    nctx_label->setMinimumWidth(100);
    layout_H3->addWidget(nctx_label);
    nctx_slider = new QSlider(Qt::Horizontal);
    nctx_slider->setRange(128, 32768);
    nctx_slider->setValue(ui_SETTINGS.nctx);
    nctx_slider->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    layout_H3->addWidget(nctx_slider);
    decode_layout->addLayout(layout_H3);  //将布局添加到总布局
    connect(nctx_slider, &QSlider::valueChanged, this, &Widget::nctx_change);
    // batch size 批大小
    QHBoxLayout *layout_H13 = new QHBoxLayout();  //水平布局器
    batch_label = new QLabel(jtr("batch size") + " " + QString::number(ui_SETTINGS.batch));
    batch_label->setToolTip(jtr("The number of tokens processed simultaneously in one decoding"));
    batch_label->setMinimumWidth(100);
    // layout_H13->addWidget(batch_label);//暂时不显示
    batch_slider = new QSlider(Qt::Horizontal);
    batch_slider->setRange(1, 2048);
    batch_slider->setValue(ui_SETTINGS.batch);
    batch_slider->setToolTip(jtr("The number of tokens processed simultaneously in one decoding"));
    // layout_H13->addWidget(batch_slider);//暂时不显示
    // decode_layout->addLayout(layout_H13);//暂时不显示
    connect(batch_slider, &QSlider::valueChanged, this, &Widget::batch_change);

    // load lora
    QHBoxLayout *layout_H12 = new QHBoxLayout();  //水平布局器
    lora_label = new QLabel(jtr("load lora"));
    lora_label->setToolTip(jtr("lora_label_tooltip"));
    lora_LineEdit = new QLineEdit();
    lora_LineEdit->setToolTip(jtr("lora_label_tooltip"));
    lora_LineEdit->setPlaceholderText(jtr("right click and choose lora"));

    layout_H12->addWidget(lora_label);
    layout_H12->addWidget(lora_LineEdit);
    decode_layout->addLayout(layout_H12);                    //将布局添加到总布局
    lora_LineEdit->setContextMenuPolicy(Qt::NoContextMenu);  //取消右键菜单
    lora_LineEdit->installEventFilter(this);

    // load mmproj
    QHBoxLayout *layout_H17 = new QHBoxLayout();  //水平布局器
    mmproj_label = new QLabel(jtr("load mmproj"));
    mmproj_label->setToolTip(jtr("mmproj_label_tooltip"));
    mmproj_LineEdit = new QLineEdit();
    mmproj_LineEdit->setToolTip(jtr("mmproj_label_tooltip"));
    mmproj_LineEdit->setPlaceholderText(jtr("right click and choose mmproj"));
    layout_H17->addWidget(mmproj_label);
    layout_H17->addWidget(mmproj_LineEdit);
    decode_layout->addLayout(layout_H17);                      //将布局添加到总布局
    mmproj_LineEdit->setContextMenuPolicy(Qt::NoContextMenu);  //取消右键菜单
    mmproj_LineEdit->installEventFilter(this);

    decode_box->setLayout(decode_layout);
    layout->addWidget(decode_box);

    //------------状态设置---------------
    mode_box = new QGroupBox(jtr("state set"));  //状态设置区域
    mode_box->setFont(ui_font);
    QVBoxLayout *mode_layout = new QVBoxLayout();  //状态设置垂直布局器

    //补完控制
    complete_btn = new QRadioButton(jtr("complete state"));
    complete_btn->setToolTip(jtr("complete_btn_tooltip"));
    complete_btn->setMinimumHeight(20);
    mode_layout->addWidget(complete_btn);
    connect(complete_btn, &QRadioButton::clicked, this, &Widget::complete_change);
    //多轮对话
    chat_btn = new QRadioButton(jtr("chat state"));
    chat_btn->setToolTip(jtr("chat_btn_tooltip"));
    chat_btn->setMinimumHeight(20);
    mode_layout->addWidget(chat_btn);
    chat_btn->setChecked(1);
    connect(chat_btn, &QRadioButton::clicked, this, &Widget::chat_change);
    //网页服务控制
    QHBoxLayout *layout_H10 = new QHBoxLayout();  //水平布局器
    web_btn = new QRadioButton(jtr("server state"));
    web_btn->setToolTip(jtr("web_btn_tooltip"));
    web_btn->setMinimumHeight(20);
    layout_H10->addWidget(web_btn);
    port_label = new QLabel(jtr("port"));
    port_label->setToolTip(jtr("port_label_tooltip"));
    layout_H10->addWidget(port_label);
    port_lineEdit = new QLineEdit();
    port_lineEdit->setToolTip(jtr("port_label_tooltip"));
    port_lineEdit->setText(ui_port);
    QIntValidator *validator = new QIntValidator(0, 65535);  //限制端口输入
    port_lineEdit->setValidator(validator);
    layout_H10->addWidget(port_lineEdit);
    mode_layout->addLayout(layout_H10);  //将布局添加到总布局
    connect(web_btn, &QRadioButton::clicked, this, &Widget::web_change);

    mode_box->setLayout(mode_layout);
    layout->addWidget(mode_box);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, date_dialog);  // 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_set);
    connect(buttonBox, &QDialogButtonBox::rejected, set_dialog, &QDialog::reject);
    set_dialog->setWindowTitle(jtr("set"));
}

//-------------------------------------------------------------------------
//--------------------------------设置约定选项------------------------------
//-------------------------------------------------------------------------
void Widget::set_DateDialog() {
    date_dialog = new QDialog(this);
    date_dialog->setWindowFlags(date_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);  //隐藏?按钮
    // date_dialog->setWindowFlags(date_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);//隐藏关闭按钮
    date_dialog->resize(150, 200);  // 设置宽度,高度

    // QFile file(":/QSS-master/MacOS.qss");//加载皮肤
    // file.open(QFile::ReadOnly);QString stylesheet = tr(file.readAll());
    // date_dialog->setStyleSheet(stylesheet);file.close();

    QVBoxLayout *layout = new QVBoxLayout(date_dialog);  //垂直布局器
    layout->setSpacing(6);                               // 设置布局中子项之间的间隔为0
    layout->setContentsMargins(3, 3, 3, 3);              // 设置布局的边缘间隔为0 (左, 上, 右, 下)
    // layout->setSizeConstraint(QLayout::SetFixedSize);//使得自动调整紧凑布局
    //------------提示词模板设置---------------
    prompt_box = new QGroupBox(jtr("prompt") + jtr("template"));  //提示词模板设置区域
    prompt_box->setFont(ui_font);
    QVBoxLayout *prompt_layout = new QVBoxLayout();  //提示词模板设置垂直布局器

    //预设模板
    QHBoxLayout *layout_H9 = new QHBoxLayout();  //水平布局器
    chattemplate_label = new QLabel(jtr("chat template"));
    chattemplate_label->setToolTip(jtr("chattemplate_label_tooltip"));
    chattemplate_label->setFixedSize(60, 30);
    layout_H9->addWidget(chattemplate_label);
    chattemplate_comboBox = new QComboBox();
    chattemplate_comboBox->setToolTip(jtr("chattemplate_label_tooltip"));
    chattemplate_comboBox->setMinimumWidth(200);
    for (const QString &key : date_map.keys()) {
        chattemplate_comboBox->addItem(key);
    }
    chattemplate_comboBox->addItem(jtr("custom set1"));  //添加自定义模板
    chattemplate_comboBox->addItem(jtr("custom set2"));  //添加自定义模板
    chattemplate_comboBox->setCurrentText(ui_template);  //默认使用default的提示词模板
    connect(chattemplate_comboBox, &QComboBox::currentTextChanged, this, &Widget::prompt_template_change);
    layout_H9->addWidget(chattemplate_comboBox);
    prompt_layout->addLayout(layout_H9);  //将布局添加到总布局
    //系统指令
    QHBoxLayout *layout_H11 = new QHBoxLayout();  //水平布局器
    date_prompt_label = new QLabel(jtr("date prompt"));
    date_prompt_label->setToolTip(jtr("date_prompt_label_tooltip"));
    date_prompt_label->setFixedSize(60, 30);
    layout_H11->addWidget(date_prompt_label);
    date_prompt_TextEdit = new QTextEdit();
    date_prompt_TextEdit->setToolTip(jtr("date_prompt_label_tooltip"));
    // 设置样式表
    // date_prompt_TextEdit->setStyleSheet("QTextEdit {"
    //                     "border: 1px solid black;"   // 边框宽度为1px, 颜色为黑色
    //                     "border-radius: 5px;"        // 边框圆角为5px
    //                     "padding: 1px;"              // 内边距为1px
    //                     "}");
    layout_H11->addWidget(date_prompt_TextEdit);
    prompt_layout->addLayout(layout_H11);  //将布局添加到总布局
    //输入前缀设置
    QHBoxLayout *layout_H5 = new QHBoxLayout();  //水平布局器
    user_name_label = new QLabel(jtr("user name"));
    user_name_label->setToolTip(jtr("user_name_label_tooltip"));
    user_name_label->setFixedSize(60, 30);
    layout_H5->addWidget(user_name_label);
    user_name_LineEdit = new QLineEdit();
    user_name_LineEdit->setToolTip(jtr("user_name_label_tooltip"));
    user_name_LineEdit->setText(ui_DATES.user_name);
    layout_H5->addWidget(user_name_LineEdit);
    prompt_layout->addLayout(layout_H5);  //将布局添加到总布局
    //输入后缀设置
    QHBoxLayout *layout_H6 = new QHBoxLayout();  //水平布局器
    model_name_label = new QLabel(jtr("model name"));
    model_name_label->setToolTip(jtr("model_name_label_tooltip"));
    model_name_label->setFixedSize(60, 30);
    layout_H6->addWidget(model_name_label);
    model_name_LineEdit = new QLineEdit();
    model_name_LineEdit->setToolTip(jtr("model_name_label_tooltip"));
    model_name_LineEdit->setText(ui_DATES.model_name);
    layout_H6->addWidget(model_name_LineEdit);
    prompt_layout->addLayout(layout_H6);  //将布局添加到垂直布局

    prompt_box->setLayout(prompt_layout);
    layout->addWidget(prompt_box);

    //------------工具设置---------------
    tool_box = new QGroupBox(jtr("mount") + jtr("tool"));  //提示词模板设置区域
    tool_box->setFont(ui_font);
    QVBoxLayout *tool_layout = new QVBoxLayout();  //提示词模板设置垂直布局器
    //可用工具
    QHBoxLayout *layout_H44 = new QHBoxLayout();  //水平布局器
    calculator_checkbox = new QCheckBox(jtr("calculator"));
    calculator_checkbox->setToolTip(jtr("calculator_checkbox_tooltip"));
    terminal_checkbox = new QCheckBox(jtr("terminal"));
    terminal_checkbox->setToolTip(jtr("terminal_checkbox_tooltip"));
    layout_H44->addWidget(calculator_checkbox);
    layout_H44->addWidget(terminal_checkbox);
    tool_layout->addLayout(layout_H44);  //将布局添加到垂直布局

    QHBoxLayout *layout_H45 = new QHBoxLayout();  //水平布局器
    controller_checkbox = new QCheckBox(jtr("controller"));
    controller_checkbox->setToolTip(jtr("controller_checkbox_tooltip"));
    knowledge_checkbox = new QCheckBox(jtr("knowledge"));
    knowledge_checkbox->setToolTip(jtr("knowledge_checkbox_tooltip"));
    layout_H45->addWidget(knowledge_checkbox);
    layout_H45->addWidget(controller_checkbox);
    tool_layout->addLayout(layout_H45);  //将布局添加到垂直布局

    QHBoxLayout *layout_H46 = new QHBoxLayout();  //水平布局器
    toolguy_checkbox = new QCheckBox(jtr("toolguy"));
    toolguy_checkbox->setToolTip(jtr("toolguy_checkbox_tooltip"));
    interpreter_checkbox = new QCheckBox(jtr("interpreter"));
    interpreter_checkbox->setToolTip(jtr("interpreter_checkbox_tooltip"));
    stablediffusion_checkbox = new QCheckBox(jtr("stablediffusion"));
    stablediffusion_checkbox->setToolTip(jtr("stablediffusion_checkbox_tooltip"));
    layout_H46->addWidget(stablediffusion_checkbox);
    // layout_H46->addWidget(toolguy_checkbox);//暂不显示
    layout_H46->addWidget(interpreter_checkbox);
    tool_layout->addLayout(layout_H46);  //将布局添加到垂直布局

    connect(calculator_checkbox, &QCheckBox::stateChanged, this, &Widget::calculator_change);
    connect(terminal_checkbox, &QCheckBox::stateChanged, this, &Widget::terminal_change);
    connect(toolguy_checkbox, &QCheckBox::stateChanged, this, &Widget::toolguy_change);
    connect(knowledge_checkbox, &QCheckBox::stateChanged, this, &Widget::knowledge_change);
    connect(controller_checkbox, &QCheckBox::stateChanged, this, &Widget::controller_change);
    connect(stablediffusion_checkbox, &QCheckBox::stateChanged, this, &Widget::stablediffusion_change);
    connect(interpreter_checkbox, &QCheckBox::stateChanged, this, &Widget::interpreter_change);

    //附加指令
    QHBoxLayout *layout_H55 = new QHBoxLayout();  //水平布局器
    extra_label = new QLabel(jtr("extra calling"));
    extra_label->setToolTip(jtr("extra_label_tooltip"));
    layout_H55->addWidget(extra_label);
    if (language_flag == 0) {
        ui_extra_lan = "zh";
    }
    if (language_flag == 1) {
        ui_extra_lan = "en";
    }
    switch_lan_button = new QPushButton(ui_extra_lan);
    switch_lan_button->setToolTip(jtr("switch_lan_button_tooltip"));
    switch_lan_button->setMinimumWidth(200);
    switch_lan_button->setMinimumHeight(20);
    layout_H55->addWidget(switch_lan_button);
    extra_TextEdit = new QTextEdit();
    extra_TextEdit->setPlaceholderText(jtr("extra_TextEdit_tooltip"));
    extra_TextEdit->setToolTip(jtr("extra_TextEdit_tooltip"));
    // 设置样式表
    // extra_TextEdit->setStyleSheet("QTextEdit {"
    //                     "border: 1px solid black;"   // 边框宽度为1px, 颜色为黑色
    //                     "border-radius: 5px;"        // 边框圆角为5px
    //                     "padding: 1px;"              // 内边距为1px
    //                     "}");
    tool_layout->addLayout(layout_H55);  //将布局添加到总布局
    tool_layout->addWidget(extra_TextEdit);

    tool_box->setLayout(tool_layout);
    layout->addWidget(tool_box);
    connect(switch_lan_button, &QPushButton::clicked, this, &Widget::switch_lan_change);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, date_dialog);  // 创建 QDialogButtonBox 用于确定按钮

    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_date);// 点击确定按钮
    connect(buttonBox, &QDialogButtonBox::rejected, date_dialog, &QDialog::reject);// 点击取消按钮，实现对话框关闭
    connect(buttonBox, &QDialogButtonBox::rejected, this, &Widget::cancel_date);// 点击取消按钮，采取后续操作
    connect(date_dialog, &QDialog::rejected, this, &Widget::cancel_date);// 点击关闭按钮，采取后续操作
    
    prompt_template_change();  //先应用提示词模板
    date_prompt_TextEdit->setText(ui_date_prompt);
    date_dialog->setWindowTitle(jtr("date"));
}

//-------------------------------------------------------------------------
//--------------------------------设置api选项------------------------------
//-------------------------------------------------------------------------
void Widget::setApiDialog() {
    api_dialog = new QDialog();
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);  //隐藏?按钮
    api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);        //隐藏关闭按钮
    api_dialog->resize(250, 100);                                                              // 设置宽度为400像素,高度为200像素

    QVBoxLayout *layout = new QVBoxLayout(api_dialog);  //垂直布局器

    QHBoxLayout *layout_H1 = new QHBoxLayout();  //水平布局器
    api_endpoint_label = new QLabel(jtr("api endpoint"));
    layout_H1->addWidget(api_endpoint_label);
    api_endpoint_LineEdit = new QLineEdit();
    api_endpoint_LineEdit->setPlaceholderText(jtr("input server ip"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_endpoint_LineEdit->setText(apis.api_endpoint);
    QRegExp ipRegex("^((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)\\.){3}"
                      "(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d):"
                      "(6553[0-5]|655[0-2]\\d|65[0-4]\\d{2}|6[0-4]\\d{3}|"
                      "[1-5]?\\d{1,4})$");  // IPv4地址冒号端口号的正则表达式限制
    QRegExpValidator *validator_ipv4 = new QRegExpValidator(ipRegex, api_endpoint_LineEdit);
    api_endpoint_LineEdit->setValidator(validator_ipv4);
    layout_H1->addWidget(api_endpoint_LineEdit);
    layout->addLayout(layout_H1);  //将布局添加到总布局

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, api_dialog);  // 创建 QDialogButtonBox 用于确定和取消按钮
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &Widget::set_api);
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
    //判断ip地址是否合理
    if (api_endpoint_LineEdit->text().contains("0.0") || api_endpoint_LineEdit->text().split(".").size() < 3 || api_endpoint_LineEdit->text() == "0.0.0.0") {
        ui_state_info = "ui:api wrong";
        reflash_state(ui_state_info, WRONG_SIGNAL);
        return;
    }
    reflash_state("ui:" + jtr("detecting") + "api...", SIGNAL_SIGNAL);
    emit ui2bot_free(0);  //释放原来的模型
    is_load = false;

    //获取设置值
    apis.api_endpoint = api_endpoint_LineEdit->text();
    if(apis.api_endpoint.contains(":"))
    {
        apis.api_ip = apis.api_endpoint.split(":")[0];
        apis.api_port= apis.api_endpoint.split(":")[1];
    }
    else
    {
        apis.api_ip = apis.api_endpoint;
    }
    startConnection(apis.api_ip, apis.api_port.toInt());  //检测ip是否通畅
}

void Widget::startConnection(const QString &ip, int port) {
    // socket should be a member variable or should be managed to ensure its lifetime
    // during the asynchronous operation
    api_dialog->setDisabled(1);  //阻塞界面
    QTcpSocket *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &Widget::onConnected);  //链接成功的后处理动作
    connect(socket, &QTcpSocket::errorOccurred, this, &Widget::onError);  //链接失败的后处理动作
    socket->connectToHost(ip, port);
}

void Widget::keepConnection() {
    keeptime.restart();
    QTcpSocket *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &Widget::keep_onConnected);
    connect(socket, &QTcpSocket::errorOccurred, this, &Widget::keep_onError);
    socket->connectToHost(apis.api_ip, apis.api_port.toInt());
}

// 连接成功
void Widget::onConnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket) {
        socket->disconnectFromHost();
    }                     //中断访问
    ui_mode = LINK_MODE;  //按照链接模式的行为来
    reflash_state("ui:" + jtr("eva link"), EVA_SIGNAL);
    if (ui_state == CHAT_STATE) {
        current_api = "http://" + apis.api_endpoint + apis.api_chat_endpoint;
    } else {
        current_api = "http://" + apis.api_endpoint + apis.api_completion_endpoint;
    }
    reflash_state("ui:" + jtr("current api") + " " + current_api, USUAL_SIGNAL);
    this->setWindowTitle(jtr("current api") + " " + current_api);
    QApplication::setWindowIcon(QIcon(":/logo/dark_logo.png"));  //设置应用程序图标
    ui->kv_bar->show_text = jtr("delay");
    ui->kv_bar->setToolTip("");

    emit ui2net_apis(apis);
    reflash_output(ui_DATES.date_prompt, 0, SYSTEM_BLUE);
    ui_state_normal();

    api_dialog->setDisabled(0);
    api_dialog->close();

    keeptimer->start(3000);  //每多少秒测一次延迟，频率太高会让服务端爆炸
}
//连接失败
void Widget::onError(QAbstractSocket::SocketError socketError) {
    // Handle the error
    ui_mode = LOCAL_MODE;
    reflash_state("ui:api" + jtr("port") + jtr("blocked"), WRONG_SIGNAL);
    this->setWindowTitle(jtr("eva"));
    api_dialog->setDisabled(0);
    api_dialog->close();
}
//链接模式下工具返回结果时延迟发送
void Widget::tool_testhandleTimeout() {
    ENDPOINT_DATA data;
    data.date_prompt = ui_DATES.date_prompt;
    data.input_pfx = ui_DATES.user_name;
    data.input_sfx = ui_DATES.model_name;
    data.stopwords = ui_DATES.extra_stop_words;
    if (ui_state == COMPLETE_STATE) {
        data.complete_state = true;
    } else {
        data.complete_state = false;
    }
    data.temp = ui_SETTINGS.temp;
    data.repeat = ui_SETTINGS.repeat;
    data.n_predict = ui_SETTINGS.npredict;
    data.insert_history = ui_insert_history;

    emit ui2net_data(data);
    emit ui2net_push();
}

void Widget::send_testhandleTimeout() { on_send_clicked(); }

// 每多少秒测一次延迟,回应时间/keeptest*100为延迟量
void Widget::keep_onConnected() {
    float percent = keeptime.nsecsElapsed() / 1000000000.0 / keeptesttime;
    // qDebug() << keeptime.nsecsElapsed()/1000000000.0<<keeptesttime<<percent;
    if (percent < 1 && percent > 0) {
        percent = 1;
    }
    ui->kv_bar->setSecondValue(percent);
}

//每多少秒测一次延迟,回应时间/keeptest*100为延迟量
void Widget::keep_onError(QAbstractSocket::SocketError socketError) {
    if (socketError != QAbstractSocket::RemoteHostClosedError) {
        ui->kv_bar->setSecondValue(100);
    }
}

//链接模式切换时某些控件可见状态
void Widget::change_api_dialog(bool enable) {
    repeat_label->setVisible(enable);
    repeat_slider->setVisible(enable);
    nctx_label->setVisible(enable);
    nctx_slider->setVisible(enable);
    nthread_label->setVisible(enable);
    nthread_slider->setVisible(enable);
    // batch_label->setVisible(enable);batch_slider->setVisible(enable);
    mmproj_label->setVisible(enable);
    mmproj_LineEdit->setVisible(enable);
    ngl_label->setVisible(enable);
    ngl_slider->setVisible(enable);
    lora_label->setVisible(enable);
    lora_LineEdit->setVisible(enable);
    port_label->setVisible(enable);
    port_lineEdit->setVisible(enable);
    web_btn->setVisible(enable);
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
    ui->input->setFocus();     //设置输入区为焦点
}

//推理中界面状态
void Widget::ui_state_pushing() {
    decode_play();  //开启推理动画
    ui->load->setEnabled(0);
    ui->date->setEnabled(0);
    ui->set->setEnabled(0);
    if (ui_syncrate_manager.is_sync) {
        ui->reset->setEnabled(0);
    } else {
        ui->reset->setEnabled(1);
    }

    ui->send->setEnabled(0);
}

//服务中界面状态
void Widget::ui_state_servering() {
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
        ui->input->setEnabled(1);
        ui->input->setReadOnly(0);
        ui->send->setEnabled(0);
        ui->input->setPlaceholderText(jtr("chat or right click to choose question"));
        ui->input->setStyleSheet("background-color: white;");
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
        ui->date->setEnabled(1);
        ui->set->setEnabled(1);
        ui->input->setVisible(1);
        ui->send->setVisible(1);

        ui->input->setPlaceholderText(jtr("chat or right click to choose question"));
        ui->input->setStyleSheet("background-color: white;");
        ui->input->setReadOnly(0);
        ui->input->setFocus();  //设置输入区为焦点
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

        ui->input->clear();
        ui->input->setPlaceholderText(jtr("Please modify any text above"));
        ui->input->setStyleSheet("background-color: rgba(255, 165, 0, 127);");  //设置背景为橘黄色
        ui->input->setReadOnly(1);
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
        ui->input->setFocus();
        ui->input->clear();
        ui->input->setStyleSheet("background-color: rgba(144, 238, 144, 127);");  //透明绿色
        ui->input->setReadOnly(1);
        ui->input->setFocus();  //设置输入区为焦点
        ui->input->setPlaceholderText(jtr("recoding") + "... " + jtr("push f2 to stop"));
    } else {
        ui->input->setPlaceholderText(jtr("recoding") + "... " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s " + jtr("push f2 to stop"));
    }
}

//添加右击问题
void Widget::create_right_menu() {
    QDate currentDate = QDate::currentDate();  //历史中的今天
    QString dateString = currentDate.toString("MM" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    //---------------创建一般问题菜单--------------
    if (right_menu != nullptr) {
        delete right_menu;
    }
    right_menu = new QMenu(this);
    for (int i = 1; i < 14; ++i) {

        QString question;

        if (i == 4) {question = jtr(QString("Q%1").arg(i)).replace("{today}", dateString);}  //历史中的今天
        else {question = jtr(QString("Q%1").arg(i));}
        QAction *action = right_menu->addAction(question);

        connect(action, &QAction::triggered, this, [=]() { ui->input->setPlainText(question); });

    }
    //------------创建自动化问题菜单-------------
    // Q14同步率测试
    QAction *action14 = right_menu->addAction(jtr("Q14"));
    connect(action14, &QAction::triggered, this, [=]() {
        if (is_run || !is_load || !is_load_play_over || ui_mode != LOCAL_MODE || ui_state != CHAT_STATE) {
            return;
        }  //只在空闲的本地模式和对话状态中生效

        ui_syncrate_manager.is_sync = true;

        //插入任务
        for (int i = 1; i < 31; ++i) {
            ui_syncrate_manager.sync_list_question << jtr(QString("sync_Q%1").arg(i));
            ui_syncrate_manager.sync_list_index.append(i);
        }

        // 自动约定，挂载所有工具
        chattemplate_comboBox->setCurrentText("default");  //默认使用default的提示词模板
        calculator_checkbox->setChecked(1);
        terminal_checkbox->setChecked(1);
        controller_checkbox->setChecked(1);
        knowledge_checkbox->setChecked(0);
        knowledge_checkbox->setChecked(1);  // 刷新一下
        stablediffusion_checkbox->setChecked(1);
        interpreter_checkbox->setChecked(1);
        get_date();                  //获取约定中的纸面值
        emit ui2bot_date(ui_DATES);  // 注意在开始同步率测试前会强制预解码一次
    });
    //上传图像
    QAction *action15 = right_menu->addAction(jtr("Q15"));
    connect(action15, &QAction::triggered, this, [=]() {
        if (is_run || (ui_mode == LOCAL_MODE && !is_load)) {
            return;
        }  //只在空闲的对话模式生效

        //用户选择图片
        currentpath = customOpenfile(currentpath, jtr("Q15"), "(*.png *.jpg *.bmp)");

        if (currentpath == "") {
            return;
        }

        if (ui->send->isEnabled()) {
            showImage(currentpath);//显示文件名和图像
            is_run = true;       //模型正在运行标签
            ui_state_pushing();  //推理中界面状态
            emit ui2bot_preDecodeImage(currentpath); //预解码图像
        }                   
        
    });
    // Q16测试相关,ceval数据集
    QAction *action16 = right_menu->addAction(jtr("Q16"));
    connect(action16, &QAction::triggered, this, [=]() {
        if (is_run || (ui_mode == LOCAL_MODE && !is_load)) {
            return;
        }  //只在空闲的对话模式生效

        clearQuestionlist();  //清空题库

        //构建测试问题集
        if (language_flag == 0) {
            makeTestQuestion(":/ceval-exam/val");
        } else {
            makeTestQuestion(":/mmlu-exam/val");
        }

        makeTestIndex();                                        //构建测试问题索引
        QApplication::setWindowIcon(QIcon(":/logo/c-eval.png"));  // 设置应用程序图标
        this->setWindowTitle(jtr("test") + "0/" + QString::number(test_list_question.size()) + "   " + ui_SETTINGS.modelpath.split("/").last());

        reflash_state("ui:" + jtr("Question bank construction completed") + " " + QString::number(test_list_question.size()) + jtr("question"), USUAL_SIGNAL);
        reflash_state("ui:" + jtr("clicked") + jtr("test") + " " + jtr("npredict") + jtr("limited") + "1", USUAL_SIGNAL);
        reflash_state("ui:" + jtr("add help question"), SIGNAL_SIGNAL);

        test_time.restart();
        is_test = true;
        help_input = true;
        ui->send->click();  //触发一次发送
    });
    // Q17测试相关,自定义数据集
    QAction *action17 = right_menu->addAction(jtr("Q17"));
    connect(action17, &QAction::triggered, this, [=]() {
        if (is_run || (ui_mode == LOCAL_MODE && !is_load)) {
            return;
        }  //只在空闲的对话模式生效
        //用户选择自定义的csv文件
        currentpath = customOpenfile(currentpath, jtr("Q17"), "CSV files (*.csv)");

        if (currentpath == "") {
            return;
        }

        clearQuestionlist();       //清空题库
        readCsvFile(currentpath);  //构建测试问题集
        makeTestIndex();           //构建测试问题索引
        if (test_question_index.size() == 0) {
            reflash_state("ui:0" + jtr("question"), WRONG_SIGNAL);
            return;
        }
        QApplication::setWindowIcon(QIcon(":/logo/c-eval.png"));  // 设置应用程序图标
        this->setWindowTitle(jtr("test") + "0/" + QString::number(test_list_question.size()) + "   " + ui_SETTINGS.modelpath.split("/").last());

        reflash_state("ui:" + jtr("Question bank construction completed") + " " + QString::number(test_list_question.size()) + jtr("question"), USUAL_SIGNAL);
        reflash_state("ui:" + jtr("clicked") + jtr("test") + " " + jtr("npredict") + jtr("limited") + "1", USUAL_SIGNAL);
        reflash_state("ui:" + jtr("add help question"), SIGNAL_SIGNAL);

        test_time.restart();
        is_test = true;
        help_input = true;
        ui->send->click();  //触发一次发送
    });
}

//获取设置中的纸面值
void Widget::get_set() {
    ui_SETTINGS.temp = temp_slider->value() / 100.0;
    ui_SETTINGS.repeat = repeat_slider->value() / 100.0;
    ui_SETTINGS.npredict = npredict_slider->value();

    ui_SETTINGS.nthread = nthread_slider->value();
    ui_SETTINGS.nctx = nctx_slider->value();    //获取nctx滑块的值
    ui_SETTINGS.batch = batch_slider->value();  //获取nctx滑块的值
    ui_SETTINGS.ngl = ngl_slider->value();  //获取npl滑块的值

    ui_SETTINGS.lorapath = lora_LineEdit->text();
    ui_SETTINGS.mmprojpath = mmproj_LineEdit->text();

    ui_SETTINGS.complete_mode = complete_btn->isChecked();
    if (chat_btn->isChecked()) {
        ui_state = CHAT_STATE;
    } else if (complete_btn->isChecked()) {
        ui_state = COMPLETE_STATE;
        history_prompt = "";
    }  // history_prompt置空是为了下一次切换为对话模式时正确处理预解码
    else if (web_btn->isChecked()) {
        ui_state = SERVER_STATE;
    }
    ui_port = port_lineEdit->text();
}

//获取约定中的纸面值
void Widget::get_date() {
    ui_extra_prompt = extra_TextEdit->toPlainText();
    ui_date_prompt = date_prompt_TextEdit->toPlainText();
    //合并附加指令
    if (ui_extra_prompt != "") {
        ui_DATES.date_prompt = ui_date_prompt + "\n\n" + ui_extra_prompt;
    } else {
        ui_DATES.date_prompt = ui_date_prompt;
    }

    ui_DATES.user_name = user_name_LineEdit->text();
    ui_DATES.model_name = model_name_LineEdit->text();

    ui_DATES.is_load_tool = is_load_tool;
    ui_template = chattemplate_comboBox->currentText();
    ui_extra_lan = switch_lan_button->text();

    ui_calculator_ischecked = calculator_checkbox->isChecked();
    ui_terminal_ischecked = terminal_checkbox->isChecked();
    ui_toolguy_ischecked = toolguy_checkbox->isChecked();
    ui_knowledge_ischecked = knowledge_checkbox->isChecked();
    ui_controller_ischecked = controller_checkbox->isChecked();
    ui_stablediffusion_ischecked = stablediffusion_checkbox->isChecked();
    ui_interpreter_ischecked = interpreter_checkbox->isChecked();


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