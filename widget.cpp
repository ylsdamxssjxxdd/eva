//主函数和主要槽函数

#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    //-------------初始化ui-------------
    ui->setupUi(this);

    //-------------初始化语言-------------
    QLocale locale = QLocale::system(); // 获取系统locale
    QLocale::Language language = locale.language(); // 获取语言
    if(locale.languageToString(language) == "English"){getWords(":/english.json");}
    else{getWords(":/chinese.json");}
    //-------------初始化约定模板-------------
    ui_system_prompt = DEFAULT_PROMPT;ui_DATES.system_prompt = DEFAULT_PROMPT;ui_DATES.input_pfx = DEFAULT_PREFIX;ui_DATES.input_sfx = DEFAULT_SUFFIX;ui_DATES.is_load_tool = false;
    ui_DATES.extra_stop_words = QStringList(ui_DATES.input_pfx + ":\n");//只有这个有用其它不要加了,set_data函数会自己改
    date_map.insert("qwen", ui_DATES);
    date_map.insert("alpaca", {"Below is an instruction that describes a task. Write a response that appropriately completes the request.", "### Instruction", "### Response",false,QStringList{}});
    date_map.insert("chatML", {"<|im_start|>system \nYou are a helpful assistant.<|im_end|>", "<|im_start|>user", "<|im_end|>\n<|im_start|>assistant",false,QStringList{}});
    date_map.insert(wordsObj["troll"].toString(), {wordsObj["you are a troll please respect any question for user"].toString(), "### " + wordsObj["user"].toString(), "### " + wordsObj["troll"].toString(),false,QStringList{}});
    date_map.insert("eva",{wordsObj["You are an ultimate humanoid weapon of war, please wait for the driver control instructions"].toString(), "### " + wordsObj["driver"].toString(), "### eva",false,QStringList{}});
    //-------------默认展示内容-------------
    ui_model_vocab = wordsObj["lode model first"].toString();//模型词表提示
    QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
    ui->set->setIcon(QIcon(":/ui/assimp_tools_icon.ico"));//设置设置图标
    ui_state = "ui:" + wordsObj["click load and choose gguf file"].toString();reflash_state(ui_state,USUAL_);
    ui_state = "ui:" + wordsObj["right click link api"].toString();reflash_state(ui_state,USUAL_);
    #if defined(BODY_USE_CUBLAST)
    //this->setWindowTitle(QString("eva-cuda") + VERSION);
#else
    ui->vcore_bar->setVisible(0);//如果没有使用cuda则不显示gpu_bar
    ui->vram_bar->setVisible(0);
