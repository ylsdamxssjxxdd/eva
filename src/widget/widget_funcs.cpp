// 功能函数
#include "../utils/depresolver.h"
#include "../utils/processrunner.h"
#include "terminal_pane.h"
#include "ui_widget.h"
#include "widget.h"
#include "toolcall_test_dialog.h"
#include <QtGlobal>
#include <QDir>
#include <QFileInfo>
#include <QByteArray>
#include <QCheckBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QStringList>
#include <QSet>
#include <QProcessEnvironment>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGlobal>
#include <algorithm>

namespace
{
QStringList collectLooseJsonObjects(const QString &text)
{
    // Collect balanced JSON objects to tolerate missing <tool_call> wrappers.
    QStringList objects;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    int start = -1;

    for (int i = 0; i < text.size(); ++i)
    {
        const QChar ch = text.at(i);
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (ch == '\\')
            {
                escaped = true;
                continue;
            }
            if (ch == '"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == '"')
        {
            inString = true;
            continue;
        }

        if (ch == '{')
        {
            if (depth == 0) start = i;
            ++depth;
        }
        else if (ch == '}')
        {
            if (depth == 0) continue;
            --depth;
            if (depth == 0 && start != -1)
            {
                objects << text.mid(start, i - start + 1);
                start = -1;
            }
        }
    }

    return objects;
}
} // namespace



// 添加右击问题
void Widget::create_right_menu()
{
    QDate currentDate = QDate::currentDate(); // 历史中的今天
    QString dateString = currentDate.toString("M" + QString(" ") + jtr("month") + QString(" ") + "d" + QString(" ") + jtr("day"));
    //---------------创建一般问题菜单--------------
    if (right_menu != nullptr)
    {
        delete right_menu;
    }
    right_menu = new QMenu(this);
    for (int i = 1; i < 14; ++i)
    {
        QString question;
        if (i == 4)
        {
            question = jtr(QString("Q%1").arg(i)).replace("{today}", dateString);
        } // 历史中的今天
        else
        {
            question = jtr(QString("Q%1").arg(i));
        }
        QAction *action = right_menu->addAction(question);
        connect(action, &QAction::triggered, this, [=]()
                { ui->input->textEdit->setPlainText(question); });
    }
    //------------创建自动化问题菜单-------------
    // 上传图像
    QAction *action14 = right_menu->addAction(jtr("Q14"));
    connect(action14, &QAction::triggered, this, [=]()
            {
        //用户选择图片
        QStringList paths = QFileDialog::getOpenFileNames(nullptr, jtr("Q14"), currentpath, "(*.png *.jpg *.bmp)");
        ui->input->addFiles(paths); });
    // 历史对话入口（打开管理界面）
    right_menu->addSeparator();
    QAction *histMgr = right_menu->addAction(jtr("history sessions"));
    connect(histMgr, &QAction::triggered, this, [this]()
            { openHistoryManager(); });
}
// 添加托盘右击事件
void Widget::create_tray_right_menu()
{
    trayMenu->clear();
    QAction *showAction_shortcut = trayMenu->addAction(jtr("shortcut"));
    QAction *blank1 = trayMenu->addAction(""); // 占位符
    QAction *blank2 = trayMenu->addAction(""); // 占位符，目的是把截图顶出去，用户点击后才会隐藏
    blank1->setEnabled(false);
    blank2->setEnabled(false);
    trayMenu->addSeparator(); // 添加分割线
    QAction *showAction_widget = trayMenu->addAction(jtr("show widget"));
    QAction *showAction_expend = trayMenu->addAction(jtr("show expend"));
    trayMenu->addSeparator(); // 添加分割线
    QAction *exitAction = trayMenu->addAction(jtr("quit"));
    QObject::connect(showAction_widget, &QAction::triggered, this, [&]()
                     {
                         toggleWindowVisibility(this, true); // 显示窗体
                     });
    QObject::connect(showAction_expend, &QAction::triggered, this, [&]()
                     { emit ui2expend_show(PREV_WINDOW); });
    QObject::connect(showAction_shortcut, &QAction::triggered, this, [&]()
                     {
                         trayMenu->hide();
                         QTimer::singleShot(100, [this]()
                                            { onShortcutActivated_F1(); }); // 触发截图
                     });
    QObject::connect(exitAction, &QAction::triggered, QApplication::quit); // 退出程序
}
// 获取设置中的纸面值
void Widget::get_set()
{
    ui_SETTINGS.temp = settings_ui->temp_slider->value() / 100.0;
    ui_SETTINGS.repeat = settings_ui->repeat_slider->value() / 100.0;
    ui_SETTINGS.hid_parallel = settings_ui->parallel_slider->value();
    ui_SETTINGS.top_k = settings_ui->topk_slider->value();
    ui_SETTINGS.hid_top_p = settings_ui->topp_slider->value() / 100.0;
    ui_SETTINGS.nthread = settings_ui->nthread_slider->value();
    ui_SETTINGS.nctx = settings_ui->nctx_slider->value(); // 获取nctx滑块的值
    ui_SETTINGS.ngl = settings_ui->ngl_slider->value();   // 获取ngl滑块的值
    ui_SETTINGS.lorapath = settings_ui->lora_LineEdit->text();
    ui_SETTINGS.mmprojpath = settings_ui->mmproj_LineEdit->text();
    const int newLazyMinutes = settings_ui->lazy_timeout_spin ? settings_ui->lazy_timeout_spin->value() : qMax(0, int(lazyUnloadMs_ / 60000));
    lazyUnloadMs_ = qMax(0, newLazyMinutes) * 60000;
    if (lazyUnloadMs_ <= 0)
    {
        lazyUnloaded_ = false;
        if (lazyUnloadTimer_) lazyUnloadTimer_->stop();
        if (lazyCountdownTimer_) lazyCountdownTimer_->stop();
    }
    else if (lazyUnloadTimer_)
    {
        if (backendOnline_ && !turnActive_ && !toolInvocationActive_)
        {
            lazyUnloadTimer_->start(lazyUnloadMs_);
        }
    }
    updateLazyCountdownLabel();
    ui_SETTINGS.complete_mode = settings_ui->complete_btn->isChecked();
    ui_SETTINGS.hid_npredict = qBound(1, settings_ui->npredict_spin->value(), 99999);
    if (settings_ui->reasoning_comboBox)
    {
        ui_SETTINGS.reasoning_effort = sanitizeReasoningEffort(settings_ui->reasoning_comboBox->currentData().toString());
    }
    else
    {
        ui_SETTINGS.reasoning_effort = QStringLiteral("auto");
    }
    ui_monitor_frame = settings_ui->frame_lineEdit->text().toDouble();
    if (settings_ui->chat_btn->isChecked())
    {
        ui_state = CHAT_STATE;
    }
    else if (settings_ui->complete_btn->isChecked())
    {
        ui_state = COMPLETE_STATE;
    }
    // 服务状态已弃用
    ui_port = settings_ui->port_lineEdit->text();
    // 推理设备：同步到 DeviceManager（auto/cpu/cuda/vulkan/opencl）
    ui_device_backend = settings_ui->device_comboBox->currentText().trimmed().toLower();
    DeviceManager::setUserChoice(ui_device_backend);
}
// 获取约定中的纸面值
void Widget::get_date()
{
    ui_date_prompt = date_ui->date_prompt_TextEdit->toPlainText();
    // 合并附加指令
    if (ui_extra_prompt != "")
    {
        ui_DATES.date_prompt = ui_date_prompt + "\n\n" + ui_extra_prompt;
    }
    else
    {
        ui_DATES.date_prompt = ui_date_prompt;
    }
    ui_DATES.is_load_tool = is_load_tool;
    ui_template = date_ui->chattemplate_comboBox->currentText();
    ui_extra_lan = date_ui->switch_lan_button->text();
    ui_calculator_ischecked = date_ui->calculator_checkbox->isChecked();
    ui_engineer_ischecked = date_ui->engineer_checkbox->isChecked();
    ui_MCPtools_ischecked = date_ui->MCPtools_checkbox->isChecked();
    ui_knowledge_ischecked = date_ui->knowledge_checkbox->isChecked();
    ui_controller_ischecked = date_ui->controller_checkbox->isChecked();
    ui_stablediffusion_ischecked = date_ui->stablediffusion_checkbox->isChecked();
    // 记录自定义模板
    if (ui_template == jtr("custom set1"))
    {
        custom1_date_system = ui_date_prompt;
    }
    else if (ui_template == jtr("custom set2"))
    {
        custom2_date_system = ui_date_prompt;
    }
    // 添加额外停止标志
    addStopwords();
}
// 手搓输出解析器，提取可能的xml，目前只支持一个参数
mcp::json Widget::XMLparser(const QString &text, QStringList *debugLog)
{
    static const QRegularExpression thinkBlock(QStringLiteral("<think>.*?</think>"),
                                               QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression toolBlock(QStringLiteral("<tool_call>(.*?)</tool_call>"),
                                              QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression leadingFence(QStringLiteral("^```[a-zA-Z0-9_+\\-]*\\s*"));
    static const QRegularExpression trailingFence(QStringLiteral("\\s*```\\s*$"));

    const auto logStep = [&](const QString &line)
    {
        if (debugLog) debugLog->append(line);
    };
    const auto logText = [&](const QString &key, const QString &fallback) -> QString {
        const QString textValue = jtr(key);
        return textValue.isEmpty() ? fallback : textValue;
    };

    QString payload = text;
    logStep(logText(QStringLiteral("toolcall xml log input length"), QStringLiteral("Input length: %1 chars")).arg(payload.size()));
    payload.remove(thinkBlock);
    if (payload.size() != text.size())
    {
        logStep(logText(QStringLiteral("toolcall xml log think stripped"), QStringLiteral("Removed <think> block, remaining length: %1"))
                    .arg(payload.size()));
    }

    QStringList tagCandidates;
    QSet<QString> deduped;
    QRegularExpressionMatchIterator it = toolBlock.globalMatch(payload);
    while (it.hasNext())
    {
        const QString captured = it.next().captured(1);
        const QString normalized = captured.trimmed();
        if (normalized.isEmpty()) continue;
        if (deduped.contains(normalized)) continue;
        deduped.insert(normalized);
        tagCandidates.append(captured);
    }
    logStep(logText(QStringLiteral("toolcall xml log tag count"), QStringLiteral("Found <tool_call> candidates: %1")).arg(tagCandidates.size()));

    mcp::json result; // empty by default
    auto parseCandidateList = [&](const QStringList &list, const QString &sourceLabel) -> bool
    {
        if (list.isEmpty())
        {
            logStep(logText(QStringLiteral("toolcall xml log list empty"), QStringLiteral("%1 candidates empty, skip.")).arg(sourceLabel));
            return false;
        }
        logStep(logText(QStringLiteral("toolcall xml log list try"), QStringLiteral("Trying %1 candidates: %2"))
                    .arg(sourceLabel)
                    .arg(list.size()));
        for (int idx = list.size() - 1; idx >= 0; --idx)
        {
            QString content = list.at(idx).trimmed();
            if (content.isEmpty())
            {
                logStep(logText(QStringLiteral("toolcall xml log slot empty"), QStringLiteral("%1 candidate #%2 empty"))
                            .arg(sourceLabel)
                            .arg(idx + 1));
                continue;
            }

            content.remove(leadingFence);
            content.remove(trailingFence);
            content = content.trimmed();
            if (content.startsWith(QStringLiteral("json"), Qt::CaseInsensitive))
            {
                const int newlineIndex = content.indexOf('\n');
                if (newlineIndex > -1)
                {
                    content = content.mid(newlineIndex + 1).trimmed();
                }
                else
                {
                    content.clear();
                }
            }
            if (content.isEmpty())
            {
                continue;
            }

            QString candidateJson = content;
            const int firstBrace = candidateJson.indexOf('{');
            const int lastBrace = candidateJson.lastIndexOf('}');
            if (firstBrace == -1 || lastBrace <= firstBrace)
            {
                qWarning() << "tool_call candidate missing braces in" << sourceLabel << "slot" << idx;
                continue;
            }
            candidateJson = candidateJson.mid(firstBrace, lastBrace - firstBrace + 1).trimmed();
            if (candidateJson.isEmpty())
            {
                continue;
            }

            const QByteArray jsonBytes = candidateJson.toUtf8();
            try
            {
                result = mcp::json::parse(jsonBytes.constData(), jsonBytes.constData() + jsonBytes.size());
                qDebug() << "parsed tool_call from" << sourceLabel << "slot" << idx;
                logStep(logText(QStringLiteral("toolcall xml log parse success"), QStringLiteral("Parsed %1 candidate #%2"))
                            .arg(sourceLabel)
                            .arg(idx + 1));
                return true;
            }
            catch (const std::exception &e)
            {
                qCritical() << "tool JSON parse error from" << sourceLabel << "slot" << idx << ":" << e.what();
                logStep(logText(QStringLiteral("toolcall xml log parse error"), QStringLiteral("%1 candidate #%2 parse error: %3"))
                            .arg(sourceLabel)
                            .arg(idx + 1)
                            .arg(QString::fromUtf8(e.what())));
            }
        }
        return false;
    };

    if (parseCandidateList(tagCandidates, QStringLiteral("tagged")))
    {
        return result;
    }

    QStringList fallbackCandidates;
    const QStringList looseObjects = collectLooseJsonObjects(payload);
    logStep(logText(QStringLiteral("toolcall xml log loose count"), QStringLiteral("Loose JSON fragments: %1")).arg(looseObjects.size()));
    for (const QString &obj : looseObjects)
    {
        const QString normalized = obj.trimmed();
        if (normalized.isEmpty()) continue;
        if (!normalized.contains(QStringLiteral("\"name\"")) || !normalized.contains(QStringLiteral("\"arguments\"")))
        {
            continue;
        }
        if (deduped.contains(normalized)) continue;
        deduped.insert(normalized);
        fallbackCandidates.append(obj);
    }

    if (!fallbackCandidates.isEmpty())
    {
        qWarning() << "tool_call tags missing or invalid, attempting loose JSON fallback";
        logStep(logText(QStringLiteral("toolcall xml log fallback attempt"),
                        QStringLiteral("Missing <tool_call> tags, trying loose candidates: %1"))
                    .arg(fallbackCandidates.size()));
        if (parseCandidateList(fallbackCandidates, QStringLiteral("fallback")))
        {
            return result;
        }
    }

    qDebug() << "no valid tool_call found";
    logStep(logText(QStringLiteral("toolcall xml log none found"), QStringLiteral("No usable tool-call snippet found.")));
    return result;
}
// 构建额外指令
QString Widget::create_extra_prompt()
{
    QString extra_prompt_;            // 额外指令
    QString available_tools_describe; // 工具名和描述
    QString skill_usage_block;
    QString engineer_info;            // 软件工程师信息
    extra_prompt_ = promptx::extraPromptTemplate();
    extra_prompt_.replace("{OBSERVATION_STOPWORD}", DEFAULT_OBSERVATION_STOPWORD);
    if (is_load_tool)
    {
        if (skillManager && date_ui && date_ui->engineer_checkbox && date_ui->engineer_checkbox->isChecked())
        {
            const QString skillBlock = skillManager->composePromptBlock(engineerWorkDir, true);
            if (!skillBlock.isEmpty()) skill_usage_block = skillBlock;
        }
        available_tools_describe += promptx::toolAnswer().text + "\n\n";
        // qDebug()<< MCP_TOOLS_INFO_LIST.size();
        if (date_ui->MCPtools_checkbox->isChecked())
        {
            QStringList mcpToolEntries;
            for (const auto &info : MCP_TOOLS_INFO_LIST)
            {
                mcpToolEntries << info.text;
            }
            if (!mcpToolEntries.isEmpty())
            {
                available_tools_describe += mcpToolEntries.join("\n") + "\n\n";
            }
        }
        if (date_ui->calculator_checkbox->isChecked())
        {
            available_tools_describe += promptx::toolCalculator().text + "\n\n";
        }
        if (date_ui->knowledge_checkbox->isChecked())
        {
            QString knowledgeText = promptx::toolKnowledge().text;
            knowledgeText.replace("{embeddingdb describe}", embeddingdb_describe);
            available_tools_describe += knowledgeText + "\n\n";
        }
        if (date_ui->stablediffusion_checkbox->isChecked())
        {
            available_tools_describe += promptx::toolStableDiffusion().text + "\n\n";
        }
        if (date_ui->controller_checkbox->isChecked())
        {
            screen_info = create_screen_info(); // 构建屏幕信息
            QString controllerText = promptx::toolController().text;
            controllerText.replace("{screen_info}", screen_info);
            available_tools_describe += controllerText + "\n\n";
        }
        if (date_ui->engineer_checkbox->isChecked())
        {
            available_tools_describe += promptx::toolExecuteCommand().text + "\n\n";
            available_tools_describe += promptx::toolReadFile().text + "\n\n";
            available_tools_describe += promptx::toolWriteFile().text + "\n\n";
            available_tools_describe += promptx::toolReplaceInFile().text + "\n\n";
            available_tools_describe += promptx::toolEditInFile().text + "\n\n";
            available_tools_describe += promptx::toolListFiles().text + "\n\n";
            available_tools_describe += promptx::toolSearchContent().text + "\n\n";
            // 这里添加更多工程师的工具
            engineer_info = create_engineer_info(); // 构建工程师信息
        }

        extra_prompt_.replace("{available_tools_describe}", available_tools_describe); // 替换相应内容
        extra_prompt_.replace("{engineer_info}", engineer_info);                       // 替换相应内容
        if (!skill_usage_block.isEmpty())
        {
            extra_prompt_ = extra_prompt_ + skill_usage_block;// 技能描述放到最后
        }
    }
    else
    {
        extra_prompt_ = ""; // 没有挂载工具则为空
    }
    return extra_prompt_;
}

void Widget::recv_mcp_tools_changed()
{
    ui_extra_prompt = create_extra_prompt();
    if (date_ui)
    {
        get_date();
        auto_save_user();
    }
}

QString Widget::truncateString(const QString &str, int maxLength)
{
    if (str.size() <= maxLength)
    {
        return str;
    }
    // 使用QTextStream来处理多字节字符
    QTextStream stream(const_cast<QString *>(&str), QIODevice::ReadOnly);
    stream.setCodec("UTF-8");
    // 找到开始截取的位置
    int startIndex = str.size() - maxLength;
    // 确保不截断多字节字符
    stream.seek(startIndex);
    return QString(stream.readAll());
}
QString Widget::checkPython()
{
    const QString projDir = applicationDirPath;
    QStringList lines;

    ExecSpec spec = DependencyResolver::discoverPython3(projDir);
    if (spec.program.isEmpty())
    {
        lines << QStringLiteral("Python: not found");
    }
    else
    {
        QString version = DependencyResolver::pythonVersion(spec);
        if (version.isEmpty()) version = QStringLiteral("Python 3");
        QString sourceHint;
        if (spec.program.compare(QStringLiteral("py"), Qt::CaseInsensitive) == 0)
        {
            sourceHint = QStringLiteral(" via py -3");
        }
        else if (spec.absolutePath.contains(QStringLiteral(".venv"), Qt::CaseInsensitive) ||
                 spec.absolutePath.contains(QStringLiteral("/venv/"), Qt::CaseInsensitive) ||
                 spec.absolutePath.contains(QStringLiteral("\\venv\\"), Qt::CaseInsensitive))
        {
            sourceHint = QStringLiteral(" from project venv");
        }
        lines << QStringLiteral("Python: %1%2").arg(version, sourceHint);
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    auto versionLine = [&](const QString &program, const QStringList &args) -> QString {
        ProcessResult result = ProcessRunner::run(program, args, projDir, env, 3000);
        if (result.exitCode != 0) return QString();
        QString text = result.stdOut.isEmpty() ? result.stdErr : result.stdOut;
        text = text.trimmed();
        if (text.isEmpty()) return QString();
        const int nl = text.indexOf('\n');
        if (nl != -1) text = text.left(nl);
        return text.trimmed();
    };

    const QString gitLine = versionLine(QStringLiteral("git"), {QStringLiteral("--version")});
    lines << (gitLine.isEmpty() ? QStringLiteral("git: not found") : QStringLiteral("git: %1").arg(gitLine));

    const QString cmakeLine = versionLine(QStringLiteral("cmake"), {QStringLiteral("--version")});
    lines << (cmakeLine.isEmpty() ? QStringLiteral("cmake: not found") : QStringLiteral("cmake: %1").arg(cmakeLine));

    return lines.join('\n') + QStringLiteral("\n");
}
QString Widget::checkCompile()
{
    QString compilerInfo;
    QProcess process;
    // Windows平台的编译器检查
#ifdef Q_OS_WIN
    // 尝试检查 MinGW
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "g++ --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            QString versionLine = lines.first();
            QRegExp regExp("\\s*\\(.*\\)"); // 使用正则表达式去除括号中的内容 匹配括号及其中的内容
            versionLine = versionLine.replace(regExp, "");
            compilerInfo += "MinGW version: ";
            compilerInfo += versionLine.trimmed(); // 去除前后的空格
            compilerInfo += "\n";
        }
    }
    // 检查 MSVC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "cl /Bv";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QByteArray output = process.readAllStandardOutput();
        output += process.readAllStandardError();
        if (!output.isEmpty())
        {
            QString outputStr = QString::fromLocal8Bit(output);
            QStringList lines = outputStr.split('\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty())
            {
                compilerInfo += "MSVC version: ";
                compilerInfo += lines.first().trimmed();
                compilerInfo += "\n";
            }
        }
    }
    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            compilerInfo += "Clang version: ";
            compilerInfo += lines.first().trimmed();
            compilerInfo += "\n";
        }
    }
#endif
    // Linux平台的编译器检查
#ifdef Q_OS_LINUX
    // 检查 GCC
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "gcc --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            compilerInfo += "GCC version: ";
            compilerInfo += lines.first().trimmed();
            compilerInfo += "\n";
        }
    }
    // 检查 Clang
    {
        QStringList shellArgs;
        shellArgs << CMDGUID << "clang --version";
        process.start(shell, shellArgs);
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty())
        {
            compilerInfo += "Clang version: ";
            compilerInfo += lines.first().trimmed();
            compilerInfo += "\n";
        }
    }
