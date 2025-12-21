
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

namespace
{
// 判断一行输出是否“像”一个音色 id（tts.cpp 的 voice id 通常是纯英文/数字/下划线组合）
// 说明：这里不用 QRegularExpression，避免引入 PCRE2 JIT 的兼容性风险。
bool isTtscppVoiceIdCandidate(const QString &s)
{
    const QString t = s.trimmed();
    if (t.isEmpty()) return false;
    for (int i = 0; i < t.size(); ++i)
    {
        const QChar c = t.at(i);
        const bool ok = (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-'));
        if (!ok) return false;
    }
    return true;
}
} // namespace

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
    // 优先使用 userData（避免界面翻译导致 currentText 变化）
    QString source = ui->speech_source_comboBox->currentData().toString().trimmed();
    if (source.isEmpty()) source = ui->speech_source_comboBox->currentText().trimmed();

    // 若系统 TTS 不可用，则强制回退到 tts.cpp，避免选中后无法朗读导致体验“像坏了一样”
    if (source == QLatin1String(SPPECH_SYSTEM) && !is_sys_speech_available)
    {
        ui->speech_log->appendPlainText("[warn] system TTS is not available, fallback to tts.cpp");
        const QSignalBlocker blocker(ui->speech_source_comboBox);
        const int idx = ui->speech_source_comboBox->findData(QStringLiteral(SPPECH_TTSCPP));
        if (idx >= 0) ui->speech_source_comboBox->setCurrentIndex(idx);
        source = QStringLiteral(SPPECH_TTSCPP);
    }

    speech_params.speech_source = source;
    // Persist selection so reload keeps the same source/voice
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    settings.setValue("speech_source", speech_params.speech_source);

    const bool useLocalTts = (speech_params.speech_source == QLatin1String(SPPECH_TTSCPP));
    // 同一行上同时放了“模型选择 + 可用音色”，这里不能直接禁用整行，否则 system 声源时无法选音色。
    // 仅禁用 tts.cpp 模型相关控件；音色下拉框由 refreshSpeechVoiceUi() 负责启用/禁用。
    if (ui->speech_ttscpp_modelpath_frame) ui->speech_ttscpp_modelpath_frame->setEnabled(true);
    if (ui->speech_ttscpp_modelpath_label) ui->speech_ttscpp_modelpath_label->setEnabled(useLocalTts);
    if (ui->speech_ttscpp_modelpath_lineEdit) ui->speech_ttscpp_modelpath_lineEdit->setEnabled(useLocalTts);
    if (ui->speech_ttscpp_modelpath_pushButton) ui->speech_ttscpp_modelpath_pushButton->setEnabled(useLocalTts);
    ui->speech_manual_frame->setEnabled(useLocalTts);

    // 切换声源后，刷新“可用音色”下拉框
    refreshSpeechVoiceUi();
}


// 添加可用声源
void Expend::set_sys_speech(QStringList avaliable_speech_list)
{
    // 刷新“声源”下拉框：只保留 tts.cpp / system 两项
    // 注：avaliable_speech_list 这里约定传入的是“声源 key 列表”，而不是音色列表
    const QSignalBlocker blocker(ui->speech_source_comboBox);
    ui->speech_source_comboBox->clear();
    for (int i = 0; i < avaliable_speech_list.size(); ++i)
    {
        const QString key = avaliable_speech_list.at(i).trimmed();
        if (key == QLatin1String(SPPECH_TTSCPP))
        {
            ui->speech_source_comboBox->addItem(QStringLiteral(SPPECH_TTSCPP), QStringLiteral(SPPECH_TTSCPP));
        }
        else if (key == QLatin1String(SPPECH_SYSTEM))
        {
            // 显示使用双语资源：中文=系统，英文=system
            ui->speech_source_comboBox->addItem(jtr(QStringLiteral("role_system")), QStringLiteral(SPPECH_SYSTEM));
        }
    }

    // 回填当前声源（优先按 data 匹配）
    int idx = ui->speech_source_comboBox->findData(speech_params.speech_source);
    if (idx < 0) idx = ui->speech_source_comboBox->findText(speech_params.speech_source);
    if (idx >= 0) ui->speech_source_comboBox->setCurrentIndex(idx);
    ui->speech_enable_radioButton->setChecked(speech_params.enable_speech);
}

