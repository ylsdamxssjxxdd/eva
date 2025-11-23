
#include "expend.h"

#include "ui_expend.h"
// Bring in backend path resolver and path helpers to run tts.cpp robustly
#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"
#include <QDateTime>
#include <QFileInfo>

//-------------------------------------------------------------------------
//----------------------------------文转声相关--------------------------------
//-------------------------------------------------------------------------

// 用户点击启用声音选项响应
void Expend::speech_enable_change()
{
    const bool on = ui->speech_enable_radioButton->isChecked();
    speech_params.enable_speech = on;
    // Persist immediately so it survives restarts
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    settings.setValue("speech_enable", on);
}

// 用户切换声源响应
void Expend::speech_source_change()
{
    speech_params.speech_name = ui->speech_source_comboBox->currentText();
    // Persist selection so reload keeps the same voice
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    settings.setValue("speech_name", speech_params.speech_name);

    const bool useLocalTts = (speech_params.speech_name == SPPECH_TTSCPP);
    ui->speech_ttscpp_modelpath_frame->setEnabled(useLocalTts);
}


// 添加可用声源
void Expend::set_sys_speech(QStringList avaliable_speech_list)
{
    // Refresh the combo with newly discovered sources
    ui->speech_source_comboBox->clear();
    for (int i = 0; i < avaliable_speech_list.size(); ++i)
    {
        ui->speech_source_comboBox->addItem(avaliable_speech_list.at(i)); // add to dropdown
    }
    ui->speech_source_comboBox->setCurrentText(speech_params.speech_name);
    ui->speech_enable_radioButton->setChecked(speech_params.enable_speech);
}