#endif
    // 如果没有找到任何编译器信息
    if (compilerInfo.isEmpty())
    {
        compilerInfo = "No compiler detected.\n";
    }
    return compilerInfo;
}
QString Widget::checkNode()
{
    const QString workingDir = applicationDirPath;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    auto versionLine = [&](const QString &program, const QStringList &args) -> QString {
        auto pickFirstLine = [](const ProcessResult &r) -> QString {
            QString text = r.stdOut.isEmpty() ? r.stdErr : r.stdOut;
            text = text.trimmed();
            if (text.isEmpty()) return QString();
            const int nl = text.indexOf('\n');
            if (nl != -1) text = text.left(nl);
            return text.trimmed();
        };

        ProcessResult result = ProcessRunner::run(program, args, workingDir, env, 3000);
        QString text = (result.exitCode == 0) ? pickFirstLine(result) : QString();
#ifdef Q_OS_WIN
        if (text.isEmpty())
        {
            QString cmdLine = program;
            for (const QString &arg : args)
            {
                if (arg.isEmpty()) continue;
                if (arg.contains(' '))
                {
                    cmdLine += QStringLiteral(" \"") + arg + QStringLiteral("\"");
                }
                else
                {
                    cmdLine += QStringLiteral(" ") + arg;
                }
            }
            ProcessResult shellResult = ProcessRunner::runShellCommand(cmdLine.trimmed(), workingDir, env, 3000);
            text = pickFirstLine(shellResult);
        }
#endif
        return text;
    };

    QStringList lines;
    const QString nodeLine = versionLine(QStringLiteral("node"), {QStringLiteral("--version")});
    lines << (nodeLine.isEmpty() ? QStringLiteral("node: not found") : QStringLiteral("node: %1").arg(nodeLine));
    const QString npmLine = versionLine(QStringLiteral("npm"), {QStringLiteral("--version")});
    lines << (npmLine.isEmpty() ? QStringLiteral("npm: not found") : QStringLiteral("npm: %1").arg(npmLine));
    return lines.join('\n') + QStringLiteral("\n");
}

