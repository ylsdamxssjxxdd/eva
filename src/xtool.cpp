#include "xtool.h"

#include "utils/processrunner.h"
#include "utils/flowtracer.h"

#include <QDirIterator>
#include <QEventLoop>
#include <QHash>
#include <QPair>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <limits>

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

struct LineRange
{
    int start = 1;
    int end = std::numeric_limits<int>::max();
};

struct FileReadSpec
{
    QString path;
    QVector<LineRange> ranges;
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

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace('\'', "'\\''");
    return QStringLiteral("'") + escaped + QStringLiteral("'");
}

QString normalizeUnixPath(const QString &path)
{
    QString normalized = QDir::fromNativeSeparators(path);
    if (normalized.isEmpty()) return QStringLiteral(".");
    const bool absolute = normalized.startsWith('/');
    QStringList parts = normalized.split('/', Qt::SkipEmptyParts);
    QStringList stack;
    for (const QString &part : parts)
    {
        if (part == QLatin1String(".")) continue;
        if (part == QLatin1String(".."))
        {
            if (!stack.isEmpty()) stack.removeLast();
            continue;
        }
        stack.append(part);
    }
    QString result = stack.join('/');
    if (result.isEmpty())
    {
        return absolute ? QStringLiteral("/") : QStringLiteral(".");
    }
    if (absolute) result.prepend('/');
    return result;
}

bool parseFileSpecsFromArgs(const mcp::json &args, QVector<FileReadSpec> &out, QString &error)
{
    out.clear();
    const int kMaxFiles = 5;
    const int kMaxRangeSpan = 400;
    const int kDefaultSpan = 200;

    auto clampRange = [&](LineRange &range) {
        if (range.start < 1) range.start = 1;
        if (range.end < range.start) range.end = range.start;
        if (range.end - range.start + 1 > kMaxRangeSpan) range.end = range.start + kMaxRangeSpan - 1;
    };

    auto parseRanges = [&](const mcp::json &item, FileReadSpec &spec) {
        if (item.contains("line_ranges") && item["line_ranges"].is_array())
        {
            for (const auto &rangeVal : item["line_ranges"])
            {
                if (!rangeVal.is_array() || rangeVal.size() < 2) continue;
                LineRange r;
                try
                {
                    r.start = std::max(1, rangeVal.at(0).get<int>());
                    r.end = std::max(1, rangeVal.at(1).get<int>());
                }
                catch (...)
                {
                    continue;
                }
                clampRange(r);
                spec.ranges.append(r);
            }
        }
        int startLine = get_int_safely(item, "start_line", -1);
        int endLine = get_int_safely(item, "end_line", -1);
        if (startLine > 0)
        {
            LineRange r;
            r.start = startLine;
            r.end = endLine > 0 ? endLine : startLine + kDefaultSpan - 1;
            clampRange(r);
            spec.ranges.append(r);
        }
    };

    if (args.contains("files") && args["files"].is_array())
    {
        for (const auto &fileVal : args["files"])
        {
            if (!fileVal.is_object()) continue;
            const QString path = QString::fromStdString(get_string_safely(fileVal, "path")).trimmed();
            if (path.isEmpty()) continue;
            FileReadSpec spec;
            spec.path = path;
            parseRanges(fileVal, spec);
            if (spec.ranges.isEmpty())
            {
                LineRange def;
                def.end = def.start + kDefaultSpan - 1;
                spec.ranges.append(def);
            }
            out.append(spec);
            if (out.size() >= kMaxFiles) break;
        }
    }

    if (out.isEmpty())
    {
        const QString legacyPath = QString::fromStdString(get_string_safely(args, "path")).trimmed();
        if (!legacyPath.isEmpty())
        {
            FileReadSpec spec;
            spec.path = legacyPath;
            LineRange range;
            int start = get_int_safely(args, "start_line", 1);
            int end = get_int_safely(args, "end_line", start + kDefaultSpan - 1);
            range.start = start;
            range.end = end;
            clampRange(range);
            spec.ranges.append(range);
            out.append(spec);
        }
    }

    if (out.isEmpty())
    {
        error = QStringLiteral("No file paths provided.");
        return false;
    }

    if (out.size() > kMaxFiles)
    {
        out = out.mid(0, kMaxFiles);
    }
    return true;
}
} // namespace

struct xTool::ToolInvocation
{
    quint64 id = 0;
    quint64 turnId = 0;
    QString name;
    mcp::json call;
    mcp::json args;
    std::atomic<bool> cancelled{false};
    QString commandContent;
    QString aggregatedOutput;
    QString workingDirectory;
};

thread_local xTool::ToolInvocation *xTool::tlsCurrentInvocation_ = nullptr;

QString xTool::flowTag(quint64 turnId) const
{
    Q_UNUSED(turnId);
    return QString();
}

void xTool::sendStateMessage(const QString &message, SIGNAL_STATE state)
{
    quint64 turnId = activeTurnId_.load(std::memory_order_relaxed);
    if (tlsCurrentInvocation_) turnId = tlsCurrentInvocation_->turnId;
    if (tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire)) return;
    const QString line = flowTag(turnId) + message;
    emit tool2ui_state(clampToolMessage(line), state);
}

void xTool::sendPushMessage(const QString &message)
{
    quint64 turnId = activeTurnId_.load(std::memory_order_relaxed);
    if (tlsCurrentInvocation_) turnId = tlsCurrentInvocation_->turnId;
    if (tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire)) return;
    const QString line = flowTag(turnId) + message;
    emit tool2ui_pushover(clampToolMessage(line));
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
    invocation->turnId = activeTurnId_.load(std::memory_order_relaxed);
    invocation->call = std::move(tools_call);
    invocation->name = QString::fromStdString(get_string_safely(invocation->call, "name"));
    invocation->args = get_json_object_safely(invocation->call, "arguments");
    setActiveInvocation(invocation);
    FlowTracer::log(FlowChannel::Tool,
                    QStringLiteral("tool:create id=%1 name=%2").arg(invocation->id).arg(invocation->name),
                    invocation->turnId);
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

void xTool::recv_dockerConfig(DockerSandbox::Config config)
{
    dockerConfig_ = config;
    if (dockerSandbox_) dockerSandbox_->applyConfig(dockerConfig_);
}

void xTool::recv_turn(quint64 turnId)
{
    activeTurnId_.store(turnId, std::memory_order_relaxed);
}

void xTool::shutdownDockerSandbox()
{
    if (!dockerSandbox_) return;
    if (!dockerConfig_.enabled) return;
    DockerSandbox::Config cfg = dockerConfig_;
    cfg.enabled = false;
    dockerConfig_ = cfg;
    dockerSandbox_->applyConfig(cfg);
    qDebug() << "docker sandbox stopped before exit";
    emit dockerShutdownCompleted();
}

void xTool::fixDockerContainerMount(const QString &containerName)
{
    if (!dockerSandbox_) return;
    if (dockerConfig_.target != DockerSandbox::TargetType::Container)
    {
        sendStateMessage("tool:docker sandbox fix ignored (not in container mode)");
        return;
    }
    if (!containerName.trimmed().isEmpty())
    {
        dockerConfig_.containerName = containerName.trimmed();
    }
    QString error;
    if (!dockerSandbox_->recreateContainerWithRequiredMount(&error))
    {
        if (error.isEmpty()) error = QStringLiteral("unknown docker error");
        sendStateMessage("tool:" + QStringLiteral("docker sandbox fix failed -> %1").arg(error), WRONG_SIGNAL);
        return;
    }
    dockerSandbox_->applyConfig(dockerConfig_);
    sendStateMessage("tool:" + QStringLiteral("docker sandbox container remounted"), SUCCESS_SIGNAL);
}

void xTool::onDockerStatusChanged(const DockerSandboxStatus &status)
{
    emit tool2ui_dockerStatusChanged(status);
    const auto targetDisplay = [](const DockerSandboxStatus &stat) -> QString {
        if (stat.usingExistingContainer)
        {
            const QString container = stat.containerName.trimmed();
            return container.isEmpty() ? QStringLiteral("none") : container;
        }
        const QString image = stat.image.trimmed();
        return image.isEmpty() ? QStringLiteral("ubuntu:latest") : image;
    };
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
    }
    else if (status.ready)
    {
        const QString hostDir = status.hostWorkdir.isEmpty() ? resolveWorkRoot() : status.hostWorkdir;
        const QString hostDisplay = QDir::toNativeSeparators(hostDir);
        const QString containerDir = status.containerWorkdir.isEmpty() ? DockerSandbox::defaultContainerWorkdir() : status.containerWorkdir;
        message = QStringLiteral("docker sandbox ready (%1)\nhost %2 -> container %3")
                      .arg(targetDisplay(status),
                           hostDisplay,
                           containerDir);
        const QString skillsTarget = status.skillsMountPoint.isEmpty() ? DockerSandbox::skillsMountPoint()
                                                                       : status.skillsMountPoint;
        if (!skillsTarget.isEmpty())
        {
            message.append(QStringLiteral("\nskills available at %1").arg(skillsTarget));
        }
        level = SUCCESS_SIGNAL;
    }
    else
    {
        message = QStringLiteral("docker sandbox preparing (%1)")
                      .arg(targetDisplay(status));
    }
    sendStateMessage("tool:" + message, level);
    if (!status.infoMessage.isEmpty())
    {
        sendStateMessage("tool:" + status.infoMessage, SIGNAL_SIGNAL);
    }
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
    FlowTracer::log(FlowChannel::Tool, QStringLiteral("tool:command cancel"), activeTurnId_.load(std::memory_order_relaxed));
}

