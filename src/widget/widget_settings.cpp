#include "ui_widget.h"
#include "widget.h"
#include <QtGlobal>
#include <QComboBox>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QFile>
#include <QFontDatabase>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSize>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QVector>
#include <QStringList>

namespace
{
int clampFontPointSize(int candidate, int fallback = Widget::kDefaultUiFontPt)
{
    const int safeFallback = fallback > 0 ? fallback : Widget::kDefaultUiFontPt;
    if (candidate <= 0)
    {
        return safeFallback;
    }
    return qBound(Widget::kMinFontPt, candidate, Widget::kMaxFontPt);
}

QString resolveSarasaFallback(const QString &resourceFamily)
{
    if (!resourceFamily.trimmed().isEmpty())
    {
        return resourceFamily.trimmed();
    }
    return QStringLiteral("Sarasa Fixed CL");
}

bool isLegacyOutputFont(const QString &family)
{
    const QString trimmed = family.trimmed();
    if (trimmed.isEmpty()) return true;
    const QString lower = trimmed.toLower();
    // 将早期的宋体/点阵字体（如 zpix）视为需要回退到默认 Sarasa 的旧配置
    return lower.contains(QStringLiteral("simsun")) || lower.contains(QStringLiteral("zpix"));
}
} // namespace

//-------------------------------------------------------------------------
//--------------------------------设置选项相关------------------------------
//-------------------------------------------------------------------------
void Widget::set_SetDialog()
{
    settings_dialog = new QDialog(this);
    settings_dialog->setWindowFlags(settings_dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint); // 隐藏?按钮
    // settings_dialog->setWindowFlags(settings_dialog->windowFlags() & ~Qt::WindowCloseButtonHint);// 隐藏关闭按钮
    settings_ui = new Ui::Settings_Dialog_Ui;
    settings_ui->setupUi(settings_dialog);
    if (QLayout *layout = settings_dialog->layout())
    {
        layout->setSizeConstraint(QLayout::SetMinimumSize);
    }

    setupGlobalSettingsPanel();

    // 推理设备下拉：根据当前目录中可用后端动态填充
    {
        settings_ui->device_comboBox->clear();
        settings_ui->device_comboBox->addItem("auto");
        const QStringList backs = DeviceManager::availableBackends();
        for (const QString &b : backs)
        {
            settings_ui->device_comboBox->addItem(b);
        }
        // 初始值沿用进程内选择（默认 auto），避免在首次打开前就触发重启
        int idx = settings_ui->device_comboBox->findText(DeviceManager::userChoice());
        if (idx >= 0) settings_ui->device_comboBox->setCurrentIndex(idx);

        // 监听选择变化并初始化一次
        connect(settings_ui->device_comboBox, &QComboBox::currentTextChanged, this, &Widget::onDeviceComboTextChanged);
        refreshDeviceBackendUI();
        syncBackendOverrideState();
        if (settings_ui->device_label)
        {
            settings_ui->device_label->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(settings_ui->device_label, &QLabel::customContextMenuRequested, this, [this](const QPoint &)
                    { openBackendManagerDialog(QStringLiteral("llama-server-main")); }, Qt::UniqueConnection);
        }
        if (settings_ui->device_comboBox)
        {
            settings_ui->device_comboBox->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(settings_ui->device_comboBox, &QComboBox::customContextMenuRequested, this, [this](const QPoint &)
                    { openBackendManagerDialog(QStringLiteral("llama-server-main")); }, Qt::UniqueConnection);
        }
    }

    // 温度控制
    settings_ui->temp_slider->setRange(0, 100); // 设置范围为1到99
    settings_ui->temp_slider->setValue(qRound(ui_SETTINGS.temp * 100.0));
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0));
    connect(settings_ui->temp_slider, &QSlider::valueChanged, this, &Widget::temp_change);
    // 重复惩罚控制
    settings_ui->repeat_slider->setRange(0, 200); // 设置范围
    settings_ui->repeat_slider->setValue(qRound(ui_SETTINGS.repeat * 100.0));
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0));
    connect(settings_ui->repeat_slider, &QSlider::valueChanged, this, &Widget::repeat_change);
    // TOP_K 控制（采样）
    settings_ui->topk_slider->setRange(0, 200);
    settings_ui->topk_slider->setValue(ui_SETTINGS.top_k);
    settings_ui->topk_label->setText(jtr("top_k") + " " + QString::number(settings_ui->topk_slider->value()));
    connect(settings_ui->topk_slider, &QSlider::valueChanged, this, &Widget::topk_change);
    settings_ui->topk_label->setVisible(false);
    settings_ui->topk_slider->setVisible(false);
    settings_ui->topk_slider->setEnabled(false);
    // TOP_P 控制（采样）
    settings_ui->topp_slider->setRange(0, 100);
    settings_ui->topp_slider->setValue(qRound(ui_SETTINGS.hid_top_p * 100.0));
    settings_ui->topp_label->setText("TOP_P " + QString::number(settings_ui->topp_slider->value() / 100.0));
    // 提示：核采样阈值（nucleus sampling）范围 0.00–1.00
    settings_ui->topp_label->setToolTip(QString::fromUtf8("核采样阈值（top_p），范围 0.00–1.00；当前：%1")
                                            .arg(QString::number(settings_ui->topp_slider->value() / 100.0, 'f', 2)));
    connect(settings_ui->topp_slider, &QSlider::valueChanged, this, &Widget::topp_change);
    settings_ui->topp_label->setVisible(false);
    settings_ui->topp_slider->setVisible(false);
    settings_ui->topp_slider->setEnabled(false);
    // 推理/思考等级
    rebuildReasoningCombo();
    auto reasoningUpdater = [this]()
    {
        if (!settings_ui || !settings_ui->reasoning_label) return;
        settings_ui->reasoning_label->setText(jtr("reasoning effort"));
    };
    reasoningUpdater();
    if (settings_ui->reasoning_comboBox)
    {
        connect(settings_ui->reasoning_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [reasoningUpdater](int) mutable
                { reasoningUpdater(); }, Qt::UniqueConnection);
    }
    // 最大输出长度
    settings_ui->npredict_spin->setAccelerated(true);
    enforcePredictLimit(true);
    connect(settings_ui->npredict_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &Widget::npredict_change);
    // 加速支持
    settings_ui->ngl_slider->setRange(0, 99);
    settings_ui->ngl_slider->setValue(ui_SETTINGS.ngl);
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(settings_ui->ngl_slider->value()));
    connect(settings_ui->ngl_slider, &QSlider::valueChanged, this, &Widget::ngl_change);
    // cpu线程数设置
    settings_ui->nthread_slider->setValue(ui_SETTINGS.nthread);
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(settings_ui->nthread_slider->value()));
    connect(settings_ui->nthread_slider, &QSlider::valueChanged, this, &Widget::nthread_change);
    // ctx length 记忆容量
    settings_ui->nctx_slider->setRange(128, 32768);
    settings_ui->nctx_slider->setValue(ui_SETTINGS.nctx);
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(settings_ui->nctx_slider->value()));
    connect(settings_ui->nctx_slider, &QSlider::valueChanged, this, &Widget::nctx_change);
    // 并发数量（llama-server --parallel）
    settings_ui->parallel_slider->setValue(ui_SETTINGS.hid_parallel);
    settings_ui->parallel_label->setText(jtr("parallel") + " " + QString::number(settings_ui->parallel_slider->value()));
    connect(settings_ui->parallel_slider, &QSlider::valueChanged, this, &Widget::parallel_change);
    // load lora
    settings_ui->lora_LineEdit->setContextMenuPolicy(Qt::NoContextMenu); // 取消右键菜单
    settings_ui->lora_LineEdit->installEventFilter(this);
    // load mmproj
    settings_ui->mmproj_LineEdit->setContextMenuPolicy(Qt::NoContextMenu); // 取消右键菜单
    settings_ui->mmproj_LineEdit->installEventFilter(this);
    // 补完控制
    connect(settings_ui->complete_btn, &QRadioButton::clicked, this, &Widget::complete_change);
    // 多轮对话
    settings_ui->chat_btn->setChecked(1);
    connect(settings_ui->chat_btn, &QRadioButton::clicked, this, &Widget::chat_change);
    // 网页服务控制（服务状态已移除，仅保留端口）
    settings_ui->port_lineEdit->setText(ui_port);
    QIntValidator *validator = new QIntValidator(0, 65535); // 限制端口输入
    settings_ui->port_lineEdit->setValidator(validator);
    if (ui_port.isEmpty())
    {
        settings_ui->port_lineEdit->setPlaceholderText("blank = localhost only (random port)");
        settings_ui->port_lineEdit->setToolTip(QString());
    }
    else if (!lastPortConflictFallback_.isEmpty() && lastPortConflictPreferred_ == ui_port && lastPortConflictFallback_ != ui_port)
    {
        settings_ui->port_lineEdit->setPlaceholderText(jtr("port fallback placeholder").arg(lastPortConflictFallback_));
        settings_ui->port_lineEdit->setToolTip(jtr("port conflict body").arg(lastPortConflictPreferred_, lastPortConflictFallback_));
    }
    else
    {
        settings_ui->port_lineEdit->setPlaceholderText(QString());
        settings_ui->port_lineEdit->setToolTip(QString());
    }
    if (settings_ui->lazy_timeout_label)
    {
        settings_ui->lazy_timeout_label->setToolTip(jtr("pop countdown tooltip"));
        settings_ui->lazy_timeout_label->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(settings_ui->lazy_timeout_label, &QLabel::customContextMenuRequested, this, [this](const QPoint &)
                { onLazyUnloadNowClicked(); }, Qt::UniqueConnection);
    }
    if (settings_ui->allow_control_checkbox)
    {
        settings_ui->allow_control_checkbox->setText(jtr("control host allow"));
        settings_ui->allow_control_checkbox->setToolTip(jtr("control host allow tooltip").arg(QString::number(DEFAULT_CONTROL_PORT)));
    }
    if (settings_ui->lazy_timeout_spin)
    {
        settings_ui->lazy_timeout_spin->setRange(0, 1440);
        settings_ui->lazy_timeout_spin->setSuffix(QStringLiteral(" ") + jtr("minute short"));
        settings_ui->lazy_timeout_spin->setToolTip(jtr("pop disable tooltip"));
        settings_ui->lazy_timeout_spin->setValue(qBound(0, int(lazyUnloadMs_ / 60000), 1440));
        connect(settings_ui->lazy_timeout_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value)
                {
            if (!settings_ui || !settings_ui->lazy_timeout_label) return;
            const int currentMinutes = lazyUnloadMs_ > 0 ? int((lazyUnloadMs_ + 59999) / 60000) : 0;
            if (value == currentMinutes)
            {
                updateLazyCountdownLabel();
                return;
            }
            const QString pending = jtr("pop countdown pending");
            QString status;
            if (value <= 0)
            {
                status = jtr("pop countdown disabled");
            }
            else
            {
                status = QStringLiteral("%1 %2").arg(QString::number(value), jtr("minute short"));
            }
            QString statusDisplay = status;
            if (!pending.isEmpty())
            {
                if (!statusDisplay.isEmpty()) statusDisplay += QStringLiteral(" ");
                statusDisplay += pending;
            }
            setLazyCountdownLabelDisplay(statusDisplay);
        });
    }
    updateLazyCountdownLabel();
    // web_btn 已从 UI 移除
    connect(settings_ui->confirm, &QPushButton::clicked, this, &Widget::settings_ui_confirm_button_clicked);
    connect(settings_ui->cancel, &QPushButton::clicked, settings_dialog, &QDialog::reject);
    connect(settings_dialog, &QDialog::rejected, this, &Widget::settings_ui_cancel_button_clicked, Qt::UniqueConnection);

    settings_dialog->setWindowTitle(jtr("set"));
}