#endif
    init_move();//初始化动画参数
    QFile file(":/ui/QSS-master/MacOS.qss");//加载皮肤
    file.open(QFile::ReadOnly);QString stylesheet = tr(file.readAll());
    this->setStyleSheet(stylesheet);file.close();
    ui->output->setStyleSheet("background-color: white;");//设置输出区背景为纯白
    ui->input->setStyleSheet("background-color: white;");//设置输入区背景为纯白
    ui->mem_bar->message = wordsObj["mem"].toString();//进度条里面的文本
    ui->vram_bar->message = wordsObj["vram"].toString();//进度条里面的文本
    ui->kv_bar->message = wordsObj["brain"].toString();//进度条里面的文本
    ui->cpu_bar->message = "cpu";//进度条里面的文本
    ui->vcore_bar->message = "gpu";//进度条里面的文本
    //-------------初始化dialog-------------
    setApiDialog();//设置api选项
    set_DateDialog();//设置约定选项
    set_SetDialog();//设置设置选项
    ui_state_init();//初始界面状态
    //-------------默认启用功能-------------
    //this->setMouseTracking(true);//开启鼠标跟踪
    //ui->state->setMouseTracking(true);//开启鼠标跟踪
    QObject::connect(ui->state,&customPlainTextEdit::createExpend,this,&Widget::recv_createExpend);//传递信号创建更多窗口
    //ui->output->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    ui->input->setContextMenuPolicy(Qt::NoContextMenu);//取消右键菜单
    ui->input->installEventFilter(this);//安装事件过滤器
    ui->load->installEventFilter(this);//安装事件过滤器
    api_ip_LineEdit->installEventFilter(this);//安装事件过滤器
    ui->state->setLineWrapMode(QPlainTextEdit::NoWrap);// 禁用自动换行
    ui->state->setFocus();//设为当前焦点
    //-------------获取cpu内存信息-------------
    max_thread = std::thread::hardware_concurrency();
    ui_SETTINGS.nthread = max_thread*0.7;
    nthread_slider->setRange(1,max_thread);//设置线程数滑块的范围
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    timer->start(300); // 多少ms更新一次
    ui->cpu_bar->setToolTip(wordsObj["nthread/maxthread"].toString()+"  "+QString::number(ui_SETTINGS.nthread)+"/"+QString::number(std::thread::hardware_concurrency()));
    //ui->mem_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_memusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_memusage + "\n" + wordsObj["else unknown"].toString());
    //ui->vram_bar->setToolTip(wordsObj["model"].toString() + wordsObj["use MB"].toString() + ":" + model_vramusage + "\n" + wordsObj["ctx"].toString() + wordsObj["use MB"].toString() + ":" + ctx_vramusage + "\n" + wordsObj["else unknown"].toString());
    //-------------输出/状态区滚动条控制-------------
    output_scrollBar =  ui->output->verticalScrollBar();
    connect(output_scrollBar, &QScrollBar::valueChanged, this, &Widget::output_scrollBarValueChanged);
    state_scrollBar =  ui->state->verticalScrollBar();
    connect(state_scrollBar, &QScrollBar::valueChanged, this, &Widget::state_scrollBarValueChanged);
    //-------------添加右击问题-------------
    QDate currentDate = QDate::currentDate();//历史中的今天
    QString dateString = currentDate.toString("MM" + wordsObj["month"].toString() + "dd" + wordsObj["day"].toString());
    for(int i = 1; i < 18;++i)
    {
        if(i == 4){questions<<wordsObj[QString("Q%1").arg(i)].toString().replace("{today}",dateString);}//历史中的今天
        else
        {
            questions<<wordsObj[QString("Q%1").arg(i)].toString();
        }
        
    }
    
    //-------------初始化工具-------------
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
    emit server_kill();
}

//用户点击装载按钮处理
void Widget::on_load_clicked()
{
    reflash_state("ui:"+wordsObj["clicked load"].toString(),SIGNAL_);
    //用户选择模型位置
    QString model_path = QFileDialog::getOpenFileName(this,wordsObj["choose soul in eva"].toString(),DEFAULT_MODELPATH);
    if(model_path==""){return;}//如果路径没选好就让它等于上一次的路径
    is_api = false;//只要点击装载有东西就不再是api模式
    ui_SETTINGS.modelpath = model_path;//模型路径变化则重置参数
    //分析显存，如果可用显存比模型大1.2倍则自动将gpu负载设置为999
    emit gpu_reflash();//强制刷新gpu信息
    QFileInfo fileInfo(ui_SETTINGS.modelpath);//获取文件大小
    int modelsize_MB = fileInfo.size() /1024/1024;
    if(vfree>modelsize_MB*1.2)
    {
        //reflash_state("ui:" +wordsObj["vram enough, gpu offload auto set 999"].toString(),SUCCESS_);
        ui_SETTINGS.ngl= 999;
    }
    else{ui_SETTINGS.ngl= 0;}
    //发送设置参数给bot
    emit ui2bot_set(ui_SETTINGS,1);//设置应用完会触发preLoad
}

