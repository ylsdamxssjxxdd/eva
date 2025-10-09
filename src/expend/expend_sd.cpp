#include "expend.h"

#include "ui_expend.h"
#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"

//-------------------------------------------------------------------------
//----------------------------------文生图相关--------------------------------
//-------------------------------------------------------------------------

// 遍历目录
QStringList Expend::listFiles(const QString &path)
{
    QStringList file_paths;
    QDir dir(path);

    // Set the filter to include files and no special files/links
    dir.setFilter(QDir::Files | QDir::NoSymLinks);

    // Get the list of files in the directory
    QFileInfoList fileList = dir.entryInfoList();

    // Iterate through the list and print the absolute file paths
    foreach (const QFileInfo &fileInfo, fileList)
    {
        file_paths << fileInfo.absoluteFilePath();
    }

    return file_paths;
}

//用户点击选择sd模型路径时响应
void Expend::on_sd_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose diffusion model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath == "")
    {
        return;
    }

    QString modelpath = currentpath;
    ui->sd_modelpath_lineEdit->setText(currentpath);

    // 自动寻找其它模型
    if (QFile::exists(modelpath))
    {
        //先清空其它路径
        ui->sd_lorapath_lineEdit->setText("");
        ui->sd_vaepath_lineEdit->setText("");
        ui->sd_clip_l_path_lineEdit->setText("");
        ui->sd_clip_g_path_lineEdit->setText("");
        ui->sd_t5path_lineEdit->setText("");

        // 遍历当前目录
        QFileInfo modelfileInfo(modelpath);
        QString model_directoryPath = modelfileInfo.absolutePath(); // 提取目录路径
        QStringList file_list = listFiles(model_directoryPath);

        for (int i = 0; i < file_list.size(); ++i)
        {
            QString file_path_name = file_list.at(i);
            if (file_path_name.contains("vae"))
            {
                ui->sd_vaepath_lineEdit->setText(file_path_name);
            }
            else if (file_path_name.contains("clip_l"))
            {
                ui->sd_clip_l_path_lineEdit->setText(file_path_name);
            }
            else if (file_path_name.contains("clip_g"))
            {
                ui->sd_clip_g_path_lineEdit->setText(file_path_name);
            }
            else if (file_path_name.contains("t5"))
            {
                ui->sd_t5path_lineEdit->setText(file_path_name);
            }
        }
    }

    // 自动设置参数模板
    if (modelpath.contains("sd1.5-anything-3"))
    {
        ui->params_template_comboBox->setCurrentText("sd1.5-anything-3");
    }
    else if (modelpath.contains("sdxl-animagine-3.1"))
    {
        ui->params_template_comboBox->setCurrentText("sdxl-animagine-3.1");
    }
    else if (modelpath.contains("sd3.5-large"))
    {
        ui->params_template_comboBox->setCurrentText("sd3.5-large");
    }
    else if (modelpath.contains("flux1-dev"))
    {
        ui->params_template_comboBox->setCurrentText("flux1-dev");
    }
}

//用户点击选择vae模型路径时响应
void Expend::on_sd_vaepath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose vae model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_vaepath_lineEdit->setText(currentpath);
    }
}

//用户点击选择clip模型路径时响应
void Expend::on_sd_clip_l_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_l model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_clip_l_path_lineEdit->setText(currentpath);
    }
}

//用户点击选择clip模型路径时响应
void Expend::on_sd_clip_g_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_g model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_clip_g_path_lineEdit->setText(currentpath);
    }
}

//用户点击选择t5模型路径时响应
void Expend::on_sd_t5path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose t5 model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_t5path_lineEdit->setText(currentpath);
    }
}

//用户点击选择lora模型路径时响应
void Expend::on_sd_lorapath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose lora model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_lorapath_lineEdit->setText(currentpath);
    }
}

