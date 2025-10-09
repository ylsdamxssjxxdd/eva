
#include "expend.h"

#include "ui_expend.h"

//-------------------------------------------------------------------------
//----------------------------------模型转换相关--------------------------------
//-------------------------------------------------------------------------

//用户点击选择原始模型目录响应
void Expend::on_modelconvert_modelpath_pushButton_clicked()
{
    convertmodeldir = QFileDialog::getExistingDirectory(this, jtr("modelconvert_modelpath_lineEdit_placeholder"), convertmodeldir);
    get_convertmodel_name(); //自动构建输出文件名
}

//用户点击执行转换响应
void Expend::on_modelconvert_exec_pushButton_clicked()
{
    if (ui->modelconvert_exec_pushButton->text() == jtr("shut down"))
    {
        convert_command_process->kill();
        ui->modelconvert_exec_pushButton->setText(jtr("exec convert"));
        return;
    }

    if (ui->modelconvert_modelpath_lineEdit->text() == "")
    {
        ui->modelconvert_log->appendPlainText(jtr("Model path must be specified!"));
        return;
    }

#ifdef BODY_LINUX_PACK
    QString appDirPath = qgetenv("APPDIR");
    QString localPath = QString(appDirPath + "/usr/scripts/"); // 脚本目录所在位置
#elif BODY_WIN_PACK
    //先把脚本复制出来
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QString localPath = QString(applicationDirPath + "/EVA_TEMP/scripts/"); // 脚本目录所在位置
    copyRecursively("./scripts", localPath);
    convert_command_process->setWorkingDirectory(applicationDirPath + "/EVA_TEMP/scripts"); //设置cmd程序运行的目录
#else
    QString localPath = QString("./scripts/"); // 脚本目录所在位置
#endif

    if (ui->modelconvert_modeltype_comboBox->currentText() == modeltype_map[MODEL_TYPE_LLM])
    {
        QString command;
        QStringList cmdline;

        command = pythonExecutable + " " +
                  localPath + ui->modelconvert_script_comboBox->currentText() + " " +
                  ui->modelconvert_modelpath_lineEdit->text() + " " +
                  "--outtype " + ui->modelconvert_converttype_comboBox->currentText() + " " +
                  "--outfile " + ui->modelconvert_modelpath_lineEdit->text() + "/" + ui->modelconvert_outputname_lineEdit->text();
        cmdline << CMDGUID << command;

        ui->modelconvert_log->appendPlainText(shell + " > " + command + "\n");
        qDebug() << shell << command;
        convert_command_process->start(shell, cmdline);
    }

    connect(convert_command_process, &QProcess::errorOccurred, [](QProcess::ProcessError error) {
        qDebug() << "Process Error: " << error;
    });
}

//命令行程序开始
void Expend::convert_command_onProcessStarted()
{
    //锁定界面
    ui->modelconvert_modeltype_comboBox->setEnabled(0);
    ui->modelconvert_script_comboBox->setEnabled(0);
    ui->modelconvert_modelpath_pushButton->setEnabled(0);
    ui->modelconvert_converttype_comboBox->setEnabled(0);
    ui->modelconvert_outputname_lineEdit->setEnabled(0);
    ui->modelconvert_exec_pushButton->setText(jtr("shut down")); //恢复成执行转换
}
//命令行程序结束
void Expend::convert_command_onProcessFinished()
{
    //解锁界面
    ui->modelconvert_modeltype_comboBox->setEnabled(1);
    ui->modelconvert_script_comboBox->setEnabled(1);
    ui->modelconvert_modelpath_pushButton->setEnabled(1);
    ui->modelconvert_converttype_comboBox->setEnabled(1);
    ui->modelconvert_outputname_lineEdit->setEnabled(1);
    ui->modelconvert_exec_pushButton->setText(jtr("exec convert")); //恢复成执行转换
}
//获取标准输出
void Expend::readyRead_convert_command_process_StandardOutput()
{

    QByteArray convert_command_output = convert_command_process->readAllStandardOutput(); // 读取子进程的标准输出
#ifdef Q_OS_WIN
    QString output_text = QString::fromLocal8Bit(convert_command_output); // 在 Windows 上，假设标准输出使用本地编码（例如 GBK）
#else
    QString output_text = QString::fromUtf8(convert_command_output); // 在其他平台（如 Linux）上，假设标准输出使用 UTF-8
#endif
    ui->modelconvert_log->appendPlainText(output_text); // 将转换后的文本附加到UI的文本框中
}

//获取错误输出
void Expend::readyRead_convert_command_process_StandardError()
{

    QByteArray convert_command_output = convert_command_process->readAllStandardError(); // 读取子进程的标准错误输出
#ifdef Q_OS_WIN
    QString output_text = QString::fromLocal8Bit(convert_command_output); // 在 Windows 上，假设标准输出使用本地编码（例如 GBK）
#else
    QString output_text = QString::fromUtf8(convert_command_output); // 在其他平台（如 Linux）上，假设标准输出使用 UTF-8
#endif
    ui->modelconvert_log->appendPlainText(output_text); // 将转换后的文本附加到UI的文本框中
}

//用户改变转换类型响应
void Expend::on_modelconvert_converttype_comboBox_currentTextChanged(QString text)
{
    get_convertmodel_name(); //自动构建输出文件名
}

//自动构建输出文件名
void Expend::get_convertmodel_name()
{
    //自动构建输出文件名
    if (ui->modelconvert_modelpath_lineEdit->text() != "")
    {
        QFileInfo fileInfo(convertmodeldir);
        QString fileName = fileInfo.fileName();
        QString outFileName = fileName + "-" + ui->modelconvert_converttype_comboBox->currentText() + ".gguf";

        ui->modelconvert_outputname_lineEdit->setText(outFileName);
    }
}

//逐字节读写文件
bool Expend::copyFile(const QString &src, const QString &dst)
{
    QFile srcFile(src);
    if (!srcFile.open(QIODevice::ReadOnly)) return false;

    QFile dstFile(dst);
    if (!dstFile.open(QIODevice::WriteOnly)) return false;

    while (!srcFile.atEnd())
    {
        QByteArray data = srcFile.read(4096);
        if (data.contains('\x00'))
        {
            qWarning() << "Null byte detected in file:" << src;
            return false;
        }
        if (dstFile.write(data) == -1) return false;
    }

    return true;
}

//复制一个目录里的文件夹所有东西到另一个文件夹
bool Expend::copyRecursively(const QString &srcFilePath, const QString &tgtFilePath)
{
    QFileInfo srcFileInfo(srcFilePath);
    if (srcFileInfo.isDir())
    {
        QDir targetDir(tgtFilePath);
        targetDir.mkpath(tgtFilePath);

        QDir sourceDir(srcFilePath);
        QStringList files = sourceDir.entryList(QDir::Files);
        foreach (QString file, files)
        {
            QString srcName = srcFilePath + QDir::separator() + file;
            QString tgtName = tgtFilePath + QDir::separator() + file;
            copyFile(srcName, tgtName);
        }

        QStringList subdirs = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        foreach (QString subdir, subdirs)
        {
            QString srcName = srcFilePath + QDir::separator() + subdir;
            QString tgtName = tgtFilePath + QDir::separator() + subdir;
            copyRecursively(srcName, tgtName);
        }
    }
    else
    {
        copyFile(srcFilePath, tgtFilePath);
    }
    return true;
}
