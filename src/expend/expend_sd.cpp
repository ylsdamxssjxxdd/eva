#include "expend.h"

#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"
#include "ui_expend.h"
#include <QDir>

//-------------------------------------------------------------------------
//----------------------------------文生图相关--------------------------------
//-------------------------------------------------------------------------

// 用于设置sd模型路径
void Expend::setSdModelpath(QString modelpath)
{
    ui->sd_modelpath_lineEdit->setText(modelpath);
    ui->params_template_comboBox->setCurrentText("sd1.5-anything-3"); // 默认
}

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

// 用户点击选择sd模型路径时响应
void Expend::on_sd_modelpath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose diffusion model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath == "")
    {
        return;
    }

    QString modelpath = currentpath;
    ui->sd_modelpath_lineEdit->setText(currentpath);
    // Mirror to advanced run config (main model path)
    sd_run_config_.modelPath = currentpath;

    // 自动寻找其它模型
    if (QFile::exists(modelpath))
    {
        // 先清空其它路径
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
    if (modelpath.contains("qwen", Qt::CaseInsensitive))
    {
        ui->params_template_comboBox->setCurrentText("qwen-image");
    }
    else if (modelpath.contains("sd1.5-anything-3"))
    {
        ui->params_template_comboBox->setCurrentText("sd1.5-anything-3");
    }
    else if (modelpath.contains("sdxl-animagine-3.1"))
    {
        // Deprecated preset removed in favor of compact set; fallback to custom1
        ui->params_template_comboBox->setCurrentText("custom1");
    }
    else if (modelpath.contains("sd3.5-large"))
    {
        ui->params_template_comboBox->setCurrentText("custom1");
    }
    else if (modelpath.contains("flux1-dev"))
    {
        ui->params_template_comboBox->setCurrentText("flux1-dev");
    }
}

// 用户点击选择vae模型路径时响应
void Expend::on_sd_vaepath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose vae model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_vaepath_lineEdit->setText(currentpath);
    }
}

// 用户点击选择clip模型路径时响应
void Expend::on_sd_clip_l_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_l model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_clip_l_path_lineEdit->setText(currentpath);
    }
}

// 用户点击选择clip模型路径时响应
void Expend::on_sd_clip_g_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_g model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_clip_g_path_lineEdit->setText(currentpath);
    }
}

// 用户点击选择t5模型路径时响应
void Expend::on_sd_t5path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose t5 model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_t5path_lineEdit->setText(currentpath);
    }
}

// 用户点击选择lora模型路径时响应
void Expend::on_sd_lorapath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose lora model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_lorapath_lineEdit->setText(currentpath);
    }
}

