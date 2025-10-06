//功能函数
#include "ui_widget.h"
#include "widget.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>

//添加右击问题
void Widget::create_right_menu()
{
    QDate currentDate = QDate::currentDate(); //历史中的今天
    QString dateString = currentDate.toString("M" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    //---------------创建一般问题菜单--------------
    if (right_menu != nullptr)
    {
        delete right_menu;
    }
    right_menu = new QMenu(this);
    for (int i = 1; i < 14; ++i)
    {
        QString question;

        if (i == 4)
        {
            question = jtr(QString("Q%1").arg(i)).replace("{today}", dateString);
        } //历史中的今天
        else
        {
            question = jtr(QString("Q%1").arg(i));
        }
        QAction *action = right_menu->addAction(question);

        connect(action, &QAction::triggered, this, [=]() { ui->input->textEdit->setPlainText(question); });
    }
    //------------创建自动化问题菜单-------------
    //上传图像
    QAction *action14 = right_menu->addAction(jtr("Q14"));
    connect(action14, &QAction::triggered, this, [=]() {
        //用户选择图片
        QStringList paths = QFileDialog::getOpenFileNames(nullptr, jtr("Q14"), currentpath, "(*.png *.jpg *.bmp)");
        ui->input->addFiles(paths);
    });
    // 历史对话入口（打开管理界面）
    right_menu->addSeparator();
    QAction *histMgr = right_menu->addAction(jtr("history sessions"));
    connect(histMgr, &QAction::triggered, this, [this]() { openHistoryManager(); });
}

//添加托盘右击事件
void Widget::create_tray_right_menu()
{
    trayMenu->clear();
    QAction *showAction_shortcut = trayMenu->addAction(jtr("shortcut"));
    QAction *blank1 = trayMenu->addAction(""); //占位符
    QAction *blank2 = trayMenu->addAction(""); //占位符，目的是把截图顶出去，用户点击后才会隐藏
    blank1->setEnabled(false);
    blank2->setEnabled(false);
    trayMenu->addSeparator(); // 添加分割线
    QAction *showAction_widget = trayMenu->addAction(jtr("show widget"));
    QAction *showAction_expend = trayMenu->addAction(jtr("show expend"));
    trayMenu->addSeparator(); // 添加分割线
    QAction *exitAction = trayMenu->addAction(jtr("quit"));
    QObject::connect(showAction_widget, &QAction::triggered, this, [&]() {
        toggleWindowVisibility(this, true); // 显示窗体
    });
    QObject::connect(showAction_expend, &QAction::triggered, this, [&]() {
        emit ui2expend_show(PREV_WINDOW);
    });
    QObject::connect(showAction_shortcut, &QAction::triggered, this, [&]() {
        trayMenu->hide();
        QTimer::singleShot(100, [this]() { onShortcutActivated_F1(); }); // 触发截图
    });
    QObject::connect(exitAction, &QAction::triggered, QApplication::quit); // 退出程序
}

//获取设置中的纸面值
void Widget::get_set()
{
    ui_SETTINGS.temp = settings_ui->temp_slider->value() / 100.0;
    ui_SETTINGS.repeat = settings_ui->repeat_slider->value() / 100.0;
    ui_SETTINGS.hid_parallel = settings_ui->parallel_slider->value();
    ui_SETTINGS.top_k = settings_ui->topk_slider->value();

    ui_SETTINGS.nthread = settings_ui->nthread_slider->value();
    ui_SETTINGS.nctx = settings_ui->nctx_slider->value(); //获取nctx滑块的值
    ui_SETTINGS.ngl = settings_ui->ngl_slider->value();   //获取ngl滑块的值

    ui_SETTINGS.lorapath = settings_ui->lora_LineEdit->text();
    ui_SETTINGS.mmprojpath = settings_ui->mmproj_LineEdit->text();

    ui_SETTINGS.complete_mode = settings_ui->complete_btn->isChecked();
    ui_monitor_frame = settings_ui->frame_lineEdit->text().toDouble();

    if (settings_ui->chat_btn->isChecked())
    {
        ui_state = CHAT_STATE;
    }
    else if (settings_ui->complete_btn->isChecked())
    {
        ui_state = COMPLETE_STATE;
    }
    // 服务状态已弃用

    ui_port = settings_ui->port_lineEdit->text();

    // 推理设备：同步到 DeviceManager（auto/cpu/cuda/vulkan/opencl）
    ui_device_backend = settings_ui->device_comboBox->currentText().trimmed().toLower();
    DeviceManager::setUserChoice(ui_device_backend);
}

//获取约定中的纸面值
void Widget::get_date()
{
    ui_date_prompt = date_ui->date_prompt_TextEdit->toPlainText();
    //合并附加指令
    if (ui_extra_prompt != "")
    {
        ui_DATES.date_prompt = ui_date_prompt + "\n\n" + ui_extra_prompt;
    }
    else
    {
        ui_DATES.date_prompt = ui_date_prompt;
    }

    ui_DATES.user_name = date_ui->user_name_LineEdit->text();
    ui_DATES.model_name = date_ui->model_name_LineEdit->text();

    ui_DATES.is_load_tool = is_load_tool;
    ui_template = date_ui->chattemplate_comboBox->currentText();
    ui_extra_lan = date_ui->switch_lan_button->text();

    ui_calculator_ischecked = date_ui->calculator_checkbox->isChecked();
    ui_engineer_ischecked = date_ui->engineer_checkbox->isChecked();
    ui_MCPtools_ischecked = date_ui->MCPtools_checkbox->isChecked();
    ui_knowledge_ischecked = date_ui->knowledge_checkbox->isChecked();
    ui_controller_ischecked = date_ui->controller_checkbox->isChecked();
    ui_stablediffusion_ischecked = date_ui->stablediffusion_checkbox->isChecked();

    //记录自定义模板
    if (ui_template == jtr("custom set1"))
    {
        custom1_date_system = ui_date_prompt;
        custom1_user_name = ui_DATES.user_name;
        custom1_model_name = ui_DATES.model_name;
    }
    else if (ui_template == jtr("custom set2"))
    {
        custom2_date_system = ui_date_prompt;
        custom2_user_name = ui_DATES.user_name;
        custom2_model_name = ui_DATES.model_name;
    }

    //添加额外停止标志
    addStopwords();
}

//手搓输出解析器，提取可能的xml，目前只支持一个参数
mcp::json Widget::XMLparser(QString text)
{
    if (text.contains("</think>"))
    {
        text = text.split("</think>")[1]; //移除思考标签前面的所有内容
    }
    mcp::json toolsarg; // 提取出的工具名和参数
    // 匹配<tool></tool>之间的内容
    QRegularExpression toolRegex("<([^>]+)>(.*)</\\1>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch toolMatch = toolRegex.match(text);
    if (toolMatch.hasMatch())
    {
        QString toolContent = toolMatch.captured(2);
        qDebug() << "toolContent:" << toolContent;
        try
        {
            toolsarg = mcp::json::parse(toolContent.toStdString());
        }
        catch (const std::exception &e)
        {
            qCritical() << "tool JSON parse error:" << e.what();
        }
    }
    else
    {
        qDebug() << "no tool matched";
    }

    return toolsarg;
}

//构建额外指令
QString Widget::create_extra_prompt()
{
    QString extra_prompt_;            //额外指令
    QString available_tools_describe; //工具名和描述
    QString engineer_info;            //软件工程师信息
    extra_prompt_ = EXTRA_PROMPT_FORMAT;
    extra_prompt_.replace("{OBSERVATION_STOPWORD}", DEFAULT_OBSERVATION_STOPWORD);
    if (is_load_tool)
    {
        available_tools_describe += Buildin_tools_answer.text + "\n\n";
        // qDebug()<< MCP_TOOLS_INFO_LIST.size();
        if (date_ui->MCPtools_checkbox->isChecked())
        {
            available_tools_describe += Buildin_tools_mcp_tools_list.text + "\n\n";
        }
        if (date_ui->calculator_checkbox->isChecked())
        {
            available_tools_describe += Buildin_tools_calculator.text + "\n\n";
        }
        if (date_ui->knowledge_checkbox->isChecked())
        {
            available_tools_describe += Buildin_tools_knowledge.text.replace("{embeddingdb describe}", embeddingdb_describe) + "\n\n";
        }
        if (date_ui->stablediffusion_checkbox->isChecked())
        {
            available_tools_describe += Buildin_tools_stablediffusion.text + "\n\n";
        }
        if (date_ui->controller_checkbox->isChecked())
        {
            screen_info = create_screen_info(); //构建屏幕信息
            available_tools_describe += Buildin_tools_controller.text.replace("{screen_info}", screen_info) + "\n\n";
        }
        if (date_ui->engineer_checkbox->isChecked())
        {
            available_tools_describe += Buildin_tools_execute_command.text + "\n\n";
            available_tools_describe += Buildin_tools_read_file.text + "\n\n";
            available_tools_describe += Buildin_tools_write_file.text + "\n\n";
            available_tools_describe += Buildin_tools_edit_file.text + "\n\n";
            // 这里添加更多工程师的工具
            engineer_info = create_engineer_info(); //构建工程师信息
        }
        extra_prompt_.replace("{available_tools_describe}", available_tools_describe); //替换相应内容
        extra_prompt_.replace("{engineer_info}", engineer_info);                       //替换相应内容
    }
    else
    {
        extra_prompt_ = ""; //没有挂载工具则为空
    }
    return extra_prompt_;
}

QString Widget::truncateString(const QString &str, int maxLength)
{
    if (str.size() <= maxLength)
    {
        return str;
    }

    // 使用QTextStream来处理多字节字符
    QTextStream stream(const_cast<QString *>(&str), QIODevice::ReadOnly);
    stream.setCodec("UTF-8");

    // 找到开始截取的位置
    int startIndex = str.size() - maxLength;

    // 确保不截断多字节字符
    stream.seek(startIndex);

    return QString(stream.readAll());
}

//获取环境中的python版本以及库信息
QString Widget::checkPython()
{
    QString result;

    // Initialize the python executable name
    QString pythonExecutable = DEFAULT_PYTHON;

    QProcess process;
    QStringList shellArgs;
    shellArgs << CMDGUID << pythonExecutable + " --version";
    process.start(shell, shellArgs);
    process.waitForFinished();

    QString versionOutput = process.readAllStandardOutput();
    QString versionError = process.readAllStandardError();
    QString versionInfo = versionOutput + versionError;

    // If still not found, return an error message
    if (versionInfo.isEmpty())
    {
        result += "Python interpreter not found in the system environment.\n";
        return result;
    }
    else
    {
        QStringList shellArgs2;
#ifdef Q_OS_WIN
        shellArgs2 << CMDGUID << pythonExecutable << "-c"
                   << "import sys; print(sys.executable)";
#elif defined(Q_OS_LINUX)
        // 注意linux下不能有两个-c
        shellArgs2 << CMDGUID << pythonExecutable + "-c \"import sys; print(sys.executable)\"";
#endif
        process.start(shell, shellArgs2);
        process.waitForFinished();
        QString python_absolutePath = process.readAllStandardOutput().trimmed();
        result += versionInfo + " " + python_absolutePath + "\n";
    }

    // // 获取安装库信息
    // QStringList shellArgs3;
    // shellArgs3 << CMDGUID << pythonExecutable + " -m pip list";
    // process.start(shell, shellArgs3);
    // process.waitForFinished();

    // QString pipOutput = process.readAllStandardOutput();
    // QString pipError = process.readAllStandardError();
    // QString pipList = pipOutput + pipError;

    // // 不想展示的库
    // QStringList excludeLibraries = {
    //     "anyio", "appdirs", "archspec", "arrow", "astroid", "astropy", "astropy-iers-data", "asttokens", "async-lru", "attrs", "autopep8", "Babel", "black", "bleach", "bokeh", "botocore", "Bottleneck",
    //     "aiohttp", "aioitertools", "altair", "anaconda-anon-usage", "anaconda-catalogs", "anaconda-client", "anaconda-cloud-auth", "anaconda-navigator", "anaconda-project",
    //     "aiobotocore", "aiohappyeyeballs", "aiosignal", "alabaster", "annotated-types", "argon2-cffi", "argon2-cffi-bindings", "atomicwrites", "Automat", "bcrypt", "binaryornot",
    //     "blinker", "boltons", "Brotli", "cachetools", "cffi", "chardet", "charset-normalizer", "colorama", "cssselect", "cycler", "cytoolz", "decorator", "defusedxml", "diff-match-patch",
    //     "distro", "docstring-to-markdown", "et-xmlfile", "executing", "frozenlist", "frozendict", "h11", "HeapDict", "hyperlink", "idna", "imagecodecs", "imagesize", "importlib-metadata",
    //     "incremental", "iniconfig", "intervaltree", "ipython-genutils", "jaraco.classes", "jmespath", "jsonpointer", "jsonpatch", "json5", "lazy-object-proxy", "lazy_loader", "locket",
    //     "mdit-py-plugins", "mdurl", "mkl_fft", "mkl_random", "mkl-service", "more-itertools", "mpmath", "multidict", "multipledispatch", "mypy-extensions", "navigator-updater", "nest-asyncio", "overrides",
    //     "pkce", "platformdirs", "pluggy", "pure-eval", "py-cpuinfo", "pyasn1", "pyasn1-modules", "pycosat", "pyct", "pydocstyle", "pyerfa", "pylint-venv", "pyls-spyder", "pytoolconfig", "pyviz_comms",
    //     "pywin32-ctypes", "pywinpty", "queuelib", "referencing", "rfc3339-validator", "rfc3986-validator", "rope", "rpds-py", "ruamel.yaml.clib", "ruamel-yaml-conda", "safetensors", "semver",
    //     "service-identity", "sip", "smmap", "sniffio", "snowballstemmer", "sortedcontainers", "stack-data", "tblib", "tenacity", "text-unidecode", "textdistance", "threadpoolctl", "three-merge", "tinycss2", "tldextract",
    //     "tomli", "toolz", "truststore", "twisted-iocpsupport", "ua-parser", "ua-parser-builtins", "uc-micro-py", "unicodedata2", "user-agents", "w3lib", "whatthepatch", "windows-curses", "zict", "zope.interface", "zstandard"
    // };

    // // If pip output is empty, pip may not be installed
    // if (pipList.contains("No module named pip")) {
    //     result += "pip is not installed for the detected Python interpreter.\n";
    // } else if (pipList.isEmpty()) {
    //     result += "No Python libraries installed or unable to retrieve the list.\n";
    // } else {
    //     // Extract package names from the pip list output
    //     QStringList lines = pipList.split('\n');
    //     QStringList packageNames;

    //     // Skip the first line (header) and process the rest
    //     for (int i = 2; i < lines.size(); ++i) {  // Start from the second line
    //         QString line = lines[i].trimmed();
    //         if (line.isEmpty()) continue;  // Skip empty lines

    //         QStringList parts = line.split(QRegExp("\\s+"));  // Split by whitespace
    //         if (parts.size() > 0) {
    //             QString packageName = parts[0];
    //             if (!excludeLibraries.contains(packageName)) {  // Exclude foundational libraries
    //                 packageNames.append(packageName);
    //             }
    //         }
    //     }

    //     // Join the package names into a single string, separated by spaces
    //     result += "Part Installed Python Libraries: " + truncateString(packageNames.join(" "),MAX_INPUT);
    // }

    return result;
}

QString Widget::checkCompile()
{
    QString compilerInfo;
    QProcess process;
    // Windows平台的编译器检查
#ifdef Q_OS_WIN
    // 尝试检查 MinGW
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "g++ --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty())
        {
            compilerInfo += "MinGW version: ";
            QStringList lines = output.split('\n');
            QString versionLine = lines.first();
            QRegExp regExp("\\s*\\(.*\\)"); // 使用正则表达式去除括号中的内容 匹配括号及其中的内容
            versionLine = versionLine.replace(regExp, "");
            compilerInfo += versionLine.trimmed(); // 去除前后的空格
            compilerInfo += "\n";
        }
    }

    // 检查 MSVC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "cl /Bv";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QByteArray output = process.readAllStandardOutput();
        output += process.readAllStandardError();
        if (!output.isEmpty())
        {
            compilerInfo += "MSVC version: ";
            QString outputStr = QString::fromLocal8Bit(output);
            QStringList lines = outputStr.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }
    }

    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty())
        {
            compilerInfo += "Clang version: ";
            QStringList lines = output.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }
    }

