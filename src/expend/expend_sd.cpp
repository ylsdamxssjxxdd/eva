#include "expend.h"

#include "../utils/devicemanager.h"
#include "../utils/pathutil.h"
#include "ui_expend.h"
#include <QDir>
#include <QFileInfo>
#include <src/utils/imagedropwidget.h>

//-------------------------------------------------------------------------
//----------------------------------文生图相关--------------------------------
//-------------------------------------------------------------------------

// Compute per-preset config keys and read the stored config
SDRunConfig Expend::loadPresetConfig(const QString &preset) const
{
    const QString key = sanitizePresetKey(preset);
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    const auto get = [&settings](const QString &k, const QVariant &def)
    { return settings.value(k, def); };
    SDRunConfig c;
    // Recommended family defaults
    SDModelArgKind defModelArg = SDModelArgKind::Auto;
    int defW = 512, defH = 512, defSteps = 20, defClipSkip = -1, defBatch = 1, defSeed = -1;
    double defCfg = 7.5;
    QString defSampler = "euler";
    bool defFlowEn = false;
    double defFlow = 0.0;
    bool defOffload = false, defFA = false;
    QString defRng = "cuda";
    QString defModelPath;
    QString defVaePath;
    QString defLlmPath;
    if (preset == "flux1-dev")
    {
        defW = 768;
        defH = 768;
        defSteps = 30;
        defCfg = 1.0;
        defSampler = "euler";
        defClipSkip = -1;
        defOffload = false;
        defFA = false;
        defFlowEn = false;
    }
    else if (preset == "qwen-image")
    {
        defW = 1024;
        defH = 1024;
        defSteps = 30;
        defCfg = 2.5;
        defSampler = "euler";
        defClipSkip = -1;
        defOffload = true;
        defFA = true;
        defFlowEn = true;
        defFlow = 3.0;
    }
    else if (preset == "z-image")
    {
        // æ ¹æ®ææŒ¥å‘˜æ��ä¾›çš„è®¾ç½®ï¼Œé»˜è®¤ä»¥ --diffusion-model æ–¹å¼è·¯å¾„å¼•å…¥ z-image è¶…å¿«æ¨¡åž‹
        defModelArg = SDModelArgKind::Diffusion;
        defW = 512;
        defH = 1024;
        defSteps = 20;
        defCfg = 1.0;
        defSampler = "euler";
        defOffload = true;
        defFA = true;
        defModelPath = applicationDirPath + "/z_image_turbo-Q4_0.gguf";
        defVaePath = applicationDirPath + "/ae.safetensors";
        defLlmPath = applicationDirPath + "/Qwen3-4B-Instruct-2507-Q4_K_M.gguf";
    }
    else if (preset == "sd1.5-anything-3")
    {
        defW = 512;
        defH = 512;
        defSteps = 20;
        defCfg = 7.5;
        defSampler = "euler_a";
        defClipSkip = 1;
    }
    else if (preset == "wan2.2")
    {
        defW = 480;
        defH = 832;
        defSteps = 30;
        defCfg = 6.0;
        defSampler = "euler";
        defClipSkip = -1;
        defOffload = true;
        defFA = true;
        defFlowEn = true;
        defFlow = 3.0;
    }

    c.modelArg = static_cast<SDModelArgKind>(get("sd_preset_" + key + "_model_arg", static_cast<int>(defModelArg)).toInt());
    c.modelPath = get("sd_preset_" + key + "_model_path", defModelPath).toString();
    c.vaePath = get("sd_preset_" + key + "_vae_path", defVaePath).toString();
    c.clipLPath = get("sd_preset_" + key + "_clip_l_path", "").toString();
    c.clipGPath = get("sd_preset_" + key + "_clip_g_path", "").toString();
    c.clipVisionPath = get("sd_preset_" + key + "_clip_vision_path", "").toString();
    c.t5xxlPath = get("sd_preset_" + key + "_t5xxl_path", "").toString();
    c.llmPath = get("sd_preset_" + key + "_llm_path", defLlmPath).toString();

    c.llmVisionPath = get("sd_preset_" + key + "_llm_vision_path", "").toString();
    c.loraDirPath = get("sd_preset_" + key + "_lora_dir", "").toString();
    c.taesdPath = get("sd_preset_" + key + "_taesd_path", "").toString();
    c.upscaleModelPath = get("sd_preset_" + key + "_upscale_model", "").toString();
    c.controlNetPath = get("sd_preset_" + key + "_control_net", "").toString();
    c.controlImagePath = get("sd_preset_" + key + "_control_img", "").toString();

    c.width = get("sd_preset_" + key + "_width", defW).toInt();
    c.height = get("sd_preset_" + key + "_height", defH).toInt();
    c.sampler = get("sd_preset_" + key + "_sampler", defSampler).toString();
    c.scheduler = get("sd_preset_" + key + "_scheduler", "discrete").toString();
    c.steps = get("sd_preset_" + key + "_steps", defSteps).toInt();
    c.cfgScale = get("sd_preset_" + key + "_cfg", defCfg).toDouble();
    c.clipSkip = get("sd_preset_" + key + "_clip_skip", defClipSkip).toInt();
    c.batchCount = get("sd_preset_" + key + "_batch", defBatch).toInt();
    c.seed = get("sd_preset_" + key + "_seed", defSeed).toInt();
    c.strength = get("sd_preset_" + key + "_strength", 0.75).toDouble();
    c.guidance = get("sd_preset_" + key + "_guidance", 3.5).toDouble();
    c.rng = get("sd_preset_" + key + "_rng", defRng).toString();
    c.videoFrames = get("sd_preset_" + key + "_video_frames", preset == "wan2.2" ? 33 : 0).toInt();

    c.flowShiftEnabled = get("sd_preset_" + key + "_flow_shift_en", defFlowEn).toBool();
    c.flowShift = get("sd_preset_" + key + "_flow_shift", defFlow).toDouble();

    c.offloadToCpu = get("sd_preset_" + key + "_offload_cpu", defOffload).toBool();
    c.clipOnCpu = get("sd_preset_" + key + "_clip_cpu", false).toBool();
    c.vaeOnCpu = get("sd_preset_" + key + "_vae_cpu", false).toBool();
    c.controlNetOnCpu = get("sd_preset_" + key + "_control_cpu", false).toBool();
    c.diffusionFA = get("sd_preset_" + key + "_diff_fa", defFA).toBool();

    c.vaeTiling = get("sd_preset_" + key + "_vae_tiling", false).toBool();
    c.vaeTileX = get("sd_preset_" + key + "_vae_tile_x", 32).toInt();
    c.vaeTileY = get("sd_preset_" + key + "_vae_tile_y", 32).toInt();
    c.vaeTileOverlap = get("sd_preset_" + key + "_vae_tile_overlap", 0.5).toDouble();

    c.modifyPrompt = get("sd_preset_" + key + "_modify", "").toString();
    c.negativePrompt = get("sd_preset_" + key + "_negative", "").toString();
    return c;
}

