//功能函数

#include "widget.h"
#include "ui_widget.h"

//手搓输出解析器，提取可能的JSON
QPair<QString, QString> Widget::JSONparser(QString text) {
    
    QPair<QString, QString> func_arg_list;
    // ----------匹配花括号中的内容----------
    QRegularExpression re;
    if(ui_interpreter_ischecked)
    {
        re.setPattern("\\{(.*)\\}"); // 如果挂载了代码解释器，匹配第一个 { 至最后一个 } 中的内容
    }
    else
    {
        re.setPattern("\\{(.*?)\\}"); // 其它情况，匹配第一个 { 至第一个 } 中的内容
    }
    
    re.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);//允许包含换行符
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch())
    {
        QString content = match.captured(1);  // 获取第一个捕获组的内容
        qDebug() << "花括号中的内容是：" << content;
        // ----------匹配"action:"至逗号----------
        // \\s*的意思是允许忽略空格
        QRegularExpression re2("\"action\"\\s*[:：]\\s*\"([^\"]*)\"");
        QRegularExpressionMatch match2 = re2.match(content);
        if (match2.hasMatch())
        {
            QString content2 = match2.captured(1);  // 获取第一个捕获组的内容
            func_arg_list.first = content2;
            qDebug() << "action中的内容是：" << content2;
            // ----------匹配"action_input:"至最后----------
            QRegularExpression re3("\"action_input\"\\s*[:：]\\s*(.*)");
            re3.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);//允许包含换行符
            QRegularExpressionMatch match3 = re3.match(content);
            if (match3.hasMatch())
            {
                QString content3 = match3.captured(1).trimmed().replace("\\n", "\n");;  // 获取第一个捕获组的内容
                
                //去除文本段前后的标点
                if(!content3.isEmpty())
                {
                    // 去除最前面的标点 { " ' ` }
                    while (content3.at(0) == QChar('`') || content3.at(0) == QChar('\"') || content3.at(0) == QChar('\'') || content3.at(0) == QChar('{')) {
                        content3 = content3.mid(1);
                    }

                    // 去除最前面的字段 python
                    while (content3.indexOf("python") == 0) {
                        content3 = content3.mid(5); // 去除前 5 个字符, 即 "python"
                    }

                    // 去除最后面的标点 { " ' ` }
                    while (content3.at(content3.length() - 1) == QChar('`') || content3.at(content3.length() - 1) == QChar('\"') || content3.at(content3.length() - 1) == QChar('\'') || content3.at(content3.length() - 1) == QChar('}')) {
                        content3.chop(1);
                    }
                }
                

                func_arg_list.second = content3;
                qDebug() << "action_input中的内容是：" << content3;
            }
            else
            {
                qDebug() << "没有找到action_input中的内容。";
            }

        }
        else {
            qDebug() << "没有找到action中的内容。";
        }
    }
    else {
        qDebug() << "没有找到花括号中的内容。";
    }


    qDebug()<<func_arg_list;
    return func_arg_list;
}