#endif

    // Linux平台的编译器检查
#ifdef Q_OS_LINUX
    // 检查 GCC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "gcc --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty())
        {
            compilerInfo += "GCC version: ";
            QStringList lines = output.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }
    }

    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        if (!output.isEmpty())
        {
            compilerInfo += "Clang version: ";
            QStringList lines = output.split('\n');
            compilerInfo += lines.first();
            compilerInfo += "\n";
        }
    }

#endif

    // 如果没有找到任何编译器信息
    if (compilerInfo.isEmpty())
    {
        compilerInfo = "No compiler detected.\n";
    }

    return compilerInfo;
}

QString Widget::create_engineer_info()
{
    QString engineer_info_ = ENGINEER_INFO;
    QString engineer_system_info_ = ENGINEER_SYSTEM_INFO;
    QDate currentDate = QDate::currentDate(); //今天日期
    QString dateString = currentDate.toString("yyyy" + QString(" ") + jtr("year") + QString(" ") + "M" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    engineer_system_info_.replace("{OS}", USEROS);
    engineer_system_info_.replace("{DATE}", dateString);
    engineer_system_info_.replace("{SHELL}", shell);
    engineer_system_info_.replace("{COMPILE_ENV}", compile_env);
    engineer_system_info_.replace("{PYTHON_ENV}", python_env);
    engineer_system_info_.replace("{DIR}", applicationDirPath + "/EVA_WORK");

    engineer_info_.replace("{engineer_system_info}", engineer_system_info_);
    return engineer_info_;
}

//添加额外停止标志，本地模式时在xbot.cpp里已经现若同时包含"<|" 和 "|>"也停止
void Widget::addStopwords()
{
    ui_DATES.extra_stop_words.clear(); //重置额外停止标志

    if (ui_DATES.is_load_tool) //如果挂载了工具则增加额外停止标志
    {
        // ui_DATES.extra_stop_words << DEFAULT_OBSERVATION_STOPWORD;//在后端已经处理了
    }
}

//获取本机第一个ip地址 排除以.1结尾的地址 如果只有一个.1结尾的则保留它
QString Widget::getFirstNonLoopbackIPv4Address()
{
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    QString ipWithDot1; // 用于存储以.1结尾的IP地址

    for (int i = 0; i < list.count(); i++)
    {
        QString ip = list[i].toString();
        // 排除回环地址和非IPv4地址
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol)
        {
            if (ip.endsWith(".1"))
            {
                ipWithDot1 = ip; // 记录以.1结尾的IP地址
            }
            else
            {
                return ip; // 返回第一个不以.1结尾的IP地址
            }
        }
    }

    // 如果没有找到不以.1结尾的IP地址，则返回以.1结尾的IP地址
    if (!ipWithDot1.isEmpty())
    {
        return ipWithDot1;
    }

    return QString(); // 如果没有找到任何符合条件的IP地址，返回空字符串
}

