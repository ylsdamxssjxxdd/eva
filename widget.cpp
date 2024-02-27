#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    getWords();//拯救中文
    ui_system_prompt = DEFAULT_PROMPT;
    ui_DATES.system_prompt = DEFAULT_PROMPT;
    ui_DATES.input_pfx = DEFAULT_PREFIX;
    ui_DATES.input_sfx = DEFAULT_SUFFIX;
    ui_DATES.is_load_tool = false;
    date_map.insert("qwen", ui_DATES);
    date_map.insert("alpaca", {"Below is an instruction that describes a task. Write a response that appropriately completes the request.", "### Instruction", "### Response",false});
    date_map.insert("chatML", {"<|im_start|>system \nYou are a helpful assistant.<|im_end|>", "<|im_start|>user", "<|im_end|>\n<|im_start|>assistant",false});
    date_map.insert(wordsObj["troll"].toString(), {wordsObj["you are a troll please respect any question for user"].toString(), "### " + wordsObj["user"].toString(), "### " + wordsObj["troll"].toString(),false});
    date_map.insert("eva",{wordsObj["You are an ultimate humanoid weapon of war, please wait for the driver control instructions"].toString(), "### " + wordsObj["driver"].toString(), "### eva",false});

    setApiDialog();//设置api选项

    ui_model_vocab = wordsObj["lode model first"].toString();
    QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
    this->setMouseTracking(true);//开启鼠标跟踪
    ui->state->setMouseTracking(true);//开启鼠标跟踪
    QObject::connect(ui->state,&customPlainTextEdit::creatVersionlog,this,&Widget::recv_creatVersionlog);//传递信号创建了解更多窗口
    ui->output->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    ui->input->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    ui->input->installEventFilter(this);//安装事件过滤器
    ui->load->installEventFilter(this);//安装事件过滤器
    api_ip_LineEdit->installEventFilter(this);//安装事件过滤器
    ui->state->setLineWrapMode(QPlainTextEdit::NoWrap);// 禁用自动换行
    ui->state->setFocus();//设为当前焦点
    ui_state = "ui:" + wordsObj["click load and choose gguf file"].toString();reflash_state(ui_state,0);
    ui_state = "ui:" + wordsObj["right click link api"].toString();reflash_state(ui_state,0);

    //获取cpu内存信息
    max_thread = std::thread::hardware_concurrency();
    ui_SETTINGS.nthread = max_thread*0.7;
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    timer->start(300); // 每秒更新一次

    ui->cpu_bar->setToolTip(wordsObj["nthread/maxthread"].toString()+"  "+QString::number(ui_SETTINGS.nthread)+"/"+QString::number(std::thread::hardware_concurrency()));
    //ui->mem_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_memusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_memusage + "\n" + wordsObj["else unknown"].toString());
    //ui->vram_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_vramusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_vramusage + "\n" + wordsObj["else unknown"].toString());
    
    set_DateDialog();//设置约定选项
    set_SetDialog();//设置设置选项
    //初始化动画参数
    init_move();

    //加载皮肤
    //QFile file(":/ui/QSS-master/ConsoleStyle.qss");
    QFile file(":/ui/QSS-master/MacOS.qss");
    file.open(QFile::ReadOnly);QString stylesheet = tr(file.readAll());
    this->setStyleSheet(stylesheet);file.close();
    ui->input->setStyleSheet("background-color: white;");//设置输入区背景为纯白

    //输出区滚动条控制
    output_scrollBar =  ui->output->verticalScrollBar();
    connect(output_scrollBar, &QScrollBar::valueChanged, this, &Widget::scrollBarValueChanged);

    process = new QProcess(this);// 创建一个QProcess实例用来启动server.exe
    connect(process, &QProcess::started, this, &Widget::onProcessStarted);//连接开始信号
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Widget::onProcessFinished);//连接结束信号
    

#if defined(BODY_USE_CUBLAST)
    //this->setWindowTitle(QString("eva-cuda") + VERSION);
#else
    ui->vcore_bar->setVisible(0);//如果没有使用cuda则不显示gpu_bar
    ui->vram_bar->setVisible(0);