// 用户切换音色响应（tts.cpp/system 共用一个下拉框）
void Expend::speech_voice_change()
{
    const QString voice = ui->speech_voice_comboBox->currentText().trimmed();

    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    if (speech_params.speech_source == QLatin1String(SPPECH_TTSCPP))
    {
        speech_params.ttscpp_voice = voice;
        settings.setValue("speech_ttscpp_voice", speech_params.ttscpp_voice);
    }
    else if (speech_params.speech_source == QLatin1String(SPPECH_SYSTEM))
    {
        speech_params.system_voice = voice;
        settings.setValue("speech_system_voice", speech_params.system_voice);

#if defined(EVA_ENABLE_QT_TTS)
        // 即时生效：下次朗读会用新音色；若当前正在朗读则不强制打断
        if (!sys_speech) sys_speech = new QTextToSpeech(this);
        if (sys_speech && !speech_params.system_voice.isEmpty())
        {
            for (const QVoice &v : sys_speech->availableVoices())
            {
                if (v.name() == speech_params.system_voice)
                {
                    sys_speech->setVoice(v);
                    break;
                }
            }
        }
#endif
    }
}

// 用户手动编辑 tts.cpp 模型路径后触发：落盘 + 自动枚举音色
void Expend::speech_ttscpp_modelpath_change()
{
    const QString modelPath = ui->speech_ttscpp_modelpath_lineEdit->text().trimmed();

    // 立即持久化，确保下次启动仍能找到模型
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    settings.setValue("ttscpp_modelpath", modelPath);
    // 清理历史字段（OuteTTS 时代遗留）
    settings.remove("outetts_modelpath");
    settings.remove("wavtokenizer_modelpath");

    // 模型路径变化时清空缓存，避免把旧音色列表套到新模型上
    if (modelPath != ttscpp_voice_list_modelpath_)
    {
        ttscpp_voice_list_cache_.clear();
        ttscpp_voice_list_modelpath_.clear();
    }

    // 只有在当前选择 tts.cpp 声源时才触发枚举（避免用户选择 system 时仍频繁跑 CLI）
    if (speech_params.speech_source == QLatin1String(SPPECH_TTSCPP))
        requestTtscppVoiceList();
}

// 按当前声源刷新“可用音色”
void Expend::refreshSpeechVoiceUi()
{
    if (!ui || !ui->speech_voice_comboBox) return;

    // 声源为 tts.cpp：从 tts-cli --list-voices 获取
    if (speech_params.speech_source == QLatin1String(SPPECH_TTSCPP))
    {
        ui->speech_voice_frame->setEnabled(true);
        requestTtscppVoiceList();
        return;
    }

    // 声源为 system：从 Qt TextToSpeech 枚举
    if (speech_params.speech_source == QLatin1String(SPPECH_SYSTEM))
    {
#if defined(EVA_ENABLE_QT_TTS)
        if (!sys_speech) sys_speech = new QTextToSpeech(this);
        QStringList voices;
        is_sys_speech_available = false;
        if (sys_speech)
        {
            const auto vs = sys_speech->availableVoices();
            if (!vs.isEmpty())
            {
                is_sys_speech_available = true;
                for (const QVoice &v : vs) voices << v.name();
            }
        }
        ui->speech_voice_frame->setEnabled(is_sys_speech_available);
        applyVoiceComboItems(voices, speech_params.system_voice);
        return;
#else
        ui->speech_voice_frame->setEnabled(false);
        applyVoiceComboItems(QStringList{}, QString{});
        return;
#endif
    }

    // 未知声源：清空
    ui->speech_voice_frame->setEnabled(false);
    applyVoiceComboItems(QStringList{}, QString{});
}

// 填充“可用音色”下拉框并回填选择（同时更新参数与配置落盘）
void Expend::applyVoiceComboItems(const QStringList &voices, const QString &preferred)
{
    if (!ui || !ui->speech_voice_comboBox) return;

    const QSignalBlocker blocker(ui->speech_voice_comboBox);
    ui->speech_voice_comboBox->clear();

    for (const QString &v : voices)
        ui->speech_voice_comboBox->addItem(v);

    // 选择默认音色的优先级：
    // 1) 用户已保存/当前传入的 preferred 且在列表中：尊重用户选择
    // 2) 若音色列表包含 DEFAULT_TTSCPP_VOICE_ID（例如 zf_001）：作为默认音色（开箱即用）
    // 3) 兜底：选择列表第一个
    QString chosen;
    const QString preferredTrimmed = preferred.trimmed();
    if (!preferredTrimmed.isEmpty() && voices.contains(preferredTrimmed))
        chosen = preferredTrimmed;
    else if (voices.contains(QStringLiteral(DEFAULT_TTSCPP_VOICE_ID)))
        chosen = QStringLiteral(DEFAULT_TTSCPP_VOICE_ID);
    else if (!voices.isEmpty())
        chosen = voices.first();

    if (!chosen.isEmpty()) ui->speech_voice_comboBox->setCurrentText(chosen);
    ui->speech_voice_comboBox->setEnabled(!voices.isEmpty());

    // 下拉框被 blocker 屏蔽信号，这里需要手动同步到参数与配置
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    if (speech_params.speech_source == QLatin1String(SPPECH_TTSCPP))
    {
        speech_params.ttscpp_voice = chosen;
        settings.setValue("speech_ttscpp_voice", speech_params.ttscpp_voice);
    }
    else if (speech_params.speech_source == QLatin1String(SPPECH_SYSTEM))
    {
        speech_params.system_voice = chosen;
        settings.setValue("speech_system_voice", speech_params.system_voice);
    }
}