//构建附加指令
QString Widget::create_extra_prompt()
{
    QString extra_prompt_;

    extra_prompt_ = jtr("head_extra_prompt");
    if(is_load_tool)
    {
        //头
        if(interpreter_checkbox->isChecked()){extra_prompt_ += tool_map["interpreter"].func_describe + "\n";}
        if(calculator_checkbox->isChecked()){extra_prompt_ += tool_map["calculator"].func_describe + "\n";}
        if(cmd_checkbox->isChecked()){extra_prompt_ += tool_map["cmd"].func_describe + "\n";}
        if(toolguy_checkbox->isChecked()){extra_prompt_ += tool_map["toolguy"].func_describe + "\n";}
        if(knowledge_checkbox->isChecked()){extra_prompt_ += tool_map["knowledge"].func_describe + " " + jtr("embeddingdb describe") + ":" + embeddingdb_describe + "\n";}
        if(stablediffusion_checkbox->isChecked()){extra_prompt_ += tool_map["stablediffusion"].func_describe + "\n";}
        if(controller_checkbox->isChecked()){extra_prompt_ += tool_map["controller"].func_describe + "\n";}
        //中
        extra_prompt_ +=jtr("middle_extra_prompt");
        if(interpreter_checkbox->isChecked()){extra_prompt_ +="\"interpreter\" ";}
        if(calculator_checkbox->isChecked()){extra_prompt_ += "\"calculator\" ";}
        if(cmd_checkbox->isChecked()){extra_prompt_ += "\"cmd\" ";}
        if(toolguy_checkbox->isChecked()){extra_prompt_ += "\"toolguy\" ";}
        if(knowledge_checkbox->isChecked()){extra_prompt_ +="\"knowledge\" ";}
        if(stablediffusion_checkbox->isChecked()){extra_prompt_ +="\"stablediffusion\" ";}
        if(controller_checkbox->isChecked()){extra_prompt_ +="\"controller\" ";}
        //尾
        extra_prompt_ += jtr("tail_extra_prompt");
    }
    else{extra_prompt_ = "";}
    return extra_prompt_;
}

//添加额外停止标志
void Widget::addStopwords()
{
    ui_DATES.extra_stop_words.clear();//重置额外停止标志
    ui_DATES.extra_stop_words << ui_DATES.input_pfx.toLower() + ":";//默认第一个是用户昵称，检测出来后下次回答将不再添加前缀
    ui_DATES.extra_stop_words << ui_DATES.input_sfx.toLower() + ":";//可以说相当严格了
    ui_DATES.extra_stop_words << ui_DATES.input_pfx.toLower() + "：";//可以说相当严格了
    ui_DATES.extra_stop_words << ui_DATES.input_sfx.toLower() + "：";//可以说相当严格了
    ui_DATES.extra_stop_words << "<|im_end|>";//可以说相当严格了, 针对某些模型

    if(ui_DATES.is_load_tool)//如果挂载了工具则增加额外停止标志
    {
        ui_DATES.extra_stop_words << "observation:";//可以说相当严格了
        ui_DATES.extra_stop_words << "observation：";//可以说相当严格了
        ui_DATES.extra_stop_words << "观察:";//可以说相当严格了
        ui_DATES.extra_stop_words << "观察：";//可以说相当严格了
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
void Widget::server_onProcessStarted()
{
    if(ui_SETTINGS.ngl==0){QApplication::setWindowIcon(QIcon(":/ui/connection-point-blue.png"));}
    else{QApplication::setWindowIcon(QIcon(":/ui/connection-point-green.png"));}
    ipAddress = getFirstNonLoopbackIPv4Address();
    reflash_state("ui:server " + jtr("oning"),SIGNAL_);
}

//第三方程序结束
void Widget::server_onProcessFinished()
{
    if(current_server)
    {
        ui_state = "ui:"+ jtr("old") + "server " + jtr("off");reflash_state(ui_state,SIGNAL_);
    }
    else
    {
        QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
        reflash_state("ui:server"+jtr("off"),SIGNAL_);
        ui_output = "\nserver"+jtr("shut down");
        output_scroll(ui_output);
    }
}

// 构建测试问题
void Widget::makeTestQuestion(QString dirPath)
{
    getAllFiles(dirPath);
    for(int i=0;i<filePathList.size();++i)
    {
        QString fileName = filePathList.at(i);
        readCsvFile(fileName);
    }
}

//显示文件名和图像
void Widget::showImage(QString imagepath)
{
    ui_output = "\nfile:///" + imagepath + "\n";
    output_scroll(ui_output);
    
    // 加载图片以获取其原始尺寸,由于qtextedit在显示时会按软件的系数对图片进行缩放,所以除回来
    QImage image(imagepath);
    int originalWidth = image.width()/devicePixelRatioF();
    int originalHeight = image.height()/devicePixelRatioF();

    QTextCursor cursor(ui->output->textCursor());
    cursor.movePosition(QTextCursor::End);

    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);  // 设置图片的宽度
    imageFormat.setHeight(originalHeight); // 设置图片的高度
    imageFormat.setName(imagepath);  // 图片资源路径

    cursor.insertImage(imageFormat);
    //滚动到底部展示
    ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum());//滚动条滚动到最下面

}

