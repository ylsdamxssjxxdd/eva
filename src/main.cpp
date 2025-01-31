#include <QCoreApplication>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStyleFactory>
#include <locale>

#include "expend.h"
#include "utils/cpuchecker.h"
#include "utils/gpuchecker.h"
#include "widget.h"
#include "xbot.h"
#include "xnet.h"
#include "xtool.h"

int main(int argc, char* argv[]) {
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
    gpuChecker gpuer;                            //监测显卡信息
    cpuChecker cpuer;                            //监视系统信息

    //-----------------初始值设定-----------------------
    expend.wordsObj = bot.wordsObj = net.wordsObj = tool.wordsObj = w.wordsObj;  //传递语言
    expend.max_thread = w.max_thread;
    w.currentpath = w.historypath = expend.currentpath = applicationDirPath;  // 默认打开路径
    w.whisper_model_path = QString::fromStdString(expend.whisper_params.model);

    // expend.init_expend();                    //更新一次expend界面

    //------------------注册信号传递变量-------------------
    qRegisterMetaType<CHATS>("CHATS");
    qRegisterMetaType<MODEL_PARAMS>("MODEL_PARAMS");
    qRegisterMetaType<QColor>("QColor");
    qRegisterMetaType<SIGNAL_STATE>("SIGNAL_STATE");
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

    //------------------开启多线程 ------------------------
    QThread* gpuer_thread = new QThread;
    gpuer.moveToThread(gpuer_thread);
    gpuer_thread->start();
    QThread* cpuer_thread = new QThread;
    cpuer.moveToThread(cpuer_thread);
    cpuer_thread->start();
    QThread* bot_thread = new QThread;
    bot.moveToThread(bot_thread);
    bot_thread->start();
    QThread* tool_thread = new QThread;
    tool.moveToThread(tool_thread);
    tool_thread->start();
    QThread* net_thread = new QThread;
    net.moveToThread(net_thread);
    net_thread->start();

    //------------------连接bot和窗口-------------------
    QObject::connect(&bot, &xBot::bot2ui_params, &w, &Widget::recv_params);                          // bot将模型参数传递给ui
    QObject::connect(&bot, &xBot::bot2ui_output, &w, &Widget::reflash_output);                       //窗口输出区更新
    QObject::connect(&bot, &xBot::bot2ui_state, &w, &Widget::reflash_state);                         //窗口状态区更新
    QObject::connect(&bot, &xBot::bot2ui_play, &w, &Widget::recv_play);                              //播放加载动画
    QObject::connect(&bot, &xBot::bot2ui_loadover, &w, &Widget::recv_loadover);                      //完成加载模型
    QObject::connect(&bot, &xBot::bot2ui_predecoding, &w, &Widget::recv_predecoding);                // 正在预解码
    QObject::connect(&bot, &xBot::bot2ui_predecoding_over, &w, &Widget::recv_predecoding_over);      // 完成预解码
    QObject::connect(&bot, &xBot::bot2ui_pushover, &w, &Widget::recv_pushover);                      //完成推理
    QObject::connect(&bot, &xBot::bot2ui_resetover, &w, &Widget::recv_resetover);                    //完成重置,预解码约定
    QObject::connect(&bot, &xBot::bot2ui_stopover, &w, &Widget::recv_stopover);                      //完成停止
    QObject::connect(&bot, &xBot::bot2ui_arrivemaxctx, &w, &Widget::recv_arrivemaxctx);              //模型达到最大上下文
    QObject::connect(&bot, &xBot::bot2ui_reload, &w, &Widget::recv_reload);                          //设置参数改变,重载模型
    QObject::connect(&bot, &xBot::bot2ui_datereset, &w, &Widget::recv_datereset);                    // bot发信号请求ui触发reset
    QObject::connect(&bot, &xBot::bot2ui_setreset, &w, &Widget::recv_setreset);                      // bot发信号请求ui触发reset
    QObject::connect(&bot, &xBot::bot2ui_tokens, &w, &Widget::recv_tokens);                          //传递测试解码token数量
    QObject::connect(&bot, &xBot::bot2ui_predecode, &w, &Widget::recv_predecode);                    //传递模型预解码的内容
    QObject::connect(&bot, &xBot::bot2ui_freeover_loadlater, &w, &Widget::recv_freeover_loadlater);  //模型释放完毕并重新装载
    QObject::connect(&w, &Widget::ui2bot_stop, &bot, &xBot::recv_stop);                              //传递停止信号
    QObject::connect(&w, &Widget::ui2bot_loadmodel, &bot, &xBot::load);                              //开始加载模型
    QObject::connect(&w, &Widget::ui2bot_predict, &bot, &xBot::predict);                             //开始推理
    QObject::connect(&w, &Widget::ui2bot_preDecodeImage, &bot, &xBot::preDecodeImage);               //开始预解码图像
    QObject::connect(&w, &Widget::ui2bot_reset, &bot, &xBot::recv_reset);                            //传递重置信号
    QObject::connect(&w, &Widget::ui2bot_date, &bot, &xBot::recv_date);                              //传递约定内容
    QObject::connect(&w, &Widget::ui2bot_set, &bot, &xBot::recv_set);                                //传递设置内容
    QObject::connect(&w, &Widget::ui2bot_language, &bot, &xBot::recv_language);                      //传递使用的语言
    QObject::connect(&w, &Widget::ui2bot_free, &bot, &xBot::recv_free);                              //释放模型和上下文
    QObject::connect(&bot, &xBot::bot2ui_kv, &w, &Widget::recv_kv);                                  //传递缓存量
    QObject::connect(&bot, &xBot::bot2ui_chat_format, &w, &Widget::recv_chat_format);                //传递格式化后的对话内容
    QObject::connect(&w, &Widget::ui2bot_dateset, &bot, &xBot::recv_dateset);                        //自动装载

    //------------------监测gpu信息-------------------
    QObject::connect(&gpuer, &gpuChecker::gpu_status, &w, &Widget::recv_gpu_status);  //传递gpu信息
    QObject::connect(&gpuer, &gpuChecker::gpu_status, &bot, &xBot::recv_gpu_status);  //传递gpu信息
    QObject::connect(&w, &Widget::gpu_reflash, &gpuer, &gpuChecker::chekGpu);         //强制刷新gpu信息

    //------------------监测系统信息-------------------
    QObject::connect(&cpuer, &cpuChecker::cpu_status, &w, &Widget::recv_cpu_status);  //传递cpu信息
    QObject::connect(&w, &Widget::cpu_reflash, &cpuer, &cpuChecker::chekCpu);         //强制刷新cpu信息

    //------------------连接窗口和增殖窗口-------------------
    QObject::connect(&w, &Widget::ui2expend_syncrate, &expend, &Expend::recv_syncrate);                          //传递同步率结果
    QObject::connect(&w, &Widget::ui2expend_language, &expend, &Expend::recv_language);                          //传递使用的语言
    QObject::connect(&w, &Widget::ui2expend_show, &expend, &Expend::recv_expend_show);                           //通知显示扩展窗口
    QObject::connect(&w, &Widget::ui2expend_speechdecode, &expend, &Expend::recv_speechdecode);                  //开始语音转文字
    QObject::connect(&w, &Widget::ui2expend_resettts, &expend, &Expend::recv_resettts);                          //重置文字转语音
    QObject::connect(&expend, &Expend::expend2ui_speechdecode_over, &w, &Widget::recv_speechdecode_over);        //转换完成返回结果
    QObject::connect(&expend, &Expend::expend2ui_whisper_modelpath, &w, &Widget::recv_whisper_modelpath);        //传递模型路径
    QObject::connect(&expend, &Expend::expend2ui_state, &w, &Widget::reflash_state);                             //窗口状态区更新
    QObject::connect(&expend, &Expend::expend2ui_embeddingdb_describe, &w, &Widget::recv_embeddingdb_describe);  //传递知识库的描述

    //------------------连接bot和增殖窗口-------------------
    QObject::connect(&bot, &xBot::bot2expend_vocab, &expend, &Expend::recv_vocab);
    QObject::connect(&bot, &xBot::bot2expend_brainvector, &expend, &Expend::recv_brainvector);  //传递记忆向量和上下文长度
    QObject::connect(&bot, &xBot::bot_llama_log, &expend, &Expend::recv_llama_log);
    QObject::connect(&bot, &xBot::bot2ui_output, &expend, &Expend::recv_output);

    //------------------连接net和窗口-------------------
    QObject::connect(&net, &xNet::net2ui_output, &w, &Widget::reflash_output);   //窗口输出区更新
    QObject::connect(&net, &xNet::net2ui_state, &w, &Widget::reflash_state);     //窗口状态区更新
    QObject::connect(&net, &xNet::net2ui_pushover, &w, &Widget::recv_pushover);  //完成推理
    QObject::connect(&w, &Widget::ui2net_push, &net, &xNet::run);                //开始推理
    QObject::connect(&w, &Widget::ui2net_language, &net, &xNet::recv_language);  //传递使用的语言
    QObject::connect(&w, &Widget::ui2net_apis, &net, &xNet::recv_apis);          //传递api设置参数
    QObject::connect(&w, &Widget::ui2net_data, &net, &xNet::recv_data);          //传递端点参数
    QObject::connect(&w, &Widget::ui2net_stop, &net, &xNet::recv_stop);          //传递停止信号

    //------------------连接tool和窗口-------------------
    QObject::connect(&tool, &xTool::tool2ui_state, &w, &Widget::reflash_state);                   //窗口状态区更新
    QObject::connect(&tool, &xTool::tool2ui_controller, &w, &Widget::recv_controller);            //传递控制信息
    QObject::connect(&w, &Widget::recv_controller_over, &tool, &xTool::tool2ui_controller_over);  //传递控制完成结果
    QObject::connect(&tool, &xTool::tool2ui_pushover, &w, &Widget::recv_toolpushover);            //完成推理
    QObject::connect(&w, &Widget::ui2tool_language, &tool, &xTool::recv_language);                //传递使用的语言
    QObject::connect(&w, &Widget::ui2tool_exec, &tool, &xTool::Exec);                             //开始推理

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
            w.custom1_date_system = settings.value("custom1_date_system", "").toString();
            w.custom1_user_name = settings.value("custom1_user_name", "").toString();
            w.custom1_model_name = settings.value("custom1_model_name", "").toString();
            w.custom2_date_system = settings.value("custom2_date_system", "").toString();
            w.custom2_user_name = settings.value("custom2_user_name", "").toString();
            w.custom2_model_name = settings.value("custom2_model_name", "").toString();

            // ui显示值
            w.date_ui->chattemplate_comboBox->setCurrentText(settings.value("chattemplate", "").toString());
            w.date_ui->date_prompt_TextEdit->setPlainText(settings.value("date_prompt_prompt", "").toString());
            w.date_ui->user_name_LineEdit->setText(settings.value("user_name", "").toString());
            w.date_ui->model_name_LineEdit->setText(settings.value("model_name", "").toString());
            w.date_ui->calculator_checkbox->setChecked(settings.value("calculator_checkbox", "").toBool());
            w.date_ui->knowledge_checkbox->setChecked(settings.value("knowledge_checkbox", "").toBool());
            w.date_ui->controller_checkbox->setChecked(settings.value("controller_checkbox", "").toBool());
            w.date_ui->stablediffusion_checkbox->setChecked(settings.value("stablediffusion_checkbox", "").toBool());
            w.date_ui->engineer_checkbox->setChecked(settings.value("engineer_checkbox", "").toBool());
            w.date_ui->webengine_checkbox->setChecked(settings.value("webengine_checkbox", "").toBool());
            if (settings.value("extra_lan", "").toString() != "zh") {
                w.switch_lan_change();
            }  
            //切换为英文
            w.settings_ui->temp_slider->setValue(settings.value("temp", "").toFloat() * 100);
            w.settings_ui->repeat_slider->setValue(settings.value("repeat", "").toFloat() * 100);
            w.settings_ui->npredict_slider->setValue(settings.value("npredict", "").toFloat());
            w.settings_ui->nthread_slider->setValue(settings.value("nthread", "").toInt());
            w.settings_ui->nctx_slider->setValue(settings.value("nctx", "").toInt());
            w.settings_ui->ngl_slider->setValue(settings.value("ngl", "").toInt());
            QFile checkFile(settings.value("lorapath", "").toString());
            if (checkFile.exists()) {
                w.settings_ui->lora_LineEdit->setText(settings.value("lorapath", "").toString());
            }
            QFile checkFile2(settings.value("mmprojpath", "").toString());
            if (checkFile2.exists()) {
                w.settings_ui->mmproj_LineEdit->setText(settings.value("mmprojpath", "").toString());
            }
            int mode_num = settings.value("ui_state", "").toInt();
            if (mode_num == 0) {
                w.settings_ui->chat_btn->setChecked(1);
            } else if (mode_num == 1) {
                w.settings_ui->complete_btn->setChecked(1);
            } else if (mode_num == 2) {
                w.settings_ui->web_btn->setChecked(1);
            }
            w.settings_ui->port_lineEdit->setText(settings.value("port", "").toString());

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
    // //---------------如果没有配置文件但是有eva-models目录，则自动装载里面的模型------------------
    // else
    // {
    //     QString modelspath = applicationDirPath + "/eva-models/";
    //     QDir modelsdir(modelspath);
    //     if(modelsdir.exists())
    //     {
    //         w.currentpath = w.historypath = expend.currentpath = modelspath;

    //         //主模型处理
    //         {
    //             QString modelpath = modelspath + "llama-model/Qwen2.5-7B-Q3_K_M.gguf";  //模型路径
    //             QFile modelpath_file(modelpath);
    //             if (modelpath_file.exists())  //模型存在的话才继续进行
    //             {
    //                 w.ui_SETTINGS.modelpath = modelpath;// 设置好模型路径
    //                 emit w.ui2bot_free(1); // 释放已有模型后执行重载操作，会自动判断显存
    //             }
    //         }

    //         //whisper模型处理
    //         {
    //             QString modelpath = modelspath + "whisper-model/whisper-base-q5_1.bin";  //模型路径
    //             QFile modelpath_file(modelpath);
    //             if (modelpath_file.exists())  //模型存在的话才继续进行
    //             {
    //                 w.whisper_model_path = modelpath;
    //                 expend.setWhisperModelpath(modelpath);
    //             }
    //         }

    //         //sd模型处理
    //         {
    //             QString modelpath = modelspath + "sd-model/sd1.5-anything-3-q8_0.gguf";  //模型路径
    //             QFile modelpath_file(modelpath);
    //             if (modelpath_file.exists())  //模型存在的话才继续进行
    //             {
    //                 expend.setSdModelpath(modelpath);
    //             }
    //         }

    //     }

    // }

    //传递停止词和约定，因为第一次没有传递约定参数给bot
    bot.bot_date.extra_stop_words = w.ui_DATES.extra_stop_words;       // 同步
    bot.common_params_.prompt = w.ui_DATES.date_prompt.toStdString();  // 同步

    return a.exec();  //进入事件循环
}