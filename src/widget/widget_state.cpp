#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//----------------------------------界面状态--------------------------------
//-------------------------------------------------------------------------
//按钮的可用和可视状态控制
//最终要归到下面的几种状态来

//初始界面状态
void Widget::ui_state_init()
{
    ui->load->setEnabled(1);  //装载按钮
    ui->date->setEnabled(0);  //约定按钮
    ui->set->setEnabled(0);   //设置按钮
    ui->reset->setEnabled(0); //重置按钮
    ui->send->setEnabled(0);  //发送按钮
    ui->output->setReadOnly(1);
}

// 装载中界面状态
void Widget::ui_state_loading()
{
    ui->send->setEnabled(0);         //发送按钮
    ui->reset->setEnabled(0);        //重置按钮
    ui->date->setEnabled(0);         //约定按钮
    ui->set->setEnabled(0);          //设置按钮
    ui->load->setEnabled(0);         //装载按钮
    ui->input->textEdit->setFocus(); //设置输入区为焦点
}

//推理中界面状态
void Widget::ui_state_pushing()
{
    wait_play(); //开启推理动画
    ui->load->setEnabled(0);
    ui->date->setEnabled(0);
    ui->set->setEnabled(0);
    ui->reset->setEnabled(1);
    ui->send->setEnabled(0);
}

// 服务模式已移除

//待机界面状态
void Widget::ui_state_normal()
{
    if (is_run) //如果是模型正在运行的状态的话
    {
        ui->reset->setEnabled(1);
        ui->input->textEdit->setEnabled(1);
        ui->input->textEdit->setReadOnly(0);
        ui->send->setEnabled(0);
        ui->input->textEdit->setPlaceholderText(jtr("chat or right click to choose question"));
        ui->input->textEdit->setStyleSheet("background-color: white;");
        return;
    }

    decode_pTimer->stop(); //停止解码动画
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
        ui->input->textEdit->setStyleSheet("background-color: white;");
        ui->input->textEdit->setReadOnly(0);
        ui->input->textEdit->setFocus(); //设置输入区为焦点
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
        ui->input->textEdit->setStyleSheet("background-color: rgba(255, 165, 0, 127);"); //设置背景为橘黄色
        ui->input->textEdit->setReadOnly(1);
        ui->send->setText(jtr("complete"));

        ui->output->setReadOnly(0);
        ui->output->setFocus(); //设置输出区为焦点
    }
    // 服务模式已移除
    if (ui_mode == LINK_MODE)
    {
        change_api_dialog(0);
    } //链接模式不要解码设置
    else
    {
        change_api_dialog(1);
    }
}

//录音界面状态
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
        ui->input->textEdit->setStyleSheet("background-color: rgba(144, 238, 144, 127);"); //透明绿色
        ui->input->textEdit->setReadOnly(1);
        ui->input->textEdit->setFocus(); //设置输入区为焦点
        ui->input->textEdit->setPlaceholderText(jtr("recoding") + "... " + jtr("push f2 to stop"));
    }
    else
    {
        ui->input->textEdit->setPlaceholderText(jtr("recoding") + "... " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s " + jtr("push f2 to stop"));
    }
}