// 打开新的文生图高级参数弹窗
void Expend::on_sd_open_params_button_clicked()
{
    if (!sdParamsDialog_)
    {
        sdParamsDialog_ = new SdParamsDialog(this);
        // Initialize preset matching the current combo box
        sdParamsDialog_->applyPreset(ui->params_template_comboBox->currentText());
        sdParamsDialog_->setConfig(sd_run_config_);
        connect(sdParamsDialog_, &SdParamsDialog::accepted, this, [this](const SDRunConfig &cfg, const QString &preset) {
            // Persist to memory and mirror essential fields to inline UI for visibility
            sd_run_config_ = cfg;
            if (!cfg.modelPath.isEmpty()) ui->sd_modelpath_lineEdit->setText(cfg.modelPath);
            if (!cfg.vaePath.isEmpty()) ui->sd_vaepath_lineEdit->setText(cfg.vaePath);
            if (!cfg.clipLPath.isEmpty()) ui->sd_clip_l_path_lineEdit->setText(cfg.clipLPath);
            if (!cfg.clipGPath.isEmpty()) ui->sd_clip_g_path_lineEdit->setText(cfg.clipGPath);
            if (!cfg.t5xxlPath.isEmpty()) ui->sd_t5path_lineEdit->setText(cfg.t5xxlPath);
            // Sync prompts back to visible prompt area
            if (!cfg.negativePrompt.isEmpty()) ui->sd_negative_lineEdit->setText(cfg.negativePrompt);
            if (!cfg.positivePrompt.isEmpty()) ui->sd_prompt_textEdit->setText(cfg.positivePrompt);
            if (!preset.isEmpty()) ui->params_template_comboBox->setCurrentText(preset);
            // Save advanced config immediately
            createTempDirectory(applicationDirPath + "/EVA_TEMP");
            QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
            settings.setIniCodec("utf-8");
            settings.setValue("sd_adv_model_path", sd_run_config_.modelPath);
            settings.setValue("sd_adv_vae_path", sd_run_config_.vaePath);
            settings.setValue("sd_adv_clip_l_path", sd_run_config_.clipLPath);
            settings.setValue("sd_adv_clip_g_path", sd_run_config_.clipGPath);
            settings.setValue("sd_adv_clip_vision_path", sd_run_config_.clipVisionPath);
            settings.setValue("sd_adv_t5xxl_path", sd_run_config_.t5xxlPath);
            settings.setValue("sd_adv_qwen2vl_path", sd_run_config_.qwen2vlPath);
            settings.setValue("sd_adv_lora_dir", sd_run_config_.loraDirPath);
            settings.setValue("sd_adv_taesd_path", sd_run_config_.taesdPath);
            settings.setValue("sd_adv_upscale_model", sd_run_config_.upscaleModelPath);
            settings.setValue("sd_adv_control_net", sd_run_config_.controlNetPath);
            settings.setValue("sd_adv_control_img", sd_run_config_.controlImagePath);
            settings.setValue("sd_adv_width", sd_run_config_.width);
            settings.setValue("sd_adv_height", sd_run_config_.height);
            settings.setValue("sd_adv_sampler", sd_run_config_.sampler);
            settings.setValue("sd_adv_scheduler", sd_run_config_.scheduler);
            settings.setValue("sd_adv_steps", sd_run_config_.steps);
            settings.setValue("sd_adv_cfg", sd_run_config_.cfgScale);
            settings.setValue("sd_adv_clip_skip", sd_run_config_.clipSkip);
            settings.setValue("sd_adv_batch", sd_run_config_.batchCount);
            settings.setValue("sd_adv_seed", sd_run_config_.seed);
            settings.setValue("sd_adv_strength", sd_run_config_.strength);
            settings.setValue("sd_adv_guidance", sd_run_config_.guidance);
            settings.setValue("sd_adv_rng", sd_run_config_.rng);
            settings.setValue("sd_adv_flow_shift_en", sd_run_config_.flowShiftEnabled);
            settings.setValue("sd_adv_flow_shift", sd_run_config_.flowShift);
            settings.setValue("sd_adv_offload_cpu", sd_run_config_.offloadToCpu);
            settings.setValue("sd_adv_clip_cpu", sd_run_config_.clipOnCpu);
            settings.setValue("sd_adv_vae_cpu", sd_run_config_.vaeOnCpu);
            settings.setValue("sd_adv_control_cpu", sd_run_config_.controlNetOnCpu);
            settings.setValue("sd_adv_diff_fa", sd_run_config_.diffusionFA);
            settings.setValue("sd_adv_vae_tiling", sd_run_config_.vaeTiling);
            settings.setValue("sd_adv_vae_tile_x", sd_run_config_.vaeTileX);
            settings.setValue("sd_adv_vae_tile_y", sd_run_config_.vaeTileY);
            settings.setValue("sd_adv_vae_tile_overlap", sd_run_config_.vaeTileOverlap);
            // Also persist current prompts for convenience
            settings.setValue("sd_prompt", ui->sd_prompt_textEdit->toPlainText());
            settings.sync();
        });
    }
    sdParamsDialog_->show();
    sdParamsDialog_->raise();
    sdParamsDialog_->activateWindow();
}

