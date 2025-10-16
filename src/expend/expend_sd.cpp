#include "expend.h"

#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"
#include "ui_expend.h"
#include <QDir>

//-------------------------------------------------------------------------
//----------------------------------文生图相关--------------------------------
//-------------------------------------------------------------------------

// Compute per-preset config keys and read the stored config
SDRunConfig Expend::loadPresetConfig(const QString &preset) const
{
    const QString key = sanitizePresetKey(preset);
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    const auto get = [&settings](const QString &k, const QVariant &def){ return settings.value(k, def); };
    SDRunConfig c;
    // Recommended family defaults
    int defW = 512, defH = 512, defSteps = 20, defClipSkip = -1, defBatch = 1, defSeed = -1; double defCfg = 7.5; QString defSampler = "euler"; bool defFlowEn=false; double defFlow=0.0; bool defOffload=false, defFA=false; QString defRng="cuda";
    if (preset == "flux1-dev") { defW=768; defH=768; defSteps=30; defCfg=1.0; defSampler="euler"; defClipSkip=-1; defOffload=false; defFA=false; defFlowEn=false; }
    else if (preset == "qwen-image") { defW=1024; defH=1024; defSteps=30; defCfg=2.5; defSampler="euler"; defClipSkip=-1; defOffload=true; defFA=true; defFlowEn=true; defFlow=3.0; }
    else if (preset == "sd1.5-anything-3") { defW=512; defH=512; defSteps=20; defCfg=7.5; defSampler="euler_a"; defClipSkip=1; }

    c.modelArg = static_cast<SDModelArgKind>(get("sd_preset_"+key+"_model_arg", static_cast<int>(SDModelArgKind::Auto)).toInt());
    c.modelPath = get("sd_preset_"+key+"_model_path", "").toString();
    c.vaePath = get("sd_preset_"+key+"_vae_path", "").toString();
    c.clipLPath = get("sd_preset_"+key+"_clip_l_path", "").toString();
    c.clipGPath = get("sd_preset_"+key+"_clip_g_path", "").toString();
    c.clipVisionPath = get("sd_preset_"+key+"_clip_vision_path", "").toString();
    c.t5xxlPath = get("sd_preset_"+key+"_t5xxl_path", "").toString();
    c.qwen2vlPath = get("sd_preset_"+key+"_qwen2vl_path", "").toString();
    c.loraDirPath = get("sd_preset_"+key+"_lora_dir", "").toString();
    c.taesdPath = get("sd_preset_"+key+"_taesd_path", "").toString();
    c.upscaleModelPath = get("sd_preset_"+key+"_upscale_model", "").toString();
    c.controlNetPath = get("sd_preset_"+key+"_control_net", "").toString();
    c.controlImagePath = get("sd_preset_"+key+"_control_img", "").toString();

    c.width = get("sd_preset_"+key+"_width", defW).toInt();
    c.height = get("sd_preset_"+key+"_height", defH).toInt();
    c.sampler = get("sd_preset_"+key+"_sampler", defSampler).toString();
    c.scheduler = get("sd_preset_"+key+"_scheduler", "discrete").toString();
    c.steps = get("sd_preset_"+key+"_steps", defSteps).toInt();
    c.cfgScale = get("sd_preset_"+key+"_cfg", defCfg).toDouble();
    c.clipSkip = get("sd_preset_"+key+"_clip_skip", defClipSkip).toInt();
    c.batchCount = get("sd_preset_"+key+"_batch", defBatch).toInt();
    c.seed = get("sd_preset_"+key+"_seed", defSeed).toInt();
    c.strength = get("sd_preset_"+key+"_strength", 0.75).toDouble();
    c.guidance = get("sd_preset_"+key+"_guidance", 3.5).toDouble();
    c.rng = get("sd_preset_"+key+"_rng", defRng).toString();

    c.flowShiftEnabled = get("sd_preset_"+key+"_flow_shift_en", defFlowEn).toBool();
    c.flowShift = get("sd_preset_"+key+"_flow_shift", defFlow).toDouble();

    c.offloadToCpu = get("sd_preset_"+key+"_offload_cpu", defOffload).toBool();
    c.clipOnCpu = get("sd_preset_"+key+"_clip_cpu", false).toBool();
    c.vaeOnCpu = get("sd_preset_"+key+"_vae_cpu", false).toBool();
    c.controlNetOnCpu = get("sd_preset_"+key+"_control_cpu", false).toBool();
    c.diffusionFA = get("sd_preset_"+key+"_diff_fa", defFA).toBool();

    c.vaeTiling = get("sd_preset_"+key+"_vae_tiling", false).toBool();
    c.vaeTileX = get("sd_preset_"+key+"_vae_tile_x", 32).toInt();
    c.vaeTileY = get("sd_preset_"+key+"_vae_tile_y", 32).toInt();
    c.vaeTileOverlap = get("sd_preset_"+key+"_vae_tile_overlap", 0.5).toDouble();

    c.modifyPrompt = get("sd_preset_"+key+"_modify", "").toString();
    c.negativePrompt = get("sd_preset_"+key+"_negative", "").toString();
    return c;
}

