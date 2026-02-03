#include "ui_widget.h"
#include "widget.h"

#include <algorithm>
#include <QTimer>

namespace
{
// 提示词“工具块”高亮类型：只针对工具相关信息（<tools>/<tool_call> 中的工具名、参数名、关键字段）做轻量高亮。
enum class ToolPromptHighlightKind
{
    Tag,      // <tools> / <tool_call> 等标签本体
    JsonKey,  // "name" / "arguments" / "properties" / "required" 等关键字段
    ToolName, // 工具名（"name" 的字符串值）
    ParamName // 参数名（properties 里的 key / tool_call.arguments 里的 key / required 数组里的字符串）
};

struct ToolPromptSpan
{
    int start = 0;
    int length = 0;
    ToolPromptHighlightKind kind = ToolPromptHighlightKind::JsonKey;
};

struct JsonHighlightOptions
{
    // 在 <tools> block 内：参数名来自 schema.properties 的 key
    bool highlightPropertiesObjectKeys = true;
    // 在 <tool_call> block 内：参数名来自 tool_call.arguments 的 key
    bool highlightArgumentsObjectKeys = false;
};

// 在指定区间内做一个“足够好用”的 JSON 扫描：它不要求整体是合法 JSON（允许多个对象拼接），
// 但要求字符串引号能正确闭合。我们只提取对用户有帮助的“工具名/参数名/关键字段”范围用于高亮。
static void collectJsonToolSpans(QVector<ToolPromptSpan> &out,
                                const QString &text,
                                int rangeStart,
                                int rangeEnd,
                                const JsonHighlightOptions &opt)
{
    rangeStart = qMax(0, rangeStart);
    rangeEnd = qMin(text.size(), rangeEnd);
    if (rangeEnd <= rangeStart) return;

    enum class ContainerType
    {
        Object,
        Array
    };

    struct ContainerState
    {
        ContainerType type = ContainerType::Object;

        // Object 状态机：key -> ':' -> value -> (',' or '}')
        bool expectKey = false;
        bool expectColon = false;
        bool expectValue = false;
        bool expectCommaOrEnd = false;
        QString lastKey;

        // 仅当该 object 是 properties/arguments 的直接 value 时为 true：其直接子 key 视为“参数名”
        bool highlightDirectKeysAsParams = false;

        // 仅当该 array 是 required 的直接 value 时为 true：其直接子 string 视为“参数名”
        bool highlightStringValuesAsParams = false;
    };

    auto pushObject = [&](QVector<ContainerState> &stack, bool highlightKeys) {
        ContainerState st;
        st.type = ContainerType::Object;
        st.expectKey = true;
        st.highlightDirectKeysAsParams = highlightKeys;
        stack.push_back(st);
    };
    auto pushArray = [&](QVector<ContainerState> &stack, bool highlightStrings) {
        ContainerState st;
        st.type = ContainerType::Array;
        st.expectValue = true;
        st.highlightStringValuesAsParams = highlightStrings;
        stack.push_back(st);
    };

    QVector<ContainerState> stack;
    stack.reserve(16);

    bool inString = false;
    bool escape = false;
    int stringStart = -1;

    const auto addSpan = [&](int start, int length, ToolPromptHighlightKind kind) {
        if (length <= 0) return;
        const int s = qMax(rangeStart, start);
        const int e = qMin(rangeEnd, start + length);
        if (e <= s) return;
        ToolPromptSpan sp;
        sp.start = s;
        sp.length = e - s;
        sp.kind = kind;
        out.push_back(sp);
    };

    const auto isKeyOfInterest = [](const QString &key) -> bool {
        return key == QStringLiteral("name") || key == QStringLiteral("arguments") ||
               key == QStringLiteral("properties") || key == QStringLiteral("required");
    };

    int i = rangeStart;
    while (i < rangeEnd)
    {
        const QChar ch = text.at(i);

        if (inString)
        {
            if (escape)
            {
                escape = false;
                ++i;
                continue;
            }
            if (ch == QLatin1Char('\\'))
            {
                escape = true;
                ++i;
                continue;
            }
            if (ch == QLatin1Char('"'))
            {
                // 完整字符串 token（含引号）范围：[stringStart, i]
                const int tokenStart = stringStart;
                const int tokenLen = (i - stringStart + 1);
                const QString tokenContent = text.mid(stringStart + 1, tokenLen - 2);

                if (!stack.isEmpty())
                {
                    ContainerState &top = stack.last();
                    if (top.type == ContainerType::Object)
                    {
                        if (top.expectKey)
                        {
                            // 这是一个 key
                            top.lastKey = tokenContent;
                            top.expectKey = false;
                            top.expectColon = true;

                            // 1) properties/arguments 直接子 key：参数名
                            if (top.highlightDirectKeysAsParams)
                            {
                                addSpan(tokenStart, tokenLen, ToolPromptHighlightKind::ParamName);
                            }
                            // 2) 关键字段：高亮
                            else if (isKeyOfInterest(tokenContent))
                            {
                                addSpan(tokenStart, tokenLen, ToolPromptHighlightKind::JsonKey);
                            }
                        }
                        else if (top.expectValue)
                        {
                            // 这是一个 string value
                            const QString key = top.lastKey;
                            if (key == QStringLiteral("name"))
                            {
                                addSpan(tokenStart, tokenLen, ToolPromptHighlightKind::ToolName);
                            }
                            // required 的数组值在 array 分支处理；这里只处理 "name" 即可。

                            top.expectValue = false;
                            top.expectCommaOrEnd = true;
                        }
                    }
                    else
                    {
                        // Array: string value
                        if (top.expectValue)
                        {
                            if (top.highlightStringValuesAsParams)
                            {
                                addSpan(tokenStart, tokenLen, ToolPromptHighlightKind::ParamName);
                            }
                            top.expectValue = false;
                            top.expectCommaOrEnd = true;
                        }
                    }
                }

                inString = false;
                stringStart = -1;
                escape = false;
                ++i;
                continue;
            }
            ++i;
            continue;
        }

        // 非字符串状态：跳过空白
        if (ch.isSpace())
        {
            ++i;
            continue;
        }

        // 字符串开始
        if (ch == QLatin1Char('"'))
        {
            inString = true;
            escape = false;
            stringStart = i;
            ++i;
            continue;
        }

        // 结构字符处理
        if (ch == QLatin1Char('{'))
        {
            bool highlightKeys = false;
            if (!stack.isEmpty())
            {
                ContainerState &parent = stack.last();
                if (parent.type == ContainerType::Object && parent.expectValue)
                {
                    const QString key = parent.lastKey;
                    if (key == QStringLiteral("properties") && opt.highlightPropertiesObjectKeys)
                    {
                        highlightKeys = true;
                    }
                    else if (key == QStringLiteral("arguments") && opt.highlightArgumentsObjectKeys)
                    {
                        highlightKeys = true;
                    }
                    // value 已开始（object）
                    parent.expectValue = false;
                    parent.expectCommaOrEnd = true;
                }
                else if (parent.type == ContainerType::Array && parent.expectValue)
                {
                    parent.expectValue = false;
                    parent.expectCommaOrEnd = true;
                }
            }
            pushObject(stack, highlightKeys);
            ++i;
            continue;
        }
        if (ch == QLatin1Char('}'))
        {
            if (!stack.isEmpty()) stack.removeLast();
            ++i;
            continue;
        }
        if (ch == QLatin1Char('['))
        {
            bool highlightStrings = false;
            if (!stack.isEmpty())
            {
                ContainerState &parent = stack.last();
                if (parent.type == ContainerType::Object && parent.expectValue)
                {
                    if (parent.lastKey == QStringLiteral("required"))
                    {
                        highlightStrings = true;
                    }
                    parent.expectValue = false;
                    parent.expectCommaOrEnd = true;
                }
                else if (parent.type == ContainerType::Array && parent.expectValue)
                {
                    parent.expectValue = false;
                    parent.expectCommaOrEnd = true;
                }
            }
            pushArray(stack, highlightStrings);
            ++i;
            continue;
        }
        if (ch == QLatin1Char(']'))
        {
            if (!stack.isEmpty()) stack.removeLast();
            ++i;
            continue;
        }
        if (ch == QLatin1Char(':'))
        {
            if (!stack.isEmpty())
            {
                ContainerState &top = stack.last();
                if (top.type == ContainerType::Object && top.expectColon)
                {
                    top.expectColon = false;
                    top.expectValue = true;
                }
            }
            ++i;
            continue;
        }
        if (ch == QLatin1Char(','))
        {
            if (!stack.isEmpty())
            {
                ContainerState &top = stack.last();
                if (top.type == ContainerType::Object)
                {
                    if (top.expectCommaOrEnd)
                    {
                        top.expectCommaOrEnd = false;
                        top.expectKey = true;
                        top.lastKey.clear();
                    }
                }
                else
                {
                    if (top.expectCommaOrEnd)
                    {
                        top.expectCommaOrEnd = false;
                        top.expectValue = true;
                    }
                }
            }
            ++i;
            continue;
        }

        // 标量 value（true/false/null/number 等）：在 expectValue 状态下也需要推进状态机
        if (!stack.isEmpty())
        {
            ContainerState &top = stack.last();
            if (top.type == ContainerType::Object && top.expectValue)
            {
                top.expectValue = false;
                top.expectCommaOrEnd = true;
            }
            else if (top.type == ContainerType::Array && top.expectValue)
            {
                top.expectValue = false;
                top.expectCommaOrEnd = true;
            }
        }

        // 继续前进（不做复杂 token 化，直到遇到下一个结构字符自然会被处理）
        ++i;
    }
}

static void collectTagSpans(QVector<ToolPromptSpan> &out, const QString &text, const QString &tag, ToolPromptHighlightKind kind)
{
    int pos = 0;
    while (true)
    {
        const int idx = text.indexOf(tag, pos);
        if (idx < 0) break;
        ToolPromptSpan sp;
        sp.start = idx;
        sp.length = tag.size();
        sp.kind = kind;
        out.push_back(sp);
        pos = idx + tag.size();
    }
}

static void collectToolPromptSpans(QVector<ToolPromptSpan> &out, const QString &text)
{
    out.clear();
    // 1) 标签本体先高亮（用户视觉锚点）
    collectTagSpans(out, text, QStringLiteral("<tools>"), ToolPromptHighlightKind::Tag);
    collectTagSpans(out, text, QStringLiteral("</tools>"), ToolPromptHighlightKind::Tag);
    collectTagSpans(out, text, QStringLiteral("<tool_call>"), ToolPromptHighlightKind::Tag);
    collectTagSpans(out, text, QStringLiteral("</tool_call>"), ToolPromptHighlightKind::Tag);

    // 2) <tools> block：高亮工具名/关键字段/参数名（properties/required）
    {
        const QString openTag = QStringLiteral("<tools>");
        const QString closeTag = QStringLiteral("</tools>");
        int pos = 0;
        while (true)
        {
            const int start = text.indexOf(openTag, pos);
            if (start < 0) break;
            const int contentStart = start + openTag.size();
            const int end = text.indexOf(closeTag, contentStart);
            if (end < 0) break;
            JsonHighlightOptions opt;
            opt.highlightPropertiesObjectKeys = true;
            opt.highlightArgumentsObjectKeys = false;
            collectJsonToolSpans(out, text, contentStart, end, opt);
            pos = end + closeTag.size();
        }
    }

    // 3) <tool_call> block：高亮 tool_call JSON 内的工具名与 arguments 参数名
    {
        const QString openTag = QStringLiteral("<tool_call>");
        const QString closeTag = QStringLiteral("</tool_call>");
        int pos = 0;
        while (true)
        {
            const int start = text.indexOf(openTag, pos);
            if (start < 0) break;
            const int contentStart = start + openTag.size();
            const int end = text.indexOf(closeTag, contentStart);
            if (end < 0) break;
            JsonHighlightOptions opt;
            opt.highlightPropertiesObjectKeys = false;
            opt.highlightArgumentsObjectKeys = true;
            collectJsonToolSpans(out, text, contentStart, end, opt);
            pos = end + closeTag.size();
        }
    }

    // 4) 排序 + 去重（避免偶发重复 span）
    std::sort(out.begin(), out.end(), [](const ToolPromptSpan &a, const ToolPromptSpan &b) {
        if (a.start != b.start) return a.start < b.start;
        return a.length > b.length;
    });

    QVector<ToolPromptSpan> deduped;
    deduped.reserve(out.size());
    int lastEnd = -1;
    for (const ToolPromptSpan &sp : out)
    {
        if (sp.length <= 0) continue;
        if (sp.start < 0 || sp.start >= text.size()) continue;
        const int end = sp.start + sp.length;
        if (end <= sp.start) continue;
        // 简化处理：出现重叠时保留更靠前的 span（常见情况下不会发生重叠）
        if (sp.start < lastEnd) continue;
        deduped.push_back(sp);
        lastEnd = end;
    }
    out.swap(deduped);
}
} // namespace