void Widget::rebuildReasoningCombo()
{
    if (!settings_ui || !settings_ui->reasoning_comboBox) return;
    struct ReasoningOption
    {
        const char *code;
        const char *labelKey;
    };
    static const QVector<ReasoningOption> options = {
        {"off", "reasoning option off"},
        {"minimal", "reasoning option minimal"},
        {"low", "reasoning option low"},
        {"medium", "reasoning option medium"},
        {"high", "reasoning option high"},
        {"auto", "reasoning option auto"}};

    const QString currentData = settings_ui->reasoning_comboBox->currentData().toString();
    QString target = currentData.isEmpty() ? ui_SETTINGS.reasoning_effort : currentData;
    target = sanitizeReasoningEffort(target);

    QSignalBlocker blocker(settings_ui->reasoning_comboBox);
    settings_ui->reasoning_comboBox->clear();
    for (const auto &opt : options)
    {
        settings_ui->reasoning_comboBox->addItem(jtr(opt.labelKey), QString::fromUtf8(opt.code));
    }
    int idx = settings_ui->reasoning_comboBox->findData(target);
    if (idx < 0) idx = settings_ui->reasoning_comboBox->findData(QStringLiteral("auto"));
    if (idx < 0) idx = 0;
    settings_ui->reasoning_comboBox->setCurrentIndex(qBound(0, idx, settings_ui->reasoning_comboBox->count() - 1));
    settings_ui->reasoning_comboBox->setToolTip(jtr("reasoning effort note"));
}

