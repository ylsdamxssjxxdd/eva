//主函数和主要槽函数

#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    //---------------初始化ui--------------
    ui->setupUi(this);

    //--------------初始化语言--------------
    QLocale locale = QLocale::system(); // 获取系统locale
    QLocale::Language language = locale.language(); // 获取语言
    if(locale.languageToString(language) == "English"){getWords(":/english.json");}
    else{getWords(":/chinese.json");}

    //-------------初始化约定模板-------------
    ui_system_prompt = DEFAULT_PROMPT;
    ui_DATES.system_prompt = DEFAULT_PROMPT;
    ui_DATES.input_pfx = DEFAULT_PREFIX;
    ui_DATES.input_sfx = DEFAULT_SUFFIX;
    ui_DATES.is_load_tool = false;
    ui_DATES.extra_stop_words = QStringList(ui_DATES.input_pfx + ":\n");//只有这个有用其它不要加了,set_data函数会自己改
    addStopwords();//添加停止词
    date_map.insert("qwen", ui_DATES);
    date_map.insert("alpaca", {"Below is an instruction that describes a task. Write a response that appropriately completes the request.", "Instruction", "Response",false,QStringList{}});
    date_map.insert("chatML", {"<|im_start|>system \nYou are a helpful assistant.<|im_end|>", "<|im_start|>user", "<|im_end|>\n<|im_start|>assistant",false,QStringList{}});
    date_map.insert(wordsObj["troll"].toString(), {wordsObj["you are a troll please respect any question for user"].toString(), "" + wordsObj["user"].toString(), "" + wordsObj["troll"].toString(),false,QStringList{}});
    date_map.insert("eva",{wordsObj["You are an ultimate humanoid weapon of war, please wait for the driver control instructions"].toString(), "" + wordsObj["driver"].toString(), "eva",false,QStringList{}});
    
    //-------------默认展示内容-------------
    ui_font.setPointSize(10); // 将设置控件的字体大小设置为10
    QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
    ui->set->setIcon(QIcon(":/ui/assimp_tools_icon.ico"));//设置设置图标
    ui->reset->setIcon(QIcon(":/ui/sync.ico"));//设置重置图标
    reflash_state("ui:" + wordsObj["click load and choose a gguf file"].toString(),USUAL_);//初始提示
    // reflash_state("ui:" + wordsObj["right click load to use link mode"].toString(),USUAL_);//初始提示
    // reflash_state("ui:" + wordsObj["right click here to open expend window"].toString(),USUAL_);//初始提示
#ifndef BODY_USE_CUBLAST
    ui->vcore_bar->setVisible(0);//如果没有使用cuda则不显示gpu_bar
    ui->vram_bar->setVisible(0);
#endif
    init_movie();//初始化动画参数
    QFile file(":/ui/QSS-master/MacOS.qss");//加载皮肤
    file.open(QFile::ReadOnly);QString stylesheet = tr(file.readAll());
    this->setStyleSheet(stylesheet);file.close();
    // ui->output->setStyleSheet("background-color: white;");//设置输出区背景为纯白
    // ui->input->setStyleSheet("background-color: white;");//设置输入区背景为纯白
    ui->mem_bar->message = wordsObj["mem"].toString();//进度条里面的文本
    ui->vram_bar->message = wordsObj["vram"].toString();//进度条里面的文本
    ui->kv_bar->message = wordsObj["brain"].toString();//进度条里面的文本
    ui->cpu_bar->message = "cpu";//进度条里面的文本
    ui->vcore_bar->message = "gpu";//进度条里面的文本

    //-------------初始化各种控件-------------
    setApiDialog();//设置api选项
    set_DateDialog();//设置约定选项
    set_SetDialog();//设置设置选项
    ui_state_init();//初始界面状态
    ui->input->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    ui->input->installEventFilter(this);//安装事件过滤器
    ui->load->installEventFilter(this);//安装事件过滤器
    api_ip_LineEdit->installEventFilter(this);//安装事件过滤器
    ui->state->setContextMenuPolicy(Qt::NoContextMenu);//取消右键
    ui->state->installEventFilter(this);//安装事件过滤器
    ui->state->setLineWrapMode(QPlainTextEdit::NoWrap);// 禁用自动换行
    ui->state->setFocus();//设为当前焦点

    //-------------获取cpu内存信息-------------
    max_thread = std::thread::hardware_concurrency();
    nthread_slider->setRange(1,max_thread);//设置线程数滑块的范围
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    timer->start(300); // 多少ms更新一次
    ui->cpu_bar->setToolTip(wordsObj["nthread/maxthread"].toString()+"  "+QString::number(ui_SETTINGS.nthread)+"/"+QString::number(std::thread::hardware_concurrency()));
    //ui->mem_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_memusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_memusage + "\n" + wordsObj["else unknown"].toString());
    //ui->vram_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_vramusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_vramusage + "\n" + wordsObj["else unknown"].toString());

    //-------------输出/状态区滚动条控制-------------
    output_scrollBar = ui->output->verticalScrollBar();
    connect(output_scrollBar, &QScrollBar::valueChanged, this, &Widget::output_scrollBarValueChanged);
    state_scrollBar = ui->state->verticalScrollBar();
    connect(state_scrollBar, &QScrollBar::valueChanged, this, &Widget::state_scrollBarValueChanged);

    //-------------添加右击问题-------------
    create_right_menu();

    //-------------初始化工具-------------
    tool_map.insert("calculator", {wordsObj["calculator"].toString(),"calculator",wordsObj["calculator_func_describe_zh"].toString(),wordsObj["calculator_func_describe_en"].toString()});
    tool_map.insert("cmd", {wordsObj["cmd"].toString(),"cmd",wordsObj["cmd_func_describe_zh"].toString(),wordsObj["cmd_func_describe_en"].toString()});
    tool_map.insert("toolguy", {wordsObj["toolguy"].toString(),"toolguy",wordsObj["toolguy_func_describe_zh"].toString(),wordsObj["toolguy_func_describe_en"].toString()});
    tool_map.insert("knowledge", {wordsObj["knowledge"].toString(),"knowledge",wordsObj["knowledge_func_describe_zh"].toString(),wordsObj["knowledge_func_describe_en"].toString()});
    tool_map.insert("positron", {wordsObj["positron"].toString(),"positron",wordsObj["positron_func_describe_zh"].toString(),wordsObj["positron_func_describe_en"].toString()});
    tool_map.insert("stablediffusion", {wordsObj["stablediffusion"].toString(),"stablediffusion",wordsObj["stablediffusion_func_describe_zh"].toString(),wordsObj["stablediffusion_func_describe_en"].toString()});

    //-------------截图声音相关-------------
    cutscreen_dialog = new CutScreenDialog(this);
    cutscreen_dialog->init_action(wordsObj["save cut image"].toString(), wordsObj["svae screen image"].toString());
    QObject::connect(cutscreen_dialog, &CutScreenDialog::cut2ui_qimagepath,this,&Widget::recv_qimagepath);// 传递截取的图像路径
    // 注册全局热键,windows平台用,第二个参数是信号标识,第三个参数是控制键，最后一个是快捷键
    //注册截图快捷键
    if(!RegisterHotKey((HWND)Widget::winId(), 7758258, 0, VK_F1))
    {reflash_state("ui:" + QString("f1 ") + wordsObj["shortcut key registration failed"].toString(), WRONG_);}
    //注册录音快捷键 
    if(!RegisterHotKey((HWND)Widget::winId(), 123456, 0, VK_F2))
    {reflash_state("ui:" + QString("f2 ") + wordsObj["shortcut key registration failed"].toString(), WRONG_);}  

    audio_timer = new QTimer(this);//录音定时器
    connect(audio_timer, &QTimer::timeout, this, &Widget::monitorAudioLevel);// 每隔100毫秒刷新一次输入区
    speech = new QTextToSpeech();
    connect(speech, &QTextToSpeech::stateChanged, this, &Widget::speechOver);//朗读结束后动作
    speechtimer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(qspeech_process()));
    speechtimer->start(500);//每半秒检查一次是否需要朗读

    //----------------第三方进程相关------------------
    server_process = new QProcess(this);// 创建一个QProcess实例用来启动server.exe
    connect(server_process, &QProcess::started, this, &Widget::server_onProcessStarted);//连接开始信号
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Widget::server_onProcessFinished);//连接结束信号        
}

