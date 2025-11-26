#include "xtool.h"

#include "utils/processrunner.h"

#include <QDirIterator>
#include <QEventLoop>
#include <QHash>
#include <QPair>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>

namespace
{
constexpr int kMaxToolMessageBytes = 30 * 1024;  // 工具输出允许的最大字节数，超出即触发截断流程
constexpr int kToolMessageHeadBytes = 15 * 1024; // 截断时保留的消息头部字节数，约 75% 用于保留开头关键内容
constexpr int kToolMessageTailBytes = 15 * 1024;  // 截断时保留的消息尾部字节数，保留尾端提示与收尾信息

QString clampToolMessage(const QString &message)
{
    const QByteArray utf8 = message.toUtf8();
    if (utf8.size() <= kMaxToolMessageBytes) return message;
    const QByteArray headBytes = utf8.left(kToolMessageHeadBytes);
    const QByteArray tailBytes = utf8.right(kToolMessageTailBytes);
    const QString head = QString::fromUtf8(headBytes.constData(), headBytes.size());
    const QString tail = QString::fromUtf8(tailBytes.constData(), tailBytes.size());
    const double totalKb = utf8.size() / 1024.0;
    const double headKb = headBytes.size() / 1024.0;
    const double tailKb = tailBytes.size() / 1024.0;
    return head + "\n...\n" + tail + QString("\n[tool output truncated: %1 KB total, showing first %2 KB and last %3 KB]").arg(totalKb, 0, 'f', 1).arg(headKb, 0, 'f', 1).arg(tailKb, 0, 'f', 1);
}

struct MatchRange
{
    int start = -1;
    int length = 0;
};

QString normalizeNewlines(QString text)
{
    text.replace("\r\n", "\n");
    text.replace('\r', '\n');
    return text;
}

QString restoreNewlines(QString text, bool hadCRLF, bool hadCR)
{
    if (hadCRLF) return text.replace("\n", "\r\n");
    if (hadCR)
    {
        text.replace('\n', '\r');
        return text;
    }
    return text;
}

QVector<MatchRange> findExactMatches(const QString &text, const QString &needle)
{
    QVector<MatchRange> results;
    if (needle.isEmpty()) return results;
    int idx = 0;
    while ((idx = text.indexOf(needle, idx, Qt::CaseSensitive)) != -1)
    {
        results.append({idx, needle.length()});
        idx += needle.length();
    }
    return results;
}

QString buildFlexiblePattern(const QString &needle)
{
    QString pattern;
    pattern.reserve(needle.size() * 2);
    bool lastWasWhitespace = false;
    for (const QChar ch : needle)
    {
        if (ch.isSpace())
        {
            if (!lastWasWhitespace)
            {
                pattern += "\\s+";
                lastWasWhitespace = true;
            }
        }
        else
        {
            pattern += QRegularExpression::escape(QString(ch));
            lastWasWhitespace = false;
        }
    }
    return pattern;
}

QVector<MatchRange> findFlexibleMatches(const QString &text, const QString &needle)
{
    QVector<MatchRange> results;
    QString pattern = buildFlexiblePattern(needle);
    if (pattern.isEmpty()) return results;
    QRegularExpression regex(pattern, QRegularExpression::UseUnicodePropertiesOption);
    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        if (!match.hasMatch()) continue;
        results.append({match.capturedStart(), match.capturedLength()});
    }
    return results;
}

QString snippetPreview(const QString &text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return {};
    constexpr int kPreviewLimit = 160;
    if (trimmed.size() > kPreviewLimit) trimmed = trimmed.left(kPreviewLimit) + "...";
    return trimmed;
}
} // namespace

struct xTool::ToolInvocation
{
    quint64 id = 0;
    QString name;
    mcp::json call;
    mcp::json args;
    std::atomic<bool> cancelled{false};
    QString commandContent;
    QString aggregatedOutput;
    QString workingDirectory;
};

thread_local xTool::ToolInvocation *xTool::tlsCurrentInvocation_ = nullptr;

void xTool::sendStateMessage(const QString &message, SIGNAL_STATE state)
{
    if (tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire)) return;
    emit tool2ui_state(clampToolMessage(message), state);
}

void xTool::sendPushMessage(const QString &message)
{
    if (tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire)) return;
    emit tool2ui_pushover(clampToolMessage(message));
}

xTool::ToolInvocationPtr xTool::activeInvocation() const
{
    std::lock_guard<std::mutex> lock(invocationMutex_);
    return activeInvocation_;
}

void xTool::setActiveInvocation(const ToolInvocationPtr &invocation)
{
    std::lock_guard<std::mutex> lock(invocationMutex_);
    activeInvocation_ = invocation;
}

void xTool::clearActiveInvocation(const ToolInvocationPtr &invocation)
{
    std::lock_guard<std::mutex> lock(invocationMutex_);
    if (activeInvocation_ == invocation)
    {
        activeInvocation_.reset();
    }
}

xTool::ToolInvocationPtr xTool::createInvocation(mcp::json tools_call)
{
    cancelActiveTool();
    auto invocation = std::make_shared<ToolInvocation>();
    invocation->id = nextInvocationId_.fetch_add(1, std::memory_order_relaxed);
    invocation->call = std::move(tools_call);
    invocation->name = QString::fromStdString(get_string_safely(invocation->call, "name"));
    invocation->args = get_json_object_safely(invocation->call, "arguments");
    setActiveInvocation(invocation);
    return invocation;
}

xTool::xTool(QString applicationDirPath_)
{
    applicationDirPath = applicationDirPath_;
    // Default engineer working directory under app path
    workDirRoot = QDir::cleanPath(applicationDirPath + "/EVA_WORK");
    dockerSandbox_ = new DockerSandbox();
    dockerSandbox_->setParent(this);
    connect(dockerSandbox_, &DockerSandbox::statusChanged, this, &xTool::onDockerStatusChanged);
    qDebug() << "tool init over";
}

xTool::~xTool()
{
    ;
}

// Update working directory root for engineer tools (created lazily)

void xTool::recv_workdir(QString dir)
{
    if (dir.isEmpty()) return;
    workDirRoot = QDir::cleanPath(dir);
    // Do not force-create here to avoid unwanted dirs; Exec() ensures presence
    sendStateMessage("tool:" + QString("workdir -> ") + workDirRoot, USUAL_SIGNAL);
}

void xTool::recv_dockerConfig(bool enabled, QString image, QString workdir)
{
    DockerSandbox::Config cfg;
    cfg.enabled = enabled;
    cfg.image = image.trimmed();
    cfg.hostWorkdir = workdir.trimmed().isEmpty() ? QString() : QDir::cleanPath(workdir.trimmed());
    dockerConfig_ = cfg;
    if (dockerSandbox_) dockerSandbox_->applyConfig(cfg);
}

void xTool::onDockerStatusChanged(const DockerSandboxStatus &status)
{
    emit tool2ui_dockerStatusChanged(status);
    QString message;
    SIGNAL_STATE level = USUAL_SIGNAL;
    if (!status.enabled)
    {
        message = QStringLiteral("docker sandbox disabled");
    }
    else if (!status.lastError.isEmpty())
    {
        message = QStringLiteral("docker sandbox error: %1").arg(status.lastError);
        level = WRONG_SIGNAL;
        sendPushMessage(message);
    }
    else if (status.ready)
    {
        const QString hostDir = status.hostWorkdir.isEmpty() ? resolveWorkRoot() : status.hostWorkdir;
        const QString hostDisplay = QDir::toNativeSeparators(hostDir);
        const QString containerDir = status.containerWorkdir.isEmpty() ? QStringLiteral("/workspace") : status.containerWorkdir;
        message = QStringLiteral("docker sandbox ready (%1)\nhost %2 -> container %3")
                      .arg(status.image.isEmpty() ? QStringLiteral("ubuntu:latest") : status.image,
                           hostDisplay,
                           containerDir);
        level = SUCCESS_SIGNAL;
    }
    else
    {
        message = QStringLiteral("docker sandbox preparing (%1)")
                      .arg(status.image.isEmpty() ? QStringLiteral("ubuntu:latest") : status.image);
    }
    sendStateMessage("tool:" + message, level);
}