// 服务模式已移除：server_onProcessStarted/server_onProcessFinished

// llama-bench进程结束响应
void Widget::bench_onProcessFinished()
{
    qDebug() << "llama-bench进程结束响应";
}

//显示文件名和图像
void Widget::showImages(QStringList images_filepath)
{
    for (int i = 0; i < images_filepath.size(); ++i)
    {
        QString imagepath = images_filepath[i];
        QString ui_output = imagepath + "\n";
        if (ui_output != ":/logo/wav.png") { output_scroll(ui_output); }

        // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
        QImage image(imagepath);
        int originalWidth = image.width() / devicePixelRatioF();
        int originalHeight = image.height() / devicePixelRatioF();

        QTextCursor cursor(ui->output->textCursor());
        cursor.movePosition(QTextCursor::End);

        QTextImageFormat imageFormat;
        imageFormat.setWidth(originalWidth / 2);   // 设置图片的宽度
        imageFormat.setHeight(originalHeight / 2); // 设置图片的高度
        imageFormat.setName(imagepath);            // 图片资源路径

        cursor.insertImage(imageFormat);
        output_scroll("\n");
        //滚动到底部展示
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum()); //滚动条滚动到最下面
    }
}

//开始录音
void Widget::recordAudio()
{
    reflash_state("ui:" + jtr("recoding") + "... ");
    ui_state_recoding();

    audioRecorder.record();  // 在这之前检查是否可用
    audio_timer->start(100); // 每隔100毫秒刷新一次输入区
}

