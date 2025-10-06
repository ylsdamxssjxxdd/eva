#include "cmakeconfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStyleFactory>
#include <locale>

#include "expend/expend.h"
#include "utils/cpuchecker.h"
#include "utils/gpuchecker.h"
#include "widget/widget.h"
#include "xmcp.h"
#include "xnet.h"
#include "xtool.h"
#include "utils/devicemanager.h"

static inline void createDesktopShortcut(QString appPath)
{
#ifdef Q_OS_LINUX
    // prepare icon path and copy resource icon to user dir
    QString iconDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/";
    QDir().mkpath(iconDir);
    QString iconPath = iconDir + "eva.png";
    QFile::copy(":/logo/blue_logo.png", iconPath);
    QFile::setPermissions(iconPath, QFile::ReadOwner | QFile::WriteOwner);

    // compose .desktop content
    QString desktopContent = QString(
                                 "[Desktop Entry]\n"
                                 "Type=Application\n"
                                 "Name=%1\n"
                                 "Comment=a lite llm tool\n"
                                 "Exec=%2\n"
                                 "Icon=%3\n"
                                 "Terminal=false\n"
                                 "Categories=Utility\n")
                                 .arg(EVA_VERSION, appPath, iconPath);

    // write to ~/.local/share/applications/eva.desktop
    QString applicationsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/";
    QDir().mkpath(applicationsDir);
    QFile applicationsFile(applicationsDir + "eva.desktop");
    if (applicationsFile.open(QIODevice::WriteOnly))
    {
        applicationsFile.write(desktopContent.toUtf8());
        applicationsFile.close();
        applicationsFile.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
    }
    else
    {
        qWarning() << "Failed to write applications desktop file";
    }

    // write to ~/Desktop/eva.desktop
    QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/";
    QFile desktopFile(desktopDir + "eva.desktop");
    if (desktopFile.open(QIODevice::WriteOnly))
    {
        desktopFile.write(desktopContent.toUtf8());
        desktopFile.close();
        desktopFile.setPermissions(QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
    }
    else
    {
        qWarning() << "Failed to write desktop shortcut";
    }
#endif
}

int main(int argc, char *argv[])
{
    // 设置linux下动态库的默认路径
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR"); // 获取镜像的路径
    QString ldLibraryPath = appDirPath + "/usr/lib";
    std::string currentPath = ldLibraryPath.toLocal8Bit().constData();
    setenv("LD_LIBRARY_PATH", currentPath.c_str(), 1); // 指定找动态库的默认路径 LD_LIBRARY_PATH
#ifdef BODY_USE_CUDA
    // cuda版本可以在系统的 /usr/local/cuda/lib64 中寻找库
    std::string cudaPath = "/usr/local/cuda/lib64";
    setenv("LD_LIBRARY_PATH", (currentPath + ":" + cudaPath).c_str(), 1); // 指定找动态库的默认路径 LD_LIBRARY_PATH
#endif
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);                                       //自适应缩放
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough); //适配非整数倍缩放
    QApplication a(argc, argv);    // Redirect Qt logs to file for crash diagnosis
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg){
        const QString dir = QCoreApplication::applicationDirPath() + "/EVA_TEMP";
        QDir().mkpath(dir);
        static QFile f(dir + "/eva_debug.log");
        static bool opened = false;
        if (!opened) { f.open(QIODevice::Append | QIODevice::Text); opened = true; }
        if (f.isOpen()) {
            QTextStream ts(&f);
            ts << msg << "\n";
            ts.flush();
        }
    });                                                                              //事件实例
    a.setQuitOnLastWindowClosed(false);                                                                      //即使关闭所有窗口也不退出程序，为了保持系统托盘正常
    // 加载资源文件中的字体, 统一使用宋体
    int fontId = QFontDatabase::addApplicationFont(":/simsun.ttc");
    if (fontId == -1)
    { //如果没有说明是在window下
        QFont font("SimSun");
        font.setStyleStrategy(QFont::PreferAntialias); //应用反锯齿
        QApplication::setFont(font);
        // qDebug() << "Loaded font:" << "windows SimSun";
    }
    else
    {
        QStringList loadedFonts = QFontDatabase::applicationFontFamilies(fontId);
        if (!loadedFonts.empty())
        {
            QFont customFont(loadedFonts.at(0));
            customFont.setStyleStrategy(QFont::PreferAntialias); //应用反锯齿
            QApplication::setFont(customFont);
            // qDebug() << "Loaded font:" << customFont.family();
        }
    }
    // 设置创建EVA_TEMP文件夹所在的目录