void Widget::setupGlobalSettingsPanel()
{
    if (!settings_ui || !settings_dialog) return;

    if (settings_ui->global_pushButton)
    {
        connect(settings_ui->global_pushButton, &QPushButton::clicked, this, &Widget::showGlobalSettingsDialog, Qt::UniqueConnection);
    }
    updateGlobalSettingsTranslations();
    syncGlobalSettingsPanelControls();
}

void Widget::updateGlobalSettingsTranslations()
{
    if (settings_ui && settings_ui->global_pushButton)
    {
        settings_ui->global_pushButton->setText(jtr("global settings button"));
        settings_ui->global_pushButton->setToolTip(jtr("open global settings"));
    }
    if (globalDialog_) globalDialog_->setWindowTitle(jtr("global settings title"));
    if (globalPanelTitleLabel_) globalPanelTitleLabel_->setText(jtr("global settings title"));
    if (globalFontLabel_) globalFontLabel_->setText(jtr("interface font"));
    if (globalFontSizeLabel_) globalFontSizeLabel_->setText(jtr("font size"));
    if (globalOutputFontLabel_) globalOutputFontLabel_->setText(jtr("output font"));
    if (globalOutputFontSizeLabel_) globalOutputFontSizeLabel_->setText(jtr("output font size"));
    if (globalThemeLabel_) globalThemeLabel_->setText(jtr("eva palette"));
    if (globalThemeCombo_)
    {
        struct ThemeTranslation
        {
            const char *id;
            const char *key;
        };
        const ThemeTranslation translations[] = {
            {"unit01", "eva theme unit01"},
            {"unit00", "eva theme unit00"},
            {"unit02", "eva theme unit02"},
            {"unit03", "eva theme unit03"}};
        QSignalBlocker blocker(globalThemeCombo_);
        for (const ThemeTranslation &translation : translations)
        {
            const int idx = globalThemeCombo_->findData(QString::fromLatin1(translation.id));
            if (idx >= 0)
            {
                globalThemeCombo_->setItemText(idx, jtr(translation.key));
            }
        }
    }
}

void Widget::syncGlobalSettingsPanelControls()
{
    if (!globalSettingsPanel_) return;
    if (globalFontCombo_)
    {
        QSignalBlocker blocker(globalFontCombo_);
        QFont font = QApplication::font();
        if (!globalUiSettings_.fontFamily.trimmed().isEmpty())
        {
            font.setFamily(globalUiSettings_.fontFamily);
        }
        globalFontCombo_->setCurrentFont(font);
    }
    if (globalFontSizeSpin_)
    {
        QSignalBlocker blocker(globalFontSizeSpin_);
        const int uiSize = clampFontPointSize(globalUiSettings_.fontSizePt, Widget::kDefaultUiFontPt);
        globalFontSizeSpin_->setValue(uiSize);
    }
    if (globalThemeCombo_)
    {
        QSignalBlocker blocker(globalThemeCombo_);
        int idx = globalThemeCombo_->findData(globalUiSettings_.themeId);
        if (idx < 0) idx = 0;
        globalThemeCombo_->setCurrentIndex(idx);
    }
    if (globalOutputFontCombo_)
    {
        QSignalBlocker blocker(globalOutputFontCombo_);
        globalOutputFontCombo_->setCurrentFont(currentOutputFont());
    }
    if (globalOutputFontSizeSpin_)
    {
        QSignalBlocker blocker(globalOutputFontSizeSpin_);
        const int fallback = outputFontFallbackSizePt_ > 0 ? outputFontFallbackSizePt_ : Widget::kDefaultOutputFontPt;
        const int outputSize = clampFontPointSize(globalUiSettings_.outputFontSizePt, fallback);
        globalOutputFontSizeSpin_->setValue(outputSize);
    }
}

