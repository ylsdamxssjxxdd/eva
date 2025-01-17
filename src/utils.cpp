//功能函数

#include "ui_widget.h"
#include "widget.h"

//手搓输出解析器，提取可能的JSON
QPair<QString, QString> Widget::JSONparser(QString text) {
    QPair<QString, QString> func_arg_list;
    // ----------匹配花括号中的内容----------
    QRegularExpression re;
    if (ui_interpreter_ischecked) {
        re.setPattern("\\{(.*)\\}");  // 如果挂载了代码解释器，匹配第一个 { 至最后一个 } 中的内容
    } else {
        re.setPattern("\\{(.*?)\\}");  // 其它情况，匹配第一个 { 至第一个 } 中的内容
    }

    re.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);  //允许包含换行符
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch()) {
        QString content = match.captured(1);  // 获取第一个捕获组的内容
        // qDebug() << "花括号中的内容是：" << content;
        // ----------匹配"action:"至逗号----------
        // \\s*的意思是允许忽略空格
        QRegularExpression re2("\"action_name\"\\s*[:：]\\s*\"([^\"]*)\"");
        QRegularExpressionMatch match2 = re2.match(content);
        if (match2.hasMatch()) {
            QString content2 = match2.captured(1);  // 获取第一个捕获组的内容
            func_arg_list.first = content2;
            // qDebug() << "action_name中的内容是：" << content2;
            // ----------匹配"action_input:"至最后----------
            QRegularExpression re3("\"action_input\"\\s*[:：]\\s*(.*)");
            re3.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);  //允许包含换行符
            QRegularExpressionMatch match3 = re3.match(content);
            if (match3.hasMatch()) {

                QString content3 = match3.captured(1).trimmed().replace("\\n", "\n");  // 获取第一个捕获组的内容

                //去除文本段前后的标点，并且过滤里面的内容
                if (!content3.isEmpty()) {
                    // 去除最前面的标点 { " ' ` }
                    while (content3.at(0) == QChar('`') || content3.at(0) == QChar('\"') || content3.at(0) == QChar('\'') || content3.at(0) == QChar('{')) {
                        content3 = content3.mid(1);
                    }

                    // 去除最前面的字段 python
                    while (content3.indexOf("python") == 0) {
                        content3 = content3.mid(6);  // 去除前 6 个字符, 即 "python"
                    }

                    // 去除最后面的标点 { " ' ` }
                    while (content3.at(content3.length() - 1) == QChar('`') || content3.at(content3.length() - 1) == QChar('\"') || content3.at(content3.length() - 1) == QChar('\'') || content3.at(content3.length() - 1) == QChar('}')) {
                        content3.chop(1);
                    }
                    // 替换所有的 \" 为 "
                    content3 = content3.replace("\\\"", "\"");

                }

                func_arg_list.second = content3;
                // qDebug() << "action_input中的内容是：" << content3;
            } else {
                // qDebug() << "没有找到action_input中的内容。";
            }

        } else {
            // qDebug() << "没有找到action_name中的内容。";
        }
    } else {
        // qDebug() << "没有找到花括号中的内容。";
    }

    // qDebug()<<func_arg_list;
    return func_arg_list;
}

//构建附加指令
QString Widget::create_extra_prompt() {
    QString extra_prompt_;

    extra_prompt_ = jtr("head_extra_prompt");
    if (is_load_tool) {
        //头
        if (interpreter_checkbox->isChecked()) {
            extra_prompt_ += tool_map["interpreter"].func_describe + "\n";
        }
        if (calculator_checkbox->isChecked()) {
            extra_prompt_ += tool_map["calculator"].func_describe + "\n";
        }
        if (terminal_checkbox->isChecked()) {
            extra_prompt_ += tool_map["terminal"].func_describe + "\n";
        }
        if (toolguy_checkbox->isChecked()) {
            extra_prompt_ += tool_map["toolguy"].func_describe + "\n";
        }
        if (knowledge_checkbox->isChecked()) {
            if (ui_syncrate_manager.is_sync) {
                extra_prompt_ += tool_map["knowledge"].func_describe + " " + jtr("embeddingdb describe") + ":" + jtr("embeddingdb_describe") + "\n";
            } else {
                extra_prompt_ += tool_map["knowledge"].func_describe + " " + jtr("embeddingdb describe") + ":" + embeddingdb_describe + "\n";
            }
        }
        if (stablediffusion_checkbox->isChecked()) {
            extra_prompt_ += tool_map["stablediffusion"].func_describe + "\n";
        }
        if (controller_checkbox->isChecked()) {
            extra_prompt_ += tool_map["controller"].func_describe + "\n";
        }
        //中
        extra_prompt_ += jtr("middle_extra_prompt");
        if (interpreter_checkbox->isChecked()) {
            extra_prompt_ += "\"interpreter\" ";
        }
        if (calculator_checkbox->isChecked()) {
            extra_prompt_ += "\"calculator\" ";
        }
        if (terminal_checkbox->isChecked()) {
            extra_prompt_ += "\"terminal\" ";
        }
        if (toolguy_checkbox->isChecked()) {
            extra_prompt_ += "\"toolguy\" ";
        }
        if (knowledge_checkbox->isChecked()) {
            extra_prompt_ += "\"knowledge\" ";
        }
        if (stablediffusion_checkbox->isChecked()) {
            extra_prompt_ += "\"stablediffusion\" ";
        }
        if (controller_checkbox->isChecked()) {
            extra_prompt_ += "\"controller\" ";
        }
        //尾
        extra_prompt_ += jtr("tail_extra_prompt");
    } else {
        extra_prompt_ = "";
    }
    return extra_prompt_;
}