// 每隔100毫秒刷新一次监视录音
void Widget::monitorAudioLevel()
{
    audio_time += 100;
    ui_state_recoding(); //更新输入区
}

//停止录音
void Widget::stop_recordAudio()
{
    QString wav_path = applicationDirPath + "/EVA_TEMP/" + QString("EVA_") + ".wav";
    is_recodering = false;
    audioRecorder.stop();
    audio_timer->stop();
    reflash_state("ui:" + jtr("recoding over") + " " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s");
    audio_time = 0;
    //将录制的wav文件重采样为16khz音频文件
#ifdef _WIN32
    QTextCodec *code = QTextCodec::codecForName("GB2312"); // mingw中文路径支持
    std::string wav_path_c = code->fromUnicode(wav_path).data();
#elif __linux__
    std::string wav_path_c = wav_path.toStdString();
#endif
    resampleWav(wav_path_c, wav_path_c);
    emit ui2expend_speechdecode(wav_path, "txt"); //传一个wav文件开始解码
}

//更新gpu内存使用率
void Widget::updateGpuStatus()
{
    emit gpu_reflash();
}

//更新cpu内存使用率
void Widget::updateCpuStatus()
{
    emit cpu_reflash();
}

//拯救中文
void Widget::getWords(QString json_file_path)
{
    QFile jfile(json_file_path);
    if (!jfile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Cannot open file for reading.";
        return;
    }

    QTextStream in(&jfile);
    in.setCodec("UTF-8"); // 确保使用UTF-8编码读取文件
    QString data = in.readAll();
    jfile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = doc.object();
    wordsObj = jsonObj["words"].toObject();
}

//切换额外指令的语言
void Widget::switch_lan_change()
{
    if (date_ui->switch_lan_button->text() == "zh")
    {
        language_flag = 1;
        date_ui->switch_lan_button->setText("en");
    }
    else if (date_ui->switch_lan_button->text() == "en")
    {
        language_flag = 0;
        date_ui->switch_lan_button->setText("zh");
    }

    apply_language(language_flag);
    ui_extra_prompt = create_extra_prompt();
    emit ui2bot_language(language_flag);
    emit ui2tool_language(language_flag);
    emit ui2net_language(language_flag);
    emit ui2expend_language(language_flag);
}
//改变语种相关
void Widget::apply_language(int language_flag_)
{
    //主界面语种
    ui->load->setText(jtr("load"));
    ui->load->setToolTip(jtr("load_button_tooltip"));
    ui->date->setText(jtr("date"));
    ui->set->setToolTip(jtr("set"));
    ui->reset->setToolTip(jtr("reset"));
    ui->send->setText(jtr("send"));
    ui->send->setToolTip(jtr("send_tooltip"));
    cutscreen_dialog->initAction(jtr("save cut image"), jtr("svae screen image"));
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(std::thread::hardware_concurrency()));
    ui->mem_bar->setShowText(jtr("mem"));   //进度条里面的文本,强制重绘
    ui->vram_bar->setShowText(jtr("vram")); //进度条里面的文本,强制重绘
    ui->kv_bar->setShowText(jtr("brain"));  //进度条里面的文本,强制重绘
    ui->cpu_bar->show_text = "cpu ";        //进度条里面的文本
    ui->vcore_bar->show_text = "gpu ";      //进度条里面的文本
    //输入区右击菜单语种
    create_right_menu(); //添加右击问题
    create_tray_right_menu();
    // api设置语种
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_endpoint_label->setText(jtr("api endpoint"));
    api_endpoint_LineEdit->setPlaceholderText(jtr("input api endpoint"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    api_key_label->setText(jtr("api key"));
    api_key_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_key_LineEdit->setToolTip(jtr("input api key"));
    api_model_label->setText(jtr("api model"));
    api_model_LineEdit->setPlaceholderText(jtr("sd_vaepath_lineEdit_placeholder"));
    api_model_LineEdit->setToolTip(jtr("input api model"));
    //约定选项语种
    date_ui->prompt_box->setTitle(jtr("character")); //提示词模板设置区域
    date_ui->chattemplate_label->setText(jtr("chat template"));
    date_ui->chattemplate_label->setToolTip(jtr("chattemplate_label_tooltip"));
    date_ui->chattemplate_comboBox->setToolTip(jtr("chattemplate_label_tooltip"));
    date_ui->date_prompt_label->setText(jtr("date prompt"));
    date_ui->date_prompt_label->setToolTip(jtr("date_prompt_label_tooltip"));
    date_ui->date_prompt_TextEdit->setToolTip(jtr("date_prompt_label_tooltip"));
    date_ui->user_name_label->setText(jtr("user name"));
    date_ui->user_name_label->setToolTip(jtr("user_name_label_tooltip"));
    date_ui->user_name_LineEdit->setToolTip(jtr("user_name_label_tooltip"));
    date_ui->model_name_label->setText(jtr("model name"));
    date_ui->model_name_label->setToolTip(jtr("model_name_label_tooltip"));
    date_ui->model_name_LineEdit->setToolTip(jtr("model_name_label_tooltip"));
    date_ui->tool_box->setTitle(jtr("mount") + jtr("tool"));
    date_ui->calculator_checkbox->setText(jtr("calculator"));
    date_ui->calculator_checkbox->setToolTip(jtr("calculator_checkbox_tooltip"));
    date_ui->engineer_checkbox->setText(jtr("engineer"));
    date_ui->engineer_checkbox->setToolTip(jtr("engineer_checkbox_tooltip"));
    date_ui->controller_checkbox->setText(jtr("controller"));
    date_ui->controller_checkbox->setToolTip(jtr("controller_checkbox_tooltip"));
    date_ui->knowledge_checkbox->setText(jtr("knowledge"));
    date_ui->knowledge_checkbox->setToolTip(jtr("knowledge_checkbox_tooltip"));
    date_ui->MCPtools_checkbox->setText(jtr("MCPtools"));
    date_ui->MCPtools_checkbox->setToolTip(jtr("MCPtools_checkbox_tooltip"));
    date_ui->stablediffusion_checkbox->setText(jtr("stablediffusion"));
    date_ui->stablediffusion_checkbox->setToolTip(jtr("stablediffusion_checkbox_tooltip"));
    date_ui->switch_lan_button->setToolTip(jtr("switch_lan_button_tooltip"));
    date_ui->confirm_button->setText(jtr("ok"));
    date_ui->cancel_button->setText(jtr("cancel"));
    date_dialog->setWindowTitle(jtr("date"));
    //设置选项语种
    settings_ui->sample_box->setTitle(jtr("sample set")); //采样设置区域
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(ui_SETTINGS.temp));
    settings_ui->temp_label->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    settings_ui->temp_slider->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat));
    // TOP_K 与 并发数量 初始文本与提示
    settings_ui->topk_label->setText(jtr("top_k") + " " + QString::number(ui_SETTINGS.top_k));
    settings_ui->topk_slider->setToolTip(jtr("top_k_label_tooltip"));
    settings_ui->topk_label->setToolTip(jtr("top_k_label_tooltip"));
    settings_ui->parallel_label->setText(jtr("parallel") + " " + QString::number(ui_SETTINGS.hid_parallel));
    settings_ui->parallel_slider->setToolTip(jtr("parallel_label_tooltip"));
    settings_ui->parallel_label->setToolTip(jtr("parallel_label_tooltip"));
    settings_ui->repeat_label->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    settings_ui->repeat_slider->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    settings_ui->backend_box->setTitle(jtr("backend set")); //后端设置区域
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(ui_SETTINGS.ngl));
    settings_ui->ngl_label->setToolTip(jtr("put some model paragram to gpu and reload model"));
    settings_ui->ngl_slider->setToolTip(jtr("put some model paragram to gpu and reload model"));
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread));
    settings_ui->nthread_label->setToolTip(jtr("not big better"));
    settings_ui->nthread_slider->setToolTip(jtr("not big better"));
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(ui_SETTINGS.nctx));
    settings_ui->nctx_label->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    settings_ui->nctx_slider->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    settings_ui->lora_label->setText(jtr("load lora"));
    settings_ui->lora_label->setToolTip(jtr("lora_label_tooltip"));
    settings_ui->lora_LineEdit->setToolTip(jtr("lora_label_tooltip"));
    settings_ui->lora_LineEdit->setPlaceholderText(jtr("right click and choose lora"));
    settings_ui->mmproj_label->setText(jtr("load mmproj"));
    settings_ui->mmproj_label->setToolTip(jtr("mmproj_label_tooltip"));
    settings_ui->mmproj_LineEdit->setToolTip(jtr("mmproj_label_tooltip"));
    settings_ui->mmproj_LineEdit->setPlaceholderText(jtr("right click and choose mmproj"));
    settings_ui->mode_box->setTitle(jtr("state set")); //状态设置区域
    settings_ui->complete_btn->setText(jtr("complete state"));
    settings_ui->complete_btn->setToolTip(jtr("complete_btn_tooltip"));
    settings_ui->chat_btn->setText(jtr("chat state"));
    settings_ui->chat_btn->setToolTip(jtr("chat_btn_tooltip"));
    // 服务模式已移除
    settings_ui->port_label->setText(jtr("exposed port"));
    settings_ui->port_label->setToolTip(jtr("port_label_tooltip"));
    settings_ui->port_lineEdit->setToolTip(jtr("port_label_tooltip"));
    settings_ui->frame_label->setText(jtr("frame"));
    settings_ui->frame_label->setToolTip(jtr("frame_label_tooltip"));
    settings_ui->frame_lineEdit->setToolTip(jtr("frame_label_tooltip"));
    settings_ui->confirm->setText(jtr("ok"));
    settings_ui->cancel->setText(jtr("cancel"));
    settings_dialog->setWindowTitle(jtr("set"));
}