void Widget::showGlobalSettingsDialog()
{
    if (!ensureGlobalSettingsDialog()) return;
    updateGlobalSettingsTranslations();
    syncGlobalSettingsPanelControls();
    if (settings_dialog)
    {
        const QRect parentGeom = settings_dialog->frameGeometry();
        const QSize hint = globalDialog_->sizeHint();
        QPoint topLeft = parentGeom.center() - QPoint(hint.width() / 2, hint.height() / 2);
        globalDialog_->move(topLeft);
    }
    globalDialog_->show();
    globalDialog_->raise();
    globalDialog_->activateWindow();
}

void Widget::handleGlobalFontFamilyChanged(const QFont &font)
{
    const int size = globalFontSizeSpin_ ? globalFontSizeSpin_->value() : globalUiSettings_.fontSizePt;
    applyGlobalFont(font.family(), size, true);
}

void Widget::handleGlobalFontSizeChanged(int value)
{
    const QString family = globalFontCombo_ ? globalFontCombo_->currentFont().family() : globalUiSettings_.fontFamily;
    applyGlobalFont(family, value, true);
}

void Widget::handleOutputFontFamilyChanged(const QFont &font)
{
    const int size = globalOutputFontSizeSpin_ ? globalOutputFontSizeSpin_->value() : resolvedOutputFontSize();
    applyOutputFont(font.family(), size, true);
}

void Widget::handleOutputFontSizeChanged(int value)
{
    const QString family = globalOutputFontCombo_ ? globalOutputFontCombo_->currentFont().family() : resolvedOutputFontFamily();
    applyOutputFont(family, value, true);
}

void Widget::handleGlobalThemeChanged(int index)
{
    if (!globalThemeCombo_) return;
    const QString themeId = globalThemeCombo_->itemData(index).toString();
    applyGlobalTheme(themeId, true);
}

void Widget::applyGlobalFont(const QString &family, int sizePt, bool persist)
{
    const QString trimmed = family.trimmed();
    if (!trimmed.isEmpty()) globalUiSettings_.fontFamily = trimmed;
    if (sizePt > 0)
    {
        globalUiSettings_.fontSizePt = clampFontPointSize(sizePt, Widget::kDefaultUiFontPt);
    }
    else if (globalUiSettings_.fontSizePt <= 0)
    {
        globalUiSettings_.fontSizePt = Widget::kDefaultUiFontPt;
    }

    refreshApplicationStyles();
    syncGlobalSettingsPanelControls();
    if (persist) auto_save_user();
}

void Widget::applyOutputFont(const QString &family, int sizePt, bool persist)
{
    const QString trimmed = family.trimmed();
    if (!trimmed.isEmpty())
    {
        globalUiSettings_.outputFontFamily = trimmed;
    }
    else
    {
        globalUiSettings_.outputFontFamily.clear();
    }

    if (sizePt > 0)
    {
        const int fallback = outputFontFallbackSizePt_ > 0 ? outputFontFallbackSizePt_ : Widget::kDefaultOutputFontPt;
        globalUiSettings_.outputFontSizePt = clampFontPointSize(sizePt, fallback);
    }
    else if (globalUiSettings_.outputFontSizePt <= 0)
    {
        globalUiSettings_.outputFontSizePt = outputFontFallbackSizePt_ > 0 ? outputFontFallbackSizePt_ : Widget::kDefaultOutputFontPt;
    }

    refreshApplicationStyles();
    syncGlobalSettingsPanelControls();
    if (persist) auto_save_user();
}

QString Widget::buildThemeOverlay(const QString &themeId) const
{
    if (themeId.isEmpty() || themeId == QStringLiteral("unit01")) return QString();
    static const QHash<QString, QString> themePaths = {
        {QStringLiteral("unit00"), QStringLiteral(":/QSS/theme_unit00.qss")},
        {QStringLiteral("unit02"), QStringLiteral(":/QSS/theme_unit02.qss")},
        {QStringLiteral("unit03"), QStringLiteral(":/QSS/theme_unit03.qss")} };
    const QString path = themePaths.value(themeId);
    if (path.isEmpty()) return QString();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    const QByteArray data = file.readAll();
    return QString::fromUtf8(data);
}

QString Widget::buildFontOverrideCss() const
{
    const QString family = globalUiSettings_.fontFamily.trimmed();
    const bool hasCustomFamily = !family.isEmpty();
    const bool hasCustomSize = globalUiSettings_.fontSizePt > 0;
    if (!hasCustomFamily && !hasCustomSize) return QString();

    const QString effectiveFamily = hasCustomFamily ? family : QApplication::font().family();
    QString escapedFamily = effectiveFamily;
    escapedFamily.replace('"', "\\\"");
    const int effectiveSize = hasCustomSize
                                  ? clampFontPointSize(globalUiSettings_.fontSizePt, Widget::kDefaultUiFontPt)
                                  : clampFontPointSize(QApplication::font().pointSize(), Widget::kDefaultUiFontPt);

    return QStringLiteral(
               "QWidget, QToolTip, QMenu, QComboBox, QComboBox QAbstractItemView,\n"
               "QComboBox QListView, QListView, QTreeView, QTableView, QTextEdit,\n"
               "QPlainTextEdit, QLineEdit, QLabel, QPushButton, QRadioButton,\n"
               "QCheckBox, QTabWidget, QTabBar::tab {\n"
               "  font-family: \"%1\";\n"
               "  font-size: %2pt;\n"
               "}\n")
        .arg(escapedFamily, QString::number(effectiveSize));
}

QString Widget::buildOutputFontCss() const
{
    const QString family = resolvedOutputFontFamily();
    const int sizePt = resolvedOutputFontSize();
    if (family.isEmpty() || sizePt <= 0) return QString();

    QString escapedFamily = family;
    escapedFamily.replace('"', "\\\"");
    return QStringLiteral(
               "QPlainTextEdit#output, QTextEdit#output {\n"
               "  font-family: \"%1\";\n"
               "  font-size: %2pt;\n"
               "}\n")
        .arg(escapedFamily, QString::number(sizePt));
}