#endif
    //历史中的今天
    QDate currentDate = QDate::currentDate();
    QString dateString = currentDate.toString("MM" + wordsObj["month"].toString() + "dd" + wordsObj["day"].toString());

    //添加常用问题
    for(int i = 1; i < 18;++i)
    {
        if(i == 4){questions<<wordsObj[QString("Q%1").arg(i)].toString().replace("{today}",dateString);}//历史中的今天
        else
        {
            questions<<wordsObj[QString("Q%1").arg(i)].toString();
        }
        
    }

    //进度条里面的文本
    ui->mem_bar->message = wordsObj["mem"].toString();
    ui->vram_bar->message = wordsObj["vram"].toString();
    ui->kv_bar->message = wordsObj["brain"].toString();
    ui->cpu_bar->message = "cpu";
    ui->vcore_bar->message = "gpu";
    //apis.api_ip = getFirstNonLoopbackIPv4Address();//测试
    keeptimer = new QTimer(this);//持续检测延迟
    connect(keeptimer, SIGNAL(timeout()), this, SLOT(keepConnection()));
    force_unlockload = new QTimer(this);//强制解锁
    connect(force_unlockload, SIGNAL(timeout()), this, SLOT(unlockLoad()));

    //初始化工具
    tool_map.insert("calculator", {wordsObj["calculator"].toString(),"calculator",wordsObj["calculator_func_describe_zh"].toString(),wordsObj["calculator_func_describe_en"].toString()});
    tool_map.insert("cmd", {wordsObj["cmd"].toString(),"cmd",wordsObj["cmd_func_describe_zh"].toString(),wordsObj["cmd_func_describe_en"].toString()});
    tool_map.insert("search", {wordsObj["search"].toString(),"search",wordsObj["search_func_describe_zh"].toString(),wordsObj["search_func_describe_en"].toString()});
    tool_map.insert("knowledge", {wordsObj["knowledge"].toString(),"knowledge",wordsObj["knowledge_func_describe_zh"].toString(),wordsObj["knowledge_func_describe_en"].toString()});
    tool_map.insert("positron", {wordsObj["positron"].toString(),"positron",wordsObj["positron_func_describe_zh"].toString(),wordsObj["positron_func_describe_en"].toString()});
    tool_map.insert("llm", {wordsObj["llm"].toString(),"llm",wordsObj["llm_func_describe_zh"].toString(),wordsObj["llm_func_describe_en"].toString()});
    

}

Widget::~Widget()
{
    delete ui;
}

//用户点击装载按钮处理
void Widget::on_load_clicked()
{
    reflash_state("ui:"+wordsObj["clicked load"].toString(),3);
    //用户选择模型位置
    QString ui_model_path = QFileDialog::getOpenFileName(this,wordsObj["choose soul in eva"].toString(),DEFAULT_MODELPATH);
    if(ui_model_path==""){return;}//如果路径没选好就让它等于上一次的路径
    is_api = false;//只要点击装载有东西就不再是api模式

    //模型路径变化则重置参数
    ui_SETTINGS.modelpath = ui_model_path;

    //分析显存
    emit gpu_reflash();//强制刷新gpu信息
    //初次加载这个模型的时候，如果可用显存比模型大1GB则自动将gpu负载设置为999
    QFileInfo fileInfo(ui_SETTINGS.modelpath);//获取文件大小
    int modelsize_MB = fileInfo.size() /1024/1024;
    if(vfree>modelsize_MB*1.2)
    {
        reflash_state("ui:" +wordsObj["vram enough"].toString(),1);
        ui_SETTINGS.ngl= 999;
    }
    else
    {
        ui_SETTINGS.ngl= 0;
    }
    emit ui2bot_set(ui_SETTINGS,1);//设置应用完会触发preLoad
}

// 装载前动作
void Widget::preLoad()
{
    is_load = false;//重置is_load标签
    if(ui_mode == 0){ui->output->clear();}//清空输出区
    ui->state->clear();//清空状态区
    ui->send->setEnabled(0);//禁用发出按钮
    ui->reset->setEnabled(0);//禁用重置按钮
    ui->date->setEnabled(0);ui->set->setEnabled(0);
    ui->load->setEnabled(0);//禁用装载按钮
    ui->input->setFocus();//设置输入区为焦点
    ui->output->setStyleSheet("color: transparent;");//设置文本为透明
    reflash_state("ui:" + wordsObj["model location"].toString() +" " + ui_SETTINGS.modelpath,0);
    emit ui2bot_loadmodel();//开始装载模型,应当确保bot的is_load参数为false
}

//完成加载模型
void Widget::recv_loadover(bool ok_,float load_time_)
{
    if(ok_)
    {
        load_time = load_time_;
        change_api_dialog(1);
        is_load = true;//标记模型已装载,只有这个是true才允许后续动作
        all_fps ++;//补上最后一帧,表示上下文也创建了
        load_pTimer->stop();//停止动画,但是动作计数load_action保留
        load_pTimer->start(10);//快速播放完剩下的动画,播放完再做一些后续动作
    }
    else
    {
        ui->state->clear();
        load_begin_pTimer->stop();//停止动画
        load_pTimer->stop();//停止动画
        is_load = false;//标记模型未装载
        ui->load->setEnabled(1);
        ui->output->setStyleSheet("");//取消文本为透明
        load_action = 0;
    }

}