// 开始文字转语音
void Expend::start_tts(QString str)
{
    if (!speech_params.enable_speech)
    {
        speechOver();
        return;
    }

    if (!speech_params.speech_name.isEmpty())
    {
        if (speech_params.speech_name == SPPECH_TTSCPP)
        {
            const QString modelPath = ui->speech_ttscpp_modelpath_lineEdit->text().trimmed();
            if (!modelPath.isEmpty())
            {
                runTtsProcess(str);
            }
            else
            {
                ui->speech_log->appendPlainText("[warn] tts.cpp model path is empty; skip synthesis.");
                speechOver();
            }
        }
        else
        {
#if defined(EVA_ENABLE_QT_TTS)
            if (!sys_speech) sys_speech = new QTextToSpeech(this);
            foreach (const QVoice &voice, sys_speech->availableVoices())
            {
                if (voice.name() == speech_params.speech_name)
                {
                    sys_speech->setVoice(voice);
                    break;
                }
            }
            sys_speech->setRate(0.3);
            sys_speech->setVolume(1.0);
            sys_speech->say(str);
#else
            ui->speech_log->appendPlainText("[info] Qt TextToSpeech support is disabled in this build.");
            speechOver();
#endif
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
        QRegularExpression re(QString::fromUtf8("([。！？]|(?<!\\d)[.!?](?=\\s|$)|[；;：:,，、])"));

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
    // Safety guard: only delete inside EVA_TEMP; ignore empty/unsafe paths
    const QString trimmed = dirName.trimmed();
    if (trimmed.isEmpty()) return false;
    const QString abs = QDir(trimmed).absolutePath();
    const QString tempRoot = QDir(applicationDirPath).filePath("EVA_TEMP");
    const QString tempAbs = QDir(tempRoot).absolutePath();
#ifdef _WIN32
    auto startsWithCi = [](const QString &a, const QString &b)
    { return a.toLower().startsWith(b.toLower() + QDir::separator()); };
    if (!(abs.toLower() == tempAbs.toLower() || startsWithCi(abs, tempAbs))) return false;
#else
    if (!(abs == tempAbs || abs.startsWith(tempAbs + QDir::separator()))) return false;
#endif
    QDir dir(abs);
    if (!dir.exists()) return true;
    // Delete entries recursively
    const auto infos = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &item : infos)
    {
        if (item.isDir())
        {
            if (!removeDir(item.absoluteFilePath())) return false;
        }
        else
        {
            if (!QFile::remove(item.absoluteFilePath())) return false;
        }
    }
    // Finally remove the directory itself
    return dir.rmdir(abs);
}

// 收到重置信号
void Expend::recv_resettts()
{
    temp_speech_txt = "";          // 清空待读列表
    wait_speech_txt_list.clear();  // 清空待读列表
    wait_speech_play_list.clear(); // 清空待读列表
#if defined(EVA_ENABLE_QT_TTS)
    if (is_sys_speech_available)
    {
        sys_speech->stop(); // 停止朗读
    }
#endif

    tts_process->kill(); // 终止继续生成
    speech_player->stop();
    removeDir(ttsOutputDir); // 清空产生的音频
}

// Run tts.cpp CLI to synthesize speech
void Expend::runTtsProcess(const QString &text)
{
    const QString modelPath = ui->speech_ttscpp_modelpath_lineEdit->text().trimmed();
    if (!QFileInfo::exists(modelPath))
    {
        ui->speech_log->appendPlainText("[error] tts.cpp model path not found");
        speechOver();
        return;
    }

    const QString program = DeviceManager::programPath(QStringLiteral("tts-cli"));
    if (program.isEmpty() || !QFileInfo::exists(program))
    {
        ui->speech_log->appendPlainText("[error] tts-cli not found under current device folder");
        ui->speech_log->appendPlainText(QString("[hint] search root=%1, arch=%2, os=%3, device=%4")
                                            .arg(DeviceManager::backendsRootDir())
                                            .arg(DeviceManager::currentArchId())
                                            .arg(DeviceManager::currentOsId())
                                            .arg(DeviceManager::effectiveBackend()));
        speechOver();
        return;
    }

    createTempDirectory(ttsOutputDir);
    const QString produced = QDir(ttsOutputDir).filePath("TTS.cpp.wav");
    if (QFile::exists(produced)) QFile::remove(produced);

    ttsLastOutputFile = QDir(ttsOutputDir).filePath(QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz") + ".wav");

    QStringList arguments;
    arguments << QStringLiteral("--model-path") << ensureToolFriendlyFilePath(modelPath);
    arguments << QStringLiteral("-p") << text;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString toolDir = QFileInfo(program).absolutePath();
#ifdef _WIN32
    env.insert("PATH", toolDir + ";" + env.value("PATH"));
#elif __APPLE__
    env.insert("DYLD_LIBRARY_PATH", toolDir + ":" + env.value("DYLD_LIBRARY_PATH"));
#else
    env.insert("LD_LIBRARY_PATH", toolDir + ":" + env.value("LD_LIBRARY_PATH"));
#endif
    tts_process->setProcessEnvironment(env);
    tts_process->setWorkingDirectory(ttsOutputDir);

    tts_process->start(program, arguments);
}


// 进程开始响应
void Expend::tts_onProcessStarted() {}

// 进程结束响应
void Expend::tts_onProcessFinished()
{
    QString playbackPath;
    const QString produced = QDir(ttsOutputDir).filePath("TTS.cpp.wav");
    if (QFileInfo::exists(produced))
    {
        if (ttsLastOutputFile.isEmpty())
        {
            ttsLastOutputFile = QDir(ttsOutputDir).filePath(QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz") + ".wav");
        }
        QFile::remove(ttsLastOutputFile);
        if (QFile::rename(produced, ttsLastOutputFile))
        {
            playbackPath = ttsLastOutputFile;
        }
        else if (QFile::copy(produced, ttsLastOutputFile))
        {
            QFile::remove(produced);
            playbackPath = ttsLastOutputFile;
        }
        else
        {
            playbackPath = produced;
        }
    }

    if (!playbackPath.isEmpty() && QFileInfo::exists(playbackPath))
    {
        wait_speech_play_list << playbackPath;
    }
    else
    {
        ui->speech_log->appendPlainText("[error] tts.cpp finished but no output file produced");
    }

    speechOver();
}


void Expend::readyRead_tts_process_StandardOutput()
{
    QString output = tts_process->readAllStandardOutput();
    ui->speech_log->appendPlainText(output);
}


void Expend::readyRead_tts_process_StandardError()
{
    QString output = tts_process->readAllStandardError();
    ui->speech_log->appendPlainText(output);
}


// 用户点击选择模型路径时响应
void Expend::on_speech_ttscpp_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose tts.cpp model", "(*.gguf)");
    ui->speech_ttscpp_modelpath_lineEdit->setText(currentpath);
    if (!currentpath.isEmpty())
    {
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        settings.setValue("ttscpp_modelpath", currentpath);
        settings.remove("outetts_modelpath");
        settings.remove("wavtokenizer_modelpath");
    }
}


// 用户点击选择模型路径时响应


// 用户点击转为音频按钮时响应
void Expend::on_speech_manual_pushButton_clicked()
{
    if (!ui->speech_ttscpp_modelpath_lineEdit->text().trimmed().isEmpty())
    {
        runTtsProcess(ui->speech_manual_plainTextEdit->toPlainText());
    }
    else
    {
        ui->speech_log->appendPlainText("[warn] tts.cpp model path is empty; cannot synthesize.");
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



