#include <QCoreApplication>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStyleFactory>
#include <locale>

#include "expend.h"
#include "widget.h"
#include "xbot.h"
#include "xnet.h"
#include "xtool.h"

#include "utils/gpuchecker.h"
#include "utils/cpuchecker.h"

int main(int argc, char* argv[]) {
    // 测试用户设备是否支持运行该机体
    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--test") {
            return 0;
        }
    }

    // 设置linux下动态库的默认路径
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");  // 获取镜像的路径
    QString ldLibraryPath = appDirPath + "/usr/lib";
    std::string currentPath = ldLibraryPath.toLocal8Bit().constData();
    setenv("LD_LIBRARY_PATH", currentPath.c_str(), 1);  // 指定找动态库的默认路径 LD_LIBRARY_PATH
#ifdef BODY_USE_CUDA
    // cuda版本可以在系统的 /usr/local/cuda/lib64 中寻找库
    // std::string cudaPath = "/usr/local/cuda/lib64";
    // setenv("LD_LIBRARY_PATH", (currentPath + ":" + cudaPath).c_str(), 1);// 指定找动态库的默认路径 LD_LIBRARY_PATH
#endif
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);                                        //自适应缩放
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);  //适配非整数倍缩放
    QApplication a(argc, argv);                                                                               //事件实例
    QApplication::setStyle(QStyleFactory::create("Fusion"));                                                  //现代风格

    // 设置创建EVA_TEMP文件夹所在的目录
#if BODY_LINUX_PACK
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString appImagePath = env.value("APPIMAGE");
    const QFileInfo fileInfo(appImagePath);
    const QString applicationDirPath = fileInfo.absolutePath();  // 在打包程序运行时所在目录创建EVA_TEMP文件夹
#else
    const QString applicationDirPath = QCoreApplication::applicationDirPath();  // 就在当前目录创建EVA_TEMP文件夹