Widget::~Widget()
{
    delete ui;
    delete cutscreen_dialog;
}

//用户点击装载按钮处理
void Widget::on_load_clicked()
{
    reflash_state("ui:"+wordsObj["clicked load"].toString(),SIGNAL_);

    //用户选择模型位置
    QString model_path = customOpenfile(DEFAULT_MODELPATH,wordsObj["choose soul in eva"].toString(),"(*.bin *.gguf)");

    if(model_path==""){return;}//如果路径没选好就让它等于上一次的路径
    is_api = false;//只要点击装载有东西就不再是api模式
    ui_SETTINGS.modelpath = model_path;//模型路径变化则重置参数

    //-------------------只会应用生效一次------------------
    //分析显存，如果可用显存比模型大1.2倍则自动将gpu负载设置为999
    emit gpu_reflash();//强制刷新gpu信息
    QFileInfo fileInfo(ui_SETTINGS.modelpath);//获取文件大小
    int modelsize_MB = fileInfo.size() /1024/1024;
    if(vfree>modelsize_MB*1.2 && ui_SETTINGS.ngl==0)
    {
        //qDebug()<<999;
        ui_SETTINGS.ngl = 999;
    }
    //-------------------只有初次装载才会生效------------------
    //发送设置参数给bot
    emit ui2bot_set(ui_SETTINGS,1);//设置应用完会触发preLoad
}

// 装载前动作
void Widget::preLoad()
{
    is_load = false;//重置is_load标签
    is_load_play_over = false;
    if(ui_mode == CHAT_){ui->output->clear();}//清空输出区
    ui->state->clear();//清空状态区
    ui_state_loading();//装载中界面状态
    reflash_state("ui:" + wordsObj["model location"].toString() +" " + ui_SETTINGS.modelpath,USUAL_);
    emit ui2bot_loadmodel();//开始装载模型,应当确保bot的is_load参数为false
}

//完成加载模型
void Widget::recv_loadover(bool ok_,float load_time_)
{
    if(ok_)
    {
        load_time = load_time_;
        is_load = true;//标记模型已装载
        all_fps ++;//补上最后一帧,表示上下文也创建了
        load_pTimer->stop();//停止动画,但是动作计数load_action保留
        load_pTimer->start(10);//快速播放完剩下的动画,播放完再做一些后续动作
        if(ui_mode == COMPLETE_){ui_state_normal();}//待机界面状态
    }
    else
    {
        ui->state->clear();
        load_begin_pTimer->stop();//停止动画
        load_pTimer->stop();//停止动画
        is_load = false;//标记模型未装载
        load_action = 0;
        this->setWindowTitle(wordsObj["current model"].toString() + " ");
        ui_state_init();
    }

}

