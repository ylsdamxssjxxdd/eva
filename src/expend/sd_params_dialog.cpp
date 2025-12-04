// sd_params_dialog.cpp - implementation

#include "sd_params_dialog.h"
#include <climits>

static QWidget *wrapH(QWidget *a, QWidget *b = nullptr, int spacing = 6)
{
    QWidget *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(spacing);
    lay->addWidget(a);
    if (b)
        lay->addWidget(b);
    lay->addStretch(1);
    return w;
}

SdParamsDialog::SdParamsDialog(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("EVA | SD Settings");
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    buildUi();
    resize(720, 560);
}

void SdParamsDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // Preset selector
    presetBox_ = new QComboBox;
    presetBox_->addItems({"flux1-dev", "qwen-image", "z-image", "sd1.5-anything-3", "wan2.2", "custom1", "custom2"});
    connect(presetBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SdParamsDialog::onPresetChanged);
    root->addWidget(wrapH(new QLabel("Preset"), presetBox_));

    // Model paths group (left column)
    auto *grpModel = new QGroupBox("Model Paths");
    auto *formModel = new QFormLayout(grpModel);
    formModel->setLabelAlignment(Qt::AlignRight);
    modelArgBox_ = new QComboBox; // Auto / -m / --diffusion-model
    modelArgBox_->addItems({"auto", "-m (legacy)", "--diffusion-model"});
    modelArgBox_->setToolTip("Main model argument: auto/-m/--diffusion-model");
    formModel->addRow("Model Arg", modelArgBox_);
    modelPathLe_ = addPathRow(formModel, "Model", "(*.ckpt *.safetensors *.diffusers *.gguf *.ggml *.pt)");
    modelPathLe_->setToolTip("Main model file path");
    vaeLe_ = addPathRow(formModel, "VAE", "(*.ckpt *.safetensors *.gguf *.ggml *.pt)");
    vaeLe_->setToolTip("VAE model (optional)");
    llmLe_ = addPathRow(formModel, "LLM", "(*.gguf *.ggml *.safetensors)");
    llmLe_->setToolTip("LLM text encoder (formerly Qwen2-VL)");

    llmVisionLe_ = addPathRow(formModel, "LLM-Vision", "(*.safetensors *.gguf *.ggml)");

    llmVisionLe_->setToolTip("LLM vision encoder (optional)");
    clipLLe_ = addPathRow(formModel, "CLIP-L", "(*.safetensors *.gguf *.ggml)");
    clipLLe_->setToolTip("CLIP-L text encoder");
    clipGLe_ = addPathRow(formModel, "CLIP-G", "(*.safetensors *.gguf *.ggml)");
    clipGLe_->setToolTip("CLIP-G text encoder");
    clipVisionLe_ = addPathRow(formModel, "CLIP-Vision", "(*.safetensors *.gguf *.ggml)");
    clipVisionLe_->setToolTip("CLIP-Vision encoder (optional)");
    t5Le_ = addPathRow(formModel, "T5XXL", "(*.safetensors *.gguf *.ggml)");
    t5Le_->setToolTip("T5-XXL text encoder");
    loraDirLe_ = addPathRow(formModel, "LoRA Dir", QString(), /*directory*/ true);
    loraDirLe_->setToolTip("Directory containing LoRA .safetensors");
    taesdLe_ = addPathRow(formModel, "TAESD", "(*.safetensors *.gguf *.ggml)");
    taesdLe_->setToolTip("Tiny AutoEncoder for fast decoding (lower quality)");
    upscaleLe_ = addPathRow(formModel, "ESRGAN", "(*.pth *.pt *.onnx)");
    upscaleLe_->setToolTip("ESRGAN upscaler model");
    controlNetLe_ = addPathRow(formModel, "ControlNet", "(*.safetensors *.gguf *.ggml)");
    controlNetLe_->setToolTip("ControlNet model (optional)");
    controlImgLe_ = addPathRow(formModel, "Control Image", "(*.png *.jpg *.jpeg)");
    controlImgLe_->setToolTip("Image for ControlNet conditioning");
    // Prompts group (right column top)
    grpPrompts_ = new QGroupBox("Prompts");
    auto *formP = new QFormLayout(grpPrompts_);
    modifyEdit_ = new QLineEdit;
    negativeEdit_ = new QPlainTextEdit;
    negativeEdit_->setFixedHeight(60);
    formP->addRow("Modify", modifyEdit_);
    formP->addRow("Negative", negativeEdit_);

    // Generation group (right column, stacked with backend)
    auto *grpGen = new QGroupBox("Generation");
    auto *formGen = new QFormLayout(grpGen);
    wSpin_ = new QSpinBox;
    wSpin_->setRange(64, 4096);
    wSpin_->setSingleStep(64);
    hSpin_ = new QSpinBox;
    hSpin_->setRange(64, 4096);
    hSpin_->setSingleStep(64);
    samplerBox_ = new QComboBox;
    samplerBox_->addItems({"euler", "euler_a", "heun", "dpm2", "dpm++2s_a", "dpm++2m", "dpm++2mv2", "ipndm", "ipndm_v", "lcm", "ddim_trailing", "tcd"});
    samplerBox_->setToolTip("Sampling method (e.g. euler/euler_a/dpm++2m/...)");
    schedulerBox_ = new QComboBox;
    schedulerBox_->addItems({"discrete", "karras", "exponential", "ays", "gits", "smoothstep", "sgm_uniform", "simple"});
    schedulerBox_->setToolTip("Sigma scheduler for denoising");
    stepsSpin_ = new QSpinBox;
    stepsSpin_->setRange(1, 200);
    cfgSpin_ = new QDoubleSpinBox;
    cfgSpin_->setRange(0.0, 50.0);
    cfgSpin_->setDecimals(2);
    cfgSpin_->setSingleStep(0.1);
    clipSkipSpin_ = new QSpinBox;
    clipSkipSpin_->setRange(-1, 12);
    batchSpin_ = new QSpinBox;
    batchSpin_->setRange(1, 16);
    seedSpin_ = new QSpinBox;
    seedSpin_->setRange(-1, INT_MAX);
    strengthSpin_ = new QDoubleSpinBox;
    strengthSpin_->setRange(0.0, 1.0);
    strengthSpin_->setSingleStep(0.01);
    guidanceSpin_ = new QDoubleSpinBox;
    guidanceSpin_->setRange(0.0, 20.0);
    guidanceSpin_->setSingleStep(0.1);
    rngBox_ = new QComboBox;
    rngBox_->addItems({"cuda", "std_default"});
    // Video frames (0=image mode; >0=vid_gen)
    videoFramesSpin_ = new QSpinBox;
    videoFramesSpin_->setRange(0, 1024);
    videoFramesSpin_->setValue(0);

    formGen->addRow("Width", wSpin_);
    wSpin_->setToolTip("Image width (px), multiple of 64 recommended");
    formGen->addRow("Height", hSpin_);
    hSpin_->setToolTip("Image height (px), multiple of 64 recommended");
    formGen->addRow("Sampler", samplerBox_);
    formGen->addRow("Scheduler", schedulerBox_);
    formGen->addRow("Steps", stepsSpin_);
    stepsSpin_->setToolTip("Sampling steps");
    formGen->addRow("CFG Scale", cfgSpin_);
    cfgSpin_->setToolTip("Prompt adherence: higher = closer to prompt");
    formGen->addRow("Clip Skip", clipSkipSpin_);
    clipSkipSpin_->setToolTip("Ignore last CLIP layers (-1 = auto)");
    formGen->addRow("Batch Count", batchSpin_);
    batchSpin_->setToolTip("Number of images to generate");
    formGen->addRow("Seed", seedSpin_);
    seedSpin_->setToolTip("Random seed (-1 = random)");
    formGen->addRow("Video Frames", videoFramesSpin_);
    videoFramesSpin_->setToolTip("0 = image; >0 = number of video frames");
    formGen->addRow("Strength", strengthSpin_);
    strengthSpin_->setToolTip("Img2img strength (0..1)");
    formGen->addRow("Guidance", guidanceSpin_);
    guidanceSpin_->setToolTip("Distilled guidance (for SD3/WAN)");
    formGen->addRow("RNG", rngBox_);
    rngBox_->setToolTip("RNG backend (cuda/std_default)");
    // Flow
    auto *flowRow = new QWidget;
    auto *flowLay = new QHBoxLayout(flowRow);
    flowLay->setContentsMargins(0, 0, 0, 0);
    flowShiftEnable_ = new QCheckBox("enable");
    flowShiftSpin_ = new QDoubleSpinBox;
    flowShiftSpin_->setRange(-1000, 1000);
    flowShiftSpin_->setSingleStep(0.5);
    flowLay->addWidget(flowShiftEnable_);
    flowLay->addWidget(new QLabel("value"));
    flowLay->addWidget(flowShiftSpin_);
    flowLay->addStretch(1);
    formGen->addRow("Flow Shift", flowRow);
    // Backend group (right column)
    auto *grpBk = new QGroupBox("Backend/Memory");
    auto *bkLay = new QGridLayout(grpBk);
    offloadCpuCb_ = new QCheckBox("--offload-to-cpu");
    offloadCpuCb_->setToolTip("Keep weights in RAM and load to VRAM on demand");
    clipCpuCb_ = new QCheckBox("--clip-on-cpu");
    clipCpuCb_->setToolTip("Keep CLIP on CPU (save VRAM)");
    vaeCpuCb_ = new QCheckBox("--vae-on-cpu");
    vaeCpuCb_->setToolTip("Keep VAE on CPU (save VRAM)");
    controlCpuCb_ = new QCheckBox("--control-net-cpu");
    controlCpuCb_->setToolTip("Keep ControlNet on CPU (save VRAM)");
    diffFaCb_ = new QCheckBox("--diffusion-fa");
    diffFaCb_->setToolTip("Use flash-attention in diffusion (save VRAM)");
    bkLay->addWidget(offloadCpuCb_, 0, 0);
    bkLay->addWidget(clipCpuCb_, 0, 1);
    bkLay->addWidget(vaeCpuCb_, 1, 0);
    bkLay->addWidget(controlCpuCb_, 1, 1);
    bkLay->addWidget(diffFaCb_, 2, 0);
    // VAE tiling row
    vaeTilingCb_ = new QCheckBox("--vae-tiling");
    vaeTilingCb_->setToolTip("Process VAE in tiles to reduce VRAM usage");
    vaeTileX_ = new QSpinBox;
    vaeTileX_->setRange(1, 2048);
    vaeTileX_->setValue(32);
    vaeTileX_->setSuffix(" px");
    vaeTileY_ = new QSpinBox;
    vaeTileY_->setRange(1, 2048);
    vaeTileY_->setValue(32);
    vaeTileY_->setSuffix(" px");
    vaeTileOverlap_ = new QDoubleSpinBox;
    vaeTileOverlap_->setRange(0.0, 1.0);
    vaeTileOverlap_->setSingleStep(0.05);
    vaeTileOverlap_->setValue(0.5);
    bkLay->addWidget(vaeTilingCb_, 3, 0, 1, 2);
    // Pack tile controls into an inline row to keep layout tidy
    auto *tilingRow = new QWidget;
    auto *tilingLay = new QHBoxLayout(tilingRow);
    tilingLay->setContentsMargins(0, 0, 0, 0);
    tilingLay->setSpacing(6);
    tilingLay->addWidget(new QLabel("tile"));
    tilingLay->addWidget(vaeTileX_);
    tilingLay->addWidget(new QLabel("x"));
    tilingLay->addWidget(vaeTileY_);
    tilingLay->addSpacing(12);
    tilingLay->addWidget(new QLabel("overlap"));
    tilingLay->addWidget(vaeTileOverlap_);
    tilingLay->addStretch(1);
    bkLay->addWidget(tilingRow, 4, 0, 1, 2);
    tilingRow->setEnabled(false);
    connect(vaeTilingCb_, &QCheckBox::toggled, tilingRow, &QWidget::setEnabled);
    // Two-column layout to reduce vertical length
    auto *cols = new QHBoxLayout;
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(10);
    auto *leftCol = new QVBoxLayout;
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(8);
    auto *rightCol = new QVBoxLayout;
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(8);
    // Put prompts under model paths on the left side
    leftCol->addWidget(grpModel, /*stretch*/ 1);
    leftCol->addWidget(grpPrompts_);
    rightCol->addWidget(grpGen);
    rightCol->addWidget(grpBk);
    rightCol->addStretch(1);
    cols->addLayout(leftCol, /*stretch*/ 1);
    cols->addLayout(rightCol, /*stretch*/ 1);
    root->addLayout(cols);

    // No Apply/Cancel/Close buttons: changes are autosaved immediately.
    // The window close button remains available from the title bar.

    hookAutosave();
}