//添加额外停止标志，本地模式时在xbot.cpp里已经现若同时包含"<|" 和 "|>"也停止
void Widget::addStopwords() {
    ui_DATES.extra_stop_words.clear();  //重置额外停止标志

    if (ui_DATES.is_load_tool)  //如果挂载了工具则增加额外停止标志
    {
        ui_DATES.extra_stop_words << "<|observation|>";
        ui_DATES.extra_stop_words << "observation:";
        ui_DATES.extra_stop_words << "observation：";
    }

}

//获取本机第一个ip地址
QString Widget::getFirstNonLoopbackIPv4Address() {
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (int i = 0; i < list.count(); i++) {
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol) {
            return list[i].toString();
        }
    }
    return QString();
}

//第三方程序开始
void Widget::server_onProcessStarted() {
    if (ui_SETTINGS.ngl == 0) {
        QApplication::setWindowIcon(QIcon(":/logo/connection-point-blue.png"));
    } else {
        QApplication::setWindowIcon(QIcon(":/logo/connection-point-green.png"));
    }
    ipAddress = getFirstNonLoopbackIPv4Address();
    reflash_state("ui:server " + jtr("oning"), SIGNAL_SIGNAL);
}

//第三方程序结束
void Widget::server_onProcessFinished() {
    if (current_server) {
        ui_state_info = "ui:" + jtr("old") + "server " + jtr("off");
        reflash_state(ui_state_info, SIGNAL_SIGNAL);
    } else {
        QApplication::setWindowIcon(QIcon(":/logo/dark_logo.png"));  //设置应用程序图标
        reflash_state("ui:server" + jtr("off"), SIGNAL_SIGNAL);
        ui_output = "\nserver" + jtr("shut down");
        output_scroll(ui_output);
    }
}

// llama-bench进程结束响应
void Widget::bench_onProcessFinished() { qDebug() << "llama-bench进程结束响应"; }

// 构建测试问题
void Widget::makeTestQuestion(QString dirPath) {
    getAllFiles(dirPath);
    for (int i = 0; i < filePathList.size(); ++i) {
        QString fileName = filePathList.at(i);
        readCsvFile(fileName);
    }
}

//显示文件名和图像
void Widget::showImage(QString imagepath) {
    ui_output = "\nfile:///" + imagepath + "\n";
    output_scroll(ui_output);

    // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
    QImage image(imagepath);
    int originalWidth = image.width() / devicePixelRatioF();
    int originalHeight = image.height() / devicePixelRatioF();

    QTextCursor cursor(ui->output->textCursor());
    cursor.movePosition(QTextCursor::End);

    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);    // 设置图片的宽度
    imageFormat.setHeight(originalHeight);  // 设置图片的高度
    imageFormat.setName(imagepath);         // 图片资源路径

    cursor.insertImage(imageFormat);
    //滚动到底部展示
    ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum());  //滚动条滚动到最下面
}

//开始录音
void Widget::recordAudio() {
    reflash_state("ui:" + jtr("recoding") + "... ");
    ui_state_recoding();

    audioRecorder.record();   // 在这之前检查是否可用
    audio_timer->start(100);  // 每隔100毫秒刷新一次输入区
}

// 每隔100毫秒刷新一次监视录音
void Widget::monitorAudioLevel() {
    audio_time += 100;
    ui_state_recoding();  //更新输入区
}