//用户点击发出按钮处理
void Widget::on_send_clicked()
{
    reflash_state("ui:" + wordsObj["clicked send"].toString(),SIGNAL_);
    QString input;

    //api模式的处理
    if(is_api)
    {
        api_send_clicked_slove();
        return;
    }

    //如果是对话模式,主要流程就是构建input,发送input,然后触发推理
    if(ui_mode == CHAT_)
    {
        if(ui_need_predecode)
        {
            input = "<ylsdamxssjxxdd:predecode>";//预解码指令
            ui_need_predecode = false;
            ui->reset->setEnabled(0);//预解码时不允许重置
            emit ui2bot_input({"",input,""},0);//传递用户输入  
        }
        else if(is_test)
        {
            if(test_question_index.size()>0)//测试中,还有题目剩余
            {
                input = QString::number(test_count+1) + ". " +test_list_question.at(test_question_index.at(0));
                //添加引导题
                if(help_input)
                {
                    emit ui2bot_input({makeHelpInput() + ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx+ ":\n"  + wordsObj["answer"].toString() + ":"},1);//传递用户输入,测试模式  
                    help_input = false;
                }
                else
                {
                    emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx+ ":\n"  + wordsObj["answer"].toString() + ":"},1);//传递用户输入,测试模式  
                }
            }
            else//完成测试完成,没有题目剩余
            {
                float acc = test_score / test_count * 100.0;//回答准确率
                ui_state = "ui:" + wordsObj["test"].toString() + wordsObj["over"].toString()+ " " + QString::number(test_count) + wordsObj["question"].toString() + " " + wordsObj["accurate"].toString() +QString::number(acc,'f',1) + "% " +wordsObj["use time"].toString() + ":"+ QString::number(test_time.nsecsElapsed()/1000000000.0,'f',2)+" s "+wordsObj["batch decode"].toString() +":" + QString::number(test_tokens/(test_time.nsecsElapsed()/1000000000.0)) + " token/s" ;
                reflash_state(ui_state,SUCCESS_);
                decode_pTimer->stop();
                is_test = false;
                is_run = false;
                //恢复
                test_question_index.clear();
                test_count = 0;
                test_score = 0;
                test_tokens = 0;
                ui_state_normal();//待机界面状态
                return;
            }
        }
        else if(is_query)
        {
            if(query_list.size()>0)//连续回答中
            {
                input = query_list.at(0);
                query_list.removeFirst();
            }
            else//连续回答完成
            {
                ui_state = "ui:" + wordsObj["query"].toString() + wordsObj["over"].toString();
                reflash_state(ui_state,SUCCESS_);
                is_query = false;
                is_run = false;

                ui_state_normal();//待机界面状态
                
                return;
            }
            emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入 
        }
        else if(is_toolguy)//如果你是工具人
        {
            is_toolguy = false;
            ui->input->installEventFilter(this);
            input = QString("toolguy ") + wordsObj["return"].toString() + " " + ui->input->toPlainText().toUtf8().data();
            ui->input->clear();
            input += "\n" + wordsObj["tool_thought"].toString();
            emit ui2bot_input({"",input,""},0);
        }
        else//正常情况!!!
        {
            if(tool_result==""){input = ui->input->toPlainText().toUtf8().data();ui->input->clear();}
            //-----------------------Q14连续回答相关----------------------------
            if(input.contains(wordsObj["Q14"].toString().split(">")[0]))
            {
                query_list = input.split(">")[1].split("/");
                if(query_list.size()==0)
                {
                    return;
                }
                is_query = true;
                input = query_list.at(0);
                query_list.removeFirst();
                emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入  
            }
            //-----------------------如果是拖进来的文件-------------------------
            else if(input.contains("file:///"))
            {
                QString imagepath = input.split("file:///")[1];
                input = "<ylsdamxssjxxdd:imagedecode>";//预解码图像指令

                showImage(imagepath);//显示文件名和图像

                emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入  
                emit ui2bot_imagepath(imagepath);
            }
            //-----------------------截图的情况-------------------------
            else if(input == wordsObj["<predecode cut image>"].toString())
            {
                input = "<ylsdamxssjxxdd:imagedecode>";//预解码图像指令
                showImage(cut_imagepath);//显示文件名和图像
                emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入  
                emit ui2bot_imagepath(cut_imagepath);
            }
            //-----------------------正常情况----------------------------
            else
            {
                //如果工具返回的结果不为空,加思考而不加前缀和后缀
                if(tool_result!="")
                {
                    input= tool_result + "\n" + wordsObj["tool_thought"].toString();
                    tool_result="";
                    emit ui2bot_input({"",input,""},0);
                }
                else
                {
                    emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);
                }
            }
        }
    }
    else if(ui_mode == COMPLETE_)
    {
        input = ui->output->toPlainText().toUtf8().data();//直接用output上的文本进行推理
        emit ui2bot_input({"",input,""},0);//传递用户输入
    }

    is_run =true;//模型正在运行标签
    emit ui2bot_push();//开始推理
    ui_state_pushing();//推理中界面状态
}


//模型输出完毕的后处理
void Widget::recv_pushover()
{
    ui_assistant_history << temp_assistant_history;
    temp_assistant_history = "";
    temp_speech = "";//清空缓存的待读的字
    
    if(is_test)//继续测试
    {
        if(is_api)
        {
            //待修复是net中maneger的问题
            QTimer::singleShot(100, this, SLOT(send_testhandleTimeout()));//api模式不能立即发送
        }
        else
        {
            on_send_clicked();
        }
    }
    else if(is_query)//继续回答
    {
        if(is_api)
        {
            //待修复是net中maneger的问题
            QTimer::singleShot(100, this, SLOT(send_testhandleTimeout()));//api模式不能立即发送
        }
        else
        {
            on_send_clicked();
        }
    }
    else if(ui_mode == COMPLETE_)//补完模式的话额外重置一下
    {
        normal_finish_pushover();
        on_reset_clicked();//触发重置
    }
    else
    {
        //如果挂载了工具,则尝试提取里面的json
        if(is_load_tool)
        {
            QString tool_str = ui_assistant_history.last();
            func_arg_list = JSONparser(tool_str);//取巧预解码的系统指令故意不让解析出json
            if(func_arg_list.size() == 0)
            {
                normal_finish_pushover();
            }
            else
            {
                //调用工具
                reflash_state("ui:" + wordsObj["clicked"].toString() + " " + func_arg_list.front(),SIGNAL_);
                //包含以下字段则停止调用
                if(func_arg_list.first().contains("Answer") || func_arg_list.first().contains("response") || func_arg_list.first().contains("最终回复") )
                {
                    normal_finish_pushover();
                }
                //如果是toolguy的情况
                else if(func_arg_list.first() == "toolguy")
                {
                    is_toolguy = true;
                    ui->send->setEnabled(1);
                    ui->input->setStyleSheet("background-color: rgba(100, 149, 237, 60);");//输入区天蓝色
                    ui->input->setPlaceholderText("工具人赶紧去给模型找答案吧~");
                    ui->input->removeEventFilter(this);//禁用输入区右击
                }
                //正常调用情况
                else
                {
                    emit ui2tool_func_arg(func_arg_list);//传递函数名和参数
                    emit ui2tool_push();//调用tool
                    //使用工具时解码动画不停
                }
            }

        }
        //正常结束
        else
        {
            normal_finish_pushover();
        }
    }
}
//正常情况处理推理完毕
void Widget::normal_finish_pushover()
{
    is_run = false;
    ui_state_normal();//待机界面状态
    decode_pTimer->stop();
    decode_action=0;
    if(wait_to_show_image!="")
    {
        showImage(wait_to_show_image);
        wait_to_show_image = "";
    }
}