void Widget::refreshApplicationStyles()
{
    QFont target = QApplication::font();
    if (!globalUiSettings_.fontFamily.trimmed().isEmpty())
    {
        target.setFamily(globalUiSettings_.fontFamily);
    }
    const int uiPointSize = clampFontPointSize(globalUiSettings_.fontSizePt, Widget::kDefaultUiFontPt);
    target.setPointSize(uiPointSize);
    QApplication::setFont(target);

    QString composed = baseStylesheet_;
    const QString overlay = buildThemeOverlay(globalUiSettings_.themeId);
    if (!overlay.isEmpty())
    {
        if (!composed.isEmpty()) composed.append('\n');
        composed.append(overlay);
    }
    const QString fontCss = buildFontOverrideCss();
    if (!fontCss.isEmpty())
    {
        if (!composed.isEmpty()) composed.append('\n');
        composed.append(fontCss);
    }
    const QString outputCss = buildOutputFontCss();
    if (!outputCss.isEmpty())
    {
        if (!composed.isEmpty()) composed.append('\n');
        composed.append(outputCss);
    }
    if (qApp) qApp->setStyleSheet(composed);
    refreshOutputFont();
    updateThemeVisuals();
}

void Widget::updateThemeVisuals()
{
    ThemeVisuals v;
    const QString id = globalUiSettings_.themeId.isEmpty() ? QStringLiteral("unit01") : globalUiSettings_.themeId;
    v.id = id;
    if (id == QLatin1String("unit02"))
    {
        v.darkBase = true;
        v.textPrimary = QColor("#ffe3d9");
        v.textSecondary = QColor("#ffbca7");
        v.stateSignal = QColor("#8dbdff");
        v.stateSuccess = QColor("#86ffb1");
        v.stateWrong = QColor("#ff9a9a");
        v.stateEva = QColor("#ffc6ff");
        v.stateTool = QColor("#7fd8ff");
        v.stateSync = QColor("#ffc67c");
        v.systemRole = v.stateSignal;
        v.assistantRole = v.stateSync;
    }
    else if (id == QLatin1String("unit03"))
    {
        v.darkBase = true;
        v.textPrimary = QColor("#e9edff");
        v.textSecondary = QColor("#b9c3ff");
        v.stateSignal = QColor("#9bb4ff");
        v.stateSuccess = QColor("#8dffd2");
        v.stateWrong = QColor("#ff9fc0");
        v.stateEva = QColor("#d8bdff");
        v.stateTool = QColor("#84ddff");
        v.stateSync = QColor("#ffd185");
        v.systemRole = v.stateSignal;
        v.assistantRole = v.stateSync;
    }
    else if (id == QLatin1String("unit00"))
    {
        v.darkBase = false;
        v.textPrimary = NORMAL_BLACK;
        v.textSecondary = THINK_GRAY;
        v.stateSignal = SYSTEM_BLUE;
        v.stateSuccess = QColor(0, 200, 0);
        v.stateWrong = QColor(200, 0, 0);
        v.stateEva = SYSTEM_BLUE;
        v.stateTool = TOOL_BLUE;
        v.stateSync = LCL_ORANGE;
        v.systemRole = SYSTEM_BLUE;
        v.assistantRole = LCL_ORANGE;
    }
    else
    {
        v.darkBase = false;
        v.textPrimary = NORMAL_BLACK;
        v.textSecondary = THINK_GRAY;
        v.stateSignal = SYSTEM_BLUE;
        v.stateSuccess = QColor(0, 200, 0);
        v.stateWrong = QColor(200, 0, 0);
        v.stateEva = SYSTEM_BLUE;
        v.stateTool = TOOL_BLUE;
        v.stateSync = LCL_ORANGE;
        v.systemRole = SYSTEM_BLUE;
        v.assistantRole = LCL_ORANGE;
    }
    themeVisuals_ = v;
}

void Widget::refreshOutputFont()
{
    if (!ui || !ui->output) return;
    const QFont font = currentOutputFont();
    if (QTextDocument *doc = ui->output->document())
    {
        doc->setDefaultFont(font);
    }
    ui->output->setFont(font);
}

QString Widget::resolvedOutputFontFamily() const
{
    if (!globalUiSettings_.outputFontFamily.trimmed().isEmpty())
    {
        return globalUiSettings_.outputFontFamily.trimmed();
    }
    if (!outputFontFallbackFamily_.trimmed().isEmpty())
    {
        return outputFontFallbackFamily_;
    }
    return QApplication::font().family();
}

int Widget::resolvedOutputFontSize() const
{
    if (globalUiSettings_.outputFontSizePt > 0)
    {
        return globalUiSettings_.outputFontSizePt;
    }
    if (outputFontFallbackSizePt_ > 0)
    {
        return outputFontFallbackSizePt_;
    }
    return Widget::kDefaultOutputFontPt;
}

QFont Widget::currentOutputFont() const
{
    QFont font = ui && ui->output ? ui->output->font() : QApplication::font();
    font.setFamily(resolvedOutputFontFamily());
    const int size = resolvedOutputFontSize();
    if (size > 0)
    {
        font.setPointSize(size);
    }
    else if (font.pointSize() <= 0)
    {
        font.setPointSize(Widget::kDefaultOutputFontPt);
    }
    return font;
}