//开始录音
void Widget::recordAudio()
{
    reflash_state("ui:" + jtr("recoding") + "... ");
    ui_state_recoding();
    //本来用QAudioRecorder会很方便但是不能设置采样率为16000HZ...
    QAudioFormat audioFormat;
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setChannelCount(1);
    audioFormat.setCodec("audio/pcm");
    audioFormat.setSampleRate(16000);
    audioFormat.setSampleSize(16);
    audioFormat.setSampleType(QAudioFormat::SignedInt);
    //判断设备，查看是否存在
    QAudioDeviceInfo devInfo = QAudioDeviceInfo::defaultInputDevice();
    //不支持格式，使用最接近格式
    if(!devInfo.isFormatSupported(audioFormat)){ //当前使用设备是否支持
        audioFormat = devInfo.nearestFormat(audioFormat); //转换为最接近格式
    }
    _audioInput = new QAudioInput(devInfo,audioFormat,this);
    createTempDirectory("./EVA_TEMP");
    QString audiopath = "./EVA_TEMP/" + QString("EVA_") + ".wav";
    outFilePath = qApp->applicationDirPath() + audiopath;
    outFile.setFileName(outFilePath); //语音原始文件
    outFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    _audioInput->start(&outFile);
    audio_timer->start(100);  // 每隔100毫秒刷新一次输入区
}

// 每隔100毫秒刷新一次监视录音
void Widget::monitorAudioLevel()
{
    audio_time += 100;
    ui_state_recoding();//更新输入区
}

//停止录音
void Widget::stop_recordAudio()
{
    is_recodering = false;
    QIODevice *device{nullptr};
    device = &outFile;
//添加wav文件头
    static WAVHEADER wavHeader;
    qstrcpy(wavHeader.RiffName,"RIFF");
    qstrcpy(wavHeader.WavName,"WAVE");
    qstrcpy(wavHeader.FmtName,"fmt ");
    qstrcpy(wavHeader.DATANAME,"data");
    wavHeader.nFmtLength = 16;
    int nAudioFormat = 1;
    wavHeader.nAudioFormat = nAudioFormat;
    wavHeader.nBitsPerSample = 16;
    wavHeader.nChannleNumber = 1;
    wavHeader.nSampleRate = 16000;
    wavHeader.nBytesPerSample = wavHeader.nChannleNumber * wavHeader.nBitsPerSample / 8;
    wavHeader.nBytesPerSecond = wavHeader.nSampleRate * wavHeader.nChannleNumber *  wavHeader.nBitsPerSample / 8;
    wavHeader.nRiffLength = device->size() - 8 + sizeof(WAVHEADER);
    wavHeader.nDataLength = device->size();
//写到IO设备头
    device->seek(0);
    device->write(reinterpret_cast<char*>(&wavHeader),sizeof(WAVHEADER));

    _audioInput->stop();
    audio_timer->stop();
    outFile.close();
    reflash_state("ui:" + jtr("recoding over") + " " + QString::number(float(audio_time)/1000.0,'f',2) + "s");
    audio_time = 0;
    
    emit ui2expend_voicedecode("./EVA_TEMP/" + QString("EVA_") + ".wav", "txt");//传一个wav文件开始解码
}

// 清空题库
void Widget::clearQuestionlist()
{
    filePathList.clear();
    test_list_answer.clear();
    test_list_question.clear();
}