void Expend::savePresetConfig(const QString &preset, const SDRunConfig &cfg) const
{
    const QString key = sanitizePresetKey(preset);
    // Ensure directory exists and persist both per-preset and last-used (sd_adv_*) for compatibility
    QDir().mkpath(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    settings.setValue("sd_preset_"+key+"_model_arg", static_cast<int>(cfg.modelArg));
    settings.setValue("sd_preset_"+key+"_model_path", cfg.modelPath);
    settings.setValue("sd_preset_"+key+"_vae_path", cfg.vaePath);
    settings.setValue("sd_preset_"+key+"_clip_l_path", cfg.clipLPath);
    settings.setValue("sd_preset_"+key+"_clip_g_path", cfg.clipGPath);
    settings.setValue("sd_preset_"+key+"_clip_vision_path", cfg.clipVisionPath);
    settings.setValue("sd_preset_"+key+"_t5xxl_path", cfg.t5xxlPath);
    settings.setValue("sd_preset_"+key+"_qwen2vl_path", cfg.qwen2vlPath);
    settings.setValue("sd_preset_"+key+"_lora_dir", cfg.loraDirPath);
    settings.setValue("sd_preset_"+key+"_taesd_path", cfg.taesdPath);
    settings.setValue("sd_preset_"+key+"_upscale_model", cfg.upscaleModelPath);
    settings.setValue("sd_preset_"+key+"_control_net", cfg.controlNetPath);
    settings.setValue("sd_preset_"+key+"_control_img", cfg.controlImagePath);

    settings.setValue("sd_preset_"+key+"_width", cfg.width);
    settings.setValue("sd_preset_"+key+"_height", cfg.height);
    settings.setValue("sd_preset_"+key+"_sampler", cfg.sampler);
    settings.setValue("sd_preset_"+key+"_scheduler", cfg.scheduler);
    settings.setValue("sd_preset_"+key+"_steps", cfg.steps);
    settings.setValue("sd_preset_"+key+"_cfg", cfg.cfgScale);
    settings.setValue("sd_preset_"+key+"_clip_skip", cfg.clipSkip);
    settings.setValue("sd_preset_"+key+"_batch", cfg.batchCount);
    settings.setValue("sd_preset_"+key+"_seed", cfg.seed);
    settings.setValue("sd_preset_"+key+"_strength", cfg.strength);
    settings.setValue("sd_preset_"+key+"_guidance", cfg.guidance);
    settings.setValue("sd_preset_"+key+"_rng", cfg.rng);

    settings.setValue("sd_preset_"+key+"_flow_shift_en", cfg.flowShiftEnabled);
    settings.setValue("sd_preset_"+key+"_flow_shift", cfg.flowShift);
    settings.setValue("sd_preset_"+key+"_offload_cpu", cfg.offloadToCpu);
    settings.setValue("sd_preset_"+key+"_clip_cpu", cfg.clipOnCpu);
    settings.setValue("sd_preset_"+key+"_vae_cpu", cfg.vaeOnCpu);
    settings.setValue("sd_preset_"+key+"_control_cpu", cfg.controlNetOnCpu);
    settings.setValue("sd_preset_"+key+"_diff_fa", cfg.diffusionFA);

    settings.setValue("sd_preset_"+key+"_vae_tiling", cfg.vaeTiling);
    settings.setValue("sd_preset_"+key+"_vae_tile_x", cfg.vaeTileX);
    settings.setValue("sd_preset_"+key+"_vae_tile_y", cfg.vaeTileY);
    settings.setValue("sd_preset_"+key+"_vae_tile_overlap", cfg.vaeTileOverlap);

    settings.setValue("sd_preset_"+key+"_modify", cfg.modifyPrompt);
    settings.setValue("sd_preset_"+key+"_negative", cfg.negativePrompt);
    // Also mirror modify/negative to legacy last-used global keys for older flows
    settings.setValue("sd_adv_modify", cfg.modifyPrompt);
    settings.setValue("sd_adv_negative", cfg.negativePrompt);
    settings.sync();
}

void Expend::applyPresetToInlineUi(const QString &preset)
{
    // Mirror essential fields to visible inline widgets for user awareness.
    const SDRunConfig &c = sd_preset_configs_.value(preset, sd_run_config_);
    if (ui->sd_modelpath_lineEdit) ui->sd_modelpath_lineEdit->setText(c.modelPath);
    if (ui->sd_vaepath_lineEdit) ui->sd_vaepath_lineEdit->setText(c.vaePath);
    if (ui->sd_clip_l_path_lineEdit) ui->sd_clip_l_path_lineEdit->setText(c.clipLPath);
    if (ui->sd_clip_g_path_lineEdit) ui->sd_clip_g_path_lineEdit->setText(c.clipGPath);
    if (ui->sd_t5path_lineEdit) ui->sd_t5path_lineEdit->setText(c.t5xxlPath);
    if (ui->sd_lorapath_lineEdit) ui->sd_lorapath_lineEdit->setText(c.loraDirPath);
    if (ui->sd_modify_lineEdit) ui->sd_modify_lineEdit->setText(c.modifyPrompt);
    if (ui->sd_negative_lineEdit) ui->sd_negative_lineEdit->setText(c.negativePrompt);
}

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
    // Persist per-preset config immediately (isolation)
    const QString preset = ui->params_template_comboBox->currentText();
    sd_preset_configs_[preset] = sd_run_config_;
    savePresetConfig(preset, sd_run_config_);
}

