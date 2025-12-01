#include "widget.h"
#include "ui_widget.h"

namespace
{
inline bool isDocLineBreak(QChar ch)
{
    return ch == QChar('\n') || ch == QChar('\r') || ch == QChar(QChar::LineSeparator) ||
           ch == QChar(QChar::ParagraphSeparator);
}
}

int Widget::recordCreate(RecordRole role)
{
    RecordEntry e;
    e.role = role;
    e.docFrom = outputDocEnd();
    e.docTo = e.docFrom;
    e.text.clear();
    e.msgIndex = -1;
    recordEntries_.push_back(e);
    const int idx = recordEntries_.size() - 1;
    if (ui->recordBar) ui->recordBar->addNode(chipColorForRole(role), QString());
    return idx;
}

void Widget::recordAppendText(int index, const QString &text)
{
    if (index < 0 || index >= recordEntries_.size()) return;
    recordEntries_[index].text += text;
    recordEntries_[index].docTo = outputDocEnd();
    QString tip = recordEntries_[index].text;
    if (tip.size() > 600) tip = tip.left(600) + "...";
    if (ui->recordBar) ui->recordBar->updateNode(index, tip);
}

void Widget::recordClear()
{
    recordEntries_.clear();
    currentThinkIndex_ = -1;
    currentAssistantIndex_ = -1;
    lastSystemRecordIndex_ = -1;
    if (ui->recordBar) ui->recordBar->clearNodes();
}

void Widget::updateRecordEntryContent(int index, const QString &newText)
{
    if (index < 0 || index >= recordEntries_.size()) return;
    RecordEntry &entry = recordEntries_[index];
    QTextDocument *doc = ui->output->document();
    if (!doc) return;
    const int docEnd = outputDocEnd();
    int contentFrom = qBound(0, entry.docFrom, docEnd);
    while (contentFrom < docEnd && isDocLineBreak(doc->characterAt(contentFrom))) ++contentFrom;

    const auto canonicalRoleName = [](RecordRole r) -> QString
    {
        switch (r)
        {
        case RecordRole::System: return QStringLiteral("system");
        case RecordRole::User: return QStringLiteral("user");
        case RecordRole::Assistant: return QStringLiteral("assistant");
        case RecordRole::Think: return QStringLiteral("think");
        case RecordRole::Tool: return QStringLiteral("tool");
        }
        return QString();
    };
    const auto legacyRoleName = [](RecordRole r) -> QString
    {
        if (r == RecordRole::Assistant) return QStringLiteral("model");
        return QString();
    };
    const auto displayRoleName = [this](RecordRole r) -> QString
    {
        switch (r)
        {
        case RecordRole::System: return jtr("role_system");
        case RecordRole::User: return jtr("role_user");
        case RecordRole::Assistant: return jtr("role_model");
        case RecordRole::Think: return jtr("role_think");
        case RecordRole::Tool: return jtr("role_tool");
        }
        return QString();
    };

    QStringList headerCandidates;
    const QString display = displayRoleName(entry.role);
    const QString canonical = canonicalRoleName(entry.role);
    const QString legacy = legacyRoleName(entry.role);
    if (!display.isEmpty()) headerCandidates << display;
    if (!canonical.isEmpty() && !headerCandidates.contains(canonical)) headerCandidates << canonical;
    if (!legacy.isEmpty() && !headerCandidates.contains(legacy)) headerCandidates << legacy;

    auto consumeHeaderIfPresent = [&](int &pos)
    {
        const auto attempt = [&](const QString &candidate) -> bool
        {
            if (candidate.isEmpty()) return false;
            const int len = candidate.size();
            if (pos + len + 1 > docEnd) return false;
            for (int i = 0; i < len; ++i)
            {
                if (doc->characterAt(pos + i) != candidate.at(i)) return false;
            }
            if (!isDocLineBreak(doc->characterAt(pos + len))) return false;
            pos += len + 1;
            return true;
        };
        for (const QString &candidate : headerCandidates)
        {
            if (attempt(candidate)) return true;
        }
        return false;
    };

    consumeHeaderIfPresent(contentFrom);

    const int oldContentTo = qBound(contentFrom, entry.docTo, docEnd);
    replaceOutputRangeColored(contentFrom, oldContentTo, newText, textColorForRole(entry.role));
    const int newEnd = contentFrom + newText.size();
    const int delta = newEnd - oldContentTo;
    entry.text = newText;
    entry.docTo = newEnd;
    for (int i = index + 1; i < recordEntries_.size(); ++i)
    {
        recordEntries_[i].docFrom += delta;
        recordEntries_[i].docTo += delta;
    }

    QString tip = newText;
    if (tip.size() > 600) tip = tip.left(600) + "...";
    if (ui->recordBar) ui->recordBar->updateNode(index, tip);
}

