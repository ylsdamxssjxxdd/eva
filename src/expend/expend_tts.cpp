
#include "expend.h"

#include "ui_expend.h"
// Bring in backend path resolver and path helpers to run llama-tts robustly
#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"

//-------------------------------------------------------------------------
//----------------------------------文转声相关--------------------------------
//-------------------------------------------------------------------------

// 用户点击启用声音选项响应
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

// 用户切换声源响应
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
        ui->speech_source_comboBox->addItem(avaliable_speech_list.at(i)); // 添加到下拉框
    }
    ui->speech_source_comboBox->setCurrentText(speech_params.speech_name);
    ui->speech_enable_radioButton->setChecked(speech_params.enable_speech);
}

// 开始文字转语音
void Expend::start_tts(QString str)
{
    // 如果禁用了朗读则直接退出
    //  qDebug()<<speech_params.is_speech<<speech_params.speech_name;
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
                // 使用用户选择的音色
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
    // 解锁并尽量无等待地触发下一段生成
    is_speech = false;
    // 继续事件驱动的推进；保留定时器作为兜底
    startNextTTSIfIdle();
    speechTimer.stop();
    speechTimer.start(500);
}

void Expend::speechPlayOver()
{
    // 解锁并尽量无等待地触发下一段播放
    is_speech_play = false;
    startNextPlayIfIdle();
    speechPlayTimer.stop();
    speechPlayTimer.start(500);
}

// 每半秒检查列表，列表中有文字就读然后删，直到读完
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

// 每半秒检查待播放列表，列表中有文字就读然后删，直到读完
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
    Q_UNUSED(color);
    if (is_while)
    {
        // 处理思考标记：<think>..</think> 内的内容不进入语音
        QString chunk = result;
        // 进入/退出 think 区域（xNet 会单独发送 begin/end 标记与灰色内容）
        if (chunk.contains(QString(DEFAULT_THINK_BEGIN)))
        {
            tts_in_think_ = true;
            chunk.replace(QString(DEFAULT_THINK_BEGIN), QString());
        }
        if (chunk.contains(QString(DEFAULT_THINK_END)))
        {
            tts_in_think_ = false;
            chunk.replace(QString(DEFAULT_THINK_END), QString());
        }
        if (tts_in_think_)
        {
            // 忽略思考内容
            return;
        }

        // 累计输出的文本（仅普通助理内容）
        temp_speech_txt += chunk;
        // 句末标点切分：中文句号/问号/叹号，以及英文 .!?（排除小数点），以及顿号、逗号、分号、冒号
        static const QRegularExpression re(QString::fromUtf8("([。！？]|(?<!\\d)[.!?](?=\\s|$)|[；;：:,，、])"));

        // 逐段切出已成句的部分，剩余作为缓存
        while (true)
        {
            const QRegularExpressionMatch m = re.match(temp_speech_txt);
            if (!m.hasMatch()) break;
            const int cut = m.capturedEnd();
            const QString seg = temp_speech_txt.left(cut).trimmed();
            if (!seg.isEmpty())
            {
                wait_speech_txt_list << seg;
            }
            temp_speech_txt.remove(0, cut);
        }

        // 兜底：缓存过长但没有标点，按空白或定长切一刀，避免长时间不朗读
        const int MAX_BUF = 240; // 约一到两句
        if (temp_speech_txt.size() > MAX_BUF)
        {
            int cut = temp_speech_txt.lastIndexOf(QRegularExpression("\\s"), MAX_BUF);
            if (cut < 40) cut = MAX_BUF; // 避免切得太短
            const QString seg = temp_speech_txt.left(cut).trimmed();
            if (!seg.isEmpty())
            {
                wait_speech_txt_list << seg;
            }
            temp_speech_txt.remove(0, cut);
        }

        // 事件驱动推进
        startNextTTSIfIdle();
    }
}

// 一轮推理结束：若缓冲里还有尾段（无终止标点），也加入待读列表
void Expend::onNetTurnDone()
{
    if (!temp_speech_txt.trimmed().isEmpty())
    {
        wait_speech_txt_list << temp_speech_txt.trimmed();
        temp_speech_txt.clear();
    }
    // 立即推进朗读/播放
    startNextTTSIfIdle();
    startNextPlayIfIdle();
}

// 递归删除文件夹及其内容的函数
bool Expend::removeDir(const QString &dirName)
{
    QDir dir(dirName);

    if (!dir.exists())
    {
        return false;
    }

    // 删除目录中的所有文件和子目录
    foreach (QFileInfo item, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries))
    {
        if (item.isDir())
        {
            // 如果是子目录，递归删除
            if (!removeDir(item.absoluteFilePath()))
            {
                return false;
            }
        }
        else
        {
            // 如果是文件，删除文件
            if (!QFile::remove(item.absoluteFilePath()))
            {
                return false;
            }
        }
    }

    // 删除目录自身
    return dir.rmdir(dirName);
}

