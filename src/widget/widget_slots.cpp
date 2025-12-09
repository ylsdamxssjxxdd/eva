#include "ui_widget.h"
#include "widget.h"
#include "toolcall_test_dialog.h"

#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QFont>
#include <QHostAddress>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTcpServer>
#include <QTextDocument>
#include <QUrl>

#include "../utils/startuplogger.h"

//-------------------------------------------------------------------------
//-------------------------------响应槽相关--------------------------------
//-------------------------------------------------------------------------

// 事件过滤器：统一处理常用的右键/聚焦行为，避免在每个控件单独实现
bool Widget::eventFilter(QObject *obj, QEvent *event)
{
    // 输入区右键菜单
    if (obj == ui->input && event->type() == QEvent::ContextMenu && ui_state == CHAT_STATE)
    {
        QContextMenuEvent *contextMenuEvent = static_cast<QContextMenuEvent *>(event);
        right_menu->exec(contextMenuEvent->globalPos());
        return true;
    }
    // Lora 路径右键快捷选择
    if (obj == settings_ui->lora_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseLorapath();
        return true;
    }
    // mmproj 路径右键快捷选择
    if (obj == settings_ui->mmproj_LineEdit && event->type() == QEvent::ContextMenu)
    {
        chooseMmprojpath();
        return true;
    }
    // API 端点右键自动填充本机地址
    if (obj == api_endpoint_LineEdit && event->type() == QEvent::ContextMenu)
    {
        QString api_endpoint = "http://" + getFirstNonLoopbackIPv4Address() + ":8080";
        api_endpoint_LineEdit->setText(api_endpoint);
        return true;
    }
    // API 模型输入框点击/聚焦时弹出模型候选；若无缓存则触发去抖拉取
    if (obj == api_model_LineEdit &&
        (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress))
    {
        if (apiModelCandidates_.isEmpty())
        {
            scheduleRemoteModelDiscovery();
        }
        if (apiModelCompleter_) apiModelCompleter_->complete();
        return false;
    }
    // 状态栏右键打开模型信息页
    if (obj == ui->state && event->type() == QEvent::ContextMenu)
    {
        emit ui2expend_show(PREV_WINDOW); // 1是模型信息页
        return true;
    }

    return QObject::eventFilter(obj, event);
}

// 传回模型预解码的内容
void Widget::recv_predecode(QString bot_predecode_content_)
{
    bot_predecode_content = bot_predecode_content_;
}

// 接收 whisper 解码完成的结果
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