//用户点击发出按钮处理
void Widget::on_send_clicked()
{
    ui_state = "ui:" + wordsObj["clicked send"].toString();reflash_state(ui_state,3);
    QString input;
    
    //api模式的处理
    if(is_api)
    {
        emit ui2net_stop(0);
        ENDPOINT_DATA data;
        data.date_prompt = ui_DATES.system_prompt;data.input_pfx = ui_DATES.input_pfx;data.input_sfx = ui_DATES.input_sfx;
        data.complete_mode = ui_mode;data.temp=ui_SETTINGS.temp;data.repeat=ui_SETTINGS.repeat;
        data.assistant_history = ui_assistant_history;data.user_history = ui_user_history;

        if(is_test)
        {
            if(test_question_index.size()>0)//测试中
            {
                input = QString::number(test_count+1) + ". " +test_list_question.at(test_question_index.at(0));
            }
            else//完成测试完成
            {
                float acc = test_score / test_count * 100.0;//回答准确率
                ui_state = "ui:" + wordsObj["test"].toString() + wordsObj["over"].toString()+ " " + QString::number(test_count) + wordsObj["question"].toString() + " " + wordsObj["accurate"].toString() +QString::number(acc,'f',1) + "% " +wordsObj["use time"].toString() + ":"+ QString::number(test_time.nsecsElapsed()/1000000000.0,'f',2)+" s ";
                reflash_state(ui_state,1);
                is_test = false;
                is_run = false;
                //恢复
                test_question_index.clear();test_count = 0;test_score=0;test_tokens=0;
                ui->send->setEnabled(1);
                ui->load->setEnabled(1);
                ui->date->setEnabled(1);ui->set->setEnabled(1);
                return;
            }
            ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
            data.input_prompt = input;data.n_predict=1;
            emit ui2net_data(data);
            reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0);
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
                reflash_state(ui_state,1);
                is_query = false;
                is_run = false;
                //恢复
                ui->send->setEnabled(1);
                ui->load->setEnabled(1);
                ui->date->setEnabled(1);ui->set->setEnabled(1);
                return;
            }
            ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
            data.input_prompt = input;data.n_predict=ui_SETTINGS.npredict;
            emit ui2net_data(data);
            reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0);
        }
        else
        {
            input = ui->input->toPlainText().toUtf8().data();//获取用户输入
            ui->input->clear();//用户输入区清空输入

            if(input ==  wordsObj["Q16"].toString())//开始测试开始
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " " + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,3);
                clearQuestionlist();//清空题库
                makeTestQuestion(":/ceval-exam/val");//构建测试问题集
                makeTestIndex();//构建测试问题索引
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,3);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标

                api_addhelpinput();//加入引导问题
                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
                data.input_prompt = input;data.n_predict=1;
                emit ui2net_data(data);
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0);
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " +wordsObj["current api"].toString() + " " + current_api);  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,3);
                
            }
            else if (input ==  wordsObj["Q17"].toString())//自定义数据集
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " "  + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,3);
                QString custom_csvpath = QFileDialog::getOpenFileName(this,wordsObj["Q17"].toString(),"D:/soul","CSV files (*.csv)");//用户选择自定义的csv文件
                clearQuestionlist();//清空题库
                readCsvFile(custom_csvpath);//构建测试问题集
                makeTestIndex();//构建测试问题索引
                if(test_question_index.size()==0){ui_state = "ui:0"+ wordsObj["question"].toString();reflash_state(ui_state,2);return;}
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,3);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标
                api_addhelpinput();//加入引导问题
                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
                
                ui_assistant_history << input;
                
                data.input_prompt = input;data.n_predict=1;
                emit ui2net_data(data);
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0);
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " +wordsObj["current api"].toString() + " " + current_api);  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,3);
                
            }
            else if(input.contains(wordsObj["Q14"].toString().split(">")[0]))
            {
                query_list = input.split(">")[1].split("/");
                if(query_list.size()==0)
                {
                    return;
                }
                is_query = true;
                input = query_list.at(0);
                query_list.removeFirst();
                ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
                data.input_prompt = input;data.n_predict=ui_SETTINGS.npredict;
                emit ui2net_data(data);
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0);
            }
            else if(ui_mode == 0)
            {
                ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0);
                data.n_predict=ui_SETTINGS.npredict;
                emit ui2net_data(data);
            } 
            else if(ui_mode == 1)//直接用output上的文本进行推理
            {
                input = ui->output->toPlainText().toUtf8().data();
                data.input_prompt = input;data.n_predict=ui_SETTINGS.npredict;
                emit ui2net_data(data);
            }
               
        }
        ui->input->setFocus();
        is_run =true;//模型正在运行标签
        emit ui2net_push();
        ui->date->setDisabled(1);ui->load->setDisabled(1);ui->send->setDisabled(1);
        ui->set->setEnabled(0);
        return;
    }
    
    //如果是对话模式
    if(ui_mode ==0)
    {
        if(ui_need_predecode)
        {
            input = "<ylsdamxssjxxdd:predecode>";//告诉bot开始预推理
            ui_need_predecode = false;
            //ui_state = "ui:" + wordsObj["input predecode"].toString();
            //reflash_state(ui_state,3);//
            //encode_play();//开启推理动画
            emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入  
        }
        else if(is_test)
        {
            if(test_question_index.size()>0)//测试中
            {
                input = QString::number(test_count+1) + ". " +test_list_question.at(test_question_index.at(0));
                
            }
            else//完成测试完成
            {
                float acc = test_score / test_count * 100.0;//回答准确率
                ui_state = "ui:" + wordsObj["test"].toString() + wordsObj["over"].toString()+ " " + QString::number(test_count) + wordsObj["question"].toString() + " " + wordsObj["accurate"].toString() +QString::number(acc,'f',1) + "% " +wordsObj["use time"].toString() + ":"+ QString::number(test_time.nsecsElapsed()/1000000000.0,'f',2)+" s "+wordsObj["batch decode"].toString() +":" + QString::number(test_tokens/(test_time.nsecsElapsed()/1000000000.0)) + " token/s" ;
                reflash_state(ui_state,1);
                is_test = false;
                is_run = false;
                //恢复
                test_question_index.clear();test_count = 0;test_score=0;test_tokens=0;
                ui->send->setEnabled(1);
                ui->load->setEnabled(1);
                ui->date->setEnabled(1);ui->set->setEnabled(1);
                return;
            }
            emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx+ ":\n"  + wordsObj["answer"].toString() + ":"},1);//传递用户输入,测试模式  
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
                reflash_state(ui_state,1);
                is_query = false;
                is_run = false;
                //恢复
                ui->send->setEnabled(1);
                ui->load->setEnabled(1);
                ui->date->setEnabled(1);ui->set->setEnabled(1);
                return;
            }
            emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入 
        }
        else//正常情况!!!
        {
            if(tool_result==""){input = ui->input->toPlainText().toUtf8().data();ui->input->clear();}

            if(input ==  wordsObj["Q16"].toString())//开始测试开始
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " "  + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,3);
                clearQuestionlist();//清空题库
                makeTestQuestion(":/ceval-exam/val");//构建测试问题集
                makeTestIndex();//构建测试问题索引
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,3);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标

                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                
                emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n" + wordsObj["answer"].toString() + ":"},1);//传递用户输入
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " + ui_SETTINGS.modelpath.split("/").last());  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,3);
                emit ui2bot_help_input();//通知模型准备加入引导问题
            }
            else if (input ==  wordsObj["Q17"].toString())//自定义数据集
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " "  + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,3);
                QString custom_csvpath = QFileDialog::getOpenFileName(this,wordsObj["Q17"].toString(),"D:/soul","CSV files (*.csv)");//用户选择自定义的csv文件
                clearQuestionlist();//清空题库
                readCsvFile(custom_csvpath);//构建测试问题集
                makeTestIndex();//构建测试问题索引
                if(test_question_index.size()==0){ui_state = "ui:0"+ wordsObj["question"].toString();reflash_state(ui_state,2);return;}
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,3);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标

                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                
                emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},1);//传递用户输入
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " + ui_SETTINGS.modelpath.split("/").last());  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,3);
                emit ui2bot_help_input();//通知模型准备加入引导问题
            }
            else if(input.contains(wordsObj["Q14"].toString().split(">")[0]))
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
            else if(input == wordsObj["Q15"].toString())
            {
                QString imagepath = QFileDialog::getOpenFileName(this,wordsObj["Q15"].toString(),"D:/soul","(*.png *.jpg *.bmp)");//用户选择图片
                input = "<ylsdamxssjxxdd:imagedecode>";
                ui_output = "\nfile:///" + imagepath + "\n";output_scroll();
                emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入  
                emit ui2bot_imagepath(imagepath);
            }
            else
            {
                if(tool_result!=""){input= tool_result + "\n" + wordsObj["tool_thought"].toString();tool_result="";emit ui2bot_input({"",input,""},0);}//工具返回的结果不带前后缀
                else{emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);}
                //传递用户输入  
                //encode_play();//开启推理动画
            }
               
        }
        ui->input->setFocus();
        is_run =true;//模型正在运行标签
        emit ui2bot_push();//开始推理
        
    }
    else if(ui_mode == 1)
    {
        input = ui->output->toPlainText().toUtf8().data();//直接用output上的文本进行推理
        emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入
        is_run =true;//模型正在运行标签
        emit ui2bot_push();//开始推理
        //ui_state = "ui:" + wordsObj["ctx decode"].toString();
        //reflash_state(ui_state,0);
        //encode_play();//开启推理动画
    }

    ui->send->setEnabled(0);//按钮禁用,直到bot输出完毕
    ui->load->setEnabled(0);//按钮禁用,直到bot输出完毕
    ui->date->setEnabled(0);ui->set->setEnabled(0);

}