QLineEdit *SdParamsDialog::addPathRow(QFormLayout *form, const QString &label, const QString &filter, bool directory)
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *le = new QLineEdit;
    le->setMinimumWidth(420);
    auto *btn = new QPushButton("...");
    btn->setFixedWidth(28);
    lay->addWidget(le);
    lay->addWidget(btn);
    if (directory)
        connect(btn, &QPushButton::clicked, this, [=]
                { onBrowseDir(le); });
    else
        connect(btn, &QPushButton::clicked, this, [=]
                { onBrowse(le, filter); });
    form->addRow(label, w);
    return le;
}

void SdParamsDialog::onBrowse(QLineEdit *le, const QString &filter)
{
    const QString path = QFileDialog::getOpenFileName(this, "Choose file", le->text(), filter.isEmpty() ? "All (*)" : filter);
    if (!path.isEmpty()) le->setText(path);
}

void SdParamsDialog::onBrowseDir(QLineEdit *le)
{
    const QString path = QFileDialog::getExistingDirectory(this, "Choose directory", le->text());
    if (!path.isEmpty()) le->setText(path);
}

void SdParamsDialog::setConfig(const SDRunConfig &c)
{
    // Avoid triggering autosave while populating fields
    muteSignals_ = true;
    // Preset stays unchanged here; caller controls presetBox_ if needed
    modelArgBox_->setCurrentIndex(static_cast<int>(c.modelArg));
    modelPathLe_->setText(c.modelPath);
    vaeLe_->setText(c.vaePath);
    clipLLe_->setText(c.clipLPath);
    clipGLe_->setText(c.clipGPath);
    clipVisionLe_->setText(c.clipVisionPath);
    t5Le_->setText(c.t5xxlPath);
    llmLe_->setText(c.llmPath);

    llmVisionLe_->setText(c.llmVisionPath);
    loraDirLe_->setText(c.loraDirPath);
    taesdLe_->setText(c.taesdPath);
    upscaleLe_->setText(c.upscaleModelPath);
    controlNetLe_->setText(c.controlNetPath);
    controlImgLe_->setText(c.controlImagePath);

    wSpin_->setValue(c.width);
    hSpin_->setValue(c.height);
    samplerBox_->setCurrentText(c.sampler);
    schedulerBox_->setCurrentText(c.scheduler);
    stepsSpin_->setValue(c.steps);
    cfgSpin_->setValue(c.cfgScale);
    clipSkipSpin_->setValue(c.clipSkip);
    batchSpin_->setValue(c.batchCount);
    seedSpin_->setValue(c.seed);
    strengthSpin_->setValue(c.strength);
    guidanceSpin_->setValue(c.guidance);
    rngBox_->setCurrentText(c.rng);
    if (videoFramesSpin_) videoFramesSpin_->setValue(c.videoFrames);

    flowShiftEnable_->setChecked(c.flowShiftEnabled);
    flowShiftSpin_->setValue(c.flowShift);

    offloadCpuCb_->setChecked(c.offloadToCpu);
    clipCpuCb_->setChecked(c.clipOnCpu);
    vaeCpuCb_->setChecked(c.vaeOnCpu);
    controlCpuCb_->setChecked(c.controlNetOnCpu);
    diffFaCb_->setChecked(c.diffusionFA);

    vaeTilingCb_->setChecked(c.vaeTiling);
    vaeTileX_->setValue(c.vaeTileX);
    vaeTileY_->setValue(c.vaeTileY);
    vaeTileOverlap_->setValue(c.vaeTileOverlap);

    // Prompts
    if (modifyEdit_) modifyEdit_->setText(c.modifyPrompt);
    if (negativeEdit_) negativeEdit_->setPlainText(c.negativePrompt);
    muteSignals_ = false;
}