//处理tool推理完毕的槽
void Widget::recv_toolpushover(QString tool_result_)
{
    if(tool_result_.contains("<ylsdamxssjxxdd:showdraw>"))//有图像要显示的情况
    {
        wait_to_show_image = tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1];//文生图后待显示图像的图像路径
        tool_result = "stablediffusion调用成功，图像保存在 " + tool_result_.split("<ylsdamxssjxxdd:showdraw>")[1];
    }
    else
    {
        tool_result = tool_result_;
    }
    
    on_send_clicked();//触发发送继续预测下一个词
}

//停止完毕的后处理
void Widget::recv_stopover()
{
    if(ui_mode == COMPLETE_){ui->reset->click();}//补完模式终止后需要重置
}

//模型达到最大上下文的后处理
void Widget::recv_arrivemaxctx(bool predecode)
{
    if(!is_test){QApplication::setWindowIcon(QIcon(":/ui/red_logo.png"));}// 设置应用程序图标
    if(predecode){history_prompt = "";}//新增,取巧使下一次重置触发预解码
}

//重置完毕的后处理
void Widget::recv_resetover()
{
    if(ui_SETTINGS.ngl ==0){QApplication::setWindowIcon(QIcon(":/ui/blue_logo.png"));}//恢复
    else{QApplication::setWindowIcon(QIcon(":/ui/green_logo.png"));}//恢复
    reflash_state("ui:" + wordsObj["reset ok"].toString(),SUCCESS_);
    //如果是对话模式且约定有变或第一次装载则预解码约定
    if(ui_mode == CHAT_)
    {
        history_prompt = ui_DATES.system_prompt;//同步
        //约定系统指令有变才预解码
        if(is_datereset)
        {
            ui_need_predecode =true;
            ui->send->click();
        }
    }
    is_datereset = false;//恢复
    
}

//设置参数改变,重载模型
void Widget::recv_reload()
{
    preLoad();//装载前动作
}


//bot发信号请求ui触发reset,针对约定
void Widget::recv_datereset()
{
    //打印约定的系统指令
    ui_state = "···········"+ wordsObj["date"].toString() + "···········";reflash_state(ui_state,USUAL_);
    if(ui_mode == COMPLETE_)
    {
        reflash_state("· "+ wordsObj["complete mode"].toString() + wordsObj["on"].toString() +" ",USUAL_);
    }
    else
    {
        reflash_state("· "+ wordsObj["system calling"].toString() +" " + system_TextEdit->toPlainText() + extra_TextEdit->toPlainText(),USUAL_);
        //展示额外停止标志
        QString stop_str;
        stop_str = wordsObj["extra stop words"].toString() + " ";
        for(int i = 0;i < ui_DATES.extra_stop_words.size(); ++i)
        {
            stop_str += ui_DATES.extra_stop_words.at(i) + " ";
        }
        reflash_state("· "+ stop_str +" ",USUAL_);
    }
    reflash_state("···········"+ wordsObj["date"].toString() + "···········",USUAL_);
    auto_save_user();//保存ui配置
    
    ui->reset->click();
}

//bot发信号请求ui触发reset,针对设置
void Widget::recv_setreset()
{
    //打印设置内容
    reflash_state("···········"+ wordsObj["set"].toString() + "···········",USUAL_);
    
    reflash_state("· " + wordsObj["temperature"].toString() + " " + QString::number(ui_SETTINGS.temp),USUAL_);
    reflash_state("· " + wordsObj["repeat"].toString() + " " + QString::number(ui_SETTINGS.repeat),USUAL_);
    reflash_state("· " + wordsObj["npredict"].toString() + " " + QString::number(ui_SETTINGS.npredict),USUAL_);
    
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    reflash_state("· gpu " + wordsObj["offload"].toString() + " " + QString::number(ui_SETTINGS.ngl),USUAL_);
#endif
    reflash_state("· cpu" + wordsObj["thread"].toString() + " " + QString::number(ui_SETTINGS.nthread),USUAL_);
    reflash_state("· " + wordsObj["ctx"].toString() + wordsObj["length"].toString() +" " + QString::number(ui_SETTINGS.nctx),USUAL_);
    reflash_state("· " + wordsObj["batch size"].toString() + " " + QString::number(ui_SETTINGS.batch),USUAL_);
    
    if(ui_SETTINGS.lorapath !=""){reflash_state("ui:" + wordsObj["load lora"].toString() + " "+ ui_SETTINGS.lorapath,USUAL_);}
    if(ui_SETTINGS.mmprojpath !=""){reflash_state("ui:" + wordsObj["load mmproj"].toString() + " "+ ui_SETTINGS.mmprojpath,USUAL_);}
    if(ui_mode == CHAT_){reflash_state("· " + wordsObj["chat mode"].toString(),USUAL_);}
    else if(ui_mode == COMPLETE_){reflash_state("· " + wordsObj["complete mode"].toString(),USUAL_);}
    
    //展示额外停止标志
    if(ui_mode == CHAT_)
    {
        QString stop_str;
        stop_str = wordsObj["extra stop words"].toString() + " ";
        for(int i = 0;i < ui_DATES.extra_stop_words.size(); ++i)
        {
            stop_str += ui_DATES.extra_stop_words.at(i) + " ";
        }
        reflash_state("· "+ stop_str +" ",USUAL_);
    }

    reflash_state("···········"+ wordsObj["set"].toString() + "···········",USUAL_);
    auto_save_user();//保存ui配置
    
    ui->reset->click();
}