//停止录音
void Widget::stop_recordAudio() {
    QString wav_path = applicationDirPath + "/EVA_TEMP/" + QString("EVA_") + ".wav";
    is_recodering = false;
    audioRecorder.stop();
    audio_timer->stop();
    reflash_state("ui:" + jtr("recoding over") + " " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s");
    audio_time = 0;
    //将录制的wav文件重采样为16khz音频文件
#ifdef _WIN32
    QTextCodec* code = QTextCodec::codecForName("GB2312");  // mingw中文路径支持
    std::string wav_path_c = code->fromUnicode(wav_path).data();
#elif __linux__
    std::string wav_path_c = wav_path.toStdString();
#endif
    resampleWav(wav_path_c, wav_path_c);
    emit ui2expend_speechdecode(wav_path, "txt");  //传一个wav文件开始解码
}

// 清空题库
void Widget::clearQuestionlist() {
    test_score = 0;  // 答对的个数
    test_count = 0;  // 回答的次数
    filePathList.clear();
    test_list_answer.clear();
    test_list_question.clear();
}

//读取csv文件
void Widget::readCsvFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for reading:" << file.errorString();
        return;
    }
    QString questiontitle = jtr("question type") + ":" + fileName.split("/").last().split(".").at(0) + "\n\n";
    QTextStream in(&file);
    in.setCodec("UTF-8");                //要求csv文件的格式必须是utf-8 不能是ansi
    QString headerLine = in.readLine();  // 读取并忽略标题行
    bool inQuotes = false;
    QString currentField;
    QStringList currentRow;

    while (!in.atEnd()) {
        QString line = in.readLine();
        for (int i = 0; i < line.length(); ++i) {
            QChar currentChar = line[i];
            if (currentChar == '\"') {
                inQuotes = !inQuotes;  // Toggle the inQuotes state
            } else if (currentChar == ',' && !inQuotes) {
                // We've reached the end of a field
                currentRow.append(currentField);
                currentField.clear();
            } else {
                currentField += currentChar;
            }
        }
        if (!inQuotes) {
            // End of line and not in quotes, add the last field to the row
            currentRow.append(currentField);
            currentField.clear();

            if (currentRow.size() >= 7) {
                // 输出题目和答案
                // qDebug() << "id:" << currentRow.at(0).trimmed();
                // qDebug() << "Question:" << currentRow.at(1).trimmed();
                // qDebug() << "A:" << currentRow.at(2).trimmed();
                // qDebug() << "B:" << currentRow.at(3).trimmed();
                // qDebug() << "C:" << currentRow.at(4).trimmed();
                // qDebug() << "D:" << currentRow.at(5).trimmed();
                // qDebug() << "Answer:" << currentRow.at(6).trimmed();
                test_list_question << questiontitle + currentRow.at(1).trimmed() + "\n\n" + "A:" + currentRow.at(2).trimmed() + "\n" + "B:" + currentRow.at(3).trimmed() + "\n" + "C:" + currentRow.at(4).trimmed() + "\n" + "D:" + currentRow.at(5).trimmed() + "\n";
                test_list_answer << currentRow.at(6).trimmed();
            } else if (currentRow.size() == 6)  //题库没有序号的情况 针对mmlu
            {
                test_list_question << questiontitle + currentRow.at(0).trimmed() + "\n\n" + "A:" + currentRow.at(1).trimmed() + "\n" + "B:" + currentRow.at(2).trimmed() + "\n" + "C:" + currentRow.at(3).trimmed() + "\n" + "D:" + currentRow.at(4).trimmed() + "\n";
                test_list_answer << currentRow.at(5).trimmed();
            }

            currentRow.clear();  // Prepare for the next row
        } else {
            // Line ends but we're inside quotes, this means the field continues to the next line
            currentField += '\n';  // Add the newline character that was part of the field
        }
    }

    file.close();
}

void Widget::makeTestIndex() {
    test_question_index.clear();
    for (int i = 0; i < test_list_question.size(); ++i) {
        test_question_index << i;
    }
    // std::random_shuffle(test_question_index.begin(), test_question_index.end());//随机打乱顺序
}

//遍历文件
void Widget::getAllFiles(const QString& floderPath) {
    QDir folder(floderPath);
    folder.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList entries = folder.entryInfoList();

    for (const QFileInfo& entry : entries) {
        if (entry.isDir()) {
            childPathList.append(entry.filePath());
        } else if (entry.isFile()) {
            filePathList.append(entry.filePath());
        }
    }
}