// 用户点击选择vae模型路径时响应
void Expend::on_sd_vaepath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose vae model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_vaepath_lineEdit->setText(currentpath);
        sd_run_config_.vaePath = currentpath;
        const QString preset = ui->params_template_comboBox->currentText();
        sd_preset_configs_[preset] = sd_run_config_;
        savePresetConfig(preset, sd_run_config_);
    }
}

// 用户点击选择clip模型路径时响应
void Expend::on_sd_clip_l_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_l model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_clip_l_path_lineEdit->setText(currentpath);
        sd_run_config_.clipLPath = currentpath;
        const QString preset = ui->params_template_comboBox->currentText();
        sd_preset_configs_[preset] = sd_run_config_;
        savePresetConfig(preset, sd_run_config_);
    }
}

// 用户点击选择clip模型路径时响应
void Expend::on_sd_clip_g_path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose clip_g model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_clip_g_path_lineEdit->setText(currentpath);
        sd_run_config_.clipGPath = currentpath;
        const QString preset = ui->params_template_comboBox->currentText();
        sd_preset_configs_[preset] = sd_run_config_;
        savePresetConfig(preset, sd_run_config_);
    }
}

// 用户点击选择t5模型路径时响应
void Expend::on_sd_t5path_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose t5 model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_t5path_lineEdit->setText(currentpath);
        sd_run_config_.t5xxlPath = currentpath;
        const QString preset = ui->params_template_comboBox->currentText();
        sd_preset_configs_[preset] = sd_run_config_;
        savePresetConfig(preset, sd_run_config_);
    }
}

