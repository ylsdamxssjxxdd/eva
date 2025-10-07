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
void Widget::init_movie()
{
    movie_line << "                        ##                        "; // 2
    movie_line << "                      #    #                      "; // 3
    movie_line << "                        ##                        "; // 4
    movie_line << "     ##                                    ##     "; // 5
    movie_line << "   #    #                                #    #   "; // 6
    movie_line << "     ##                                    ##     "; // 7
    movie_line << "                        ##                        "; // 8
    movie_line << "                      #    #                      "; // 9
    movie_line << "                        ##                        "; // 10
    movie_line << "     ##                                    ##     "; // 11
    movie_line << "   #    #                                #    #   "; // 12
    movie_line << "     ##                                    ##     "; // 13
    movie_line << "                        ##                        "; // 14
    movie_line << "                      #    #                      "; // 15
    movie_line << "                        ##                        "; // 16

    movie_dot << QPointF(5, 12) << QPointF(4, 15) << QPointF(3, 19) << QPointF(2, 22);                                                                                                                  // load_action 1
    movie_dot << QPointF(5, 12) << QPointF(6, 15) << QPointF(7, 19) << QPointF(8, 22);                                                                                                                  // load_action 2
    movie_dot << QPointF(5, 12) << QPointF(6, 13) << QPointF(7, 14) << QPointF(8, 15) << QPointF(9, 16) << QPointF(10, 17) << QPointF(11, 18) << QPointF(12, 19) << QPointF(13, 20) << QPointF(14, 22); // load_action 3

    movie_dot << QPointF(11, 12) << QPointF(10, 13) << QPointF(9, 14) << QPointF(8, 15) << QPointF(7, 16) << QPointF(6, 17) << QPointF(5, 18) << QPointF(4, 19) << QPointF(3, 20) << QPointF(2, 22); // load_action 4
    movie_dot << QPointF(11, 12) << QPointF(10, 15) << QPointF(9, 19) << QPointF(8, 22);                                                                                                             // load_action 5
    movie_dot << QPointF(11, 12) << QPointF(12, 15) << QPointF(13, 19) << QPointF(14, 22);                                                                                                           // load_action 6

    movie_dot << QPointF(2, 31) << QPointF(3, 34) << QPointF(4, 38) << QPointF(5, 41);                                                                                                               // load_action 7
    movie_dot << QPointF(2, 31) << QPointF(3, 33) << QPointF(4, 34) << QPointF(5, 35) << QPointF(6, 36) << QPointF(7, 37) << QPointF(8, 38) << QPointF(9, 39) << QPointF(10, 40) << QPointF(11, 41); // load_action 8

    movie_dot << QPointF(8, 31) << QPointF(7, 34) << QPointF(6, 38) << QPointF(5, 41);   // load_action 9
    movie_dot << QPointF(8, 31) << QPointF(9, 34) << QPointF(10, 38) << QPointF(11, 41); // load_action 10

    movie_dot << QPointF(14, 31) << QPointF(13, 33) << QPointF(12, 34) << QPointF(11, 35) << QPointF(10, 36) << QPointF(9, 37) << QPointF(8, 38) << QPointF(7, 39) << QPointF(6, 40) << QPointF(5, 41); // load_action 11
    movie_dot << QPointF(14, 31) << QPointF(13, 34) << QPointF(12, 38) << QPointF(11, 41);                                                                                                              // load_action 12

    //添加颜色
    for (int i = 0; i < 12; ++i)
    {
        //彩色
        // movie_color <<QColor(QRandomGenerator::global()->bounded(256), QRandomGenerator::global()->bounded(256), QRandomGenerator::global()->bounded(256));
        //黑色
        movie_color << NORMAL_BLACK;
    }

    //设置动画内容字体格式
    movie_format.setFontWeight(QFont::Bold); // 设置粗体
    movie_font.setPointSize(6);
    // movie_font.setFamily(DEFAULT_FONT);
    movie_format.setFont(movie_font);

    load_pTimer = new QTimer(this);                                                        //连接接动画
    load_begin_pTimer = new QTimer(this);                                                  //向中滑动
    load_over_pTimer = new QTimer(this);                                                   //向下滑动
    force_unlockload_pTimer = new QTimer(this);                                            //强制解锁
    connect(load_pTimer, SIGNAL(timeout()), this, SLOT(load_handleTimeout()));             //设置终止信号触发的槽函数
    connect(load_begin_pTimer, SIGNAL(timeout()), this, SLOT(load_begin_handleTimeout())); //设置终止信号触发的槽函数
    connect(load_over_pTimer, SIGNAL(timeout()), this, SLOT(load_over_handleTimeout()));   //设置终止信号触发的槽函数
    connect(force_unlockload_pTimer, SIGNAL(timeout()), this, SLOT(unlockLoad()));         //新开一个线程

    decode_pTimer = new QTimer(this);                                              //启动后,达到规定时间将发射终止信号
    connect(decode_pTimer, SIGNAL(timeout()), this, SLOT(decode_handleTimeout())); //设置终止信号触发的槽函数
}