//创建临时文件夹EVA_TEMP
bool Widget::createTempDirectory(const QString &path)
{
    QDir dir;
    // 检查路径是否存在
    if (dir.exists(path))
    {
        return false;
    }
    else
    {
        // 尝试创建目录
        if (dir.mkpath(path))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

// 打开文件夹
QString Widget::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);
    return filepath;
}

//语音朗读相关 文转声相关

//每次约定和设置后都保存配置到本地
void Widget::auto_save_user()
{
    //--------------保存当前用户配置---------------
    // 创建 QSettings 对象，指定配置文件的名称和格式

    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    settings.setValue("ui_mode", ui_mode);         //机体模式
    settings.setValue("ui_state", ui_state);       //机体状态
    settings.setValue("shell", shell);             //shell路径
    settings.setValue("python", pythonExecutable); //python版本
    //保存设置参数
    settings.setValue("modelpath", ui_SETTINGS.modelpath); //模型路径
    settings.setValue("temp", ui_SETTINGS.temp);           //温度
    settings.setValue("repeat", ui_SETTINGS.repeat);       //惩罚系数
    settings.setValue("top_k", ui_SETTINGS.top_k);         // top-k 采样
    settings.setValue("ngl", ui_SETTINGS.ngl);             // gpu负载层数
    settings.setValue("nthread", ui_SETTINGS.nthread);     // cpu线程数
    settings.setValue("nctx", ui_SETTINGS.nctx);
    settings.setValue("mmprojpath", ui_SETTINGS.mmprojpath); //视觉
    settings.setValue("lorapath", ui_SETTINGS.lorapath);     // lora
    settings.setValue("monitor_frame", ui_monitor_frame);    // 监视帧率

    //保存隐藏设置
    settings.setValue("hid_npredict", ui_SETTINGS.hid_npredict); //最大输出长度
    settings.setValue("hid_special", ui_SETTINGS.hid_special);
    settings.setValue("hid_top_p", ui_SETTINGS.hid_top_p);
    settings.setValue("hid_batch", ui_SETTINGS.hid_batch);
    settings.setValue("hid_n_ubatch", ui_SETTINGS.hid_n_ubatch);
    settings.setValue("hid_use_mmap", ui_SETTINGS.hid_use_mmap);
    settings.setValue("hid_use_mlock", ui_SETTINGS.hid_use_mlock);
    settings.setValue("hid_flash_attn", ui_SETTINGS.hid_flash_attn);
    settings.setValue("hid_parallel", ui_SETTINGS.hid_parallel);
    settings.setValue("port", ui_port); //服务端口
    settings.setValue("device_backend", ui_device_backend); // 推理设备（auto/cpu/cuda/vulkan/opencl）
    //保存约定参数
    settings.setValue("chattemplate", date_ui->chattemplate_comboBox->currentText());              //对话模板
    settings.setValue("calculator_checkbox", date_ui->calculator_checkbox->isChecked());           //计算器工具
    settings.setValue("knowledge_checkbox", date_ui->knowledge_checkbox->isChecked());             // knowledge工具
    settings.setValue("controller_checkbox", date_ui->controller_checkbox->isChecked());           // controller工具
    settings.setValue("stablediffusion_checkbox", date_ui->stablediffusion_checkbox->isChecked()); //计算器工具
    settings.setValue("engineer_checkbox", date_ui->engineer_checkbox->isChecked());               // engineer工具
    settings.setValue("MCPtools_checkbox", date_ui->MCPtools_checkbox->isChecked());               // MCPtools工具
    settings.setValue("extra_lan", ui_extra_lan);                                                  //额外指令语种

    //保存自定义的约定模板
    settings.setValue("custom1_date_system", custom1_date_system);
    settings.setValue("custom1_user_name", custom1_user_name);
    settings.setValue("custom1_model_name", custom1_model_name);
    settings.setValue("custom2_date_system", custom2_date_system);
    settings.setValue("custom2_user_name", custom2_user_name);
    settings.setValue("custom2_model_name", custom2_model_name);

    //保存api参数
    settings.setValue("api_endpoint", apis.api_endpoint);
    settings.setValue("api_key", apis.api_key);
    settings.setValue("api_model", apis.api_model);

    reflash_state("ui:" + jtr("save_config_mess"), USUAL_SIGNAL);
}