#if BODY_LINUX_PACK
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString appImagePath = env.value("APPIMAGE");
    const QFileInfo fileInfo(appImagePath);
    const QString applicationDirPath = fileInfo.absolutePath();                        // 在打包程序运行时所在目录创建EVA_TEMP文件夹
    const QString appPath = fileInfo.absolutePath() + "/" + EVA_VERSION + ".AppImage"; // .AppImage所在路径
#else
    const QString applicationDirPath = QCoreApplication::applicationDirPath(); // 就在当前目录创建EVA_TEMP文件夹
    const QString appPath = applicationDirPath;
#endif
    //linux下每次启动都创建.desktop到~/.local/share/applications/（开始菜单）和~/Desktop（桌面快捷方式）中
    createDesktopShortcut(appPath);
    qDebug() << "EVA_PATH" << appPath;
    //------------------实例化主要节点------------------
    Widget w(nullptr, applicationDirPath);      //窗口实例\n    // Prepare sub-dialog UIs before accessing their widgets from main\n    w.set_DateDialog();\n    w.set_SetDialog();\n    w.setApiDialog();
    Expend expend(nullptr, applicationDirPath); //增殖窗口实例
    xTool tool(applicationDirPath);             //工具实例
    // xBot removed: all inference goes through xNet to a llama.cpp server
    xNet net;         //链接实例
    xMcp mcp;         //mcp管理实例
    gpuChecker gpuer; //监测显卡信息
    cpuChecker cpuer; //监视系统信息

    //-----------------初始值设定-----------------------
    expend.wordsObj = net.wordsObj = tool.wordsObj = w.wordsObj; //传递语言
    expend.max_thread = w.max_thread;
    tool.embedding_server_resultnumb = expend.embedding_resultnumb;          //同步数目
    w.currentpath = w.historypath = expend.currentpath = applicationDirPath; // 默认打开路径
    w.whisper_model_path = QString::fromStdString(expend.whisper_params.model);

    //-----------------加载皮肤-----------------------
    QFile file(":/QSS/eva.qss");
    file.open(QFile::ReadOnly);
    QString stylesheet = file.readAll();
    w.setStyleSheet(stylesheet);
    expend.setStyleSheet(stylesheet);

    //------------------注册信号传递变量-------------------
    qRegisterMetaType<EVA_CHATS_TEMPLATE>("EVA_CHATS_TEMPLATE");
    qRegisterMetaType<MODEL_PARAMS>("MODEL_PARAMS");
    qRegisterMetaType<QColor>("QColor");
    qRegisterMetaType<SIGNAL_STATE>("SIGNAL_STATE");
    qRegisterMetaType<EVA_DATES>("EVA_DATES");
    qRegisterMetaType<SETTINGS>("SETTINGS");
    qRegisterMetaType<EVA_INPUTS>("EVA_INPUTS");
    qRegisterMetaType<QVector<Embedding_vector>>("QVector<Embedding_vector>");
    qRegisterMetaType<Speech_Params>("Speech_Params");
    qRegisterMetaType<QPair<QString, QString>>("QPair<QString, QString>");
    qRegisterMetaType<mcp::json>("mcp::json");
    qRegisterMetaType<std::vector<Brain_Cell>>("std::vector<Brain_Cell>");
    qRegisterMetaType<ENDPOINT_DATA>("ENDPOINT_DATA");
    qRegisterMetaType<APIS>("APIS");
    qRegisterMetaType<EXPEND_WINDOW>("EXPEND_WINDOW");
    qRegisterMetaType<MCP_CONNECT_STATE>("MCP_CONNECT_STATE");
    //------------------开启多线程 ------------------------
    QThread *gpuer_thread = new QThread;
    gpuer.moveToThread(gpuer_thread);
    gpuer_thread->start();
    QThread *cpuer_thread = new QThread;
    cpuer.moveToThread(cpuer_thread);
    cpuer_thread->start();
    QThread *tool_thread = new QThread;
    tool.moveToThread(tool_thread);
    tool_thread->start();
    QThread *net_thread = new QThread;
    net.moveToThread(net_thread);
    net_thread->start();
    QObject::connect(
        &a, &QCoreApplication::aboutToQuit, &net, [&net]() { net.recv_stop(true); }, Qt::QueuedConnection);
    QObject::connect(&a, &QCoreApplication::aboutToQuit, net_thread, &QThread::quit, Qt::QueuedConnection);
    QThread *mcp_thread = new QThread;
    mcp.moveToThread(mcp_thread);
    mcp_thread->start();

    //------------------bot相关（已移除）-------------------
    // xBot connections removed

    //------------------监测gpu信息-------------------
    QObject::connect(&gpuer, &gpuChecker::gpu_status, &w, &Widget::recv_gpu_status); //传递gpu信息
    QObject::connect(&w, &Widget::gpu_reflash, &gpuer, &gpuChecker::checkGpu);       //强制刷新gpu信息

    //------------------监测系统信息-------------------
    QObject::connect(&cpuer, &cpuChecker::cpu_status, &w, &Widget::recv_cpu_status); //传递cpu信息
    QObject::connect(&w, &Widget::cpu_reflash, &cpuer, &cpuChecker::chekCpu);        //强制刷新cpu信息

    //------------------连接窗口和增殖窗口-------------------
    QObject::connect(&w, &Widget::ui2expend_language, &expend, &Expend::recv_language);                         //传递使用的语言
    QObject::connect(&w, &Widget::ui2expend_show, &expend, &Expend::recv_expend_show);                          //通知显示扩展窗口
    QObject::connect(&w, &Widget::ui2expend_speechdecode, &expend, &Expend::recv_speechdecode);                 //开始语音转文字
    QObject::connect(&w, &Widget::ui2expend_resettts, &expend, &Expend::recv_resettts);                         //重置文字转语音
    QObject::connect(&expend, &Expend::expend2ui_speechdecode_over, &w, &Widget::recv_speechdecode_over);       //转换完成返回结果
    QObject::connect(&expend, &Expend::expend2ui_whisper_modelpath, &w, &Widget::recv_whisper_modelpath);       //传递模型路径
    QObject::connect(&expend, &Expend::expend2ui_state, &w, &Widget::reflash_state);                            //窗口状态区更新
    QObject::connect(&expend, &Expend::expend2ui_embeddingdb_describe, &w, &Widget::recv_embeddingdb_describe); //传递知识库的描述
    QObject::connect(&w, &Widget::ui2expend_llamalog, &expend, &Expend::recv_llama_log);                        //传递llama日志

    // xBot -> expend connections removed

    //------------------连接net和窗口-------------------
    QObject::connect(&net, &xNet::net2ui_output, &w, &Widget::reflash_output, Qt::QueuedConnection);                  //窗口输出区更新
    QObject::connect(&net, &xNet::net2ui_state, &w, &Widget::reflash_state, Qt::QueuedConnection);                    //窗口状态区更新
    QObject::connect(&net, &xNet::net2ui_pushover, &w, &Widget::recv_pushover, Qt::QueuedConnection);                 //完成推理
    QObject::connect(&net, &xNet::net2ui_kv_tokens, &w, &Widget::recv_kv_from_net, Qt::QueuedConnection);             // kv used tokens
    QObject::connect(&net, &xNet::net2ui_slot_id, &w, &Widget::onSlotAssigned, Qt::QueuedConnection);                 // capture server slot id
    QObject::connect(&net, &xNet::net2ui_reasoning_tokens, &w, &Widget::recv_reasoning_tokens, Qt::QueuedConnection); // think tokens for this turn
    QObject::connect(&w, &Widget::ui2net_push, &net, &xNet::run, Qt::QueuedConnection);                               //开始推理
    QObject::connect(&w, &Widget::ui2net_language, &net, &xNet::recv_language, Qt::QueuedConnection);                 //传递使用的语言
    QObject::connect(&w, &Widget::ui2net_apis, &net, &xNet::recv_apis, Qt::QueuedConnection);                         //传递api设置参数
    QObject::connect(&w, &Widget::ui2net_data, &net, &xNet::recv_data, Qt::QueuedConnection);                         //传递端点参数
    QObject::connect(&w, &Widget::ui2net_stop, &net, &xNet::recv_stop, Qt::QueuedConnection);                         //传递停止信号

    //------------------连接tool和窗口-------------------
    QObject::connect(&tool, &xTool::tool2ui_state, &w, &Widget::reflash_state);                  //窗口状态区更新
    QObject::connect(&tool, &xTool::tool2ui_controller, &w, &Widget::recv_controller);           //传递控制信息
    QObject::connect(&w, &Widget::recv_controller_over, &tool, &xTool::tool2ui_controller_over); //传递控制完成结果
    QObject::connect(&tool, &xTool::tool2ui_pushover, &w, &Widget::recv_toolpushover);           //完成推理
    QObject::connect(&w, &Widget::ui2tool_language, &tool, &xTool::recv_language);               //传递使用的语言
    QObject::connect(&w, &Widget::ui2tool_exec, &tool, &xTool::Exec);                            //开始推理

    //------------------连接增殖窗口和tool-------------------
    QObject::connect(&expend, &Expend::expend2tool_embeddingdb, &tool, &xTool::recv_embeddingdb);                 //传递已嵌入文本段数据
    QObject::connect(&expend, &Expend::expend2ui_embedding_resultnumb, &tool, &xTool::recv_embedding_resultnumb); //传递嵌入结果返回个数

    QObject::connect(&tool, &xTool::tool2expend_draw, &expend, &Expend::recv_draw);         //开始绘制图像
    QObject::connect(&expend, &Expend::expend2tool_drawover, &tool, &xTool::recv_drawover); //图像绘制完成

    //------------------连接mcp管理器-------------------
    QObject::connect(&mcp, &xMcp::mcp_message, &expend, &Expend::recv_mcp_message);
    QObject::connect(&expend, &Expend::expend2mcp_addService, &mcp, &xMcp::addService);
    QObject::connect(&mcp, &xMcp::addService_single_over, &expend, &Expend::recv_addService_single_over); // 添加某个mcp服务完成
    QObject::connect(&mcp, &xMcp::addService_over, &expend, &Expend::recv_addService_over);
    QObject::connect(&tool, &xTool::tool2mcp_toollist, &mcp, &xMcp::callList);       //查询mcp可用工具
    QObject::connect(&mcp, &xMcp::callList_over, &tool, &xTool::recv_calllist_over); //查询mcp可用工具完成
    QObject::connect(&tool, &xTool::tool2mcp_toolcall, &mcp, &xMcp::callTool);       //开始调用mcp可用工具
    QObject::connect(&mcp, &xMcp::callTool_over, &tool, &xTool::recv_callTool_over); //mcp可用工具调用完成

    qDebug() << "before show";
    w.show(); //展示窗口
    qDebug() << "after show";

    //---------------读取配置文件并执行------------------
    qDebug() << "before settings";
    // 读取配置文件中的值
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    qDebug() << "s0";
    settings.setIniCodec("utf-8");
    qDebug() << "s1";
    w.shell = tool.shell = expend.shell = settings.value("shell", DEFAULT_SHELL).toString();
    qDebug() << "s2"; // shell
    w.pythonExecutable = tool.pythonExecutable = expend.pythonExecutable = settings.value("python", DEFAULT_PYTHON).toString();
    qDebug() << "s3"; // python
    QString modelpath = settings.value("modelpath", applicationDirPath + DEFAULT_LLM_MODEL_PATH).toString();
    qDebug() << "s4" << modelpath; // modelpath
    w.currentpath = w.historypath = expend.currentpath = modelpath; qDebug() << "sa";
    w.ui_SETTINGS.modelpath = modelpath; qDebug() << "sb";
    w.ui_mode = static_cast<EVA_MODE>(settings.value("ui_mode", "0").toInt()); qDebug() << "sc";
    w.ui_monitor_frame = settings.value("monitor_frame", DEFAULT_MONITOR_FRAME).toDouble(); qDebug() << "sd";
    // set api fields
    /* debug removed */
    if (w.api_endpoint_LineEdit) w.api_endpoint_LineEdit->setText(settings.value("api_endpoint", "").toString()); qDebug() << "se";
    if (w.api_key_LineEdit) w.api_key_LineEdit->setText(settings.value("api_key", "").toString()); qDebug() << "sf";
    if (w.api_model_LineEdit) w.api_model_LineEdit->setText(settings.value("api_model", "default").toString()); qDebug() << "sg";
    w.apis.api_endpoint = w.api_endpoint_LineEdit->text();
    w.apis.api_key = w.api_key_LineEdit->text();
    w.apis.api_model = w.api_model_LineEdit->text();
    qDebug() << "s5"; // api fields ok
    w.date_ui->calculator_checkbox->setChecked(settings.value("calculator_checkbox", 0).toBool());
    w.date_ui->knowledge_checkbox->setChecked(settings.value("knowledge_checkbox", 0).toBool());
    w.date_ui->controller_checkbox->setChecked(settings.value("controller_checkbox", 0).toBool());
    w.date_ui->stablediffusion_checkbox->setChecked(settings.value("stablediffusion_checkbox", 0).toBool());
    w.date_ui->engineer_checkbox->setChecked(settings.value("engineer_checkbox", 0).toBool());
    w.date_ui->MCPtools_checkbox->setChecked(settings.value("MCPtools_checkbox", 0).toBool());
    if (settings.value("extra_lan", "zh").toString() != "zh") { w.switch_lan_change(); }
    // 推理设备：先根据目录填充选项，再应用用户偏好
    const QString savedDevice = settings.value("device_backend", "auto").toString();
    DeviceManager::setUserChoice(savedDevice);
    int devIdx = w.settings_ui->device_comboBox->findText(DeviceManager::userChoice());
    if (devIdx >= 0) w.settings_ui->device_comboBox->setCurrentIndex(devIdx);

    w.settings_ui->repeat_slider->setValue(settings.value("repeat", DEFAULT_REPEAT).toFloat() * 100);
    w.settings_ui->nthread_slider->setValue(settings.value("nthread", w.ui_SETTINGS.nthread).toInt());
    w.settings_ui->nctx_slider->setValue(settings.value("nctx", DEFAULT_NCTX).toInt());
    w.settings_ui->ngl_slider->setValue(settings.value("ngl", DEFAULT_NGL).toInt());
    w.settings_ui->temp_slider->setValue(settings.value("temp", DEFAULT_TEMP).toFloat() * 100);
    w.settings_ui->topk_slider->setValue(settings.value("top_k", DEFAULT_TOP_K).toInt());
    w.settings_ui->parallel_slider->setValue(settings.value("hid_parallel", DEFAULT_PARALLEL).toInt());
    w.settings_ui->port_lineEdit->setText(settings.value("port", DEFAULT_SERVER_PORT).toString());
    w.settings_ui->frame_lineEdit->setText(settings.value("monitor_frame", DEFAULT_MONITOR_FRAME).toString());
    bool embedding_server_need = settings.value("embedding_server_need", 0).toBool(); //默认不主动嵌入词向量
    QString embedding_modelpath = settings.value("embedding_modelpath", "").toString();
    QFile checkFile(settings.value("lorapath", "").toString());
    if (checkFile.exists()) { w.settings_ui->lora_LineEdit->setText(settings.value("lorapath", "").toString()); }
    QFile checkFile2(settings.value("mmprojpath", "").toString());
    if (checkFile2.exists()) { w.settings_ui->mmproj_LineEdit->setText(settings.value("mmprojpath", "").toString()); }
    int mode_num = settings.value("ui_state", 0).toInt();
    if (mode_num == 0) { w.settings_ui->chat_btn->setChecked(1); }
    else if (mode_num == 1)
    {
        w.settings_ui->complete_btn->setChecked(1);
    }

    // 初次启动强制赋予隐藏的设定值
    w.ui_SETTINGS.hid_npredict = settings.value("hid_npredict", DEFAULT_NPREDICT).toInt();
    w.ui_SETTINGS.hid_special = settings.value("hid_special", DEFAULT_SPECIAL).toBool();
    w.ui_SETTINGS.hid_top_p = settings.value("hid_top_p", DEFAULT_TOP_P).toFloat();
    w.ui_SETTINGS.hid_batch = settings.value("hid_batch", DEFAULT_BATCH).toInt();
    w.ui_SETTINGS.hid_n_ubatch = settings.value("hid_n_ubatch", DEFAULT_UBATCH).toInt();
    w.ui_SETTINGS.hid_use_mmap = settings.value("hid_use_mmap", DEFAULT_USE_MMAP).toBool();
    w.ui_SETTINGS.hid_use_mlock = settings.value("hid_use_mlock", DEFAULT_USE_MLOCCK).toBool();
    w.ui_SETTINGS.hid_flash_attn = settings.value("hid_flash_attn", DEFAULT_FLASH_ATTN).toBool();
    w.ui_SETTINGS.hid_parallel = settings.value("hid_parallel", DEFAULT_PARALLEL).toInt();

    // ui显示值传给ui内部值
    w.get_date(); //获取约定中的纸面值
    w.get_set();  //获取设置中的纸面值
    w.is_config = true;

    // 初次启动强制赋予隐藏的设定值
    // 处理模型装载相关
    QFile modelpath_file(modelpath);
    if (w.ui_mode == LOCAL_MODE && modelpath_file.exists())
    {
        w.ensureLocalServer();
    }
    else if (w.ui_mode == LINK_MODE)
    {
        w.set_api();
    }

    // 是否需要自动重构知识库, 源文档在expend实例化时已经完成
    if (embedding_server_need)
    {
        QFile embedding_modelpath_file(embedding_modelpath);
        if (embedding_modelpath_file.exists())
        {
            expend.embedding_embed_need = true;
            expend.embedding_params.modelpath = embedding_modelpath;
            expend.embedding_server_start(); //启动嵌入服务
        }
        else //借助端点直接嵌入
        {
            expend.embedding_processing(); //执行嵌入
        }
    }

    return a.exec(); //进入事件循环
}