//模型输出完毕的后处理
void Widget::recv_pushover()
{
    ui_assistant_history << temp_assistant_history;
    temp_assistant_history = "";
    //qDebug() << ui_assistant_history;
    if(is_test)//继续测试
    {
        if(is_api)
        {
            //待修复是net中maneger的问题
            QTimer::singleShot(100, this, SLOT(send_testhandleTimeout()));//api模式不能立即发送
        }
        else
        {
            QTimer::singleShot(10, this, SLOT(send_testhandleTimeout()));//对话模式也不能立即发送
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
            QTimer::singleShot(10, this, SLOT(send_testhandleTimeout()));//对话模式也不能立即发送
        }
    }
    else
    {
        //如果挂载了工具,则尝试提取里面的json
        if(is_load_tool)
        {
            QStringList func_arg_list;
            func_arg_list = matchJSON(ui_assistant_history.last());//取巧预解码的系统指令故意不让解析出json
            if(func_arg_list.size() == 0)
            {
                is_run = false;
                ui->send->setEnabled(1);
                ui->load->setEnabled(1);
                ui->date->setEnabled(1);ui->set->setEnabled(1);
                ui->reset->setEnabled(1);
                encode_pTimer->stop();encode_action=0;
            }
            else
            {
                //调用工具
                reflash_state("ui:" + wordsObj["clicked"].toString() + " " + func_arg_list.front(),3);
                emit ui2tool_func_arg(func_arg_list);//传递函数名和参数
                emit ui2tool_push();
            }

        }
        //正常结束
        else
        {
            is_run = false;
            ui->send->setEnabled(1);
            ui->load->setEnabled(1);
            ui->date->setEnabled(1);ui->set->setEnabled(1);
            ui->reset->setEnabled(1);
            encode_pTimer->stop();encode_action=0;
        }
        
    }

    
}