SDRunConfig SdParamsDialog::config() const
{
    SDRunConfig c;
    c.modelArg = static_cast<SDModelArgKind>(modelArgBox_->currentIndex());
    c.modelPath = modelPathLe_->text();
    c.vaePath = vaeLe_->text();
    c.clipLPath = clipLLe_->text();
    c.clipGPath = clipGLe_->text();
    c.clipVisionPath = clipVisionLe_->text();
    c.t5xxlPath = t5Le_->text();
    c.llmPath = llmLe_->text();
    c.llmVisionPath = llmVisionLe_->text();
    c.loraDirPath = loraDirLe_->text();
    c.taesdPath = taesdLe_->text();
    c.upscaleModelPath = upscaleLe_->text();
    c.controlNetPath = controlNetLe_->text();
    c.controlImagePath = controlImgLe_->text();

    c.width = wSpin_->value();
    c.height = hSpin_->value();
    c.sampler = samplerBox_->currentText();
    c.scheduler = schedulerBox_->currentText();
    c.steps = stepsSpin_->value();
    c.cfgScale = cfgSpin_->value();
    c.clipSkip = clipSkipSpin_->value();
    c.batchCount = batchSpin_->value();
    c.seed = seedSpin_->value();
    c.strength = strengthSpin_->value();
    c.guidance = guidanceSpin_->value();
    c.rng = rngBox_->currentText();
    c.videoFrames = videoFramesSpin_ ? videoFramesSpin_->value() : 0;

    c.flowShiftEnabled = flowShiftEnable_->isChecked();
    c.flowShift = flowShiftSpin_->value();

    c.offloadToCpu = offloadCpuCb_->isChecked();
    c.clipOnCpu = clipCpuCb_->isChecked();
    c.vaeOnCpu = vaeCpuCb_->isChecked();
    c.controlNetOnCpu = controlCpuCb_->isChecked();
    c.diffusionFA = diffFaCb_->isChecked();

    c.vaeTiling = vaeTilingCb_->isChecked();
    c.vaeTileX = vaeTileX_->value();
    c.vaeTileY = vaeTileY_->value();
    c.vaeTileOverlap = vaeTileOverlap_->value();
    // Prompts
    c.modifyPrompt = modifyEdit_ ? modifyEdit_->text() : QString();
    c.negativePrompt = negativeEdit_ ? negativeEdit_->toPlainText() : QString();
    return c;
}

