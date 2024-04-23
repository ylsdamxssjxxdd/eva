//功能函数

#include "widget.h"
#include "ui_widget.h"

//手搓输出解析器，提取JSON
QPair<QString, QString> Widget::JSONparser(QString text) {
    
    QPair<QString, QString> func_arg_list;
    // 匹配花括号至最后一个花括号中的内容
    QRegularExpression re("\\{(.*)\\}");
    re.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);//允许包含换行符
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch())
    {
        QString content = match.captured(1);  // 获取第一个捕获组的内容
        qDebug() << "花括号中的内容是：" << content;
        // 匹配"action:"至逗号， \\s*的意思是允许忽略空格
        QRegularExpression re2("\"action\"\\s*[:：]\\s*\"([^\"]*)\"");
        QRegularExpressionMatch match2 = re2.match(content);
        if (match2.hasMatch())
        {
            QString content2 = match2.captured(1);  // 获取第一个捕获组的内容
            func_arg_list.first = content2;
            qDebug() << "action中的内容是：" << content2;
            // 匹配"action_input:"至最后
            QRegularExpression re3("\"action_input\"\\s*[:：]\\s*(.*)");
            re3.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);//允许包含换行符
            QRegularExpressionMatch match3 = re3.match(content);
            if (match3.hasMatch())
            {
                QString content3 = match3.captured(1).trimmed().replace("\\n", "\n");;  // 获取第一个捕获组的内容
                //如果挂载了代码解释器则去除文本段前后的标点
                if(ui_interpreter_ischecked)
                {
                    // 去除最前面的标点 { " ' }
                    while (!content3.isEmpty() && (content3.at(0) == QChar('\"') || content3.at(0) == QChar('\'') || content3.at(0) == QChar('{'))) {
                        content3 = content3.mid(1);
                    }

                    // 去除最后面的标点 { " ' }
                    while (!content3.isEmpty() && (content3.at(content3.length() - 1) == QChar('\"') || content3.at(content3.length() - 1) == QChar('\'') || content3.at(content3.length() - 1) == QChar('}'))) {
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

    extra_prompt_ = wordsObj["head_extra_prompt"].toArray()[language_flag].toString();
    if(is_load_tool)
    {
        //头
        if(interpreter_checkbox->isChecked()){extra_prompt_ += tool_map["interpreter"].func_describe + "\n";}
        if(calculator_checkbox->isChecked()){extra_prompt_ += tool_map["calculator"].func_describe + "\n";}
        if(cmd_checkbox->isChecked()){extra_prompt_ += tool_map["cmd"].func_describe + "\n";}
        if(toolguy_checkbox->isChecked()){extra_prompt_ += tool_map["toolguy"].func_describe + "\n";}
        if(knowledge_checkbox->isChecked()){extra_prompt_ += tool_map["knowledge"].func_describe + " " + wordsObj["embeddingdb describe"].toArray()[language_flag].toString() + ":" + embeddingdb_describe + "\n";}
        if(stablediffusion_checkbox->isChecked()){extra_prompt_ += tool_map["stablediffusion"].func_describe + "\n";}
        if(controller_checkbox->isChecked()){extra_prompt_ += tool_map["controller"].func_describe + "\n";}
        //中
        extra_prompt_ +=wordsObj["middle_extra_prompt"].toArray()[language_flag].toString();
        if(interpreter_checkbox->isChecked()){extra_prompt_ +="\"interpreter\" ";}
        if(calculator_checkbox->isChecked()){extra_prompt_ += "\"calculator\" ";}
        if(cmd_checkbox->isChecked()){extra_prompt_ += "\"cmd\" ";}
        if(toolguy_checkbox->isChecked()){extra_prompt_ += "\"toolguy\" ";}
        if(knowledge_checkbox->isChecked()){extra_prompt_ +="\"knowledge\" ";}
        if(stablediffusion_checkbox->isChecked()){extra_prompt_ +="\"stablediffusion\" ";}
        if(controller_checkbox->isChecked()){extra_prompt_ +="\"controller\" ";}
        //尾
        extra_prompt_ += wordsObj["tail_extra_prompt"].toArray()[language_flag].toString();
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
    ui_DATES.extra_stop_words << "<|im_end|>";//可以说相当严格了

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
    reflash_state("ui:server " + wordsObj["oning"].toArray()[language_flag].toString(),SIGNAL_);
}

//第三方程序结束
void Widget::server_onProcessFinished()
{
    if(current_server)
    {
        ui_state = "ui:"+ wordsObj["old"].toArray()[language_flag].toString() + "server " + wordsObj["off"].toArray()[language_flag].toString();reflash_state(ui_state,SIGNAL_);
    }
    else
    {
        QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
        reflash_state("ui:server"+wordsObj["off"].toArray()[language_flag].toString(),SIGNAL_);
        ui_output = "\nserver"+wordsObj["shut down"].toArray()[language_flag].toString();
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
    reflash_state("ui:" + wordsObj["recoding"].toArray()[language_flag].toString() + "... ");
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
    reflash_state("ui:" + wordsObj["recoding over"].toArray()[language_flag].toString() + " " + QString::number(float(audio_time)/1000.0,'f',2) + "s");
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
    QString questiontitle = wordsObj["question type"].toArray()[language_flag].toString() + ":" + fileName.split("/").last().split(".").at(0) + "\n\n";
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
    ui->load->setText(wordsObj["load"].toArray()[language_flag].toString());
    ui->load->setToolTip(wordsObj["load_button_tooltip"].toArray()[language_flag].toString());
    ui->date->setText(wordsObj["date"].toArray()[language_flag].toString());
    ui->set->setToolTip(wordsObj["set"].toArray()[language_flag].toString());
    ui->reset->setToolTip(wordsObj["reset"].toArray()[language_flag].toString());
    ui->send->setText(wordsObj["send"].toArray()[language_flag].toString());
    ui->send->setToolTip(wordsObj["send_tooltip"].toArray()[language_flag].toString());
    cutscreen_dialog->init_action(wordsObj["save cut image"].toArray()[language_flag].toString(), wordsObj["svae screen image"].toArray()[language_flag].toString());
    ui->cpu_bar->setToolTip(wordsObj["nthread/maxthread"].toArray()[language_flag].toString()+"  "+QString::number(ui_SETTINGS.nthread)+"/"+QString::number(std::thread::hardware_concurrency()));
    ui->mem_bar->set_show_text(wordsObj["mem"].toArray()[language_flag].toString());//进度条里面的文本,强制重绘
    ui->vram_bar->set_show_text(wordsObj["vram"].toArray()[language_flag].toString());//进度条里面的文本,强制重绘
    ui->kv_bar->set_show_text(wordsObj["brain"].toArray()[language_flag].toString());//进度条里面的文本,强制重绘
    ui->cpu_bar->show_text = "cpu ";//进度条里面的文本
    ui->vcore_bar->show_text = "gpu ";//进度条里面的文本
    //输入区右击菜单语种
    create_right_menu();//添加右击问题
    //api设置语种
    api_dialog->setWindowTitle("api" + wordsObj["set"].toArray()[language_flag].toString());
    api_ip_label->setText("api " + wordsObj["address"].toArray()[language_flag].toString());
    api_ip_LineEdit->setPlaceholderText(wordsObj["input server ip"].toArray()[language_flag].toString());
    api_port_label->setText("api " + wordsObj["port"].toArray()[language_flag].toString());
    api_chat_label->setText(wordsObj["chat"].toArray()[language_flag].toString()+wordsObj["endpoint"].toArray()[language_flag].toString());
    api_complete_label->setText(wordsObj["complete"].toArray()[language_flag].toString()+wordsObj["endpoint"].toArray()[language_flag].toString());
    //约定选项语种
    prompt_box->setTitle(wordsObj["prompt"].toArray()[language_flag].toString() + wordsObj["template"].toArray()[language_flag].toString());//提示词模板设置区域
    chattemplate_label->setText(wordsObj["chat template"].toArray()[language_flag].toString());
    chattemplate_label->setToolTip(wordsObj["chattemplate_label_tooltip"].toArray()[language_flag].toString());
    chattemplate_comboBox->setToolTip(wordsObj["chattemplate_label_tooltip"].toArray()[language_flag].toString());
    system_label->setText(wordsObj["system calling"].toArray()[language_flag].toString());
    system_label->setToolTip(wordsObj["system_label_tooltip"].toArray()[language_flag].toString());
    system_TextEdit->setToolTip(wordsObj["system_label_tooltip"].toArray()[language_flag].toString());
    input_pfx_label->setText(wordsObj["user name"].toArray()[language_flag].toString());
    input_pfx_label->setToolTip(wordsObj["input_pfx_label_tooltip"].toArray()[language_flag].toString());
    input_pfx_LineEdit->setToolTip(wordsObj["input_pfx_label_tooltip"].toArray()[language_flag].toString());
    input_sfx_label->setText(wordsObj["bot name"].toArray()[language_flag].toString());
    input_sfx_label->setToolTip(wordsObj["input_sfx_label_tooltip"].toArray()[language_flag].toString());
    input_sfx_LineEdit->setToolTip(wordsObj["input_sfx_label_tooltip"].toArray()[language_flag].toString());
    tool_box->setTitle(wordsObj["mount"].toArray()[language_flag].toString() + wordsObj["tool"].toArray()[language_flag].toString());
    calculator_checkbox->setText(wordsObj["calculator"].toArray()[language_flag].toString());
    calculator_checkbox->setToolTip(wordsObj["calculator_checkbox_tooltip"].toArray()[language_flag].toString());
    cmd_checkbox->setText(wordsObj["cmd"].toArray()[language_flag].toString());
    cmd_checkbox->setToolTip(wordsObj["cmd_checkbox_tooltip"].toArray()[language_flag].toString());
    controller_checkbox->setText(wordsObj["controller"].toArray()[language_flag].toString());
    controller_checkbox->setToolTip(wordsObj["controller_checkbox_tooltip"].toArray()[language_flag].toString());
    knowledge_checkbox->setText(wordsObj["knowledge"].toArray()[language_flag].toString());
    knowledge_checkbox->setToolTip(wordsObj["knowledge_checkbox_tooltip"].toArray()[language_flag].toString());
    toolguy_checkbox->setText(wordsObj["toolguy"].toArray()[language_flag].toString());
    toolguy_checkbox->setToolTip(wordsObj["toolguy_checkbox_tooltip"].toArray()[language_flag].toString());
    stablediffusion_checkbox->setText(wordsObj["stablediffusion"].toArray()[language_flag].toString());
    stablediffusion_checkbox->setToolTip(wordsObj["stablediffusion_checkbox_tooltip"].toArray()[language_flag].toString());
    interpreter_checkbox->setText(wordsObj["interpreter"].toArray()[language_flag].toString());
    interpreter_checkbox->setToolTip(wordsObj["interpreter_checkbox_tooltip"].toArray()[language_flag].toString());
    extra_label->setText(wordsObj["extra calling"].toArray()[language_flag].toString());
    extra_label->setToolTip(wordsObj["extra_label_tooltip"].toArray()[language_flag].toString());
    switch_lan_button->setToolTip(wordsObj["switch_lan_button_tooltip"].toArray()[language_flag].toString());
    extra_TextEdit->setPlaceholderText(wordsObj["extra_TextEdit_tooltip"].toArray()[language_flag].toString());
    extra_TextEdit->setToolTip(wordsObj["extra_TextEdit_tooltip"].toArray()[language_flag].toString());
    tool_map.clear();
    tool_map.insert("calculator", {wordsObj["calculator"].toArray()[language_flag].toString(),"calculator",wordsObj["calculator_func_describe"].toArray()[language_flag].toString()});
    tool_map.insert("cmd", {wordsObj["cmd"].toArray()[language_flag].toString(),"cmd",wordsObj["cmd_func_describe"].toArray()[language_flag].toString()});
    tool_map.insert("toolguy", {wordsObj["toolguy"].toArray()[language_flag].toString(),"toolguy",wordsObj["toolguy_func_describe"].toArray()[language_flag].toString()});
    tool_map.insert("knowledge", {wordsObj["knowledge"].toArray()[language_flag].toString(),"knowledge",wordsObj["knowledge_func_describe"].toArray()[language_flag].toString()});
    tool_map.insert("controller", {wordsObj["controller"].toArray()[language_flag].toString(),"controller",wordsObj["controller_func_describe"].toArray()[language_flag].toString()});
    tool_map.insert("stablediffusion", {wordsObj["stablediffusion"].toArray()[language_flag].toString(),"stablediffusion",wordsObj["stablediffusion_func_describe"].toArray()[language_flag].toString()});
    tool_map.insert("interpreter", {wordsObj["interpreter"].toArray()[language_flag].toString(),"interpreter",wordsObj["interpreter_func_describe"].toArray()[language_flag].toString()});
    extra_TextEdit->setText(create_extra_prompt());//构建附加指令
    date_dialog->setWindowTitle(wordsObj["date"].toArray()[language_flag].toString());
    //设置选项语种
    sample_box->setTitle(wordsObj["sample set"].toArray()[language_flag].toString());//采样设置区域
    temp_label->setText(wordsObj["temperature"].toArray()[language_flag].toString()+" " + QString::number(ui_SETTINGS.temp));
    temp_label->setToolTip(wordsObj["The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"].toArray()[language_flag].toString());
    temp_slider->setToolTip(wordsObj["The higher the temperature, the more divergent the response; the lower the temperature, the more accurate the response"].toArray()[language_flag].toString());
    repeat_label->setText(wordsObj["repeat"].toArray()[language_flag].toString() + " " + QString::number(ui_SETTINGS.repeat));
    repeat_label->setToolTip(wordsObj["Reduce the probability of the model outputting synonymous words"].toArray()[language_flag].toString());
    repeat_slider->setToolTip(wordsObj["Reduce the probability of the model outputting synonymous words"].toArray()[language_flag].toString());
    npredict_label->setText(wordsObj["npredict"].toArray()[language_flag].toString() + " " + QString::number(ui_SETTINGS.npredict));
    npredict_label->setToolTip(wordsObj["The maximum number of tokens that the model can output in a single prediction process"].toArray()[language_flag].toString());npredict_label->setMinimumWidth(100);
    npredict_slider->setToolTip(wordsObj["The maximum number of tokens that the model can output in a single prediction process"].toArray()[language_flag].toString());
    decode_box->setTitle(wordsObj["decode set"].toArray()[language_flag].toString());//解码设置区域
#if defined(BODY_USE_VULKAN) || defined(BODY_USE_CLBLAST) || defined(BODY_USE_CUBLAST)
    ngl_label->setText("gpu " + wordsObj["offload"].toArray()[language_flag].toString() + QString::number(ui_SETTINGS.ngl));
    ngl_label->setToolTip(wordsObj["put some model paragram to gpu and reload model"].toArray()[language_flag].toString());ngl_label->setMinimumWidth(100);
    ngl_slider->setToolTip(wordsObj["put some model paragram to gpu and reload model"].toArray()[language_flag].toString());
#endif
    nthread_label->setText("cpu " + wordsObj["thread"].toArray()[language_flag].toString() + " " + QString::number(ui_SETTINGS.nthread));
    nthread_label->setToolTip(wordsObj["not big better"].toArray()[language_flag].toString());
    nthread_slider->setToolTip(wordsObj["not big better"].toArray()[language_flag].toString());
    nctx_label->setText(wordsObj["brain size"].toArray()[language_flag].toString()+" " + QString::number(ui_SETTINGS.nctx));
    nctx_label->setToolTip(wordsObj["ctx"].toArray()[language_flag].toString() + wordsObj["length"].toArray()[language_flag].toString() + "," + wordsObj["big brain size lead small wisdom"].toArray()[language_flag].toString());nctx_label->setMinimumWidth(100);
    nctx_slider->setToolTip(wordsObj["ctx"].toArray()[language_flag].toString() + wordsObj["length"].toArray()[language_flag].toString() + "," + wordsObj["big brain size lead small wisdom"].toArray()[language_flag].toString());
    batch_label->setText(wordsObj["batch size"].toArray()[language_flag].toString() + " " + QString::number(ui_SETTINGS.batch));
    batch_label->setToolTip(wordsObj["The number of tokens processed simultaneously in one decoding"].toArray()[language_flag].toString());batch_label->setMinimumWidth(100);
    batch_slider->setToolTip(wordsObj["The number of tokens processed simultaneously in one decoding"].toArray()[language_flag].toString());
    lora_label->setText(wordsObj["load lora"].toArray()[language_flag].toString());
    lora_label->setToolTip(wordsObj["lora_label_tooltip"].toArray()[language_flag].toString());
    lora_LineEdit->setToolTip(wordsObj["lora_label_tooltip"].toArray()[language_flag].toString());
    lora_LineEdit->setPlaceholderText(wordsObj["right click and choose lora"].toArray()[language_flag].toString());
    mmproj_label->setText(wordsObj["load mmproj"].toArray()[language_flag].toString());
    mmproj_label->setToolTip(wordsObj["mmproj_label_tooltip"].toArray()[language_flag].toString());
    mmproj_LineEdit->setToolTip(wordsObj["mmproj_label_tooltip"].toArray()[language_flag].toString());
    mmproj_LineEdit->setPlaceholderText(wordsObj["right click and choose mmproj"].toArray()[language_flag].toString());
    mode_box->setTitle(wordsObj["mode set"].toArray()[language_flag].toString());//模式设置区域
    complete_btn->setText(wordsObj["complete mode"].toArray()[language_flag].toString());
    complete_btn->setToolTip(wordsObj["complete_btn_tooltip"].toArray()[language_flag].toString());
    chat_btn->setText(wordsObj["chat mode"].toArray()[language_flag].toString());
    chat_btn->setToolTip(wordsObj["chat_btn_tooltip"].toArray()[language_flag].toString());
    web_btn->setText(wordsObj["server mode"].toArray()[language_flag].toString());
    web_btn->setToolTip(wordsObj["web_btn_tooltip"].toArray()[language_flag].toString());
    port_label->setText(wordsObj["port"].toArray()[language_flag].toString());
    port_label->setToolTip(wordsObj["port_label_tooltip"].toArray()[language_flag].toString());
    port_lineEdit->setToolTip(wordsObj["port_label_tooltip"].toArray()[language_flag].toString());
    set_dialog->setWindowTitle(wordsObj["set"].toArray()[language_flag].toString());
}

QString Widget::makeHelpInput()
{
    QString help_input;

    for(int i = 1; i < 3;++i)//2个
    {
        help_input = help_input + ui_DATES.input_pfx + ":\n";//前缀,用户昵称
        help_input = help_input + wordsObj[QString("H%1").arg(i)].toArray()[language_flag].toString() + "\n";//问题
        help_input = help_input + "\n" + ui_DATES.input_sfx + ":\n";//后缀,模型昵称
        help_input = help_input + wordsObj[QString("A%1").arg(i)].toArray()[language_flag].toString() + "\n";//答案
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
            onShortcutActivated();//处理截图事件
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
                recordAudio();//开始录音
                is_recodering = true;
            }
            else if(is_recodering)
            {
                stop_recordAudio();//停止录音
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
    
    reflash_state("ui:" + wordsObj["save_config_mess"].toArray()[language_flag].toString(),USUAL_);
 }
