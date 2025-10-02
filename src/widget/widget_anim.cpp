#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//----------------------------------编码动画--------------------------------
//-------------------------------------------------------------------------

void Widget::decode_play()
{
    decode_pTimer->start(100); //延时多少ms后发出timeout()信号
}
void Widget::decode_move()
{
    int decode_LineNumber = ui->state->document()->blockCount() - 1; // 获取最后一行的行数
    //如果在新的行数上播放动画,则先删除上一次解码动画的残余部分
    if (currnet_LineNumber != decode_LineNumber)
    {
        QTextBlock currnet_block = ui->state->document()->findBlockByLineNumber(currnet_LineNumber); //取上次最后一行
        QTextCursor currnet_cursor(currnet_block);                                                   //取游标
        currnet_cursor.movePosition(QTextCursor::EndOfBlock);                                        //游标移动到末尾
        currnet_cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);                     //选中当前字符
        if (currnet_cursor.selectedText() == "\\" || currnet_cursor.selectedText() == "/" || currnet_cursor.selectedText() == "-" || currnet_cursor.selectedText() == "|")
        {
            currnet_cursor.removeSelectedText(); //删除选中字符
        }
        currnet_LineNumber = decode_LineNumber;
    }

    QTextBlock block = ui->state->document()->findBlockByLineNumber(decode_LineNumber); //取最后一行
    QTextCursor cursor(block);                                                          //取游标
    cursor.movePosition(QTextCursor::EndOfBlock);                                       //游标移动到末尾

    //四帧动画
    if (decode_action % 8 == 0)
    {
        cursor.insertText("|"); //插入字符
    }
    else if (decode_action % 8 == 2)
    {
        cursor.insertText("/"); //插入字符
    }
    else if (decode_action % 8 == 4)
    {
        cursor.insertText("-"); //插入字符
    }
    else if (decode_action % 8 == 6)
    {
        cursor.insertText("\\"); //插入字符
    }
    else
    {
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor); //选中当前字符
        if (cursor.selectedText() == "|" || cursor.selectedText() == "\\" || cursor.selectedText() == "/" || cursor.selectedText() == "-")
        {
            cursor.removeSelectedText(); //删除选中字符
        }
    }

    decode_action++;
}

void Widget::decode_handleTimeout()
{
    if (decode_pTimer->isActive())
    {
        decode_pTimer->stop();
    } //控制超时处理函数只会处理一次
    decode_move();
    decode_pTimer->start(100); //延时多少ms后发出timeout()信号
}