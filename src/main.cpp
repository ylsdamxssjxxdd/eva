#include <QCoreApplication>
#include <QFileInfo>
#include <QFont>
#include <QProcessEnvironment>
#include <QStyleFactory>
#include <locale>
#include "cmakeconfig.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>

#include "expend/expend.h"
#include "utils/cpuchecker.h"
#include "utils/gpuchecker.h"
#include "widget/widget.h"
#include "xmcp.h"
#include "xnet.h"
#include "xtool.h"


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
    // ??linux?????????
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR"); // ???????
    QString ldLibraryPath = appDirPath + "/usr/lib";
    std::string currentPath = ldLibraryPath.toLocal8Bit().constData();
    setenv("LD_LIBRARY_PATH", currentPath.c_str(), 1); // ??????????? LD_LIBRARY_PATH
#ifdef BODY_USE_CUDA
    // cuda???????? /usr/local/cuda/lib64 ????
    std::string cudaPath = "/usr/local/cuda/lib64";
    setenv("LD_LIBRARY_PATH", (currentPath + ":" + cudaPath).c_str(), 1); // ??????????? LD_LIBRARY_PATH
#endif
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);                                       //?????
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough); //????????
    QApplication a(argc, argv);                                                                              //????
    a.setQuitOnLastWindowClosed(false);                                                                      //?????????????????????????
    // ??????????, ??????
    int fontId = QFontDatabase::addApplicationFont(":/simsun.ttc");
    if (fontId == -1)
    { //????????window?
        QFont font("SimSun");
        font.setStyleStrategy(QFont::PreferAntialias); //?????
        QApplication::setFont(font);
        // qDebug() << "Loaded font:" << "windows SimSun";
    }
    else
    {
        QStringList loadedFonts = QFontDatabase::applicationFontFamilies(fontId);
        if (!loadedFonts.empty())
        {
            QFont customFont(loadedFonts.at(0));
            customFont.setStyleStrategy(QFont::PreferAntialias); //?????
            QApplication::setFont(customFont);
            // qDebug() << "Loaded font:" << customFont.family();
        }
    }
    // ????EVA_TEMP????????
#if BODY_LINUX_PACK
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString appImagePath = env.value("APPIMAGE");
    const QFileInfo fileInfo(appImagePath);
    const QString applicationDirPath = fileInfo.absolutePath();                        // ??????????????EVA_TEMP???
    const QString appPath = fileInfo.absolutePath() + "/" + EVA_VERSION + ".AppImage"; // .AppImage????
#else
    const QString applicationDirPath = QCoreApplication::applicationDirPath(); // ????????EVA_TEMP???
    const QString appPath = applicationDirPath;