//监视时间到
void Widget::monitorTime()
{
    // 这些情况不处理
    if (!is_load || is_run || ui_state != CHAT_STATE || ui_mode != LOCAL_MODE || is_monitor)
    {
        return;
    }
    is_monitor = true;
    QString filePath = saveScreen();
    emit ui2bot_monitor_filepath(filePath); //给模型发信号，能处理就处理
}

// 保存当前屏幕截图
QString Widget::saveScreen()
{
    QScreen *screen = QApplication::primaryScreen();

    // 获取屏幕几何信息
    QRect screenGeometry = screen->geometry();
    qreal devicePixelRatio = screen->devicePixelRatio();

    // qDebug() << "逻辑尺寸:" << screenGeometry.width() << screenGeometry.height();
    // qDebug() << "缩放比例:" << devicePixelRatio;

    // 直接使用 grabWindow 获取完整屏幕截图（会自动处理DPI）
    QPixmap m_screenPicture = screen->grabWindow(0);

    // qDebug() << "截图实际尺寸:" << m_screenPicture.width() << m_screenPicture.height();

    // 获取鼠标位置（使用逻辑坐标，不需要手动缩放）
    QPoint cursorPos = QCursor::pos();

    // 将逻辑坐标转换为截图中的物理坐标
    cursorPos.setX(cursorPos.x() * devicePixelRatio);
    cursorPos.setY(cursorPos.y() * devicePixelRatio);

    // 创建光标图标
    QPixmap cursorPixmap;

    // 尝试获取当前光标
    if (QApplication::overrideCursor())
    {
        cursorPixmap = QApplication::overrideCursor()->pixmap();
    }

    // 如果没有获取到光标，创建默认箭头光标
    if (cursorPixmap.isNull())
    {
        // 光标大小按DPI缩放
        int baseSize = 16;
        int cursorSize = baseSize * devicePixelRatio;

        cursorPixmap = QPixmap(cursorSize, cursorSize);
        cursorPixmap.fill(Qt::transparent);
        cursorPixmap.setDevicePixelRatio(devicePixelRatio);

        QPainter cursorPainter(&cursorPixmap);
        cursorPainter.setRenderHint(QPainter::Antialiasing);
        cursorPainter.setPen(QPen(Qt::black, 1));
        cursorPainter.setBrush(Qt::white);

        // 绘制箭头（使用逻辑坐标，QPainter会自动处理缩放）
        QPolygonF arrow;
        arrow << QPointF(0, 0) << QPointF(0, 10) << QPointF(3, 7)
              << QPointF(7, 11) << QPointF(9, 9)
              << QPointF(5, 5) << QPointF(10, 0);

        cursorPainter.drawPolygon(arrow);
        cursorPainter.end();
    }
    else
    {
        // 如果获取到了系统光标，确保它有正确的DPI设置
        cursorPixmap.setDevicePixelRatio(devicePixelRatio);
    }

    // 将光标绘制到截图上
    QPainter painter(&m_screenPicture);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制光标时，考虑到cursorPixmap可能已经有devicePixelRatio设置
    // 所以绘制位置需要调整
    QPoint drawPos = cursorPos;
    if (cursorPixmap.devicePixelRatio() > 1.0)
    {
        // 如果光标pixmap已经设置了devicePixelRatio，绘制位置需要相应调整
        drawPos.setX(cursorPos.x() / devicePixelRatio);
        drawPos.setY(cursorPos.y() / devicePixelRatio);
    }

    painter.drawPixmap(drawPos, cursorPixmap);
    painter.end();

    QImage image = m_screenPicture.toImage();

    // 逐步缩小图片直到尺寸 <= 1920x1080
    while (image.width() > 1920 || image.height() > 1080)
    {
        // 计算缩放比例，保持宽高比
        qreal scaleRatio = qMin(1920.0 / image.width(), 1080.0 / image.height());
        int newWidth = image.width() * scaleRatio;
        int newHeight = image.height() * scaleRatio;

        image = image.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
    QString filePath = QDir::currentPath() + "/EVA_TEMP/screen_cut/" + timestamp + ".png";
    createTempDirectory(QDir::currentPath() + "/EVA_TEMP/screen_cut");
    image.save(filePath);
    return filePath;
}

void Widget::recv_monitor_decode_ok()
{
    is_monitor = false; //解锁
}

//构建屏幕信息
QString Widget::create_screen_info()
{
    // 屏幕左上角坐标为(0,0) 右下角坐标为(x,y)
    QString info;
    QScreen *screen = QApplication::primaryScreen();

    // 使用物理像素尺寸，而不是逻辑像素
    QRect screenGeometry = screen->geometry();
    qreal devicePixelRatio = screen->devicePixelRatio();

    // 计算实际的物理像素尺寸
    int physicalWidth = screenGeometry.width() * devicePixelRatio;
    int physicalHeight = screenGeometry.height() * devicePixelRatio;

    info = QString("The coordinates of the top left corner of the screen are (0,0) and the coordinates of the bottom right corner are (%1, %2)")
               .arg(physicalWidth)
               .arg(physicalHeight);

    return info;
}


void Widget::openHistoryManager()
{
    if (!history_)
    {
        reflash_state(jtr("history db error"), WRONG_SIGNAL);
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(jtr("history sessions"));
    dlg.resize(600, 420);

    QVBoxLayout *v = new QVBoxLayout(&dlg);

    QDialog *d = &dlg;

    // search bar
    QLineEdit *search = new QLineEdit(&dlg);
    search->setPlaceholderText(jtr("search"));
    v->addWidget(search);

    // table
    QTableWidget *table = new QTableWidget(&dlg);
    table->setColumnCount(2);
    QStringList headers;
    headers << jtr("title");
    headers << jtr("time");
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->addWidget(table);

    auto fill = [&](const QString &filter) {
        table->setRowCount(0);
        auto items = history_->listRecent(1000);
        for (const auto &it : items)
        {
            const QString title = it.title.isEmpty() ? QStringLiteral("(untitled)") : it.title;
            const QString when = it.startedAt.toString("yyyy-MM-dd hh:mm");
            if (!filter.isEmpty())
            {
                const QString key = (title + " " + when).toLower();
                if (!key.contains(filter.toLower())) continue;
            }
            const int row = table->rowCount();
            table->insertRow(row);
            auto *c0 = new QTableWidgetItem(title);
            c0->setData(Qt::UserRole, it.id);
            auto *c1 = new QTableWidgetItem(when);
            table->setItem(row, 0, c0);
            table->setItem(row, 1, c1);
        }
        table->resizeColumnsToContents();
    };
    fill("");

    // buttons
    QHBoxLayout *h = new QHBoxLayout();
    QPushButton *restoreBtn = new QPushButton(jtr("restore"), &dlg);
    QPushButton *renameBtn = new QPushButton(jtr("rename"), &dlg);
    QPushButton *deleteBtn = new QPushButton(jtr("delete"), &dlg);
    QPushButton *clearBtn = new QPushButton(jtr("clear all history"), &dlg);
    QPushButton *closeBtn = new QPushButton(jtr("close"), &dlg);
    h->addWidget(restoreBtn);
    h->addWidget(renameBtn);
    h->addWidget(deleteBtn);
    h->addStretch(1);
    h->addWidget(clearBtn);
    h->addWidget(closeBtn);
    v->addLayout(h);

    auto currentId = [&]() -> QString {
        const auto ranges = table->selectedRanges();
        if (ranges.isEmpty()) return QString();
        const int row = ranges.first().topRow();
        if (!table->item(row, 0)) return QString();
        return table->item(row, 0)->data(Qt::UserRole).toString();
    };

    QObject::connect(search, &QLineEdit::textChanged, &dlg, [=](const QString &t) { fill(t); });

    QObject::connect(table, &QTableWidget::itemDoubleClicked, &dlg, [=](QTableWidgetItem *) {
        const QString id = currentId();
        if (id.isEmpty()) return;
        const_cast<Widget *>(this)->restoreSessionById(id);
        d->accept();
    });

    QObject::connect(restoreBtn, &QPushButton::clicked, &dlg, [=]() {
        const QString id = currentId();
        if (id.isEmpty()) return;
        const_cast<Widget *>(this)->restoreSessionById(id);
        d->accept();
    });

    QObject::connect(renameBtn, &QPushButton::clicked, &dlg, [=]() {
        const QString id = currentId();
        if (id.isEmpty()) return;
        bool ok = false;
        const QString t = QInputDialog::getText(d, jtr("rename"), jtr("new title"), QLineEdit::Normal, QString(), &ok);
        if (!ok) return;
        if (history_->renameSession(id, t))
        {
            // update table
            const auto ranges = table->selectedRanges();
            if (!ranges.isEmpty())
            {
                const int row = ranges.first().topRow();
                if (auto *c0 = table->item(row, 0)) c0->setText(t);
            }
            reflash_state(jtr("session title updated"), SUCCESS_SIGNAL);
        }
        else
        {
            reflash_state(jtr("history db error"), WRONG_SIGNAL);
        }
    });

    QObject::connect(deleteBtn, &QPushButton::clicked, &dlg, [=]() {
        const QString id = currentId();
        if (id.isEmpty()) return;
        auto btn = QMessageBox::question(d, jtr("delete"), jtr("confirm delete?"));
        if (btn != QMessageBox::Yes) return;
        if (history_->deleteSession(id))
        {
            fill(search->text());
            reflash_state(jtr("deleted"), SUCCESS_SIGNAL);
        }
        else
        {
            reflash_state(jtr("history db error"), WRONG_SIGNAL);
        }
    });

    QObject::connect(clearBtn, &QPushButton::clicked, &dlg, [=]() {
        auto btn = QMessageBox::question(d, jtr("clear all history"), jtr("confirm delete?"));
        if (btn != QMessageBox::Yes) return;
        if (history_->purgeAll())
        {
            fill(search->text());
            reflash_state(jtr("deleted"), SUCCESS_SIGNAL);
        }
        else
        {
            reflash_state(jtr("history db error"), WRONG_SIGNAL);
        }
    });

    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    dlg.exec();
}




