#include "ui_widget.h"
#include "widget.h"

//-------------------------------------------------------------------------
//------------------------------输出--------------------------------
//-------------------------------------------------------------------------
// output 与 state 的 verticalScrollBar() 统一管理：如果位于底部，新增内容将自动滚动；用户上滚后不再自动滚动

// 更新输出，is_while 表示从流式输出的 token
void Widget::reflash_output(const QString result, bool is_while, QColor color)
{
    if (is_while)
    {
        // Robust segmented rendering: handle multiple <think>...</think> in a single chunk
        const QString begin = QString(DEFAULT_THINK_BEGIN);
        const QString tend = QString(DEFAULT_THINK_END);

        int pos = 0;
        const int n = result.size();
        while (pos < n)
        {
            if (turnThinkActive_)
            {
                // Inside think: find closing tag
                int endIdx = result.indexOf(tend, pos);
                const int until = (endIdx == -1) ? n : endIdx;
                QString thinkPart = result.mid(pos, until - pos);
                thinkPart.replace(begin, QString());
                thinkPart.replace(tend, QString());
                if (!turnThinkHeaderPrinted_)
                {
                    if (currentThinkIndex_ < 0) currentThinkIndex_ = recordCreate(RecordRole::Think);
                    appendRoleHeader(QStringLiteral("think"));
                    turnThinkHeaderPrinted_ = true;
                }
                if (!thinkPart.isEmpty()) output_scroll(thinkPart, THINK_GRAY);
                if (!thinkPart.isEmpty() && currentThinkIndex_ >= 0) recordAppendText(currentThinkIndex_, thinkPart);
                if (endIdx == -1)
                {
                    // Still open, consume all
                    break;
                }
                // Close think and continue after </think>
                turnThinkActive_ = false;
                pos = endIdx + tend.size();
                // Ensure assistant header after leaving think
                if (!turnAssistantHeaderPrinted_)
                {
                    if (currentAssistantIndex_ < 0) currentAssistantIndex_ = recordCreate(RecordRole::Assistant);
                    appendRoleHeader(QStringLiteral("assistant"));
                    turnAssistantHeaderPrinted_ = true;
                }
                continue;
            }
            else
            {
                // Outside think: find next <think>
                int beginIdx = result.indexOf(begin, pos);
                const int until = (beginIdx == -1) ? n : beginIdx;
                QString asstPart = result.mid(pos, until - pos);
                asstPart.replace(begin, QString());
                asstPart.replace(tend, QString());
                if (!asstPart.isEmpty())
                {
                    if (!turnAssistantHeaderPrinted_)
                    {
                        if (currentAssistantIndex_ < 0) currentAssistantIndex_ = recordCreate(RecordRole::Assistant);
                        appendRoleHeader(QStringLiteral("assistant"));
                        turnAssistantHeaderPrinted_ = true;
                    }
                    output_scroll(asstPart, NORMAL_BLACK);
                    if (currentAssistantIndex_ >= 0) recordAppendText(currentAssistantIndex_, asstPart);
                }
                if (beginIdx == -1)
                {
                    // No think begins; done
                    break;
                }
                // Enter think after printing assistant prefix
                if (!turnThinkHeaderPrinted_)
                {
                    if (currentThinkIndex_ < 0) currentThinkIndex_ = recordCreate(RecordRole::Think);
                    appendRoleHeader(QStringLiteral("think"));
                    turnThinkHeaderPrinted_ = true;
                }
                turnThinkActive_ = true;
                pos = beginIdx + begin.size();
                continue;
            }
        }

        // Keep accumulating raw stream (with markers) for final separation
        temp_assistant_history += result;
        return;
    }

    // Non-stream: sanitize and print with provided color
    QString out = result;
    out.replace(QString(DEFAULT_THINK_BEGIN), QString());
    out.replace(QString(DEFAULT_THINK_END), QString());
    output_scroll(out, color);
}

// Print a role header above content; color by role
void Widget::appendRoleHeader(const QString &role)
{
    // Ensure a blank line before header if output is not empty
    const bool emptyDoc = ui->output->document() && ui->output->document()->isEmpty();
    if (!emptyDoc)
    {
        output_scroll(QString(DEFAULT_SPLITER), NORMAL_BLACK);
    }
    QColor c = SYSTEM_BLUE;
    const QString r = role.trimmed().toLower();
    if (r == QStringLiteral("tool"))
        c = TOOL_BLUE;
    else if (r == QStringLiteral("think"))
        c = THINK_GRAY;
    else if (r == QStringLiteral("assistant"))
        c = LCL_ORANGE;
    // Insert role label and a newline
    output_scroll(r, c);
    output_scroll(QString(DEFAULT_SPLITER), NORMAL_BLACK);
}

