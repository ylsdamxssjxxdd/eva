
#include "expend.h"

#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------文转声相关--------------------------------
//-------------------------------------------------------------------------

//用户点击启用声音选项响应
void Expend::speech_enable_change()
{
    if (ui->speech_enable_radioButton->isChecked())
    {
        speech_params.enable_speech = true;
    }
    else
    {
        speech_params.enable_speech = false;
    }
}

//用户切换声源响应
void Expend::speech_source_change()
{
    speech_params.speech_name = ui->speech_source_comboBox->currentText();
    if (speech_params.speech_name == SPPECH_OUTETTS)
    {
        ui->speech_outetts_modelpath_frame->setEnabled(1);
        ui->speech_wavtokenizer_modelpath_frame->setEnabled(1);
    }
    else
    {
        ui->speech_outetts_modelpath_frame->setEnabled(0);
        ui->speech_wavtokenizer_modelpath_frame->setEnabled(0);
    }
}

// 添加可用声源
void Expend::set_sys_speech(QStringList avaliable_speech_list)
{
    for (int i = 0; i < avaliable_speech_list.size(); ++i)
    {
        ui->speech_source_comboBox->addItem(avaliable_speech_list.at(i)); //添加到下拉框
    }
    ui->speech_source_comboBox->setCurrentText(speech_params.speech_name);
    ui->speech_enable_radioButton->setChecked(speech_params.enable_speech);
}

//开始文字转语音
void Expend::start_tts(QString str)
{
    //如果禁用了朗读则直接退出
    // qDebug()<<speech_params.is_speech<<speech_params.speech_name;
    if (!speech_params.enable_speech)
    {
        speechOver();
        return;
    }

    if (speech_params.speech_name != "")
    {
        if (speech_params.speech_name == SPPECH_OUTETTS) // 使用模型声源
        {
            if (ui->speech_outetts_modelpath_lineEdit->text() != "" && ui->speech_wavtokenizer_modelpath_lineEdit->text() != "")
            {
                outettsProcess(str);
            }
        }
        else
        {
            // 遍历所有可用音色
            foreach (const QVoice &voice, sys_speech->availableVoices())
            {
                // qDebug() << "Name:" << speech.name();
                // qDebug() << "Age:" << speech.age();
                // qDebug() << "Gender:" << speech.gender();
                //使用用户选择的音色
                if (voice.name() == speech_params.speech_name)
                {
                    sys_speech->setVoice(voice);
                    break;
                }
            }

            // 设置语速，范围从-1到1
            sys_speech->setRate(0.3);

            // 设置音量，范围从0到1
            sys_speech->setVolume(1.0);

            // 开始文本到语音转换
            sys_speech->say(str);
        }
    }
}

void Expend::speechOver()
{
    speechTimer.stop();
    speechTimer.start(500);
    is_speech = false; //解锁
}

void Expend::speechPlayOver()
{
    speechPlayTimer.stop();
    speechPlayTimer.start(500);
    is_speech_play = false; //解锁
}

//每半秒检查列表，列表中有文字就读然后删，直到读完
void Expend::speech_process()
{
    if (!is_speech)
    {
        if (wait_speech_txt_list.size() > 0)
        {
            speechTimer.stop();
            is_speech = true;
            start_tts(wait_speech_txt_list.first());
            wait_speech_txt_list.removeFirst();
        }
    }
}

//每半秒检查待播放列表，列表中有文字就读然后删，直到读完
void Expend::speech_play_process()
{
    if (!is_speech_play)
    {
        if (wait_speech_play_list.size() > 0)
        {
            speechPlayTimer.stop();
            is_speech_play = true;
            // 播放第一路径的音频
            speech_player->setMedia(QUrl::fromLocalFile(wait_speech_play_list.first()));
            speech_player->play();
            wait_speech_play_list.removeFirst();
        }
    }
}

