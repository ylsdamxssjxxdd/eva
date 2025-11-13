#include "ui_widget.h"
#include "widget.h"
#include "toolcall_test_dialog.h"
#include <QElapsedTimer>
#include <QHostAddress>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTcpServer>
#include <QUrl>
#include <QTextDocument>
#include <QFont>
#include "../utils/startuplogger.h"

//-------------------------------------------------------------------------
//-------------------------------响应槽相关---------------------------------
//-------------------------------------------------------------------------


// 事件过滤器,鼠标跟踪效果不好要在各种控件单独实现
bool Widget::eventFilter(QObject *obj, QEvent *event)
{
    // 响应已安装控件上的鼠标右击事件
    if (obj == ui->input && event->type() == QEvent::ContextMenu && ui_state == CHAT_STATE)
    {
        QContextMenuEvent *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        // 显示菜单
        right_menu->exec(contextMenuEvent->globalPos());
        return true;
    }
    // 响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->lora_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseLorapath();
        return true;
    }
    // 响应已安装控件上的鼠标右击事件
    if (obj == settings_ui->mmproj_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseMmprojpath();
        return true;
    }
    // 取消通过右击装载按钮进入链接模式的逻辑（改为点击装载后弹模式选择）
    // 响应已安装控件上的鼠标右击事件
    if (obj == api_endpoint_LineEdit && event->type() == QEvent::ContextMenu)
    {
        QString api_endpoint = "http://" + getFirstNonLoopbackIPv4Address() + ":8080";
        api_endpoint_LineEdit->setText(api_endpoint);
        return true;
    }
    // 响应已安装控件上的鼠标右击事件
    if (obj == ui->state && event->type() == QEvent::ContextMenu)
    {
        emit ui2expend_show(PREV_WINDOW); // 1是模型信息页
        return true;
    }

    return QObject::eventFilter(obj, event);
}

// 传递模型预解码的内容
void Widget::recv_predecode(QString bot_predecode_content_)
{
    bot_predecode_content = bot_predecode_content_;
}

// 接收whisper解码后的结果
void Widget::recv_speechdecode_over(QString result)
{
    ui_state_normal();
    ui->input->textEdit->append(result);
    // ui->send->click();//尝试一次发送
}

// 接收模型路径
void Widget::recv_whisper_modelpath(QString modelpath)
{
    whisper_model_path = modelpath;
}