//处理tool推理完毕的槽
void Widget::recv_toolpushover(QString tool_result_)
{
    tool_result = tool_result_;
    on_send_clicked();
}

//停止完毕的后处理
void Widget::recv_stopover()
{
    if(ui_mode == 1){ui->reset->click();}//补完模式终止后需要重置
}

//模型达到最大上下文的后处理
void Widget::recv_arrivemaxctx(bool predecode)
{
    if(!is_test){QApplication::setWindowIcon(QIcon(":/ui/red_logo.png"));}// 设置应用程序图标
    if(predecode){history_prompt = "";}//新增,取巧使下一次重置触发预推理
}

//重置完毕的后处理
void Widget::recv_resetover()
{
    
    if(ui_SETTINGS.ngl ==0){QApplication::setWindowIcon(QIcon(":/ui/blue_logo.png"));}//恢复
    else{QApplication::setWindowIcon(QIcon(":/ui/green_logo.png"));}//恢复
    //如果是对话模式且约定有变或第一次装载则预推理约定
    if(ui_mode == 0)
    {
        history_prompt = ui_DATES.system_prompt;//同步
        //约定系统指令有变才预推理
        if(is_datereset)
        {
            ui_need_predecode =true;
            ui->send->click();
        }
    }
    is_datereset = false;//恢复
    ui_state = "ui:" + wordsObj["reset ok"].toString();reflash_state(ui_state,1);
    
}

//设置参数改变,重载模型
void Widget::recv_reload()
{
    ui_state = "ui:"+wordsObj["set"].toString()+wordsObj["success"].toString();reflash_state(ui_state,1);
    preLoad();//装载前动作
}


//bot发信号请求ui触发reset,针对约定
void Widget::recv_datereset()
{
    ui_state = "···········"+ wordsObj["date"].toString() + "···········";reflash_state(ui_state,0);
    //打印约定内容
    ui_state = "· " + wordsObj["repeat"].toString() + QString::number(ui_SETTINGS.repeat);reflash_state(ui_state,0);
    if(ui_mode == 1){ui_state = "· "+ wordsObj["complete mode"].toString() + wordsObj["on"].toString() +" ";reflash_state(ui_state,0);}
    else{ui_state = "· "+ wordsObj["system calling"].toString() +" " + system_TextEdit->toPlainText();reflash_state(ui_state,0);}
    ui_state = "···········"+ wordsObj["date"].toString() + "···········";reflash_state(ui_state,0);
    is_datereset = true;
    //ui_state = "ui:"+wordsObj["date"].toString()+wordsObj["success"].toString();reflash_state(ui_state,1);
    ui->reset->click();
}

//bot发信号请求ui触发reset,针对设置
void Widget::recv_setreset()
{
    reflash_state("···········"+ wordsObj["set"].toString() + "···········",0);
    //打印约定内容
#if defined(GGML_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    ui_state = "· gpu " + wordsObj["offload"].toString() + QString::number(ui_SETTINGS.ngl);reflash_state(ui_state,0);
#endif
    ui_state = "· " + wordsObj["ctx"].toString() + wordsObj["length"].toString() +" " + QString::number(ui_SETTINGS.nctx);reflash_state(ui_state,0);
    ui_state = "···········"+ wordsObj["set"].toString() + "···········";reflash_state(ui_state,0);
    if(ui_SETTINGS.lorapath !=""){ui_state = "ui:" + wordsObj["load lora"].toString() + " "+ ui_SETTINGS.lorapath;reflash_state(ui_state,0);}
    if(ui_SETTINGS.mmprojpath !=""){ui_state = "ui:" + wordsObj["load mmproj"].toString() + " "+ ui_SETTINGS.mmprojpath;reflash_state(ui_state,0);}
    //reflash_state("ui:"+wordsObj["set"].toString()+wordsObj["success"].toString(),1);
    ui->reset->click();
}