void Expend::recv_output(const QString result, bool is_while, QColor color)
{
    if (is_while)
    {
        //添加待朗读的文字
        temp_speech_txt += result; // 累计输出的文本
        //如果积累到包含 叹号/分号/顿号/逗号/句号/问号/冒号 时分段并等待朗读
        // QRegularExpression re("[！；、，。？：!;,?:]");
        QRegularExpression re("[！；、，。？：!;,?:]|\\.\\s"); //新增对小数点后跟空格的捕获，但是如果模型输出带空格的字符将会分割异常，待修复
        QRegularExpressionMatch match = re.match(temp_speech_txt);
        if (match.hasMatch())
        {
            wait_speech_txt_list << temp_speech_txt;
            temp_speech_txt = "";
        }
    }
}

// 收到重置信号
void Expend::recv_resettts()
{
    temp_speech_txt = "";          //清空待读列表
    wait_speech_txt_list.clear();  //清空待读列表
    wait_speech_play_list.clear(); //清空待读列表
    if (is_sys_speech_available)
    {
        sys_speech->stop(); //停止朗读
    }

    outetts_process->kill(); //终止继续生成
    speech_player->stop();
    removeDir(outettsDir); //清空产生的音频
}

//使用outetts进行文转声
void Expend::outettsProcess(QString str)
{
#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/bin/llama-tts") + SFX_NAME;
    QString program = localPath; // 设置要运行的exe文件的路径
#else
    QString localPath = QString("./llama-tts") + SFX_NAME;
    QString program = localPath; // 设置要运行的exe文件的路径
#endif

    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;
    arguments << "-m" << ui->speech_outetts_modelpath_lineEdit->text();
    arguments << "-mv" << ui->speech_wavtokenizer_modelpath_lineEdit->text();
    arguments << "-ngl"
              << "99";
    arguments << "-p" << str;

    // 开始运行程序
    outetts_process->start(program, arguments);
}

//进程开始响应
void Expend::outetts_onProcessStarted() {}

//进程结束响应
void Expend::outetts_onProcessFinished()
{
    //修改生成的音频名称，添加一个时间后缀
    createTempDirectory(outettsDir);
    // 获取当前日期和时间，精确到分钟
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz"); // 格式化为 20241226_1430
    QString destinationPath = outettsDir + currentDateTime + ".wav";
    // 使用 QFile 移动并重命名文件
    QFile file("output.wav");
    if (file.rename(destinationPath))
    {
        // qDebug() << "File moved and renamed successfully.";
        wait_speech_play_list << destinationPath;
    }
    else
    {
        qDebug() << "Failed to move and rename file." << file.errorString();
    }

    speechOver(); // 利用speechOver再次进入文转声生成处理流程
}

void Expend::readyRead_outetts_process_StandardOutput()
{
    QString outetts_output = outetts_process->readAllStandardOutput();
    ui->speech_log->appendPlainText(outetts_output);
}

void Expend::readyRead_outetts_process_StandardError()
{
    QString outetts_output = outetts_process->readAllStandardError();
    ui->speech_log->appendPlainText(outetts_output);
}

//用户点击选择模型路径时响应
void Expend::on_speech_outetts_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose outetts model", "(*.bin *.gguf)");
    ui->speech_outetts_modelpath_lineEdit->setText(currentpath);
}

//用户点击选择模型路径时响应
void Expend::on_speech_wavtokenizer_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose outetts model", "(*.bin *.gguf)");
    ui->speech_wavtokenizer_modelpath_lineEdit->setText(currentpath);
}

//用户点击转为音频按钮时响应
void Expend::on_speech_manual_pushButton_clicked()
{
    if (ui->speech_outetts_modelpath_lineEdit->text() != "" && ui->speech_wavtokenizer_modelpath_lineEdit->text() != "")
    {
        outettsProcess(ui->speech_manual_plainTextEdit->toPlainText());
    }
}

//音频播放完响应
void Expend::speech_player_over(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::MediaStatus::EndOfMedia)
    {
        // 播放停止时执行的操作
        speechPlayOver();
    }
}
