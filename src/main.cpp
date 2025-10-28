#include "cmakeconfig.h"
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QCheckBox>
#include <QLocale> // C-locale parsing for numeric strings
#include <QProcessEnvironment>
#include <QSignalBlocker>
#include <QStringList>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QtGlobal> // qRound/qBound
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QtPlugin>
#include <climits>
#include <functional>
#include <locale>

#if defined(Q_OS_LINUX) && defined(EVA_LINUX_STATIC_BUILD)
Q_IMPORT_PLUGIN(QFcitxPlatformInputContextPlugin)
#endif

#include "expend/expend.h"
#include "utils/cpuchecker.h"
#include "utils/devicemanager.h"
#include "utils/gpuchecker.h"
#include "utils/startuplogger.h"
#include "utils/singleinstance.h" // single-instance guard (per app path)
#include "widget/widget.h"
#include "xmcp.h"
#include "xnet.h"
#include "xtool.h"

static inline void createDesktopShortcut(QString appPath)
{
#ifdef Q_OS_LINUX
    // Prepare icon path and copy resource icon to user dir
    const QString iconDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/icons/";
    QDir().mkpath(iconDir);
    const QString iconPath = iconDir + "eva.png";
    QFile::copy(":/logo/blue_logo.png", iconPath);
    QFile::setPermissions(iconPath, QFile::ReadOwner | QFile::WriteOwner);

    // Compose .desktop content using Qt placeholders (%1, %2, %3)
    const QString desktopContent = QStringLiteral(
                                       "[Desktop Entry]\n"
                                       "Type=Application\n"
                                       "Name=%1\n"
                                       "Comment=a lite llm tool\n"
                                       "Exec=\"%2\"\n"
                                       "Icon=%3\n"
                                       "Terminal=false\n"
                                       "Categories=Utility;\n")
                                       .arg(QStringLiteral("eva"), appPath, iconPath);

    // Write to ~/.local/share/applications/eva.desktop
    const QString applicationsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/";
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

    // Write to ~/Desktop/eva.desktop
    const QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/";
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
    StartupLogger::start();
    StartupLogger::log(QStringLiteral("进入 main"));
    // 设置linux下动态库的默认路径
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR"); // 获取镜像的路径
    QString ldLibraryPath = appDirPath + "/usr/lib";
    std::string currentPath = ldLibraryPath.toLocal8Bit().constData();
    setenv("LD_LIBRARY_PATH", currentPath.c_str(), 1); // 指定找动态库的默认路径 LD_LIBRARY_PATH
#endif

#if defined(Q_OS_LINUX) && defined(EVA_LINUX_STATIC_BUILD)
    qputenv("QT_IM_MODULE", QByteArray("fcitx"));
    qputenv("XMODIFIERS", QByteArray("@im=fcitx"));
    qputenv("GTK_IM_MODULE", QByteArray("fcitx"));
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);                                       // 自适应缩放
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough); // 适配非整数倍缩放
    QApplication a(argc, argv);                                                                              // 事件实例
    StartupLogger::log(QStringLiteral("QApplication 初始化完成"));
    a.setQuitOnLastWindowClosed(false);                                                                      // 即使关闭所有窗口也不退出程序，为了保持系统托盘正常
    // 加载资源文件中的字体
    int fontId = QFontDatabase::addApplicationFont(":/zpix.ttf");
    QStringList loadedFonts = QFontDatabase::applicationFontFamilies(fontId);
    if (!loadedFonts.empty())
    {
        QFont customFont(loadedFonts.at(0));
        const QFont defaultFont = QApplication::font(); // preserve platform-default sizing when swapping the typeface
        if (defaultFont.pointSize() > 0)
        {
            customFont.setPointSize(defaultFont.pointSize());
        }
        else if (defaultFont.pixelSize() > 0)
        {
            customFont.setPixelSize(defaultFont.pixelSize());
        }
        customFont.setWeight(defaultFont.weight());
        customFont.setStyleStrategy(QFont::PreferAntialias); // keep the existing anti-aliasing strategy for pixel clarity
        QApplication::setFont(customFont);
        // qDebug() << "Loaded font:" << customFont.family();
    }
    StartupLogger::log(QStringLiteral("字体资源加载完成"));

    // 设置创建EVA_TEMP文件夹所在的目录
#if BODY_LINUX_PACK
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString appImagePath = env.value("APPIMAGE");
    const QFileInfo fileInfo(appImagePath);
    const QString applicationDirPath = fileInfo.absolutePath(); // 在打包程序运行时所在目录创建EVA_TEMP文件夹
    const QString appPath = appImagePath;                       // .AppImage所在路径
#else
    const QString applicationDirPath = QCoreApplication::applicationDirPath(); // 就在当前目录创建EVA_TEMP文件夹
    const QString appPath = QCoreApplication::applicationFilePath();
