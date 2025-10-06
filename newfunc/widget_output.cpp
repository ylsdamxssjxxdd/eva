#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//------------------------------文字输出相关--------------------------------
//-------------------------------------------------------------------------
// output和state采用verticalScrollBar()控制滑动条,如果其在底部,有新内容加入将自动下滑,用户上滑后下滑效果取消

//更新输出区,is_while表示从流式输出的token
void Widget::reflash_output(const QString result, bool is_while, QColor color)
{
    output_scroll(result, color);
    if (is_while)
    {
        temp_assistant_history += result;
    }
}

//输出区滚动条事件响应
void Widget::output_scrollBarValueChanged(int value)
{
    //如果滑动条在最下面则自动滚动
    int maximumValue = ui->output->verticalScrollBar()->maximum();
    if (value == maximumValue)
    {
        is_stop_output_scroll = 0;
    }
    else
    {
        is_stop_output_scroll = 1;
    }
}

//向output末尾添加文本并滚动
void Widget::output_scroll(QString output, QColor color)
{
    QTextCursor cursor = ui->output->textCursor();
    QTextCharFormat textFormat;

    textFormat.setForeground(QBrush(color)); // 设置文本颜色
    cursor.movePosition(QTextCursor::End);   //光标移动到末尾
    cursor.mergeCharFormat(textFormat);      // 应用文本格式

    cursor.insertText(output); //输出

    QTextCharFormat textFormat0;           // 清空文本格式
    cursor.movePosition(QTextCursor::End); //光标移动到末尾
    cursor.mergeCharFormat(textFormat0);   // 应用文本格式

    if (!is_stop_output_scroll) //如果停止标签没有启用,则每次输出完自动滚动到最下面
    {
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum()); //滚动条滚动到最下面
    }
}

//更新状态区
void Widget::reflash_state(QString state_string, SIGNAL_STATE state)
{
    QTextCharFormat format; //设置特殊文本颜色
    // QFont font;//字体 设置了字体就不能缩放了
    // font.setPointSize(9);
    // format.setFont(font);
    //过滤回车和换行符
    if (state != MATRIX_SIGNAL)
    {
        state_string.replace("\n", "\\n");
        state_string.replace("\r", "\\r");
    }

    if (state == USUAL_SIGNAL || state == MATRIX_SIGNAL) //一般黑色
    {
        format.clearForeground();                //清除前景颜色
        format.setForeground(NORMAL_BLACK);      //还是黑色吧
        ui->state->setCurrentCharFormat(format); //设置光标格式
        ui->state->appendPlainText(state_string);
    }
    else if (state == SUCCESS_SIGNAL) //正常绿色
    {
        format.setForeground(QColor(0, 200, 0)); // 设置前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式

        ui->state->appendPlainText(state_string);
        format.clearForeground();                //清除前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
    }
    else if (state == WRONG_SIGNAL) //不正常红色
    {
        format.setForeground(QColor(200, 0, 0)); // 设置前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                //清除前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
    }
    else if (state == SIGNAL_SIGNAL) //信号蓝色
    {
        format.setForeground(QColor(0, 0, 200)); // 蓝色设置前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                //清除前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
    }
    else if (state == EVA_SIGNAL) //行为警告
    {
        QFont font = format.font();
        // font.setFamily(DEFAULT_FONT);
        // font.setLetterSpacing(QFont::AbsoluteSpacing, 0); // 设置字母间的绝对间距
        font.setPixelSize(14);
        format.setFont(font);
        format.setFontItalic(true); // 设置斜体
        // format.setForeground(QColor(128,0,128));    // 紫色设置前景颜色
        format.setForeground(NORMAL_BLACK);      //还是黑色吧
        ui->state->setCurrentCharFormat(format); //设置光标格式
        //■■■■■■■■■■■■■■
        ui->state->appendPlainText(jtr("cubes")); //显示

        //中间内容
        format.setFontItalic(false);                             // 取消斜体
        format.setFontWeight(QFont::Black);                      // 设置粗体
        ui->state->setCurrentCharFormat(format);                 //设置光标格式
        ui->state->appendPlainText("          " + state_string); //显示

        //■■■■■■■■■■■■■■
        format.setFontItalic(true);               // 设置斜体
        format.setFontWeight(QFont::Normal);      // 取消粗体
        ui->state->setCurrentCharFormat(format);  //设置光标格式
        ui->state->appendPlainText(jtr("cubes")); //显示

        format.setFontWeight(QFont::Normal);     // 取消粗体
        format.setFontItalic(false);             // 取消斜体
        format.clearForeground();                //清除前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
    }
    else if (state == TOOL_SIGNAL) //工具天蓝色
    {
        format.setForeground(TOOL_BLUE);         //天蓝色设置前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                //清除前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
    }
    else if (state == SYNC_SIGNAL) //同步橘黄色
    {
        format.setForeground(LCL_ORANGE);        //天蓝色设置前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
        ui->state->appendPlainText(state_string);
        format.clearForeground();                //清除前景颜色
        ui->state->setCurrentCharFormat(format); //设置光标格式
    }
}