//-------------------------------------------------------------------------
//------------------------------输出--------------------------------
//-------------------------------------------------------------------------
// output 与 state 的 verticalScrollBar() 统一管理：如果位于底部，新增内容将自动滚动；用户上滚后不再自动滚动

// 更新输出，is_while 表示从流式输出的 token
void Widget::reflash_output(const QString result, bool is_while, QColor color)
{
    if (engineerProxyRuntime_.active)
    {
        temp_assistant_history += result;
        handleEngineerStreamOutput(result, is_while);
        return;
    }
    if (compactionInFlight_ && !is_while)
    {
        flushPendingStream();
        if (!compactionHeaderPrinted_)
        {
            currentCompactIndex_ = recordCreate(RecordRole::Compact);
            appendRoleHeader(QStringLiteral("compact"));
            compactionHeaderPrinted_ = true;
        }
        QString clean = result;
        clean.replace(QString(DEFAULT_THINK_BEGIN), QString());
        clean.replace(QString(DEFAULT_THINK_END), QString());
        if (!clean.isEmpty())
        {
            output_scroll(clean, textColorForRole(RecordRole::Compact));
            if (currentCompactIndex_ >= 0) recordAppendText(currentCompactIndex_, clean);
        }
        temp_assistant_history += result;
        return;
    }
    if (is_while)
    {
        enqueueStreamChunk(result, color);
        return;
    }

    flushPendingStream();

    // Non-stream: sanitize and print with provided color
    QString out = result;
    out.replace(QString(DEFAULT_THINK_BEGIN), QString());
    out.replace(QString(DEFAULT_THINK_END), QString());
    output_scroll(out, color);
}

