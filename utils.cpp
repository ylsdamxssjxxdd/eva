//功能函数

#include "widget.h"
#include "ui_widget.h"

//输出解析器，提取JSON
QStringList Widget::JSONparser(QString text)
{
    QStringList func_arg_list;
    // 使用正则表达式来定位JSON部分
    // 使用QRegularExpression查找JSON字符串
    // 正则表达式解释：
    // \s* 可能的空白符
    // \{ 开始的大括号
    // (?:.|\n)*? 非贪婪匹配任意字符包括换行符
    // \} 结束的大括号
    QRegularExpression re("\\{(?:.|\\s)*?\\}");
    QRegularExpressionMatch match = re.match(text);

    if (match.hasMatch()) {
        // 提取JSON字符串
        QString jsonString = match.captured(0);
        // 将JSON字符串转换为QJsonDocument
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8());
        // 检查解析是否成功
        if (!jsonDoc.isNull()) {
            if (jsonDoc.isObject()) {
                // 获取QJsonObject并操作它
                QJsonObject jsonObj = jsonDoc.object();
                QString action = jsonObj["action"].toString();
                
                QString action_input;
                // 检查action_input的类型并相应处理
                QJsonValue actionInputValue = jsonObj["action_input"];
                if (actionInputValue.isString()) 
                {
                    // 如果是字符串，则获取字符串
                    action_input = actionInputValue.toString();
                    
                } 
                else if (actionInputValue.isDouble()) {
                    // 如果是数字，则获取数字
                    action_input = QString::number(actionInputValue.toDouble());
                }
                func_arg_list << action;
                func_arg_list << action_input;
                qDebug() << "action:" << action<< "action_input:" << action_input;
                reflash_state("ui:" + wordsObj["json detect"].toString() + " action:" + action + " action_input:" + action_input,USUAL_);
            } else {
                reflash_state("ui:" + wordsObj["no json detect"].toString() + " JSON document is not an object",USUAL_);
            }
        } else {
            reflash_state("ui:" + wordsObj["no json detect"].toString() + " Invalid JSON...",USUAL_);
        }
    } else {
        reflash_state("ui:" + wordsObj["no json detect"].toString(),USUAL_);
    }
    return func_arg_list;
}

//构建附加指令
QString Widget::create_extra_prompt()
{
    QString extra_prompt_;
    if(switch_lan_button->text()=="zh")
    {
        extra_prompt_ = wordsObj["head_extra_prompt_zh"].toString();
        if(is_load_tool)
        {
            //头
            if(calculator_checkbox->isChecked()){extra_prompt_ += tool_map["calculator"].func_describe_zh + "\n";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += tool_map["cmd"].func_describe_zh + "\n";}
            if(search_checkbox->isChecked()){extra_prompt_ += tool_map["search"].func_describe_zh + "\n";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ += tool_map["knowledge"].func_describe_zh + " " + wordsObj["embeddingdb describe zh"].toString() + ":" + embeddingdb_describe + "\n";}
            if(positron_checkbox->isChecked()){extra_prompt_ += tool_map["positron"].func_describe_zh + "\n";}
            if(stablediffusion_checkbox->isChecked()){extra_prompt_ += tool_map["stablediffusion"].func_describe_zh + "\n";}
            //中
            extra_prompt_ +=wordsObj["middle_extra_prompt_zh"].toString();
            if(calculator_checkbox->isChecked()){extra_prompt_ += "\"calculator\" ";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += "\"cmd\" ";}
            if(search_checkbox->isChecked()){extra_prompt_ += "\"search\" ";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ +="\"knowledge\" ";}
            if(positron_checkbox->isChecked()){extra_prompt_ +="\"positron\" ";}
            if(stablediffusion_checkbox->isChecked()){extra_prompt_ +="\"stablediffusion\" ";}
            //尾
            extra_prompt_ += wordsObj["tail_extra_prompt_zh"].toString();
        }
        else{extra_prompt_ = "";}
        return extra_prompt_;
    }
    else if(switch_lan_button->text()=="en")
    {
        extra_prompt_ = wordsObj["head_extra_prompt_en"].toString();
        if(is_load_tool)
        {
            //头
            if(calculator_checkbox->isChecked()){extra_prompt_ += tool_map["calculator"].func_describe_en + "\n";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += tool_map["cmd"].func_describe_en + "\n";}
            if(search_checkbox->isChecked()){extra_prompt_ += tool_map["search"].func_describe_en + "\n";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ += tool_map["knowledge"].func_describe_en  + " " + wordsObj["embeddingdb describe en"].toString() + ":" + embeddingdb_describe + "\n";}
            if(positron_checkbox->isChecked()){extra_prompt_ += tool_map["positron"].func_describe_en + "\n";}
            if(stablediffusion_checkbox->isChecked()){extra_prompt_ += tool_map["stablediffusion"].func_describe_en + "\n";}
            //中
            extra_prompt_ +=wordsObj["middle_extra_prompt_en"].toString();
            if(calculator_checkbox->isChecked()){extra_prompt_ += "\"calculator\" ";}
            if(cmd_checkbox->isChecked()){extra_prompt_ += "\"cmd\" ";}
            if(search_checkbox->isChecked()){extra_prompt_ += "\"search\" ";}
            if(knowledge_checkbox->isChecked()){extra_prompt_ +="\"knowledge\" ";}
            if(positron_checkbox->isChecked()){extra_prompt_ +="\"positron\" ";}
            if(stablediffusion_checkbox->isChecked()){extra_prompt_ +="\"stablediffusion\" ";}
            //尾
            extra_prompt_ += wordsObj["tail_extra_prompt_en"].toString();
        }
        else{extra_prompt_ = "";}
        
    }
    return extra_prompt_;
    
}