#endif
    qDebug() << "EVA_TEMP: " + applicationDirPath;

    //------------------实例化主要节点------------------
    Widget w(nullptr, applicationDirPath);       //窗口实例
    Expend expend(nullptr, applicationDirPath);  //增殖窗口实例
    xTool tool(applicationDirPath);              //工具实例
    xBot bot;                                    //模型实例
    xNet net;                                    //链接实例
    gpuChecker gpuer;  //监测显卡信息
    cpuChecker cpuer;  //监视系统信息

    //-----------------初始值设定-----------------------
    expend.wordsObj = bot.wordsObj = net.wordsObj = tool.wordsObj = w.wordsObj;  //传递语言
    expend.max_thread = w.max_thread;
    w.currentpath = w.historypath = expend.currentpath = applicationDirPath; // 默认打开路径
    w.whisper_model_path = QString::fromStdString(expend.whisper_params.model);
    w.speech_params = expend.speech_params;
    expend.set_sys_speech(w.sys_speech_list);  // 设置可用系统声源
    // expend.init_expend();                    //更新一次expend界面

    //------------------注册信号传递变量-------------------
    qRegisterMetaType<MODEL_PARAMS>("MODEL_PARAMS");  //注册PARAMS作为信号传递变量
    qRegisterMetaType<QColor>("QColor");              //注册QColor作为信号传递变量
    qRegisterMetaType<SIGNAL_STATE>("SIGNAL_STATE");  //注册STATE作为信号传递变量
    qRegisterMetaType<DATES>("DATES");
    qRegisterMetaType<SETTINGS>("SETTINGS");
    qRegisterMetaType<INPUTS>("INPUTS");
    qRegisterMetaType<QVector<Embedding_vector>>("QVector<Embedding_vector>");
    qRegisterMetaType<Speech_Params>("Speech_Params");
    qRegisterMetaType<QPair<QString, QString>>("QPair<QString, QString>");
    qRegisterMetaType<std::vector<Brain_Cell>>("std::vector<Brain_Cell>");
    qRegisterMetaType<Syncrate_Manager>("Syncrate_Manager");
    qRegisterMetaType<ENDPOINT_DATA>("ENDPOINT_DATA");
    qRegisterMetaType<APIS>("APIS");

    //------------------开启多线程 todo ------------------------
    QThread* gpuer_thread = new QThread;gpuer.moveToThread(gpuer_thread);gpuer_thread->start();
    QThread* cpuer_thread = new QThread;cpuer.moveToThread(cpuer_thread);cpuer_thread->start();
    QThread* bot_thread = new QThread;bot.moveToThread(bot_thread);bot_thread->start();
    QThread* tool_thread = new QThread;tool.moveToThread(tool_thread);tool_thread->start();
    QThread* net_thread = new QThread;net.moveToThread(net_thread);net_thread->start();

    //------------------连接模型和窗口-------------------
    QObject::connect(&bot, &xBot::bot2ui_params, &w, &Widget::recv_params);              // bot将模型参数传递给ui
    QObject::connect(&bot, &xBot::bot2ui_output, &w, &Widget::reflash_output);           //窗口输出区更新
    QObject::connect(&bot, &xBot::bot2ui_state, &w, &Widget::reflash_state);             //窗口状态区更新
    QObject::connect(&bot, &xBot::bot2ui_play, &w, &Widget::recv_play);                  //播放加载动画
    QObject::connect(&bot, &xBot::bot2ui_loadover, &w, &Widget::recv_loadover);          //完成加载模型
    QObject::connect(&bot, &xBot::bot2ui_pushover, &w, &Widget::recv_pushover);          //完成推理
    QObject::connect(&bot, &xBot::bot2ui_resetover, &w, &Widget::recv_resetover);        //完成重置,预解码约定
    QObject::connect(&bot, &xBot::bot2ui_stopover, &w, &Widget::recv_stopover);          //完成停止
    QObject::connect(&bot, &xBot::bot2ui_arrivemaxctx, &w, &Widget::recv_arrivemaxctx);  //模型达到最大上下文
    QObject::connect(&bot, &xBot::bot2ui_reload, &w, &Widget::recv_reload);              //设置参数改变,重载模型
    QObject::connect(&bot, &xBot::bot2ui_datereset, &w, &Widget::recv_datereset);        // bot发信号请求ui触发reset
    QObject::connect(&bot, &xBot::bot2ui_setreset, &w, &Widget::recv_setreset);          // bot发信号请求ui触发reset
    QObject::connect(&bot, &xBot::bot2ui_tokens, &w, &Widget::recv_tokens);              //传递测试解码token数量
    QObject::connect(&bot, &xBot::bot2ui_predecode, &w, &Widget::recv_predecode);        //传递模型预解码的内容
    QObject::connect(&bot, &xBot::bot2ui_freeover_loadlater, &w, &Widget::recv_freeover_loadlater);          //模型释放完毕并重新装载
    QObject::connect(&w, &Widget::ui2bot_stop,&bot,&xBot::recv_stop);//传递停止信号
    QObject::connect(&w, &Widget::ui2bot_loadmodel, &bot, &xBot::load);                  //开始加载模型
    QObject::connect(&w, &Widget::ui2bot_predict, &bot, &xBot::predict);                 //开始推理
    QObject::connect(&w, &Widget::ui2bot_preDecode, &bot, &xBot::preDecode);             //开始预解码
    QObject::connect(&w, &Widget::ui2bot_preDecodeImage, &bot, &xBot::preDecodeImage); //开始预解码图像
    QObject::connect(&w, &Widget::ui2bot_reset, &bot, &xBot::recv_reset);              //传递重置信号
    QObject::connect(&w, &Widget::ui2bot_date, &bot, &xBot::recv_date);                //传递约定内容
    QObject::connect(&w, &Widget::ui2bot_set, &bot, &xBot::recv_set);                  //传递设置内容
    QObject::connect(&w, &Widget::ui2bot_language, &bot, &xBot::recv_language);        //传递使用的语言
    QObject::connect(&w, &Widget::ui2bot_free, &bot, &xBot::recv_free);                //释放模型和上下文
    QObject::connect(&bot, &xBot::bot2ui_kv, &w, &Widget::recv_kv);                    //传递缓存量
    QObject::connect(&w, &Widget::ui2bot_dateset, &bot, &xBot::recv_dateset);          //自动装载
    QObject::connect(&w, &Widget::ui2bot_debuging, &bot, &xBot::recv_debuging);        //传递debug中状态

    //------------------监测gpu信息-------------------
    QObject::connect(&gpuer, &gpuChecker::gpu_status, &w, &Widget::recv_gpu_status);    //传递gpu信息
    QObject::connect(&gpuer, &gpuChecker::gpu_status, &bot, &xBot::recv_gpu_status);    //传递gpu信息
    QObject::connect(&w, &Widget::gpu_reflash, &gpuer, &gpuChecker::chekGpu);  //强制刷新gpu信息

    //------------------监测系统信息-------------------
    QObject::connect(&cpuer, &cpuChecker::cpu_status, &w, &Widget::recv_cpu_status);    //传递cpu信息
    QObject::connect(&w, &Widget::cpu_reflash, &cpuer, &cpuChecker::chekCpu);  //强制刷新cpu信息

    //------------------连接窗口和增殖窗口-------------------
    QObject::connect(&w, &Widget::ui2expend_syncrate, &expend, &Expend::recv_syncrate);                          //传递同步率结果
    QObject::connect(&w, &Widget::ui2expend_language, &expend, &Expend::recv_language);                          //传递使用的语言
    QObject::connect(&w, &Widget::ui2expend_show, &expend, &Expend::recv_expend_show);                           //通知显示扩展窗口
    QObject::connect(&w, &Widget::ui2expend_speechdecode, &expend, &Expend::recv_speechdecode);                    //开始语音转文字
    QObject::connect(&expend, &Expend::expend2ui_speechdecode_over, &w, &Widget::recv_speechdecode_over);          //转换完成返回结果
    QObject::connect(&expend, &Expend::expend2ui_whisper_modelpath, &w, &Widget::recv_whisper_modelpath);        //传递模型路径
    QObject::connect(&expend, &Expend::expend2ui_state, &w, &Widget::reflash_state);                             //窗口状态区更新
    QObject::connect(&expend, &Expend::expend2ui_embeddingdb_describe, &w, &Widget::recv_embeddingdb_describe);  //传递知识库的描述
    QObject::connect(&expend, &Expend::expend2ui_speechparams, &w, &Widget::recv_speechparams);                    //传递文转声参数

    //------------------连接bot和增殖窗口-------------------
    QObject::connect(&bot, &xBot::bot2expend_vocab, &expend, &Expend::recv_vocab);
    QObject::connect(&bot, &xBot::bot2expend_brainvector, &expend, &Expend::recv_brainvector);  //传递记忆向量和上下文长度
    QObject::connect(&bot, &xBot::bot_llama_log, &expend, &Expend::recv_llama_log);

    //------------------连接net和窗口-------------------
    QObject::connect(&net, &xNet::net2ui_output, &w, &Widget::reflash_output);    //窗口输出区更新
    QObject::connect(&net, &xNet::net2ui_state, &w, &Widget::reflash_state);      //窗口状态区更新
    QObject::connect(&net, &xNet::net2ui_pushover, &w, &Widget::recv_pushover);   //完成推理
    QObject::connect(&w, &Widget::ui2net_push, &net, &xNet::run);  //开始推理
    QObject::connect(&w, &Widget::ui2net_language, &net, &xNet::recv_language);   //传递使用的语言
    QObject::connect(&w, &Widget::ui2net_apis, &net, &xNet::recv_apis);           //传递api设置参数
    QObject::connect(&w, &Widget::ui2net_data, &net, &xNet::recv_data);           //传递端点参数
    QObject::connect(&w, &Widget::ui2net_stop, &net, &xNet::recv_stop);           //传递停止信号

    //------------------连接tool和窗口-------------------
    QObject::connect(&tool, &xTool::tool2ui_state, &w, &Widget::reflash_state);                   //窗口状态区更新
    QObject::connect(&tool, &xTool::tool2ui_controller, &w, &Widget::recv_controller);            //传递控制信息
    QObject::connect(&w, &Widget::recv_controller_over, &tool, &xTool::tool2ui_controller_over);  //传递控制完成结果
    QObject::connect(&tool, &xTool::tool2ui_pushover, &w, &Widget::recv_toolpushover);            //完成推理
    QObject::connect(&w, &Widget::ui2tool_language, &tool, &xTool::recv_language);                //传递使用的语言
    QObject::connect(&w, &Widget::ui2tool_exec, &tool, &xTool::Exec);              //开始推理

    //------------------连接增殖窗口和tool-------------------
    QObject::connect(&expend, &Expend::expend2tool_embeddingdb, &tool, &xTool::recv_embeddingdb);                  //传递已嵌入文本段数据
    QObject::connect(&expend, &Expend::expend2tool_embedding_serverapi, &tool, &xTool::recv_embedding_serverapi);  //传递嵌入服务端点

    QObject::connect(&tool, &xTool::tool2expend_draw, &expend, &Expend::recv_draw);          //开始绘制图像
    QObject::connect(&expend, &Expend::expend2tool_drawover, &tool, &xTool::recv_drawover);  //图像绘制完成

    w.show();  //展示窗口

    //---------------读取配置文件并执行------------------
    emit w.gpu_reflash();  //强制刷新gpu信息，为了获取未装载时的显存占用
    QFile configfile(applicationDirPath + "/EVA_TEMP/eva_config.ini");
    if (configfile.exists()) {
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        QString modelpath = settings.value("modelpath", "").toString();  //模型路径

        QFile modelpath_file(modelpath);
        if (modelpath_file.exists())  //模型存在的话才继续进行
        {
            // 读取配置文件中的值
            w.ui_SETTINGS.modelpath = modelpath;
            w.currentpath = w.historypath = expend.currentpath = modelpath;
            w.custom1_system_prompt = settings.value("custom1_system_prompt", "").toString();
            w.custom1_input_pfx = settings.value("custom1_input_pfx", "").toString();
            w.custom1_input_sfx = settings.value("custom1_input_sfx", "").toString();
            w.custom2_system_prompt = settings.value("custom2_system_prompt", "").toString();
            w.custom2_input_pfx = settings.value("custom2_input_pfx", "").toString();
            w.custom2_input_sfx = settings.value("custom2_input_sfx", "").toString();

            // ui显示值
            w.chattemplate_comboBox->setCurrentText(settings.value("chattemplate", "").toString());
            w.system_TextEdit->setText(settings.value("system_prompt", "").toString());
            w.input_pfx_LineEdit->setText(settings.value("input_pfx", "").toString());
            w.input_sfx_LineEdit->setText(settings.value("input_sfx", "").toString());
            w.terminal_checkbox->setChecked(settings.value("terminal_checkbox", "").toBool());
            w.calculator_checkbox->setChecked(settings.value("calculator_checkbox", "").toBool());
            w.knowledge_checkbox->setChecked(settings.value("knowledge_checkbox", "").toBool());
            w.controller_checkbox->setChecked(settings.value("controller_checkbox", "").toBool());
            w.stablediffusion_checkbox->setChecked(settings.value("stablediffusion_checkbox", "").toBool());
            w.toolguy_checkbox->setChecked(settings.value("toolguy_checkbox", "").toBool());
            w.interpreter_checkbox->setChecked(settings.value("interpreter_checkbox", "").toBool());
            if (settings.value("extra_lan", "").toString() != "zh") {
                w.switch_lan_change();
            }                                                                          //切换为英文
            w.extra_TextEdit->setText(settings.value("extra_prompt", "").toString());  //放到后面保护用户定义值

            w.temp_slider->setValue(settings.value("temp", "").toFloat() * 100);
            w.repeat_slider->setValue(settings.value("repeat", "").toFloat() * 100);
            w.npredict_slider->setValue(settings.value("npredict", "").toFloat());
            w.nthread_slider->setValue(settings.value("nthread", "").toInt());
            w.nctx_slider->setValue(settings.value("nctx", "").toInt());
            w.batch_slider->setValue(settings.value("batch", "").toInt());
            w.ngl_slider->setValue(settings.value("ngl", "").toInt());
            QFile checkFile(settings.value("lorapath", "").toString());
            if (checkFile.exists()) {
                w.lora_LineEdit->setText(settings.value("lorapath", "").toString());
            }
            QFile checkFile2(settings.value("mmprojpath", "").toString());
            if (checkFile2.exists()) {
                w.mmproj_LineEdit->setText(settings.value("mmprojpath", "").toString());
            }
            int mode_num = settings.value("ui_state", "").toInt();
            if (mode_num == 0) {
                w.chat_btn->setChecked(1);
            } else if (mode_num == 1) {
                w.complete_btn->setChecked(1);
            } else if (mode_num == 2) {
                w.web_btn->setChecked(1);
            }
            w.port_lineEdit->setText(settings.value("port", "").toString());

            // ui显示值传给ui内部值
            w.get_date();  //获取约定中的纸面值
            w.get_set();   //获取设置中的纸面值
            w.is_config = true;

            if (w.ui_state == SERVER_STATE) {
                w.serverControl();
            }  //自动启动服务
            else {
                emit w.ui2bot_dateset(w.ui_DATES, w.ui_SETTINGS);
            }  //自动装载模型
        }

        //是否需要自动重构知识库, 源文档在expend实例化时已经完成
        bool embedding_need = settings.value("embedding_need", "").toBool();
        if (embedding_need) {
            QString embedding_modelpath = settings.value("embedding_modelpath", "").toString();
            QFile embedding_modelpath_file(embedding_modelpath);
            if (embedding_modelpath_file.exists()) {
                expend.embedding_need_auto = true;
                expend.embedding_params.modelpath = embedding_modelpath;
                expend.embedding_server_start();  //启动嵌入服务,并执行嵌入
            } else                                //借助端点直接嵌入
            {
                expend.embedding_processing();                                                    //执行嵌入
                tool.embedding_server_api = settings.value("embedding_endpoint", "").toString();  //要让tool拿到api端点地址
            }
        }
    }

    //传递停止词和约定，因为第一次没有传递约定参数给bot
    bot.extra_stop_words = w.ui_DATES.extra_stop_words;               // 同步
    bot.gpt_params_.prompt = w.ui_DATES.system_prompt.toStdString();  // 同步

    return a.exec();  //进入事件循环
}