//更新gpu内存使用率
void Widget::updateGpuStatus() { emit gpu_reflash(); }

//更新cpu内存使用率
void Widget::updateCpuStatus() { emit cpu_reflash(); }

//拯救中文
void Widget::getWords(QString json_file_path) {
    QFile jfile(json_file_path);
    if (!jfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for reading.";
        return;
    }

    QTextStream in(&jfile);
    in.setCodec("UTF-8");  // 确保使用UTF-8编码读取文件
    QString data = in.readAll();
    jfile.close();

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = doc.object();
    wordsObj = jsonObj["words"].toObject();
}

//切换额外指令的语言
void Widget::switch_lan_change() {
    if (switch_lan_button->text() == "zh") {
        language_flag = 1;
        switch_lan_button->setText("en");

    } else if (switch_lan_button->text() == "en") {
        language_flag = 0;
        switch_lan_button->setText("zh");
    }
    
    apply_language(language_flag);
    emit ui2bot_language(language_flag);
    emit ui2tool_language(language_flag);
    emit ui2net_language(language_flag);
    emit ui2expend_language(language_flag);
}
//改变语种相关
void Widget::apply_language(int language_flag_) {
    //主界面语种
    ui->load->setText(jtr("load"));
    ui->load->setToolTip(jtr("load_button_tooltip"));
    ui->date->setText(jtr("date"));
    ui->set->setToolTip(jtr("set"));
    ui->reset->setToolTip(jtr("reset"));
    ui->send->setText(jtr("send"));
    ui->send->setToolTip(jtr("send_tooltip"));
    cutscreen_dialog->init_action(jtr("save cut image"), jtr("svae screen image"));
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread") + "  " + QString::number(ui_SETTINGS.nthread) + "/" + QString::number(std::thread::hardware_concurrency()));
    ui->mem_bar->set_show_text(jtr("mem"));    //进度条里面的文本,强制重绘
    ui->vram_bar->set_show_text(jtr("vram"));  //进度条里面的文本,强制重绘
    ui->kv_bar->set_show_text(jtr("brain"));   //进度条里面的文本,强制重绘
    ui->cpu_bar->show_text = "cpu ";           //进度条里面的文本
    ui->vcore_bar->show_text = "gpu ";         //进度条里面的文本
    //输入区右击菜单语种
    create_right_menu();  //添加右击问题
    // api设置语种
    api_dialog->setWindowTitle(jtr("link") + jtr("set"));
    api_endpoint_label->setText(jtr("api endpoint"));
    api_endpoint_LineEdit->setPlaceholderText(jtr("input server ip"));
    api_endpoint_LineEdit->setToolTip(jtr("api endpoint tool tip"));
    //约定选项语种
    prompt_box->setTitle(jtr("prompt") + jtr("template"));  //提示词模板设置区域
    chattemplate_label->setText(jtr("chat template"));
    chattemplate_label->setToolTip(jtr("chattemplate_label_tooltip"));
    chattemplate_comboBox->setToolTip(jtr("chattemplate_label_tooltip"));
    date_prompt_label->setText(jtr("date prompt"));
    date_prompt_label->setToolTip(jtr("date_prompt_label_tooltip"));
    date_prompt_TextEdit->setToolTip(jtr("date_prompt_label_tooltip"));
    user_name_label->setText(jtr("user name"));
    user_name_label->setToolTip(jtr("user_name_label_tooltip"));
    user_name_LineEdit->setToolTip(jtr("user_name_label_tooltip"));
    model_name_label->setText(jtr("model name"));
    model_name_label->setToolTip(jtr("model_name_label_tooltip"));
    model_name_LineEdit->setToolTip(jtr("model_name_label_tooltip"));
    tool_box->setTitle(jtr("mount") + jtr("tool"));
    calculator_checkbox->setText(jtr("calculator"));
    calculator_checkbox->setToolTip(jtr("calculator_checkbox_tooltip"));
    terminal_checkbox->setText(jtr("terminal"));
    terminal_checkbox->setToolTip(jtr("terminal_checkbox_tooltip"));
    controller_checkbox->setText(jtr("controller"));
    controller_checkbox->setToolTip(jtr("controller_checkbox_tooltip"));
    knowledge_checkbox->setText(jtr("knowledge"));
    knowledge_checkbox->setToolTip(jtr("knowledge_checkbox_tooltip"));
    toolguy_checkbox->setText(jtr("toolguy"));
    toolguy_checkbox->setToolTip(jtr("toolguy_checkbox_tooltip"));
    stablediffusion_checkbox->setText(jtr("stablediffusion"));
    stablediffusion_checkbox->setToolTip(jtr("stablediffusion_checkbox_tooltip"));
    interpreter_checkbox->setText(jtr("interpreter"));
    interpreter_checkbox->setToolTip(jtr("interpreter_checkbox_tooltip"));
    extra_label->setText(jtr("extra calling"));
    extra_label->setToolTip(jtr("extra_label_tooltip"));
    switch_lan_button->setToolTip(jtr("switch_lan_button_tooltip"));
    extra_TextEdit->setPlaceholderText(jtr("extra_TextEdit_tooltip"));
    extra_TextEdit->setToolTip(jtr("extra_TextEdit_tooltip"));
    tool_map.clear();
    tool_map.insert("calculator", {jtr("calculator"), "calculator", jtr("calculator_func_describe")});
    tool_map.insert("terminal", {jtr("terminal"), "terminal", jtr("terminal_func_describe").replace("{OS}", OS)});
    tool_map.insert("toolguy", {jtr("toolguy"), "toolguy", jtr("toolguy_func_describe")});
    tool_map.insert("knowledge", {jtr("knowledge"), "knowledge", jtr("knowledge_func_describe")});
    tool_map.insert("controller", {jtr("controller"), "controller", jtr("controller_func_describe")});
    tool_map.insert("stablediffusion", {jtr("stablediffusion"), "stablediffusion", jtr("stablediffusion_func_describe")});
    tool_map.insert("interpreter", {jtr("interpreter"), "interpreter", jtr("interpreter_func_describe")});
    extra_TextEdit->setText(create_extra_prompt());  //构建附加指令
    date_dialog->setWindowTitle(jtr("date"));
    //设置选项语种
    sample_box->setTitle(jtr("sample set"));  //采样设置区域
    temp_label->setText(jtr("temperature") + " " + QString::number(ui_SETTINGS.temp));
    temp_label->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    temp_slider->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    repeat_label->setText(jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat));
    repeat_label->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    repeat_slider->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    npredict_label->setText(jtr("npredict") + " " + QString::number(ui_SETTINGS.npredict));
    npredict_label->setToolTip(jtr("The maximum number of tokens that the model can output in a single prediction process"));
    npredict_label->setMinimumWidth(100);
    npredict_slider->setToolTip(jtr("The maximum number of tokens that the model can output in a single prediction process"));
    decode_box->setTitle(jtr("decode set"));  //解码设置区域
    ngl_label->setText("gpu " + jtr("offload") + QString::number(ui_SETTINGS.ngl));
    ngl_label->setToolTip(jtr("put some model paragram to gpu and reload model"));
    ngl_label->setMinimumWidth(100);
    ngl_slider->setToolTip(jtr("put some model paragram to gpu and reload model"));
    nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread));
    nthread_label->setToolTip(jtr("not big better"));
    nthread_slider->setToolTip(jtr("not big better"));
    nctx_label->setText(jtr("brain size") + " " + QString::number(ui_SETTINGS.nctx));
    nctx_label->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    nctx_label->setMinimumWidth(100);
    nctx_slider->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    batch_label->setText(jtr("batch size") + " " + QString::number(ui_SETTINGS.batch));
    batch_label->setToolTip(jtr("The number of tokens processed simultaneously in one decoding"));
    batch_label->setMinimumWidth(100);
    batch_slider->setToolTip(jtr("The number of tokens processed simultaneously in one decoding"));
    lora_label->setText(jtr("load lora"));
    lora_label->setToolTip(jtr("lora_label_tooltip"));
    lora_LineEdit->setToolTip(jtr("lora_label_tooltip"));
    lora_LineEdit->setPlaceholderText(jtr("right click and choose lora"));
    mmproj_label->setText(jtr("load mmproj"));
    mmproj_label->setToolTip(jtr("mmproj_label_tooltip"));
    mmproj_LineEdit->setToolTip(jtr("mmproj_label_tooltip"));
    mmproj_LineEdit->setPlaceholderText(jtr("right click and choose mmproj"));
    mode_box->setTitle(jtr("state set"));  //状态设置区域
    complete_btn->setText(jtr("complete state"));
    complete_btn->setToolTip(jtr("complete_btn_tooltip"));
    chat_btn->setText(jtr("chat state"));
    chat_btn->setToolTip(jtr("chat_btn_tooltip"));
    web_btn->setText(jtr("server state"));
    web_btn->setToolTip(jtr("web_btn_tooltip"));
    port_label->setText(jtr("port"));
    port_label->setToolTip(jtr("port_label_tooltip"));
    port_lineEdit->setToolTip(jtr("port_label_tooltip"));
    set_dialog->setWindowTitle(jtr("set"));
}