// 输出区滚动条事件响应
void Widget::output_scrollBarValueChanged(int value)
{
    // 滚动条处于底部时自动滚动
    int maximumValue = output_scrollBar->maximum();
    if (value == maximumValue)
    {
        is_stop_output_scroll = 0;
    }
    else
    {
        is_stop_output_scroll = 1;
    }
}

// 在 output 末尾追加文本并着色
void Widget::output_scroll(QString output, QColor color)
{
    QTextCursor cursor = ui->output->textCursor();
    QTextCharFormat textFormat;

    textFormat.setForeground(QBrush(color)); // 设置文本颜色
    cursor.movePosition(QTextCursor::End);   // 光标移动到末尾
    cursor.mergeCharFormat(textFormat);      // 应用文本格式

    cursor.insertText(output); // 写入

    QTextCharFormat textFormat0;           // 恢复文本格式
    cursor.movePosition(QTextCursor::End); // 光标移动到末尾
    cursor.mergeCharFormat(textFormat0);   // 应用文本格式

    if (!is_stop_output_scroll) // 未手动停用自动滚动时每次追加自动滚动到底
    {
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum()); // 设置滚动条到最底端
    }
}

// 刷新状态区
void Widget::reflash_state(QString state_string, SIGNAL_STATE state)
{
    QTextCharFormat format; // 设置状态文本颜色
    // 过滤回车和换行
    if (state != MATRIX_SIGNAL)
    {
        state_string.replace("\n", "\\n");
        state_string.replace("\r", "\\r");
    }

    if (state == USUAL_SIGNAL || state == MATRIX_SIGNAL) // 普通黑色
    {
        format.clearForeground();
        format.setForeground(NORMAL_BLACK);
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(state_string);
    }
    else if (state == SUCCESS_SIGNAL) // 绿色
    {
        format.setForeground(QColor(0, 200, 0));
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(state_string);
        format.clearForeground();
        ui->state->setCurrentCharFormat(format);
    }
    else if (state == WRONG_SIGNAL) // 红色
    {
        format.setForeground(QColor(200, 0, 0));
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(state_string);
        format.clearForeground();
        ui->state->setCurrentCharFormat(format);
    }
    else if (state == SIGNAL_SIGNAL) // 蓝色
    {
        format.setForeground(QColor(0, 0, 200));
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(state_string);
        format.clearForeground();
        ui->state->setCurrentCharFormat(format);
    }
    else if (state == EVA_SIGNAL) // EVA 样式
    {
        QFont font = format.font();
        font.setPixelSize(14);
        format.setFont(font);
        format.setFontItalic(true);
        format.setForeground(NORMAL_BLACK);
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(jtr("cubes"));

        format.setFontItalic(false);
        format.setFontWeight(QFont::Black);
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText("          " + state_string);

        format.setFontItalic(true);
        format.setFontWeight(QFont::Normal);
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(jtr("cubes"));

        format.setFontWeight(QFont::Normal);
        format.setFontItalic(false);
        format.clearForeground();
        ui->state->setCurrentCharFormat(format);
    }
    else if (state == TOOL_SIGNAL) // 工具蓝色
    {
        format.setForeground(TOOL_BLUE);
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(state_string);
        format.clearForeground();
        ui->state->setCurrentCharFormat(format);
    }
    else if (state == SYNC_SIGNAL) // 同步橙色
    {
        format.setForeground(LCL_ORANGE);
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(state_string);
        format.clearForeground();
        ui->state->setCurrentCharFormat(format);
    }

    // 轻量级的工具忙碌动画：根据 tool: 文本启停
    if (state_string.startsWith("tool:"))
    {
        const bool isReturn = state_string.contains(jtr("return")) || state_string.contains("return");
        const bool looksStart = state_string.contains('(') && !isReturn;
        if (looksStart && (!decode_pTimer || !decode_pTimer->isActive()))
        {
            wait_play("tool executing");
        }
        else if (isReturn && decode_pTimer && decode_pTimer->isActive() && decodeLabelKey_ == QStringLiteral("tool executing"))
        {
            decode_finish();
        }
    }
}
