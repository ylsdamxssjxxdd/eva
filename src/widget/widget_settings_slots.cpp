#include "widget.h"
#include "ui_widget.h"

void Widget::temp_change()
{
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0));
}

void Widget::ngl_change()
{
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(settings_ui->ngl_slider->value()));
}

void Widget::nctx_change()
{
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(settings_ui->nctx_slider->value()));
    enforcePredictLimit(true, false);
}

void Widget::repeat_change()
{
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0));
}

void Widget::topk_change()
{
    settings_ui->topk_label->setText(jtr("top_k") + " " + QString::number(settings_ui->topk_slider->value()));
}

void Widget::topp_change()
{
    const double val = settings_ui->topp_slider->value() / 100.0;
    settings_ui->topp_label->setText("TOP_P " + QString::number(val));
    settings_ui->topp_label->setToolTip(QString::fromUtf8("核采样阈值（top_p），范围 0.00–1.00；当前：%1")
                                            .arg(QString::number(val, 'f', 2)));
}

void Widget::npredict_change()
{
    if (!settings_ui || !settings_ui->npredict_spin || !settings_ui->npredict_label) return;
    const int value = settings_ui->npredict_spin->value();
    const int cap = predictTokenCap();
    Q_UNUSED(cap);
    const QString text = jtr("npredict") + " " + QString::number(value);
    settings_ui->npredict_label->setText(text);
    settings_ui->npredict_label->setToolTip(text);
    settings_ui->npredict_spin->setToolTip(text);
}

void Widget::parallel_change()
{
    settings_ui->parallel_label->setText(jtr("parallel") + " " + QString::number(settings_ui->parallel_slider->value()));
}

void Widget::nthread_change()
{
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(settings_ui->nthread_slider->value()));
}

void Widget::complete_change()
{
    // 选中则禁止约定输入
    if (settings_ui->complete_btn->isChecked())
    {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nthread_slider->setEnabled(1);
        settings_ui->nctx_slider->setEnabled(1);
        // 端口设置始终可用（服务状态已移除，本地后端自动启动）
        settings_ui->port_lineEdit->setEnabled(1);
    }
}

void Widget::chat_change()
{
    if (settings_ui->chat_btn->isChecked())
    {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nctx_slider->setEnabled(1);
        settings_ui->nthread_slider->setEnabled(1);
        settings_ui->port_lineEdit->setEnabled(1);
    }
}

void Widget::web_change()
{
    // 服务状态已移除
}

void Widget::prompt_template_change()
{
    if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set1"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom1_date_system);
    }
    else if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set2"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom2_date_system);
    }
    else
    {
        date_ui->date_prompt_TextEdit->setPlainText(date_map[date_ui->chattemplate_comboBox->currentText()].date_prompt);
        date_ui->date_prompt_TextEdit->setEnabled(0);
    }
}

void Widget::chooseLorapath()
{
    // 用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose lora model"), "(*.bin *.gguf)");

    settings_ui->lora_LineEdit->setText(currentpath);
}

void Widget::chooseMmprojpath()
{
    // 用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose mmproj model"), "(*.bin *.gguf)");

    settings_ui->mmproj_LineEdit->setText(currentpath);
}

