#include "customplaintextedit.h"

customPlainTextEdit::customPlainTextEdit(QWidget *parent) : QPlainTextEdit(parent)
{
    // 设置鼠标追踪，以便能够捕获鼠标移动事件
    //setMouseTracking(true);
}

void customPlainTextEdit::mouseMoveEvent(QMouseEvent *event)
{
    // 获取鼠标的当前位置
    QTextCursor cursor = cursorForPosition(event->pos());

    // 判断是否位于包含 "加速支持" 的文本位置
    QString str = "surport";
    if (isInSpecialText(cursor, str))
    {
        // 显示提示条
        QToolTip::showText(event->globalPos(), device_tooltip, this);
    }
    else
    {
        // 鼠标不在特定文本位置，隐藏提示条
        QToolTip::hideText();
    }

    QPlainTextEdit::mouseMoveEvent(event);
}
//鼠标右击事件
void customPlainTextEdit::contextMenuEvent(QContextMenuEvent *event)
{
    emit createExpend();
}


bool customPlainTextEdit::isInSpecialText(QTextCursor &cursor, QString targetText)
{
    // 获取光标所在位置的文本
    QString currentText = cursor.block().text();

    // 判断文本是否包含目标文本
    if (currentText.contains(targetText))
    {
        return true;
    }

    return false;
}
