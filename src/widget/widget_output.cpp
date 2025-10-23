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
                if (!thinkPart.isEmpty()) output_scroll(thinkPart, themeThinkColor());
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
                    output_scroll(asstPart, themeTextPrimary());
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
        output_scroll(QString(DEFAULT_SPLITER), themeTextPrimary());
    }
    QColor c = chipColorForRole(RecordRole::System);
    const QString trimmed = role.trimmed();
    const QString canonical = trimmed.toLower();
    QString label = trimmed;
    const QString labelSystem = jtr("role_system");
    const QString labelUser = jtr("role_user");
    const QString labelThink = jtr("role_think");
    const QString labelTool = jtr("role_tool");
    const QString labelModel = jtr("role_model");
    if (canonical == QStringLiteral("tool") || trimmed == labelTool)
    {
        c = chipColorForRole(RecordRole::Tool);
        label = labelTool;
    }
    else if (canonical == QStringLiteral("think") || trimmed == labelThink)
    {
        c = chipColorForRole(RecordRole::Think);
        label = labelThink;
    }
    else if (canonical == QStringLiteral("assistant") || canonical == QStringLiteral("model") || trimmed == labelModel)
    {
        c = chipColorForRole(RecordRole::Assistant);
        label = labelModel;
    }
    else if (canonical == QStringLiteral("user") || trimmed == labelUser)
    {
        c = chipColorForRole(RecordRole::User);
        label = labelUser;
    }
    else if (canonical == QStringLiteral("system") || trimmed == labelSystem)
    {
        c = chipColorForRole(RecordRole::System);
        label = labelSystem;
    }
    else
    {
        label = trimmed;
    }
    // Insert role label and a newline
    output_scroll(label, c);
    output_scroll(QString(DEFAULT_SPLITER), themeTextPrimary());
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
    QTextCharFormat format;
    if (state != MATRIX_SIGNAL)
    {
        state_string.replace("\n", "\\n");
        state_string.replace("\r", "\\r");
    }

    const auto resetFormat = [this]()
    {
        QTextCharFormat base;
        base.setForeground(themeStateColor(USUAL_SIGNAL));
        base.setFontItalic(false);
        base.setFontWeight(QFont::Normal);
        ui->state->setCurrentCharFormat(base);
    };

    if (state == EVA_SIGNAL)
    {
        QFont font = format.font();
        font.setPixelSize(14);
        format.setFont(font);
        format.setFontItalic(true);
        format.setForeground(themeStateColor(EVA_SIGNAL));
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(jtr("cubes"));

        format.setFontItalic(false);
        format.setFontWeight(QFont::Black);
        format.setForeground(themeStateColor(EVA_SIGNAL));
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(QStringLiteral("          ") + state_string);

        format.setFontItalic(true);
        format.setFontWeight(QFont::Normal);
        format.setForeground(themeStateColor(EVA_SIGNAL));
        ui->state->setCurrentCharFormat(format);
        ui->state->appendPlainText(jtr("cubes"));

        resetFormat();
        return;
    }

    format.setForeground(themeStateColor(state));
    ui->state->setCurrentCharFormat(format);
    ui->state->appendPlainText(state_string);
    resetFormat();

    if (state_string.startsWith("tool:"))
    {
        const bool isReturn = state_string.contains(jtr("return")) || state_string.contains("return");
        const bool looksStart = state_string.contains('(') && !isReturn;
        if (looksStart)
        {
            if (decode_pTimer && decode_pTimer->isActive()) decode_finish();
            startWaitOnStateLine(QStringLiteral("tool executing"), state_string);
        }
        else if (isReturn && decode_pTimer && decode_pTimer->isActive() && decodeLabelKey_ == QStringLiteral("tool executing"))
        {
            decode_finish();
        }
    }
}