void xTool::startWorkerInvocation(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    FlowTracer::log(FlowChannel::Tool,
                    QStringLiteral("tool:start async name=%1").arg(invocation->name),
                    invocation->turnId);
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
    FlowTracer::log(FlowChannel::Tool,
                    QStringLiteral("tool:dispatch name=%1").arg(invocation->name),
                    invocation->turnId);
    sendStateMessage(QStringLiteral("tool:start %1").arg(invocation->name), SIGNAL_SIGNAL);
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
    //----------------------桌面控制器------------------
    else if (tools_name == "controller")
    {
        // -----------------------------------------------------------------------------
        // 桌面控制器（新版）：bbox + action + description
        // - bbox 坐标：以“最新截图”的像素坐标为准（截图已缩放到归一化尺寸），原点左上角，x 向右，y 向下
        // - 执行坐标：取 bbox 中心点，并按 controllerNormX_/Y_ 映射回真实屏幕坐标
        // - UI 提示：执行前通过 tool2ui_controller_hint 在屏幕上绘制中心点 + 80x80 框 + 描述
        // -----------------------------------------------------------------------------
        const bool looksLikeNewSchema = tools_args_.is_object() && tools_args_.contains("action") && tools_args_.contains("description") &&
                                        tools_args_.contains("bbox");
        if (looksLikeNewSchema)
        {
            auto cancelled = [&]() -> bool {
                return tlsCurrentInvocation_ && tlsCurrentInvocation_->cancelled.load(std::memory_order_acquire);
            };

            struct BBox
            {
                int x1 = 0;
                int y1 = 0;
                int x2 = 0;
                int y2 = 0;
            };

            auto parseIntJsonRounded = [](const mcp::json &val, int &out) -> bool {
                try
                {
                    if (val.is_number_integer())
                    {
                        out = val.get<int>();
                        return true;
                    }
                    if (val.is_number_float())
                    {
                        const double v = val.get<double>();
                        out = static_cast<int>(v >= 0.0 ? (v + 0.5) : (v - 0.5));
                        return true;
                    }
                    if (val.is_string())
                    {
                        const std::string s = val.get<std::string>();
                        size_t idx = 0;
                        const double v = std::stod(s, &idx);
                        while (idx < s.size() && std::isspace(static_cast<unsigned char>(s[idx]))) ++idx;
                        if (idx != s.size()) return false;
                        out = static_cast<int>(v >= 0.0 ? (v + 0.5) : (v - 0.5));
                        return true;
                    }
                }
                catch (...)
                {
                }
                return false;
            };

            auto parseBBox = [&](const mcp::json &obj, const char *key, BBox &out, QString &error) -> bool {
                if (!obj.contains(key))
                {
                    error = QStringLiteral("missing %1").arg(QString::fromLatin1(key));
                    return false;
                }
                const auto &v = obj.at(key);
                if (!v.is_array() || v.size() != 4)
                {
                    error = QStringLiteral("%1 must be [x1,y1,x2,y2]").arg(QString::fromLatin1(key));
                    return false;
                }
                int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                if (!parseIntJsonRounded(v.at(0), x1) || !parseIntJsonRounded(v.at(1), y1) || !parseIntJsonRounded(v.at(2), x2) ||
                    !parseIntJsonRounded(v.at(3), y2))
                {
                    error = QStringLiteral("%1 contains non-numeric values").arg(QString::fromLatin1(key));
                    return false;
                }
                out = {x1, y1, x2, y2};
                return true;
            };

            BBox bbox;
            QString parseError;
            if (!parseBBox(tools_args_, "bbox", bbox, parseError))
            {
                const QString detail = QStringLiteral("controller invalid args: %1").arg(parseError);
                sendStateMessage("tool:" + QStringLiteral("controller ") + jtr("return") + "\n" + detail, WRONG_SIGNAL);
                sendPushMessage(QStringLiteral("controller ") + jtr("return") + "\n" + detail);
                return;
            }

            const QString actionRaw = QString::fromStdString(get_string_safely(tools_args_, "action")).trimmed();
            const QString description = QString::fromStdString(get_string_safely(tools_args_, "description")).trimmed();
            const QString key = QString::fromStdString(get_string_safely(tools_args_, "key")).trimmed();
            const QString text = QString::fromStdString(get_string_safely(tools_args_, "text"));

            if (actionRaw.isEmpty() || description.isEmpty())
            {
                const QString detail = QStringLiteral("controller invalid args: action/description is required");
                sendStateMessage("tool:" + QStringLiteral("controller ") + jtr("return") + "\n" + detail, WRONG_SIGNAL);
                sendPushMessage(QStringLiteral("controller ") + jtr("return") + "\n" + detail);
                return;
            }

            auto normAction = [](QString a) -> QString {
                a = a.trimmed().toLower();
                a.replace('-', '_');
                a.remove(' ');
                return a;
            };
            const QString action = normAction(actionRaw);

            // 将“模型传入的参数”（原始 arguments JSON）交给 UI：
            // - 用于把 bbox/action/description/to_bbox 等信息绘制到截图上并落盘（EVA_TEMP/overlay）
            // - 仅用于回溯观察模型定位，不参与执行逻辑
            emit tool2ui_controller_overlay(invocation->turnId, tools_args);

            // -----------------------------------------------------------------------------
            // 坐标映射：归一化坐标系 -> 真实屏幕坐标（0~screenMaxX / 0~screenMaxY）
            // -----------------------------------------------------------------------------
            const int normMaxX = std::max(1, controllerNormX_.load(std::memory_order_acquire));
            const int normMaxY = std::max(1, controllerNormY_.load(std::memory_order_acquire));

            int screenMaxX = 0;
            int screenMaxY = 0;
#ifdef _WIN32
            screenMaxX = std::max(0, GetSystemMetrics(SM_CXSCREEN) - 1);
            screenMaxY = std::max(0, GetSystemMetrics(SM_CYSCREEN) - 1);
#else
            Display *display = platform::dsp();
            if (display)
            {
                const int screenIndex = DefaultScreen(display);
                screenMaxX = std::max(0, DisplayWidth(display, screenIndex) - 1);
                screenMaxY = std::max(0, DisplayHeight(display, screenIndex) - 1);
            }
#endif

            auto mapCoord = [](int value, int srcMax, int dstMax) -> int {
                if (dstMax <= 0) return 0;
                if (srcMax <= 0) return std::clamp(value, 0, dstMax);
                const int clamped = std::clamp(value, 0, srcMax);
                const long long numerator = 1LL * clamped * dstMax + srcMax / 2;
                const int mapped = int(numerator / srcMax);
                return std::clamp(mapped, 0, dstMax);
            };

            auto bboxCenterNorm = [](const BBox &b) -> std::pair<int, int> {
                int x1 = b.x1, y1 = b.y1, x2 = b.x2, y2 = b.y2;
                if (x1 > x2) std::swap(x1, x2);
                if (y1 > y2) std::swap(y1, y2);
                return {(x1 + x2) / 2, (y1 + y2) / 2};
            };

            const auto [cxNormRaw, cyNormRaw] = bboxCenterNorm(bbox);
            const int cxNorm = std::clamp(cxNormRaw, 0, normMaxX);
            const int cyNorm = std::clamp(cyNormRaw, 0, normMaxY);
            const int cx = mapCoord(cxNorm, normMaxX, screenMaxX);
            const int cy = mapCoord(cyNorm, normMaxY, screenMaxY);

            auto showOverlayHint = [&](int x, int y, const QString &desc) {
                // 通过信号让 UI 线程绘制叠加提示。
                // 需求：提示框显示一段时间后再执行动作（避免“提示刚出现就已经点下去了”）。
                emit tool2ui_controller_hint(x, y, desc);

                // 与 UI 侧 ControllerOverlay 的 durationMs 对齐：当前固定为 2000ms。
                // 额外 +100ms 用于覆盖跨线程排队与绘制抖动，尽量确保“提示时间结束后再执行”。
                constexpr unsigned long kOverlayDurationMs = 2000;
                constexpr unsigned long kOverlayJitterMs = 100;
                constexpr unsigned long kStepMs = 20;
                unsigned long remaining = kOverlayDurationMs + kOverlayJitterMs;
                while (remaining > 0)
                {
                    if (cancelled()) return;
                    const unsigned long step = (remaining > kStepMs) ? kStepMs : remaining;
                    msleep(step);
                    remaining -= step;
                }
            };

            auto smoothMoveTo = [&](int ex, int ey, int durationMs) {
                using namespace platform;
                if (durationMs <= 0)
                {
                    moveCursor(ex, ey);
                    return;
                }
                const int fps = 60;
                const int steps = std::max(1, int((durationMs / 1000.0) * fps));
#ifdef _WIN32
                POINT p;
                GetCursorPos(&p);
                int sx = p.x, sy = p.y;
#else
                Window root_r, child_r;
                int sx = 0, sy = 0;
                int rx = 0, ry = 0;
                unsigned int mask = 0;
                XQueryPointer(dsp(), DefaultRootWindow(dsp()), &root_r, &child_r,
                              &rx, &ry, &sx, &sy, &mask);
#endif
                const int stepSleep = std::max(1, durationMs / steps);
                for (int i = 1; i <= steps; ++i)
                {
                    if (cancelled()) return;
                    const double k = double(i) / steps;
                    moveCursor(int(sx + k * (ex - sx)), int(sy + k * (ey - sy)));
                    msleep(static_cast<unsigned long>(stepSleep));
                }
            };

            auto sendKeyString = [&](const QString &keysText) {
                std::string keys = keysText.toStdString();
                if (keys.find('+') != std::string::npos)
                    platform::sendKeyCombo(split(keys, '+'));
                else
                    platform::sendKeyCombo({keys});
            };

            const int delayMs = std::max(0, get_int_safely(tools_args_, "delay_ms", 0));
            const int durationMs = std::max(0, get_int_safely(tools_args_, "duration_ms", 0));
            const int scrollSteps = std::max(1, get_int_safely(tools_args_, "scroll_steps", 1));

            sendStateMessage(QStringLiteral("tool:controller action=%1 center_norm=(%2,%3)")
                                 .arg(actionRaw)
                                 .arg(cxNorm)
                                 .arg(cyNorm),
                             SIGNAL_SIGNAL);

            if (cancelled()) return;

            using namespace platform;
            if (action == QStringLiteral("left_click") || actionRaw == QStringLiteral("左键单击") || actionRaw == QStringLiteral("左键点击"))
            {
                showOverlayHint(cx, cy, description);
                leftDown(cx, cy);
                msleep(30);
                leftUp();
            }
            else if (action == QStringLiteral("left_double_click") || actionRaw == QStringLiteral("左键双击"))
            {
                showOverlayHint(cx, cy, description);
                leftDown(cx, cy);
                msleep(20);
                leftUp();
                msleep(100);
                leftDown(cx, cy);
                msleep(20);
                leftUp();
            }
            else if (action == QStringLiteral("right_click") || actionRaw == QStringLiteral("右击单击") || actionRaw == QStringLiteral("右键单击") ||
                     actionRaw == QStringLiteral("右键点击"))
            {
                showOverlayHint(cx, cy, description);
                rightDown(cx, cy);
                msleep(20);
                rightUp();
            }
            else if (action == QStringLiteral("middle_click") || actionRaw == QStringLiteral("中键单击") || actionRaw == QStringLiteral("中键点击"))
            {
                showOverlayHint(cx, cy, description);
                middleDown(cx, cy);
                msleep(20);
                middleUp();
            }
            else if (action == QStringLiteral("left_hold") || actionRaw == QStringLiteral("按住左键"))
            {
                showOverlayHint(cx, cy, description);
                leftDown(cx, cy);
            }
            else if (action == QStringLiteral("right_hold") || actionRaw == QStringLiteral("按住右键"))
            {
                showOverlayHint(cx, cy, description);
                rightDown(cx, cy);
            }
            else if (action == QStringLiteral("middle_hold") || actionRaw == QStringLiteral("按住中键"))
            {
                showOverlayHint(cx, cy, description);
                middleDown(cx, cy);
            }
            else if (action == QStringLiteral("left_release") || actionRaw == QStringLiteral("松开左键"))
            {
                showOverlayHint(cx, cy, description);
                leftUp();
            }
            else if (action == QStringLiteral("right_release") || actionRaw == QStringLiteral("松开右键"))
            {
                showOverlayHint(cx, cy, description);
                rightUp();
            }
            else if (action == QStringLiteral("middle_release") || actionRaw == QStringLiteral("松开中键"))
            {
                showOverlayHint(cx, cy, description);
                middleUp();
            }
            else if (action == QStringLiteral("scroll_down") || actionRaw == QStringLiteral("滚轮下滚"))
            {
                showOverlayHint(cx, cy, description);
                moveCursor(cx, cy);
                wheel(-scrollSteps);
            }
            else if (action == QStringLiteral("scroll_up") || actionRaw == QStringLiteral("滚轮上滚"))
            {
                showOverlayHint(cx, cy, description);
                moveCursor(cx, cy);
                wheel(scrollSteps);
            }
            else if (action == QStringLiteral("press_key") || actionRaw == QStringLiteral("按键盘") || action == QStringLiteral("keyboard"))
            {
                if (key.isEmpty())
                {
                    const QString detail = QStringLiteral("controller invalid args: key is required for action=%1").arg(actionRaw);
                    sendStateMessage("tool:" + QStringLiteral("controller ") + jtr("return") + "\n" + detail, WRONG_SIGNAL);
                    sendPushMessage(QStringLiteral("controller ") + jtr("return") + "\n" + detail);
                    return;
                }
                showOverlayHint(cx, cy, description);
                sendKeyString(key);
            }
            else if (action == QStringLiteral("type_text") || actionRaw == QStringLiteral("发送文字") || action == QStringLiteral("send_text"))
            {
                showOverlayHint(cx, cy, description);
#ifdef _WIN32
                // Windows：使用 KEYEVENTF_UNICODE 直接注入 UTF-16，避免污染剪贴板。
                const std::wstring w = text.toStdWString();
                for (wchar_t ch : w)
                {
                    if (cancelled()) return;
                    INPUT inputs[2]{};
                    inputs[0].type = INPUT_KEYBOARD;
                    inputs[0].ki.wVk = 0;
                    inputs[0].ki.wScan = static_cast<WORD>(ch);
                    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
                    inputs[1] = inputs[0];
                    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                    SendInput(2, inputs, sizeof(INPUT));
                    msleep(1);
                }
#else
                const QString warn = QStringLiteral("type_text is not supported on Linux/X11 yet");
                sendStateMessage(QStringLiteral("tool:controller warn: ") + warn, WRONG_SIGNAL);
#endif
            }
            else if (action == QStringLiteral("delay") || actionRaw == QStringLiteral("延时") || action == QStringLiteral("sleep"))
            {
                showOverlayHint(cx, cy, description);
                if (delayMs > 0) msleep(static_cast<unsigned long>(delayMs));
            }
            else if (action == QStringLiteral("move_mouse") || actionRaw == QStringLiteral("移动鼠标") || action == QStringLiteral("move"))
            {
                showOverlayHint(cx, cy, description);
                smoothMoveTo(cx, cy, durationMs);
            }
            else if (action == QStringLiteral("drag_drop") || actionRaw == QStringLiteral("拖放") || action == QStringLiteral("drag"))
            {
                BBox toBbox;
                QString err;
                if (!parseBBox(tools_args_, "to_bbox", toBbox, err))
                {
                    const QString detail = QStringLiteral("controller invalid args: to_bbox is required for drag_drop (%1)").arg(err);
                    sendStateMessage("tool:" + QStringLiteral("controller ") + jtr("return") + "\n" + detail, WRONG_SIGNAL);
                    sendPushMessage(QStringLiteral("controller ") + jtr("return") + "\n" + detail);
                    return;
                }
                const auto [toCxNormRaw, toCyNormRaw] = bboxCenterNorm(toBbox);
                const int toCxNorm = std::clamp(toCxNormRaw, 0, normMaxX);
                const int toCyNorm = std::clamp(toCyNormRaw, 0, normMaxY);
                const int toCx = mapCoord(toCxNorm, normMaxX, screenMaxX);
                const int toCy = mapCoord(toCyNorm, normMaxY, screenMaxY);

                showOverlayHint(cx, cy, description);
                leftDown(cx, cy);
                msleep(50);
                smoothMoveTo(toCx, toCy, durationMs > 0 ? durationMs : 400);
                msleep(20);
                showOverlayHint(toCx, toCy, description);
                leftUp();
            }
            else
            {
                const QString detail = QStringLiteral("controller invalid action: %1").arg(actionRaw);
                sendStateMessage("tool:" + QStringLiteral("controller ") + jtr("return") + "\n" + detail, WRONG_SIGNAL);
                sendPushMessage(QStringLiteral("controller ") + jtr("return") + "\n" + detail);
                return;
            }

            if (cancelled()) return;

            const QString detail = QStringLiteral("ok\naction=%1\nbbox=[%2,%3,%4,%5]\ncenter_norm=(%6,%7)")
                                       .arg(actionRaw)
                                       .arg(bbox.x1)
                                       .arg(bbox.y1)
                                       .arg(bbox.x2)
                                       .arg(bbox.y2)
                                       .arg(cxNorm)
                                       .arg(cyNorm);
            sendStateMessage("tool:" + QStringLiteral("controller ") + jtr("return") + "\n" + detail, TOOL_SIGNAL);
            sendPushMessage(QStringLiteral("controller ") + jtr("return") + "\n" + detail);
            return;
        }

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
        const QString detail = QStringLiteral("excute sequence over\nsteps=%1\nsequence=%2")
                                   .arg(build_in_tool_arg.size())
                                   .arg(QString::fromStdString(build_in_tool_arg_));
        sendStateMessage("tool:" + QString("controller ") + jtr("return") + "\n" + detail, TOOL_SIGNAL);
        sendPushMessage(QString("controller ") + jtr("return") + "\n" + detail);
    }
    //----------------------文生图------------------
    else if (tools_name == "stablediffusion")
    {
        return;
    }
    //----------------------读取文件------------------
    else if (tools_name == "read_file")
    {
        QVector<FileReadSpec> specs;
        QString parseError;
        if (!parseFileSpecsFromArgs(tools_args_, specs, parseError))
        {
            sendPushMessage(QStringLiteral("read_file ") + jtr("return") + " " + parseError);
            return;
        }
        const int kMaxFiles = 5;
        if (specs.size() > kMaxFiles)
        {
            specs = specs.mid(0, kMaxFiles);
        }
        const bool useDocker = dockerSandboxEnabled();
        const QString root = QDir::cleanPath(resolveWorkRoot());
        QStringList outputs;
        outputs.reserve(specs.size());

        auto readFileContent = [&](const ToolPathResolution &pathRes, QString &content, QString &error) -> bool {
            if (useDocker)
            {
                const bool pathIsContainer = pathRes.containerAbsolute;
                const QString dockerPath = pathIsContainer ? pathRes.containerPath : pathRes.hostPath;
                if (!dockerReadTextFile(dockerPath, &content, &error, pathIsContainer)) return false;
                return true;
            }
            QFile file(pathRes.hostPath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                error = QStringLiteral("cannot open file: %1").arg(pathRes.hostPath);
                return false;
            }
            QTextStream in(&file);
            in.setCodec("UTF-8");
            content = in.readAll();
            file.close();
            return true;
        };

        for (const FileReadSpec &spec : specs)
        {
            if (shouldAbort(invocation)) break;
            ToolPathResolution pathRes;
            QString pathError;
            if (!resolveToolPath(spec.path, &pathRes, &pathError))
            {
                outputs << QStringLiteral(">>> %1\n%2").arg(spec.path, pathError.isEmpty() ? QStringLiteral("invalid path") : pathError);
                continue;
            }
            QString fileContent;
            QString readError;
            if (!readFileContent(pathRes, fileContent, readError))
            {
                outputs << QStringLiteral(">>> %1\n%2").arg(spec.path, readError);
                continue;
            }

            const bool hadCRLF = fileContent.contains("\r\n");
            const bool hadCR = !hadCRLF && fileContent.contains('\r');
            QString normalized = normalizeNewlines(fileContent);
            const bool hadTrailingNewline = normalized.endsWith('\n');
            QStringList lines = normalized.split('\n', Qt::KeepEmptyParts);
            if (hadTrailingNewline && !lines.isEmpty() && lines.last().isEmpty()) lines.removeLast();

            QString displayPath;
            if (useDocker && pathRes.containerAbsolute)
                displayPath = pathRes.containerPath;
            else
                displayPath = QDir(root).relativeFilePath(pathRes.hostPath);
            if (displayPath.isEmpty() || displayPath.startsWith("..")) displayPath = pathRes.hostPath;

            QStringList rangeOutput;
            for (const LineRange &range : spec.ranges)
            {
                if (shouldAbort(invocation)) break;
                const int start = std::max(1, range.start);
                const int end = std::max(start, range.end);
                if (lines.isEmpty())
                {
                    rangeOutput << QStringLiteral("(empty file)");
                    continue;
                }
                if (start > lines.size())
                {
                    rangeOutput << QStringLiteral("(range %1-%2 out of file size %3)").arg(start).arg(end).arg(lines.size());
                    continue;
                }
                const int last = std::min(end, lines.size());
                QStringList slice;
                for (int i = start - 1; i < last; ++i)
                {
                    slice << QStringLiteral("%1: %2").arg(i + 1).arg(lines.at(i));
                }
                rangeOutput << slice.join("\n");
            }
            const QString header = QStringLiteral(">>> %1").arg(displayPath);
            outputs << header + QStringLiteral("\n") + rangeOutput.join("\n---\n");
        }

        if (outputs.isEmpty())
        {
            sendPushMessage(QStringLiteral("read_file ") + jtr("return") + " no readable files.");
            return;
        }

        const QString result = outputs.join("\n\n");
        sendStateMessage("tool:" + QString("read_file ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
        sendPushMessage(QString("read_file ") + jtr("return") + "\n" + result);
    }

    //----------------------写入文件------------------
    else if (tools_name == "write_file")
    {
        QString filepath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        QString content = QString::fromStdString(get_string_safely(tools_args_, "content"));
        ToolPathResolution pathRes;
        QString pathError;
        if (!resolveToolPath(filepath, &pathRes, &pathError))
        {
            sendPushMessage(QString("write_file ") + jtr("return") + "\n" + (pathError.isEmpty() ? QStringLiteral("invalid path") : pathError));
            return;
        }
        if (dockerSandboxEnabled())
        {
            QString error;
            const bool pathIsContainer = pathRes.containerAbsolute;
            const QString dockerPath = pathIsContainer ? pathRes.containerPath : pathRes.hostPath;
            if (!dockerWriteTextFile(dockerPath, content, &error, pathIsContainer))
            {
                sendPushMessage(QString("write_file ") + jtr("return") + "\n" + error);
                return;
            }
        }
        else
        {
            QFileInfo fileInfo(pathRes.hostPath);
            QString dirPath = fileInfo.absolutePath();
            QDir dir;
            if (!dir.mkpath(dirPath))
            {
                sendPushMessage(QString("write_file ") + jtr("return") + "Failed to create directory");
                return;
            }
            QFile file(pathRes.hostPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                sendPushMessage(QString("write_file ") + jtr("return") + "Could not open file for writing" + file.errorString());
                return;
            }
            QTextStream out(&file);
            out.setCodec("UTF-8");
            out << content;
            file.close();
        }
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
            sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + QStringLiteral(" old_string is empty."));
            return;
        }
        ToolPathResolution pathRes;
        QString pathError;
        if (!resolveToolPath(filepath, &pathRes, &pathError))
        {
            sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + " " + (pathError.isEmpty() ? QStringLiteral("invalid path") : pathError));
            return;
        }
        QString originalContent;
        if (dockerSandboxEnabled())
        {
            QString error;
            const bool pathIsContainer = pathRes.containerAbsolute;
            const QString dockerPath = pathIsContainer ? pathRes.containerPath : pathRes.hostPath;
            if (!dockerReadTextFile(dockerPath, &originalContent, &error, pathIsContainer))
            {
                sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + " " + error);
                return;
            }
        }
        else
        {
            QFile inFile(pathRes.hostPath);
            if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + "Could not open file for reading: " + inFile.errorString());
                return;
            }
            originalContent = QString::fromUtf8(inFile.readAll());
            inFile.close();
        }
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
            QString message = QStringLiteral("replace_in_file ") + jtr("return") + QStringLiteral(" old_string NOT found.");
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
            QString msg = QString("Expected %1 replacement(s) but found %2. ").arg(expectedRepl).arg(matches.size());
            msg += QStringLiteral("Consider narrowing old_string or reading the file to confirm current content.");
            sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + " " + msg);
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
        if (dockerSandboxEnabled())
        {
            QString error;
            const bool pathIsContainer = pathRes.containerAbsolute;
            const QString dockerPath = pathIsContainer ? pathRes.containerPath : pathRes.hostPath;
            if (!dockerWriteTextFile(dockerPath, finalContent, &error, pathIsContainer))
            {
                sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + " " + error);
                return;
            }
        }
        else
        {
            QFileInfo fi(pathRes.hostPath);
            QDir dir;
            if (!dir.mkpath(fi.absolutePath()))
            {
                sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + QStringLiteral("Failed to create directory."));
                return;
            }
            QFile outFile(pathRes.hostPath);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            {
                sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + "Could not open file for writing: " + outFile.errorString());
                return;
            }
            QTextStream ts(&outFile);
            ts.setCodec("UTF-8");
            ts << finalContent;
            outFile.close();
        }
        QString result = QString("replaced %1 occurrence(s)").arg(applied);
        QStringList notes;
        if (usedFlexibleMatch) notes << "whitespace-insensitive search";
        if (autoExpanded && applied > 1) notes << QString("auto-applied to %1 identical matches").arg(applied);
        if (!notes.isEmpty()) result += QString(" [%1]").arg(notes.join("; "));
        sendStateMessage(QStringLiteral("tool:replace_in_file ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
        sendPushMessage(QStringLiteral("replace_in_file ") + jtr("return") + "\n" + result);
    }




    else if (tools_name == "edit_in_file")
    {
        const auto sendError = [&](const QString &msg) {
            sendPushMessage(QStringLiteral("edit_in_file ") + jtr("return") + " " + msg);
        };
        if (!tools_args_.contains("edits") || !tools_args_["edits"].is_array())
        {
            sendError("edits must be an array.");
            return;
        }
        QString filepath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        ToolPathResolution pathRes;
        QString pathError;
        if (!resolveToolPath(filepath, &pathRes, &pathError))
        {
            sendError(pathError.isEmpty() ? QStringLiteral("Invalid path") : pathError);
            return;
        }
        QString originalContent;
        if (dockerSandboxEnabled())
        {
            QString error;
            const bool pathIsContainer = pathRes.containerAbsolute;
            const QString dockerPath = pathIsContainer ? pathRes.containerPath : pathRes.hostPath;
            if (!dockerReadTextFile(dockerPath, &originalContent, &error, pathIsContainer))
            {
                sendError(error);
                return;
            }
        }
        else
        {
            QFile inFile(pathRes.hostPath);
            if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                sendError("Could not open file for reading: " + inFile.errorString());
                return;
            }
            originalContent = QString::fromUtf8(inFile.readAll());
            inFile.close();
        }
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
            sendPushMessage(QStringLiteral("edit_in_file ") + jtr("return") + QStringLiteral(" no edits supplied."));
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
                    sendError(QString("edit #%1 references line out of range (file has %2 lines). Please refresh the file with read_file before editing.").arg(op.ordinal).arg(currentLineCount));
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
                    sendError(QString("edit #%1 insert_before target is out of range (file has %2 lines). Please refresh the file with read_file before editing.").arg(op.ordinal).arg(currentLineCount));
                    return;
                }
                for (int i = 0; i < op.newLines.size(); ++i) lines.insert(insertIdx + i, op.newLines.at(i));
            }
            else if (op.action == "insert_after")
            {
                const int anchorIdx = op.startLine - 1;
                if (anchorIdx < 0 || anchorIdx >= currentLineCount)
                {
                    sendError(QString("edit #%1 insert_after target is out of range (file has %2 lines). Please refresh the file with read_file before editing.").arg(op.ordinal).arg(currentLineCount));
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
        if (dockerSandboxEnabled())
        {
            QString error;
            const bool pathIsContainer = pathRes.containerAbsolute;
            const QString dockerPath = pathIsContainer ? pathRes.containerPath : pathRes.hostPath;
            if (!dockerWriteTextFile(dockerPath, finalContent, &error, pathIsContainer))
            {
                sendError(error);
                return;
            }
        }
        else
        {
            QFileInfo fi(pathRes.hostPath);
            QDir dir;
            if (!dir.mkpath(fi.absolutePath()))
            {
                sendError("Failed to create directory.");
                return;
            }
            QFile outFile(pathRes.hostPath);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            {
                sendError("Could not open file for writing: " + outFile.errorString());
                return;
            }
            QTextStream ts(&outFile);
            ts.setCodec("UTF-8");
            ts << finalContent;
            outFile.close();
        }
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
        sendStateMessage(QStringLiteral("tool:edit_in_file ") + jtr("return") + "\n" + result, TOOL_SIGNAL);
        sendPushMessage(QStringLiteral("edit_in_file ") + jtr("return") + "\n" + result);
    }

    //----------------------ptc（工程师）------------------
    else if (tools_name == "ptc")
    {
        auto fail = [&](const QString &message) {
            sendPushMessage(QStringLiteral("ptc ") + jtr("return") + " " + message);
            sendStateMessage(QStringLiteral("tool:ptc error -> ") + message, WRONG_SIGNAL);
        };
        QString fileName = QString::fromStdString(get_string_safely(tools_args_, "filename")).trimmed();
        QString workdirArg = QString::fromStdString(get_string_safely(tools_args_, "workdir")).trimmed();
        QString scriptContent = QString::fromStdString(get_string_safely(tools_args_, "content"));
        if (fileName.isEmpty())
        {
            fail(QStringLiteral("filename is required."));
            return;
        }
        if (fileName.contains('/') || fileName.contains('\\') || fileName.contains(".."))
        {
            fail(QStringLiteral("filename must not contain path separators or \"..\" segments."));
            return;
        }
        if (scriptContent.trimmed().isEmpty())
        {
            fail(QStringLiteral("content is empty."));
            return;
        }
        if (workdirArg.isEmpty()) workdirArg = QStringLiteral(".");
        QString pathError;
        const QString hostRunDir = resolveHostPathWithinWorkdir(workdirArg, &pathError);
        if (hostRunDir.isEmpty())
        {
            fail(pathError.isEmpty() ? QStringLiteral("Unable to resolve workdir.") : pathError);
            return;
        }
        QDir runDir(hostRunDir);
        if (!runDir.exists())
        {
            fail(QStringLiteral("workdir not found: %1").arg(QDir::toNativeSeparators(hostRunDir)));
            return;
        }

        const QString workRoot = resolveWorkRoot();
        ensureWorkdirExists(workRoot);
        QDir workRootDir(workRoot);
        const QString ptcDirPath = workRootDir.filePath(QStringLiteral("ptc_temp"));
        if (!QDir().mkpath(ptcDirPath))
        {
            fail(QStringLiteral("Failed to create ptc_temp under %1").arg(QDir::toNativeSeparators(workRoot)));
            return;
        }
        const QString scriptHostPath = QDir(ptcDirPath).filePath(fileName);
        QFile scriptFile(scriptHostPath);
        if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            fail(QStringLiteral("Could not write %1: %2").arg(QDir::toNativeSeparators(scriptHostPath), scriptFile.errorString()));
            return;
        }
        QTextStream stream(&scriptFile);
        stream.setCodec("UTF-8");
        stream << scriptContent;
        scriptFile.close();
        sendStateMessage(QStringLiteral("tool:ptc saved -> %1").arg(QDir::toNativeSeparators(scriptHostPath)));

        auto pythonParts = [this]() {
            QString spec = pythonExecutable.trimmed();
            if (spec.isEmpty())
            {
                spec = QStringLiteral(DEFAULT_PYTHON);
            }
            QStringList parts = QProcess::splitCommand(spec);
            if (parts.isEmpty())
            {
                parts << QStringLiteral(DEFAULT_PYTHON);
            }
            return parts;
        }();
        if (pythonParts.isEmpty())
        {
            pythonParts << QStringLiteral(DEFAULT_PYTHON);
        }
        const QString pythonProgram = pythonParts.first();
        QStringList pythonArgs = pythonParts.mid(1);

        QString stdoutText;
        QString stderrText;
        int exitCode = 0;
        bool exitKnown = false;
        bool dockerOk = true;
        QString dockerFailureText;

        if (dockerSandboxEnabled())
        {
            QString ensureError;
            if (!ensureDockerSandboxReady(&ensureError))
            {
                fail(ensureError.isEmpty() ? QStringLiteral("docker sandbox not ready.") : ensureError);
                return;
            }
            QString containerRunDir = containerPathForHost(hostRunDir);
            if (containerRunDir.isEmpty())
            {
                fail(QStringLiteral("Unable to map workdir into docker sandbox."));
                return;
            }
            QString containerScriptPath = containerPathForHost(scriptHostPath);
            if (containerScriptPath.isEmpty())
            {
                fail(QStringLiteral("Unable to map script path into docker sandbox."));
                return;
            }
            QStringList dockerCmdParts = pythonParts;
            dockerCmdParts << containerScriptPath;
            auto joinShellParts = [](const QStringList &parts) {
                QStringList quoted;
                quoted.reserve(parts.size());
                for (const QString &part : parts)
                {
                    quoted << shellQuote(part);
                }
                return quoted.join(' ');
            };
            const QString command = QStringLiteral("cd %1 && %2")
                                        .arg(shellQuote(containerRunDir),
                                             joinShellParts(dockerCmdParts));
            QString dockerStdOut;
            QString dockerStdErr;
            QString dockerError;
            dockerOk = runDockerShellCommand(command, &dockerStdOut, &dockerStdErr, &dockerError);
            stdoutText = dockerStdOut;
            stderrText = dockerStdErr;
            if (!dockerOk)
            {
                dockerFailureText = dockerError.isEmpty() ? QStringLiteral("docker exec returned non-zero exit status.") : dockerError;
            }
        }
        else
        {
            exitKnown = true;
            QStringList finalArgs = pythonArgs;
            finalArgs << QDir::toNativeSeparators(scriptHostPath);
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            ProcessResult result = ProcessRunner::run(pythonProgram, finalArgs, hostRunDir, env);
            if (result.timedOut)
            {
                fail(QStringLiteral("python execution timed out."));
                return;
            }
            stdoutText = result.stdOut;
            stderrText = result.stdErr;
            exitCode = result.exitCode;
        }

        QString finalText = stdoutText.trimmed();
        if (finalText.isEmpty())
        {
            finalText = QStringLiteral("[no stdout]");
        }
        if (!stderrText.trimmed().isEmpty())
        {
            if (!finalText.isEmpty()) finalText += QStringLiteral("\n[stderr]\n") + stderrText.trimmed();
            else finalText = QStringLiteral("[stderr]\n") + stderrText.trimmed();
        }
        if (exitKnown)
        {
            finalText.prepend(QStringLiteral("exit code %1\n").arg(exitCode));
        }
        else if (!dockerOk)
        {
            if (dockerFailureText.isEmpty())
                dockerFailureText = QStringLiteral("docker exec returned non-zero exit status.");
            finalText.prepend(dockerFailureText + QStringLiteral("\n"));
        }
        sendStateMessage(QStringLiteral("tool:ptc ") + jtr("return") + "\n" + finalText, TOOL_SIGNAL);
        sendPushMessage(QStringLiteral("ptc ") + jtr("return") + "\n" + finalText);
    }

    //----------------------列出目录（工程师）------------------
    else if (tools_name == "list_files")
    {
        QString reqPath = QString::fromStdString(get_string_safely(tools_args_, "path"));
        const QString effectivePath = reqPath.trimmed().isEmpty() ? QStringLiteral(".") : reqPath.trimmed();
        sendStateMessage("tool:" + QString("list_files(") + effectivePath + ")");
        const QString root = QDir::cleanPath(resolveWorkRoot());
        QDir rootDir(root);
        QString resolveError;
        const QString targetPath = resolveHostPathWithinWorkdir(effectivePath, &resolveError);
        if (targetPath.isEmpty())
        {
            const QString msg = resolveError.isEmpty() ? QStringLiteral("Unable to resolve path: %1").arg(effectivePath)
                                                       : resolveError;
            sendPushMessage(QString("list_files ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("list_files ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }
        QFileInfo dirInfo(targetPath);
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
        const QString subDir = QString::fromStdString(get_string_safely(tools_args_, "path")).trimmed();
        const QString filePattern = QString::fromStdString(get_string_safely(tools_args_, "file_pattern")).trimmed();
        sendStateMessage("tool:" + QString("search_content(") + query + ")");
        if (query.trimmed().isEmpty())
        {
            const QString msg = QString("Empty query.");
            sendPushMessage(QString("search_content ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("search_content ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }
        const QString root = QDir::cleanPath(resolveWorkRoot());
        QString targetRoot = root;
        if (!subDir.isEmpty())
        {
            QString resolveError;
            const QString resolved = resolveHostPathWithinWorkdir(subDir, &resolveError);
            if (resolved.isEmpty())
            {
                const QString msg = resolveError.isEmpty() ? QStringLiteral("Invalid search path: %1").arg(subDir) : resolveError;
                sendPushMessage(QString("search_content ") + jtr("return") + " " + msg);
                sendStateMessage("tool:" + QString("search_content ") + jtr("return") + " " + msg, TOOL_SIGNAL);
                return;
            }
            targetRoot = resolved;
        }
        QDir rootDir(targetRoot);
        if (!rootDir.exists())
        {
            const QString msg = QString("Work directory not found: %1").arg(targetRoot);
            sendPushMessage(QString("search_content ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("search_content ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }

        auto runRipGrep = [&]() -> QStringList {
            QStringList lines;
            QProcess rg;
#ifdef Q_OS_WIN
            rg.setProgram(QStringLiteral("rg.exe"));
#else
            rg.setProgram(QStringLiteral("rg"));
#endif
            QStringList args;
            args << QStringLiteral("--fixed-strings") << QStringLiteral("--ignore-case") << QStringLiteral("--no-heading") << QStringLiteral("--line-number")
                 << QStringLiteral("--max-count") << QStringLiteral("3") << QStringLiteral("--max-filesize") << QStringLiteral("2000K")
                 << QStringLiteral("--color") << QStringLiteral("never");
            if (!filePattern.isEmpty()) args << QStringLiteral("-g") << filePattern;
            args << query << QStringLiteral(".");
            rg.setArguments(args);
            rg.setWorkingDirectory(rootDir.absolutePath());
            rg.start();
            if (!rg.waitForFinished(15000))
            {
                rg.kill();
                return lines;
            }
            const QString stderrText = QString::fromUtf8(rg.readAllStandardError());
            if (rg.error() != QProcess::UnknownError || rg.exitCode() >= 2)
            {
                qDebug() << "ripgrep failed:" << stderrText;
                return lines;
            }
            const QString stdoutText = QString::fromUtf8(rg.readAllStandardOutput());
            const QStringList raw = stdoutText.split('\n', Qt::SkipEmptyParts);
            const int kMaxLines = 160;
            for (int i = 0; i < raw.size() && lines.size() < kMaxLines; ++i)
            {
                if (shouldAbort(invocation)) break;
                lines << raw.at(i);
            }
            return lines;
        };

        QStringList results = runRipGrep();
        if (results.isEmpty())
        {
            // 回退到简易扫描
            auto isLikelyText = [](const QFileInfo &info, qint64 size) -> bool {
                if (size > (qint64)2 * 1024 * 1024) return false;
                static const QSet<QString> exts = {
                    "txt", "md", "markdown", "log", "ini", "cfg", "conf", "csv", "tsv", "json", "yaml", "yml", "toml", "xml", "html", "htm",
                    "css", "js", "ts", "tsx", "jsx", "py", "ipynb", "c", "cc", "cpp", "h", "hpp", "hh", "java", "kt", "rs", "go", "rb", "php",
                    "sh", "bash", "zsh", "ps1", "bat", "cmake", "mak", "make", "gradle", "properties", "sql", "mm", "m", "swift"
                };
                static const QSet<QString> namesWithoutExt = {
                    ".gitignore", ".gitmodules", ".gitattributes", ".clang-format", ".clang-tidy", ".editorconfig", ".env", ".env.local",
                    ".env.example", "dockerfile", "makefile", "cmakelists.txt", "license", "license.txt", "readme", "readme.md"
                };
                const QString ext = info.suffix().toLower();
                if (!ext.isEmpty()) return exts.contains(ext);
                const QString name = info.fileName().toLower();
                return namesWithoutExt.contains(name);
            };
            QRegularExpression globRegex;
            if (!filePattern.isEmpty())
            {
                globRegex = QRegularExpression(QRegularExpression::wildcardToRegularExpression(filePattern));
            }

            const int kMaxMatches = 200;
            QDirIterator it(rootDir.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                if (shouldAbort(invocation)) break;
                const QString absolutePath = it.next();
                const QFileInfo fi(absolutePath);
                const QString relCheck = rootDir.relativeFilePath(fi.absoluteFilePath());
                if (relCheck.startsWith("..")) continue;
                if (fi.fileName().startsWith('.')) continue;
                if (!globRegex.pattern().isEmpty() && !globRegex.match(relCheck).hasMatch()) continue;
                if (!isLikelyText(fi, fi.size())) continue;
                QFile file(absolutePath);
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                QTextStream stream(&file);
                stream.setCodec("UTF-8");
                int lineNumber = 0;
                while (!stream.atEnd() && results.size() < kMaxMatches)
                {
                    if (shouldAbort(invocation)) break;
                    ++lineNumber;
                    const QString line = stream.readLine();
                    if (!line.contains(query, Qt::CaseInsensitive)) continue;
                    const QString rel = rootDir.relativeFilePath(fi.absoluteFilePath());
                    results << QString("%1:%2:%3").arg(rel).arg(lineNumber).arg(line.trimmed());
                }
                file.close();
                if (results.size() >= kMaxMatches || shouldAbort(invocation)) break;
            }
        }

        if (shouldAbort(invocation)) return;
        if (results.isEmpty())
        {
            const QString msg = QString("No matches.");
            sendPushMessage(QString("search_content ") + jtr("return") + " " + msg);
            sendStateMessage("tool:" + QString("search_content ") + jtr("return") + " " + msg, TOOL_SIGNAL);
            return;
        }

        const QString summary = QString("Found %1 line(s) in %2 (pattern:%3)")
                                    .arg(results.size())
                                    .arg(QDir::toNativeSeparators(rootDir.absolutePath()))
                                    .arg(filePattern.isEmpty() ? QStringLiteral("all") : filePattern);
        results.prepend(summary);
        if (results.size() > 160)
        {
            results = results.mid(0, 160);
            results << QStringLiteral("[Results truncated. Refine your query or add file_pattern/path.]");
        }

        const QString result = results.join("\n");
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
    FlowTracer::log(FlowChannel::Tool,
                    QStringLiteral("tool:exec start workdir=%1 docker=%2")
                        .arg(resolveWorkRoot(),
                             dockerSandboxEnabled() ? QStringLiteral("yes") : QStringLiteral("no")),
                    invocation->turnId);
    const QString work = resolveWorkRoot();
    ensureWorkdirExists(work);
    const bool useDocker = dockerSandboxEnabled();
    QString dockerError;
    if (useDocker && !ensureDockerSandboxReady(&dockerError))
    {
        sendPushMessage(QStringLiteral("execute_command failed: ") + dockerError);
        sendStateMessage("tool:" + QStringLiteral("execute_command docker error\n") + dockerError, WRONG_SIGNAL);
        FlowTracer::log(FlowChannel::Tool,
                        QStringLiteral("tool:exec docker error %1").arg(dockerError),
                        invocation->turnId);
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
    const QString name = invocation->name.isEmpty() ? QStringLiteral("<unknown>") : invocation->name;
    const QString line = QStringLiteral("tool:done %1").arg(name);
    FlowTracer::log(FlowChannel::Tool, line, invocation->turnId);
    // emit tool2ui_state(clampToolMessage(QStringLiteral("tool:done %1").arg(name)), SIGNAL_SIGNAL);
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
    FlowTracer::log(FlowChannel::Tool,
                    QStringLiteral("tool:mcp list id=%1").arg(invocation->id),
                    invocation->turnId);
    emit tool2mcp_toollist(invocation->id);
}

void xTool::handleMcpToolCall(const ToolInvocationPtr &invocation)
{
    if (!invocation) return;
    pendingMcpInvocations_[invocation->id] = invocation;
    QString toolArgs = QString::fromStdString(invocation->args.dump());
    FlowTracer::log(FlowChannel::Tool,
                    QStringLiteral("tool:mcp call %1 id=%2").arg(invocation->name).arg(invocation->id),
                    invocation->turnId);
    emit tool2mcp_toolcall(invocation->id, invocation->name, toolArgs);
}

QString xTool::resolveWorkRoot() const
{
    if (!workDirRoot.isEmpty()) return workDirRoot;
    return QDir::cleanPath(applicationDirPath + "/EVA_WORK");
}

QString xTool::resolveHostPathWithinWorkdir(const QString &inputPath, QString *errorMessage) const
{
    const QString root = QDir::cleanPath(resolveWorkRoot());
    QDir rootDir(root);
    QString trimmed = QDir::fromNativeSeparators(inputPath.trimmed());
    if (trimmed.isEmpty()) trimmed = QStringLiteral(".");
    if (dockerSandboxEnabled())
    {
        QString containerRoot = dockerSandbox_ && !dockerSandbox_->containerWorkdir().isEmpty() ? dockerSandbox_->containerWorkdir()
                                                                                               : DockerSandbox::defaultContainerWorkdir();
        containerRoot = normalizeUnixPath(containerRoot.startsWith('/') ? containerRoot : QStringLiteral("/") + containerRoot);
        QString normalizedInput = normalizeUnixPath(trimmed);
        if (normalizedInput.startsWith('/'))
        {
            QString prefix = containerRoot;
            if (!prefix.endsWith('/')) prefix += '/';
            if (normalizedInput == containerRoot)
            {
                trimmed = QStringLiteral(".");
            }
            else if (normalizedInput.startsWith(prefix))
            {
                trimmed = normalizedInput.mid(prefix.length());
                if (trimmed.isEmpty()) trimmed = QStringLiteral(".");
            }
            else
            {
                if (errorMessage) *errorMessage = QStringLiteral("Access denied: path outside docker workspace -> %1").arg(inputPath);
                return {};
            }
        }
    }
    QString candidate;
    const bool looksUnixAbsolute = trimmed.startsWith('/');
    if (trimmed == QStringLiteral("."))
    {
        candidate = root;
    }
    else if (QDir::isAbsolutePath(trimmed) || looksUnixAbsolute)
    {
        candidate = QDir::cleanPath(trimmed);
    }
    else
    {
        candidate = QDir::cleanPath(rootDir.filePath(trimmed));
    }
    const QString normalizedRootFs = QDir::fromNativeSeparators(root);
    const QString normalizedCandidateFs = QDir::fromNativeSeparators(candidate);
#ifdef _WIN32
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    QString prefix = normalizedRootFs;
    if (!prefix.endsWith('/')) prefix += '/';
    const bool insideRoot = normalizedCandidateFs.compare(normalizedRootFs, cs) == 0 || normalizedCandidateFs.startsWith(prefix, cs);
    if (!insideRoot)
    {
        if (errorMessage) *errorMessage = QStringLiteral("Access denied: path outside work root -> %1").arg(QDir::toNativeSeparators(candidate));
        return {};
    }
    return candidate;
}

bool xTool::resolveToolPath(const QString &inputPath, ToolPathResolution *resolution, QString *errorMessage) const
{
    if (!resolution) return false;
    ToolPathResolution result;
    result.originalInput = inputPath;
    const bool docker = dockerSandboxEnabled();
    const QString trimmed = QDir::fromNativeSeparators(inputPath.trimmed());
    if (docker && trimmed.startsWith(QLatin1Char('/')))
    {
        result.containerAbsolute = true;
        result.containerPath = normalizeUnixPath(trimmed);
    }
    else
    {
        QString hostError;
        result.hostPath = resolveHostPathWithinWorkdir(trimmed, &hostError);
        if (result.hostPath.isEmpty())
        {
            if (errorMessage) *errorMessage = hostError.isEmpty() ? QStringLiteral("Invalid path") : hostError;
            return false;
        }
        if (docker)
        {
            const QString mapped = containerPathForHost(result.hostPath);
            if (mapped.isEmpty())
            {
                if (errorMessage) *errorMessage = QStringLiteral("Path outside permitted roots");
                return false;
            }
            result.containerPath = mapped;
        }
    }
    *resolution = result;
    return true;
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

QString xTool::containerPathForHost(const QString &absHostPath) const
{
    const QString normalized = QDir::cleanPath(absHostPath);
    auto mapIntoContainer = [&](const QString &hostRoot, const QString &containerRoot) -> QString {
        if (hostRoot.isEmpty() || containerRoot.isEmpty()) return {};
        QString cleanHostRoot = QDir::cleanPath(hostRoot);
        QDir rootDir(cleanHostRoot);
        QString relative = rootDir.relativeFilePath(normalized);
        if (relative.startsWith(QStringLiteral(".."))) return {};
        QDir containerDir(containerRoot);
        return QDir::cleanPath(containerDir.filePath(relative));
    };

    QString containerRoot = DockerSandbox::defaultContainerWorkdir();
    if (dockerSandbox_)
    {
        const QString sandboxRoot = dockerSandbox_->containerWorkdir();
        if (!sandboxRoot.isEmpty()) containerRoot = sandboxRoot;
    }

    const QString workMapped = mapIntoContainer(resolveWorkRoot(), containerRoot);
    if (!workMapped.isEmpty()) return workMapped;

    if (!dockerConfig_.hostSkillsDir.isEmpty())
    {
        const QString skillsMapped = mapIntoContainer(dockerConfig_.hostSkillsDir, DockerSandbox::skillsMountPoint());
        if (!skillsMapped.isEmpty()) return skillsMapped;
    }

    return {};
}

bool xTool::dockerReadTextFile(const QString &path, QString *content, QString *errorMessage, bool pathIsContainer)
{
    if (!dockerSandboxEnabled())
    {
        if (errorMessage) *errorMessage = QStringLiteral("docker sandbox not enabled");
        return false;
    }
    QString ensureError;
    if (!ensureDockerSandboxReady(&ensureError))
    {
        if (errorMessage) *errorMessage = ensureError;
        return false;
    }
    QString containerPath;
    if (pathIsContainer)
    {
        containerPath = normalizeUnixPath(path);
    }
    else
    {
        containerPath = containerPathForHost(path);
        if (containerPath.isEmpty())
        {
            if (errorMessage) *errorMessage = QStringLiteral("Path outside permitted roots");
            return false;
        }
    }
    QString stdOut;
    QString stdErr;
    QString execError;
    const QString command = QStringLiteral("cat %1").arg(shellQuote(containerPath));
    if (!runDockerShellCommand(command, &stdOut, &stdErr, &execError))
    {
        if (errorMessage) *errorMessage = execError.isEmpty() ? stdErr.trimmed() : execError;
        return false;
    }
    if (content) *content = stdOut;
    return true;
}

bool xTool::dockerWriteTextFile(const QString &path, const QString &content, QString *errorMessage, bool pathIsContainer)
{
    if (!dockerSandboxEnabled())
    {
        if (errorMessage) *errorMessage = QStringLiteral("docker sandbox not enabled");
        return false;
    }
    QString ensureError;
    if (!ensureDockerSandboxReady(&ensureError))
    {
        if (errorMessage) *errorMessage = ensureError;
        return false;
    }
    QString containerPath;
    if (pathIsContainer)
    {
        containerPath = normalizeUnixPath(path);
    }
    else
    {
        containerPath = containerPathForHost(path);
        if (containerPath.isEmpty())
        {
            if (errorMessage) *errorMessage = QStringLiteral("Path outside permitted roots");
            return false;
        }
    }
    QString dirPath = containerPath;
    const int lastSlash = dirPath.lastIndexOf('/');
    if (lastSlash > 0)
        dirPath = dirPath.left(lastSlash);
    else
        dirPath = QStringLiteral(".");
    const QString command = QStringLiteral("set -e; mkdir -p %1 && cat > %2").arg(shellQuote(dirPath), shellQuote(containerPath));
    QString stdErr;
    QString execError;
    QByteArray data = content.toUtf8();
    if (!runDockerShellCommand(command, nullptr, &stdErr, &execError, data))
    {
        if (errorMessage) *errorMessage = execError.isEmpty() ? stdErr.trimmed() : execError;
        return false;
    }
    return true;
}

bool xTool::runDockerShellCommand(const QString &shellCommand, QString *stdOut, QString *stdErr, QString *errorMessage, const QByteArray &stdinData) const
{
    if (!dockerSandboxEnabled() || !dockerSandbox_ || dockerSandbox_->containerName().isEmpty())
    {
        if (errorMessage) *errorMessage = QStringLiteral("docker sandbox not ready");
        return false;
    }
#ifdef _WIN32
    const QString program = QStringLiteral("docker.exe");
#else
    const QString program = QStringLiteral("docker");
#endif
    QStringList args;
    args << QStringLiteral("exec") << QStringLiteral("-i") << dockerSandbox_->containerName()
         << QStringLiteral("/bin/sh") << QStringLiteral("-c") << shellCommand;
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!stdinData.isEmpty())
    {
        process.write(stdinData);
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(120000))
    {
        process.kill();
        if (errorMessage) *errorMessage = QStringLiteral("docker exec timeout");
        return false;
    }
    const QString outText = QString::fromUtf8(process.readAllStandardOutput());
    const QString errText = QString::fromUtf8(process.readAllStandardError());
    if (stdOut) *stdOut = outText;
    if (stdErr) *stdErr = errText;
    const bool success = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (!success && errorMessage)
    {
        if (!errText.trimmed().isEmpty())
            *errorMessage = errText.trimmed();
        else
            *errorMessage = QStringLiteral("docker exec failed (%1)").arg(process.exitCode());
    }
    return success;
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

void xTool::recv_controllerNormalize(int normX, int normY)
{
    // 保护：避免异常值导致后续换算出现除零或溢出
    const int x = std::clamp(normX, 100, 2048);
    const int y = std::clamp(normY, 100, 2048);
    controllerNormX_.store(x, std::memory_order_release);
    controllerNormY_.store(y, std::memory_order_release);
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
    // -----------------------------------------------------------------------------
    // 桌面控制器：归一化坐标 -> 真实屏幕坐标
    // - UI 侧会把截图缩放到 (controllerNormX_, controllerNormY_)
    // - 模型输出的 x/y 也按该归一化坐标系给出
    // - 工具层在执行前按比例换算回真实屏幕坐标（0~screenMaxX / 0~screenMaxY）
    // -----------------------------------------------------------------------------
    const int normMaxX = std::max(1, controllerNormX_.load(std::memory_order_acquire));
    const int normMaxY = std::max(1, controllerNormY_.load(std::memory_order_acquire));

    int screenMaxX = 0;
    int screenMaxY = 0;
#ifdef _WIN32
    screenMaxX = std::max(0, GetSystemMetrics(SM_CXSCREEN) - 1);
    screenMaxY = std::max(0, GetSystemMetrics(SM_CYSCREEN) - 1);
#else
    Display *display = platform::dsp();
    if (display)
    {
        const int screenIndex = DefaultScreen(display);
        screenMaxX = std::max(0, DisplayWidth(display, screenIndex) - 1);
        screenMaxY = std::max(0, DisplayHeight(display, screenIndex) - 1);
    }
#endif

    auto mapCoord = [](int value, int srcMax, int dstMax) -> int {
        if (dstMax <= 0) return 0;
        if (srcMax <= 0) return std::clamp(value, 0, dstMax);
        const int clamped = std::clamp(value, 0, srcMax);
        const long long numerator = 1LL * clamped * dstMax + srcMax / 2;
        const int mapped = int(numerator / srcMax);
        return std::clamp(mapped, 0, dstMax);
    };

    auto parseIntRounded = [](std::string text, int &out) -> bool {
        // 允许输入 "123" / 123 / 123.4 这类格式，统一解析为四舍五入后的整数
        text.erase(std::remove(text.begin(), text.end(), '\''), text.end());
        text.erase(std::remove(text.begin(), text.end(), '\"'), text.end());
        if (text.empty()) return false;
        try
        {
            size_t idx = 0;
            const double v = std::stod(text, &idx);
            while (idx < text.size() && std::isspace(static_cast<unsigned char>(text[idx]))) ++idx;
            if (idx != text.size()) return false;
            out = static_cast<int>(v >= 0.0 ? (v + 0.5) : (v - 0.5));
            return true;
        }
        catch (...)
        {
            return false;
        }
    };

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
            int xNorm = 0;
            int yNorm = 0;
            if (!parseIntRounded(args_list[0], xNorm) || !parseIntRounded(args_list[1], yNorm))
            {
                std::cout << "left_down invalid args";
                continue;
            }
            const int x = mapCoord(xNorm, normMaxX, screenMaxX);
            const int y = mapCoord(yNorm, normMaxY, screenMaxY);
            std::cout << "left_down "
                      << " x " << xNorm << " y " << yNorm;
            leftDown(x, y);
        }
        else if (func_name == "left_up")
        {
            std::cout << "leftUp ";
            leftUp();
        }
        else if (func_name == "right_down" && args_list.size() == 2)
        {
            int xNorm = 0;
            int yNorm = 0;
            if (!parseIntRounded(args_list[0], xNorm) || !parseIntRounded(args_list[1], yNorm))
            {
                std::cout << "right_down invalid args";
                continue;
            }
            const int x = mapCoord(xNorm, normMaxX, screenMaxX);
            const int y = mapCoord(yNorm, normMaxY, screenMaxY);
            std::cout << "right_down "
                      << " x " << xNorm << " y " << yNorm;
            rightDown(x, y);
        }
        else if (func_name == "right_up")
        {
            std::cout << "right_up ";
            rightUp();
        }
        else if (func_name == "move" && args_list.size() == 3)
        {
            int exNorm = 0;
            int eyNorm = 0;
            if (!parseIntRounded(args_list[0], exNorm) || !parseIntRounded(args_list[1], eyNorm))
            {
                std::cout << "move invalid args";
                continue;
            }
            float sec = 0.0f;
            try
            {
                sec = std::stof(args_list[2]);
            }
            catch (...)
            {
                std::cout << "move invalid duration";
                continue;
            }
            const int ex = mapCoord(exNorm, normMaxX, screenMaxX);
            const int ey = mapCoord(eyNorm, normMaxY, screenMaxY);
            std::cout << "move "
                      << " x " << exNorm << " y " << eyNorm << " t " << args_list[2];
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