//用户点击开始绘制时响应
void Expend::on_sd_draw_pushButton_clicked()
{
    //处理stop的情况
    if (ui->sd_img2img_pushButton->text() == "stop" || ui->sd_draw_pushButton->text() == "stop")
    {
        ui->sd_log->appendPlainText("stop");
        sd_process->kill(); //强制结束sd
        ui->sd_draw_pushButton->setText(jtr("text to image"));
        ui->sd_img2img_pushButton->setText(jtr("image to image"));
        img2img = false;
        return;
    }

    ui->sd_img2img_pushButton->setText("stop");
    ui->sd_draw_pushButton->setText("stop");

    if (is_handle_sd && ui->sd_prompt_textEdit->toPlainText() == "")
    {
        ui->sd_log->appendPlainText(jtr("Please enter prompt words to tell the model what you want the image to look like"));
        return;
    }
    else if (is_handle_sd && ui->sd_modelpath_lineEdit->text() == "")
    {
        ui->sd_log->appendPlainText(jtr("Please specify the SD model path first"));
        return;
    }
    else if (!is_handle_sd)
    {
        emit expend2ui_state(QString("expend:sd") + SFX_NAME + " " + jtr("drawing"), USUAL_SIGNAL);
    }

    QTime currentTime = QTime::currentTime();               // 获取当前时间
    QString timeString = currentTime.toString("-hh-mm-ss"); // 格式化时间为时-分-秒
    sd_outputpath = applicationDirPath + "/EVA_TEMP/sd_output" + timeString + ".png";

    //结束sd
    sd_process->kill();

    const QString program = DeviceManager::programPath(QStringLiteral("sd")); // 设置要运行的exe文件的路径
    if (program.isEmpty() || !QFileInfo::exists(program)) { ui->sd_log->appendPlainText("[error] sd backend not found under current device folder"); return; }
    // 如果你的程序需要命令行参数,你可以将它们放在一个QStringList中
    QStringList arguments;

    if (img2img)
    {
        arguments << "-M"
                  << "img_gen";                               //运行模式 图生图
        arguments << "-i" << ensureToolFriendlyFilePath(ui->sd_img2img_lineEdit->text()); // 传入图像路径（处理中文路径）
        img2img = false;
    }
    else
    {
        arguments << "-M"
                  << "img_gen"; //运行模式 文生图
    }

    //模型路径 sd系列模型用-m flux模型用--diffusion-model
    if (ui->sd_modelpath_lineEdit->text().contains("flux"))
    {
        arguments << "--diffusion-model" << ensureToolFriendlyFilePath(ui->sd_modelpath_lineEdit->text());
    }
    else
    {
        arguments << "-m" << ensureToolFriendlyFilePath(ui->sd_modelpath_lineEdit->text());
    }

    if (QFile::exists(ui->sd_vaepath_lineEdit->text()))
    {
        arguments << "--vae" << ensureToolFriendlyFilePath(ui->sd_vaepath_lineEdit->text());
    } // vae路径
    if (QFile::exists(ui->sd_clip_l_path_lineEdit->text()))
    {
        arguments << "--clip_l" << ensureToolFriendlyFilePath(ui->sd_clip_l_path_lineEdit->text());
    } // clip_l路径
    if (QFile::exists(ui->sd_clip_g_path_lineEdit->text()))
    {
        arguments << "--clip_g" << ensureToolFriendlyFilePath(ui->sd_clip_g_path_lineEdit->text());
    } // clip_g路径
    if (QFile::exists(ui->sd_t5path_lineEdit->text()))
    {
        arguments << "--t5xxl" << ensureToolFriendlyFilePath(ui->sd_t5path_lineEdit->text());
    }                                         // vae路径
    QString lora_prompt = "<lora:{model}:1>"; // 应用lora的提示，将会添加到提示词的最后
    if (QFile::exists(ui->sd_lorapath_lineEdit->text()))
    {
        QFileInfo lorafileInfo(ui->sd_lorapath_lineEdit->text());
        QString lora_directoryPath = lorafileInfo.absolutePath(); // 提取lora目录路径
        QString lora_name = lorafileInfo.fileName().replace(".safetensors", "");
        if (lora_directoryPath != "")
        {
            arguments << "--lora-model-dir" << toToolFriendlyPath(lora_directoryPath);
            lora_prompt.replace("{model}", lora_name);
        }
    }

    arguments << "-W" << QString::number(ui->sd_imagewidth->value());        //图像宽
    arguments << "-H" << QString::number(ui->sd_imageheight->value());       //图像长
    arguments << "--sampling-method" << ui->sd_sampletype->currentText();    //采样方法
    arguments << "--clip-skip" << QString::number(ui->sd_clipskip->value()); //跳层
    arguments << "--cfg-scale" << QString::number(ui->sd_cfgscale->value()); //相关系数
    arguments << "--steps" << QString::number(ui->sd_samplesteps->value());  //采样步数
    arguments << "-s" << QString::number(ui->sd_seed->value());              //随机种子
    arguments << "-b" << QString::number(ui->sd_batch_count->value());       //出图张数
    arguments << "-n" << ui->sd_negative_lineEdit->text();                   //反向提示词

    //提示词
    if (arguments.contains("--lora-model-dir"))
    {
        // 应用lora的情况
        arguments << "-p" << ui->sd_modify_lineEdit->text() + ", " + ui->sd_prompt_textEdit->toPlainText() + lora_prompt;
    }
    else
    {
        arguments << "-p" << ui->sd_modify_lineEdit->text() + ", " + ui->sd_prompt_textEdit->toPlainText();
    }

    arguments << "-t" << QString::number(std::thread::hardware_concurrency() * 0.5); //线程数
    arguments << "-o" << toToolFriendlyPath(sd_outputpath);                           // 输出路径（确保无中文）
    arguments << "--strength" << DEFAULT_SD_NOISE;                                   //噪声系数
    arguments << "-v";                                                               // 打印细节

    //连接信号和槽,获取程序的输出
    connect(sd_process, &QProcess::readyReadStandardOutput, [=]() {
        QByteArray sd_process_output_byte = sd_process->readAllStandardOutput(); // 读取子进程的标准错误输出
#ifdef Q_OS_WIN
        QString sd_process_output = QString::fromLocal8Bit(sd_process_output_byte); // 在 Windows 上，假设标准输出使用本地编码（例如 GBK）
#else
        QString sd_process_output = QString::fromUtf8(sd_process_output_byte);// 在其他平台（如 Linux）上，假设标准输出使用 UTF-8
#endif

        QTextCursor cursor(ui->sd_log->textCursor());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(sd_process_output);
        ui->sd_log->verticalScrollBar()->setValue(ui->sd_log->verticalScrollBar()->maximum()); //滚动条滚动到最下面
        if (sd_process_output.contains("CUDA error"))
        {
            sd_process->kill();
        }
    });
    connect(sd_process, &QProcess::readyReadStandardError, [=]() {
        QByteArray sd_process_output_byte = sd_process->readAllStandardError(); // 读取子进程的标准错误输出
#ifdef Q_OS_WIN
        QString sd_process_output = QString::fromLocal8Bit(sd_process_output_byte); // 在 Windows 上，假设标准输出使用本地编码（例如 GBK）
#else
        QString sd_process_output = QString::fromUtf8(sd_process_output_byte);// 在其他平台（如 Linux）上，假设标准输出使用 UTF-8
#endif
        QTextCursor cursor(ui->sd_log->textCursor());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(sd_process_output);
        ui->sd_log->verticalScrollBar()->setValue(ui->sd_log->verticalScrollBar()->maximum()); //滚动条滚动到最下面
        if (sd_process_output.contains("CUDA error"))
        {
            sd_process->kill();
        }
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
    sd_process->setProcessEnvironment(env);
    sd_process->setWorkingDirectory(toolDir);
    sd_process->start(program, arguments);
}
//进程开始响应
void Expend::sd_onProcessStarted() {}
//进程结束响应
void Expend::sd_onProcessFinished()
{
    ui->sd_draw_pushButton->setText(jtr("text to image"));
    ui->sd_img2img_pushButton->setText(jtr("image to image"));

    //绘制结果
    QImage image(sd_outputpath);
    int originalWidth = image.width() / devicePixelRatioF();
    int originalHeight = image.height() / devicePixelRatioF();
    QTextCursor cursor(ui->sd_result->textCursor());
    cursor.movePosition(QTextCursor::End);

    QTextImageFormat imageFormat;
    imageFormat.setWidth(originalWidth);   // 设置图片的宽度
    imageFormat.setHeight(originalHeight); // 设置图片的高度
    imageFormat.setName(sd_outputpath);    // 图片资源路径
    cursor.insertImage(imageFormat);
    ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum()); //滚动条滚动到最下面
    //如果是多幅
    if (ui->sd_batch_count->value() > 1)
    {
        for (int i = 1; i < ui->sd_batch_count->value(); ++i)
        {
            QTextImageFormat imageFormats;
            imageFormats.setWidth(originalWidth);                                                         // 设置图片的宽度
            imageFormats.setHeight(originalHeight);                                                       // 设置图片的高度
            imageFormats.setName(sd_outputpath.split(".png")[0] + "_" + QString::number(i + 1) + ".png"); // 图片资源路径
            cursor.insertImage(imageFormats);
            ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum()); //滚动条滚动到最下面
        }
    }

    //处理工具调用情况
    if (!is_handle_sd && originalWidth > 0)
    {
        is_handle_sd = true;
        emit expend2ui_state("expend:" + jtr("draw over"), USUAL_SIGNAL);
        emit expend2tool_drawover(sd_outputpath, 1); //绘制完成信号
    }
    else if (!is_handle_sd)
    {
        is_handle_sd = true;
        if (sd_process_output.contains("CUDA error"))
        {
            emit expend2ui_state("expend:" + jtr("draw fail cuda"), WRONG_SIGNAL);
            emit expend2tool_drawover(jtr("draw fail cuda"), 0); //绘制完成信号
        }
        else
        {
            emit expend2ui_state("expend:" + jtr("draw fail prompt"), WRONG_SIGNAL);
            emit expend2tool_drawover(jtr("draw fail prompt"), 0); //绘制完成信号
        }
    }
}