//接受支持设备信息
void Widget::recv_device(QString device_)
{
    ui->state->device_tooltip = device_;//传递给自定义的plaintextedit控件
    //内容在模型装载完毕动画完毕后显示
}


//用户点击重置按钮的处理,重置模型以及对话,并设置约定的参数
void Widget::on_reset_clicked()
{
    //如果模型正在推理就改变模型的停止标签
    if(is_run)
    {
        ui_state = "ui:"+ wordsObj["clicked"].toString()+ wordsObj["shut down"].toString();reflash_state(ui_state,3);
        emit ui2bot_help_input(0);//通知模型不再加入引导问题
        test_question_index.clear();//清空待测试问题列表
        query_list.clear();//清空待回答列表
        if(is_api){emit ui2net_stop(1);}
        else{emit ui2bot_stop();}//传递推理停止信号,模型停止后会再次触发on_reset_clicked()
        return;
    }
    reflash_state("ui:"+ wordsObj["clicked reset"].toString(),3);
    ui_change();//改变相关控件状态

    if(is_api)
    {
        ui_assistant_history.clear();//清空历史
        ui_user_history.clear();//清空历史
        this->setWindowTitle(wordsObj["current api"].toString() + " " + current_api);
        QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
        return;
    }
    this->setWindowTitle(wordsObj["current model"].toString() + " " + ui_SETTINGS.modelpath.split("/").last());

    //如果约定没有变则不需要预推理
    if(ui_mode == 0  && ui_DATES.system_prompt == history_prompt)
    {
        is_datereset = false;
        emit ui2bot_reset(0);//传递重置信号,删除约定以外的kv缓存
    }
    else
    {
       is_datereset = true;//预推理准备
       emit ui2bot_reset(1);//传递重置信号,清空kv缓存
    }


}

