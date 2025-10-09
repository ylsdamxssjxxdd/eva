#include "expend.h"

#include "ui_expend.h"
#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"

//-------------------------------------------------------------------------
//----------------------------------声转文相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择whisper路径时响应
void Expend::on_whisper_load_modelpath_button_clicked()
{
    currentpath = customOpenfile(currentpath, "choose whisper model", "(*.bin *.gguf)");
    whisper_params.model = currentpath.toStdString();
    ui->whisper_load_modelpath_linedit->setText(currentpath);
    emit expend2ui_whisper_modelpath(currentpath);
    ui->whisper_log->setPlainText(jtr("once selected, you can record by pressing f2"));
}

//开始语音转文字
void Expend::recv_speechdecode(QString wavpath, QString out_format)
{
    whisper_time.restart();

    const QString localPath = DeviceManager::programPath(QStringLiteral("whisper-cli"));

    //将wav文件重采样为16khz音频文件
#ifdef _WIN32
    QTextCodec *code = QTextCodec::codecForName("GB2312"); // mingw中文路径支持
    std::string wav_path_c = code->fromUnicode(wavpath).data();
#elif __linux__
    std::string wav_path_c = wavpath.toStdString();
#endif
    resampleWav(wav_path_c, wav_path_c);

    // 设置要运行的exe文件的路径
    QString program = localPath; if (program.isEmpty() || !QFileInfo::exists(program)) { ui->whisper_log->appendPlainText("[error] whisper backend not found under current device folder"); return; }
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;

    // Convert potentially non-ASCII paths to tool-friendly form on Windows
    const QString modelPathArg = ensureToolFriendlyFilePath(ui->whisper_load_modelpath_linedit->text());
    const QString wavPathArg   = ensureToolFriendlyFilePath(wavpath);
    arguments << "-m" << modelPathArg;       // 模型路径（可能为 8.3 短路径）
    arguments << "-f" << wavPathArg;         // wav 文件路径（可能为 8.3 短路径）
    arguments << "--language" << QString::fromStdString(whisper_params.language); //识别语种
    arguments << "--threads" << QString::number(max_thread * 0.5);
    if (out_format == "txt")
    {
        arguments << "--output-txt";
    } //结果输出为一个txt
    else if (out_format == "srt")
    {
        arguments << "--output-srt";
    }
    else if (out_format == "csv")
    {
        arguments << "--output-csv";
    }
    else if (out_format == "json")
    {
        arguments << "--output-json";
    }

    // 开始运行程序
    //连接信号和槽,获取程序的输出
    connect(whisper_process, &QProcess::readyReadStandardOutput, [=]() {
        QString output = whisper_process->readAllStandardOutput();
        ui->whisper_log->appendPlainText(output);
    });
    connect(whisper_process, &QProcess::readyReadStandardError, [=]() {
        QString output = whisper_process->readAllStandardError();
        ui->whisper_log->appendPlainText(output);
    });

    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    // Add tool dir to library search path and set working directory
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString toolDir = QFileInfo(program).absolutePath();
#ifdef _WIN32
    env.insert("PATH", toolDir + ";" + env.value("PATH"));
#elif __APPLE__
    env.insert("DYLD_LIBRARY_PATH", toolDir + ":" + env.value("DYLD_LIBRARY_PATH"));
#else
    env.insert("LD_LIBRARY_PATH", toolDir + ":" + env.value("LD_LIBRARY_PATH"));
#endif
    whisper_process->setProcessEnvironment(env);
    whisper_process->setWorkingDirectory(toolDir);
    whisper_process->start(program, arguments);
}

void Expend::whisper_onProcessStarted()
{
    if (!is_handle_whisper)
    {
        emit expend2ui_state("expend:" + jtr("calling whisper to decode recording"), USUAL_SIGNAL);
    }
}

void Expend::whisper_onProcessFinished()
{
    if (!is_handle_whisper)
    {
        QString content;
        // 文件路径
        QString filePath = applicationDirPath + "/EVA_TEMP/" + QString("EVA_") + ".wav.txt";
        // 创建 QFile 对象
        QFile file(filePath);
        // 打开文件
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file); // 创建 QTextStream 对象
            in.setCodec("UTF-8");
            content = in.readAll(); // 读取文件内容
        }
        file.close();
        emit expend2ui_state("expend:" + jtr("decode over") + " " + QString::number(whisper_time.nsecsElapsed() / 1000000000.0, 'f', 2) + "s ->" + content, SUCCESS_SIGNAL);
        emit expend2ui_speechdecode_over(content);
    }
    else
    {
        ui->whisper_log->appendPlainText(jtr("the result has been saved in the source wav file directory") + " " + QString::number(whisper_time.nsecsElapsed() / 1000000000.0, 'f', 2) + "s");
    }
    is_handle_whisper = false;
}

//用户点击选择wav路径时响应
void Expend::on_whisper_wavpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose a .wav file", "(*.wav)");
    wavpath = currentpath;
    if (wavpath == "")
    {
        return;
    }
    ui->whisper_wavpath_lineedit->setText(wavpath);
}
//用户点击执行转换时响应
void Expend::on_whisper_execute_pushbutton_clicked()
{
    //执行whisper
    is_handle_whisper = true;
    whisper_process->kill();
    recv_speechdecode(ui->whisper_wavpath_lineedit->text(), ui->whisper_output_format->currentText());
}