void Widget::gotoRecord(int index)
{
    if (index < 0 || index >= recordEntries_.size()) return;
    if (ui->recordBar) ui->recordBar->setSelectedIndex(index);
    const auto &e = recordEntries_[index];
    QTextDocument *doc = ui->output->document();
    const int end = outputDocEnd();

    // Normalize start: skip any leading blank lines so the role header is the first visible line
    int from = qBound(0, e.docFrom, end);
    while (from < end && isDocLineBreak(doc->characterAt(from))) ++from;

    // Place caret at header start and ensure it's visible
    QTextCursor c(doc);
    c.setPosition(from);
    ui->output->setTextCursor(c);
    ui->output->ensureCursorVisible();

    // Align header line to the very top of the viewport
    if (QScrollBar *vs = ui->output->verticalScrollBar())
    {
        const QRect r = ui->output->cursorRect(c);
        const int target = qBound(0, vs->maximum(), vs->value() + r.top());
        vs->setValue(target);
    }

    ui->output->setFocus();
}

void Widget::restoreSessionById(const QString &sessionId)
{
    if (!history_) return;
    recordClear();
    SessionMeta meta;
    QJsonArray msgs;
    if (!history_->loadSession(sessionId, meta, msgs))
    {
        reflash_state(jtr("history db error"), WRONG_SIGNAL);
        return;
    }
    if (!history_->resume(sessionId))
    {
        reflash_state(jtr("history db error"), WRONG_SIGNAL);
    }

    flushPendingStream();
    ui->output->clear();
    ui_messagesArray = QJsonArray();

    // Helper to map role string to UI color and record role
    auto roleToRecord = [](const QString &r) -> RecordRole
    {
        if (r == QLatin1String("system")) return RecordRole::System;
        if (r == QLatin1String("user")) return RecordRole::User;
        if (r == QLatin1String("assistant") || r == QLatin1String("model")) return RecordRole::Assistant;
        if (r == QLatin1String("think")) return RecordRole::Think;
        if (r == QLatin1String("tool")) return RecordRole::Tool;
        return RecordRole::User;
    };
    auto roleToColor = [&](const QString &r) -> QColor
    {
        if (r == QLatin1String("think")) return themeThinkColor();
        if (r == QLatin1String("tool")) return themeStateColor(TOOL_SIGNAL);
        return themeTextPrimary();
    };

    for (const auto &v : msgs)
    {
        QJsonObject m = v.toObject();
        const QString role = m.value("role").toString();
        const QJsonValue contentVal = m.value("content");
        const RecordRole recRole = roleToRecord(role);
        QString reasoningText = m.value("reasoning_content").toString();
        if (reasoningText.isEmpty()) reasoningText = m.value("thinking").toString();

        // Build displayable text from content (string or multimodal array)
        QString displayText;
        if (contentVal.isArray())
        {
            const QJsonArray parts = contentVal.toArray();
            for (const auto &pv : parts)
            {
                if (!pv.isObject()) continue;
                const QJsonObject po = pv.toObject();
                const QString type = po.value("type").toString();
                if (type == QLatin1String("text"))
                    displayText += po.value("text").toString();
            }
        }
        else
        {
            QString textWithTags = contentVal.isString() ? contentVal.toString() : contentVal.toVariant().toString();
            QString finalText = textWithTags;
            const QString tBegin = QString(DEFAULT_THINK_BEGIN);
            const QString tEnd = QString(DEFAULT_THINK_END);
            if (reasoningText.isEmpty())
            {
                QStringList extracted;
                int searchPos = 0;
                while (true)
                {
                    int startIdx = finalText.indexOf(tBegin, searchPos);
                    if (startIdx == -1) break;
                    int endIdx = finalText.indexOf(tEnd, startIdx + tBegin.size());
                    if (endIdx == -1) break;
                    const int rStart = startIdx + tBegin.size();
                    extracted << finalText.mid(rStart, endIdx - rStart);
                    finalText.remove(startIdx, (endIdx + tEnd.size()) - startIdx);
                    searchPos = startIdx;
                }
                if (!extracted.isEmpty()) reasoningText = extracted.join(QString());
            }
            finalText.replace(tBegin, QString());
            finalText.replace(tEnd, QString());
            displayText = finalText;
        }

        int thinkIdx = -1;
        if (recRole == RecordRole::Assistant && !reasoningText.isEmpty())
        {
            thinkIdx = recordCreate(RecordRole::Think);
            appendRoleHeader(QStringLiteral("think"));
            reflash_output(reasoningText, 0, themeThinkColor());
            recordAppendText(thinkIdx, reasoningText);
        }

        const int recIdx = recordCreate(recRole);
        appendRoleHeader(role);
        reflash_output(displayText, 0, roleToColor(role));
        recordAppendText(recIdx, displayText);

        // Append sanitized message back to UI memory (remove local-only metadata)
        QJsonObject uiMsg = m;
        uiMsg.remove("local_images");
        uiMsg.remove("thinking");
        if (!contentVal.isArray()) uiMsg.insert("content", displayText);
        if (recRole == RecordRole::Assistant)
        {
            if (!reasoningText.isEmpty())
            {
                uiMsg.insert("reasoning_content", reasoningText);
            }
            else
            {
                uiMsg.remove("reasoning_content");
            }
        }
        ui_messagesArray.append(uiMsg);
        const int msgIndex = ui_messagesArray.size() - 1;
        recordEntries_[recIdx].msgIndex = msgIndex;
        if (thinkIdx >= 0) recordEntries_[thinkIdx].msgIndex = msgIndex;

        // Restore any local images recorded in history
        QStringList localPaths;
        const QJsonValue localsVal = m.value("local_images");
        if (localsVal.isArray())
        {
            for (const auto &lv : localsVal.toArray())
            {
                if (lv.isString()) localPaths << lv.toString();
            }
        }
        if (!localPaths.isEmpty())
        {
            QStringList showable;
            for (const QString &p : localPaths)
            {
                if (QFileInfo::exists(p))
                {
                    showable << p;
                }
                else
                {
                    // Warn user that the original image file is missing
                    reflash_state(QStringLiteral("ui: missing image file -> ") + p, WRONG_SIGNAL);
                    // Also print a visible placeholder into the transcript
                    output_scroll(p + QStringLiteral(" (missing)\n"));
                }
            }
            if (!showable.isEmpty()) showImages(showable);
        }
    }

    if (!meta.title.isEmpty())
        reflash_state("ui:" + jtr("loaded session") + " " + meta.title, SUCCESS_SIGNAL);
    else
        reflash_state("ui:" + jtr("loaded session"), SUCCESS_SIGNAL);

    int resumeSlot = -1;
    if (ui_mode == LINK_MODE)
    {
        const QString ep = (ui_state == CHAT_STATE) ? (apis.api_endpoint + apis.api_chat_endpoint)
                                                    : (apis.api_endpoint + apis.api_completion_endpoint);
        if (meta.endpoint == ep) resumeSlot = meta.slot_id;
    }
    else
    {
        const QString currentEp = formatLocalEndpoint(activeServerHost_, activeServerPort_);
        const QString legacyEp = serverManager ? serverManager->endpointBase() : QString();
        if ((!currentEp.isEmpty() && meta.endpoint == currentEp) ||
            (!legacyEp.isEmpty() && meta.endpoint == legacyEp))
        {
            resumeSlot = meta.slot_id;
        }
    }
    currentSlotId_ = (resumeSlot >= 0) ? resumeSlot : -1;
}

