#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//-------------------------------响应槽相关---------------------------------
//-------------------------------------------------------------------------

//温度滑块响应
void Widget::temp_change()
{
    settings_ui->temp_label->setText(jtr("temperature") + " " + QString::number(settings_ui->temp_slider->value() / 100.0));
}
// ngl滑块响应
void Widget::ngl_change()
{
    settings_ui->ngl_label->setText("gpu " + jtr("offload") + " " + QString::number(settings_ui->ngl_slider->value()));
}
// nctx滑块响应
void Widget::nctx_change()
{
    settings_ui->nctx_label->setText(jtr("brain size") + " " + QString::number(settings_ui->nctx_slider->value()));
}
// repeat滑块响应
void Widget::repeat_change()
{
    settings_ui->repeat_label->setText(jtr("repeat") + " " + QString::number(settings_ui->repeat_slider->value() / 100.0));
}

void Widget::nthread_change()
{
    settings_ui->nthread_label->setText("cpu " + jtr("thread") + " " + QString::number(settings_ui->nthread_slider->value()));
}

//补完状态按钮响应
void Widget::complete_change()
{
    //选中则禁止约定输入
    if (settings_ui->complete_btn->isChecked())
    {
        settings_ui->sample_box->setEnabled(1);

        settings_ui->nthread_slider->setEnabled(1);
        settings_ui->nctx_slider->setEnabled(1);
        // 端口设置始终可用（服务状态已移除，本地后端自动启动）
        settings_ui->port_lineEdit->setEnabled(1);
    }
}

//对话状态按钮响应
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

//服务状态按钮响应
void Widget::web_change()
{
    // 服务状态已移除
}

//提示词模板下拉框响应
void Widget::prompt_template_change()
{
    if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set1"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);
        date_ui->user_name_LineEdit->setEnabled(1);
        date_ui->model_name_LineEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom1_date_system);
        date_ui->user_name_LineEdit->setText(custom1_user_name);
        date_ui->model_name_LineEdit->setText(custom1_model_name);
    }
    else if (date_ui->chattemplate_comboBox->currentText() == jtr("custom set2"))
    {
        date_ui->date_prompt_TextEdit->setEnabled(1);
        date_ui->user_name_LineEdit->setEnabled(1);
        date_ui->model_name_LineEdit->setEnabled(1);

        date_ui->date_prompt_TextEdit->setPlainText(custom2_date_system);
        date_ui->user_name_LineEdit->setText(custom2_user_name);
        date_ui->model_name_LineEdit->setText(custom2_model_name);
    }
    else
    {
        date_ui->date_prompt_TextEdit->setPlainText(date_map[date_ui->chattemplate_comboBox->currentText()].date_prompt);
        date_ui->date_prompt_TextEdit->setEnabled(0);
        date_ui->user_name_LineEdit->setText(date_map[date_ui->chattemplate_comboBox->currentText()].user_name);
        date_ui->user_name_LineEdit->setEnabled(0);
        date_ui->model_name_LineEdit->setText(date_map[date_ui->chattemplate_comboBox->currentText()].model_name);
        date_ui->model_name_LineEdit->setEnabled(0);
    }
}

void Widget::chooseLorapath()
{
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose lora model"), "(*.bin *.gguf)");

    settings_ui->lora_LineEdit->setText(currentpath);
}

void Widget::chooseMmprojpath()
{
    //用户选择模型位置
    currentpath = customOpenfile(currentpath, jtr("choose mmproj model"), "(*.bin *.gguf)");

    settings_ui->mmproj_LineEdit->setText(currentpath);
}

//响应工具选择
void Widget::tool_change()
{
    QObject *senderObj = sender(); // gets the object that sent the signal

    // 如果是软件工程师则查询python环境
    if (QCheckBox *checkbox = qobject_cast<QCheckBox *>(senderObj))
    {
        if (checkbox == date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked())
        {
            python_env = checkPython();
            compile_env = checkCompile();
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
    ui_extra_prompt = create_extra_prompt();
}