void xTool::cancelExecuteCommand()
{
    if (!activeCommandProcess_) return;
    activeCommandInterrupted_ = true;
    if (activeCommandInvocation_) activeCommandInvocation_->cancelled.store(true, std::memory_order_release);
    if (activeCommandProcess_->state() != QProcess::NotRunning)
    {
        activeCommandProcess_->kill();
    }
}

void xTool::startWorkerInvocation(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    QtConcurrent::run([this, invocation]()
                      {
        ToolInvocation *previous = tlsCurrentInvocation_;
        tlsCurrentInvocation_ = invocation.get();
        struct Cleanup
        {
            xTool *tool;
            ToolInvocation *previous;
            ToolInvocationPtr invocation;
            ~Cleanup()
            {
                xTool::tlsCurrentInvocation_ = previous;
                if (!tool) return;
                auto localTool = tool;
                auto localInvocation = invocation;
                QMetaObject::invokeMethod(localTool, [localTool, localInvocation]() { localTool->finishInvocation(localInvocation); }, Qt::QueuedConnection);
            }
        } cleanup{this, previous, invocation};
        runToolWorker(invocation); });
}

void xTool::Exec(mcp::json tools_call)
{
    auto invocation = createInvocation(std::move(tools_call));
    if (!invocation) return;
    QString tools_args = QString::fromStdString(invocation->args.dump());
    qDebug() << "tools_name" << invocation->name << "tools_args" << tools_args;
    if (invocation->name == "execute_command")
    {
        invocation->commandContent = QString::fromStdString(get_string_safely(invocation->args, "content"));
        startExecuteCommand(invocation);
        return;
    }
    if (invocation->name == "stablediffusion")
    {
        handleStableDiffusion(invocation);
        return;
    }
    if (invocation->name.contains("mcp_tools_list"))
    {
        handleMcpToolList(invocation);
        return;
    }
    if (invocation->name.contains("@"))
    {
        handleMcpToolCall(invocation);
        return;
    }
    startWorkerInvocation(invocation);
}