// 用户点击开始绘制时响应
void Expend::on_sd_draw_pushButton_clicked()
{
    // 处理stop的情况
    if (ui->sd_img2img_pushButton->text() == "stop" || ui->sd_draw_pushButton->text() == "stop")
    {
        ui->sd_log->appendPlainText("stop");
        sd_process->kill(); // 强制结束sd
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

    // 结束sd
    sd_process->kill();

    const QString program = DeviceManager::programPath(QStringLiteral("sd")); // 设置要运行的exe文件的路径
    if (program.isEmpty() || !QFileInfo::exists(program))
    {
        ui->sd_log->appendPlainText("[error] sd backend not found under current device folder");
        return;
    }
    // Build unified argument list using the new advanced run-config
    QStringList arguments;
    // mode
    arguments << "-M" << "img_gen";
    if (img2img)
    {
        arguments << "-i" << ensureToolFriendlyFilePath(ui->sd_img2img_lineEdit->text());
        img2img = false;
        // strength for img2img (from config)
        arguments << "--strength" << QString::number(sd_run_config_.strength);
    }
    // main model arg
    SDModelArgKind argk = sd_run_config_.modelArg;
    if (argk == SDModelArgKind::Auto)
    {
        // Heuristic: prefer --diffusion-model when clip/t5/qwen2vl provided or filename hints
        const bool hasExtra = !sd_run_config_.t5xxlPath.isEmpty() || !sd_run_config_.qwen2vlPath.isEmpty() || !sd_run_config_.clipLPath.isEmpty() || !sd_run_config_.clipGPath.isEmpty();
        if (hasExtra || sd_run_config_.modelPath.contains("flux", Qt::CaseInsensitive) || sd_run_config_.modelPath.contains("qwen", Qt::CaseInsensitive))
            argk = SDModelArgKind::Diffusion;
        else
            argk = SDModelArgKind::LegacyM;
    }
    if (argk == SDModelArgKind::Diffusion)
        arguments << "--diffusion-model" << ensureToolFriendlyFilePath(sd_run_config_.modelPath);
    else
        arguments << "-m" << ensureToolFriendlyFilePath(sd_run_config_.modelPath);

    // optional components
    if (!sd_run_config_.vaePath.isEmpty()) arguments << "--vae" << ensureToolFriendlyFilePath(sd_run_config_.vaePath);
    if (!sd_run_config_.clipLPath.isEmpty()) arguments << "--clip_l" << ensureToolFriendlyFilePath(sd_run_config_.clipLPath);
    if (!sd_run_config_.clipGPath.isEmpty()) arguments << "--clip_g" << ensureToolFriendlyFilePath(sd_run_config_.clipGPath);
    if (!sd_run_config_.clipVisionPath.isEmpty()) arguments << "--clip_vision" << ensureToolFriendlyFilePath(sd_run_config_.clipVisionPath);
    if (!sd_run_config_.t5xxlPath.isEmpty()) arguments << "--t5xxl" << ensureToolFriendlyFilePath(sd_run_config_.t5xxlPath);
    if (!sd_run_config_.qwen2vlPath.isEmpty()) arguments << "--qwen2vl" << ensureToolFriendlyFilePath(sd_run_config_.qwen2vlPath);
    if (!sd_run_config_.taesdPath.isEmpty()) arguments << "--taesd" << ensureToolFriendlyFilePath(sd_run_config_.taesdPath);
    if (!sd_run_config_.upscaleModelPath.isEmpty()) arguments << "--upscale-model" << ensureToolFriendlyFilePath(sd_run_config_.upscaleModelPath);
    if (!sd_run_config_.controlNetPath.isEmpty()) arguments << "--control-net" << ensureToolFriendlyFilePath(sd_run_config_.controlNetPath);
    if (!sd_run_config_.controlImagePath.isEmpty()) arguments << "--control-image" << ensureToolFriendlyFilePath(sd_run_config_.controlImagePath);

    // LoRA directory (and heuristic prompt injection)
    QString lora_prompt;
    if (!sd_run_config_.loraDirPath.isEmpty())
    {
        arguments << "--lora-model-dir" << toToolFriendlyPath(sd_run_config_.loraDirPath);
        // Heuristically pick first .safetensors file name for prompt tag
        QDir ld(sd_run_config_.loraDirPath);
        QStringList loraFiles = ld.entryList(QStringList() << "*.safetensors", QDir::Files);
        if (!loraFiles.isEmpty())
        {
            const QString name = QFileInfo(loraFiles.first()).fileName().replace(".safetensors", "");
            lora_prompt = QString(" <lora:%1:1>").arg(name);
        }
    }
    // Also support legacy single-file line edit if provided (backward compat)
    if (lora_prompt.isEmpty() && QFile::exists(ui->sd_lorapath_lineEdit->text()))
    {
        QFileInfo lorafileInfo(ui->sd_lorapath_lineEdit->text());
        QString lora_directoryPath = lorafileInfo.absolutePath();
        QString lora_name = lorafileInfo.fileName().replace(".safetensors", "");
        if (!lora_directoryPath.isEmpty())
        {
            arguments << "--lora-model-dir" << toToolFriendlyPath(lora_directoryPath);
            lora_prompt = QString(" <lora:%1:1>").arg(lora_name);
        }
    }

    // dims and sampling
    arguments << "-W" << QString::number(sd_run_config_.width);
    arguments << "-H" << QString::number(sd_run_config_.height);
    arguments << "--sampling-method" << sd_run_config_.sampler;
    if (!sd_run_config_.scheduler.isEmpty()) arguments << "--scheduler" << sd_run_config_.scheduler;
    arguments << "--clip-skip" << QString::number(sd_run_config_.clipSkip);
    arguments << "--cfg-scale" << QString::number(sd_run_config_.cfgScale);
    arguments << "--steps" << QString::number(sd_run_config_.steps);
    arguments << "-s" << QString::number(sd_run_config_.seed);
    arguments << "-b" << QString::number(sd_run_config_.batchCount);
    if (sd_run_config_.guidance > 0.0) arguments << "--guidance" << QString::number(sd_run_config_.guidance);
    arguments << "--rng" << sd_run_config_.rng;

    // negative prompt and prompt assembly (no separate Modify prefix)
    const QString neg = !sd_run_config_.negativePrompt.trimmed().isEmpty() ? sd_run_config_.negativePrompt : ui->sd_negative_lineEdit->text();
    arguments << "-n" << neg;
    const QString pos = !sd_run_config_.positivePrompt.trimmed().isEmpty() ? sd_run_config_.positivePrompt : ui->sd_prompt_textEdit->toPlainText();
    const QString modUi = ui->sd_modify_lineEdit->text();
    const QString promptCore = (modUi.isEmpty() ? pos : (modUi + ", " + pos));
    if (!lora_prompt.isEmpty())
        arguments << "-p" << (promptCore + lora_prompt);
    else
        arguments << "-p" << promptCore;

    // Backend toggles
    if (sd_run_config_.offloadToCpu) arguments << "--offload-to-cpu";
    if (sd_run_config_.clipOnCpu) arguments << "--clip-on-cpu";
    if (sd_run_config_.vaeOnCpu) arguments << "--vae-on-cpu";
    if (sd_run_config_.controlNetOnCpu) arguments << "--control-net-cpu";
    if (sd_run_config_.diffusionFA) arguments << "--diffusion-fa";
    if (sd_run_config_.flowShiftEnabled) arguments << "--flow-shift" << QString::number(sd_run_config_.flowShift);
    if (sd_run_config_.vaeTiling)
    {
        arguments << "--vae-tiling";
        arguments << "--vae-tile-size" << QString("%1x%2").arg(sd_run_config_.vaeTileX).arg(sd_run_config_.vaeTileY);
        arguments << "--vae-tile-overlap" << QString::number(sd_run_config_.vaeTileOverlap);
    }

    // threads, output, verbosity
    arguments << "-t" << QString::number(std::thread::hardware_concurrency() * 0.5);
    arguments << "-o" << toToolFriendlyPath(sd_outputpath);
    arguments << "-v";

    // 连接信号和槽,获取程序的输出
    connect(sd_process, &QProcess::readyReadStandardOutput, [=]()
            {
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
        } });
    connect(sd_process, &QProcess::readyReadStandardError, [=]()
            {
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
        } });

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
// 进程开始响应
void Expend::sd_onProcessStarted() {}
// 进程结束响应
void Expend::sd_onProcessFinished()
{
    ui->sd_draw_pushButton->setText(jtr("text to image"));
    ui->sd_img2img_pushButton->setText(jtr("image to image"));

    // 绘制结果
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
    ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum()); // 滚动条滚动到最下面
    // 如果是多幅
    if (ui->sd_batch_count->value() > 1)
    {
        for (int i = 1; i < ui->sd_batch_count->value(); ++i)
        {
            QTextImageFormat imageFormats;
            imageFormats.setWidth(originalWidth);                                                         // 设置图片的宽度
            imageFormats.setHeight(originalHeight);                                                       // 设置图片的高度
            imageFormats.setName(sd_outputpath.split(".png")[0] + "_" + QString::number(i + 1) + ".png"); // 图片资源路径
            cursor.insertImage(imageFormats);
            ui->sd_result->verticalScrollBar()->setValue(ui->sd_result->verticalScrollBar()->maximum()); // 滚动条滚动到最下面
        }
    }

    // 处理工具调用情况
    if (!is_handle_sd && originalWidth > 0)
    {
        is_handle_sd = true;
        emit expend2ui_state("expend:" + jtr("draw over"), USUAL_SIGNAL);
        emit expend2tool_drawover(sd_outputpath, 1); // 绘制完成信号
    }
    else if (!is_handle_sd)
    {
        is_handle_sd = true;
        if (sd_process_output.contains("CUDA error"))
        {
            emit expend2ui_state("expend:" + jtr("draw fail cuda"), WRONG_SIGNAL);
            emit expend2tool_drawover(jtr("draw fail cuda"), 0); // 绘制完成信号
        }
        else
        {
            emit expend2ui_state("expend:" + jtr("draw fail prompt"), WRONG_SIGNAL);
            emit expend2tool_drawover(jtr("draw fail prompt"), 0); // 绘制完成信号
        }
    }
}