// 装载前动作
void Widget::preLoad()
{
    is_load = false;//重置is_load标签
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
        change_api_dialog(1);
        is_load = true;//标记模型已装载,只有这个是true才允许后续动作
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
                decode_pTimer->stop();
                reflash_state(ui_state,SUCCESS_);
                is_test = false;
                is_run = false;
                //恢复
                test_question_index.clear();test_count = 0;test_score=0;test_tokens=0;
                ui->send->setEnabled(1);
                ui->load->setEnabled(1);
                ui->date->setEnabled(1);
                ui->set->setEnabled(1);
                return;
            }
            ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
            data.input_prompt = input;data.n_predict=1;
            emit ui2net_data(data);
            reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0,Qt::black);
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
                //恢复
                ui->send->setEnabled(1);
                ui->load->setEnabled(1);
                ui->date->setEnabled(1);ui->set->setEnabled(1);
                return;
            }
            ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
            data.input_prompt = input;data.n_predict=ui_SETTINGS.npredict;
            emit ui2net_data(data);
            reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0,Qt::black);
        }
        else
        {
            input = ui->input->toPlainText().toUtf8().data();//获取用户输入
            ui->input->clear();//用户输入区清空输入

            if(input ==  wordsObj["Q16"].toString())//开始测试开始
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " " + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,SIGNAL_);
                clearQuestionlist();//清空题库
                makeTestQuestion(":/ceval-exam/val");//构建测试问题集
                makeTestIndex();//构建测试问题索引
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,SIGNAL_);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标

                api_addhelpinput();//加入引导问题
                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
                data.input_prompt = input;data.n_predict=1;
                emit ui2net_data(data);
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0,Qt::black);
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " +wordsObj["current api"].toString() + " " + current_api);  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,SIGNAL_);
                
            }
            else if (input ==  wordsObj["Q17"].toString())//自定义数据集
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " "  + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,SIGNAL_);
                QString custom_csvpath = QFileDialog::getOpenFileName(this,wordsObj["Q17"].toString(),"D:/soul","CSV files (*.csv)");//用户选择自定义的csv文件
                clearQuestionlist();//清空题库
                readCsvFile(custom_csvpath);//构建测试问题集
                makeTestIndex();//构建测试问题索引
                if(test_question_index.size()==0){ui_state = "ui:0"+ wordsObj["question"].toString();reflash_state(ui_state,WRONG_);return;}
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,SIGNAL_);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标
                api_addhelpinput();//加入引导问题
                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
                
                ui_assistant_history << input;
                
                data.input_prompt = input;data.n_predict=1;
                emit ui2net_data(data);
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0,Qt::black);
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " +wordsObj["current api"].toString() + " " + current_api);  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,SIGNAL_);
                
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
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0,Qt::black);
            }
            else if(ui_mode == CHAT_)
            {
                ui_user_history << input;data.user_history = ui_user_history;data.assistant_history = ui_assistant_history;
                reflash_output("\n" + ui_DATES.input_pfx + ":\n" + ui_user_history.last() + "\n" + ui_DATES.input_sfx + ":\n",0,Qt::black);
                data.n_predict=ui_SETTINGS.npredict;
                emit ui2net_data(data);
            } 
            else if(ui_mode == COMPLETE_)//直接用output上的文本进行推理
            {
                input = ui->output->toPlainText().toUtf8().data();
                data.input_prompt = input;data.n_predict=ui_SETTINGS.npredict;
                emit ui2net_data(data);
            }
               
        }
        ui->input->setFocus();
        is_run =true;//模型正在运行标签
        emit ui2net_push();
        ui->date->setDisabled(1);
        ui->load->setDisabled(1);
        ui->send->setDisabled(1);
        ui->set->setEnabled(0);
        return;
    }
    
    //如果是对话模式,主要流程就是构建input,发送input,然后触发推理
    if(ui_mode == CHAT_)
    {
        if(ui_need_predecode)
        {
            input = "<ylsdamxssjxxdd:predecode>";//告诉bot开始预解码
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
        else//正常情况!!!
        {
            if(tool_result==""){input = ui->input->toPlainText().toUtf8().data();ui->input->clear();}
            //-----------------------Q16/Q17测试相关----------------------------
            if(input ==  wordsObj["Q16"].toString())//开始测试开始
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " "  + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,SIGNAL_);
                clearQuestionlist();//清空题库
                makeTestQuestion(":/ceval-exam/val");//构建测试问题集
                makeTestIndex();//构建测试问题索引
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,SIGNAL_);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标
                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                emit ui2bot_input({makeHelpInput() + ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n" + wordsObj["answer"].toString() + ":"},1);//传递用户输入
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " + ui_SETTINGS.modelpath.split("/").last());  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,SIGNAL_);
                
            }
            else if (input ==  wordsObj["Q17"].toString())//自定义数据集
            {
                ui_state = "ui:"+ wordsObj["clicked"].toString() + wordsObj["test"].toString() + " "  + wordsObj["npredict"].toString() + wordsObj["limited"].toString() + "1";reflash_state(ui_state,SIGNAL_);
                QString custom_csvpath = QFileDialog::getOpenFileName(this,wordsObj["Q17"].toString(),"D:/soul","CSV files (*.csv)");//用户选择自定义的csv文件
                clearQuestionlist();//清空题库
                readCsvFile(custom_csvpath);//构建测试问题集
                makeTestIndex();//构建测试问题索引
                if(test_question_index.size()==0){ui_state = "ui:0"+ wordsObj["question"].toString();reflash_state(ui_state,WRONG_);return;}
                ui_state = "ui:"+ wordsObj["question make well"].toString() + " " + QString::number(test_list_question.size())+ wordsObj["question"].toString();reflash_state(ui_state,SIGNAL_);
                test_time.restart();
                QApplication::setWindowIcon(QIcon(":/ui/c-eval.png"));// 设置应用程序图标

                is_test = true;
                input = QString::number(test_count+1) + ". "+test_list_question.at(test_question_index.at(0));
                
                emit ui2bot_input({makeHelpInput() + ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},1);//传递用户输入
                this->setWindowTitle(wordsObj["test"].toString() +"0/" + QString::number(test_list_question.size()) + "   " + ui_SETTINGS.modelpath.split("/").last());  
                ui_state = "ui:"+ wordsObj["add help question"].toString();reflash_state(ui_state,SIGNAL_);
            }
            //-----------------------Q14连续回答相关----------------------------
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
            //-----------------------Q15多模态相关----------------------------
            else if(input == wordsObj["Q15"].toString())
            {
                QString imagepath = QFileDialog::getOpenFileName(this,wordsObj["Q15"].toString(),"D:/soul","(*.png *.jpg *.bmp)");//用户选择图片
                input = "<ylsdamxssjxxdd:imagedecode>";
                ui_output = "\nfile:///" + imagepath + "\n";output_scroll();
                emit ui2bot_input({ui_DATES.input_pfx+ ":\n",input,ui_DATES.input_sfx + ":\n"},0);//传递用户输入  
                emit ui2bot_imagepath(imagepath);
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
    decode_play();//开启推理动画

    ui_state_pushing();//推理中界面状态
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
    else
    {
        //如果挂载了工具,则尝试提取里面的json
        if(is_load_tool)
        {
            QStringList func_arg_list;
            func_arg_list = JSONparser(ui_assistant_history.last());//取巧预解码的系统指令故意不让解析出json
            if(func_arg_list.size() == 0)
            {
                is_run = false;
                ui_state_normal();//待机界面状态
            }
            else
            {
                //调用工具
                reflash_state("ui:" + wordsObj["clicked"].toString() + " " + func_arg_list.front(),SIGNAL_);
                emit ui2tool_func_arg(func_arg_list);//传递函数名和参数
                emit ui2tool_push();
            }

        }
        //正常结束
        else
        {
            is_run = false;
            ui_state_normal();//待机界面状态
        }
        decode_pTimer->stop();
        decode_action=0;
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
    reflash_state("ui:" + wordsObj["reset ok"].toString(),SUCCESS_);
    
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
    //is_datereset = true;
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
    
    reflash_state("···········"+ wordsObj["set"].toString() + "···········",USUAL_);
    
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
        reflash_state("ui:"+ wordsObj["clicked"].toString()+ wordsObj["shut down"].toString(),SIGNAL_);
        emit ui2bot_help_input(0);//通知模型不再加入引导问题
        test_question_index.clear();//清空待测试问题列表
        query_list.clear();//清空待回答列表
        if(is_api){emit ui2net_stop(1);}
        else{emit ui2bot_stop();}//传递推理停止信号,模型停止后会再次触发on_reset_clicked()
        return;
    }
    reflash_state("ui:"+ wordsObj["clicked reset"].toString(),SIGNAL_);

    if(ui_mode == CHAT_){ui->output->clear();}
    ui_state_normal();//待机界面状态

    if(is_api)
    {
        ui_assistant_history.clear();//清空历史
        ui_user_history.clear();//清空历史
        this->setWindowTitle(wordsObj["current api"].toString() + " " + current_api);
        QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
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
    ui_state = "ui:"+wordsObj["clicked date"].toString();reflash_state(ui_state,SIGNAL_);
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
    ui_state = "ui:"+wordsObj["clicked"].toString()+wordsObj["set"].toString();reflash_state(ui_state,SIGNAL_);
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
//设置用户设置内容
void Widget::set_set()
{
    ui_SETTINGS.temp = temp_slider->value()/100.0;
    ui_SETTINGS.repeat = repeat_slider->value()/100.0;
    ui_SETTINGS.npredict = npredict_slider->value();

    ui_SETTINGS.nthread =nthread_slider->value();
    ui_SETTINGS.nctx = nctx_slider->value();//获取nctx滑块的值
    ui_SETTINGS.batch = batch_slider->value();//获取nctx滑块的值
#if defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    ui_SETTINGS.ngl = ngl_slider->value();//获取npl滑块的值
#endif

    ui_SETTINGS.lorapath = lora_LineEdit->text();
    ui_SETTINGS.mmprojpath = mmproj_LineEdit->text();

    ui_SETTINGS.complete_mode = complete_btn->isChecked();
    if(chat_btn->isChecked()){ui_mode=CHAT_;}
    else if(complete_btn->isChecked()){ui_mode=COMPLETE_;history_prompt="";}//history_prompt置空是为了下一次切换为对话模式时正确处理预解码
    else if(web_btn->isChecked()){ui_mode=SERVER_;}
    ui_port = port_lineEdit->text();
    
    // QDir checkDir(ui_SETTINGS.mmprojpath);
    // qDebug()<<ui_SETTINGS.mmprojpath<<checkDir.exists();
    // if (!checkDir.exists()) {ui_SETTINGS.mmprojpath="";ui_state = "ui:mmporj path not exit";reflash_state(ui_state,WRONG_);}// 目录不存在
    set_dialog->close();
    if(ui_mode!=CHAT_){prompt_box->setEnabled(0);tool_box->setEnabled(0);}//如果不是对话模式则禁用约定
    else{prompt_box->setEnabled(1);tool_box->setEnabled(1);}
    //从服务模式回来强行重载
    if(current_server && ui_mode!=SERVER_)
    {
        current_server=false;
        emit ui2bot_set(ui_SETTINGS,1);
    }
    else if(ui_mode!=SERVER_){emit ui2bot_set(ui_SETTINGS,is_load);}

    //server.exe接管,不需要告知bot约定
    if(ui_mode == SERVER_){emit server_kill();}//关闭重启
    if(ui_mode==SERVER_)
    {
        ui_state_servering();//服务中界面状态
        serverControl();
    }
    else
    {
        if(is_api)//api模式不发信号
        {
            ui_user_history.clear();
            ui_assistant_history.clear();
            if(ui_mode == CHAT_){current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_chat_endpoint;}
            else{current_api = "http://" + apis.api_ip + ":" + apis.api_port + apis.api_complete_endpoint;}
            ui_state = "ui:"+wordsObj["current api"].toString() + " " + current_api;reflash_state(ui_state,USUAL_);
            this->setWindowTitle(wordsObj["current api"].toString() + " " + current_api);
        }
    }
}

//应用用户设置的约定内容
void Widget::set_date()
{
    ui_extra_prompt = extra_TextEdit->toPlainText();
    ui_system_prompt = system_TextEdit->toPlainText();
    //合并附加指令
    if(ui_extra_prompt!=""){ui_DATES.system_prompt = ui_system_prompt + "\n\n" + ui_extra_prompt;}
    else{ui_DATES.system_prompt = ui_system_prompt;}

    ui_DATES.input_pfx = input_pfx_LineEdit->text();
    ui_DATES.input_sfx = input_sfx_LineEdit->text();
    ui_DATES.is_load_tool = is_load_tool;
    ui_template = prompt_comboBox->currentText();
    ui_extra_lan = switch_lan_button->text();

    //处理额外停止标志
    ui_DATES.extra_stop_words.clear();//重置额外停止标志
    ui_DATES.extra_stop_words << ui_DATES.input_pfx + ":\n";//默认第一个是用户昵称，检测出来后下次回答将不再添加前缀
    ui_DATES.extra_stop_words << "<|im_end|>";//防chatml
    if(ui_DATES.is_load_tool)//如果挂载了工具则增加额外停止标志
    {
        ui_DATES.extra_stop_words << "Observation:";
        ui_DATES.extra_stop_words << wordsObj["tool_observation"].toString();
        ui_DATES.extra_stop_words << wordsObj["tool_observation2"].toString();
    }

    date_dialog->close();
    emit ui2bot_date(ui_DATES);
}

// server.exe接管
void Widget::serverControl()
{
    current_server = true;
    //如果还没有选择模型路径
    if(ui_SETTINGS.modelpath=="")
    {
        ui_SETTINGS.modelpath = QFileDialog::getOpenFileName(this,wordsObj["choose soul in eva"].toString(),DEFAULT_MODELPATH);
    }
    
    emit ui2bot_free();
    is_load = false;

    QString resourcePath = ":/server.exe";
    QString localPath = "server.exe";

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
    if(ui_SETTINGS.lorapath!=""){arguments << "--no-mmap";arguments << "--lora" << ui_SETTINGS.lorapath;}//挂载lora不能开启mmp
    if(ui_SETTINGS.mmprojpath!=""){arguments << "--mmproj" << ui_SETTINGS.mmprojpath;}

    // 开始运行程序
    QProcess *server_process;//用来启动server.exe
    server_process = new QProcess(this);//实例化
    connect(server_process, &QProcess::started, this, &Widget::server_onProcessStarted);//连接开始信号
    connect(server_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),this, &Widget::server_onProcessFinished);//连接结束信号
    connect(this,&Widget::server_kill, server_process, &QProcess::kill);

    server_process->start(program, arguments);
    setWindowState(windowState() | Qt::WindowMaximized);//设置窗口最大化
    reflash_state("ui:   " + wordsObj["eva"].toString() + wordsObj["eva expand"].toString(),EVA_);
    
    //连接信号和槽,获取程序的输出
    connect(server_process, &QProcess::readyReadStandardOutput, [=]() {
        ui_output = server_process->readAllStandardOutput();
        if(ui_output.contains("model loaded"))
        {
            ui_output += "\n"+wordsObj["browser at"].toString() +QString(" http://")+ipAddress+":"+ui_port;
            ui_output += "\n"+wordsObj["chat"].toString()+wordsObj["endpoint"].toString()+ " " + "/v1/chat/completions";
            ui_output += "\n"+wordsObj["complete"].toString()+wordsObj["endpoint"].toString()+ " " + "/completion"+"\n";
            ui_state = "ui:server " +wordsObj["on"].toString()+wordsObj["success"].toString()+ ","+wordsObj["browser at"].toString()+ ipAddress + ":"+ ui_port;reflash_state(ui_state,SUCCESS_);

        }//替换ip地址
        output_scroll();
    });    connect(server_process, &QProcess::readyReadStandardError, [=]() {
        ui_output = server_process->readAllStandardError();
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
    emit ui2expend_vocab(ui_model_vocab);
}

//创建扩展窗口
void Widget::recv_createExpend()
{
    expend_ = new Expend(NULL,wordsObj,ui_model_vocab,ui_model_logs);
    connect(expend_, &Expend::finished, expend_, &QObject::deleteLater);//保证正确释放控件内存
    connect(this, &Widget::ui2expend_log,expend_,&Expend::recv_log);
    connect(this, &Widget::ui2expend_vocab,expend_,&Expend::recv_vocab);
    expend_->show();
    expend_->exec();
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
        emit ui2expend_log(log);//单条记录
        ui_model_logs << log;//总记录
    }
    else
    {
        emit ui2expend_log(dateTimeString + log);//单条记录 
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

    return QObject::eventFilter(obj, event);
}

//传递模型预解码的内容
void Widget::recv_predecode(QString bot_predecode_)
{
    bot_predecode = bot_predecode_;
}