//读取csv文件
void Widget::readCsvFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Cannot open file for reading:" << file.errorString();
        return;
    }
    QString questiontitle = jtr("question type") + ":" + fileName.split("/").last().split(".").at(0) + "\n\n";
    QTextStream in(&file);
    in.setCodec("UTF-8");//要求csv文件的格式必须是utf-8 不能是ansi
    QString headerLine = in.readLine();// 读取并忽略标题行
    bool inQuotes = false;
    QString currentField;
    QStringList currentRow;

    while (!in.atEnd()) {
        QString line = in.readLine();
        for (int i = 0; i < line.length(); ++i) {
            QChar currentChar = line[i];
            if (currentChar == '\"') {
                inQuotes = !inQuotes; // Toggle the inQuotes state
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

            if(currentRow.size() >= 7)
            {
                // 输出题目和答案
                // qDebug() << "id:" << currentRow.at(0).trimmed();
                // qDebug() << "Question:" << currentRow.at(1).trimmed();
                // qDebug() << "A:" << currentRow.at(2).trimmed();
                // qDebug() << "B:" << currentRow.at(3).trimmed();
                // qDebug() << "C:" << currentRow.at(4).trimmed();
                // qDebug() << "D:" << currentRow.at(5).trimmed();
                // qDebug() << "Answer:" << currentRow.at(6).trimmed();
                test_list_question<<questiontitle +currentRow.at(1).trimmed()+"\n\n"+"A:" + currentRow.at(2).trimmed()+"\n"+"B:"+currentRow.at(3).trimmed()+"\n"+"C:"+currentRow.at(4).trimmed()+"\n"+"D:"+currentRow.at(5).trimmed()+"\n";
                test_list_answer<<currentRow.at(6).trimmed();
            }
            else if(currentRow.size() == 6)//题库没有序号的情况 针对mmlu
            {
                test_list_question<<questiontitle +currentRow.at(0).trimmed()+"\n\n"+"A:" + currentRow.at(1).trimmed()+"\n"+"B:"+currentRow.at(2).trimmed()+"\n"+"C:"+currentRow.at(3).trimmed()+"\n"+"D:"+currentRow.at(4).trimmed()+"\n";
                test_list_answer<<currentRow.at(5).trimmed();
            }

            currentRow.clear(); // Prepare for the next row
        } else {
            // Line ends but we're inside quotes, this means the field continues to the next line
            currentField += '\n'; // Add the newline character that was part of the field
        }
    }

    file.close();

}

void Widget::makeTestIndex()
{
    test_question_index.clear();
    for(int i =0;i<test_list_question.size();++i)
    {
        test_question_index<<i;
    }
    //std::random_shuffle(test_question_index.begin(), test_question_index.end());//随机打乱顺序
}

//遍历文件
void Widget::getAllFiles(const QString&floderPath)
{
    QDir folder(floderPath);
    folder.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList entries = folder.entryInfoList();

    for(const QFileInfo&entry : entries)
    {
        if(entry.isDir())
        {
            childPathList.append(entry.filePath());
        }
        else if(entry.isFile())
        {
            filePathList.append(entry.filePath());
        }
    }
}

double Widget::CalculateCPULoad()
{
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        // 获取系统时间失败
        return -1;
    }

    ULARGE_INTEGER idle, kernel, user;
    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;

    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;

    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    // Convert previous FILETIME values to ULARGE_INTEGER.
    ULARGE_INTEGER prevIdle, prevKernel, prevUser;
    prevIdle.LowPart = preidleTime.dwLowDateTime;
    prevIdle.HighPart = preidleTime.dwHighDateTime;

    prevKernel.LowPart = prekernelTime.dwLowDateTime;
    prevKernel.HighPart = prekernelTime.dwHighDateTime;

    prevUser.LowPart = preuserTime.dwLowDateTime;
    prevUser.HighPart = preuserTime.dwHighDateTime;

    // Calculate the differences between the previous and current times.
    ULARGE_INTEGER sysIdle, sysKernel, sysUser;
    sysIdle.QuadPart = idle.QuadPart - prevIdle.QuadPart;
    sysKernel.QuadPart = kernel.QuadPart - prevKernel.QuadPart;
    sysUser.QuadPart = user.QuadPart - prevUser.QuadPart;

    // Update the stored times for the next calculation.
    preidleTime = idleTime;
    prekernelTime = kernelTime;
    preuserTime = userTime;

    // Avoid division by zero.
    if (sysKernel.QuadPart + sysUser.QuadPart == 0) {
        return 0;
    }

    // Calculate the CPU load as a percentage.
    return (sysKernel.QuadPart + sysUser.QuadPart - sysIdle.QuadPart) * 100.0 / (sysKernel.QuadPart + sysUser.QuadPart);
}