//添加额外停止标志
void Widget::addStopwords()
{
    ui_DATES.extra_stop_words.clear();//重置额外停止标志
    ui_DATES.extra_stop_words << ui_DATES.input_pfx + ":\n";//默认第一个是用户昵称，检测出来后下次回答将不再添加前缀
    ui_DATES.extra_stop_words << "<|im_end|>";//防chatml
    if(ui_DATES.is_load_tool)//如果挂载了工具则增加额外停止标志
    {
        ui_DATES.extra_stop_words << "Observation:";
        ui_DATES.extra_stop_words << wordsObj["tool_observation"].toString();
        ui_DATES.extra_stop_words << wordsObj["tool_observation2"].toString();
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
    reflash_state("ui:server"+wordsObj["oning"].toString()+"...",SIGNAL_);
}

//第三方程序结束
void Widget::server_onProcessFinished()
{
    if(current_server)
    {
        ui_state = "ui:"+ wordsObj["old"].toString() + "server" + wordsObj["off"].toString();reflash_state(ui_state,SIGNAL_);
    }
    else
    {
        QApplication::setWindowIcon(QIcon(":/ui/dark_logo.png"));//设置应用程序图标
        reflash_state("ui:server"+wordsObj["off"].toString(),SIGNAL_);
        ui_output = "\nserver"+wordsObj["shut down"].toString();
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
}

//开始录音
void Widget::recordAudio()
{
    reflash_state("ui:" + wordsObj["recoding"].toString() + "... ");
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
    reflash_state("ui:" + wordsObj["recoding over"].toString() + " " + QString::number(float(audio_time)/1000.0,'f',2) + "s");
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
    QString questiontitle = wordsObj["question type"].toString() + ":" + fileName.split("/").last().split(".").at(0) + "\n\n";
    if (!file.open(QIODevice::ReadOnly)) 
    {
        qDebug() << file.errorString();
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    // 读取并忽略标题行
    QString headerLine = in.readLine();

    // 读取文件的每一行
    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 使用制表符分割每一行的内容
        QStringList fields = line.split(",");

        
        // 确保每行有足够的列
        if(fields.size() >= 7)
        {
            // 输出题目和答案
            // qDebug() << "id:" << fields.at(0).trimmed();
            // qDebug() << "Question:" << fields.at(1).trimmed();
            // qDebug() << "A:" << fields.at(2).trimmed();
            // qDebug() << "B:" << fields.at(3).trimmed();
            // qDebug() << "C:" << fields.at(4).trimmed();
            // qDebug() << "D:" << fields.at(5).trimmed();
            // qDebug() << "Answer:" << fields.at(6).trimmed();
            test_list_question<<questiontitle +fields.at(1).trimmed()+"\n\n"+"A:" + fields.at(2).trimmed()+"\n"+"B:"+fields.at(3).trimmed()+"\n"+"C:"+fields.at(4).trimmed()+"\n"+"D:"+fields.at(5).trimmed()+"\n";
            test_list_answer<<fields.at(6).trimmed();
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

// 判断是否挂载了工具
bool Widget::toolcheckbox_checked()
{
    if(calculator_checkbox->isChecked() || cmd_checkbox->isChecked() || search_checkbox->isChecked() || knowledge_checkbox->isChecked() || positron_checkbox->isChecked() || stablediffusion_checkbox->isChecked())
    {
        return true;
    }
    else
    {
        return false;
    }
    
}
//切换额外指令的语言
void Widget::switch_lan_change()
{
    if(switch_lan_button->text()=="zh")
    {
        switch_lan_button->setText("en");
        create_extra_prompt();
        extra_TextEdit->setText(create_extra_prompt());
    }
    else if(switch_lan_button->text()=="en")
    {
        switch_lan_button->setText("zh");
        create_extra_prompt();
        extra_TextEdit->setText(create_extra_prompt());
    }
}

QString Widget::makeHelpInput()
{
    QString help_input;

    for(int i = 1; i < 3;++i)//2个
    {
        help_input = help_input + ui_DATES.input_pfx + ":\n";//前缀,用户昵称
        help_input = help_input + wordsObj[QString("H%1").arg(i)].toString() + "\n";//问题
        help_input = help_input + "\n" + ui_DATES.input_sfx + ":\n";//后缀,模型昵称
        help_input = help_input + wordsObj[QString("A%1").arg(i)].toString() + "\n";//答案
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