void SdParamsDialog::applyPreset(const QString &name)
{
    // Reflect preset in combobox selection first
    if (presetBox_)
    {
        int idx = presetBox_->findText(name);
        if (idx >= 0) presetBox_->setCurrentIndex(idx);
    }
    // Minimal sensible defaults tailored for the three main families.
    if (name == "flux1-dev")
    {
        modelArgBox_->setCurrentIndex(static_cast<int>(SDModelArgKind::Diffusion));
        samplerBox_->setCurrentText("euler");
        wSpin_->setValue(768);
        hSpin_->setValue(768);
        stepsSpin_->setValue(30);
        cfgSpin_->setValue(1.0);
        clipSkipSpin_->setValue(-1);
        batchSpin_->setValue(1);
        seedSpin_->setValue(-1);
        rngBox_->setCurrentText("cuda");
        diffFaCb_->setChecked(false);
        offloadCpuCb_->setChecked(false);
        clipCpuCb_->setChecked(true); // recommended: --clip-on-cpu
        flowShiftEnable_->setChecked(false);
        // keep user's prompts as-is
    }
    else if (name == "qwen-image")
    {
        modelArgBox_->setCurrentIndex(static_cast<int>(SDModelArgKind::Diffusion));
        samplerBox_->setCurrentText("euler");
        wSpin_->setValue(1024);
        hSpin_->setValue(1024);
        stepsSpin_->setValue(30);
        cfgSpin_->setValue(2.5);
        clipSkipSpin_->setValue(-1);
        batchSpin_->setValue(1);
        seedSpin_->setValue(-1);
        rngBox_->setCurrentText("cuda");
        diffFaCb_->setChecked(true);
        offloadCpuCb_->setChecked(true);
        flowShiftEnable_->setChecked(true);
        flowShiftSpin_->setValue(3.0);
        // keep user's prompts as-is
    }
    else if (name == "z-image")
    {
        // 按 z-image 超快配置给出默认参数，采用 512x1024 分辨率与 1.0 CFG，便于一键出图
        modelArgBox_->setCurrentIndex(static_cast<int>(SDModelArgKind::Diffusion));
        samplerBox_->setCurrentText("euler");
        wSpin_->setValue(512);
        hSpin_->setValue(1024);
        stepsSpin_->setValue(20);
        cfgSpin_->setValue(1.0);
        clipSkipSpin_->setValue(-1);
        batchSpin_->setValue(1);
        seedSpin_->setValue(-1);
        rngBox_->setCurrentText("cuda");
        diffFaCb_->setChecked(true);
        offloadCpuCb_->setChecked(true);
        flowShiftEnable_->setChecked(false);
        if (modifyEdit_ && modifyEdit_->text().trimmed().isEmpty())
            modifyEdit_->setText(QStringLiteral("EVA第十三号机"));
    }
    else if (name == "sd1.5-anything-3")
    {
        modelArgBox_->setCurrentIndex(static_cast<int>(SDModelArgKind::LegacyM));
        samplerBox_->setCurrentText("euler_a");
        wSpin_->setValue(512);
        hSpin_->setValue(512);
        stepsSpin_->setValue(20);
        cfgSpin_->setValue(7.5);
        clipSkipSpin_->setValue(1);
        batchSpin_->setValue(1);
        seedSpin_->setValue(-1);
        rngBox_->setCurrentText("cuda");
        diffFaCb_->setChecked(false);
        offloadCpuCb_->setChecked(false);
        flowShiftEnable_->setChecked(false);
        // Provide sd1.5 default prompts if empty (do not override user content)
        const QString defaultModify = QStringLiteral("masterpieces, best quality, beauty, detailed, Pixar, 8k");
        const QString defaultNegative = QStringLiteral("EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure");
        if (modifyEdit_ && modifyEdit_->text().trimmed().isEmpty())
            modifyEdit_->setText(defaultModify);
        if (negativeEdit_ && negativeEdit_->toPlainText().trimmed().isEmpty())
            negativeEdit_->setPlainText(defaultNegative);
    }
    else if (name == "wan2.2")
    {
        modelArgBox_->setCurrentIndex(static_cast<int>(SDModelArgKind::Diffusion));
        samplerBox_->setCurrentText("euler");
        wSpin_->setValue(480);
        hSpin_->setValue(832);
        stepsSpin_->setValue(30);
        cfgSpin_->setValue(6.0);
        clipSkipSpin_->setValue(-1);
        batchSpin_->setValue(1);
        seedSpin_->setValue(-1);
        rngBox_->setCurrentText("cuda");
        diffFaCb_->setChecked(true);
        offloadCpuCb_->setChecked(true);
        flowShiftEnable_->setChecked(true);
        flowShiftSpin_->setValue(3.0);
        if (videoFramesSpin_) videoFramesSpin_->setValue(33);
        // Wan2.2 recommended negative prompt (Chinese); keep user's content if any
        if (negativeEdit_ && negativeEdit_->toPlainText().trimmed().isEmpty())
            negativeEdit_->setPlainText(QStringLiteral("色调艳丽，过曝，静态，细节模糊不清，字幕，风格，作品，画作，画面，静止，整体发灰，最差质量，低质量，JPEG压缩残留，丑陋的，残缺的，多余的手指，画得不好的手部，画得不好的脸部，畸形的，毁容的，形态畸形的肢体，手指融合，静止不动的画面，杂乱的背景，三条腿，背景人很多，倒着走"));
    }
    else if (name == "custom1" || name == "custom2")
    {
        // Keep user's saved config; do not override fields here.
        // Selection is already applied to presetBox_.
    }
    // Notify owner to load preset-specific stored config
    emit presetChanged(name);
}