//更新cpu内存使用率
void Widget::updateStatus()
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
    double physMemUsedPercent = (physMemUsed * 100.0) / totalPhysMem;// 计算内存使用率
    double cpuLoad = CalculateCPULoad();// 计算cpu使用率
    ui->cpu_bar->setValue(cpuLoad);
    //取巧,用第一次内存作为基准,模型占的内存就是当前多出来的内存,因为模型占的内存存在泄露不好测
    if(is_first_getmem){first_memp = physMemUsedPercent;ui->mem_bar->setValue(first_memp);is_first_getmem=false;}
    ui->mem_bar->setSecondValue(physMemUsedPercent - first_memp);
    //ui->mem_bar->setValue(physMemUsedPercent-(model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    //ui->mem_bar->setSecondValue((model_memusage.toFloat() + ctx_memusage.toFloat())*100 *1024*1024 / totalPhysMem);
    
}

//拯救中文
void Widget::getWords(QString json_file_path)
{
    
    QFile jfile(json_file_path);
    if (!jfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
    if(switch_lan_button->text()=="zh")
    {
        language_flag = 1;
        switch_lan_button->setText("en");
        
    }
    else if(switch_lan_button->text()=="en")
    {
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
void Widget::apply_language(int language_flag_)
{
    //主界面语种
    ui->load->setText(jtr("load"));
    ui->load->setToolTip(jtr("load_button_tooltip"));
    ui->date->setText(jtr("date"));
    ui->set->setToolTip(jtr("set"));
    ui->reset->setToolTip(jtr("reset"));
    if(!is_debug){ui->send->setText(jtr("send"));}
    ui->send->setToolTip(jtr("send_tooltip"));
    cutscreen_dialog->init_action(jtr("save cut image"), jtr("svae screen image"));
    ui->cpu_bar->setToolTip(jtr("nthread/maxthread")+"  "+QString::number(ui_SETTINGS.nthread)+"/"+QString::number(std::thread::hardware_concurrency()));
    ui->mem_bar->set_show_text(jtr("mem"));//进度条里面的文本,强制重绘
    ui->vram_bar->set_show_text(jtr("vram"));//进度条里面的文本,强制重绘
    ui->kv_bar->set_show_text(jtr("brain"));//进度条里面的文本,强制重绘
    ui->cpu_bar->show_text = "cpu ";//进度条里面的文本
    ui->vcore_bar->show_text = "gpu ";//进度条里面的文本
    //输入区右击菜单语种
    create_right_menu();//添加右击问题
    //api设置语种
    api_dialog->setWindowTitle("api" + jtr("set"));
    api_ip_label->setText("api " + jtr("address"));
    api_ip_LineEdit->setPlaceholderText(jtr("input server ip"));
    api_port_label->setText("api " + jtr("port"));
    api_chat_label->setText(jtr("chat")+jtr("endpoint"));
    api_complete_label->setText(jtr("complete")+jtr("endpoint"));
    //约定选项语种
    prompt_box->setTitle(jtr("prompt") + jtr("template"));//提示词模板设置区域
    chattemplate_label->setText(jtr("chat template"));
    chattemplate_label->setToolTip(jtr("chattemplate_label_tooltip"));
    chattemplate_comboBox->setToolTip(jtr("chattemplate_label_tooltip"));
    system_label->setText(jtr("system calling"));
    system_label->setToolTip(jtr("system_label_tooltip"));
    system_TextEdit->setToolTip(jtr("system_label_tooltip"));
    input_pfx_label->setText(jtr("user name"));
    input_pfx_label->setToolTip(jtr("input_pfx_label_tooltip"));
    input_pfx_LineEdit->setToolTip(jtr("input_pfx_label_tooltip"));
    input_sfx_label->setText(jtr("bot name"));
    input_sfx_label->setToolTip(jtr("input_sfx_label_tooltip"));
    input_sfx_LineEdit->setToolTip(jtr("input_sfx_label_tooltip"));
    tool_box->setTitle(jtr("mount") + jtr("tool"));
    calculator_checkbox->setText(jtr("calculator"));
    calculator_checkbox->setToolTip(jtr("calculator_checkbox_tooltip"));
    cmd_checkbox->setText(jtr("cmd"));
    cmd_checkbox->setToolTip(jtr("cmd_checkbox_tooltip"));
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
    tool_map.insert("calculator", {jtr("calculator"),"calculator",jtr("calculator_func_describe")});
    tool_map.insert("cmd", {jtr("cmd"),"cmd",jtr("cmd_func_describe")});
    tool_map.insert("toolguy", {jtr("toolguy"),"toolguy",jtr("toolguy_func_describe")});
    tool_map.insert("knowledge", {jtr("knowledge"),"knowledge",jtr("knowledge_func_describe")});
    tool_map.insert("controller", {jtr("controller"),"controller",jtr("controller_func_describe")});
    tool_map.insert("stablediffusion", {jtr("stablediffusion"),"stablediffusion",jtr("stablediffusion_func_describe")});
    tool_map.insert("interpreter", {jtr("interpreter"),"interpreter",jtr("interpreter_func_describe")});
    extra_TextEdit->setText(create_extra_prompt());//构建附加指令
    date_dialog->setWindowTitle(jtr("date"));
    //设置选项语种
    sample_box->setTitle(jtr("sample set"));//采样设置区域
    temp_label->setText(jtr("temperature")+" " + QString::number(ui_SETTINGS.temp));
    temp_label->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    temp_slider->setToolTip(jtr("The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"));
    repeat_label->setText(jtr("repeat") + " " + QString::number(ui_SETTINGS.repeat));
    repeat_label->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    repeat_slider->setToolTip(jtr("Reduce the probability of the model outputting synonymous words"));
    npredict_label->setText(jtr("npredict") + " " + QString::number(ui_SETTINGS.npredict));
    npredict_label->setToolTip(jtr("The maximum number of tokens that the model can output in a single prediction process"));npredict_label->setMinimumWidth(100);
    npredict_slider->setToolTip(jtr("The maximum number of tokens that the model can output in a single prediction process"));
    decode_box->setTitle(jtr("decode set"));//解码设置区域
#if defined(BODY_USE_VULKAN) || defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUDA)
    ngl_label->setText("gpu " + jtr("offload") + QString::number(ui_SETTINGS.ngl));
    ngl_label->setToolTip(jtr("put some model paragram to gpu and reload model"));ngl_label->setMinimumWidth(100);
    ngl_slider->setToolTip(jtr("put some model paragram to gpu and reload model"));
#endif
    nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(ui_SETTINGS.nthread));
    nthread_label->setToolTip(jtr("not big better"));
    nthread_slider->setToolTip(jtr("not big better"));
    nctx_label->setText(jtr("brain size")+" " + QString::number(ui_SETTINGS.nctx));
    nctx_label->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));nctx_label->setMinimumWidth(100);
    nctx_slider->setToolTip(jtr("ctx") + jtr("length") + "," + jtr("big brain size lead small wisdom"));
    batch_label->setText(jtr("batch size") + " " + QString::number(ui_SETTINGS.batch));
    batch_label->setToolTip(jtr("The number of tokens processed simultaneously in one decoding"));batch_label->setMinimumWidth(100);
    batch_slider->setToolTip(jtr("The number of tokens processed simultaneously in one decoding"));
    lora_label->setText(jtr("load lora"));
    lora_label->setToolTip(jtr("lora_label_tooltip"));
    lora_LineEdit->setToolTip(jtr("lora_label_tooltip"));
    lora_LineEdit->setPlaceholderText(jtr("right click and choose lora"));
    mmproj_label->setText(jtr("load mmproj"));
    mmproj_label->setToolTip(jtr("mmproj_label_tooltip"));
    mmproj_LineEdit->setToolTip(jtr("mmproj_label_tooltip"));
    mmproj_LineEdit->setPlaceholderText(jtr("right click and choose mmproj"));
    mode_box->setTitle(jtr("mode set"));//模式设置区域
    complete_btn->setText(jtr("complete mode"));
    complete_btn->setToolTip(jtr("complete_btn_tooltip"));
    chat_btn->setText(jtr("chat mode"));
    chat_btn->setToolTip(jtr("chat_btn_tooltip"));
    web_btn->setText(jtr("server mode"));
    web_btn->setToolTip(jtr("web_btn_tooltip"));
    port_label->setText(jtr("port"));
    port_label->setToolTip(jtr("port_label_tooltip"));
    port_lineEdit->setToolTip(jtr("port_label_tooltip"));
    set_dialog->setWindowTitle(jtr("set"));
}

