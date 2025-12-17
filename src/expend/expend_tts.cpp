
#include "expend.h"

#include "ui_expend.h"
// Bring in backend path resolver and path helpers to run tts.cpp robustly
#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

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
        // 文转声被禁用时：直接解锁状态，避免卡在 is_speech=true 导致后续无法朗读
        speechOver();
        return;
    }

    if (!speech_params.speech_name.isEmpty())
    {
        if (speech_params.speech_name == SPPECH_TTSCPP)
        {
            // 本地 tts.cpp：由子进程结束回调推进下一段
            tts_sys_speaking_ = false;
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
            // 系统语音：必须监听 stateChanged 才能在朗读结束后推进下一段
            if (sys_speech)
            {
                connect(sys_speech, &QTextToSpeech::stateChanged, this, &Expend::onSysSpeechStateChanged, Qt::UniqueConnection);
            }
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
            // 标记“当前段落由系统语音朗读中”，等待 Ready 状态时自动 speechOver()
            tts_sys_speaking_ = true;
            sys_speech->say(str);
#else
            ui->speech_log->appendPlainText("[info] Qt TextToSpeech support is disabled in this build.");
            speechOver();
#endif
        }
    }
}

#if defined(EVA_ENABLE_QT_TTS)
// 系统语音状态变化：用于在朗读结束时推进下一段
void Expend::onSysSpeechStateChanged(QTextToSpeech::State state)
{
    // 重置过程中会 stop()，此时不应继续推进（避免“重置后还在朗读/继续播放”）
    if (tts_resetting_)
    {
        tts_sys_speaking_ = false;
        is_speech = false;
        return;
    }

    // 只处理由本类发起的“段落朗读”
    if (!tts_sys_speaking_) return;

    // Ready = 当前朗读结束（或 stop 导致回到 Ready）
    if (state == QTextToSpeech::Ready)
    {
        tts_sys_speaking_ = false;
        speechOver();
        return;
    }

    // 后端报错：避免卡死在 is_speech=true，直接放行下一段
    if (state == QTextToSpeech::BackendError)
    {
        tts_sys_speaking_ = false;
        if (ui && ui->speech_log) ui->speech_log->appendPlainText("[error] system TTS backend error; skip.");
        speechOver();
        return;
    }
}
#endif


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
    if (!is_while) return;
    if (result.isEmpty()) return;

    // ---------------------------------------------------------------------
    // 文转声流式朗读（TTS）策略：
    // 1) 按“段落/换行”切分朗读，而不是按句号等标点切分（避免一句一句读太碎）。
    // 2) 不朗读两类内容：
    //    - 思考区：<think>..</think>
    //    - 工具调用区：<tool_call>..</tool_call>（其内容为 JSON，给工具层解析）
    //
    // 兼容性：流式输出可能把标签拆成多个 chunk，因此这里维护一个解析缓冲，
    //        并把“疑似标签前缀”的尾巴留到下一次再判断，避免读出 "<tool_" 之类碎片。
    // ---------------------------------------------------------------------

    // 累积到解析缓冲中，再统一剥离标签/过滤不可朗读区域
    tts_stream_buffer_ += result;

    const QString thinkBegin = QStringLiteral(DEFAULT_THINK_BEGIN);
    const QString thinkEnd = QStringLiteral(DEFAULT_THINK_END);
    const QString toolBegin = QStringLiteral("<tool_call>");
    const QString toolEnd = QStringLiteral(DEFAULT_OBSERVATION_STOPWORD); // </tool_call>

    auto findEarliestTag = [&](const QString &s, QString *tagOut) -> int
    {
        int bestPos = -1;
        QString bestTag;
        auto consider = [&](const QString &tag)
        {
            const int pos = s.indexOf(tag);
            if (pos < 0) return;
            if (bestPos < 0 || pos < bestPos)
            {
                bestPos = pos;
                bestTag = tag;
            }
        };
        consider(thinkBegin);
        consider(thinkEnd);
        consider(toolBegin);
        consider(toolEnd);
        if (tagOut) *tagOut = bestTag;
        return bestPos;
    };

    auto parseToolNameFromPayload = [&](const QString &payload) -> QString
    {
        // payload 通常是 JSON：{"name":"xxx","arguments":{...}}
        // 这里仅用于拿到 name，不需要完整解析 arguments。
        // 先截取最外层 {...} 再尝试 JSON 解析；失败再用正则兜底（容忍噪声/换行）。
        const int l = payload.indexOf('{');
        const int r = payload.lastIndexOf('}');
        if (l >= 0 && r > l)
        {
            const QByteArray bytes = payload.mid(l, r - l + 1).toUtf8();
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
            if (err.error == QJsonParseError::NoError && doc.isObject())
            {
                const QString name = doc.object().value(QStringLiteral("name")).toString().trimmed();
                if (!name.isEmpty()) return name;
            }
        }
        QRegularExpression re(QStringLiteral("\"name\"\\s*:\\s*\"([^\"]+)\""));
        const QRegularExpressionMatch m = re.match(payload);
        if (m.hasMatch()) return m.captured(1).trimmed();
        return QString();
    };

    // 收集本次可以被朗读的“可见文本”
    QString visible;

    // 1) 处理所有“完整标签”，把可朗读内容拼到 visible 中
    while (true)
    {
        QString hitTag;
        const int pos = findEarliestTag(tts_stream_buffer_, &hitTag);
        if (pos < 0 || hitTag.isEmpty()) break;

        const QString before = tts_stream_buffer_.left(pos);
        if (tts_in_tool_call_)
        {
            // 工具调用 JSON：只缓存不朗读
            tts_tool_call_buffer_ += before;
        }
        else if (!tts_in_think_)
        {
            // 普通可朗读内容
            visible += before;
        }

        // 消费掉 before + tag
        tts_stream_buffer_.remove(0, pos + hitTag.size());

        // 标签切换状态
        if (hitTag == thinkBegin)
        {
            tts_in_think_ = true;
        }
        else if (hitTag == thinkEnd)
        {
            tts_in_think_ = false;
        }
        else if (hitTag == toolBegin)
        {
            tts_in_tool_call_ = true;
            tts_tool_call_buffer_.clear();
        }
        else if (hitTag == toolEnd)
        {
            tts_in_tool_call_ = false;
            // 工具调用结束：默认只记录工具名（用于工具返回时播报），不朗读 JSON。
            // 但如果是 answer/response 工具，则需要把 arguments.content 当作最终回答朗读出来。
            QString toolName;
            QString answerContent;

            const QString payload = tts_tool_call_buffer_;
            const int l = payload.indexOf('{');
            const int r = payload.lastIndexOf('}');
            if (l >= 0 && r > l)
            {
                const QByteArray bytes = payload.mid(l, r - l + 1).toUtf8();
                QJsonParseError err;
                const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
                if (err.error == QJsonParseError::NoError && doc.isObject())
                {
                    const QJsonObject obj = doc.object();
                    toolName = obj.value(QStringLiteral("name")).toString().trimmed();
                    if (toolName == QStringLiteral("answer") || toolName == QStringLiteral("response"))
                    {
                        const QJsonObject args = obj.value(QStringLiteral("arguments")).toObject();
                        answerContent = args.value(QStringLiteral("content")).toString();
                    }
                }
            }

            if (toolName.isEmpty()) toolName = parseToolNameFromPayload(payload);

            if (toolName == QStringLiteral("answer") || toolName == QStringLiteral("response"))
            {
                const QString trimmed = answerContent.trimmed();
                if (!trimmed.isEmpty())
                {
                    if (!visible.isEmpty() && !visible.endsWith('\n')) visible += '\n';
                    visible += trimmed;
                    // 追加一个换行，确保该段落能被及时切分入队
                    if (!visible.endsWith('\n')) visible += '\n';
                }
            }
            else if (!toolName.isEmpty())
            {
                tts_last_tool_call_name_ = toolName;
            }
            tts_tool_call_buffer_.clear();
        }
    }

    // 2) 剩余内容里可能包含“标签前缀”（例如 "<tool_"），暂时保留到下一次
    auto keepTailLen = [&](const QString &s) -> int
    {
        const QStringList tags = {thinkBegin, thinkEnd, toolBegin, toolEnd};
        int keep = 0;
        const int maxCheck = qMin(s.size(), 16); // 标签都很短，检查尾部最多 16 个字符即可
        for (int len = 1; len <= maxCheck; ++len)
        {
            const QString suffix = s.right(len);
            for (const QString &t : tags)
            {
                if (t.startsWith(suffix))
                {
                    keep = qMax(keep, len);
                    break;
                }
            }
        }
        return keep;
    };

    const int keep = keepTailLen(tts_stream_buffer_);
    const int readyLen = tts_stream_buffer_.size() - keep;
    if (readyLen > 0)
    {
        const QString ready = tts_stream_buffer_.left(readyLen);
        if (tts_in_tool_call_)
            tts_tool_call_buffer_ += ready;
        else if (!tts_in_think_)
            visible += ready;
    }
    if (keep > 0)
        tts_stream_buffer_ = tts_stream_buffer_.right(keep);
    else
        tts_stream_buffer_.clear();

    if (visible.isEmpty()) return;

    // 3) 累计可朗读文本，并按“段落/换行”切分入队
    temp_speech_txt += visible;
    // 统一换行符（Windows \r\n / 老式 \r -> \n）
    temp_speech_txt.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    temp_speech_txt.replace('\r', '\n');

    // 按换行切分：一行/一段读一次；连续空行会被折叠
    while (true)
    {
        const int nl = temp_speech_txt.indexOf('\n');
        if (nl < 0) break;
        const QString seg = temp_speech_txt.left(nl).trimmed();
        if (!seg.isEmpty()) wait_speech_txt_list << seg;
        int removeLen = nl + 1;
        while (removeLen < temp_speech_txt.size() && temp_speech_txt.at(removeLen) == '\n')
        {
            ++removeLen;
        }
        temp_speech_txt.remove(0, removeLen);
    }

    // 兜底：没有换行但缓存过长时，按空白/定长切分，避免长时间不朗读
    const int MAX_BUF = 480; // 大约一段较长文本
    if (temp_speech_txt.size() > MAX_BUF)
    {
        int cut = -1;
        const int scanFrom = qMin(MAX_BUF, temp_speech_txt.size());
        for (int i = scanFrom - 1; i >= 0; --i)
        {
            if (temp_speech_txt.at(i).isSpace())
            {
                cut = i + 1; // 保留空白作为分隔
                break;
            }
        }
        if (cut < 0) cut = scanFrom;
        if (cut < 80) cut = qMin(scanFrom, temp_speech_txt.size()); // 避免切得太短导致过碎
        const QString seg = temp_speech_txt.left(cut).trimmed();
        if (!seg.isEmpty()) wait_speech_txt_list << seg;
        temp_speech_txt.remove(0, cut);
    }

    // 事件驱动推进
    startNextTTSIfIdle();
}