void SdParamsDialog::onPresetChanged(int)
{
    const QString name = presetBox_->currentText();
    // Mute autosave while programmatically switching preset UI
    const bool prevMute = muteSignals_;
    muteSignals_ = true;
    applyPreset(name);
    // If caller provided per-preset store, reflect it.
    if (modifyEdit_ && negativeEdit_)
    {
        const QString mod = presetModify_.value(name);
        const QString neg = presetNegative_.value(name);
        if (name == "sd1.5-anything-3")
        {
            // sd1.5: if empty, provide defaults
            const QString defaultModify = QStringLiteral("masterpieces, best quality, beauty, detailed, Pixar, 8k");
            const QString defaultNegative = QStringLiteral("EasyNegative,badhandv4,ng_deepnegative_v1_75t,worst quality, low quality, normal quality, lowres, monochrome, grayscale, bad anatomy,DeepNegative, skin spots, acnes, skin blemishes, fat, facing away, looking away, tilted head, lowres, bad anatomy, bad hands, missing fingers, extra digit, fewer digits, bad feet, poorly drawn hands, poorly drawn face, mutation, deformed, extra fingers, extra limbs, extra arms, extra legs, malformed limbs,fused fingers,too many fingers,long neck,cross-eyed,mutated hands,polar lowres,bad body,bad proportions,gross proportions,missing arms,missing legs,extra digit, extra arms, extra leg, extra foot,teethcroppe,signature, watermark, username,blurry,cropped,jpeg artifacts,text,error,Lower body exposure");
            modifyEdit_->setText(mod.isEmpty() ? defaultModify : mod);
            negativeEdit_->setPlainText(neg.isEmpty() ? defaultNegative : neg);
        }
        else
        {
            // Non-sd1.5: reflect own store only (may be empty), avoid cross-preset residue
            modifyEdit_->setText(mod);
            negativeEdit_->setPlainText(neg);
        }
    }
    muteSignals_ = prevMute;
    // Notify owner to load preset-specific stored config
    emit presetChanged(name);
}