// 添加额外停止标志，本地模式时在xbot.cpp里已经现若同时包含"<|" 和 "|>"也停止
void Widget::addStopwords()
{
    ui_DATES.extra_stop_words.clear(); // 重置额外停止标志
    if (ui_DATES.is_load_tool)         // 如果挂载了工具则增加额外停止标志
    {
        // ui_DATES.extra_stop_words << DEFAULT_OBSERVATION_STOPWORD;//在后端已经处理了
    }
}
// 获取本机第一个ip地址 排除以.1结尾的地址 如果只有一个.1结尾的则保留它
QString Widget::getFirstNonLoopbackIPv4Address()
{
    QList<QHostAddress> list = QNetworkInterface::allAddresses();
    QString ipWithDot1; // 用于存储以.1结尾的IP地址
    for (int i = 0; i < list.count(); i++)
    {
        QString ip = list[i].toString();
        // 排除回环地址和非IPv4地址
        if (!list[i].isLoopback() && list[i].protocol() == QAbstractSocket::IPv4Protocol)
        {
            if (ip.endsWith(".1"))
            {
                ipWithDot1 = ip; // 记录以.1结尾的IP地址
            }
            else
            {
                return ip; // 返回第一个不以.1结尾的IP地址
            }
        }
    }
    // 如果没有找到不以.1结尾的IP地址，则返回以.1结尾的IP地址
    if (!ipWithDot1.isEmpty())
    {
        return ipWithDot1;
    }
    return QString(); // 如果没有找到任何符合条件的IP地址，返回空字符串
}
// 服务模式已移除：server_onProcessStarted/server_onProcessFinished
// llama-bench进程结束响应
void Widget::bench_onProcessFinished()
{
    qDebug() << "llama-bench进程结束响应";
}