#endif
// linux下每次启动都创建.desktop到~/.local/share/applications/（开始菜单）和~/Desktop（桌面快捷方式）中
#ifdef Q_OS_LINUX
    createDesktopShortcut(appPath);
#endif
    qDebug() << "EVA_PATH" << appPath;
    StartupLogger::log(QStringLiteral("应用目录初始化完成"));

    // Single-instance: only one process per application path. If another is running,
    // ping it to raise the window and exit quietly.
    const QString instanceKey = SingleInstance::keyForAppPath(appPath);
    SingleInstance instanceGuard(instanceKey);
    if (!instanceGuard.ensurePrimary())
    {
        instanceGuard.notifyRunning("ACTIVATE");
        return 0;
    }
    StartupLogger::log(QStringLiteral("单实例检查通过"));
    // Auto-discover default models from EVA_MODELS when no config exists
    {
        const QString tempDir = applicationDirPath + "/EVA_TEMP";
        QDir().mkpath(tempDir);
        const QString cfgPath = tempDir + "/eva_config.ini";
        if (!QFile::exists(cfgPath))
        {
            auto findSmallest = [](const QString &root, const QStringList &exts, std::function<bool(const QFileInfo &)> pred = nullptr) -> QString
            {
                if (root.isEmpty() || !QDir(root).exists()) return QString();
                QString best;
                qint64 bestSz = LLONG_MAX;
                QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext())
                {
                    const QString p = it.next();
                    QFileInfo fi(p);
                    if (!fi.isFile()) continue;
                    const QString suffix = fi.suffix().toLower();
                    if (!exts.contains("*." + suffix)) continue;
                    if (pred && !pred(fi)) continue;
                    const qint64 sz = fi.size();
                    if (sz > 0 && sz < bestSz)
                    {
                        best = fi.absoluteFilePath();
                        bestSz = sz;
                    }
                }
                return best;
            };
            const QString modelsRoot = QDir(applicationDirPath).filePath("EVA_MODELS");
            QString llmModel, embModel, whisperModel, ttsModel, sdModel;
            if (QDir(modelsRoot).exists())
            {
                // LLM: EVA_MODELS/llm -> smallest .gguf
                llmModel = findSmallest(QDir(modelsRoot).filePath("llm"), {"*.gguf"});
                // Embedding: EVA_MODELS/embedding -> smallest .gguf
                embModel = findSmallest(QDir(modelsRoot).filePath("embedding"), {"*.gguf"});
                // Whisper(STT): EVA_MODELS/speech2text -> prefer filenames containing 'whisper'
                const QString sttRoot = QDir(modelsRoot).filePath("speech2text");
                whisperModel = findSmallest(sttRoot, {"*.bin", "*.gguf"}, [](const QFileInfo &fi)
                                            { return fi.fileName().toLower().contains("whisper"); });
                // TTS: prefer tts.cpp-compatible models (kokoro / tts keyword) under text2speech; fallback to smallest gguf
                auto findTts = [&](const QString &root)
                {
                    QString picked = findSmallest(root, {"*.gguf"}, [](const QFileInfo &fi)
                                                   {
                                                       const QString name = fi.fileName().toLower();
                                                       return name.contains("kokoro") || name.contains("tts");
                                                   });
                    if (picked.isEmpty()) picked = findSmallest(root, {"*.gguf"});
                    return picked;
                };
                ttsModel = findTts(QDir(modelsRoot).filePath("text2speech"));
                if (ttsModel.isEmpty()) ttsModel = findTts(sttRoot);
                // SD: Prefer fixed path EVA_MODELS/text2image/sd1.5-anything-3-q8_0.gguf; fallback: smallest .gguf under text2image
                const QString sdFixed = QDir(modelsRoot).filePath("text2image/sd1.5-anything-3-q8_0.gguf");
                if (QFile::exists(sdFixed)) sdModel = QFileInfo(sdFixed).absoluteFilePath();
                if (sdModel.isEmpty()) sdModel = findSmallest(QDir(modelsRoot).filePath("text2image"), {"*.gguf"});
            }
            // Persist discovered defaults so subsequent startup path applies uniformly
            QSettings s(cfgPath, QSettings::IniFormat);
            s.setIniCodec("utf-8");
            if (!llmModel.isEmpty()) s.setValue("modelpath", llmModel);
            if (!embModel.isEmpty()) s.setValue("embedding_modelpath", embModel);
            if (!whisperModel.isEmpty()) s.setValue("whisper_modelpath", whisperModel);
            if (!ttsModel.isEmpty()) s.setValue("ttscpp_modelpath", ttsModel);
            if (!sdModel.isEmpty())
            {
                s.setValue("sd_modelpath", sdModel);
                s.setValue("sd_params_template", "sd1.5-anything-3");
            }
            // Default to local mode and auto device backend on first boot
            s.setValue("ui_mode", 0);
            s.setValue("device_backend", DeviceManager::userChoice().isEmpty() ? "auto" : DeviceManager::userChoice());
            s.sync();
        }
    }
    StartupLogger::log(QStringLiteral("默认模型自动发现完成"));
    //------------------实例化主要节点------------------
    QElapsedTimer widgetTimer;
    widgetTimer.start();
    Widget w(nullptr, applicationDirPath);      // 窗口实例
    StartupLogger::log(QStringLiteral("Widget 构造完成（%1 ms）").arg(widgetTimer.elapsed()));
    QElapsedTimer expendTimer;
    expendTimer.start();
    Expend expend(nullptr, applicationDirPath); // 增殖窗口实例
    StartupLogger::log(QStringLiteral("Expend 构造完成（%1 ms）").arg(expendTimer.elapsed()));
    QElapsedTimer toolTimer;
    toolTimer.start();
    xTool tool(applicationDirPath);             // 工具实例
    StartupLogger::log(QStringLiteral("xTool 构造完成（%1 ms）").arg(toolTimer.elapsed()));
    // 将 xNet 改为堆对象，确保在其所属线程内析构，避免 Windows 下 QWinEventNotifier 跨线程清理告警
    xNet *net = new xNet; // 链接实例（worker 线程内生命周期）
    xMcp mcp;             // mcp管理实例
    gpuChecker gpuer;     // 监测显卡信息
    cpuChecker cpuer;     // 监视系统信息

    //-----------------初始值设定-----------------------
    // 传递语言（注意 net 改为指针）
    expend.wordsObj = w.wordsObj;
    net->wordsObj = w.wordsObj;
    tool.wordsObj = w.wordsObj;
    expend.max_thread = w.max_thread;
    tool.embedding_server_resultnumb = expend.embedding_resultnumb;          // 同步数目
    w.currentpath = w.historypath = expend.currentpath = applicationDirPath; // 默认打开路径
    w.whisper_model_path = QString::fromStdString(expend.whisper_params.model);

    //------------------二次启动激活已开窗口------------------
    QObject::connect(&instanceGuard, &SingleInstance::received, &w, [&w](const QByteArray &)
                     {
                         // Show and activate the main window when a secondary instance pings us
                         toggleWindowVisibility(&w, true); });

    //-----------------加载皮肤-----------------------
    QFile file(":/QSS/theme_unit01.qss");
    file.open(QFile::ReadOnly);
    QString stylesheet = file.readAll();
    a.setStyleSheet(stylesheet);
    w.setBaseStylesheet(stylesheet);

    //------------------注册信号传递变量-------------------
    qRegisterMetaType<EVA_CHATS_TEMPLATE>("EVA_CHATS_TEMPLATE");
    qRegisterMetaType<MODEL_PARAMS>("MODEL_PARAMS");
    qRegisterMetaType<QColor>("QColor");
    qRegisterMetaType<SIGNAL_STATE>("SIGNAL_STATE");
    qRegisterMetaType<EVA_DATES>("EVA_DATES");
    qRegisterMetaType<SETTINGS>("SETTINGS");
    qRegisterMetaType<QVector<Embedding_vector>>("QVector<Embedding_vector>");
    qRegisterMetaType<Speech_Params>("Speech_Params");
    qRegisterMetaType<QPair<QString, QString>>("QPair<QString, QString>");
    qRegisterMetaType<mcp::json>("mcp::json");
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
    net->moveToThread(net_thread);
    // 当线程结束时，在线程上下文中安全删除 xNet，避免跨线程销毁导致的 QWinEventNotifier 警告/卡顿
    QObject::connect(net_thread, &QThread::finished, net, &QObject::deleteLater);
    net_thread->start();
    // 退出前先请求停止网络流（排队到 net 所在线程）
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [net]()
                     { QMetaObject::invokeMethod(net, "recv_stop", Qt::QueuedConnection, Q_ARG(bool, true)); });
    // 再优雅退出并等待线程结束，确保 xNet 在其线程内清理完毕，避免退出时短暂卡住
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [net_thread]()
                     {
        net_thread->quit();
        net_thread->wait(3000); });
    // Ensure knowledge-base embedding server is stopped when the app quits,
    // even if the Expend window was closed earlier
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &expend, [&expend]()
                     { expend.stopEmbeddingServer(true); }, Qt::QueuedConnection);
    QThread *mcp_thread = new QThread;
    mcp.moveToThread(mcp_thread);
    mcp_thread->start();

    // 统一的应用退出收尾：优雅停止各工作线程，避免退出阶段跨线程清理产生告警/卡顿
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [gpuer_thread]()
                     {
        gpuer_thread->quit();
        gpuer_thread->wait(1000); });
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [cpuer_thread]()
                     {
        cpuer_thread->quit();
        cpuer_thread->wait(1000); });
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [tool_thread]()
                     {
        tool_thread->quit();
        tool_thread->wait(2000); });
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [mcp_thread]()
                     {
        mcp_thread->quit();
        mcp_thread->wait(2000); });
    //------------------监测gpu信息-------------------
    QObject::connect(&gpuer, &gpuChecker::gpu_status, &w, &Widget::recv_gpu_status); // 传递gpu信息
    QObject::connect(&w, &Widget::gpu_reflash, &gpuer, &gpuChecker::checkGpu);       // 强制刷新gpu信息

    //------------------监测系统信息-------------------
    QObject::connect(&cpuer, &cpuChecker::cpu_status, &w, &Widget::recv_cpu_status); // 传递cpu信息
    QObject::connect(&w, &Widget::cpu_reflash, &cpuer, &cpuChecker::chekCpu);        // 强制刷新cpu信息

    //------------------连接窗口和增殖窗口-------------------
    QObject::connect(&w, &Widget::ui2expend_language, &expend, &Expend::recv_language);         // 传递使用的语言
    QObject::connect(&w, &Widget::ui2expend_show, &expend, &Expend::recv_expend_show);          // 通知显示扩展窗口
    QObject::connect(&w, &Widget::ui2expend_speechdecode, &expend, &Expend::recv_speechdecode); // 开始语音转文字
    QObject::connect(&w, &Widget::ui2expend_resettts, &expend, &Expend::recv_resettts);         // 重置文字转语音
    // 模型评估：同步 UI 端点/设置/模式
    QObject::connect(&w, &Widget::ui2expend_apis, &expend, &Expend::recv_eval_apis);
    QObject::connect(&w, &Widget::ui2expend_settings, &expend, &Expend::recv_eval_settings);
    QObject::connect(&w, &Widget::ui2expend_mode, &expend, &Expend::recv_eval_mode);
    QObject::connect(&expend, &Expend::expend2ui_speechdecode_over, &w, &Widget::recv_speechdecode_over);       // 转换完成返回结果
    QObject::connect(&expend, &Expend::expend2ui_whisper_modelpath, &w, &Widget::recv_whisper_modelpath);       // 传递模型路径
    QObject::connect(&expend, &Expend::expend2ui_state, &w, &Widget::reflash_state);                            // 窗口状态区更新
    QObject::connect(&expend, &Expend::expend2ui_embeddingdb_describe, &w, &Widget::recv_embeddingdb_describe); // 传递知识库的描述
    QObject::connect(&expend, &Expend::expend2ui_mcpToolsChanged, &w, &Widget::recv_mcp_tools_changed);         // MCP工具开关更新
    QObject::connect(&w, &Widget::ui2expend_llamalog, &expend, &Expend::recv_llama_log);                        // 传递llama日志

    //------------------连接net和窗口-------------------
    QObject::connect(net, &xNet::net2ui_output, &w, &Widget::reflash_output, Qt::QueuedConnection); // 窗口输出区更新
    // Forward streaming output to Expend for TTS segmentation/playback
    QObject::connect(net, &xNet::net2ui_output, &expend, &Expend::recv_output, Qt::QueuedConnection);                // 文转声：接收模型流式输出
    QObject::connect(net, &xNet::net2ui_state, &w, &Widget::reflash_state, Qt::QueuedConnection);                    // 窗口状态区更新
    QObject::connect(net, &xNet::net2ui_pushover, &w, &Widget::recv_pushover, Qt::QueuedConnection);                 // 完成推理
    QObject::connect(net, &xNet::net2ui_pushover, &expend, &Expend::onNetTurnDone, Qt::QueuedConnection);            // 文转声：回合结束时刷新未完句
    QObject::connect(net, &xNet::net2ui_kv_tokens, &w, &Widget::recv_kv_from_net, Qt::QueuedConnection);             // 流式近似KV用量（链接模式兜底）
    QObject::connect(net, &xNet::net2ui_speeds, &w, &Widget::recv_net_speeds, Qt::QueuedConnection);                 // 最终速度（来自 xNet timings）
    QObject::connect(net, &xNet::net2ui_prompt_baseline, &w, &Widget::recv_prompt_baseline, Qt::QueuedConnection);   // prompt baseline tokens (LINK mode)
    QObject::connect(net, &xNet::net2ui_turn_counters, &w, &Widget::recv_turn_counters, Qt::QueuedConnection);       // timings cache/prompt/generated totals
    QObject::connect(net, &xNet::net2ui_slot_id, &w, &Widget::onSlotAssigned, Qt::QueuedConnection);                 // capture server slot id
    QObject::connect(net, &xNet::net2ui_reasoning_tokens, &w, &Widget::recv_reasoning_tokens, Qt::QueuedConnection); // think tokens for this turn
    QObject::connect(&w, &Widget::ui2net_push, net, &xNet::run, Qt::QueuedConnection);                               // 开始推理
    QObject::connect(&w, &Widget::ui2net_language, net, &xNet::recv_language, Qt::QueuedConnection);                 // 传递使用的语言
    QObject::connect(&w, &Widget::ui2net_apis, net, &xNet::recv_apis, Qt::QueuedConnection);                         // 传递api设置参数
    QObject::connect(&w, &Widget::ui2net_data, net, &xNet::recv_data, Qt::QueuedConnection);                         // 传递端点参数
    QObject::connect(&w, &Widget::ui2net_stop, net, &xNet::recv_stop, Qt::QueuedConnection);                         // 传递停止信号

    //------------------连接tool和窗口-------------------
    QObject::connect(&tool, &xTool::tool2ui_state, &w, &Widget::reflash_state);        // 窗口状态区更新
    QObject::connect(&tool, &xTool::tool2ui_pushover, &w, &Widget::recv_toolpushover); // 完成推理
    QObject::connect(&tool, &xTool::tool2ui_terminalCommandStarted, &w, &Widget::toolCommandStarted);
    QObject::connect(&tool, &xTool::tool2ui_terminalStdout, &w, &Widget::toolCommandStdout);
    QObject::connect(&tool, &xTool::tool2ui_terminalStderr, &w, &Widget::toolCommandStderr);
    QObject::connect(&tool, &xTool::tool2ui_terminalCommandFinished, &w, &Widget::toolCommandFinished);
    QObject::connect(&w, &Widget::ui2tool_language, &tool, &xTool::recv_language); // 传递使用的语言
    QObject::connect(&w, &Widget::ui2tool_exec, &tool, &xTool::Exec);              // 开始推理
    QObject::connect(&w, &Widget::ui2tool_workdir, &tool, &xTool::recv_workdir);   // 设置工程师工作目录
    QObject::connect(&w, &Widget::ui2tool_interruptCommand, &tool, &xTool::cancelExecuteCommand);
    QObject::connect(&w, &Widget::ui2tool_cancelActive, &tool, &xTool::cancelActiveTool);

    //------------------连接增殖窗口和tool-------------------
    QObject::connect(&expend, &Expend::expend2tool_embeddingdb, &tool, &xTool::recv_embeddingdb);                 // 传递已嵌入文本段数据
    QObject::connect(&expend, &Expend::expend2ui_embedding_resultnumb, &tool, &xTool::recv_embedding_resultnumb); // 传递嵌入结果返回个数

    QObject::connect(&tool, &xTool::tool2expend_draw, &expend, &Expend::recv_draw);         // 开始绘制图像
    QObject::connect(&expend, &Expend::expend2tool_drawover, &tool, &xTool::recv_drawover); // 图像绘制完成

    // 将持久化向量库的内容同步给工具层（若已存在）
    if (!expend.Embedding_DB.isEmpty())
    {
        emit expend.expend2tool_embeddingdb(expend.Embedding_DB);
    }

    //------------------连接mcp管理器-------------------
    QObject::connect(&mcp, &xMcp::mcp_message, &expend, &Expend::recv_mcp_message);
    QObject::connect(&expend, &Expend::expend2mcp_addService, &mcp, &xMcp::addService);
    QObject::connect(&expend, &Expend::expend2mcp_refreshTools, &mcp, &xMcp::refreshTools);
    QObject::connect(&expend, &Expend::expend2mcp_disconnectAll, &mcp, &xMcp::disconnectAll);
    QObject::connect(&mcp, &xMcp::toolsRefreshed, &expend, &Expend::recv_mcp_tools_refreshed);
    QObject::connect(&mcp, &xMcp::addService_single_over, &expend, &Expend::recv_addService_single_over); // 添加某个mcp服务完成
    QObject::connect(&mcp, &xMcp::addService_over, &expend, &Expend::recv_addService_over);
    QObject::connect(&tool, &xTool::tool2mcp_toollist, &mcp, &xMcp::callList);       // 查询mcp可用工具
    QObject::connect(&mcp, &xMcp::callList_over, &tool, &xTool::recv_calllist_over); // 查询mcp可用工具完成
    QObject::connect(&tool, &xTool::tool2mcp_toolcall, &mcp, &xMcp::callTool);       // 开始调用mcp可用工具
    QObject::connect(&mcp, &xMcp::callTool_over, &tool, &xTool::recv_callTool_over); // mcp可用工具调用完成

    //---------------读取配置文件并执行------------------
    QFile configfile(applicationDirPath + "/EVA_TEMP/eva_config.ini");
    if (configfile.exists())
    {
        emit w.gpu_reflash(); // 强制刷新gpu信息，为了获取未装载时的显存占用
                              //  读取配置文件中的值
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        w.loadGlobalUiSettings(settings);
        w.shell = tool.shell = expend.shell = settings.value("shell", DEFAULT_SHELL).toString();                                    // 读取记录在配置文件中的shell路径
        w.pythonExecutable = tool.pythonExecutable = expend.pythonExecutable = settings.value("python", DEFAULT_PYTHON).toString(); // 读取记录在配置文件中的python版本
        QString modelpath = settings.value("modelpath", applicationDirPath + DEFAULT_LLM_MODEL_PATH).toString();                    // 模型路径
        w.currentpath = w.historypath = expend.currentpath = modelpath;                                                             // 默认打开路径
        w.ui_SETTINGS.modelpath = modelpath;
        w.ui_mode = static_cast<EVA_MODE>(settings.value("ui_mode", "0").toInt()); //
        w.ui_monitor_frame = settings.value("monitor_frame", DEFAULT_MONITOR_FRAME).toDouble();
        w.api_endpoint_LineEdit->setText(settings.value("api_endpoint", "").toString());
        w.api_key_LineEdit->setText(settings.value("api_key", "").toString());
        w.api_model_LineEdit->setText(settings.value("api_model", "default").toString());
        w.apis.api_endpoint = w.api_endpoint_LineEdit->text();
        w.apis.api_key = w.api_key_LineEdit->text();
        w.apis.api_model = w.api_model_LineEdit->text();

        // Migrate numeric keys -> string keys, then remove numeric/percent keys to keep only strings
        {
            // top_p
            bool okS = false;
            const double num = settings.value("hid_top_p", DEFAULT_TOP_P).toDouble();
            const QString s = settings.value("hid_top_p_str", "").toString().trimmed();
            const double strv = s.isEmpty() ? -1.0 : QLocale::c().toDouble(s, &okS);
            const bool num_valid = (num > 0.0 && num <= 1.0);
            const bool str_valid = okS && (strv > 0.0 && strv <= 1.0);
            if (num_valid && (!str_valid || qAbs(num - strv) > 0.0005))
            {
                settings.setValue("hid_top_p_str", QString::number(num, 'f', 6));
            }
            settings.remove("hid_top_p");
            settings.remove("hid_top_p_percent");

            // temp
            bool okTs = false;
            const QString ts = settings.value("temp_str", "").toString().trimmed();
            const double tsv = ts.isEmpty() ? -1.0 : QLocale::c().toDouble(ts, &okTs);
            const double tnum = settings.value("temp", DEFAULT_TEMP).toDouble();
            const bool tnum_valid = (tnum >= 0.0 && tnum <= 2.0); // UI maps 0~2
            const bool tstr_valid = okTs && (tsv >= 0.0 && tsv <= 2.0);
            if (tnum_valid && (!tstr_valid || qAbs(tnum - tsv) > 0.0005))
            {
                settings.setValue("temp_str", QString::number(tnum, 'f', 6));
            }
            settings.remove("temp");
            settings.remove("temp_percent");

            // repeat
            bool okRs = false;
            const QString rs = settings.value("repeat_str", "").toString().trimmed();
            const double rsv = rs.isEmpty() ? -1.0 : QLocale::c().toDouble(rs, &okRs);
            const double rnum = settings.value("repeat", DEFAULT_REPEAT).toDouble();
            const bool rnum_valid = (rnum >= 0.0 && rnum <= 2.0);
            const bool rstr_valid = okRs && (rsv >= 0.0 && rsv <= 2.0);
            if (rnum_valid && (!rstr_valid || qAbs(rnum - rsv) > 0.0005))
            {
                settings.setValue("repeat_str", QString::number(rnum, 'f', 6));
            }
            settings.remove("repeat");
            settings.remove("repeat_percent");
            settings.sync();
        }
        w.custom1_date_system = settings.value("custom1_date_system", "").toString();
        w.custom2_date_system = settings.value("custom2_date_system", "").toString();
        {
            const QSignalBlocker blocker(w.date_ui->chattemplate_comboBox);
            w.date_ui->chattemplate_comboBox->setCurrentText(settings.value("chattemplate", "default").toString());
        }
        const QStringList enabledTools = settings.value("enabled_tools").toStringList();
        auto restoreToolCheckbox = [&](QCheckBox *box, const QString &id, const QString &legacyKey) -> bool {
            if (!box) return false;
            const bool fallback = settings.value(legacyKey, false).toBool();
            const bool shouldCheck = enabledTools.isEmpty() ? fallback : enabledTools.contains(id);
            const QSignalBlocker blocker(box);
            box->setChecked(shouldCheck);
            return shouldCheck;
        };
        const bool calculatorOn = restoreToolCheckbox(w.date_ui->calculator_checkbox, QStringLiteral("calculator"), QStringLiteral("calculator_checkbox"));
        const bool knowledgeOn = restoreToolCheckbox(w.date_ui->knowledge_checkbox, QStringLiteral("knowledge"), QStringLiteral("knowledge_checkbox"));
        const bool controllerOn = restoreToolCheckbox(w.date_ui->controller_checkbox, QStringLiteral("controller"), QStringLiteral("controller_checkbox"));
        const bool stablediffusionOn = restoreToolCheckbox(w.date_ui->stablediffusion_checkbox, QStringLiteral("stablediffusion"), QStringLiteral("stablediffusion_checkbox"));
        // Restore engineer work dir before toggling engineer checkbox (avoid double emits)
        {
            const QString saved = settings.value("engineer_work_dir", QDir(w.applicationDirPath).filePath("EVA_WORK")).toString();
            QString norm = QDir::cleanPath(saved);
            if (!QDir(norm).exists()) { norm = QDir(w.applicationDirPath).filePath("EVA_WORK"); }
            w.setEngineerWorkDirSilently(norm);
        }
        const bool engineerOn = restoreToolCheckbox(w.date_ui->engineer_checkbox, QStringLiteral("engineer"), QStringLiteral("engineer_checkbox"));
        const bool mcpOn = restoreToolCheckbox(w.date_ui->MCPtools_checkbox, QStringLiteral("mcp"), QStringLiteral("MCPtools_checkbox"));
        if (settings.contains("skills_enabled"))
        {
            w.restoreSkillSelection(settings.value("skills_enabled").toStringList());
        }
        if (w.date_ui->date_engineer_workdir_LineEdit)
        {
            w.date_ui->date_engineer_workdir_label->setVisible(engineerOn);
            w.date_ui->date_engineer_workdir_LineEdit->setVisible(engineerOn);
            w.date_ui->date_engineer_workdir_browse->setVisible(engineerOn);
        }
        w.is_load_tool = calculatorOn || knowledgeOn || controllerOn || stablediffusionOn || engineerOn || mcpOn;
        if (engineerOn)
        {
            const Widget::EngineerEnvSnapshot snapshot = w.collectEngineerEnvSnapshot();
            w.applyEngineerEnvSnapshot(snapshot, true);
        }
        else
        {
            w.ui_extra_prompt = w.is_load_tool ? w.create_extra_prompt() : QString();
        }
        if (settings.value("extra_lan", "zh").toString() != "zh") { w.switch_lan_change(); }
        if (engineerOn) { w.triggerEngineerEnvRefresh(true); }
        // 推理设备：先根据目录填充选项，再应用用户偏好
        const QString savedDevice = settings.value("device_backend", "auto").toString();
        DeviceManager::setUserChoice(savedDevice);
        int devIdx = w.settings_ui->device_comboBox->findText(DeviceManager::userChoice());
        if (devIdx >= 0) w.settings_ui->device_comboBox->setCurrentIndex(devIdx);

        // Use string-first read to eliminate drift; fallback to numeric
        {
            double repeat = DEFAULT_REPEAT;
            const QString repStr = settings.value("repeat_str", "").toString().trimmed();
            bool ok = false;
            if (!repStr.isEmpty())
            {
                repeat = QLocale::c().toDouble(repStr, &ok);
                if (!ok) repeat = DEFAULT_REPEAT;
            }
            else
            {
                repeat = settings.value("repeat", DEFAULT_REPEAT).toDouble();
            }
            w.settings_ui->repeat_slider->setValue(qBound(0, qRound(repeat * 100.0), 200));
        }
        w.settings_ui->nthread_slider->setValue(settings.value("nthread", w.ui_SETTINGS.nthread).toInt());
        w.settings_ui->nctx_slider->setValue(settings.value("nctx", DEFAULT_NCTX).toInt());
        w.settings_ui->ngl_slider->setValue(settings.value("ngl", DEFAULT_NGL).toInt());
        {
            double temp = DEFAULT_TEMP;
            const QString tempStr = settings.value("temp_str", "").toString().trimmed();
            bool ok = false;
            if (!tempStr.isEmpty())
            {
                temp = QLocale::c().toDouble(tempStr, &ok);
                if (!ok) temp = DEFAULT_TEMP;
            }
            else
            {
                temp = settings.value("temp", DEFAULT_TEMP).toDouble();
            }
            w.settings_ui->temp_slider->setValue(qBound(0, qRound(temp * 100.0), 100));
        }
        w.settings_ui->topk_slider->setValue(settings.value("top_k", DEFAULT_TOP_K).toInt());
        // 采样：top_p（作为隐藏参数持久化；此处提供显式滑块以便 LINK 模式也可调整）
        {
            // Prefer string value; then numeric; then legacy percent
            bool ok = false;
            const QString pStr = settings.value("hid_top_p_str", "").toString().trimmed();
            const double strv = !pStr.isEmpty() ? QLocale::c().toDouble(pStr, &ok) : DEFAULT_TOP_P;
            const bool str_valid = ok && (strv > 0.0 && strv <= 1.0);
            const double num = settings.value("hid_top_p", DEFAULT_TOP_P).toDouble();
            const bool num_valid = (num > 0.0 && num <= 1.0);
            double topp;
            if (str_valid)
                topp = strv;
            else if (num_valid)
                topp = num;
            else
            {
                const int perc = settings.value("hid_top_p_percent", int(DEFAULT_TOP_P * 100)).toInt();
                topp = qBound(0, perc, 100) / 100.0;
            }
            w.settings_ui->topp_slider->setValue(qBound(0, qRound(topp * 100.0), 100));
        }
        w.settings_ui->parallel_slider->setValue(settings.value("hid_parallel", DEFAULT_PARALLEL).toInt());
        w.settings_ui->port_lineEdit->setText(settings.value("port", DEFAULT_SERVER_PORT).toString());
        w.settings_ui->frame_lineEdit->setText(settings.value("monitor_frame", DEFAULT_MONITOR_FRAME).toString());
        bool embedding_server_need = settings.value("embedding_server_need", 0).toBool(); // 是否需要自动启动嵌入相关流程
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
        // Keep hidden top_p in sync using string-first read (will be overwritten by get_set later)
        {
            // Same precedence as above: string > numeric > percent
            bool ok = false;
            const QString pStr = settings.value("hid_top_p_str", "").toString().trimmed();
            const double strv = !pStr.isEmpty() ? QLocale::c().toDouble(pStr, &ok) : DEFAULT_TOP_P;
            const bool str_valid = ok && (strv > 0.0 && strv <= 1.0);
            const double num = settings.value("hid_top_p", DEFAULT_TOP_P).toDouble();
            const bool num_valid = (num > 0.0 && num <= 1.0);
            if (str_valid)
                w.ui_SETTINGS.hid_top_p = strv;
            else if (num_valid)
                w.ui_SETTINGS.hid_top_p = num;
            else
            {
                const int perc = settings.value("hid_top_p_percent", int(DEFAULT_TOP_P * 100)).toInt();
                w.ui_SETTINGS.hid_top_p = qBound(0, perc, 100) / 100.0;
            }
        }
        w.ui_SETTINGS.hid_batch = settings.value("hid_batch", DEFAULT_BATCH).toInt();
        w.ui_SETTINGS.hid_n_ubatch = settings.value("hid_n_ubatch", DEFAULT_UBATCH).toInt();
        w.ui_SETTINGS.hid_use_mmap = settings.value("hid_use_mmap", DEFAULT_USE_MMAP).toBool();
        w.ui_SETTINGS.hid_use_mlock = settings.value("hid_use_mlock", DEFAULT_USE_MLOCCK).toBool();
        w.ui_SETTINGS.hid_flash_attn = settings.value("hid_flash_attn", DEFAULT_FLASH_ATTN).toBool();
        w.ui_SETTINGS.hid_parallel = settings.value("hid_parallel", DEFAULT_PARALLEL).toInt();

        // Sync persisted max output length back to UI before propagating values
        w.settings_ui->npredict_spin->setValue(qBound(1, w.ui_SETTINGS.hid_npredict, 99999));
        w.npredict_change();

        // ui显示值传给ui内部值
        w.get_date(); // 获取约定中的纸面值
        w.get_set();  // 获取设置中的纸面值
        // Broadcast initial settings to Expend (evaluation tab)
        // In LINK mode, use discovered maximum context as n_ctx for display
        {
            SETTINGS snap = w.ui_SETTINGS;
            if (w.ui_mode == LINK_MODE && w.slotCtxMax_ > 0)
                snap.nctx = w.slotCtxMax_;
            emit w.ui2expend_settings(snap);
        }
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

        // 自动启动嵌入服务：
        // - 若已有持久化向量(Expend::Embedding_DB非空)，为支持查询自动启动服务（仅用于查询向量），不触发重嵌入
        // - 或者用户显式开启了 embedding_server_need（兼容旧配置），同样仅启动服务，不自动重嵌
        expend.configureEmbeddingAutoStart(embedding_modelpath, embedding_server_need);
    }
    w.show();        // 展示窗口
    StartupLogger::log(QStringLiteral("主窗口显示完成"));
    QFontInfo info(w.font());              // this widget's resolved font
    qDebug() << "Widget uses font:" << info.family();
    StartupLogger::log(QStringLiteral("启动阶段结束，进入事件循环"));
    return a.exec(); // 进入事件循环
}