void Widget::loadOutputFontFromResource()
{
    if (!ui || !ui->output) return;
    auto tryRegister = [&]() -> bool
    {
        const int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/SarasaFixedCL-Regular.ttf"));
        if (fontId >= 0)
        {
            const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
            if (!families.isEmpty())
            {
                outputFontFallbackFamily_ = families.first();
                outputFontResourceLoaded_ = true;
                return true;
            }
        }
        return false;
    };
    if (!outputFontResourceLoaded_)
    {
        if (!tryRegister())
        {
            // Windows-specific fallback removed; qrc big resource should always be available
        }
    }
    // 兜底使用 SarasaFixedCL-Regular 作为输出区默认字体，避免回退到系统宋体或其他不一致字体
    if (outputFontFallbackFamily_.isEmpty())
    {
        outputFontFallbackFamily_ = QStringLiteral("Sarasa Fixed CL");
    }
    outputFontFallbackFamily_ = resolveSarasaFallback(outputFontFallbackFamily_);
    if (outputFontFallbackSizePt_ <= 0)
    {
        // Force a predictable default for the output pane regardless of template font sizes.
        outputFontFallbackSizePt_ = Widget::kDefaultOutputFontPt;
    }
    if (globalUiSettings_.outputFontFamily.isEmpty())
    {
        globalUiSettings_.outputFontFamily = outputFontFallbackFamily_;
    }
    if (globalUiSettings_.outputFontSizePt <= 0)
    {
        globalUiSettings_.outputFontSizePt = outputFontFallbackSizePt_;
    }
}

QColor Widget::themeStateColor(SIGNAL_STATE state) const
{
    switch (state)
    {
    case SIGNAL_SIGNAL: return themeVisuals_.stateSignal;
    case SUCCESS_SIGNAL: return themeVisuals_.stateSuccess;
    case WRONG_SIGNAL: return themeVisuals_.stateWrong;
    case EVA_SIGNAL: return themeVisuals_.stateEva;
    case TOOL_SIGNAL: return themeVisuals_.stateTool;
    case SYNC_SIGNAL: return themeVisuals_.stateSync;
    case MATRIX_SIGNAL:
    case USUAL_SIGNAL:
    default: return themeVisuals_.textPrimary;
    }
}

QColor Widget::chipColorForRole(RecordRole r) const
{
    switch (r)
    {
    case RecordRole::Tool: return themeVisuals_.stateTool;
    case RecordRole::Think: return themeVisuals_.textSecondary;
    case RecordRole::Assistant: return themeVisuals_.assistantRole;
    case RecordRole::User:
    case RecordRole::System:
    default: return themeVisuals_.systemRole;
    }
}

QColor Widget::textColorForRole(RecordRole r) const
{
    switch (r)
    {
    case RecordRole::Think: return themeVisuals_.textSecondary;
    case RecordRole::Tool: return themeVisuals_.stateTool;
    default: return themeVisuals_.textPrimary;
    }
}

void Widget::applyGlobalTheme(const QString &themeId, bool persist)
{
    QString effective = themeId;
    if (effective.isEmpty()) effective = QStringLiteral("unit01");
    if (effective != QStringLiteral("unit01") && effective != QStringLiteral("unit00") &&
        effective != QStringLiteral("unit02") && effective != QStringLiteral("unit03"))
    {
        effective = QStringLiteral("unit01");
    }

    globalUiSettings_.themeId = effective;
    refreshApplicationStyles();
    syncGlobalSettingsPanelControls();
    if (persist) auto_save_user();
}

void Widget::setBaseStylesheet(const QString &style)
{
    baseStylesheet_ = style;
    refreshApplicationStyles();
}

void Widget::loadGlobalUiSettings(const QSettings &settings)
{
    const QString family = settings.value("global_font_family", globalUiSettings_.fontFamily).toString();
    const int sizePt = settings.value("global_font_size", globalUiSettings_.fontSizePt).toInt();
    QString outputFamily = settings.value("output_font_family", globalUiSettings_.outputFontFamily).toString();
    const int outputSize = settings.value("output_font_size", globalUiSettings_.outputFontSizePt).toInt();
    const QString themeId = settings.value("global_theme", globalUiSettings_.themeId).toString();

    applyGlobalFont(family, sizePt > 0 ? sizePt : globalUiSettings_.fontSizePt, false);
    const QString fallbackFamily = resolveSarasaFallback(outputFontFallbackFamily_);
    // 若未配置输出区字体，则默认使用 Sarasa；用户已有选择时尊重配置
    if (outputFamily.trimmed().isEmpty())
    {
        outputFamily = fallbackFamily;
    }
    applyOutputFont(outputFamily, outputSize > 0 ? outputSize : globalUiSettings_.outputFontSizePt, false);
    refreshOutputFont(); // 确保输出区应用最新字体（默认 Sarasa，或用户自选）
    applyGlobalTheme(themeId, false);
}

void Widget::applySettingsDialogSizing()
{
    if (!settings_dialog) return;
    if (QLayout *layout = settings_dialog->layout())
    {
        layout->invalidate();
        layout->activate();
    }
    const QSize hint = settings_dialog->minimumSizeHint();
    settings_dialog->setMinimumSize(hint);
    settings_dialog->resize(hint);
}