QString Widget::makeHelpInput()
{
    QString help_input;

    for(int i = 1; i < 3;++i)//2个
    {
        help_input = help_input + ui_DATES.input_pfx + ":\n";//前缀,用户昵称
        help_input = help_input + jtr(QString("H%1").arg(i)) + "\n";//问题
        help_input = help_input + "\n" + ui_DATES.input_sfx + ":\n";//后缀,模型昵称
        help_input = help_input + jtr(QString("A%1").arg(i)) + "\n";//答案
    }
    
    return help_input;
}

//监听操作系统
bool Widget::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)
    // Transform the message pointer to the MSG WinAPI
    MSG* msg = reinterpret_cast<MSG*>(message);
 
    // If the message is a HotKey, then ...
    if(msg->message == WM_HOTKEY){
        // ... check HotKey
        if(msg->wParam == 7758258)
        {
            // We inform about this to the console
            if(!is_debuging)
            {
                onShortcutActivated();//处理截图事件
            }

            return true;
        }
        else if (msg->wParam == 123456)
        {
            if(whisper_model_path == "")//如果还未指定模型路径则先指定
            {
                emit ui2expend_show(6);//语音增殖界面
                return true;
            }   
            else if(!is_recodering)
            {
                if(!is_debuging)
                {
                    recordAudio();//开始录音
                    is_recodering = true;
                }
                
            }
            else if(is_recodering)
            {
                if(!is_debuging)
                {
                    stop_recordAudio();//停止录音
                }
            }
            
            return true;
        }
        else if (msg->wParam == 741852963)
        {
            ui->send->click();
        }
        
    }
    return false;
}