#endif
    //linux????????.desktop?~/.local/share/applications/???????~/Desktop?????????
    createDesktopShortcut(appPath);
    qDebug() << "EVA_PATH" << appPath;
    //------------------???????------------------
    Widget w(nullptr, applicationDirPath);      //????
    Expend expend(nullptr, applicationDirPath); //??????
    xTool tool(applicationDirPath);             //????
    // xBot removed: all inference goes through xNet to a llama.cpp server
    xNet net;                                   //????
    xMcp mcp;                                   //mcp????
    gpuChecker gpuer;                           //??????
    cpuChecker cpuer;                           //??????

    //-----------------?????-----------------------
    expend.wordsObj = net.wordsObj = tool.wordsObj = w.wordsObj; //????
    expend.max_thread = w.max_thread;
    tool.embedding_server_resultnumb = expend.embedding_resultnumb;          //????
    w.currentpath = w.historypath = expend.currentpath = applicationDirPath; // ??????
    w.whisper_model_path = QString::fromStdString(expend.whisper_params.model);

    //-----------------????-----------------------
    QFile file(":/QSS/eva.qss");
    file.open(QFile::ReadOnly);
    QString stylesheet = file.readAll();
    w.setStyleSheet(stylesheet);
    expend.setStyleSheet(stylesheet);

    //------------------????????-------------------
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
    //------------------????? ------------------------
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
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &net, [&net]() { net.recv_stop(true); }, Qt::QueuedConnection);
    QObject::connect(&a, &QCoreApplication::aboutToQuit, net_thread, &QThread::quit, Qt::QueuedConnection);
    QThread *mcp_thread = new QThread;
    mcp.moveToThread(mcp_thread);
    mcp_thread->start();

    //------------------??bot???-------------------
    // xBot connections removed

    //------------------??gpu??-------------------
    QObject::connect(&gpuer, &gpuChecker::gpu_status, &w, &Widget::recv_gpu_status); //??gpu??
    QObject::connect(&w, &Widget::gpu_reflash, &gpuer, &gpuChecker::checkGpu);       //????gpu??

    //------------------??????-------------------
    QObject::connect(&cpuer, &cpuChecker::cpu_status, &w, &Widget::recv_cpu_status); //??cpu??
    QObject::connect(&w, &Widget::cpu_reflash, &cpuer, &cpuChecker::chekCpu);        //????cpu??

    //------------------?????????-------------------                   //??????
    QObject::connect(&w, &Widget::ui2expend_language, &expend, &Expend::recv_language);                         //???????
    QObject::connect(&w, &Widget::ui2expend_show, &expend, &Expend::recv_expend_show);                          //????????
    QObject::connect(&w, &Widget::ui2expend_speechdecode, &expend, &Expend::recv_speechdecode);                 //???????
    QObject::connect(&w, &Widget::ui2expend_resettts, &expend, &Expend::recv_resettts);                         //???????
    QObject::connect(&expend, &Expend::expend2ui_speechdecode_over, &w, &Widget::recv_speechdecode_over);       //????????
    QObject::connect(&expend, &Expend::expend2ui_whisper_modelpath, &w, &Widget::recv_whisper_modelpath);       //??????
    QObject::connect(&expend, &Expend::expend2ui_state, &w, &Widget::reflash_state);                            //???????
    QObject::connect(&expend, &Expend::expend2ui_embeddingdb_describe, &w, &Widget::recv_embeddingdb_describe); //????????
    QObject::connect(&w, &Widget::ui2expend_llamalog, &expend, &Expend::recv_llama_log);                        // ?????? -> ????

    // xBot -> expend connections removed

    //------------------??net???-------------------
    QObject::connect(&net, &xNet::net2ui_output, &w, &Widget::reflash_output, Qt::QueuedConnection);  //???????
    QObject::connect(&net, &xNet::net2ui_state, &w, &Widget::reflash_state, Qt::QueuedConnection);    //???????
    QObject::connect(&net, &xNet::net2ui_pushover, &w, &Widget::recv_pushover, Qt::QueuedConnection); //????
    QObject::connect(&net, &xNet::net2ui_kv_tokens, &w, &Widget::recv_kv_from_net, Qt::QueuedConnection); // kv used tokens
    QObject::connect(&net, &xNet::net2ui_slot_id, &w, &Widget::onSlotAssigned, Qt::QueuedConnection); // capture server slot id
    QObject::connect(&net, &xNet::net2ui_reasoning_tokens, &w, &Widget::recv_reasoning_tokens, Qt::QueuedConnection); // think tokens for this turn
    QObject::connect(&w, &Widget::ui2net_push, &net, &xNet::run, Qt::QueuedConnection);               //????
    QObject::connect(&w, &Widget::ui2net_language, &net, &xNet::recv_language, Qt::QueuedConnection); //???????
    QObject::connect(&w, &Widget::ui2net_apis, &net, &xNet::recv_apis, Qt::QueuedConnection);         //??api????
    QObject::connect(&w, &Widget::ui2net_data, &net, &xNet::recv_data, Qt::QueuedConnection);         //??????
    QObject::connect(&w, &Widget::ui2net_stop, &net, &xNet::recv_stop, Qt::QueuedConnection);         //??????

    //------------------??tool???-------------------
    QObject::connect(&tool, &xTool::tool2ui_state, &w, &Widget::reflash_state);                  //???????
    QObject::connect(&tool, &xTool::tool2ui_controller, &w, &Widget::recv_controller);           //??????
    QObject::connect(&w, &Widget::recv_controller_over, &tool, &xTool::tool2ui_controller_over); //????????
    QObject::connect(&tool, &xTool::tool2ui_pushover, &w, &Widget::recv_toolpushover);           //????
    QObject::connect(&w, &Widget::ui2tool_language, &tool, &xTool::recv_language);               //???????
    QObject::connect(&w, &Widget::ui2tool_exec, &tool, &xTool::Exec);                            //????

    //------------------???????tool-------------------
    QObject::connect(&expend, &Expend::expend2tool_embeddingdb, &tool, &xTool::recv_embeddingdb);                 //??????????
    QObject::connect(&expend, &Expend::expend2ui_embedding_resultnumb, &tool, &xTool::recv_embedding_resultnumb); //??????????

    QObject::connect(&tool, &xTool::tool2expend_draw, &expend, &Expend::recv_draw);         //??????
    QObject::connect(&expend, &Expend::expend2tool_drawover, &tool, &xTool::recv_drawover); //??????

    //------------------??mcp???-------------------
    QObject::connect(&mcp, &xMcp::mcp_message, &expend, &Expend::recv_mcp_message);
    QObject::connect(&expend, &Expend::expend2mcp_addService, &mcp, &xMcp::addService);
    QObject::connect(&mcp, &xMcp::addService_single_over, &expend, &Expend::recv_addService_single_over); // ????mcp????
    QObject::connect(&mcp, &xMcp::addService_over, &expend, &Expend::recv_addService_over);
    QObject::connect(&tool, &xTool::tool2mcp_toollist, &mcp, &xMcp::callList);       //??mcp????
    QObject::connect(&mcp, &xMcp::callList_over, &tool, &xTool::recv_calllist_over); //??mcp??????
    QObject::connect(&tool, &xTool::tool2mcp_toolcall, &mcp, &xMcp::callTool);       //????mcp????
    QObject::connect(&mcp, &xMcp::callTool_over, &tool, &xTool::recv_callTool_over); //mcp????????

    w.show(); //????

    //---------------?????????------------------
    emit w.gpu_reflash(); //????gpu????????????????
    QFile configfile(applicationDirPath + "/EVA_TEMP/eva_config.ini");

    // ?????????
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    w.shell = tool.shell = expend.shell = settings.value("shell", DEFAULT_SHELL).toString();                                    // ???????????shell??
    w.pythonExecutable = tool.pythonExecutable = expend.pythonExecutable = settings.value("python", DEFAULT_PYTHON).toString(); // ???????????python??
    QString modelpath = settings.value("modelpath", applicationDirPath + DEFAULT_LLM_MODEL_PATH).toString();                    //????
    w.currentpath = w.historypath = expend.currentpath = modelpath;
    w.ui_SETTINGS.modelpath = modelpath;
    w.ui_mode = static_cast<EVA_MODE>(settings.value("ui_mode", "0").toInt()); //
    w.ui_monitor_frame = settings.value("monitor_frame", DEFAULT_MONITOR_FRAME).toDouble();
    w.api_endpoint_LineEdit->setText(settings.value("api_endpoint", "").toString());
    w.api_key_LineEdit->setText(settings.value("api_key", "").toString());
    w.api_model_LineEdit->setText(settings.value("api_model", "default").toString());
    w.apis.api_endpoint = w.api_endpoint_LineEdit->text();
    w.apis.api_key = w.api_key_LineEdit->text();
    w.apis.api_model = w.api_model_LineEdit->text();
    w.custom1_date_system = settings.value("custom1_date_system", "").toString();
    w.custom1_user_name = settings.value("custom1_user_name", "").toString();
    w.custom1_model_name = settings.value("custom1_model_name", "").toString();
    w.custom2_date_system = settings.value("custom2_date_system", "").toString();
    w.custom2_user_name = settings.value("custom2_user_name", "").toString();
    w.custom2_model_name = settings.value("custom2_model_name", "").toString();
    w.date_ui->chattemplate_comboBox->setCurrentText(settings.value("chattemplate", "default").toString());
    w.date_ui->calculator_checkbox->setChecked(settings.value("calculator_checkbox", 0).toBool());
    w.date_ui->knowledge_checkbox->setChecked(settings.value("knowledge_checkbox", 0).toBool());
    w.date_ui->controller_checkbox->setChecked(settings.value("controller_checkbox", 0).toBool());
    w.date_ui->stablediffusion_checkbox->setChecked(settings.value("stablediffusion_checkbox", 0).toBool());
    w.date_ui->engineer_checkbox->setChecked(settings.value("engineer_checkbox", 0).toBool());
    w.date_ui->MCPtools_checkbox->setChecked(settings.value("MCPtools_checkbox", 0).toBool());
    if (settings.value("extra_lan", "zh").toString() != "zh") { w.switch_lan_change(); }
    w.settings_ui->repeat_slider->setValue(settings.value("repeat", DEFAULT_REPEAT).toFloat() * 100);
    w.settings_ui->nthread_slider->setValue(settings.value("nthread", w.ui_SETTINGS.nthread).toInt());
    w.settings_ui->nctx_slider->setValue(settings.value("nctx", DEFAULT_NCTX).toInt());
    w.settings_ui->ngl_slider->setValue(settings.value("ngl", DEFAULT_NGL).toInt());
    w.settings_ui->temp_slider->setValue(settings.value("temp", DEFAULT_TEMP).toFloat() * 100);
    w.settings_ui->port_lineEdit->setText(settings.value("port", DEFAULT_SERVER_PORT).toString());
    w.settings_ui->frame_lineEdit->setText(settings.value("monitor_frame", DEFAULT_MONITOR_FRAME).toString());
    bool embedding_server_need = settings.value("embedding_server_need", 0).toBool(); //??????????
    QString embedding_modelpath = settings.value("embedding_modelpath", "").toString();
    QFile checkFile(settings.value("lorapath", "").toString());
    if (checkFile.exists()) { w.settings_ui->lora_LineEdit->setText(settings.value("lorapath", "").toString()); }
    QFile checkFile2(settings.value("mmprojpath", "").toString());
    if (checkFile2.exists()) { w.settings_ui->mmproj_LineEdit->setText(settings.value("mmprojpath", "").toString()); }
    int mode_num = settings.value("ui_state", 0).toInt();
    if (mode_num == 0) { w.settings_ui->chat_btn->setChecked(1); }
    else if (mode_num == 1) { w.settings_ui->complete_btn->setChecked(1); }

    //??????????????
    w.ui_SETTINGS.hid_npredict = settings.value("hid_npredict", DEFAULT_NPREDICT).toInt();
    w.ui_SETTINGS.hid_special = settings.value("hid_special", DEFAULT_SPECIAL).toBool();
    w.ui_SETTINGS.hid_top_p = settings.value("hid_top_p", DEFAULT_TOP_P).toFloat();
    w.ui_SETTINGS.hid_batch = settings.value("hid_batch", DEFAULT_BATCH).toInt();
    w.ui_SETTINGS.hid_n_ubatch = settings.value("hid_n_ubatch", DEFAULT_UBATCH).toInt();
    w.ui_SETTINGS.hid_use_mmap = settings.value("hid_use_mmap", DEFAULT_USE_MMAP).toBool();
    w.ui_SETTINGS.hid_use_mlock = settings.value("hid_use_mlock", DEFAULT_USE_MLOCCK).toBool();
    w.ui_SETTINGS.hid_flash_attn = settings.value("hid_flash_attn", DEFAULT_FLASH_ATTN).toBool();
    w.ui_SETTINGS.hid_parallel = settings.value("hid_parallel", DEFAULT_PARALLEL).toInt();

    // ui?????ui???
    w.get_date(); //?????????
    w.get_set();  //?????????
    w.is_config = true;

    //?????????????????????????????
    QFile modelpath_file(modelpath);
    if (w.ui_mode == LOCAL_MODE && modelpath_file.exists())
    {
        w.ensureLocalServer();
    }
    else if (w.ui_mode == LINK_MODE)
    {
        w.set_api();
    }

    //???????????, ????expend????????
    if (embedding_server_need)
    {
        QFile embedding_modelpath_file(embedding_modelpath);
        if (embedding_modelpath_file.exists())
        {
            expend.embedding_embed_need = true;
            expend.embedding_params.modelpath = embedding_modelpath;
            expend.embedding_server_start(); //??????
        }
        else //????????
        {
            expend.embedding_processing(); //????
        }
    }

    // ?????????????????????????
    return a.exec();                                                  //??????
}