//用户点击重置按钮的处理,重置模型以及对话,并设置约定的参数
void Widget::on_reset_clicked()
{
    wait_to_show_image = "";//清空待显示图像
    temp_speech="";//清空待读列表
    wait_speech.clear();//清空待读列表
    speech->stop();//停止朗读
    
    //如果模型正在推理就改变模型的停止标签
    if(is_run)
    {
        reflash_state("ui:"+ wordsObj["clicked"].toString()+ wordsObj["shut down"].toString(),SIGNAL_);
        test_question_index.clear();//清空待测试问题列表
        query_list.clear();//清空待回答列表
        if(is_api){emit ui2net_stop(1);}
        else{emit ui2bot_stop();}//传递推理停止信号,模型停止后会再次触发on_reset_clicked()
        return;
    }
    
    reflash_state("ui:"+ wordsObj["clicked reset"].toString(),SIGNAL_);

    if(ui_mode == CHAT_){ui->output->clear();}
    ui_state_normal();//待机界面状态
    
    //如果是链接模式就简单处理
    if(is_api)
    {
        ui_user_history.clear();
        ui_assistant_history.clear();
        if(ui_mode == CHAT_)
        {
            reflash_output(ui_DATES.system_prompt,0,QColor(0,0,0));
            current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_chat_endpoint;
        }
        else
        {
            current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_complete_endpoint;
        }

        QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
        reflash_state("ui:"+wordsObj["current api"].toString() + " " + current_api,USUAL_);
        this->setWindowTitle(wordsObj["current api"].toString() + " " + current_api);

        return;
    }

    this->setWindowTitle(wordsObj["current model"].toString() + " " + ui_SETTINGS.modelpath.split("/").last());

    //如果约定没有变则不需要预解码
    if(ui_mode == CHAT_ && ui_DATES.system_prompt == history_prompt)
    {
        reflash_output(bot_predecode,0,Qt::black);//直接展示预解码的内容
        is_datereset = false;
        emit ui2bot_reset(0);//传递重置信号,删除约定以外的kv缓存
    }
    //需要预解码
    else
    {
        is_datereset = true;//预解码准备
        emit ui2bot_reset(1);//传递重置信号,清空kv缓存
    }
}

//用户点击约定按钮处理
void Widget::on_date_clicked()
{
    reflash_state("ui:"+wordsObj["clicked date"].toString(),SIGNAL_);

    //展示最近一次设置值
    chattemplate_comboBox->setCurrentText(ui_template);//默认使用qwen的提示词模板
    system_TextEdit->setText(ui_system_prompt);

    calculator_checkbox->setChecked(ui_calculator_ischecked);
    cmd_checkbox->setChecked(ui_cmd_ischecked);
    toolguy_checkbox->setChecked(ui_toolguy_ischecked);
    positron_checkbox->setChecked(ui_positron_ischecked);
    knowledge_checkbox->setChecked(ui_knowledge_ischecked);
    stablediffusion_checkbox->setChecked(ui_stablediffusion_ischecked);

    switch_lan_button->setText(ui_extra_lan);
    extra_TextEdit->setText(ui_extra_prompt);//这个要放到各个checkbox的后面来，可以保护用户的修改

    date_dialog->exec();
}

//应用用户设置的约定内容
void Widget::set_date()
{
    get_date();//获取约定中的纸面值

    if(is_api){on_reset_clicked();}//如果是链接模式就重置一下

    date_dialog->close();
    emit ui2bot_date(ui_DATES);
}

//用户点击设置按钮响应
void Widget::on_set_clicked()
{
    server_process->kill();
    reflash_state("ui:"+wordsObj["clicked"].toString()+wordsObj["set"].toString(),SIGNAL_);
    if(ui_mode == CHAT_){chat_btn->setChecked(1),chat_change();}
    else if(ui_mode == COMPLETE_){complete_btn->setChecked(1),complete_change();}
    else if(ui_mode == SERVER_){web_btn->setChecked(1),web_change();}
    //展示最近一次设置值
    temp_slider->setValue(ui_SETTINGS.temp*100);
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    ngl_slider->setValue(ui_SETTINGS.ngl);
#endif
    nctx_slider->setValue(ui_SETTINGS.nctx);
    batch_slider->setValue(ui_SETTINGS.batch);
    repeat_slider->setValue(ui_SETTINGS.repeat*100.00);
    lora_LineEdit->setText(ui_SETTINGS.lorapath);
    mmproj_LineEdit->setText(ui_SETTINGS.mmprojpath);
    npredict_slider->setValue(ui_SETTINGS.npredict);
    nthread_slider->setValue(ui_SETTINGS.nthread);
    port_lineEdit->setText(ui_port);
    set_dialog->exec();
}

//用户按下截图键响应
void Widget::onShortcutActivated()
{
    cutscreen_dialog->showFullScreen();
}

//接收传来的图像
void Widget::recv_qimagepath(QString cut_imagepath_)
{
    cut_imagepath = cut_imagepath_;
    reflash_state("ui:" + wordsObj["cut image success"].toString(),USUAL_);
    ui->input->setPlainText(wordsObj["<predecode cut image>"].toString());
    if(is_load && ui_mode == CHAT_){on_send_clicked();}//如果装载了模型直接发送截图
}