// 启动 tts-cli --list-voices 并解析输出填充下拉框
void Expend::requestTtscppVoiceList()
{
    if (!ui || !ui->speech_voice_comboBox) return;

    const QString modelPath = ui->speech_ttscpp_modelpath_lineEdit->text().trimmed();
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath))
    {
        ui->speech_log->appendPlainText("[warn] tts.cpp model path is empty or not found; cannot list voices.");
        ui->speech_voice_comboBox->clear();
        ui->speech_voice_comboBox->setEnabled(false);
        return;
    }

    // 如果模型路径未变且缓存非空，直接使用缓存，避免重复跑 CLI
    if (modelPath == ttscpp_voice_list_modelpath_ && !ttscpp_voice_list_cache_.isEmpty())
    {
        applyVoiceComboItems(ttscpp_voice_list_cache_, speech_params.ttscpp_voice);
        return;
    }

    const QString program = DeviceManager::programPath(QStringLiteral("tts-cli"));
    if (program.isEmpty() || !QFileInfo::exists(program))
    {
        ui->speech_log->appendPlainText("[error] tts-cli not found; cannot list voices.");
        ui->speech_voice_comboBox->clear();
        ui->speech_voice_comboBox->setEnabled(false);
        return;
    }

    // 若上一次枚举仍在运行，先静默终止（避免 finished 回调把“半截输出”当成有效结果）
    if (tts_list_process && tts_list_process->state() != QProcess::NotRunning)
    {
        const QSignalBlocker blocker(tts_list_process);
        tts_list_process->kill();
        tts_list_process->waitForFinished(150);
    }

    // UI 先给出“加载中”状态，避免用户误以为无音色
    {
        const QSignalBlocker blocker(ui->speech_voice_comboBox);
        ui->speech_voice_comboBox->clear();
        ui->speech_voice_comboBox->addItem(QStringLiteral("loading..."));
        ui->speech_voice_comboBox->setEnabled(false);
    }

    // 记录本次模型路径，用于缓存判断
    ttscpp_voice_list_modelpath_ = modelPath;
    ttscpp_voice_list_cache_.clear();

    QStringList arguments;
    arguments << QStringLiteral("--model-path") << ensureToolFriendlyFilePath(modelPath);
    arguments << QStringLiteral("--list-voices");

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString toolDir = QFileInfo(program).absolutePath();
#ifdef _WIN32
    env.insert("PATH", toolDir + ";" + env.value("PATH"));
#elif __APPLE__
    env.insert("DYLD_LIBRARY_PATH", toolDir + ":" + env.value("DYLD_LIBRARY_PATH"));
#else
    env.insert("LD_LIBRARY_PATH", toolDir + ":" + env.value("LD_LIBRARY_PATH"));
#endif

    createTempDirectory(ttsOutputDir);
    tts_list_process->setProcessEnvironment(env);
    tts_list_process->setWorkingDirectory(ttsOutputDir);
    tts_list_process->start(program, arguments);
}