//设置72个点的字体前景色颜色
void Widget::set_dotcolor(QTextCharFormat *format, int load_action)
{
    if (load_action < 4)
    {
        format->setForeground(movie_color.at(0));
    }
    else if (load_action < 8)
    {
        format->setForeground(movie_color.at(1));
    }
    else if (load_action < 18)
    {
        format->setForeground(movie_color.at(2));
    }
    else if (load_action < 28)
    {
        format->setForeground(movie_color.at(3));
    }
    else if (load_action < 32)
    {
        format->setForeground(movie_color.at(4));
    }
    else if (load_action < 36)
    {
        format->setForeground(movie_color.at(5));
    }
    else if (load_action < 40)
    {
        format->setForeground(movie_color.at(6));
    }
    else if (load_action < 50)
    {
        format->setForeground(movie_color.at(7));
    }
    else if (load_action < 54)
    {
        format->setForeground(movie_color.at(8));
    }
    else if (load_action < 58)
    {
        format->setForeground(movie_color.at(9));
    }
    else if (load_action < 68)
    {
        format->setForeground(movie_color.at(10));
    }
    else if (load_action < 72)
    {
        format->setForeground(movie_color.at(11));
    }
}

//连接动画的下一帧
void Widget::load_move()
{
    QTextCursor cursor = ui->state->textCursor();

    if (load_action % 2 == 0)
    {
        cursor.movePosition(QTextCursor::Start);                                                                                 //移到文本开头
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2).x() - 2 + playlineNumber); //向下移动到指定行
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2).y() - 1);                 //向右移动到指定列
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);                                                         //选中当前字符
        cursor.removeSelectedText();                                                                                             //删除选中字符
        set_dotcolor(&movie_format, load_action / 2);                                                                            //设置字体颜色
        cursor.setCharFormat(movie_format);                                                                                      //设置字体
        cursor.insertText("*");                                                                                                  //插入字符
    }
    else
    {
        cursor.movePosition(QTextCursor::Start);                                                                                     //移到文本开头
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2 + 1).x() - 2 + playlineNumber); //向下移动到指定行
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, movie_dot.at(load_action / 2 + 1).y() - 1);                 //向右移动到指定列
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);                                                             //选中当前字符
        cursor.removeSelectedText();                                                                                                 //删除选中字符
        cursor.setCharFormat(movie_format);                                                                                          //设置字体
        cursor.insertText(" ");                                                                                                      //插入字符
    }

    load_action++;
}

//开始播放
void Widget::load_play()
{
    // 简化装载动画：不再播放复杂 ASCII/连线动画，改为复用“解码转轮”
    if (decode_pTimer && decode_pTimer->isActive()) decode_pTimer->stop();
    wait_play("load model");
}

//连接动画
void Widget::load_handleTimeout()
{
    if (load_pTimer->isActive())
    {
        load_pTimer->stop();
    } //控制超时处理函数只会处理一次
    if (load_action < all_fps)
    {
        load_move(); //下一帧
    }

    //循环播放
    if (load_action < all_fps)
    {
        if (is_load)
        {
            load_pTimer->start(10); //延时多少ms后发出timeout()信号
        }
        else
        {
            load_pTimer->start(1100); //延时多少ms后发出timeout()信号
        }
    }
    else if (is_load)
    {
        load_action = 0; //重置动作计数
        all_fps--;       //减去补上的最后一帧
        load_over_pTimer->start(100);
    }
}

//滑动到最佳动画位置
void Widget::load_begin_handleTimeout()
{
    if (load_begin_pTimer->isActive())
    {
        load_begin_pTimer->stop();
    }                                                           //控制超时处理函数只会处理一次
    int currentValue = ui->state->verticalScrollBar()->value(); //当前滑动条位置
    ui->state->verticalScrollBar()->setValue(currentValue + 1);
    // qDebug() << currentValue <<playlineNumber;
    if (currentValue < playlineNumber - 2)
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
    if (load_over_pTimer->isActive())
    {
        load_over_pTimer->stop();
    } //控制超时处理函数只会处理一次

    int currentValue = ui->state->verticalScrollBar()->value(); //当前滑动条位置
    ui->state->verticalScrollBar()->setValue(currentValue + 1);
    currentValue++;
    //展示完就停止
    if (currentValue <= ui->state->verticalScrollBar()->maximum())
    {
        load_over_pTimer->start(100);
    }
    //滚到最下面才解锁按钮,真正装载完毕
    else
    {
        force_unlockload_pTimer->start(0); //强制解锁
    }

    if (ui_monitor_frame > 0 && ui_state == CHAT_STATE)
    {
        qDebug() << "开始监视..." << ui_monitor_frame;
        monitor_timer.start(1000 / ui_monitor_frame);
    }
    else
    {
        monitor_timer.stop();
    }
}

// 装载完毕强制预处理
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
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }                                // 设置应用程序图标
    else
    {
        EVA_icon = QIcon(":/logo/blue_logo.png");
        QApplication::setWindowIcon(EVA_icon);
        trayIcon->setIcon(EVA_icon); // 设置系统托盘图标
    }                                // 设置应用程序图标
    EVA_title = jtr("current model") + " " + ui_SETTINGS.modelpath.split("/").last();
    this->setWindowTitle(EVA_title);
    trayIcon->setToolTip(EVA_title);
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(max_thread));
    auto_save_user(); //保存ui配置
    force_unlockload_pTimer->stop();
    is_load_play_over = true; //标记模型动画已经完成
    ui_state_normal();        //解锁界面
    reflash_output(bot_predecode_content, 0, SYSTEM_BLUE);
    ; //显示预解码内容
}

// 按日志显示装载进度
void Widget::load_log_play()
{
    int load_count = load_percent * all_fps / 100;
    // qDebug() << load_count;
    while (load_count > load_action && ui_SETTINGS.ngl != 0)
    {
        load_move();
    }
}