//用户点击约定按钮处理
void Widget::on_date_clicked()
{
    ui_state = "ui:"+wordsObj["clicked date"].toString();reflash_state(ui_state,3);
    if(is_api)//api模式不要解码设置
    {
        change_api_dialog(0);
    }

    //展示最近一次设置值
    prompt_comboBox->setCurrentText(ui_template);//默认使用qwen的提示词模板
    system_TextEdit->setText(ui_system_prompt);
    extra_TextEdit->setText(ui_extra_prompt);
    switch_lan_button->setText(ui_extra_lan);

    date_dialog->exec();
}
//用户点击设置按钮响应
void Widget::on_set_clicked()
{
    ui_state = "ui:"+wordsObj["clicked"].toString()+wordsObj["set"].toString();reflash_state(ui_state,3);
    if(ui_mode == 2){process->kill();}//要在应用前关闭掉,否则再次启动不了
    if(ui_mode == 0){chat_btn->setChecked(1),chat_change();}
    else if(ui_mode == 1){complete_btn->setChecked(1),complete_change();}
    else if(ui_mode == 2){web_btn->setChecked(1),web_change();}
    //展示最近一次设置值
    temp_slider->setValue(ui_SETTINGS.temp*100);
#if defined(GGML_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
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
//设置用户设置内容
void Widget::set_set()
{
    ui_SETTINGS.temp = temp_slider->value()/100.0;
    ui_SETTINGS.repeat = repeat_slider->value()/100.0;
    ui_SETTINGS.npredict = npredict_slider->value();

    ui_SETTINGS.nthread =nthread_slider->value();
    ui_SETTINGS.nctx = nctx_slider->value();//获取nctx滑块的值
    ui_SETTINGS.batch = batch_slider->value();//获取nctx滑块的值
#if defined(GGML_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    ui_SETTINGS.ngl = ngl_slider->value();//获取npl滑块的值
#endif

    ui_SETTINGS.lorapath = lora_LineEdit->text();
    ui_SETTINGS.mmprojpath = mmproj_LineEdit->text();

    ui_SETTINGS.complete_mode = complete_btn->isChecked();
    if(chat_btn->isChecked()){ui_mode=0;}
    else if(complete_btn->isChecked()){ui_mode=1;history_prompt="";}//history_prompt置空是为了下一次切换为对话模式时正确处理预推理
    else if(web_btn->isChecked()){ui_mode=2;}
    ui_port = port_lineEdit->text();
    
    // QDir checkDir(ui_SETTINGS.mmprojpath);
    // qDebug()<<ui_SETTINGS.mmprojpath<<checkDir.exists();
    // if (!checkDir.exists()) {ui_SETTINGS.mmprojpath="";ui_state = "ui:mmporj path not exit";reflash_state(ui_state,2);}// 目录不存在
    set_dialog->close();
    if(current_server && ui_mode!=2){current_server=false;emit ui2bot_set(ui_SETTINGS,1);}//从服务模式回来强行重载
    else{emit ui2bot_set(ui_SETTINGS,is_load);}
    modeChange();
}

//应用用户设置的约定内容
void Widget::set_date()
{
    ui_extra_prompt = extra_TextEdit->toPlainText();
    ui_system_prompt = system_TextEdit->toPlainText();
    ui_DATES.system_prompt = ui_system_prompt + ui_extra_prompt;//合并附加指令
    ui_DATES.input_pfx = input_pfx_LineEdit->text();
    ui_DATES.input_sfx = input_sfx_LineEdit->text();
    ui_DATES.is_load_tool = is_load_tool;
    ui_template = prompt_comboBox->currentText();
    ui_extra_lan = switch_lan_button->text();
    date_dialog->close();
    emit ui2bot_date(ui_DATES,is_load);
    modeChange();

}
//选用计算器工具
void Widget::calculator_change()
{
    
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
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
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::search_change()
{
    
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::knowledge_change()
{
    
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::positron_change()
{
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}

void Widget::llm_change()
{
    if(toolcheckbox_checked())
    {
        is_load_tool = true;
    }
    else{is_load_tool = false;}
    extra_TextEdit->setText(create_extra_prompt());
}
//切换行动纲领的语言
void Widget::switch_lan_change()
{
    if(switch_lan_button->text()=="zh")
    {
        switch_lan_button->setText("en");
        create_extra_prompt();
        extra_TextEdit->setText(create_extra_prompt());
    }
    else if(switch_lan_button->text()=="en")
    {
        switch_lan_button->setText("zh");
        create_extra_prompt();
        extra_TextEdit->setText(create_extra_prompt());
    }
}
// 判断是否挂载了工具
bool Widget::toolcheckbox_checked()
{
    if(calculator_checkbox->isChecked() || cmd_checkbox->isChecked() || search_checkbox->isChecked() || knowledge_checkbox->isChecked() || positron_checkbox->isChecked() || llm_checkbox->isChecked())
    {
        return true;
    }
    else
    {
        return false;
    }
    
}

void Widget::modeChange()
{
    //server.exe接管,不需要告知bot约定
    if(ui_mode==2)
    {
        ui->output->clear();
        ui->load->setEnabled(0);
        ui->reset->setEnabled(0);
        ui->input->setVisible(0);
        ui->send->setVisible(0);
        ui->date->setEnabled(0);
        current_server = true;
        serverControl();
    }
    else
    {
        ui->input->setVisible(1);
        ui->send->setVisible(1);
        ui_change();//因为要重载,所以先根据标签改变ui控件的状态
        //展示预解码的系统指令
        if(!is_api && ui_mode == 0 && is_load){reflash_output(ui_DATES.system_prompt,0);}

        if(is_api)
        {
            
            ui_user_history.clear();ui_assistant_history.clear();
            if(ui_mode == 0){current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_chat_endpoint;}
            else{current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_complete_endpoint;}
            ui_state = "ui:"+wordsObj["current api"].toString() + " " + current_api;reflash_state(ui_state,0);
            this->setWindowTitle(wordsObj["current api"].toString() + " " + current_api);
            return ;
        }//api模式不发信号

    }
}
//server.exe接管
void Widget::serverControl()
{
    //如果还没有选择模型路径
    if(ui_SETTINGS.modelpath=="")
    {
        ui_SETTINGS.modelpath = QFileDialog::getOpenFileName(this,wordsObj["choose soul in eva"].toString(),DEFAULT_MODELPATH);
    }
    
    emit ui2bot_free();
    is_load = false;

#ifdef GGML_USE_CLBLAST
    QString resourcePath = ":/server_1803_clblast.exe";
    QString localPath = "server_1803_clblast.exe";
#elif BODY_USE_CUBLAST
    QString resourcePath = ":/server_1999_cuda.exe";
    QString localPath = "server_1999_cuda.exe";
#elif BODY_USE_32BIT
    QString resourcePath = ":/server_1803_32bit.exe";
    QString localPath = "server_1803_32bit.exe";
#else
    QString resourcePath = ":/server_1803.exe";
    QString localPath = "server_1803.exe";
#endif


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

    QFile localFile(localPath);

    // 尝试打开本地文件进行写入
    if (!localFile.open(QIODevice::WriteOnly)) {
        qWarning("cannot write local");
        return ;
    }

    // 写入内容到本地文件
    localFile.write(fileData);
    localFile.close();

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
    if(ui_SETTINGS.lorapath!=""){arguments << "--no-mmap";arguments << "--lora" << ui_SETTINGS.lorapath;}//挂载lora不能开启mmp
    if(ui_SETTINGS.mmprojpath!=""){arguments << "--mmproj" << ui_SETTINGS.mmprojpath;}

    // 开始运行程序
    process->start(program, arguments);
    setWindowState(windowState() | Qt::WindowMaximized);//设置窗口最大化
    reflash_state("ui:" + wordsObj["eva"].toString() + wordsObj["eva expand"].toString(),4);
    //连接信号和槽,获取程序的输出
    connect(process, &QProcess::readyReadStandardOutput, [=]() {
        ui_output = process->readAllStandardOutput();
        if(ui_output.contains("model loaded"))
        {
            ui_output += "\n"+wordsObj["browser at"].toString() +QString(" http://")+ipAddress+":"+ui_port;
            ui_output += "\n"+wordsObj["chat"].toString()+wordsObj["endpoint"].toString()+ " " + "/v1/chat/completions";
            ui_output += "\n"+wordsObj["complete"].toString()+wordsObj["endpoint"].toString()+ " " + "/completion"+"\n";
            ui_state = "ui:server " +wordsObj["on"].toString()+wordsObj["success"].toString()+ ","+wordsObj["browser at"].toString()+ ipAddress + ":"+ ui_port;reflash_state(ui_state,1);

        }//替换ip地址
        output_scroll();
    });    connect(process, &QProcess::readyReadStandardError, [=]() {
        ui_output = process->readAllStandardError();
        if(ui_output.contains("0.0.0.0")){ui_output.replace("0.0.0.0", ipAddress);}//替换ip地址
        output_scroll();
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
    emit ui2version_vocab(ui_model_vocab);
}

//创建了解更多窗口
void Widget::recv_creatVersionlog()
{
    versionlog_ = new Versionlog(NULL,ui_model_vocab,ui_model_logs);
    QFile file(":/ui/QSS-master/ConsoleStyle.qss");
    file.open(QFile::ReadOnly);QString stylesheet = tr(file.readAll());
    versionlog_->setStyleSheet(stylesheet);file.close();
    connect(versionlog_, &Versionlog::finished, versionlog_, &QObject::deleteLater);//保证正确释放控件内存
    connect(this, &Widget::ui2version_log,versionlog_,&Versionlog::recv_log);
    connect(this, &Widget::ui2version_vocab,versionlog_,&Versionlog::recv_vocab);
    versionlog_->show();
    versionlog_->exec();
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
        #if defined(GGML_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
            int maxngl = log.split("=")[1].toInt()+1;//gpu负载层数是n_layer+1
            emit ui2bot_maxngl(maxngl);
            ngl_slider->setMaximum(maxngl);
        #endif
    }
    //截获装载进度,需要修改llama.cpp里的llama_load_model_from_file函数
    /*
    while (percentage > cur_percent) 
    {
        cur_percent++;
        LLAMA_LOG_INFO("load_percent = %d",cur_percent);
        if (cur_percent >= 100) {
            LLAMA_LOG_INFO("\n");
        }
    }
    if(percentage > *cur_percentage_p)
    {
        *cur_percentage_p = percentage;
    }
    */
    //注意匹配格式
    else if(log.contains("load_percent"))
    {
        //上一次也是这个则删除它
        if(load_percent_tag)
        {
            ui_model_logs.removeLast();
        }
        load_percent_tag = true;
        load_percent = log.split("=")[1].toInt();
        load_log_play();//按日志显示装载进度
    }
    // else if(log.contains("CPU buffer size"))
    // {
    //     model_memusage = log.split("=")[1].split("MiB")[0];
    //     ui->mem_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_memusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_memusage + "\n" + wordsObj["else unknown"].toString());

    // }
    // else if(log.contains("KV self size"))
    // {
    //     ctx_memusage = log.split("=")[1].split("MiB")[0];
    //     ui->mem_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_memusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_memusage + "\n" + wordsObj["else unknown"].toString());
    // }
    // else if(log.contains("KV buffer size"))
    // {
    //     ctx_vramusage = log.split("=")[1].split("MiB")[0];
    //     ui->vram_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_vramusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_vramusage + "\n" + wordsObj["else unknown"].toString());
    // }
    // else if(log.contains("offloading 0 repeating layers to GPU"))
    // {
    //     model_vramusage = "0";
    //     ctx_vramusage = "0";
    //     ui->vram_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_vramusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_vramusage + "\n" + wordsObj["else unknown"].toString());
    // }


    if(log == "\n")
    {
        emit ui2version_log(log);//单条记录
        ui_model_logs << log;//总记录
    }
    else
    {
        emit ui2version_log(dateTimeString + log);//单条记录 
        ui_model_logs << dateTimeString + log;//总记录
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
    if (obj == ui->input && event->type() == QEvent::ContextMenu)
    {
        QContextMenuEvent *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        
        // 创建菜单并添加动作
        QMenu menu(this);
        for(int i=0;i<questions.size();++i)
        {
            
            QAction *action = menu.addAction(questions.at(i));

            // 连接信号和槽
            connect(action, &QAction::triggered, this, [=]() {ui->input->setPlainText(questions.at(i));});
        }

        // 显示菜单
        menu.exec(contextMenuEvent->globalPos());
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
        ui_state = "ui:"+wordsObj["clicked"].toString() + QString("api") + wordsObj["set"].toString();reflash_state(ui_state,3);
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

    return QObject::eventFilter(obj, event);

}