// 设置选项卡确认按钮响应
void Widget::settings_ui_confirm_button_clicked()
{
    // 先读取对话框中的值，与打开对话框时的快照对比；完全一致则不做任何动作
    // 注意：get_set() 会把控件值写入 ui_SETTINGS 与 ui_port
    get_set();
    const bool requestedHostAllow = (settings_ui && settings_ui->allow_control_checkbox) ? settings_ui->allow_control_checkbox->isChecked() : controlHostAllowed_;
    const bool hostPermissionChanged = (requestedHostAllow != controlHostAllowed_);
    // Inform Expend (evaluation tab) of latest settings snapshot
    {
        SETTINGS snap = ui_SETTINGS;
        if (ui_mode == LINK_MODE)
            snap.nctx = (slotCtxMax_ > 0 ? slotCtxMax_ : 0);
        emit ui2expend_settings(snap);
    }

    auto eq_str = [](const QString &a, const QString &b)
    { return a == b; };
    auto eq_ngl = [&](int a, int b)
    {
        // 视 999 与 (n_layer+1) 为等价（已知服务端最大层数时）
        if (a == b) return true;
        if (ui_maxngl > 0)
        {
            if ((a == 999 && b == ui_maxngl) || (b == 999 && a == ui_maxngl)) return true;
        }
        return false;
    };
    auto eq = [&](const SETTINGS &A, const SETTINGS &B)
    {
        // 影响 llama-server 启动参数的设置项（保持与 LocalServerManager::buildArgs 一致）
        if (A.modelpath != B.modelpath) return false;
        if (A.mmprojpath != B.mmprojpath) return false;
        if (A.lorapath != B.lorapath) return false;
        if (A.nctx != B.nctx) return false;
        if (!eq_ngl(A.ngl, B.ngl)) return false;
        if (A.nthread != B.nthread) return false;
        if (A.hid_batch != B.hid_batch) return false;
        if (A.hid_parallel != B.hid_parallel) return false;
        if (A.hid_use_mmap != B.hid_use_mmap) return false;
        if (A.hid_use_mlock != B.hid_use_mlock) return false;
        if (A.hid_flash_attn != B.hid_flash_attn) return false;
        // 推理设备（切换后需要重启后端）
        if (ui_device_backend != device_snapshot_) return false;
        // 其他仅影响采样/推理流程的设置项（不触发后端重启）也一并比较；若都未变，则完全不处理
        if (A.temp != B.temp) return false;
        if (A.repeat != B.repeat) return false;
        if (A.top_k != B.top_k) return false;
        if (A.hid_top_p != B.hid_top_p) return false;
        if (A.hid_npredict != B.hid_npredict) return false;
        if (A.hid_n_ubatch != B.hid_n_ubatch) return false;
        if (A.complete_mode != B.complete_mode) return false;
        if (A.reasoning_effort != B.reasoning_effort) return false;
        return true;
    };

    const bool sameSettings = eq(ui_SETTINGS, settings_snapshot_) && eq_str(ui_port, port_snapshot_) && !backendOverrideDirty_;
    const bool onlyHostChange = hostPermissionChanged && sameSettings;

    // 仅比较会触发后端重启的设置项
    auto eq_server = [&](const SETTINGS &A, const SETTINGS &B)
    {
        if (A.modelpath != B.modelpath) return false;
        if (A.mmprojpath != B.mmprojpath) return false;
        if (A.lorapath != B.lorapath) return false;
        if (A.nctx != B.nctx) return false;
        if (!eq_ngl(A.ngl, B.ngl)) return false;
        if (A.nthread != B.nthread) return false;
        if (A.hid_batch != B.hid_batch) return false;
        if (A.hid_parallel != B.hid_parallel) return false;
        if (A.hid_use_mmap != B.hid_use_mmap) return false;
        if (A.hid_use_mlock != B.hid_use_mlock) return false;
        if (A.hid_flash_attn != B.hid_flash_attn) return false;
        // 设备切换也需要重启后端
        if (ui_device_backend != device_snapshot_) return false;
        if (backendOverrideDirty_) return false;
        return true;
    };
    const bool sameServer = eq_server(ui_SETTINGS, settings_snapshot_) && eq_str(ui_port, port_snapshot_);

    settings_dialog->accept();
    auto finalizeOverrides = [this]()
    {
        if (backendOverrideDirty_ || !pendingBackendOverrides_.isEmpty())
        {
            commitPendingBackendOverrides();
        }
        else
        {
            backendOverrideSnapshot_ = DeviceManager::programOverrides();
        }
        pendingBackendOverrides_.clear();
        backendOverrideDirty_ = false;
        syncBackendOverrideState();
        refreshDeviceBackendUI();
    };
    auto finalizeAndPersist = [this, finalizeOverrides, hostPermissionChanged, requestedHostAllow]()
    {
        finalizeOverrides();
        if (hostPermissionChanged)
        {
            setControlHostEnabled(requestedHostAllow);
        }
        auto_save_user();
        enforceEngineerEnvReadyCheckpoint();
    };
    if (sameSettings && !hostPermissionChanged)
    {
        // 未发生任何变化：不重启、不重置
        finalizeAndPersist();
        return;
    }
    auto applyChanges = [this, sameServer, finalizeAndPersist, onlyHostChange]()
    {
        if (sameServer)
        {
            if (!onlyHostChange)
            {
                if (ui_mode == LOCAL_MODE || ui_mode == LINK_MODE)
                {
                    on_reset_clicked();
                }
            }
            finalizeAndPersist();
            return;
        }
        if (backendOverrideDirty_ || !pendingBackendOverrides_.isEmpty())
        {
            commitPendingBackendOverrides();
            backendOverrideSnapshot_ = DeviceManager::programOverrides();
        }
        if (ui_mode == LOCAL_MODE)
        {
            ensureLocalServer();
            if (!lastServerRestart_)
            {
                on_reset_clicked();
            }
        }
        else
        {
            on_reset_clicked();
        }
        finalizeAndPersist();
    };

    queueEngineerGateAction(applyChanges, ui_engineer_ischecked && ui_dockerSandboxEnabled);
}