// 设置用户设置内容
void Widget::set_set()
{
    get_set();//获取设置中的纸面值
    
    set_dialog->close();

    //如果不是对话模式则禁用约定
    if(ui_mode!=CHAT_)
    {prompt_box->setEnabled(0);tool_box->setEnabled(0);}
    else{prompt_box->setEnabled(1);tool_box->setEnabled(1);}

    //从服务模式回来强行重载
    if(current_server && ui_mode!=SERVER_)
    {
        current_server=false;
        emit ui2bot_set(ui_SETTINGS,1);
    }
    else if(ui_mode!=SERVER_){emit ui2bot_set(ui_SETTINGS,is_load);}

    //server.exe接管,不需要告知bot约定
    if(ui_mode==SERVER_)
    {
        serverControl();
    }
    else
    {
        if(is_api)//api模式不发信号
        {
            on_reset_clicked();
        }
    }
}

// server.exe接管
void Widget::serverControl()
{
    ui_state_servering();//服务中界面状态
    current_server = true;
    //如果还没有选择模型路径
    if(ui_SETTINGS.modelpath=="")
    {
        ui_SETTINGS.modelpath = customOpenfile(DEFAULT_MODELPATH,wordsObj["choose soul in eva"].toString(),"(*.bin *.gguf)");
        
    }
    if(ui_SETTINGS.modelpath==""){return;}
    
    emit ui2bot_free();
    is_load = false;

    QString resourcePath = ":/server.exe";
    QString localPath = "./EVA_TEMP/server.exe";

    // 获取资源文件
    QFile resourceFile(resourcePath);

    // 尝试打开资源文件进行读取
    if (!resourceFile.open(QIODevice::ReadOnly)) {
        qWarning("cannot open qrc file");
        return ;
    }

    // 读取资源文件的内容
    QByteArray fileData = resourceFile.readAll();
    resourceFile.close();

    createTempDirectory("./EVA_TEMP");
    QFile localFile(localPath);

    // 尝试打开本地文件进行写入
    if (localFile.open(QIODevice::WriteOnly)) 
    {
        localFile.write(fileData);
        localFile.close();
    }

    // 设置要运行的exe文件的路径
    QString program = localPath;

    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << ui_SETTINGS.modelpath;
    arguments << "--host" << "0.0.0.0";//暴露本机ip
    arguments << "--port" << ui_port;//服务端口
    arguments << "-c" << QString::number(ui_SETTINGS.nctx);//使用最近一次应用的nctx作为服务的上下文长度
    arguments << "-ngl" << QString::number(ui_SETTINGS.ngl);//使用最近一次应用的ngl作为服务的gpu负载
    arguments << "--threads" << QString::number(ui_SETTINGS.nthread);//使用线程
    arguments << "-b" << QString::number(ui_SETTINGS.batch);//批大小
    arguments << "-cb";//允许连续批处理
    arguments << "--embedding";//允许词嵌入
    arguments << "--log-disable";//不要日志
    // arguments << "-np";//设置进程请求的槽数 默认：1

    if(ui_SETTINGS.lorapath!=""){arguments << "--no-mmap";arguments << "--lora" << ui_SETTINGS.lorapath;}//挂载lora不能开启mmp
    // server的多模态原作者正在重写中
    // if(ui_SETTINGS.mmprojpath!="")
    // {
    //     arguments << "--mmproj" << ui_SETTINGS.mmprojpath;
    // }

    // 开始运行程序
    server_process->start(program, arguments);
    setWindowState(windowState() | Qt::WindowMaximized);//设置窗口最大化
    reflash_state(wordsObj["eva expend"].toString(),EVA_);
    
    //连接信号和槽,获取程序的输出
    connect(server_process, &QProcess::readyReadStandardOutput, [=]() {
        ui_output = server_process->readAllStandardOutput();
        if(ui_output.contains("warming up the model with an empty run"))
        {
            ui_output += "\n"+wordsObj["browser at"].toString() +QString(" http://")+ipAddress+":"+ui_port;
            ui_output += "\n"+wordsObj["chat"].toString()+wordsObj["endpoint"].toString()+ " " + "/v1/chat/completions";
            ui_output += "\n"+wordsObj["complete"].toString()+wordsObj["endpoint"].toString()+ " " + "/completion"+"\n";
            ui_state = "ui:server " +wordsObj["on"].toString()+wordsObj["success"].toString()+ ","+wordsObj["browser at"].toString()+ ipAddress + ":"+ ui_port;
            auto_save_user();//保存ui配置
            reflash_state(ui_state,SUCCESS_);

        }//替换ip地址
        output_scroll(ui_output);
    });    
    connect(server_process, &QProcess::readyReadStandardError, [=]() {
        ui_output = server_process->readAllStandardError();
        if(ui_output.contains("0.0.0.0")){ui_output.replace("0.0.0.0", ipAddress);}//替换ip地址
        output_scroll(ui_output);
    });
}

//bot将模型参数传递给ui
void Widget::recv_params(PARAMS p)
{
    nctx_slider->setMaximum(p.n_ctx_train);//没有拓展4倍,因为批解码时还是会失败
}

//接收模型词表
void Widget::recv_vocab(QString model_vocab)
{
    ui_model_vocab = model_vocab;
    emit ui2expend_vocab(ui_model_vocab);
}


//接收缓存量
void Widget::recv_kv(float percent,int ctx_size)
{
    if(percent>0 && percent<1){percent=1;}
    ui->kv_bar->setSecondValue(percent);
    ui->kv_bar->setToolTip(wordsObj["kv cache"].toString() + " " + QString::number(ctx_size) + " token");
}
//接收测试的tokens
void Widget::recv_tokens(int tokens)
{
    test_tokens += tokens;
    //qDebug() <<test_tokens<< tokens;
}

