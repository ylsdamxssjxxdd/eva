#include "ui_widget.h"
#include "widget.h"

#include <QStyle>
#include <QVariant>

void Widget::applyInputVisualState(const QByteArray &state)
{
    if (!ui || !ui->input || !ui->input->textEdit) return;

    const auto applyState = [](QWidget *target, const QByteArray &value)
    {
        if (!target) return;
        const bool reset = value.isEmpty();
        const QVariant current = target->property("evaState");
        if (reset)
        {
            if (!current.isValid())
            {
                // Property already cleared; still refresh to ensure style picks up defaults.
            }
            target->setProperty("evaState", QVariant());
        }
        else
        {
            const QString desired = QString::fromUtf8(value);
            if (current.isValid() && current.toString() == desired)
            {
                // No change; still force polish to pick up potential style updates.
            }
            target->setProperty("evaState", desired);
        }

        if (QStyle *style = target->style())
        {
            style->unpolish(target);
            style->polish(target);
        }
        target->update();
    };

    applyState(ui->input, state);
    applyState(ui->input->textEdit, state);
}

//-------------------------------------------------------------------------
//----------------------------------界面状态--------------------------------
//-------------------------------------------------------------------------
// 按钮的可用和可视状态控制
// 最终要归到下面的几种状态来

// 初始界面状态
void Widget::ui_state_init()
{
    ui->load->setEnabled(1);  // 装载按钮
    ui->date->setEnabled(0);  // 约定按钮
    ui->set->setEnabled(0);   // 设置按钮
    ui->reset->setEnabled(0); // 重置按钮
    ui->send->setEnabled(0);  // 发送按钮
    ui->output->setReadOnly(1);
    applyInputVisualState(QByteArray());
}

// 装载中界面状态
void Widget::ui_state_loading()
{
    ui->send->setEnabled(0);         // 发送按钮
    ui->reset->setEnabled(0);        // 重置按钮
    ui->date->setEnabled(0);         // 约定按钮
    ui->set->setEnabled(0);          // 设置按钮
    ui->load->setEnabled(0);         // 装载按钮
    ui->input->textEdit->setFocus(); // 设置输入区为焦点
}

// 推理中界面状态
void Widget::ui_state_pushing()
{
    wait_play(); // 开启推理动画
    ui->load->setEnabled(0);
    ui->date->setEnabled(0);
    ui->set->setEnabled(0);
    ui->reset->setEnabled(1);
    ui->send->setEnabled(0);
}

// 服务模式已移除

// 待机界面状态
void Widget::ui_state_normal()
{
    if (is_run) // 如果是模型正在运行的状态的话
    {
        ui->reset->setEnabled(1);
        ui->input->textEdit->setEnabled(1);
        ui->input->textEdit->setReadOnly(0);
        ui->send->setEnabled(0);
        ui->input->textEdit->setPlaceholderText(jtr("chat or right click to choose question"));
        applyInputVisualState(QByteArray());
        return;
    }

    decode_pTimer->stop(); // 停止解码动画
    if (ui_state == CHAT_STATE)
    {
        ui->input->setVisible(1);
        ui->send->setVisible(1);

        ui->load->setEnabled(1);
        if (is_load || ui_mode == LINK_MODE)
        {
            ui->reset->setEnabled(1);
            ui->send->setEnabled(1);
        }
        if (is_load || ui_mode == LINK_MODE)
        {
            ui->date->setEnabled(1);
            ui->set->setEnabled(1);
        }
        ui->input->setVisible(1);
        ui->send->setVisible(1);

        ui->input->textEdit->setPlaceholderText(jtr("chat or right click to choose question"));
        applyInputVisualState(QByteArray());
        ui->input->textEdit->setReadOnly(0);
        ui->input->textEdit->setFocus(); // 设置输入区为焦点
        ui->send->setText(jtr("send"));

        ui->output->setReadOnly(1);
    }
    else if (ui_state == COMPLETE_STATE)
    {
        ui->load->setEnabled(1);

        if (is_load || ui_mode == LINK_MODE)
        {
            ui->reset->setEnabled(1);
            ui->send->setEnabled(1);
        }
        ui->date->setEnabled(1);
        ui->set->setEnabled(1);
        ui->input->setVisible(1);
        ui->send->setVisible(1);

        ui->input->textEdit->clear();
        ui->input->textEdit->setPlaceholderText(jtr("Please modify any text above"));
        applyInputVisualState(QByteArray("complete"));
        ui->input->textEdit->setReadOnly(1);
        ui->send->setText(jtr("complete"));

        ui->output->setReadOnly(0);
        ui->output->setFocus(); // 设置输出区为焦点
    }
    // 服务模式已移除
    if (ui_mode == LINK_MODE)
    {
        change_api_dialog(0);
    } // 链接模式不要解码设置
    else
    {
        change_api_dialog(1);
    }
}

// After backend exceptions or unexpected stops, ensure core controls are usable.
// Always allow "约定" and "设置" so user can adjust and recover.
void Widget::unlockButtonsAfterError()
{
    // Base normalization
    ui_state_normal();
    // Force-enable key controls regardless of current load flag
    if (ui && ui->load) ui->load->setEnabled(true);
    if (ui && ui->date) ui->date->setEnabled(true);
    if (ui && ui->set) ui->set->setEnabled(true);
    // Send/reset remain governed by whether a model/endpoint is active
}

// 录音界面状态
void Widget::ui_state_recoding()
{
    if (audio_time == 0)
    {
        ui->load->setEnabled(0);
        ui->date->setEnabled(0);
        ui->set->setEnabled(0);
        ui->reset->setEnabled(0);
        ui->send->setEnabled(0);
        ui->input->textEdit->setFocus();
        ui->input->textEdit->clear();
        applyInputVisualState(QByteArray("recording"));
        ui->input->textEdit->setReadOnly(1);
        ui->input->textEdit->setFocus(); // 设置输入区为焦点
        ui->input->textEdit->setPlaceholderText(jtr("recoding") + "... " + jtr("push f2 to stop"));
    }
    else
    {
        ui->input->textEdit->setPlaceholderText(jtr("recoding") + "... " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s " + jtr("push f2 to stop"));
    }
}