//参数模板改变响应
void Expend::on_params_template_comboBox_currentIndexChanged(int index)
{
    // 以前是自定义模板，触发这个函数说明现在换了，保存以前的这个模板
    if (is_readconfig)
    {
        if (is_sd_custom1)
        {
            sd_save_template("custom1");
        }
        else if (is_sd_custom2)
        {
            sd_save_template("custom2");
        }

        if (ui->params_template_comboBox->currentText().contains("custom1"))
        {
            is_sd_custom1 = true;
            is_sd_custom2 = false;
        }
        else if (ui->params_template_comboBox->currentText().contains("custom2"))
        {
            is_sd_custom2 = true;
            is_sd_custom1 = false;
        }
        else
        {
            is_sd_custom1 = false;
            is_sd_custom2 = false;
        }
    }
    sd_apply_template(sd_params_templates[ui->params_template_comboBox->currentText()]);
}

// 保存参数到自定义模板
void Expend::sd_save_template(QString template_name)
{
    sd_params_templates[template_name].batch_count = ui->sd_batch_count->value();
    sd_params_templates[template_name].cfg_scale = ui->sd_cfgscale->value();
    sd_params_templates[template_name].clip_skip = ui->sd_clipskip->value();
    sd_params_templates[template_name].height = ui->sd_imageheight->value();
    sd_params_templates[template_name].width = ui->sd_imagewidth->value();
    sd_params_templates[template_name].seed = ui->sd_seed->value();
    sd_params_templates[template_name].steps = ui->sd_samplesteps->value();
    sd_params_templates[template_name].sample_type = ui->sd_sampletype->currentText();
    sd_params_templates[template_name].negative_prompt = ui->sd_negative_lineEdit->text();
    sd_params_templates[template_name].modify_prompt = ui->sd_modify_lineEdit->text();
}