void Expend::savePresetConfig(const QString &preset, const SDRunConfig &cfg) const
{
    const QString key = sanitizePresetKey(preset);
    // Ensure directory exists and persist both per-preset and last-used (sd_adv_*) for compatibility
    QDir().mkpath(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");

    settings.setValue("sd_preset_" + key + "_model_arg", static_cast<int>(cfg.modelArg));
    settings.setValue("sd_preset_" + key + "_model_path", cfg.modelPath);
    settings.setValue("sd_preset_" + key + "_vae_path", cfg.vaePath);
    settings.setValue("sd_preset_" + key + "_clip_l_path", cfg.clipLPath);
    settings.setValue("sd_preset_" + key + "_clip_g_path", cfg.clipGPath);
    settings.setValue("sd_preset_" + key + "_clip_vision_path", cfg.clipVisionPath);
    settings.setValue("sd_preset_" + key + "_t5xxl_path", cfg.t5xxlPath);
    settings.setValue("sd_preset_" + key + "_llm_path", cfg.llmPath);

    settings.setValue("sd_preset_" + key + "_llm_vision_path", cfg.llmVisionPath);
    settings.setValue("sd_preset_" + key + "_lora_dir", cfg.loraDirPath);
    settings.setValue("sd_preset_" + key + "_taesd_path", cfg.taesdPath);
    settings.setValue("sd_preset_" + key + "_upscale_model", cfg.upscaleModelPath);
    settings.setValue("sd_preset_" + key + "_control_net", cfg.controlNetPath);
    settings.setValue("sd_preset_" + key + "_control_img", cfg.controlImagePath);

    settings.setValue("sd_preset_" + key + "_width", cfg.width);
    settings.setValue("sd_preset_" + key + "_height", cfg.height);
    settings.setValue("sd_preset_" + key + "_sampler", cfg.sampler);
    settings.setValue("sd_preset_" + key + "_scheduler", cfg.scheduler);
    settings.setValue("sd_preset_" + key + "_steps", cfg.steps);
    settings.setValue("sd_preset_" + key + "_cfg", cfg.cfgScale);
    settings.setValue("sd_preset_" + key + "_clip_skip", cfg.clipSkip);
    settings.setValue("sd_preset_" + key + "_batch", cfg.batchCount);
    settings.setValue("sd_preset_" + key + "_seed", cfg.seed);
    settings.setValue("sd_preset_" + key + "_strength", cfg.strength);
    settings.setValue("sd_preset_" + key + "_guidance", cfg.guidance);
    settings.setValue("sd_preset_" + key + "_rng", cfg.rng);
    settings.setValue("sd_preset_" + key + "_video_frames", cfg.videoFrames);

    settings.setValue("sd_preset_" + key + "_flow_shift_en", cfg.flowShiftEnabled);
    settings.setValue("sd_preset_" + key + "_flow_shift", cfg.flowShift);
    settings.setValue("sd_preset_" + key + "_offload_cpu", cfg.offloadToCpu);
    settings.setValue("sd_preset_" + key + "_clip_cpu", cfg.clipOnCpu);
    settings.setValue("sd_preset_" + key + "_vae_cpu", cfg.vaeOnCpu);
    settings.setValue("sd_preset_" + key + "_control_cpu", cfg.controlNetOnCpu);
    settings.setValue("sd_preset_" + key + "_diff_fa", cfg.diffusionFA);

    settings.setValue("sd_preset_" + key + "_vae_tiling", cfg.vaeTiling);
    settings.setValue("sd_preset_" + key + "_vae_tile_x", cfg.vaeTileX);
    settings.setValue("sd_preset_" + key + "_vae_tile_y", cfg.vaeTileY);
    settings.setValue("sd_preset_" + key + "_vae_tile_overlap", cfg.vaeTileOverlap);

    settings.setValue("sd_preset_" + key + "_modify", cfg.modifyPrompt);
    settings.setValue("sd_preset_" + key + "_negative", cfg.negativePrompt);
    // Also mirror modify/negative to legacy last-used global keys for older flows
    settings.setValue("sd_adv_modify", cfg.modifyPrompt);
    settings.setValue("sd_adv_negative", cfg.negativePrompt);
    settings.sync();
}

void Expend::applyPresetToInlineUi(const QString &preset)
{
    Q_UNUSED(preset);
    // Legacy inline SD fields removed; advanced dialog is the single source of truth.
}

// Legacy inline SD path selection and auto-completion removed; advanced dialog contains all controls.

// 打开新的文生图高级参数弹窗
void Expend::on_sd_open_params_button_clicked()
{
    if (!sdParamsDialog_)
    {
        sdParamsDialog_ = new SdParamsDialog(this);
        // Initialize preset matching the last-used selection and inject its stored config
        sdParamsDialog_->setAutosaveMuted(true);
        QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
        settings.setIniCodec("utf-8");
        const QString presetNow = settings.value("sd_params_template", QStringLiteral("sd1.5-anything-3")).toString();
        sdParamsDialog_->applyPreset(presetNow);
        // Load stored config for current preset and reflect into dialog
        if (!sd_preset_configs_.contains(presetNow))
            sd_preset_configs_[presetNow] = loadPresetConfig(presetNow);
        SDRunConfig cfgNow = sd_preset_configs_.value(presetNow, SDRunConfig{});
        // Default sd1.5 prompts if missing, and persist immediately so that
        // switching presets does not wipe them out.
        if (presetNow == "sd1.5-anything-3")
        {
            const QString defMod = QStringLiteral("masterpieces, best quality, beauty, detailed, Pixar, 8k");
            const QString defNeg = QStringLiteral("EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure");
            bool applied = false;
            if (cfgNow.modifyPrompt.isEmpty())
            {
                cfgNow.modifyPrompt = defMod;
                applied = true;
            }
            if (cfgNow.negativePrompt.isEmpty())
            {
                cfgNow.negativePrompt = defNeg;
                applied = true;
            }
            if (applied)
            {
                // Persist defaults once so they survive preset switches
                sd_preset_configs_[presetNow] = cfgNow;
                sd_preset_modify_[presetNow] = cfgNow.modifyPrompt;
                sd_preset_negative_[presetNow] = cfgNow.negativePrompt;
                createTempDirectory(applicationDirPath + "/EVA_TEMP");
                savePresetConfig(presetNow, cfgNow);
            }
        }
        sdParamsDialog_->setConfig(cfgNow);
        // Keep legacy per-preset prompt store in sync for compatibility
        sdParamsDialog_->setPresetPromptStore(sd_preset_modify_, sd_preset_negative_);
        // Autosave handler: persist strictly per preset
        connect(sdParamsDialog_, &SdParamsDialog::accepted, this, [this](const SDRunConfig &cfg, const QString &preset)
                {
            QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
            settings.setIniCodec("utf-8");
            const QString p = preset.isEmpty()? settings.value("sd_params_template", QStringLiteral("sd1.5-anything-3")).toString() : preset;
            sd_preset_configs_[p] = cfg;
            sd_run_config_ = cfg; // make it current
            // Inline fields removed; no mirroring
            // Keep compatibility maps for prompts
            sd_preset_modify_[p] = cfg.modifyPrompt;
            sd_preset_negative_[p] = cfg.negativePrompt;
            // Persist to QSettings by preset
            createTempDirectory(applicationDirPath + "/EVA_TEMP");
            savePresetConfig(p, cfg);
            // Also persist current positive prompt for convenience (global)
            settings.setValue("sd_prompt", ui->sd_prompt_textEdit->toPlainText());
            // Keep last-used global keys aligned
            settings.setValue("sd_adv_modify", cfg.modifyPrompt);
            settings.setValue("sd_adv_negative", cfg.negativePrompt);
            settings.sync(); });
        // When user switches preset inside dialog, inject stored config so fields do not leak
        connect(sdParamsDialog_, &SdParamsDialog::presetChanged, this, [this](const QString &p)
                {
            QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
            settings.setIniCodec("utf-8");
            settings.setValue("sd_params_template", p);
            settings.sync();
            if (!sd_preset_configs_.contains(p)) sd_preset_configs_[p] = loadPresetConfig(p);
            // Update current run config and dialog fields
            sd_run_config_ = sd_preset_configs_.value(p);
            // If user switches to sd1.5 and prompts are missing, inject defaults
            if (p == "sd1.5-anything-3")
            {
                const QString defMod = QStringLiteral("masterpieces, best quality, beauty, detailed, Pixar, 8k");
                const QString defNeg = QStringLiteral("EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure");
                bool applied = false;
                if (sd_run_config_.modifyPrompt.isEmpty()) { sd_run_config_.modifyPrompt = defMod; applied = true; }
                if (sd_run_config_.negativePrompt.isEmpty()) { sd_run_config_.negativePrompt = defNeg; applied = true; }
                if (applied)
                {
                    sd_preset_configs_[p] = sd_run_config_;
                    sd_preset_modify_[p] = sd_run_config_.modifyPrompt;
                    sd_preset_negative_[p] = sd_run_config_.negativePrompt;
                    createTempDirectory(applicationDirPath + "/EVA_TEMP");
                    savePresetConfig(p, sd_run_config_);
                }
            }
            sdParamsDialog_->setConfig(sd_run_config_); });
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
    // 处理stop的情况（单按钮）
    if (ui->sd_draw_pushButton->text() == "stop")
    {
        ui->sd_log->appendPlainText("stop");
        sd_process->kill(); // 强制结束sd
        ui->sd_draw_pushButton->setText(QStringLiteral("生成"));
        img2img = false;
        return;
    }

    ui->sd_draw_pushButton->setText("stop");

    if (is_handle_sd && ui->sd_prompt_textEdit->toPlainText() == "")
    {
        ui->sd_log->appendPlainText(jtr("Please enter prompt words to tell the model what you want the image to look like"));
        return;
    }
    else if (is_handle_sd && sd_run_config_.modelPath.isEmpty())
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
    // Decide output extension by mode (image/video)
    const bool genVideo = (sd_run_config_.videoFrames > 0);
    sd_outputpath = applicationDirPath + "/EVA_TEMP/sd_output" + timeString + (genVideo ? ".avi" : ".png");

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
    // mode: image or video
    if (sd_run_config_.videoFrames > 0)
        arguments << "-M" << "vid_gen";
    else
        arguments << "-M" << "img_gen";
    // Decide img2img by whether user provided an init image
    img2img = (sd_imgDrop && QFile::exists(sd_imgDrop->imagePath()));
    if (img2img)
    {
        // Align with reference CLI: use -r <image> to enable image-to-image
        arguments << "-r" << ensureToolFriendlyFilePath(sd_imgDrop->imagePath());
        img2img = false;
        // strength for img2img (from config)
        arguments << "--strength" << QString::number(sd_run_config_.strength);
    }
    // main model arg
    SDModelArgKind argk = sd_run_config_.modelArg;
    if (argk == SDModelArgKind::Auto)
    {
        // Heuristic: prefer --diffusion-model when clip/t5/llm provided or filename hints
        const bool hasExtra = !sd_run_config_.t5xxlPath.isEmpty() || !sd_run_config_.llmPath.isEmpty() || !sd_run_config_.llmVisionPath.isEmpty() || !sd_run_config_.clipLPath.isEmpty() || !sd_run_config_.clipGPath.isEmpty();
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
    if (!sd_run_config_.llmPath.isEmpty()) arguments << "--llm" << ensureToolFriendlyFilePath(sd_run_config_.llmPath);
 
    if (!sd_run_config_.llmVisionPath.isEmpty()) arguments << "--llm_vision" << ensureToolFriendlyFilePath(sd_run_config_.llmVisionPath);
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
    // Legacy single-file LoRA fallback removed; advanced dialog uses directory

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
    if (sd_run_config_.videoFrames > 0)
        arguments << "--video-frames" << QString::number(sd_run_config_.videoFrames);

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
    ui->sd_draw_pushButton->setText(QStringLiteral("生成"));

    // Detect media type by file suffix
    const QString suffix = QFileInfo(sd_outputpath).suffix().toLower();
    const bool isVideo = (suffix == "avi" || suffix == "mp4" || suffix == "mov" || suffix == "mkv");

    bool ok = false;
    if (isVideo)
    {
        // Some sd backends may ignore -o name and produce sd_output-..avi, try to discover
        QString path = sd_outputpath;
        if (!QFileInfo::exists(path))
        {
            QDir dir(applicationDirPath + "/EVA_TEMP");
            const QStringList cands = dir.entryList(QStringList() << "sd_output-*.avi" << "sd_output*.avi", QDir::Files, QDir::Time);
            if (!cands.isEmpty()) path = dir.absoluteFilePath(cands.first());
        }
        if (QFileInfo::exists(path) && sd_mediaResult)
        {
            sd_mediaResult->addVideo(path);
            ok = true;
            sd_outputpath = path; // normalize for tool signal
        }
    }
    else
    {
        QImage image(sd_outputpath);
        const int originalWidth = image.width() / qMax(1.0, devicePixelRatioF());
        if (originalWidth > 0 && sd_mediaResult)
        {
            sd_mediaResult->addImage(sd_outputpath);
            ok = true;
        }
    }

    // 处理工具调用情况
    if (!is_handle_sd && ok)
    {
        is_handle_sd = true;
        emit expend2ui_state("expend:" + jtr("draw over"), USUAL_SIGNAL);
        emit expend2tool_drawover(current_sd_invocation_id_, sd_outputpath, 1); // 绘制完成信号
    }
    else if (!is_handle_sd)
    {
        is_handle_sd = true;
        if (sd_process_output.contains("CUDA error"))
        {
            emit expend2ui_state("expend:" + jtr("draw fail cuda"), WRONG_SIGNAL);
            emit expend2tool_drawover(current_sd_invocation_id_, jtr("draw fail cuda"), 0); // 绘制完成信号
        }
        else
        {
            emit expend2ui_state("expend:" + jtr("draw fail prompt"), WRONG_SIGNAL);
            emit expend2tool_drawover(current_sd_invocation_id_, jtr("draw fail prompt"), 0); // 绘制完成信号
        }
    }
}

// 接收到tool的开始绘制图像信号
void Expend::recv_draw(quint64 invocationId, QString prompt_)
{
    // 判断是否空闲
    if (!ui->sd_draw_pushButton->isEnabled())
    {
        emit expend2tool_drawover(invocationId, "stablediffusion" + jtr("Running, please try again later"), 0); // 绘制完成信号
        return;
    }
    else if (sd_run_config_.modelPath.isEmpty())
    {
        emit expend2tool_drawover(invocationId, jtr("The command is invalid. Please ask the user to specify the SD model path in the breeding window first"), 0); // 绘制完成信号
        return;
    }
    current_sd_invocation_id_ = invocationId;
    // 先把提示词写进输入框
    ui->sd_prompt_textEdit->setText(prompt_);
    // 不是手动
    is_handle_sd = false;
    // 触发绘制
    ui->sd_draw_pushButton->click();
}