//创建临时文件夹EVA_TEMP
bool Widget::createTempDirectory(const QString &path) {
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
//解决打开选择文件窗口异常大的问题
QString Widget::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath="";
#if defined(_WIN32)
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);
#else
    QFileDialog dlg(NULL, describe);
    dlg.setDirectory(dirpath);//默认打开路径
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);//不使用系统原生的窗口
    dlg.setFileMode(QFileDialog::ExistingFile);dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setNameFilter(format);//筛选格式
    if (dlg.exec() == QDialog::Accepted) {filepath = dlg.selectedFiles().first();}//只要一个文件
#endif

    return filepath;
}

//语音朗读相关
void Widget::qspeech(QString str)
{
    //如果禁用了朗读则直接退出
    //qDebug()<<voice_params.is_voice<<voice_params.voice_name;
    if(!voice_params.is_voice)
    {
        speechOver();
        return;
    }
    
    if(voice_params.voice_name!="")
    {
        // 遍历所有可用音色
        foreach (const QVoice &voice, speech->availableVoices()) 
        {
            // qDebug() << "Name:" << voice.name();
            // qDebug() << "Age:" << voice.age();
            // qDebug() << "Gender:" << voice.gender();
            //使用用户选择的音色
            if (voice.name() == voice_params.voice_name) 
            {
                speech->setVoice(voice);
                break;
            }
        }

        // 设置语速，范围从-1到1
        speech->setRate(0.3);

        // 设置音量，范围从0到1
        speech->setVolume(1.0);

        // 开始文本到语音转换
        speech->say(str);
    }

}