// 用户点击选择lora模型路径时响应
void Expend::on_sd_lorapath_pushButton_clicked()
{
    currentpath = customOpenfile(currentpath, "choose lora model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    if (currentpath != "")
    {
        ui->sd_lorapath_lineEdit->setText(currentpath);
        QFileInfo fi(currentpath);
        sd_run_config_.loraDirPath = fi.isDir()? currentpath : fi.absolutePath();
        const QString preset = ui->params_template_comboBox->currentText();
        sd_preset_configs_[preset] = sd_run_config_;
        savePresetConfig(preset, sd_run_config_);
    }
}

// 打开新的文生图高级参数弹窗
void Expend::on_sd_open_params_button_clicked()
{
    if (!sdParamsDialog_)
    {
        sdParamsDialog_ = new SdParamsDialog(this);
        // Initialize preset matching the current combo box and inject its stored config
        sdParamsDialog_->setAutosaveMuted(true);
        const QString presetNow = ui->params_template_comboBox->currentText();
        sdParamsDialog_->applyPreset(presetNow);
        // Load stored config for current preset and reflect into dialog
        if (!sd_preset_configs_.contains(presetNow))
            sd_preset_configs_[presetNow] = loadPresetConfig(presetNow);
        SDRunConfig cfgNow = sd_preset_configs_.value(presetNow, SDRunConfig{});
        // Default sd1.5 prompts if empty
        if (presetNow == "sd1.5-anything-3")
        {
            if (cfgNow.modifyPrompt.isEmpty()) cfgNow.modifyPrompt = sd_params_templates[presetNow].modify_prompt;
            if (cfgNow.negativePrompt.isEmpty()) cfgNow.negativePrompt = sd_params_templates[presetNow].negative_prompt;
        }
        sdParamsDialog_->setConfig(cfgNow);
        // Keep legacy per-preset prompt store in sync for compatibility
        sdParamsDialog_->setPresetPromptStore(sd_preset_modify_, sd_preset_negative_);
        // Autosave handler: persist strictly per preset
        connect(sdParamsDialog_, &SdParamsDialog::accepted, this, [this](const SDRunConfig &cfg, const QString &preset) {
            const QString p = preset.isEmpty()? ui->params_template_comboBox->currentText() : preset;
            sd_preset_configs_[p] = cfg;
            sd_run_config_ = cfg; // make it current
            // Mirror essentials to inline fields
            applyPresetToInlineUi(p);
            // Keep compatibility maps for prompts
            sd_preset_modify_[p] = cfg.modifyPrompt;
            sd_preset_negative_[p] = cfg.negativePrompt;
            // Persist to QSettings by preset
            createTempDirectory(applicationDirPath + "/EVA_TEMP");
            savePresetConfig(p, cfg);
            // Also persist current positive prompt for convenience (global)
            QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
            settings.setIniCodec("utf-8");
            settings.setValue("sd_prompt", ui->sd_prompt_textEdit->toPlainText());
            // Keep last-used global keys aligned
            settings.setValue("sd_adv_modify", cfg.modifyPrompt);
            settings.setValue("sd_adv_negative", cfg.negativePrompt);
            settings.sync();
        });
        // When user switches preset inside dialog, inject stored config so fields do not leak
        connect(sdParamsDialog_, &SdParamsDialog::presetChanged, this, [this](const QString &p){
            if (!sd_preset_configs_.contains(p)) sd_preset_configs_[p] = loadPresetConfig(p);
            sdParamsDialog_->setConfig(sd_preset_configs_.value(p));
            // Switch inline preset selector as well
            if (ui && ui->params_template_comboBox)
                ui->params_template_comboBox->setCurrentText(p);
        });
        // Unmute autosave after initial programmatic setup
        sdParamsDialog_->setAutosaveMuted(false);
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

    // negative prompt and prompt assembly (strictly per-preset values; do not fallback to hidden UI)
    const QString neg = sd_run_config_.negativePrompt.trimmed();
    if (!neg.isEmpty())
        arguments << "-n" << neg;
    const QString pos = ui->sd_prompt_textEdit->toPlainText(); // main UI prompt (positive)
    const QString mod = sd_run_config_.modifyPrompt.trimmed();
    const QString promptCore = (mod.isEmpty() ? pos : (mod + ", " + pos));
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
    const QString preset = ui->params_template_comboBox->currentText();
    // Persist the selected preset immediately
    {
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        settings.setValue("sd_params_template", preset);
        // Keep last-used global keys aligned for modify/negative
        settings.setValue("sd_adv_modify", sd_run_config_.modifyPrompt);
        settings.setValue("sd_adv_negative", sd_run_config_.negativePrompt);
        settings.sync();
    }
    // Load per-preset config (isolated)
    if (!sd_preset_configs_.contains(preset))
        sd_preset_configs_[preset] = loadPresetConfig(preset);
    sd_run_config_ = sd_preset_configs_.value(preset);
    // sd1.5: ensure defaults for empty prompts
    if (preset == "sd1.5-anything-3")
    {
        if (sd_run_config_.modifyPrompt.isEmpty()) sd_run_config_.modifyPrompt = sd_params_templates[preset].modify_prompt;
        if (sd_run_config_.negativePrompt.isEmpty()) sd_run_config_.negativePrompt = sd_params_templates[preset].negative_prompt;
    }
    // Mirror essentials to inline widgets
    applyPresetToInlineUi(preset);
    // If advanced dialog is open, refresh it to show this preset values only
    if (sdParamsDialog_) sdParamsDialog_->setConfig(sd_run_config_);
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