// 在指定 cursor 位置插入文本，并对提示词中的工具相关内容做轻量高亮。
// 说明：这是“展示层”能力，不改变原始文本内容，也不影响消息/记录的持久化。
void Widget::insertTextWithToolHighlight(QTextCursor &cursor, const QString &text, const QColor &baseColor)
{
    // 不包含工具标签时直接走普通插入，避免无意义的扫描开销
    const bool mayContainTools = text.contains(QLatin1Char('<')) &&
                                (text.contains(QStringLiteral("<tools>")) || text.contains(QStringLiteral("<tool_call>")) ||
                                 text.contains(QStringLiteral("</tools>")) || text.contains(QStringLiteral("</tool_call>")));

    QTextCharFormat baseFmt;
    baseFmt.setForeground(QBrush(baseColor));

    if (!mayContainTools)
    {
        cursor.setCharFormat(baseFmt);
        cursor.insertText(text);
        cursor.setCharFormat(QTextCharFormat());
        return;
    }

    QVector<ToolPromptSpan> spans;
    collectToolPromptSpans(spans, text);
    if (spans.isEmpty())
    {
        cursor.setCharFormat(baseFmt);
        cursor.insertText(text);
        cursor.setCharFormat(QTextCharFormat());
        return;
    }

    // 高亮样式：只把“原本带背景填充”的那部分（标签/参数名）改为蓝灰色，
    // 其余（关键字段/工具名）仍保持原有配色，以便用户一眼分辨层级。
    const QColor highlightBlueGray = themeVisuals_.darkBase ? PROMPT_TOOL_HIGHLIGHT_BLUEGRAY_DARK
                                                            : PROMPT_TOOL_HIGHLIGHT_BLUEGRAY_LIGHT;
    const QColor toolColor = themeVisuals_.stateTool;
    const QColor keyColor = themeVisuals_.stateSignal;

    QTextCharFormat tagFmt;
    tagFmt.setForeground(QBrush(highlightBlueGray));
    tagFmt.setFontWeight(QFont::Bold);

    QTextCharFormat keyFmt;
    keyFmt.setForeground(QBrush(keyColor));
    keyFmt.setFontWeight(QFont::Bold);

    QTextCharFormat toolNameFmt;
    toolNameFmt.setForeground(QBrush(toolColor));
    toolNameFmt.setFontWeight(QFont::Bold);

    QTextCharFormat paramFmt;
    paramFmt.setForeground(QBrush(highlightBlueGray));
    paramFmt.setFontWeight(QFont::Bold);

    const auto formatForKind = [&](ToolPromptHighlightKind kind) -> const QTextCharFormat & {
        switch (kind)
        {
        case ToolPromptHighlightKind::Tag: return tagFmt;
        case ToolPromptHighlightKind::JsonKey: return keyFmt;
        case ToolPromptHighlightKind::ToolName: return toolNameFmt;
        case ToolPromptHighlightKind::ParamName: return paramFmt;
        }
        return baseFmt;
    };

    int pos = 0;
    for (const ToolPromptSpan &sp : spans)
    {
        if (sp.length <= 0) continue;
        if (sp.start < 0 || sp.start >= text.size()) continue;
        const int end = qMin(text.size(), sp.start + sp.length);
        if (end <= sp.start) continue;

        if (sp.start > pos)
        {
            cursor.setCharFormat(baseFmt);
            cursor.insertText(text.mid(pos, sp.start - pos));
        }

        cursor.setCharFormat(formatForKind(sp.kind));
        cursor.insertText(text.mid(sp.start, end - sp.start));
        pos = end;
    }

    if (pos < text.size())
    {
        cursor.setCharFormat(baseFmt);
        cursor.insertText(text.mid(pos));
    }

    // 恢复默认格式，避免后续输出继承高亮样式
    cursor.setCharFormat(QTextCharFormat());
}