// tts.cpp --list-voices 执行结束：解析输出并填充下拉框
void Expend::tts_list_onProcessFinished()
{
    if (!tts_list_process) return;

    const int exitCode = tts_list_process->exitCode();
    const QProcess::ExitStatus exitStatus = tts_list_process->exitStatus();
    const QString stdoutText = QString::fromUtf8(tts_list_process->readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(tts_list_process->readAllStandardError());

    if (exitStatus != QProcess::NormalExit || exitCode != 0)
    {
        ui->speech_log->appendPlainText(QStringLiteral("[warn] list voices failed (exitCode=%1)").arg(exitCode));
        if (!stderrText.trimmed().isEmpty()) ui->speech_log->appendPlainText(stderrText.trimmed());
    }

    QStringList voices;
    QString normalized = stdoutText;
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = normalized.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines)
    {
        const QString t = line.trimmed();
        if (!isTtscppVoiceIdCandidate(t)) continue;
        if (!voices.contains(t)) voices << t;
    }

    ttscpp_voice_list_cache_ = voices;

    // 如果用户此刻不在 tts.cpp 声源上，不要强行刷新 UI（但缓存已更新，切回来会立即可用）
    if (speech_params.speech_source != QLatin1String(SPPECH_TTSCPP)) return;

    if (voices.isEmpty())
    {
        ui->speech_log->appendPlainText("[warn] no voices found from tts-cli --list-voices");
        ui->speech_voice_comboBox->clear();
        ui->speech_voice_comboBox->setEnabled(false);
        return;
    }

    applyVoiceComboItems(voices, speech_params.ttscpp_voice);
    ui->speech_log->appendPlainText(QStringLiteral("[info] tts.cpp voices loaded: %1").arg(voices.size()));
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

    const QString source = speech_params.speech_source.trimmed();
    if (!source.isEmpty())
    {
        if (source == QLatin1String(SPPECH_TTSCPP))
        {
            // 本地 tts.cpp：由子进程结束回调推进下一段
            tts_sys_speaking_ = false;
            const QString modelPath = ui->speech_ttscpp_modelpath_lineEdit->text().trimmed();
            if (!modelPath.isEmpty())
                runTtsProcess(str);
            else
            {
                ui->speech_log->appendPlainText("[warn] tts.cpp model path is empty; skip synthesis.");
                speechOver();
            }
        }
        else if (source == QLatin1String(SPPECH_SYSTEM))
        {
#if defined(EVA_ENABLE_QT_TTS)
            if (!sys_speech) sys_speech = new QTextToSpeech(this);
            // 系统语音：必须监听 stateChanged 才能在朗读结束后推进下一段
            if (sys_speech)
            {
                connect(sys_speech, &QTextToSpeech::stateChanged, this, &Expend::onSysSpeechStateChanged, Qt::UniqueConnection);
            }
            // 选择用户在“可用音色”里选中的系统音色
            const QString want = speech_params.system_voice.trimmed();
            if (!want.isEmpty() && sys_speech)
            {
                for (const QVoice &voice : sys_speech->availableVoices())
                {
                    if (voice.name() == want)
                    {
                        sys_speech->setVoice(voice);
                        break;
                    }
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
        else
        {
            // 未知声源：直接放行，避免卡死
            ui->speech_log->appendPlainText("[warn] unknown speech source, skip.");
            speechOver();
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
    if (language_flag == EVA_LANG_ZH)
        speak = QStringLiteral("模型调用%1工具").arg(toolName);
    else if (language_flag == EVA_LANG_JA)
        speak = QStringLiteral("モデルは%1ツールを呼び出しました").arg(toolName);
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

    // 用户在“可用音色”下拉框中选择的音色（tts.cpp 使用 --voice 指定）
    const QString voice = speech_params.ttscpp_voice.trimmed();
    if (!voice.isEmpty()) arguments << QStringLiteral("--voice") << voice;

    // ---------------------------------------------------------------------
    // tts.cpp 的 `--lang` 参数用于控制“数字读法”等语言偏好（zh / en / ja）。
    // EVA 的界面语言现在支持：中文/英文/日文，但 tts.cpp 目前仅支持 zh/en，
    // 因此这里做一个明确映射：
    // - 中文界面：zh（也是 tts.cpp 默认值，但显式传入更可控）
    // - 英文/日文界面：en（兜底到英文读法，避免日文界面仍用中文数字读法）
    // ---------------------------------------------------------------------
    // tts.cpp 当前支持 `--lang zh/en/ja`：按界面语言自动映射，未知值兜底为 en。
    QString ttsLang = QStringLiteral("en");
    if (language_flag == EVA_LANG_ZH) ttsLang = QStringLiteral("zh");
    else if (language_flag == EVA_LANG_JA) ttsLang = QStringLiteral("ja");
    arguments << QStringLiteral("--lang") << ttsLang;

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
    // 统一走“模型路径变化”逻辑：落盘 + 自动枚举音色
    speech_ttscpp_modelpath_change();
}


// 用户点击选择模型路径时响应


// 用户点击转为音频按钮时响应
void Expend::on_speech_manual_pushButton_clicked()
{
    if (speech_params.speech_source != QLatin1String(SPPECH_TTSCPP))
    {
        ui->speech_log->appendPlainText("[warn] manual synthesis is only supported for tts.cpp source.");
        return;
    }

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