//传递llama.cpp的log
void Widget::recv_log(QString log)
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString dateTimeString = currentDateTime.toString("hh:mm:ss  ");
    
    //截获gpu最大负载层数
    if(log.contains("llm_load_print_meta: n_layer"))
    {
        #if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
            int maxngl = log.split("=")[1].toInt()+1;//gpu负载层数是n_layer+1
            emit ui2bot_maxngl(maxngl);
            ngl_slider->setMaximum(maxngl);
            if(ui_SETTINGS.ngl==999){ui_SETTINGS.ngl=maxngl;}//及时修正999值
        #endif
    }

    if(log == ".")
    {
        //不处理
    }
    else
    {
        log.remove("\n");
        emit ui2expend_log(dateTimeString + log);//单条记录 
    }

    //处理异常情况
    if(log.contains("failed to load model"))//提示用户不能有中文
    {
        emit ui2expend_log("请确认模型格式正确，请确认模型路径中不存在中文字符，请确认内存充足");//单条记录 
    }

}
//播放装载动画
void Widget::recv_play()
{
    load_play();//开始播放动画
}

#ifdef BODY_USE_CUBLAST
//更新gpu内存使用率
void Widget::recv_gpu_status(float vmem, float vramp, float vcore, float vfree_)
{
    vfree = vfree_;//剩余显存
    ui->vcore_bar->setValue(vcore);
    //取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if(is_first_getvram){is_first_getvram = false;first_vramp = vramp;ui->vram_bar->setValue(first_vramp);}
    ui->vram_bar->setSecondValue(vramp - first_vramp);
    // ui->vram_bar->setValue(vram - (model_vramusage.toFloat()+ctx_vramusage.toFloat())*100/vmem);
    // ui->vram_bar->setSecondValue((model_vramusage.toFloat()+ctx_vramusage.toFloat())*100/vmem);
    
}
#endif

//事件过滤器,鼠标跟踪效果不好要在各种控件单独实现
bool Widget::eventFilter(QObject *obj, QEvent *event)
{
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->input && event->type() == QEvent::ContextMenu && ui_mode == CHAT_)
    {
        QContextMenuEvent *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        // 显示菜单
        right_menu->exec(contextMenuEvent->globalPos());
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == lora_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseLorapath();
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == mmproj_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseMmprojpath();
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->load && ui->load->isEnabled() && event->type() == QEvent::ContextMenu)
    {
        //防止点不了
        if(!api_dialog->isEnabled()){api_dialog->setWindowFlags(api_dialog->windowFlags() & Qt::WindowCloseButtonHint);}
        else{api_dialog->setWindowFlags(api_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);}//隐藏关闭按钮
        ui_state = "ui:"+wordsObj["clicked"].toString() + QString("api") + wordsObj["set"].toString();reflash_state(ui_state,SIGNAL_);
        //设置当前值
        api_ip_LineEdit->setText(apis.api_ip);
        api_port_LineEdit->setText(apis.api_port);
        api_chat_LineEdit->setText(apis.api_chat_endpoint);
        api_complete_LineEdit->setText(apis.api_complete_endpoint);
        api_is_cache->setChecked(apis.is_cache);
        api_dialog->exec();
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == api_ip_LineEdit && event->type() == QEvent::ContextMenu)
    {
        api_ip_LineEdit->setText(getFirstNonLoopbackIPv4Address());
        return true;
    }
    //响应已安装控件上的鼠标右击事件
    if (obj == ui->state && event->type() == QEvent::ContextMenu)
    {
        emit ui2expend_show(-1);//2是模型日志页
        return true;
    }

    return QObject::eventFilter(obj, event);
}

//传递模型预解码的内容
void Widget::recv_predecode(QString bot_predecode_)
{
    bot_predecode = bot_predecode_;
}

//接收whisper解码后的结果
void Widget::recv_voicedecode_over(QString result)
{
    ui_state_normal();
    ui->input->append(result);
}

//接收模型路径
void Widget::recv_whisper_modelpath(QString modelpath)
{
    whisper_model_path = modelpath;
}