// 更新gpu内存使用率
void Widget::updateGpuStatus()
{
    emit gpu_reflash();
}
// 更新cpu内存使用率
void Widget::updateCpuStatus()
{
    emit cpu_reflash();
}

// 创建临时文件夹EVA_TEMP
bool Widget::createTempDirectory(const QString &path)
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
// 打开文件夹
QString Widget::customOpenfile(QString dirpath, QString describe, QString format)
{
    QString filepath = "";
    filepath = QFileDialog::getOpenFileName(nullptr, describe, dirpath, format);
    return filepath;
}
// 语音朗读相关 文转声相关
// 每次约定和设置后都保存配置到本地
void Widget::auto_save_user()
{
    //--------------保存当前用户配置---------------
    // 创建 QSettings 对象，指定配置文件的名称和格式
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    QSettings settings(applicationDirPath + "/EVA_TEMP/eva_config.ini", QSettings::IniFormat);
    settings.setIniCodec("utf-8");
    settings.setValue("ui_mode", ui_mode);         // 机体模式
    settings.setValue("ui_state", ui_state);       // 机体状态
    settings.setValue("shell", shell);             // shell路径
    settings.setValue("python", pythonExecutable); // python版本
    settings.setValue("global_font_family", globalUiSettings_.fontFamily);
    settings.setValue("global_font_size", globalUiSettings_.fontSizePt);
    settings.setValue("output_font_family", globalUiSettings_.outputFontFamily);
    settings.setValue("output_font_size", globalUiSettings_.outputFontSizePt);
    settings.setValue("global_theme", globalUiSettings_.themeId);
    // 保存设置参数
    settings.setValue("modelpath", ui_SETTINGS.modelpath); // 模型路径
    // Persist core sampling params as strings only to avoid float drift on reload
    settings.setValue("temp_str", QString::number(ui_SETTINGS.temp, 'f', 6));
    settings.setValue("repeat_str", QString::number(ui_SETTINGS.repeat, 'f', 6));
    settings.setValue("top_k", ui_SETTINGS.top_k);     // top-k 采样
    settings.setValue("ngl", ui_SETTINGS.ngl);         // gpu负载层数
    settings.setValue("nthread", ui_SETTINGS.nthread); // cpu线程数
    settings.setValue("nctx", ui_SETTINGS.nctx);
    settings.setValue("mmprojpath", ui_SETTINGS.mmprojpath); // 视觉
    settings.setValue("lorapath", ui_SETTINGS.lorapath);     // lora
    settings.setValue("monitor_frame", ui_monitor_frame);    // 监视帧率
    // 保存隐藏设置
    settings.setValue("hid_npredict", ui_SETTINGS.hid_npredict); // 最大输出长度
    settings.setValue("hid_top_p_str", QString::number(ui_SETTINGS.hid_top_p, 'f', 6));
    // Clean legacy percent keys and numeric keys to keep only string keys
    settings.remove("hid_top_p_percent");
    settings.remove("temp_percent");
    settings.remove("repeat_percent");
    settings.remove("temp");
    settings.remove("repeat");
    settings.remove("hid_top_p");
    settings.setValue("hid_batch", ui_SETTINGS.hid_batch);
    settings.setValue("hid_n_ubatch", ui_SETTINGS.hid_n_ubatch);
    settings.setValue("hid_use_mmap", ui_SETTINGS.hid_use_mmap);
    settings.setValue("hid_use_mlock", ui_SETTINGS.hid_use_mlock);
    settings.setValue("hid_flash_attn", ui_SETTINGS.hid_flash_attn);
    settings.setValue("hid_parallel", ui_SETTINGS.hid_parallel);
    settings.setValue("reasoning_effort", ui_SETTINGS.reasoning_effort);
    settings.setValue("port", ui_port);                     // 服务端口
    settings.setValue("device_backend", ui_device_backend); // 推理设备auto/cpu/cuda/vulkan/opencl
    settings.setValue("lazy_unload_minutes", lazyUnloadMs_ / 60000); // 惰性卸载(分钟)
    settings.setValue("chattemplate", date_ui->chattemplate_comboBox->currentText()); // 对话模板
    QStringList enabledTools;
    auto appendTool = [&](QCheckBox *box, const QString &id) {
        if (box && box->isChecked()) enabledTools << id;
    };
    appendTool(date_ui->calculator_checkbox, QStringLiteral("calculator"));
    appendTool(date_ui->knowledge_checkbox, QStringLiteral("knowledge"));
    appendTool(date_ui->controller_checkbox, QStringLiteral("controller"));
    appendTool(date_ui->stablediffusion_checkbox, QStringLiteral("stablediffusion"));
    appendTool(date_ui->engineer_checkbox, QStringLiteral("engineer"));
    appendTool(date_ui->MCPtools_checkbox, QStringLiteral("mcp"));
    settings.setValue("enabled_tools", enabledTools);
    settings.setValue("calculator_checkbox", date_ui->calculator_checkbox->isChecked());           // 计算器工具
    settings.setValue("knowledge_checkbox", date_ui->knowledge_checkbox->isChecked());             // knowledge工具
    settings.setValue("controller_checkbox", date_ui->controller_checkbox->isChecked());           // controller工具
    settings.setValue("stablediffusion_checkbox", date_ui->stablediffusion_checkbox->isChecked()); // 计算器工具
    settings.setValue("engineer_checkbox", date_ui->engineer_checkbox->isChecked());               // engineer工具
    settings.setValue("MCPtools_checkbox", date_ui->MCPtools_checkbox->isChecked());               // MCPtools工具
    settings.setValue("engineer_work_dir", engineerWorkDir);                                       // 工程师工作目录
    settings.setValue("extra_lan", ui_extra_lan);                                                  // 额外指令语种
    // 保存自定义的约定模板
    settings.setValue("custom1_date_system", custom1_date_system);
    // 保存 api 参数：仅在链接模式下更新，避免切到本地模式后把远端配置覆盖掉
    if (ui_mode == LINK_MODE)
    {
        settings.setValue("api_endpoint", apis.api_endpoint);
        settings.setValue("api_key", apis.api_key);
        settings.setValue("api_model", apis.api_model);
    }
    if (skillManager)
    {
        settings.setValue("skills_enabled", skillManager->enabledSkillIds());
    }
    else
    {
        settings.remove("skills_enabled");
    }
    settings.sync(); // flush to disk immediately
    // reflash_state("ui:" + jtr("save_config_mess"), USUAL_SIGNAL);
}