// 参数模板改变响应
void Expend::on_params_template_comboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
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
    // Apply prompt/size knobs
    sd_apply_template(sd_params_templates[ui->params_template_comboBox->currentText()]);
    // Also pre-configure advanced run config according to selected preset
    const QString preset = ui->params_template_comboBox->currentText();
    if (preset == "flux1-dev")
    {
        sd_run_config_.modelArg = SDModelArgKind::Diffusion;
        sd_run_config_.width = sd_params_templates[preset].width;
        sd_run_config_.height = sd_params_templates[preset].height;
        sd_run_config_.sampler = "euler";
        sd_run_config_.steps = 30;
        sd_run_config_.cfgScale = 1.0;
        sd_run_config_.clipSkip = -1;
        sd_run_config_.offloadToCpu = false;
        sd_run_config_.diffusionFA = false;
        sd_run_config_.flowShiftEnabled = false;
    }
    else if (preset == "qwen-image")
    {
        sd_run_config_.modelArg = SDModelArgKind::Diffusion;
        sd_run_config_.width = sd_params_templates[preset].width;
        sd_run_config_.height = sd_params_templates[preset].height;
        sd_run_config_.sampler = "euler";
        sd_run_config_.steps = 30;
        sd_run_config_.cfgScale = 2.5;
        sd_run_config_.clipSkip = -1;
        sd_run_config_.offloadToCpu = true;
        sd_run_config_.diffusionFA = true;
        sd_run_config_.flowShiftEnabled = true; sd_run_config_.flowShift = 3.0;
    }
    else if (preset == "sd1.5-anything-3")
    {
        sd_run_config_.modelArg = SDModelArgKind::LegacyM;
        sd_run_config_.width = sd_params_templates[preset].width;
        sd_run_config_.height = sd_params_templates[preset].height;
        sd_run_config_.sampler = "euler_a";
        sd_run_config_.steps = 20;
        sd_run_config_.cfgScale = 7.5;
        sd_run_config_.clipSkip = 1;
        sd_run_config_.offloadToCpu = false;
        sd_run_config_.diffusionFA = false;
        sd_run_config_.flowShiftEnabled = false;
    }
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

// 用户点击图生图时响应
void Expend::on_sd_img2img_pushButton_clicked()
{
    img2img = true;
    ui->sd_draw_pushButton->click();
}

// 接收到tool的开始绘制图像信号
void Expend::recv_draw(QString prompt_)
{
    // 判断是否空闲
    if (!ui->sd_draw_pushButton->isEnabled())
    {
        emit expend2tool_drawover("stablediffusion" + jtr("Running, please try again later"), 0); // 绘制完成信号
        return;
    }
    else if (ui->sd_modelpath_lineEdit->text() == "")
    {
        emit expend2tool_drawover(jtr("The command is invalid. Please ask the user to specify the SD model path in the breeding window first"), 0); // 绘制完成信号
        return;
    }
    // 先把提示词写进输入框
    ui->sd_prompt_textEdit->setText(prompt_);
    // 不是手动
    is_handle_sd = false;
    // 触发绘制
    ui->sd_draw_pushButton->click();
}