// 一轮推理结束：若缓冲里还有尾段（无终止标点），也加入待读列表
void Expend::onNetTurnDone()
{
    // 回合结束：把尚未遇到换行的“尾段”也加入队列，确保无换行结尾也能朗读
    const QString tail = temp_speech_txt.trimmed();
    if (!tail.isEmpty()) wait_speech_txt_list << tail;
    temp_speech_txt.clear();

    // 防止标签残留跨回合污染：回合结束后直接清空解析缓冲并重置状态
    // 注意：tts_last_tool_call_name_ 需要保留到“工具返回”时使用，因此不在此清空
    tts_stream_buffer_.clear();
    tts_tool_call_buffer_.clear();
    tts_in_think_ = false;
    tts_in_tool_call_ = false;
    // 立即推进朗读/播放
    startNextTTSIfIdle();
    startNextPlayIfIdle();
}

// 工具返回：不朗读工具细节，只播报“模型调用xxx工具 / The model calls the xxx tool”
void Expend::recv_toolpushover(QString tool_result_)
{
    Q_UNUSED(tool_result_);
    if (!speech_params.enable_speech) return;

    // 说明：tool_result_ 的内容往往很长且包含大量细节，不适合朗读；
    // 这里只播报一条提示语。工具名优先来自 <tool_call> 的 JSON（更准确），
    // 若解析失败再从 tool_result_ 的首个 token 做兜底提取。
    QString toolName = tts_last_tool_call_name_.trimmed();
    if (toolName.isEmpty())
    {
        QString s = tool_result_.trimmed();
        const int nl = s.indexOf('\n');
        if (nl >= 0) s = s.left(nl);
        if (s.startsWith(QStringLiteral("<ylsdamxssjxxdd:showdraw>")))
        {
            toolName = QStringLiteral("stablediffusion");
        }
        else
        {
            int end = 0;
            while (end < s.size() && !s.at(end).isSpace()) ++end;
            toolName = s.left(end).trimmed();
        }
    }
    if (toolName.isEmpty()) return;

    QString speak;
    if (language_flag == 0)
        speak = QStringLiteral("模型调用%1工具").arg(toolName);
    else
        speak = QStringLiteral("The model calls the %1 tool").arg(toolName);

    wait_speech_txt_list << speak;
    // 使用一次就清空，避免后续误用旧工具名
    tts_last_tool_call_name_.clear();
    startNextTTSIfIdle();
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
    // 重置是“强终止”：停止当前朗读/播放，并清空所有队列与解析状态
    tts_resetting_ = true;

    // 先停掉轮询，避免 reset 过程被 timeout 回调打断导致状态错乱
    speechTimer.stop();
    speechPlayTimer.stop();

    temp_speech_txt = "";          // 清空待读列表
    wait_speech_txt_list.clear();  // 清空待读列表
    wait_speech_play_list.clear(); // 清空待读列表
    // 重置流式解析状态，避免跨对话残留
    tts_stream_buffer_.clear();
    tts_tool_call_buffer_.clear();
    tts_last_tool_call_name_.clear();
    tts_in_think_ = false;
    tts_in_tool_call_ = false;
    tts_sys_speaking_ = false;
#if defined(EVA_ENABLE_QT_TTS)
    // 无论是否能枚举到 voice，都要 stop()；否则某些环境会出现“重置后仍在朗读”
    if (sys_speech) sys_speech->stop();
#endif

    // 终止 tts.cpp 合成进程：blockSignals 避免 kill -> finished 回调又往播放队列里塞文件
    if (tts_process)
    {
        const QSignalBlocker blocker(tts_process);
        if (tts_process->state() != QProcess::NotRunning)
        {
            tts_process->kill();
            tts_process->waitForFinished(150);
        }
    }
    // 停止正在播放的音频
    if (speech_player) speech_player->stop();
    is_speech = false;
    is_speech_play = false;
    removeDir(ttsOutputDir); // 清空产生的音频

    // 恢复轮询（兜底），并解除 reset 屏蔽
    tts_resetting_ = false;
    speechTimer.start(500);
    speechPlayTimer.start(500);
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
    // 重置过程中 kill() 也会触发 finished，这里必须直接丢弃，否则会出现“重置后还在播放”
    if (tts_resetting_)
    {
        is_speech = false;
        return;
    }

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
    if (tts_resetting_)
    {
        is_speech_play = false;
        return;
    }
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