// 收到重置信号
void Expend::recv_resettts()
{
    temp_speech_txt = "";          // 清空待读列表
    wait_speech_txt_list.clear();  // 清空待读列表
    wait_speech_play_list.clear(); // 清空待读列表
    if (is_sys_speech_available)
    {
        sys_speech->stop(); // 停止朗读
    }

    outetts_process->kill(); // 终止继续生成
    speech_player->stop();
    removeDir(outettsDir); // 清空产生的音频
}

// 使用outetts进行文转声
void Expend::outettsProcess(QString str)
{
    // 解析 llama-tts 路径（按设备后端）
    const QString program = DeviceManager::programPath(QStringLiteral("llama-tts"));
    if (program.isEmpty() || !QFileInfo::exists(program))
    {
        ui->speech_log->appendPlainText("[error] llama-tts not found under current device folder");
        ui->speech_log->appendPlainText(QString("[hint] search root=%1, arch=%2, os=%3, device=%4")
                                            .arg(DeviceManager::backendsRootDir())
                                            .arg(DeviceManager::currentArchId())
                                            .arg(DeviceManager::currentOsId())
                                            .arg(DeviceManager::effectiveBackend()));
        speechOver();
        return;
    }

    // 目标输出文件（唯一名）
    createTempDirectory(outettsDir);
    outetts_last_output_file = QDir(outettsDir).filePath(QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz") + ".wav");

    // 组装参数
    QStringList arguments;
    arguments << "-m" << ensureToolFriendlyFilePath(ui->speech_outetts_modelpath_lineEdit->text());
    arguments << "-mv" << ensureToolFriendlyFilePath(ui->speech_wavtokenizer_modelpath_lineEdit->text());
    arguments << "-ngl" << QString::number(99);
    arguments << "-o" << ensureToolFriendlyFilePath(outetts_last_output_file);
    arguments << "-p" << str;

    // 设置运行环境与工作目录，确保依赖 DLL/so 可见
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString toolDir = QFileInfo(program).absolutePath();
#ifdef _WIN32
    env.insert("PATH", toolDir + ";" + env.value("PATH"));
#elif __APPLE__
    env.insert("DYLD_LIBRARY_PATH", toolDir + ":" + env.value("DYLD_LIBRARY_PATH"));
#else
    env.insert("LD_LIBRARY_PATH", toolDir + ":" + env.value("LD_LIBRARY_PATH"));
#endif
    outetts_process->setProcessEnvironment(env);
    outetts_process->setWorkingDirectory(toolDir);

    // 开始运行程序
    outetts_process->start(program, arguments);
}

// 进程开始响应
void Expend::outetts_onProcessStarted() {}

// 进程结束响应
void Expend::outetts_onProcessFinished()
{
    // 优先采用显式 -o 指定的输出路径
    if (!outetts_last_output_file.isEmpty() && QFileInfo::exists(outetts_last_output_file))
    {
        wait_speech_play_list << outetts_last_output_file;
    }
    else
    {
        // 兼容旧行为：若工具写在工作目录 output.wav，尝试搬运
        const QString fallback = QDir(outettsDir).filePath("output.wav");
        if (QFileInfo::exists(fallback))
        {
            const QString dst = QDir(outettsDir).filePath(QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz") + ".wav");
            if (QFile::rename(fallback, dst)) { wait_speech_play_list << dst; }
            else
            {
                ui->speech_log->appendPlainText("[warn] failed to move fallback output.wav");
            }
        }
        else
        {
            ui->speech_log->appendPlainText("[error] TTS finished but no output file produced");
        }
    }

    // 推进生成管线
    speechOver();
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

// 用户点击选择模型路径时响应
void Expend::on_speech_outetts_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose outetts model", "(*.bin *.gguf)");
    ui->speech_outetts_modelpath_lineEdit->setText(currentpath);
}

// 用户点击选择模型路径时响应
void Expend::on_speech_wavtokenizer_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose outetts model", "(*.bin *.gguf)");
    ui->speech_wavtokenizer_modelpath_lineEdit->setText(currentpath);
}

// 用户点击转为音频按钮时响应
void Expend::on_speech_manual_pushButton_clicked()
{
    if (ui->speech_outetts_modelpath_lineEdit->text() != "" && ui->speech_wavtokenizer_modelpath_lineEdit->text() != "")
    {
        outettsProcess(ui->speech_manual_plainTextEdit->toPlainText());
    }
}

// 音频播放完响应
void Expend::speech_player_over(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::MediaStatus::EndOfMedia)
    {
        // 播放停止时执行的操作
        speechPlayOver();
        startNextPlayIfIdle();
    }
}

// 立即推进下一段生成/播放（事件驱动，减少轮询等待）
void Expend::startNextTTSIfIdle()
{
    if (!is_speech && !wait_speech_txt_list.isEmpty())
    {
        is_speech = true;
        const QString seg = wait_speech_txt_list.takeFirst();
        start_tts(seg);
    }
}

void Expend::startNextPlayIfIdle()
{
    if (!is_speech_play && !wait_speech_play_list.isEmpty())
    {
        is_speech_play = true;
        const QString path = wait_speech_play_list.takeFirst();
        speech_player->setMedia(QUrl::fromLocalFile(path));
        speech_player->play();
    }
}