// 设置选项卡取消按钮响应
void Widget::settings_ui_cancel_button_clicked()
{
    // Cancel should not mutate any in-memory settings or persist to disk.
    // Simply close the dialog and discard any slider edits.
    updateLazyCountdownLabel();
    pendingBackendOverrides_ = backendOverrideSnapshot_;
    backendOverrideDirty_ = false;
    QString restoredDevice = device_snapshot_;
    if (restoredDevice.isEmpty())
    {
        restoredDevice = DeviceManager::userChoice();
    }
    if (restoredDevice.isEmpty())
    {
        restoredDevice = ui_device_backend;
    }
    if (!restoredDevice.isEmpty())
    {
        ui_device_backend = restoredDevice;
        lastDeviceBeforeCustom_ = ui_device_backend;
        DeviceManager::setUserChoice(ui_device_backend);
        if (settings_ui && settings_ui->device_comboBox)
        {
            QSignalBlocker blocker(settings_ui->device_comboBox);
            int idx = settings_ui->device_comboBox->findText(restoredDevice, Qt::MatchFixedString);
            if (idx < 0 && restoredDevice == QStringLiteral("custom"))
            {
                settings_ui->device_comboBox->addItem(QStringLiteral("custom"));
                idx = settings_ui->device_comboBox->findText(QStringLiteral("custom"));
            }
            if (idx >= 0)
            {
                settings_ui->device_comboBox->setCurrentIndex(idx);
            }
        }
    }
    syncBackendOverrideState();
    refreshDeviceBackendUI();
}

// Centralized device/backend UI refresh implementation (see header for rules)
void Widget::refreshDeviceBackendUI()
{
    if (!settings_ui) return;
    QString sel = settings_ui->device_comboBox->currentText().trimmed().toLower();
    const QMap<QString, QString> overrides = currentOverrideMapForUi();
    const QString inferenceRole = QStringLiteral("llama-server-main");
    const bool hasInferenceOverride = overrides.contains(inferenceRole);
    if (hasInferenceOverride)
    {
        int customIdx = settings_ui->device_comboBox->findText(QStringLiteral("custom"));
        if (customIdx < 0)
        {
            settings_ui->device_comboBox->addItem(QStringLiteral("custom"));
            customIdx = settings_ui->device_comboBox->findText(QStringLiteral("custom"));
        }
        if (customIdx >= 0 && sel != QStringLiteral("custom"))
        {
            QSignalBlocker blocker(settings_ui->device_comboBox);
            settings_ui->device_comboBox->setCurrentIndex(customIdx);
            sel = QStringLiteral("custom");
        }
    }
    const QString eff = DeviceManager::effectiveBackendFor(sel);
    const bool cpuLike = (eff == QLatin1String("cpu") || eff == QLatin1String("cpu-noavx") || eff == QLatin1String("opencl"));
    settings_ui->ngl_slider->setEnabled(!cpuLike);
    // Keep base label text in sync with current language
    deviceLabelBaseText = jtr("device");
    QString runtime = runtimeDeviceBackend_;
    if (runtime.isEmpty())
    {
        runtime = DeviceManager::lastResolvedDeviceFor(QStringLiteral("llama-server-main"));
    }
    if (runtime.isEmpty())
    {
        runtime = eff;
    }
    if (runtime.isEmpty())
    {
        runtime = QStringLiteral("unknown");
    }
    QString labelText = deviceLabelBaseText;
    if (hasInferenceOverride)
    {
        labelText += QStringLiteral(" (custom)");
    }
    else if (sel == QLatin1String("auto"))
    {
        labelText += QStringLiteral(" (%1)").arg(runtime);
    }
    settings_ui->device_label->setText(labelText);
}

// 设置用户设置内容
void Widget::set_set()
{
    get_set(); // 获取设置中的纸面值

    // 如果不是对话模式则禁用约定
    if (ui_state != CHAT_STATE)
    {
        date_ui->prompt_box->setEnabled(0);
        date_ui->tool_box->setEnabled(0);
    }
    else
    {
        date_ui->prompt_box->setEnabled(1);
        date_ui->tool_box->setEnabled(1);
    }

    // 本地模式：按需重启 llama-server（内部会切至装载中并更新端点）；
    // 若无需重启（仅采样参数变化），则简单重置对话上下文。
    if (ui_mode == LOCAL_MODE)
    {
        ensureLocalServer();
        if (!lastServerRestart_)
        {
            on_reset_clicked();
        }
        // 若发生了重启，等待 onServerReady() 回调恢复 UI 和上下文
    }
    else if (ui_mode == LINK_MODE)
    {
        on_reset_clicked();
    }
    backendOverrideDirty_ = false;
}

int Widget::predictTokenCap() const
{
    int ctxLimit = 0;
    if (slotCtxMax_ > 0)
    {
        ctxLimit = slotCtxMax_;
    }
    else if (settings_ui && settings_ui->nctx_slider)
    {
        ctxLimit = settings_ui->nctx_slider->value();
    }
    else if (ui_SETTINGS.nctx > 0)
    {
        ctxLimit = ui_SETTINGS.nctx;
    }
    else
    {
        ctxLimit = DEFAULT_NCTX;
    }
    if (ctxLimit <= 0) ctxLimit = DEFAULT_NCTX;
    return ctxLimit;
}

void Widget::enforcePredictLimit(bool syncSpin, bool clampSettings)
{
    const int limit = qMax(1, predictTokenCap());
    if (clampSettings)
    {
        if (ui_SETTINGS.hid_npredict < -1)
            ui_SETTINGS.hid_npredict = -1;
        else if (ui_SETTINGS.hid_npredict > limit)
            ui_SETTINGS.hid_npredict = limit;
    }
    if (settings_ui && settings_ui->npredict_spin)
    {
        const QSignalBlocker blocker(settings_ui->npredict_spin);
        settings_ui->npredict_spin->setRange(-1, limit);
        if (syncSpin)
        {
            const int target = clampSettings ? qBound(-1, ui_SETTINGS.hid_npredict, limit)
                                              : qBound(-1, settings_ui->npredict_spin->value(), limit);
            settings_ui->npredict_spin->setValue(target);
        }
    }
    if (settings_ui && settings_ui->npredict_spin)
    {
        npredict_change();
    }
}