//每半秒检查列表，列表中有文字就读然后删，直到读完
void Widget::qspeech_process()
{
    if(!is_speech)
    {
        if(wait_speech.size()>0)
        {
            speechtimer->stop();
            is_speech = true;
            qspeech(wait_speech.first());
            //qDebug()<<wait_speech.first();
            wait_speech.removeFirst();
            
        }
    }
}
//朗读结束后动作
void Widget::speechOver()
{
    speechtimer->stop();
    speechtimer->start(500);
    is_speech = false;//解锁
}

//每次约定和设置后都保存配置到本地
 void Widget::auto_save_user()
 {
    //--------------保存当前用户配置---------------
    // 创建 QSettings 对象，指定配置文件的名称和格式

    createTempDirectory("./EVA_TEMP");
    QSettings settings("./EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    //保存设置参数
    settings.setValue("modelpath",ui_SETTINGS.modelpath);//模型路径
    settings.setValue("temp",ui_SETTINGS.temp);//惩罚系数
    settings.setValue("repeat",ui_SETTINGS.repeat);//惩罚系数
    settings.setValue("npredict",ui_SETTINGS.npredict);//最大输出长度
    settings.setValue("ngl",ui_SETTINGS.ngl);//gpu负载层数
    settings.setValue("nthread",ui_SETTINGS.nthread);//cpu线程数
    if(ui_SETTINGS.nctx > ui_n_ctx_train){settings.setValue("nctx",ui_n_ctx_train);}//防止溢出
    else{settings.setValue("nctx",ui_SETTINGS.nctx);}
    settings.setValue("batch",ui_SETTINGS.batch);//批大小
    settings.setValue("mmprojpath",ui_SETTINGS.mmprojpath);//视觉
    settings.setValue("lorapath",ui_SETTINGS.lorapath);//lora
    settings.setValue("ui_mode",ui_mode);//模式
    settings.setValue("port",ui_port);//服务端口

    //保存约定参数
    settings.setValue("chattemplate",chattemplate_comboBox->currentText());//对话模板
    settings.setValue("system_prompt",system_TextEdit->toPlainText());//系统指令
    settings.setValue("extra_prompt",extra_TextEdit->toPlainText());//额外指令
    settings.setValue("input_pfx",ui_DATES.input_pfx);//用户昵称
    settings.setValue("input_sfx",ui_DATES.input_sfx);//模型昵称
    settings.setValue("calculator_checkbox",calculator_checkbox->isChecked());//计算器工具
    settings.setValue("cmd_checkbox",cmd_checkbox->isChecked());//cmd工具
    settings.setValue("knowledge_checkbox",knowledge_checkbox->isChecked());//knowledge工具
    settings.setValue("controller_checkbox",controller_checkbox->isChecked());//controller工具
    settings.setValue("stablediffusion_checkbox",stablediffusion_checkbox->isChecked());//计算器工具
    settings.setValue("toolguy_checkbox",toolguy_checkbox->isChecked());//toolguy工具
    settings.setValue("interpreter_checkbox",interpreter_checkbox->isChecked());//toolguy工具
    settings.setValue("extra_lan",ui_extra_lan);//额外指令语种

    //保存自定义的约定模板
    settings.setValue("custom1_system_prompt",custom1_system_prompt);
    settings.setValue("custom1_input_pfx",custom1_input_pfx);
    settings.setValue("custom1_input_sfx",custom1_input_sfx);
    settings.setValue("custom2_system_prompt",custom2_system_prompt);
    settings.setValue("custom2_input_pfx",custom2_input_pfx);
    settings.setValue("custom2_input_sfx",custom2_input_sfx);
    
    reflash_state("ui:" + jtr("save_config_mess"),USUAL_);
 }