// 应用sd参数模板
void Expend::sd_apply_template(SD_PARAMS sd_params)
{
    ui->sd_imagewidth->setValue(sd_params.width);
    ui->sd_imageheight->setValue(sd_params.height);
    ui->sd_sampletype->setCurrentText(sd_params.sample_type);
    ui->sd_samplesteps->setValue(sd_params.steps);
    ui->sd_cfgscale->setValue(sd_params.cfg_scale);
    ui->sd_batch_count->setValue(sd_params.batch_count);
    ui->sd_imagewidth->setValue(sd_params.width);
    ui->sd_seed->setValue(sd_params.seed);
    ui->sd_clipskip->setValue(sd_params.clip_skip);
    ui->sd_negative_lineEdit->setText(sd_params.negative_prompt);
    ui->sd_modify_lineEdit->setText(sd_params.modify_prompt);
}

//用户点击图生图时响应
void Expend::on_sd_img2img_pushButton_clicked()
{
    img2img = true;
    ui->sd_draw_pushButton->click();
}

//接收到tool的开始绘制图像信号
void Expend::recv_draw(QString prompt_)
{
    //判断是否空闲
    if (!ui->sd_draw_pushButton->isEnabled())
    {
        emit expend2tool_drawover("stablediffusion" + jtr("Running, please try again later"), 0); //绘制完成信号
        return;
    }
    else if (ui->sd_modelpath_lineEdit->text() == "")
    {
        emit expend2tool_drawover(jtr("The command is invalid. Please ask the user to specify the SD model path in the breeding window first"), 0); //绘制完成信号
        return;
    }
    //先把提示词写进输入框
    ui->sd_prompt_textEdit->setText(prompt_);
    //不是手动
    is_handle_sd = false;
    //触发绘制
    ui->sd_draw_pushButton->click();
}