// Connect all fields to a debounced autosave callback
void SdParamsDialog::hookAutosave()
{
    autosaveTimer_.setSingleShot(true);
    autosaveTimer_.setInterval(250); // debounce interval
    connect(&autosaveTimer_, &QTimer::timeout, this, [this]
            {
        if (!muteSignals_) emit accepted(config(), presetBox_? presetBox_->currentText() : QString()); });

    auto arm = [this]
    { if (!muteSignals_) { autosaveTimer_.start(); } };

    // Combo boxes
    connect(modelArgBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, arm);
    connect(samplerBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, arm);
    connect(schedulerBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, arm);
    connect(rngBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, arm);
    // Line edits
    for (QLineEdit *le : {modelPathLe_, vaeLe_, clipLLe_, clipGLe_, clipVisionLe_, t5Le_, llmLe_, llmVisionLe_, loraDirLe_, taesdLe_, upscaleLe_, controlNetLe_, controlImgLe_, modifyEdit_})
        if (le) connect(le, &QLineEdit::textChanged, this, arm);
    // Text edits
    if (negativeEdit_) connect(negativeEdit_, &QPlainTextEdit::textChanged, this, arm);
    // Spin boxes
    for (QSpinBox *sb : {wSpin_, hSpin_, stepsSpin_, clipSkipSpin_, batchSpin_, seedSpin_, videoFramesSpin_, vaeTileX_, vaeTileY_})
        if (sb) connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, arm);
    for (QDoubleSpinBox *ds : {cfgSpin_, strengthSpin_, guidanceSpin_, vaeTileOverlap_})
        if (ds) connect(ds, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, arm);
    // Checkboxes
    for (QCheckBox *cb : {flowShiftEnable_, offloadCpuCb_, clipCpuCb_, vaeCpuCb_, controlCpuCb_, diffFaCb_, vaeTilingCb_})
        if (cb) connect(cb, &QCheckBox::toggled, this, arm);
}

void SdParamsDialog::onAnyChanged()
{
    if (!muteSignals_) emit accepted(config(), presetBox_ ? presetBox_->currentText() : QString());
}