void Widget::tool_change()
{
    QObject *senderObj = sender(); // gets the object that sent the signal

    // 如果是系统工程师则查询python环境
    if (QCheckBox *checkbox = qobject_cast<QCheckBox *>(senderObj))
    {
        if (checkbox == date_ui->engineer_checkbox)
        {
            const bool wasEngineerActive = ui_engineer_ischecked;
            ui_engineer_ischecked = checkbox->isChecked();
            if (ui_engineer_ischecked && !wasEngineerActive)
            {
                markEngineerEnvDirty();
                markEngineerSandboxDirty();
                markEngineerWorkDirPending();
            }
            else if (!ui_engineer_ischecked && wasEngineerActive)
            {
                engineerEnvReady_ = true;
                engineerGateActive_ = false;
                engineerGateQueue_.clear();
                engineerDockerReady_ = true;
                if (engineerUiLockActive_) applyEngineerUiLock(false);
                markEngineerSandboxDirty();
            }
            if (checkbox->isChecked())
            {
                triggerEngineerEnvRefresh(true);
                refreshDockerImageList(true);
                // If a work dir was chosen previously, reuse it silently
                const QString fallback = QDir(applicationDirPath).filePath("EVA_WORK");
                const QString current = engineerWorkDir.isEmpty() ? fallback : engineerWorkDir;
                if (engineerWorkDir.isEmpty())
                {
                    // Prompt only when not set before (or path missing)
                    QString picked = QFileDialog::getExistingDirectory(this, jtr("choose work dir"), current,
                                                                       QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                    if (!picked.isEmpty())
                    {
                        setEngineerWorkDirSilently(picked);
                        markEngineerWorkDirPending();
                        markEngineerSandboxDirty();
                        // Persist immediately to avoid losing selection on crash
                        auto_save_user();
                    }
                    else // user canceled and not set previously -> stick to default silently
                    {
                        setEngineerWorkDirSilently(current);
                        markEngineerWorkDirPending();
                        markEngineerSandboxDirty();
                        auto_save_user();
                    }
                }
                else
                {
                    // Already determined; defer propagation until user confirms
                    markEngineerSandboxDirty();
                }

                // 显示“工程师工作目录”行（在约定对话框）并更新显示
                if (date_ui->date_engineer_workdir_LineEdit)
                {
                    date_ui->date_engineer_workdir_label->setVisible(true);
                    date_ui->date_engineer_workdir_LineEdit->setVisible(true);
                    date_ui->date_engineer_workdir_browse->setVisible(true);
                    date_ui->date_engineer_workdir_LineEdit->setText(engineerWorkDir);
                }
                if (date_ui->docker_target_comboBox) date_ui->docker_target_comboBox->setVisible(true);
                if (date_ui->docker_image_comboBox) date_ui->docker_image_comboBox->setVisible(true);
            }
            else
            {
                markEngineerSandboxDirty();
                if (date_ui->date_engineer_workdir_LineEdit)
                {
                    date_ui->date_engineer_workdir_label->setVisible(false);
                    date_ui->date_engineer_workdir_LineEdit->setVisible(false);
                    date_ui->date_engineer_workdir_browse->setVisible(false);
                }
                if (date_ui->docker_target_comboBox) date_ui->docker_target_comboBox->setVisible(false);
                if (date_ui->docker_image_comboBox) date_ui->docker_image_comboBox->setVisible(false);
                dockerImagesFetched_ = false;
                dockerContainersFetched_ = false;
                dockerContainerList_.clear();
                dockerContainerTooltips_.clear();
                if (engineerProxyOuterActive_)
                {
                    cancelEngineerProxy(QStringLiteral("系统工程师已关闭"));
                }
            }
            refreshWindowIcon();
            updateEngineerConsoleVisibility();
        }
    }

    // 判断是否挂载了工具
    if (date_ui->calculator_checkbox->isChecked() || date_ui->engineer_checkbox->isChecked() || date_ui->MCPtools_checkbox->isChecked() || date_ui->knowledge_checkbox->isChecked() || date_ui->controller_checkbox->isChecked() || date_ui->stablediffusion_checkbox->isChecked())
    {
        if (is_load_tool == false)
        {
            reflash_state("ui:" + jtr("enable output parser"), SIGNAL_SIGNAL);
        }
        is_load_tool = true;
    }
    else
    {
        if (is_load_tool == true)
        {
            reflash_state("ui:" + jtr("disable output parser"), SIGNAL_SIGNAL);
        }
        is_load_tool = false;
    }

    // 桌面控制器：仅当用户勾选该工具时，才显示“归一化设置”区域（放在沙盒设置上方）
    if (date_ui && date_ui->controller_norm_box && date_ui->controller_checkbox)
    {
        date_ui->controller_norm_box->setVisible(date_ui->controller_checkbox->isChecked());
    }
    ui_extra_prompt = create_extra_prompt();
}