//api模式的发送处理
void Widget::api_send_clicked_slove()
{
    //注意api模式不发送前后缀
    QString input;

    emit ui2net_stop(0);
    ENDPOINT_DATA data;
    data.date_prompt = ui_DATES.system_prompt;
    data.input_pfx = ui_DATES.input_pfx;
    data.input_sfx = ui_DATES.input_sfx;
    data.stopwords = ui_DATES.extra_stop_words;
    data.complete_mode = ui_mode;
    data.temp=ui_SETTINGS.temp;
    data.n_predict = ui_SETTINGS.npredict;
    data.repeat=ui_SETTINGS.repeat;
    data.assistant_history = ui_assistant_history;
    data.user_history = ui_user_history;

    if(is_test)
    {
        QString input;
        if(test_question_index.size()>0)//测试中
        {
            input = QString::number(test_count+1) + ". " +test_list_question.at(test_question_index.at(0));//题目
            //添加引导题
            if(help_input)
            {
                for(int i = 1; i < 3;++i)//2个引导题
                {
                    ui_user_history << wordsObj[QString("H%1").arg(i)].toString();//问题
                    ui_assistant_history << wordsObj[QString("A%1").arg(i)].toString().remove(wordsObj["answer"].toString() + ":");//答案不要答案:这三个字
                    //贴出引导题
                    reflash_output("\n" + ui_DATES.input_pfx + ":\n" + wordsObj[QString("H%1").arg(i)].toString(), 0, QColor(0, 0, 255));
                    reflash_output("\n" + ui_DATES.input_sfx + ":\n" + wordsObj[QString("A%1").arg(i)].toString().remove(wordsObj["answer"].toString() + ":"), 0, QColor(0, 0, 255));
                }
                help_input = false;
            }
        }
        else//完成测试完成
        {
            float acc = test_score / test_count * 100.0;//回答准确率
            decode_pTimer->stop();
            reflash_state("ui:" + wordsObj["test"].toString() + wordsObj["over"].toString()+ " " + QString::number(test_count) + wordsObj["question"].toString() + " " + wordsObj["accurate"].toString() +QString::number(acc,'f',1) + "% " +wordsObj["use time"].toString() + ":"+ QString::number(test_time.nsecsElapsed()/1000000000.0,'f',2)+" s ",SUCCESS_);
            //恢复
            test_question_index.clear();test_count = 0;test_score=0;test_tokens=0;
            is_test = false;
            is_run = false;
            ui->send->setEnabled(1);
            ui->load->setEnabled(1);
            ui->date->setEnabled(1);
            ui->set->setEnabled(1);
            
            return;
        }
        ui_user_history << input;
        data.user_history = ui_user_history;
        data.assistant_history = ui_assistant_history;
        data.n_predict=1;
        emit ui2net_data(data);
        reflash_output("\n" + ui_DATES.input_pfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
        reflash_output(ui_user_history.last(), 0, QColor(0, 0, 0));//输入用黑色
        reflash_output("\n" + ui_DATES.input_sfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
    }
    else if(is_query)
    {
        if(query_list.size()>0)//连续回答中
        {
            input = query_list.at(0);
            query_list.removeFirst();
        }
        else//连续回答完成
        {
            reflash_state("ui:" + wordsObj["query"].toString() + wordsObj["over"].toString(),SUCCESS_);
            is_query = false;
            is_run = false;
            //恢复
            ui->send->setEnabled(1);
            ui->load->setEnabled(1);
            ui->date->setEnabled(1);
            ui->set->setEnabled(1);
            return;
        }
        ui_user_history << input;//置入用户问题
        data.user_history = ui_user_history;
        data.assistant_history = ui_assistant_history;
        data.n_predict=ui_SETTINGS.npredict;
        emit ui2net_data(data);
        reflash_output("\n" + ui_DATES.input_pfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
        reflash_output(ui_user_history.last(), 0, QColor(0, 0, 0));//输入用黑色
        reflash_output("\n" + ui_DATES.input_sfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
    }
    else if(is_toolguy)//如果你是工具人
    {
        is_toolguy = false;
        ui->input->installEventFilter(this);
        input = QString("toolguy ") + wordsObj["return"].toString() + " " + ui->input->toPlainText().toUtf8().data();
        ui->input->clear();
        input += "\n" + wordsObj["tool_thought"].toString();

        if(ui_extra_lan == "zh")
        {
            ui_assistant_history << wordsObj["tool_observation"].toString() + input;
        }
        else if(ui_extra_lan == "en")
        {
            ui_assistant_history << "observation: " + input;
        }
        reflash_output(ui_assistant_history.last() + "\n", 0, QColor(100, 149, 237));//天蓝色表示工具返回结果

        QTimer::singleShot(100, this, SLOT(tool_testhandleTimeout()));//api模式不能立即发送
        is_run =true;//模型正在运行标签
        ui_state_pushing();
        return;
    }
    else
    {
        if(tool_result==""){input = ui->input->toPlainText().toUtf8().data();ui->input->clear();}
        //连续回答
        if(input.contains(wordsObj["Q14"].toString().split(">")[0]))
        {
            query_list = input.split(">")[1].split("/");
            if(query_list.size()==0)
            {
                return;
            }
            is_query = true;
            input = query_list.at(0);
            query_list.removeFirst();
            ui_user_history << input;
            data.user_history = ui_user_history;
            data.assistant_history = ui_assistant_history;
            data.input_prompt = input;
            data.n_predict=ui_SETTINGS.npredict;
            emit ui2net_data(data);
            
            reflash_output("\n" + ui_DATES.input_pfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
            reflash_output(ui_user_history.last(), 0, QColor(0, 0, 0));//输入用黑色
            reflash_output("\n" + ui_DATES.input_sfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
        }
        //
        //来补充链接模式的各种情况/上传图像/图像文件
        //
        //-----------------------正常情况----------------------------
        else if(ui_mode == CHAT_)
        {
            //如果工具返回的结果不为空,则发送工具结果给net
            if(tool_result!="")
            {
                if(ui_extra_lan == "zh")
                {
                    ui_assistant_history << wordsObj["tool_observation"].toString() + tool_result;
                }
                else if(ui_extra_lan == "en")
                {
                    ui_assistant_history << "observation: " + tool_result;
                }
                reflash_output(ui_assistant_history.last() + "\n", 0, QColor(100, 149, 237));//天蓝色表示工具返回结果
                
                tool_result="";

                QTimer::singleShot(100, this, SLOT(tool_testhandleTimeout()));//api模式不能立即发送
                is_run =true;//模型正在运行标签
                ui_state_pushing();
                return;
            }
            else
            {
                ui_user_history << input;
                data.user_history = ui_user_history;
                data.assistant_history = ui_assistant_history;
                reflash_output("\n" + ui_DATES.input_pfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
                reflash_output(ui_user_history.last(), 0, QColor(0, 0, 0));//输入用黑色
                reflash_output("\n" + ui_DATES.input_sfx + ":\n", 0, QColor(0, 0, 255));//前后缀用蓝色
                data.n_predict=ui_SETTINGS.npredict;
                emit ui2net_data(data);
            }
        }
        else if(ui_mode == COMPLETE_)//直接用output上的文本进行推理
        {
            data.input_prompt = input;
            data.n_predict=ui_SETTINGS.npredict;
            emit ui2net_data(data);
        }
            
    }
    
    emit ui2net_push();
    is_run =true;//模型正在运行标签
    ui_state_pushing();
}
//传递知识库的描述
void Widget::recv_embeddingdb_describe(QString describe)
{
    embeddingdb_describe = describe;
}

//传递文转声参数
void Widget::recv_voiceparams(Voice_Params Voice_Params_)
{
    voice_params = Voice_Params_;
}