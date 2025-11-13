#include "widget.h"
#include "ui_widget.h"
#include "toolcall_test_dialog.h"
#include <QImage>

void Widget::onShortcutActivated_F1()
{
    createTempDirectory("./EVA_TEMP");
    cutscreen_dialog->showFullScreen(); // 处理截图事件
}

void Widget::onShortcutActivated_F2()
{
    if (whisper_model_path == "") // 如果还未指定模型路径则先指定
    {
        emit ui2expend_show(WHISPER_WINDOW); // 语音增殖界面
    }
    else if (!is_recodering)
    {
        recordAudio(); // 开始录音
        is_recodering = true;
    }
    else if (is_recodering)
    {
        stop_recordAudio(); // 停止录音
    }
}

void Widget::onShortcutActivated_F3()
{
    ToolCallTestDialog *dialog = ensureToolCallTestDialog();
    if (!dialog) return;
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    dialog->focusInput();
}

void Widget::onShortcutActivated_CTRL_ENTER()
{
    ui->send->click();
}

void Widget::recv_qimagepath(QString cut_imagepath_)
{
    reflash_state("ui:" + jtr("cut image success"), USUAL_SIGNAL);
    ui->input->addFileThumbnail(cut_imagepath_);
}