void Widget::replaceOutputRangeColored(int from, int to, const QString &text, QColor color)
{
    QTextCursor c(ui->output->document());
    const int endBound = outputDocEnd();
    from = qBound(0, from, endBound);
    to = qBound(0, to, endBound);
    if (to < from) std::swap(to, from);
    c.setPosition(from);
    c.setPosition(to, QTextCursor::KeepAnchor);
    c.removeSelectedText();
    QTextCharFormat fmt;
    fmt.setForeground(QBrush(color));
    c.mergeCharFormat(fmt);
    c.insertText(text);
    QTextCharFormat fmt0;
    c.mergeCharFormat(fmt0);
}

void Widget::onRecordClicked(int index)
{
    gotoRecord(index);
}

void Widget::onRecordDoubleClicked(int index)
{
    if (index < 0 || index >= recordEntries_.size()) return;
    if (ui->recordBar) ui->recordBar->setSelectedIndex(index);
    auto &e = recordEntries_[index];
    const auto canonicalRoleName = [](RecordRole r) -> QString
    {
        switch (r)
        {
        case RecordRole::System: return QStringLiteral("system");
        case RecordRole::User: return QStringLiteral("user");
        case RecordRole::Assistant: return QStringLiteral("assistant");
        case RecordRole::Think: return QStringLiteral("think");
        case RecordRole::Tool: return QStringLiteral("tool");
        }
        return QStringLiteral("user");
    };
    const auto legacyRoleName = [](RecordRole r) -> QString
    {
        if (r == RecordRole::Assistant) return QStringLiteral("model");
        return QString();
    };
    const auto displayRoleName = [this, &canonicalRoleName](RecordRole r) -> QString
    {
        switch (r)
        {
        case RecordRole::System: return jtr("role_system");
        case RecordRole::User: return jtr("role_user");
        case RecordRole::Assistant: return jtr("role_model");
        case RecordRole::Think: return jtr("role_think");
        case RecordRole::Tool: return jtr("role_tool");
        }
        return canonicalRoleName(r);
    };
    const QString roleDisplay = displayRoleName(e.role);
    const QString roleCanonical = canonicalRoleName(e.role);
    const QString roleLegacy = legacyRoleName(e.role);
    QStringList headerCandidates;
    if (!roleDisplay.isEmpty()) headerCandidates << roleDisplay;
    if (!roleCanonical.isEmpty() && !headerCandidates.contains(roleCanonical)) headerCandidates << roleCanonical;
    if (!roleLegacy.isEmpty() && !headerCandidates.contains(roleLegacy)) headerCandidates << roleLegacy;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("%1 %2").arg(jtr("edit history record"), roleDisplay));
    dlg.setModal(true);
    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    QTextEdit *ed = new QTextEdit(&dlg);
    ed->setPlainText(e.text);
    ed->setMinimumSize(QSize(480, 280));
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(ed);
    lay->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;
    const QString newText = ed->toPlainText();
    if (newText == e.text) return;

    // Compute content range to replace (preserve role header line)
    QTextDocument *doc = ui->output->document();
    const int docEnd = outputDocEnd();
    int contentFrom = qBound(0, e.docFrom, docEnd);

    // Skip leading blank line inserted before header (if any)
    while (contentFrom < docEnd && isDocLineBreak(doc->characterAt(contentFrom))) ++contentFrom;

    auto consumeHeaderIfPresent = [&](int &pos, const QStringList &candidates)
    {
        const auto attempt = [&](const QString &candidate) -> bool
        {
            if (candidate.isEmpty()) return false;
            const int len = candidate.size();
            if (pos + len + 1 > docEnd) return false;
            for (int i = 0; i < len; ++i)
            {
                if (doc->characterAt(pos + i) != candidate.at(i)) return false;
            }
            if (!isDocLineBreak(doc->characterAt(pos + len))) return false;
            pos += len + 1;
            return true;
        };
        for (const QString &candidate : candidates)
        {
            if (attempt(candidate)) return true;
        }
        return false;
    };

    consumeHeaderIfPresent(contentFrom, headerCandidates);

    const int oldContentTo = qBound(contentFrom, e.docTo, docEnd);
    // Replace only the content region, keep coloring by role
    replaceOutputRangeColored(contentFrom, oldContentTo, newText, textColorForRole(e.role));

    // Update current entry and cascade position delta to following entries
    const int newEnd = contentFrom + newText.size();
    const int delta = newEnd - oldContentTo;
    e.text = newText;
    e.docTo = newEnd;
    for (int i = index + 1; i < recordEntries_.size(); ++i)
    {
        recordEntries_[i].docFrom += delta;
        recordEntries_[i].docTo += delta;
    }

    QString tip = newText;
    if (tip.size() > 600) tip = tip.left(600) + "...";
    if (ui->recordBar) ui->recordBar->updateNode(index, tip);

    // Ensure role header line exists after editing (guard against accidental deletion)
    {
        QTextDocument *doc = ui->output->document();
        const int docEnd2 = outputDocEnd();
        int s = qBound(0, recordEntries_[index].docFrom, docEnd2);
        // Skip any leading blank lines before header
        while (s < docEnd2 && isDocLineBreak(doc->characterAt(s))) ++s;

        const auto hasHeader = [&](int startPos, const QStringList &candidates) -> bool
        {
            for (const QString &candidate : candidates)
            {
                if (candidate.isEmpty()) continue;
                const int len = candidate.size();
                if (startPos + len + 1 > docEnd2) continue;
                bool headerMatch = true;
                for (int i = 0; i < len; ++i)
                {
                    if (doc->characterAt(startPos + i) != candidate.at(i))
                    {
                        headerMatch = false;
                        break;
                    }
                }
                if (!headerMatch) continue;
                if (!isDocLineBreak(doc->characterAt(startPos + len))) continue;
                return true;
            }
            return false;
        };

        if (!hasHeader(s, headerCandidates))
        {
            // Insert header + newline at position s with role color
            QTextCursor ic(doc);
            ic.setPosition(s);
            QTextCharFormat headerFmt;
            headerFmt.setForeground(QBrush(chipColorForRole(e.role)));
            ic.mergeCharFormat(headerFmt);
            ic.insertText(roleDisplay + QString(DEFAULT_SPLITER));

            // Adjust current and subsequent record ranges by the inserted length
            const int ins = roleDisplay.size() + 1;
            e.docTo += ins;
            for (int i = index + 1; i < recordEntries_.size(); ++i)
            {
                recordEntries_[i].docFrom += ins;
                recordEntries_[i].docTo += ins;
            }
        }
    }

    // Maintain a leading separator so each record header stays visually separated after edits
    if (index > 0)
    {
        QTextDocument *doc2 = ui->output->document();
        if (doc2)
        {
            RecordEntry &cur = recordEntries_[index];
            const int docTail = outputDocEnd();
            if (docTail > 0)
            {
                const auto safeCharAt = [&](int pos) -> QChar
                {
                    if (pos < 0 || pos >= docTail) return QChar();
                    return doc2->characterAt(pos);
                };
                int startPos = cur.docFrom;
                if (startPos < 0) startPos = 0;
                if (startPos >= docTail) startPos = docTail - 1;
                const QChar atStart = safeCharAt(startPos);
                const QChar beforeStart = safeCharAt(startPos - 1);
                if (isDocLineBreak(beforeStart) && !isDocLineBreak(atStart) && startPos > 0)
                {
                    cur.docFrom = startPos - 1;
                }
                else if (!isDocLineBreak(atStart))
                {
                    QTextCursor separatorCursor(doc2);
                    separatorCursor.setPosition(startPos);
                    separatorCursor.insertText(QString(DEFAULT_SPLITER));
                    const int insLen = QString(DEFAULT_SPLITER).size();
                    cur.docFrom = startPos;
                    cur.docTo += insLen;
                    for (int i = index + 1; i < recordEntries_.size(); ++i)
                    {
                        recordEntries_[i].docFrom += insLen;
                        recordEntries_[i].docTo += insLen;
                    }
                }
            }
        }
    }

    // Update in-memory message content and persist to history
    if (e.msgIndex >= 0 && e.msgIndex < ui_messagesArray.size())
    {
        QJsonObject m = ui_messagesArray[e.msgIndex].toObject();
        if (e.role == RecordRole::Think)
        {
            if (newText.isEmpty())
            {
                m.remove("reasoning_content");
            }
            else
            {
                m.insert("reasoning_content", newText);
            }
            m.remove("thinking");
        }
        else
        {
            m.insert("content", newText);
        }
        ui_messagesArray[e.msgIndex] = m;
        if (history_ && !history_->sessionId().isEmpty()) { history_->rewriteAllMessages(ui_messagesArray); }
    }
}

int Widget::outputDocEnd() const
{
    QTextCursor c(ui->output->document());
    c.movePosition(QTextCursor::End);
    return c.position();
}