// 更新输出：专用于“系统提示词/约定提示词”的展示
// - 基础颜色仍由调用方决定（通常为 themeTextPrimary）
// - 仅在文本中出现 <tools>/<tool_call> 时进行工具相关高亮
void Widget::reflash_output_tool_highlight(const QString &result, const QColor &baseColor)
{
    if (!ui || !ui->output) return;

    // 工程师代理运行时，输出区被工程师 session 接管，避免破坏其流式处理逻辑
    if (engineerProxyRuntime_.active)
    {
        reflash_output(result, false, baseColor);
        return;
    }

    flushPendingStream();

    QString out = result;
    out.replace(QString(DEFAULT_THINK_BEGIN), QString());
    out.replace(QString(DEFAULT_THINK_END), QString());

    QTextCursor cursor = ui->output->textCursor();
    cursor.movePosition(QTextCursor::End);
    insertTextWithToolHighlight(cursor, out, baseColor);
    ui->output->setTextCursor(cursor);

    if (!is_stop_output_scroll)
    {
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum());
    }
    if (isHostControlled())
    {
        broadcastControlOutput(out, false, baseColor);
    }
}

void Widget::processStreamChunk(const QString &chunk, const QColor &color)
{
    if (chunk.isEmpty()) return;
    Q_UNUSED(color);

    // 压缩模式：直接输出紫色摘要，不进入 think/assistant 分支
    if (compactionInFlight_)
    {
        QString clean = chunk;
        clean.replace(QString(DEFAULT_THINK_BEGIN), QString());
        clean.replace(QString(DEFAULT_THINK_END), QString());
        if (!compactionHeaderPrinted_)
        {
            currentCompactIndex_ = recordCreate(RecordRole::Compact);
            appendRoleHeader(QStringLiteral("compact"));
            compactionHeaderPrinted_ = true;
        }
        if (!clean.isEmpty())
        {
            output_scroll(clean, textColorForRole(RecordRole::Compact), true, QStringLiteral("compact"));
            if (currentCompactIndex_ >= 0) recordAppendText(currentCompactIndex_, clean);
        }
        temp_assistant_history += chunk;
        return;
    }

    if (pendingAssistantHeaderReset_)
    {
        pendingAssistantHeaderReset_ = false;
        turnThinkActive_ = false;
        turnThinkHeaderPrinted_ = false;
        turnAssistantHeaderPrinted_ = false;
        currentThinkIndex_ = -1;
        currentAssistantIndex_ = -1;
    }

    // 补完模式：输出区全文作为 prompt，模型返回内容应当“续写追加”在原文本末尾。
    // 1) 不能插入新的“模型/think”角色头与分隔线，否则会破坏文本连续性；
    // 2) <think>...</think> 属于推理过程，补完模式下不应污染续写文本，直接忽略显示；
    // 3) 仍然把原始 chunk 追加到 temp_assistant_history，保证收尾阶段能正确提取最终文本与推理。
    if (ui_state == COMPLETE_STATE)
    {
        const QString begin = QString(DEFAULT_THINK_BEGIN);
        const QString tend = QString(DEFAULT_THINK_END);

        int pos = 0;
        const int n = chunk.size();
        while (pos < n)
        {
            if (turnThinkActive_)
            {
                const int endIdx = chunk.indexOf(tend, pos);
                if (endIdx == -1)
                {
                    // 仍处于 think 块中：本段剩余内容全部跳过（仅保留在历史里用于收尾解析）。
                    break;
                }
                // 跳过 </think>，恢复到正文续写
                turnThinkActive_ = false;
                pos = endIdx + tend.size();
                continue;
            }

            const int beginIdx = chunk.indexOf(begin, pos);
            if (beginIdx == -1)
            {
                const QString appendText = chunk.mid(pos);
                if (!appendText.isEmpty())
                {
                    output_scroll(appendText, themeTextPrimary(), true);
                }
                break;
            }

            // 先输出 <think> 之前的正文部分
            const QString appendText = chunk.mid(pos, beginIdx - pos);
            if (!appendText.isEmpty())
            {
                output_scroll(appendText, themeTextPrimary(), true);
            }
            // 进入 think 块：跳过 <think>
            turnThinkActive_ = true;
            pos = beginIdx + begin.size();
        }

        temp_assistant_history += chunk;
        return;
    }

    const QString begin = QString(DEFAULT_THINK_BEGIN);
    const QString tend = QString(DEFAULT_THINK_END);

    int pos = 0;
    const int n = chunk.size();
    while (pos < n)
    {
        if (turnThinkActive_)
        {
            int endIdx = chunk.indexOf(tend, pos);
            const int until = (endIdx == -1) ? n : endIdx;
            QString thinkPart = chunk.mid(pos, until - pos);
            thinkPart.replace(begin, QString());
            thinkPart.replace(tend, QString());
            if (!turnThinkHeaderPrinted_)
            {
                if (currentThinkIndex_ < 0) currentThinkIndex_ = recordCreate(RecordRole::Think);
                appendRoleHeader(QStringLiteral("think"));
                turnThinkHeaderPrinted_ = true;
            }
            const bool endsHere = (endIdx != -1);
            if (!thinkPart.isEmpty()) output_scroll(thinkPart, themeThinkColor(), true, QStringLiteral("think"), endsHere ? 0 : 1);
            if (!thinkPart.isEmpty() && currentThinkIndex_ >= 0) recordAppendText(currentThinkIndex_, thinkPart);
            if (endIdx == -1)
            {
                break;
            }
            turnThinkActive_ = false;
            pos = endIdx + tend.size();
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
            int beginIdx = chunk.indexOf(begin, pos);
            const int until = (beginIdx == -1) ? n : beginIdx;
            QString asstPart = chunk.mid(pos, until - pos);
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
                output_scroll(asstPart, themeTextPrimary(), true, QStringLiteral("assistant"));
                if (currentAssistantIndex_ >= 0) recordAppendText(currentAssistantIndex_, asstPart);
            }
            if (beginIdx == -1)
            {
                break;
            }
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

    temp_assistant_history += chunk;
}

void Widget::enqueueStreamChunk(const QString &chunk, const QColor &color)
{
    if (chunk.isEmpty())
    {
        return;
    }

    streamPendingChars_ += chunk.size();
    streamPending_.append({chunk, color});

    if (!streamFlushTimer_)
    {
        streamFlushTimer_ = new QTimer(this);
        streamFlushTimer_->setSingleShot(true);
        connect(streamFlushTimer_, &QTimer::timeout, this, &Widget::flushPendingStream);
    }

    if (streamPendingChars_ >= kStreamMaxBufferChars)
    {
        flushPendingStream();
        return;
    }

    if (!streamFlushTimer_->isActive())
    {
        streamFlushTimer_->start(kStreamFlushIntervalMs);
    }
}

void Widget::flushPendingStream()
{
    if (streamFlushTimer_ && streamFlushTimer_->isActive())
    {
        streamFlushTimer_->stop();
    }
    if (streamPending_.isEmpty())
    {
        return;
    }

    QVector<PendingStreamUpdate> pending;
    pending.swap(streamPending_);
    streamPendingChars_ = 0;

    for (const PendingStreamUpdate &update : pending)
    {
        processStreamChunk(update.text, update.color);
    }
}

// Print a role header above content; color by role
void Widget::appendRoleHeader(const QString &role)
{
    // Ensure a blank line before header if output is not empty
    const bool emptyDoc = ui->output->document() && ui->output->document()->isEmpty();
    const QColor primaryColor = themeTextPrimary();
    if (!emptyDoc)
    {
        output_scroll(QString(DEFAULT_SPLITER), primaryColor);
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
    const QString labelCompact = jtr("role_compact");
    if (canonical == QStringLiteral("tool") || trimmed == labelTool)
    {
        c = chipColorForRole(RecordRole::Tool);
        label = labelTool;
    }
    else if (canonical == QStringLiteral("compact") || trimmed == labelCompact)
    {
        c = chipColorForRole(RecordRole::Compact);
        label = labelCompact;
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
    output_scroll(QString(DEFAULT_SPLITER), primaryColor);
}

// 输出区滚动条事件响应
void Widget::output_scrollBarValueChanged(int value)
{
    // 滚动条处于底部时自动滚动
    int maximumValue = output_scrollBar->maximum();
    // 经验：一次性追加大量文本时，布局更新会导致 maximum 短时间内跳变，
    // 此时 value 可能短暂落后于 maximum，若严格用 “==” 判定会误判为用户手动上滚，
    // 进而关闭自动置底。这里用一个极小阈值做兜底（不影响用户真正上滚的场景）。
    const int kBottomEpsilon = 2;
    if (maximumValue - value <= kBottomEpsilon)
    {
        is_stop_output_scroll = 0;
    }
    else
    {
        is_stop_output_scroll = 1;
    }
}

// 在 output 末尾追加文本并着色
void Widget::output_scroll(QString output, QColor color, bool isStream, const QString &roleHint, int thinkActiveFlag)
{
    QTextCursor cursor = ui->output->textCursor();
    cursor.movePosition(QTextCursor::End); // 光标移动到末尾

    // 统一插入：正常文本直接插入；若包含 <tools>/<tool_call> 则仅对“工具相关关键字段”做高亮。
    // 注意：这是纯展示逻辑，不改变 output 字符串本身，也不影响后续工具解析与消息持久化。
    insertTextWithToolHighlight(cursor, output, color);

    if (!is_stop_output_scroll) // 未手动停用自动滚动时每次追加自动滚动到底
    {
        ui->output->verticalScrollBar()->setValue(ui->output->verticalScrollBar()->maximum()); // 设置滚动条到最底端
    }
    if (isHostControlled())
    {
        broadcastControlOutput(output, isStream, color, roleHint, thinkActiveFlag);
    }
}

void Widget::ensureOutputAtBottom()
{
    if (!ui || !ui->output) return;
    QScrollBar *scrollBar = ui->output->verticalScrollBar();
    if (!scrollBar) return;

    const auto applyBottom = [this]()
    {
        if (!ui || !ui->output) return;
        if (QScrollBar *sb = ui->output->verticalScrollBar())
        {
            is_stop_output_scroll = false;
            sb->setValue(sb->maximum());
        }
    };

    is_stop_output_scroll = false;
    applyBottom();
    QTimer::singleShot(0, this, applyBottom);
}

// 状态区统一追加接口，保证始终写在末尾
int Widget::appendStateLine(const QString &text, const QTextCharFormat &format)
{
    if (!ui || !ui->state) return -1;

    QTextCursor cursor = ui->state->textCursor();
    cursor.movePosition(QTextCursor::End);
    const bool docEmpty = ui->state->document() && ui->state->document()->blockCount() == 1 && ui->state->toPlainText().isEmpty();
    if (!docEmpty)
    {
        cursor.insertBlock(); // 非首行先换行再写入，避免插入到中间
    }
    cursor.setCharFormat(format);
    cursor.insertText(text);
    const int lineNumber = cursor.blockNumber();
    ui->state->setTextCursor(cursor);
    if (QScrollBar *hbar = ui->state->horizontalScrollBar())
    {
        hbar->setValue(hbar->minimum());
    }
    return lineNumber;
}

// 刷新状态区
void Widget::reflash_state(QString state_string, SIGNAL_STATE state)
{
    if (!ui || !ui->state) return;
    QTextCharFormat format;
    if (state != MATRIX_SIGNAL)
    {
        state_string.replace("\n", "\\n");
        state_string.replace("\r", "\\r");
    }

    const bool controllerView = (linkProfile_ == LinkProfile::Control && !isHostControlled());

    if (state == EVA_SIGNAL)
    {
        QFont font = format.font();
        font.setPixelSize(14);
        format.setFont(font);
        format.setFontItalic(true);
        format.setForeground(themeStateColor(EVA_SIGNAL));
        appendStateLine(jtr("cubes"), format);

        format.setFontItalic(false);
        format.setFontWeight(QFont::Black);
        format.setForeground(themeStateColor(EVA_SIGNAL));
        appendStateLine(QStringLiteral("          ") + state_string, format);

        format.setFontItalic(true);
        format.setFontWeight(QFont::Normal);
        format.setForeground(themeStateColor(EVA_SIGNAL));
        appendStateLine(jtr("cubes"), format);
        return;
    }

    format.setForeground(themeStateColor(state));
    const int lineNumber = appendStateLine(state_string, format);

    if (!controllerView && state_string.startsWith("tool:"))
    {
        const bool isReturn = state_string.contains(jtr("return")) || state_string.contains("return");
        const bool looksStart = state_string.contains('(') && !isReturn;
        if (looksStart)
        {
            if (decode_pTimer && decode_pTimer->isActive()) decode_finish();
            startWaitOnStateLine(QStringLiteral("tool executing"), state_string, lineNumber);
        }
        else if (isReturn && decode_pTimer && decode_pTimer->isActive() && decodeLabelKey_ == QStringLiteral("tool executing"))
        {
            decode_finish();
        }
    }
    if (isHostControlled())
    {
        broadcastControlState(state_string, state);
    }
}