void xTool::runToolWorker(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    mcp::json tools_call = invocation->call;
    QString tools_name = invocation->name;
    mcp::json tools_args_ = invocation->args;
    QString tools_args = QString::fromStdString(tools_args_.dump()); // arguments字段提取出来还是一个对象所以用dump
    qDebug() << "tools_name" << tools_name << "tools_args" << tools_args;
    if (shouldAbort(invocation)) return;
    //----------------------计算器------------------
    if (tools_name == "calculator")
    {
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_, "expression"));
        sendStateMessage("tool:" + QString("calculator(") + build_in_tool_arg + ")");
        QString result = QString::number(te_interp(build_in_tool_arg.toStdString().c_str(), 0));
        // qDebug()<<"tool:" + QString("calculator ") + jtr("return") + "\n" + result;
        if (result == "nan") // 计算失败的情况
        {
            sendPushMessage(QString("calculator ") + jtr("return") + "Calculation failed, please confirm if the calculation formula is reasonable");
        }
        else
        {
            sendPushMessage(QString("calculator ") + jtr("return") + "\n" + result);
        }
        sendStateMessage("tool:" + QString("calculator ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
    }
    //----------------------命令提示符------------------
    else if (tools_name == "execute_command")
    {
        return;
    }
    else if (tools_name == "knowledge")
    {
        QElapsedTimer time4;
        time4.start();
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_, "content"));
        QString result;
        if (Embedding_DB.size() == 0)
        {
            result = jtr("Please tell user to embed knowledge into the knowledge base first");
            sendStateMessage("tool:" + QString("knowledge ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
            sendPushMessage(QString("knowledge ") + jtr("return") + "\n" + result);
        }
        else
        {
            // 查询计算词向量和计算相似度，返回匹配的文本段
            sendStateMessage("tool:" + jtr("qureying"));
            result = embedding_query_process(build_in_tool_arg);
            sendStateMessage("tool:" + jtr("qurey&timeuse") + QString(": ") + QString::number(time4.nsecsElapsed() / 1000000000.0, 'f', 2) + " s");
            sendStateMessage("tool:" + QString("knowledge ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
            sendPushMessage(QString("knowledge ") + jtr("return") + "\n" + result);
        }
    }
    //----------------------鼠标键盘------------------
    else if (tools_name == "controller")
    {
        std::vector<std::string> build_in_tool_arg = get_string_list_safely(tools_args_, "sequence");
        // 拼接打印参数
        std::ostringstream oss;
        for (size_t i = 0; i < build_in_tool_arg.size(); ++i)
        {
            oss << build_in_tool_arg[i];
            if (i < build_in_tool_arg.size() - 1)
            {
                oss << " "; // 用空格分隔
            }
        }
        std::string build_in_tool_arg_ = oss.str();
        sendStateMessage("tool:" + QString("controller(") + QString::fromStdString(build_in_tool_arg_) + ")");
        // 执行行动序列
        excute_sequence(build_in_tool_arg);
        sendPushMessage(QString("controller ") + jtr("return") + "\n" + "excute sequence over" + "\n");
    }
    //----------------------文生图------------------
    else if (tools_name == "stablediffusion")
    {
        return;
    }
    //----------------------读取文件------------------
    else if (tools_name == "read_file")
    {
        // 获取路径参数
        QString build_in_tool_arg = QString::fromStdString(get_string_safely(tools_args_, "path"));
        QString filepath = build_in_tool_arg;
        // Normalize to work root; strip duplicated root prefix then join
        const QString root = QDir::fromNativeSeparators(workDirRoot.isEmpty() ? applicationDirPath + "/EVA_WORK" : workDirRoot);
        filepath = QDir::fromNativeSeparators(filepath);
        if (filepath.startsWith(root + "/")) filepath = filepath.mid(root.size() + 1);
        filepath = QDir(root).filePath(filepath);
        // 获取行号参数，默认为1
        int start_line = get_int_safely(tools_args_, "start_line", 1);
        int end_line = get_int_safely(tools_args_, "end_line", INT_MAX);
        // 验证行号有效性
        if (start_line < 1) start_line = 1;
        if (end_line < start_line) end_line = start_line;
        if (end_line - start_line + 1 > 200) end_line = start_line + 199;
        QString result;
        QFile file(filepath);
        // 尝试打开文件
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            sendPushMessage(QString("read_file ") + jtr("return") + QString("can not open file: %1").arg(filepath)); // 返回错误
            return;
        }
        // 使用 QTextStream 读取文件内容
        QTextStream in(&file);
        in.setCodec("UTF-8"); // 设置编码为UTF-8
        // 读取指定行范围
        QStringList lines;
        int current_line = 0;
        while (!in.atEnd())
        {
            current_line++;
            QString line = in.readLine();
            if (current_line >= start_line && current_line <= end_line)
            {
                lines.append(line);
            }
            if (current_line > end_line) break;
        }
        file.close();
        result = lines.join("\n");
        sendStateMessage("tool:" + QString("read_file ") + jtr("return") + QString(" (lines %1-%2)\n").arg(start_line).arg(qMin(current_line, end_line)) + result, TOOL_SIGNAL);
        sendPushMessage(QString("read_file ") + jtr("return") + QString(" (lines %1-%2)\n").arg(start_line).arg(qMin(current_line, end_line)) + result); // 返回结果
    }
    //----------------------写入文件------------------
    else if (tools_name == "write_file")
    {
        QString filepath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        QString content = QString::fromStdString(get_string_safely(tools_args_, "content"));
        const QString root = QDir::fromNativeSeparators(workDirRoot.isEmpty() ? applicationDirPath + "/EVA_WORK" : workDirRoot);
        filepath = QDir::fromNativeSeparators(filepath);
        if (filepath.startsWith(root + "/")) filepath = filepath.mid(root.size() + 1);
        filepath = QDir(root).filePath(filepath);
        // Extract the directory path from the file path
        QFileInfo fileInfo(filepath);
        QString dirPath = fileInfo.absolutePath();
        // Create the directory structure if it doesn't exist
        QDir dir;
        if (!dir.mkpath(dirPath))
        {
            sendPushMessage(QString("write_file ") + jtr("return") + "Failed to create directory"); // 返回错误
            return; // or handle the error as appropriate
        }
        QFile file(filepath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            sendPushMessage(QString("write_file ") + jtr("return") + "Could not open file for writing" + file.errorString()); // 返回错误
            return; // or handle the error as appropriate
        }
        // 处理换行符
        // qDebug()<<content;
        QTextStream out(&file);
        out.setCodec("UTF-8"); // 设置编码为UTF-8
        out << content;
        file.close();
        QString result = "write over";
        sendStateMessage("tool:" + QString("write_file ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
        sendPushMessage(QString("write_file ") + jtr("return") + "\n" + result);
    }
    else if (tools_name == "replace_in_file")
    {
        QString filepath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        QString oldStrRaw = QString::fromStdString(get_string_safely(tools_args_, "old_string"));
        QString newStrRaw = QString::fromStdString(get_string_safely(tools_args_, "new_string"));
        bool expectedProvided = false;
        int expectedRepl = 1;
        if (tools_args_.contains("expected_replacements"))
        {
            expectedProvided = true;
            try
            {
                expectedRepl = std::max(1, tools_args_["expected_replacements"].get<int>());
            }
            catch (...)
            {
                expectedRepl = 1;
            }
        }
        if (oldStrRaw.isEmpty())
        {
            sendPushMessage("replace_in_file " + jtr("return") + " old_string is empty.");
            return;
        }
        const QString root = QDir::fromNativeSeparators(workDirRoot.isEmpty() ? applicationDirPath + "/EVA_WORK" : workDirRoot);
        filepath = QDir::fromNativeSeparators(filepath);
        if (filepath.startsWith(root + "/")) filepath = filepath.mid(root.size() + 1);
        filepath = QDir(root).filePath(filepath);
        QFile inFile(filepath);
        if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            sendPushMessage("replace_in_file " + jtr("return") + "Could not open file for reading: " + inFile.errorString());
            return;
        }
        QString originalContent = QString::fromUtf8(inFile.readAll());
        inFile.close();
        const bool hadCRLF = originalContent.contains("\r\n");
        const bool hadCR = !hadCRLF && originalContent.contains('\r');
        QString normalizedContent = normalizeNewlines(originalContent);
        const QString normalizedOld = normalizeNewlines(oldStrRaw);
        const QString normalizedNew = normalizeNewlines(newStrRaw);
        QVector<MatchRange> matches = findExactMatches(normalizedContent, normalizedOld);
        bool usedFlexibleMatch = false;
        if (matches.isEmpty())
        {
            matches = findFlexibleMatches(normalizedContent, normalizedOld);
            usedFlexibleMatch = !matches.isEmpty();
        }
        if (matches.isEmpty())
        {
            QString message = "replace_in_file " + jtr("return") + " old_string NOT found.";
            const QString preview = snippetPreview(normalizedOld);
            if (!preview.isEmpty())
            {
                message += "\nSnippet: " + preview;
            }
            message += "\nHint: provide more surrounding context or verify indentation.";
            sendPushMessage(message);
            return;
        }
        if (expectedProvided && matches.size() != expectedRepl)
        {
            sendPushMessage("replace_in_file " + jtr("return") + " " + QString("Expected %1 replacement(s) but found %2.").arg(expectedRepl).arg(matches.size()));
            return;
        }
        const bool autoExpanded = !expectedProvided && matches.size() > expectedRepl;
        int replacementsToApply = expectedProvided ? expectedRepl : matches.size();
        if (replacementsToApply > matches.size()) replacementsToApply = matches.size();
        int applied = 0;
        for (int i = replacementsToApply - 1; i >= 0; --i)
        {
            const MatchRange &range = matches.at(i);
            normalizedContent.replace(range.start, range.length, normalizedNew);
            ++applied;
        }
        QString finalContent = restoreNewlines(normalizedContent, hadCRLF, hadCR);
        QFileInfo fi(filepath);
        QDir dir;
        if (!dir.mkpath(fi.absolutePath()))
        {
            sendPushMessage("replace_in_file " + jtr("return") + "Failed to create directory.");
            return;
        }
        QFile outFile(filepath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            sendPushMessage("replace_in_file " + jtr("return") + "Could not open file for writing: " + outFile.errorString());
            return;
        }
        QTextStream ts(&outFile);
        ts.setCodec("UTF-8");
        ts << finalContent;
        outFile.close();
        QString result = QString("replaced %1 occurrence(s)").arg(applied);
        QStringList notes;
        if (usedFlexibleMatch) notes << "whitespace-insensitive search";
        if (autoExpanded && applied > 1) notes << QString("auto-applied to %1 identical matches").arg(applied);
        if (!notes.isEmpty()) result += QString(" [%1]").arg(notes.join("; "));
        sendStateMessage("tool:replace_in_file " + jtr("return") + "\n" + result, TOOL_SIGNAL);
        sendPushMessage("replace_in_file " + jtr("return") + "\n" + result);
    }



    else if (tools_name == "edit_in_file")
    {
        const auto sendError = [&](const QString &msg) {
            sendPushMessage("edit_in_file " + jtr("return") + " " + msg);
        };
        if (!tools_args_.contains("edits") || !tools_args_["edits"].is_array())
        {
            sendError("edits must be an array.");
            return;
        }
        QString filepath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        const QString root = QDir::fromNativeSeparators(workDirRoot.isEmpty() ? applicationDirPath + "/EVA_WORK" : workDirRoot);
        filepath = QDir::fromNativeSeparators(filepath);
        if (filepath.startsWith(root + "/")) filepath = filepath.mid(root.size() + 1);
        filepath = QDir(root).filePath(filepath);
        QFile inFile(filepath);
        if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            sendError("Could not open file for reading: " + inFile.errorString());
            return;
        }
        QString originalContent = QString::fromUtf8(inFile.readAll());
        inFile.close();
        const bool hadCRLF = originalContent.contains("\r\n");
        const bool hadCR = !hadCRLF && originalContent.contains('\r');
        QString normalizedContent = normalizeNewlines(originalContent);
        const bool hadTrailingNewline = normalizedContent.endsWith('\n');
        QStringList lines = normalizedContent.split('\n', Qt::KeepEmptyParts);
        if (hadTrailingNewline && !lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();
        struct EditOperation
        {
            QString action;
            int startLine = 0;
            int endLine = 0;
            QStringList newLines;
            int anchorLine = 0;
            int ordinal = 0;
        };
        QVector<EditOperation> operations;
        const mcp::json &editsJson = tools_args_["edits"];
        operations.reserve(static_cast<int>(editsJson.size()));
        int ordinal = 0;
        for (const auto &editVal : editsJson)
        {
            ++ordinal;
            if (!editVal.is_object())
            {
                sendError(QString("edit #%1 is not an object.").arg(ordinal));
                return;
            }
            const mcp::json &editObj = editVal;
            QString action = QString::fromStdString(get_string_safely(editObj, "action")).trimmed();
            if (action.isEmpty())
            {
                sendError(QString("edit #%1 is missing action.").arg(ordinal));
                return;
            }
            action = action.toLower();
            if (action != "replace" && action != "insert_before" && action != "insert_after" && action != "delete")
            {
                sendError(QString("edit #%1 has unsupported action '%2'.").arg(ordinal).arg(action));
                return;
            }
            int startLine = get_int_safely(editObj, "start_line", -1);
            if (startLine <= 0)
            {
                sendError(QString("edit #%1 has invalid start_line %2.").arg(ordinal).arg(startLine));
                return;
            }
            int endLine = startLine;
            const bool endProvided = editObj.contains("end_line");
            if (action == "replace" || action == "delete")
            {
                if (!endProvided)
                {
                    sendError(QString("edit #%1 requires end_line.").arg(ordinal));
                    return;
                }
                try
                {
                    endLine = editObj.at("end_line").get<int>();
                }
                catch (...)
                {
                    sendError(QString("edit #%1 has invalid end_line.").arg(ordinal));
                    return;
                }
                if (endLine < startLine)
                {
                    sendError(QString("edit #%1 has end_line < start_line.").arg(ordinal));
                    return;
                }
            }
            QStringList newLines;
            if (action == "replace" || action == "insert_before" || action == "insert_after")
            {
                if (!editObj.contains("new_content"))
                {
                    sendError(QString("edit #%1 requires new_content.").arg(ordinal));
                    return;
                }
                QString newContent = QString::fromStdString(get_string_safely(editObj, "new_content"));
                QString normalizedNew = normalizeNewlines(newContent);
                if (!normalizedNew.isEmpty())
                {
                    newLines = normalizedNew.split('\n', Qt::KeepEmptyParts);
                    if (normalizedNew.endsWith('\n') && !newLines.isEmpty()) newLines.removeLast();
                }
            }
            EditOperation op;
            op.action = action;
            op.startLine = startLine;
            op.endLine = (action == "replace" || action == "delete") ? endLine : startLine;
            op.newLines = newLines;
            op.ordinal = ordinal;
            if (action == "replace" || action == "delete")
            {
                op.anchorLine = op.endLine;
            }
            else if (action == "insert_after")
            {
                op.anchorLine = startLine + 1;
            }
            else
            {
                op.anchorLine = startLine;
            }
            operations.append(op);
        }
        if (operations.isEmpty())
        {
            sendPushMessage("edit_in_file " + jtr("return") + " no edits supplied.");
            return;
        }
        std::sort(operations.begin(), operations.end(), [](const EditOperation &a, const EditOperation &b) {
            if (a.anchorLine != b.anchorLine) return a.anchorLine > b.anchorLine;
            return a.ordinal > b.ordinal;
        });
        QHash<QString, int> actionCount;
        for (const EditOperation &op : operations)
        {
            const int currentLineCount = lines.size();
            if (op.action == "replace")
            {
                const int startIdx = op.startLine - 1;
                const int endIdx = op.endLine - 1;
                if (startIdx < 0 || endIdx >= currentLineCount)
                {
                    sendError(QString("edit #%1 references line out of range (file has %2 lines).").arg(op.ordinal).arg(currentLineCount));
                    return;
                }
                for (int i = endIdx; i >= startIdx; --i) lines.removeAt(i);
                for (int i = 0; i < op.newLines.size(); ++i) lines.insert(startIdx + i, op.newLines.at(i));
            }
            else if (op.action == "delete")
            {
                const int startIdx = op.startLine - 1;
                const int endIdx = op.endLine - 1;
                if (startIdx < 0 || endIdx >= currentLineCount)
                {
                    sendError(QString("edit #%1 references line out of range (file has %2 lines).").arg(op.ordinal).arg(currentLineCount));
                    return;
                }
                for (int i = endIdx; i >= startIdx; --i) lines.removeAt(i);
            }
            else if (op.action == "insert_before")
            {
                const int insertIdx = op.startLine - 1;
                if (insertIdx < 0 || insertIdx > currentLineCount)
                {
                    sendError(QString("edit #%1 insert_before target is out of range (file has %2 lines).").arg(op.ordinal).arg(currentLineCount));
                    return;
                }
                for (int i = 0; i < op.newLines.size(); ++i) lines.insert(insertIdx + i, op.newLines.at(i));
            }
            else if (op.action == "insert_after")
            {
                const int anchorIdx = op.startLine - 1;
                if (anchorIdx < 0 || anchorIdx >= currentLineCount)
                {
                    sendError(QString("edit #%1 insert_after target is out of range (file has %2 lines).").arg(op.ordinal).arg(currentLineCount));
                    return;
                }
                int insertIdx = anchorIdx + 1;
                for (int i = 0; i < op.newLines.size(); ++i) lines.insert(insertIdx + i, op.newLines.at(i));
            }
            actionCount[op.action] += 1;
        }
        bool ensureNewline = hadTrailingNewline;
        if (tools_args_.contains("ensure_newline_at_eof"))
        {
            try
            {
                ensureNewline = tools_args_.at("ensure_newline_at_eof").get<bool>();
            }
            catch (...)
            {
                sendError("ensure_newline_at_eof must be a boolean.");
                return;
            }
        }
        QString normalizedResult = lines.join("\n");
        if (ensureNewline && !normalizedResult.endsWith("\n")) normalizedResult.append("\n");
        else if (!ensureNewline && normalizedResult.endsWith("\n") && !normalizedResult.isEmpty()) normalizedResult.chop(1);
        QString finalContent = restoreNewlines(normalizedResult, hadCRLF, hadCR);
        QFileInfo fi(filepath);
        QDir dir;
        if (!dir.mkpath(fi.absolutePath()))
        {
            sendError("Failed to create directory.");
            return;
        }
        QFile outFile(filepath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            sendError("Could not open file for writing: " + outFile.errorString());
            return;
        }
        QTextStream ts(&outFile);
        ts.setCodec("UTF-8");
        ts << finalContent;
        outFile.close();
        QStringList parts;
        const QString actions[] = {"replace", "insert_before", "insert_after", "delete"};
        for (const QString &act : actions)
        {
            const int count = actionCount.value(act, 0);
            if (count > 0) parts << QString("%1:%2").arg(act).arg(count);
        }
        QString result = QString("applied %1 edit(s)").arg(operations.size());
        if (!parts.isEmpty()) result += " [" + parts.join(", ") + "]";
        if (ensureNewline != hadTrailingNewline)
        {
            result += ensureNewline ? " [newline ensured]" : " [newline removed]";
        }
        sendStateMessage("tool:edit_in_file " + jtr("return") + "\n" + result, TOOL_SIGNAL);
        sendPushMessage("edit_in_file " + jtr("return") + "\n" + result);
    }

    //----------------------列出目录（工程师）------------------
    else if (tools_name == "list_files")
    {
        QString reqPath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        const QString effectivePath = reqPath.trimmed().isEmpty() ? QStringLiteral(".") : reqPath.trimmed();
        sendStateMessage("tool:" + QString("list_files(") + effectivePath + ")");
        const QString root = QDir::fromNativeSeparators(workDirRoot.isEmpty() ? applicationDirPath + "/EVA_WORK" : workDirRoot);
        QDir rootDir(root);
        QString abs = QDir::fromNativeSeparators(rootDir.filePath(effectivePath));
        QFileInfo dirInfo(abs);
        // 安全校验：限制在工程师根目录内
        const QString relCheck = rootDir.relativeFilePath(dirInfo.absoluteFilePath());
        if (relCheck.startsWith(".."))
        {
            const QString msg = QString("Access denied: path outside work root -> %1").arg(dirInfo.absoluteFilePath());
            sendPushMessage(QString("list_files ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("list_files ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }
        if (!dirInfo.exists() || !dirInfo.isDir())
        {
            const QString msg = QString("Not a directory: %1").arg(dirInfo.absoluteFilePath());
            sendPushMessage(QString("list_files ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("list_files ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }
        QDir d(dirInfo.absoluteFilePath());
        d.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        d.setSorting(QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
        QStringList outLines;
        const QFileInfoList items = d.entryInfoList();
        int shown = 0;
        const int kMax = 2000; // 上限，防止过长输出
        for (const QFileInfo &it : items)
        {
            if (shouldAbort(invocation)) break;
            const QString rel = rootDir.relativeFilePath(it.absoluteFilePath());
            if (it.isDir())
            {
                outLines << (QString("DIR  ") + rel + "/");
            }
            else
            {
                outLines << (QString("FILE ") + rel + QString(" (%1 B)").arg(it.size()));
            }
            if (++shown >= kMax) break;
        }
        if (items.size() > kMax)
        {
            outLines << QString("... %1 more").arg(items.size() - kMax);
        }
        if (shouldAbort(invocation)) return;
        const QString result = outLines.join(" ");
        sendStateMessage("tool:" + QString("list_files ") + jtr("return") + " " + result, TOOL_SIGNAL);
        sendPushMessage(QString("list_files ") + jtr("return") + " " + result);
    }
    //----------------------搜索内容（工程师）------------------
    else if (tools_name == "search_content")
    {
        const QString query = QString::fromStdString(get_string_safely(tools_args_, "query"));
        sendStateMessage("tool:" + QString("search_content(") + query + ")");
        if (query.trimmed().isEmpty())
        {
            const QString msg = QString("Empty query.");
            sendPushMessage(QString("search_content ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("search_content ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }
        const QString root = QDir::fromNativeSeparators(workDirRoot.isEmpty() ? applicationDirPath + "/EVA_WORK" : workDirRoot);
        QDir rootDir(root);
        // 仅扫描工程师目录内的文本文件
        auto isLikelyText = [](const QString &path, qint64 size) -> bool
        {
            static const QSet<QString> exts = {
                "txt", "md", "markdown", "log", "ini", "cfg", "conf", "csv", "tsv", "json", "yaml", "yml", "toml", "xml", "html", "htm",
                "css", "js", "ts", "tsx", "jsx", "py", "ipynb", "c", "cc", "cpp", "h", "hpp", "hh", "java", "kt", "rs", "go", "rb", "php",
                "sh", "bash", "zsh", "ps1", "bat", "cmake", "mak", "make", "gradle", "properties", "sql", "mm", "m", "swift"
            };
            static const QSet<QString> namesWithoutExt = {
                ".gitignore", ".gitmodules", ".gitattributes", ".clang-format", ".clang-tidy", ".editorconfig", ".env", ".env.local",
                ".env.example", "dockerfile", "makefile", "cmakelists.txt", "license", "license.txt", "readme", "readme.md"
            };
            if (size > (qint64)2 * 1024 * 1024) return false; // >2MB 视为非文本，直接跳过
            const QFileInfo info(path);
            const QString ext = info.suffix().toLower();
            if (!ext.isEmpty()) return exts.contains(ext);
            const QString name = info.fileName().toLower();
            return namesWithoutExt.contains(name);
        };
        if (!rootDir.exists())
        {
            const QString msg = QString("Work directory not found: %1").arg(root);
            sendPushMessage(QString("search_content ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("search_content ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }
        struct FileSearchResult
        {
            QString path;
            QVector<QPair<int, QString>> matches;
        };
        QVector<FileSearchResult> fileResults;
        QHash<QString, int> fileIndexByPath;
        const int kMaxMatches = 300;
        const int kMaxOutputBytes = 220 * 1024;
        int totalMatches = 0;
        bool matchLimitHit = false;
        auto sanitizeLine = [](QString text) -> QString
        {
            text.replace('\t', ' ');
            text.replace('\r', ' ');
            if (text.size() > 400) text = text.left(400) + "...";
            return text;
        };
        auto ensureFileResult = [&](const QString &relativePath) -> FileSearchResult &
        {
            auto it = fileIndexByPath.find(relativePath);
            if (it != fileIndexByPath.end())
            {
                return fileResults[*it];
            }
            FileSearchResult result;
            result.path = relativePath;
            fileResults.append(result);
            const int newIndex = fileResults.size() - 1;
            fileIndexByPath.insert(relativePath, newIndex);
            return fileResults[newIndex];
        };
        QDirIterator it(rootDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (it.hasNext())
        {
            if (shouldAbort(invocation)) break;
            const QString absolutePath = it.next();
            const QFileInfo fi(absolutePath);
            const QString relCheck = rootDir.relativeFilePath(fi.absoluteFilePath());
            if (relCheck.startsWith("..")) continue;
            if (!isLikelyText(absolutePath, fi.size())) continue;
            QFile file(absolutePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QTextStream stream(&file);
            stream.setCodec("UTF-8");
            QStringList lines;
            while (!stream.atEnd())
            {
                if (shouldAbort(invocation)) break;
                lines.append(stream.readLine());
            }
            file.close();
            if (shouldAbort(invocation)) break;
            const QString relativePath = rootDir.relativeFilePath(fi.absoluteFilePath());
            for (int i = 0; i < lines.size(); ++i)
            {
                if (shouldAbort(invocation)) break;
                const QString &line = lines.at(i);
                if (!line.contains(query, Qt::CaseInsensitive)) continue;
                FileSearchResult &fileResult = ensureFileResult(relativePath);
                fileResult.matches.append(QPair<int, QString>(i + 1, sanitizeLine(line)));
                ++totalMatches;
                if (totalMatches >= kMaxMatches)
                {
                    matchLimitHit = true;
                    break;
                }
            }
            if (matchLimitHit) break;
        }
        if (shouldAbort(invocation)) return;
        if (totalMatches == 0)
        {
            const QString msg = QString("No matches.");
            sendPushMessage(QString("search_content ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("search_content ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }
        QStringList outputLines;
        int byteCount = 0;
        bool outputLimitHit = false;
        auto tryAppendLine = [&](const QString &line) -> bool
        {
            const QByteArray utf8 = line.toUtf8();
            const int needed = utf8.size() + 1;
            if (byteCount + needed > kMaxOutputBytes)
            {
                return false;
            }
            outputLines.append(line);
            byteCount += needed;
            return true;
        };
        const int fileCount = fileResults.size();
        if (!outputLimitHit)
        {
            const QString summary =
                QString("Found %1 match%2 across %3 file%4.")
                    .arg(totalMatches)
                    .arg(totalMatches == 1 ? "" : "es")
                    .arg(fileCount)
                    .arg(fileCount == 1 ? "" : "s");
            if (!tryAppendLine(summary)) outputLimitHit = true;
        }
        if (!outputLimitHit)
        {
            if (!tryAppendLine(QString("Search root: %1").arg(root))) outputLimitHit = true;
        }
        for (const FileSearchResult &fileResult : fileResults)
        {
            if (outputLimitHit) break;
            if (fileResult.matches.isEmpty()) continue;
            if (!outputLimitHit)
            {
                if (!tryAppendLine(QString())) { outputLimitHit = true; break; }
            }
            if (!tryAppendLine(fileResult.path)) { outputLimitHit = true; break; }
            for (const auto &match : fileResult.matches)
            {
                const QString line =
                    QString("%1: %2").arg(match.first).arg(match.second);
                if (!tryAppendLine(line))
                {
                    outputLimitHit = true;
                    break;
                }
            }
        }
        const bool truncated = matchLimitHit || outputLimitHit;
        if (truncated)
        {
            outputLines.append(QString("[Results truncated. Refine your search to narrow matches.]"));
        }
        const QString result = outputLines.join("\n");
        const QString prefix = QString("search_content ") + jtr("return") + "\n";
        sendStateMessage("tool:" + prefix + result, TOOL_SIGNAL);
        sendPushMessage(prefix + result);
    }
    else if (tools_name.contains("mcp_tools_list")) // 查询mcp可用工具
    {
        return;
    }
    else if (tools_name.contains("@")) // 如果工具名包含@则假设他是mcp工具
    {
        return;
    }
    //----------------------没有该工具------------------
    else
    {
        sendPushMessage(jtr("not load tool"));
    }
}

void xTool::startExecuteCommand(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    const QString content = invocation->commandContent;
    sendStateMessage("tool:" + QString("execute_command(") + content + ")");
    const QString work = resolveWorkRoot();
    ensureWorkdirExists(work);
    const bool useDocker = dockerSandboxEnabled();
    QString dockerError;
    if (useDocker && !ensureDockerSandboxReady(&dockerError))
    {
        sendPushMessage(QStringLiteral("execute_command failed: ") + dockerError);
        sendStateMessage("tool:" + QStringLiteral("execute_command docker error\n") + dockerError, WRONG_SIGNAL);
        finishInvocation(invocation);
        return;
    }
    const QString effectiveWorkdir = dockerWorkdirOrFallback(work);
    auto process = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef __linux__
    env.insert("PATH", "/usr/local/bin:/usr/bin:/bin:" + env.value("PATH"));
#endif
    process->setProcessEnvironment(env);
    process->setWorkingDirectory(work);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    activeCommandProcess_ = process;
    activeCommandInvocation_ = invocation;
    activeCommandInterrupted_ = false;
    invocation->aggregatedOutput.clear();
    invocation->workingDirectory = effectiveWorkdir;
    emit tool2ui_terminalCommandStarted(content, effectiveWorkdir);
    QObject::connect(process, &QProcess::readyReadStandardOutput, this, [this, process, invocation]()
                     { handleCommandStdout(invocation, process, false); });
    QObject::connect(process, &QProcess::readyReadStandardError, this, [this, process, invocation]()
                     { handleCommandStdout(invocation, process, true); });
    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, process, invocation](int exitCode, QProcess::ExitStatus status)
                     {
        handleCommandFinished(invocation, process, exitCode, status);
        process->deleteLater(); });
    QObject::connect(process, &QProcess::errorOccurred, this, [this, process, invocation](QProcess::ProcessError error)
                     {
        if (error == QProcess::FailedToStart)
        {
            const QString err = process->errorString();
            emit tool2ui_terminalStderr(err + "\n");
            handleCommandFinished(invocation, process, -1, QProcess::CrashExit);
            process->deleteLater();
        } });
    QString program;
    QStringList args;
    if (useDocker)
    {
#ifdef _WIN32
        program = QStringLiteral("docker.exe");
#else
        program = QStringLiteral("docker");
#endif
        args << QStringLiteral("exec") << QStringLiteral("-i") << dockerSandbox_->containerName()
             << QStringLiteral("/bin/sh") << QStringLiteral("-lc") << content;
    }
    else
    {
#ifdef _WIN32
        program = QStringLiteral("cmd.exe");
        args << QStringLiteral("/c") << content;
#else
        program = QStringLiteral("/bin/sh");
        args << QStringLiteral("-lc") << content;
#endif
    }
    process->start(program, args);
    if (process->state() == QProcess::NotRunning && process->error() == QProcess::FailedToStart)
    {
        const QString err = process->errorString();
        emit tool2ui_terminalStderr(err + "\n");
        handleCommandFinished(invocation, process, -1, QProcess::CrashExit);
        process->deleteLater();
    }
}

void xTool::handleCommandStdout(const ToolInvocationPtr &invocation, QProcess *process, bool isError)
{
    if (!invocation || !process) return;
    const QByteArray data = isError ? process->readAllStandardError() : process->readAllStandardOutput();
    if (data.isEmpty()) return;
#ifdef Q_OS_WIN
    const QString text = QString::fromLocal8Bit(data);
#else
    const QString text = QString::fromUtf8(data);
#endif
    invocation->aggregatedOutput += text;
    if (isError)
        emit tool2ui_terminalStderr(text);
    else
        emit tool2ui_terminalStdout(text);
}

void xTool::handleCommandFinished(const ToolInvocationPtr &invocation, QProcess *process, int exitCode, QProcess::ExitStatus status)
{
    if (!process) return;
    if (process != activeCommandProcess_) return;
    const bool interrupted = activeCommandInterrupted_ || status == QProcess::CrashExit || (invocation && invocation->cancelled.load(std::memory_order_acquire));
    activeCommandProcess_ = nullptr;
    activeCommandInvocation_.reset();
    activeCommandInterrupted_ = false;
    emit tool2ui_terminalCommandFinished(exitCode, interrupted);
    if (invocation && !invocation->cancelled.load(std::memory_order_acquire))
    {
        QString finalOutput = invocation->aggregatedOutput;
        if (finalOutput.isEmpty())
        {
            finalOutput = interrupted ? QStringLiteral("[command interrupted]") : QStringLiteral("[no output]");
        }
        else if (interrupted)
        {
            if (!finalOutput.endsWith('\n')) finalOutput.append('\n');
            finalOutput += QStringLiteral("[command interrupted]");
        }
        sendStateMessage("tool:" + QString("execute_command ") + "\n" + finalOutput, TOOL_SIGNAL);
        sendPushMessage(QString("execute_command ") + "\n" + finalOutput);
        qDebug() << QString("execute_command ") + "\n" + finalOutput;
    }
    finishInvocation(invocation);
}

void xTool::finishInvocation(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    postFinishCleanup(invocation);
    clearActiveInvocation(invocation);
}

bool xTool::shouldAbort(const ToolInvocationPtr &invocation) const
{
    return !invocation || invocation->cancelled.load(std::memory_order_acquire);
}

void xTool::postFinishCleanup(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    auto eraseMatching = [&](auto &map)
    {
        for (auto it = map.begin(); it != map.end();)
        {
            auto ptr = it->second.lock();
            if (!ptr || ptr == invocation)
                it = map.erase(it);
            else
                ++it;
        }
    };
    eraseMatching(pendingDrawInvocations_);
    eraseMatching(pendingMcpInvocations_);
    eraseMatching(pendingMcpListInvocations_);
    if (activeCommandInvocation_ == invocation) activeCommandInvocation_.reset();
}

void xTool::handleStableDiffusion(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    pendingDrawInvocations_[invocation->id] = invocation;
    QString prompt = QString::fromStdString(get_string_safely(invocation->args, "prompt"));
    emit tool2expend_draw(invocation->id, prompt);
}

void xTool::handleMcpToolList(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    pendingMcpListInvocations_[invocation->id] = invocation;
    emit tool2mcp_toollist(invocation->id);
}

void xTool::handleMcpToolCall(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    pendingMcpInvocations_[invocation->id] = invocation;
    QString toolArgs = QString::fromStdString(invocation->args.dump());
    emit tool2mcp_toolcall(invocation->id, invocation->name, toolArgs);
}

QString xTool::resolveWorkRoot() const
{
    if (!workDirRoot.isEmpty()) return workDirRoot;
    return QDir::cleanPath(applicationDirPath + "/EVA_WORK");
}

bool xTool::dockerSandboxEnabled() const
{
    return dockerSandbox_ && dockerConfig_.enabled && !dockerConfig_.hostWorkdir.isEmpty();
}

bool xTool::ensureDockerSandboxReady(QString *errorMessage)
{
    if (!dockerSandboxEnabled()) return false;
    if (!dockerSandbox_->prepare(errorMessage))
    {
        return false;
    }
    return true;
}

QString xTool::dockerWorkdirOrFallback(const QString &hostWorkdir) const
{
    if (dockerSandboxEnabled() && dockerSandbox_->sandboxEnabled())
    {
        return dockerSandbox_->containerWorkdir();
    }
    return hostWorkdir;
}

void xTool::ensureWorkdirExists(const QString &work) const
{
    if (work.isEmpty()) return;
    QDir dir(work);
    if (!dir.exists()) dir.mkpath(QStringLiteral("."));
}

void xTool::cancelActiveTool()
{
    ToolInvocationPtr invocation;
    {
        std::lock_guard<std::mutex> lock(invocationMutex_);
        invocation = activeInvocation_;
    }
    if (invocation) invocation->cancelled.store(true, std::memory_order_release);
    auto mark = [](auto &map)
    {
        for (auto &entry : map)
        {
            if (auto inv = entry.second.lock())
            {
                inv->cancelled.store(true, std::memory_order_release);
            }
        }
    };
    mark(pendingDrawInvocations_);
    mark(pendingMcpInvocations_);
    mark(pendingMcpListInvocations_);
    cancelExecuteCommand();
}

// 创建临时文件夹EVA_TEMP

bool xTool::createTempDirectory(const QString &path)
{
    QDir dir;
    // 检查路径是否存在
    if (dir.exists(path))
    {
        return false;
    }
    else
    {
        // 尝试创建目录
        if (dir.mkpath(path))
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

// 查询计算词向量和计算相似度，返回匹配的文本段

QString xTool::embedding_query_process(QString query_str)
{
    QString knowledge_result;
    //---------------计算查询文本段的词向量-------------------------
    ipAddress = getFirstNonLoopbackIPv4Address();
    QEventLoop loop; // 进入事件循环，等待回复
    QNetworkAccessManager manager;
    // 设置请求的端点 URL
    QString embedding_server_api = "http://" + QString(DEFAULT_EMBEDDING_IP) + ":" + DEFAULT_EMBEDDING_PORT + DEFAULT_EMBEDDING_API;
    QNetworkRequest request(QUrl(embedding_server_api + QString(""))); // 加一个""是为了避免语法解析错误
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QString api_key = "Bearer " + QString("sjxx");
    request.setRawHeader("Authorization", api_key.toUtf8());
    // 构造请求的数据体
    QJsonObject json;
    json.insert("model", "defult");
    json.insert("encoding_format", "float");
    json.insert("input", query_str);
    QJsonDocument doc(json);
    QByteArray data = doc.toJson();
    // POST 请求
    QNetworkReply *reply = manager.post(request, data);
    // 处理响应
    QObject::connect(reply, &QNetworkReply::readyRead, [&]()
                     {
                         QString jsonString = reply->readAll();
                         QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8()); // 使用QJsonDocument解析JSON数据
                         QJsonObject rootObject = document.object();
                         // 遍历"data"数组,获取嵌入向量结构体的嵌入向量
                         QJsonArray dataArray = rootObject["data"].toArray();
                         QString vector_str = "[";
                         for (int i = 0; i < dataArray.size(); ++i)
                         {
                             QJsonObject dataObj = dataArray[i].toObject();
                             // 检查"data"对象中是否存在"embedding"
                             if (dataObj.contains("embedding"))
                             {
                                 QJsonArray embeddingArray = dataObj["embedding"].toArray();
                                 query_embedding_vector.value.resize(embedding_server_dim);
                                 // 处理"embedding"数组
                                 for (int j = 0; j < embeddingArray.size(); ++j)
                                 {
                                     query_embedding_vector.value[j] = embeddingArray[j].toDouble();
                                     vector_str += QString::number(query_embedding_vector.value[j], 'f', 4) + ", ";
                                 }
                             }
                         }
                         vector_str += "]";
                         sendStateMessage("tool:" + jtr("The query text segment has been embedded") + jtr("dimension") + ": " + QString::number(query_embedding_vector.value.size()) + " " + jtr("word vector") + ": " + vector_str, USUAL_SIGNAL); });
    // 完成
    QObject::connect(reply, &QNetworkReply::finished, [&]()
                     {
                         if (reply->error() == QNetworkReply::NoError)
                         {
                             // 请求完成，所有数据都已正常接收
                             //------------------------计算余弦相似度---------------------------
                             // A向量点积B向量除以(A模乘B模)
                             std::vector<std::pair<int, double>> score;
                             score = similar_indices(query_embedding_vector.value, Embedding_DB); // 计算查询文本段和所有嵌入文本段之间的相似度
                             if (score.size() > 0)
                             {
                                 knowledge_result += jtr("The three text segments with the highest similarity") + DEFAULT_SPLITER;
                             }
                              size_t limit = std::min<size_t>(static_cast<size_t>(embedding_server_resultnumb), score.size());
                              for (size_t i = 0; i < limit; ++i)
                             {
                                 knowledge_result += QString::number(score[i].first + 1) + jtr("Number text segment similarity") + ": " + QString::number(score[i].second);
                                 knowledge_result += " " + jtr("content") + DEFAULT_SPLITER + Embedding_DB.at(score[i].first).chunk + "\n";
                             }
                             if (score.size() > 0)
                             {
                                 knowledge_result += jtr("Based on this information, reply to the user's previous questions");
                             }
                         }
                         else
                         {
                             // 请求出错
                             sendStateMessage("tool:" + jtr("Request error") + " " + reply->error(), WRONG_SIGNAL);
                             knowledge_result += jtr("Request error") + " " + reply->error();
                         }
                         reply->abort(); // 终止
                         reply->deleteLater(); });
    // 回复完成时退出事件循环
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // 进入事件循环
    loop.exec();
    return knowledge_result;
}

// 获取ipv4地址

QString xTool::getFirstNonLoopbackIPv4Address()
{
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    for (int i = 0; i < list.count(); i++)
    {
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol)
        {
            return list[i].toString();
        }
    }
    return QString();
}

// 计算两个向量的余弦相似度，A向量点积B向量除以(A模乘B模)

double xTool::cosine_similarity_1024(const std::vector<double> &a, const std::vector<double> &b)
{
    // 确保两个向量维度相同
    if (a.size() != b.size())
    {
        throw std::invalid_argument("Vectors must be of the same length");
    }
    double dot_product = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return dot_product / (sqrt(norm_a) * sqrt(norm_b));
}

// 计算user_vector与Embedding_DB中每个元素的相似度，并返回得分最高的3个索引

std::vector<std::pair<int, double>> xTool::similar_indices(const std::vector<double> &user_vector, const QVector<Embedding_vector> &embedding_DB)
{
    std::vector<std::pair<int, double>> scores; // 存储每个索引和其相应的相似度得分
    // 计算相似度得分
    for (const auto &emb : embedding_DB)
    {
        double sim = cosine_similarity_1024(user_vector, emb.value);
        scores.emplace_back(emb.index, sim);
    }
    // 根据相似度得分排序（降序）
    std::sort(scores.begin(), scores.end(), [](const std::pair<int, double> &a, const std::pair<int, double> &b)
              { return a.second > b.second; });
    return scores;
}

void xTool::recv_embeddingdb(QVector<Embedding_vector> Embedding_DB_)
{
    Embedding_DB.clear();
    Embedding_DB = Embedding_DB_;
    sendStateMessage("tool:" + jtr("Received embedded text segment data"), USUAL_SIGNAL);
}

// 传递嵌入结果返回个数

void xTool::recv_embedding_resultnumb(int resultnumb)
{
    embedding_server_resultnumb = resultnumb;
}

// 接收图像绘制完成信号

void xTool::recv_drawover(quint64 invocationId, QString result_, bool ok_)
{
    auto it = pendingDrawInvocations_.find(invocationId);
    if (it == pendingDrawInvocations_.end()) return;
    auto invocation = it->second.lock();
    pendingDrawInvocations_.erase(it);
    if (!invocation) return;
    if (invocation->cancelled.load(std::memory_order_acquire))
    {
        finishInvocation(invocation);
        return;
    }
    if (!ok_)
    {
        sendPushMessage(result_);
        finishInvocation(invocation);
        return;
    }
    sendStateMessage("tool:" + QString("stablediffusion ") + jtr("return") + "\n" + "<ylsdamxssjxxdd:showdraw>" + result_, TOOL_SIGNAL);
    sendPushMessage("<ylsdamxssjxxdd:showdraw>" + result_);
    finishInvocation(invocation);
}

// 传递控制完成结果

void xTool::tool2ui_controller_over(QString result)
{
    sendStateMessage("tool:" + QString("controller ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
    sendPushMessage(QString("controller ") + jtr("return") + "\n" + result);
}

void xTool::recv_language(int language_flag_)
{
    language_flag = language_flag_;
}

// 根据language.json和language_flag中找到对应的文字

QString xTool::jtr(QString customstr)
{
    return wordsObj[customstr].toArray()[language_flag].toString();
}

void xTool::recv_callTool_over(quint64 invocationId, QString result)
{
    auto it = pendingMcpInvocations_.find(invocationId);
    if (it == pendingMcpInvocations_.end()) return;
    auto invocation = it->second.lock();
    pendingMcpInvocations_.erase(it);
    if (!invocation) return;
    if (invocation->cancelled.load(std::memory_order_acquire))
    {
        finishInvocation(invocation);
        return;
    }
    if (result.isEmpty())
    {
        sendPushMessage(jtr("not load tool"));
    }
    else
    {
        sendStateMessage("tool:" + QString("mcp ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
        sendPushMessage(QString("mcp ") + jtr("return") + "\n" + result);
    }
    finishInvocation(invocation);
}

// mcp列出工具完毕

void xTool::recv_calllist_over(quint64 invocationId)
{
    auto it = pendingMcpListInvocations_.find(invocationId);
    if (it == pendingMcpListInvocations_.end()) return;
    auto invocation = it->second.lock();
    pendingMcpListInvocations_.erase(it);
    if (!invocation) return;
    if (invocation->cancelled.load(std::memory_order_acquire))
    {
        finishInvocation(invocation);
        return;
    }
    QString result = mcpToolParser(MCP_TOOLS_INFO_ALL);
    sendStateMessage("tool:" + QString("mcp_tool_list ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
    sendPushMessage(QString("mcp_tool_list ") + jtr("return") + "\n" + result);
    finishInvocation(invocation);
}

// 解析出所有mcp工具信息拼接为一段文本

QString xTool::mcpToolParser(mcp::json toolsinfo)
{
    QString result = "";
    for (const auto &tool : toolsinfo)
    {
        TOOLS_INFO mcp_tools_info(
            QString::fromStdString(tool["service"].get<std::string>() + "@" + tool["name"].get<std::string>()), // 工具名
            QString::fromStdString(tool["description"]), // 工具描述
            QString::fromStdString(tool["inputSchema"].dump()) // 参数结构
        );
        result += mcp_tools_info.text + "\n";
    }
    return result;
}

// 执行行动序列

void xTool::excute_sequence(std::vector<std::string> build_in_tool_arg)
{
    // build_in_tool_arg的样式为 ["left_down(100,200)", "time_span(0.1)", "left_up()", "time_span(0.5)", "left_down(100,200)", "time_span(0.1)", "left_up()"]
    for (const auto &action : build_in_tool_arg)
    {
        if (tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire)) return;
        // 1. 提取函数名
        size_t pos_start = action.find('(');
        size_t pos_end = action.find(')');
        // 检查括号是否存在
        if (pos_start == std::string::npos || pos_end == std::string::npos)
        {
            // 错误处理：跳过无效格式
            continue;
        }
        // 函数名（左括号前的部分）
        std::string func_name = action.substr(0, pos_start);
        // 2. 提取参数字符串（括号内的内容）
        std::string args_str = action.substr(pos_start + 1, pos_end - pos_start - 1);
        std::vector<std::string> args_list;
        // 3. 分割参数（逗号分隔）
        if (!args_str.empty())
        {
            std::istringstream iss(args_str);
            std::string arg;
            while (std::getline(iss, arg, ','))
            {
                // 可选：去除参数两端的空格
                auto trim = [](std::string &s)
                {
                    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                                    { return !std::isspace(ch); }));
                    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                                         { return !std::isspace(ch); })
                                .base(),
                            s.end());
                };
                trim(arg); // 调用 trim 函数
                args_list.push_back(arg);
            }
        }
        // // 打印提取结果（实际执行时替换为具体操作）
        // std::cout << "Function: " << func_name << ", Args: ";
        // for (const auto& a : args_list) {
        //     std::cout << a << " ";
        // }
        using namespace platform;
        if (func_name == "left_down" && args_list.size() == 2)
        {
            std::cout << "left_down "
                      << " x " << args_list[0] << " y " << args_list[1];
            leftDown(std::stoi(args_list[0]), std::stoi(args_list[1]));
        }
        else if (func_name == "left_up")
        {
            std::cout << "leftUp ";
            leftUp();
        }
        else if (func_name == "right_down" && args_list.size() == 2)
        {
            std::cout << "right_down "
                      << " x " << args_list[0] << " y " << args_list[1];
            rightDown(std::stoi(args_list[0]), std::stoi(args_list[1]));
        }
        else if (func_name == "right_up")
        {
            std::cout << "right_up ";
            rightUp();
        }
        else if (func_name == "move" && args_list.size() == 3)
        {
            int ex = std::stoi(args_list[0]);
            int ey = std::stoi(args_list[1]);
            float sec = std::stof(args_list[2]);
            std::cout << "move "
                      << " x " << args_list[0] << " y " << args_list[1] << " t " << args_list[2];
#ifdef _WIN32
            POINT p;
            GetCursorPos(&p);
            int sx = p.x, sy = p.y;
#else
            Window root_r, child_r;
            int sx, sy;
            int rx, ry;
            unsigned int mask;
            XQueryPointer(dsp(), DefaultRootWindow(dsp()), &root_r, &child_r,
                          &rx, &ry, &sx, &sy, &mask);
#endif
            const int fps = 60;
            int steps = std::max(1, int(sec * fps));
            for (int i = 1; i <= steps; ++i)
            {
                if (tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire)) return;
                double k = double(i) / steps;
                moveCursor(int(sx + k * (ex - sx)), int(sy + k * (ey - sy)));
                msleep(unsigned(sec * 1000 / steps));
            }
        }
        else if (func_name == "keyboard" && args_list.size() == 1)
        {
            std::string keys = args_list[0];
            // 去掉所有单引号和双引号
            keys.erase(std::remove(keys.begin(), keys.end(), '\''), keys.end());
            keys.erase(std::remove(keys.begin(), keys.end(), '\"'), keys.end());
            std::cout << "keyboard " << keys;
            if (keys.find('+') != std::string::npos)
            { // 组合键
                sendKeyCombo(split(keys, '+'));
            }
            else
            {
                sendKeyCombo({keys});
            }
        }
        else if (func_name == "time_span" && args_list.size() == 1)
        {
            if (tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire)) return;
            std::cout << "time_span "
                      << " t " << args_list[0];
            msleep(unsigned(std::stof(args_list[0]) * 1000));
        }
        std::cout << std::endl;
    }
    msleep(100); // 强制等待0.1s，让电脑显示出最终结果
}
