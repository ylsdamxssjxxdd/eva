// sd_params_dialog.h - Popup settings window for Stable Diffusion family
// Lightweight QWidget (dialog-styled) to configure model paths and advanced
// generation/back-end options for Flux/Qwen-Image/SD1.x/etc.

#pragma once

#include <QWidget>
#include <QDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QFileDialog>

#include "../xconfig.h"

class SdParamsDialog : public QWidget
{
    Q_OBJECT
  public:
    explicit SdParamsDialog(QWidget *parent = nullptr);

    void setConfig(const SDRunConfig &cfg);
    SDRunConfig config() const;

    // Apply a preset by name: "flux1-dev", "qwen-image", "sd1.5-anything-3",
    // "custom1", "custom2"
    void applyPreset(const QString &name);

  signals:
    void accepted(const SDRunConfig &cfg, const QString &presetName);

  private slots:
    void onBrowse(QLineEdit *le, const QString &filter = QString());
    void onBrowseDir(QLineEdit *le);
    void onApplyClicked();
    void onPresetChanged(int idx);

  private:
    // Optional per-preset prompt store injected by caller (Expend)
    QMap<QString, QString> presetModify_;
    QMap<QString, QString> presetNegative_;
  public:
    void setPresetPromptStore(const QMap<QString, QString> &modifyMap,
                              const QMap<QString, QString> &negativeMap)
    {
        presetModify_ = modifyMap;
        presetNegative_ = negativeMap;
    }
  private:
    // UI helpers
    QLineEdit *addPathRow(QFormLayout *form, const QString &label, const QString &filter = QString(), bool directory = false);
    void buildUi();

    // Widgets
    QComboBox *presetBox_ = nullptr;
    // Model paths
    QComboBox *modelArgBox_ = nullptr; // Auto / -m / --diffusion-model
    QLineEdit *modelPathLe_ = nullptr;
    QLineEdit *vaeLe_ = nullptr;
    QLineEdit *clipLLe_ = nullptr;
    QLineEdit *clipGLe_ = nullptr;
    QLineEdit *clipVisionLe_ = nullptr;
    QLineEdit *t5Le_ = nullptr;
    QLineEdit *qwen2vlLe_ = nullptr;
    QLineEdit *loraDirLe_ = nullptr;
    QLineEdit *taesdLe_ = nullptr;
    QLineEdit *upscaleLe_ = nullptr;
    QLineEdit *controlNetLe_ = nullptr;
    QLineEdit *controlImgLe_ = nullptr;

    // Generation
    QSpinBox *wSpin_ = nullptr;
    QSpinBox *hSpin_ = nullptr;
    QComboBox *samplerBox_ = nullptr;
    QComboBox *schedulerBox_ = nullptr;
    QSpinBox *stepsSpin_ = nullptr;
    QDoubleSpinBox *cfgSpin_ = nullptr;
    QSpinBox *clipSkipSpin_ = nullptr;
    QSpinBox *batchSpin_ = nullptr;
    QSpinBox *seedSpin_ = nullptr;
    QDoubleSpinBox *strengthSpin_ = nullptr;
    QDoubleSpinBox *guidanceSpin_ = nullptr;
    QComboBox *rngBox_ = nullptr;
    // Flow/DiT
    QCheckBox *flowShiftEnable_ = nullptr;
    QDoubleSpinBox *flowShiftSpin_ = nullptr;

    // Prompts (Modify + Negative only)
    QGroupBox *grpPrompts_ = nullptr;
    QLineEdit *modifyEdit_ = nullptr;
    QPlainTextEdit *negativeEdit_ = nullptr;

    // Backend toggles
    QCheckBox *offloadCpuCb_ = nullptr;
    QCheckBox *clipCpuCb_ = nullptr;
    QCheckBox *vaeCpuCb_ = nullptr;
    QCheckBox *controlCpuCb_ = nullptr;
    QCheckBox *diffFaCb_ = nullptr;

    // VAE tiling
    QCheckBox *vaeTilingCb_ = nullptr;
    QSpinBox *vaeTileX_ = nullptr;
    QSpinBox *vaeTileY_ = nullptr;
    QDoubleSpinBox *vaeTileOverlap_ = nullptr;
};