QString Widget::makeHelpInput() {
    QString help_input;

    for (int i = 1; i < 3; ++i)  // 2个
    {
        help_input = help_input + DEFAULT_SPLITER;  //前缀
        help_input = help_input + jtr(QString("H%1").arg(i)) + "\n";            //问题
        help_input = help_input + DEFAULT_SPLITER;  //后缀
        help_input = help_input + jtr(QString("A%1").arg(i)) + "\n";            //答案
    }

    return help_input;
}

//创建临时文件夹EVA_TEMP
bool Widget::createTempDirectory(const QString& path) {
    QDir dir;
    // 检查路径是否存在
    if (dir.exists(path)) {
        return false;
    } else {
        // 尝试创建目录
        if (dir.mkpath(path)) {
            return true;
        } else {
            return false;
        }
    }
}

// 打开文件夹
QString Widget::customOpenfile(QString dirpath, QString describe, QString format) {
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);
    return filepath;
}

//语音朗读相关 文转声相关



//每次约定和设置后都保存配置到本地
void Widget::auto_save_user() {
    //--------------保存当前用户配置---------------
    // 创建 QSettings 对象，指定配置文件的名称和格式

    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    //保存设置参数
    settings.setValue("modelpath", ui_SETTINGS.modelpath);  //模型路径
    settings.setValue("temp", ui_SETTINGS.temp);            //惩罚系数
    settings.setValue("repeat", ui_SETTINGS.repeat);        //惩罚系数
    settings.setValue("npredict", ui_SETTINGS.npredict);    //最大输出长度
    settings.setValue("ngl", ui_SETTINGS.ngl);              // gpu负载层数
    settings.setValue("nthread", ui_SETTINGS.nthread);      // cpu线程数
    if (ui_SETTINGS.nctx > ui_n_ctx_train) {
        settings.setValue("nctx", ui_n_ctx_train);
    }  //防止溢出
    else {
        settings.setValue("nctx", ui_SETTINGS.nctx);
    }
    settings.setValue("batch", ui_SETTINGS.batch);            //批大小
    settings.setValue("mmprojpath", ui_SETTINGS.mmprojpath);  //视觉
    settings.setValue("lorapath", ui_SETTINGS.lorapath);      // lora
    settings.setValue("ui_state", ui_state);                  //模式
    settings.setValue("port", ui_port);                       //服务端口

    //保存约定参数
    settings.setValue("chattemplate", chattemplate_comboBox->currentText());               //对话模板
    settings.setValue("date_prompt_prompt", date_prompt_TextEdit->toPlainText());                    //系统指令
    settings.setValue("extra_prompt", extra_TextEdit->toPlainText());                      //额外指令
    settings.setValue("user_name", ui_DATES.user_name);                                    //用户昵称
    settings.setValue("model_name", ui_DATES.model_name);                                    //模型昵称
    settings.setValue("calculator_checkbox", calculator_checkbox->isChecked());            //计算器工具
    settings.setValue("terminal_checkbox", terminal_checkbox->isChecked());                // terminal工具
    settings.setValue("knowledge_checkbox", knowledge_checkbox->isChecked());              // knowledge工具
    settings.setValue("controller_checkbox", controller_checkbox->isChecked());            // controller工具
    settings.setValue("stablediffusion_checkbox", stablediffusion_checkbox->isChecked());  //计算器工具
    settings.setValue("toolguy_checkbox", toolguy_checkbox->isChecked());                  // toolguy工具
    settings.setValue("interpreter_checkbox", interpreter_checkbox->isChecked());          // toolguy工具
    settings.setValue("extra_lan", ui_extra_lan);                                          //额外指令语种

    //保存自定义的约定模板
    settings.setValue("custom1_date_system", custom1_date_system);
    settings.setValue("custom1_user_name", custom1_user_name);
    settings.setValue("custom1_model_name", custom1_model_name);
    settings.setValue("custom2_date_system", custom2_date_system);
    settings.setValue("custom2_user_name", custom2_user_name);
    settings.setValue("custom2_model_name", custom2_model_name);

    reflash_state("ui:" + jtr("save_config_mess"), USUAL_SIGNAL);
}

