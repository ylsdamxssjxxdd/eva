#include "terminal_pane.h"

#include <QBoxLayout>
#include <QColor>
#include <QDir>
#include <QFontDatabase>
#include <QKeySequence>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QtGlobal>

namespace
{
constexpr int kMaxBlocks = 4000;
constexpr int kMaxCharacters = 250000;
constexpr int kTrimBatchBlocks = 200;
constexpr int kFlushIntervalMs = 30;
const QColor kStdOutGreen(90, 247, 141);
const QColor kStdErrRed(255, 123, 109);
const QColor kSystemGray(135, 158, 189);

QString makePrompt(const QString &command, const QString &workingDir)
{
    if (workingDir.isEmpty()) return QStringLiteral("$ %1\n").arg(command);
    return QStringLiteral("[%1]$ %2\n").arg(workingDir, command);
}

QString decodeBytes(const QByteArray &bytes)
{
#ifdef Q_OS_WIN
    return QString::fromLocal8Bit(bytes);
#else
    return QString::fromUtf8(bytes);
#endif
}
} // namespace

TerminalPane::TerminalPane(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);

    output_ = new QPlainTextEdit(this);
    output_->setObjectName(QStringLiteral("terminalOutput"));
    output_->setReadOnly(true);
    output_->setFrameShape(QFrame::NoFrame);
    output_->setMaximumBlockCount(kMaxBlocks);
    output_->setUndoRedoEnabled(false);
    output_->setWordWrapMode(QTextOption::NoWrap);
    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSizeF(qMax(10.0, monoFont.pointSizeF()));
    output_->setFont(monoFont);

    input_ = new QLineEdit(this);
    input_->setObjectName(QStringLiteral("terminalInput"));
    input_->setFont(monoFont);

    interruptButton_ = new QPushButton(tr("Stop"), this);
    interruptButton_->setObjectName(QStringLiteral("terminalInterrupt"));
    interruptButton_->setToolTip(tr("Click STOP to stop the current command"));
    interruptButton_->setCursor(Qt::PointingHandCursor);

    auto *controls = new QHBoxLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(6);
    controls->addWidget(input_);
    controls->addWidget(interruptButton_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    layout->addWidget(output_);
    layout->addLayout(controls);

    connect(input_, &QLineEdit::returnPressed, this, &TerminalPane::handleReturnPressed);
    connect(interruptButton_, &QPushButton::clicked, this, &TerminalPane::handleInterrupt);

    auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, &TerminalPane::handleInterrupt);

    // Apply a cohesive dark theme so the terminal area feels self-contained.
    const QString theme = QStringLiteral(
        "TerminalPane {"
        "  background-color: #080808;"
        "  border: 1px solid #141414;"
        "  border-radius: 8px;"
        "}"
        "TerminalPane QPlainTextEdit#terminalOutput {"
        "  background: transparent;"
        "  color: #5af78d;"
        "  border: none;"
        "  selection-background-color: #1b3521;"
        "  selection-color: #98ffc2;"
        "  padding: 0px;"
        "}"
        "TerminalPane QScrollBar {"
        "  background: #080808;"
        "  border: none;"
        "  width: 10px;"
        "  margin: 0px;"
        "}"
        "TerminalPane QScrollBar::handle {"
        "  background: #1a3324;"
        "  border-radius: 4px;"
        "}"
        "TerminalPane QScrollBar::handle:hover {"
        "  background: #25523a;"
        "}"
        "TerminalPane QScrollBar::add-line, TerminalPane QScrollBar::sub-line {"
        "  height: 0px;"
        "  width: 0px;"
        "}"
        "TerminalPane QScrollBar::add-page, TerminalPane QScrollBar::sub-page {"
        "  background: transparent;"
        "}"
        "TerminalPane QLineEdit#terminalInput {"
        "  background-color: #0c0c0c;"
        "  color: #5af78d;"
        "  border: 1px solid #1c1c1c;"
        "  border-radius: 4px;"
        "  padding: 6px 8px;"
        "}"
        "TerminalPane QLineEdit#terminalInput:disabled {"
        "  color: #335c42;"
        "  background-color: #0c0c0c;"
        "}"
        "TerminalPane QPushButton#terminalInterrupt {"
        "  background-color: #0c0c0c;"
        "  color: #5af78d;"
        "  border: 1px solid #1c1c1c;"
        "  border-radius: 4px;"
        "  padding: 6px 14px;"
        "}"
        "TerminalPane QPushButton#terminalInterrupt:hover {"
        "  border-color: #5af78d;"
        "}"
        "TerminalPane QPushButton#terminalInterrupt:pressed {"
        "  background-color: #111;"
        "}"
        "TerminalPane QPushButton#terminalInterrupt:disabled {"
        "  color: #2f4a3a;"
        "  border-color: #1c1c1c;"
        "}");
    setStyleSheet(theme);

    updateControls();

    flushTimer_ = new QTimer(this);
    flushTimer_->setSingleShot(true);
    flushTimer_->setInterval(kFlushIntervalMs);
    connect(flushTimer_, &QTimer::timeout, this, &TerminalPane::flushPendingChunks);
}

void TerminalPane::setManualWorkingDirectory(const QString &path)
{
    if (path.isEmpty())
    {
        manualWorkingDir_.clear();
        return;
    }
    manualWorkingDir_ = QDir::cleanPath(path);
}

void TerminalPane::handleExternalStart(const QString &command, const QString &workingDir)
{
    if (manualRunning_) resetManualProcess(true);
    externalRunning_ = true;
    appendChunk(makePrompt(command, workingDir), Channel::System);
    updateControls();
}

void TerminalPane::handleExternalStdout(const QString &chunk)
{
    appendChunk(chunk, Channel::StdOut);
}

void TerminalPane::handleExternalStderr(const QString &chunk)
{
    appendChunk(chunk, Channel::StdErr);
}

void TerminalPane::handleExternalFinished(int exitCode, bool interrupted)
{
    externalRunning_ = false;
    if (interrupted)
        appendChunk(tr("Process interrupted.\n"), Channel::System);
    else
        appendChunk(tr("Process finished with exit code %1.\n").arg(exitCode), Channel::System);
    updateControls();
}

void TerminalPane::handleReturnPressed()
{
    if (externalRunning_)
    {
        appendChunk(tr("A command is already running. Interrupt it before starting a new one.\n"), Channel::System);
        return;
    }

    const QString rawText = input_->text();
    if (manualRunning_)
    {
        input_->clear();
        if (!manualProcess_)
        {
            appendChunk(tr("No active process is available to receive input.\n"), Channel::System);
            return;
        }
        sendInputToManualProcess(rawText);
        return;
    }

    const QString command = rawText.trimmed();
    if (command.isEmpty())
    {
        input_->clear();
        return;
    }
    input_->clear();
    startManualCommand(command);
}

void TerminalPane::handleManualStdout()
{
    if (!manualProcess_) return;
    appendChunk(decodeBytes(manualProcess_->readAllStandardOutput()), Channel::StdOut);
}

void TerminalPane::handleManualStderr()
{
    if (!manualProcess_) return;
    appendChunk(decodeBytes(manualProcess_->readAllStandardError()), Channel::StdErr);
}

void TerminalPane::handleManualFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool interrupted = (status == QProcess::CrashExit);
    appendChunk(interrupted ? tr("Process interrupted.\n") : tr("Process finished with exit code %1.\n").arg(exitCode), Channel::System);
    resetManualProcess(interrupted);
}

void TerminalPane::handleInterrupt()
{
    if (manualRunning_ && manualProcess_)
    {
        appendChunk(tr("^C\n"), Channel::System);
        manualProcess_->kill();
        return;
    }
    if (externalRunning_)
    {
        appendChunk(tr("^C\n"), Channel::System);
        emit interruptRequested();
    }
}

void TerminalPane::appendChunk(const QString &text, Channel channel)
{
    if (text.isEmpty()) return;
    pendingChunks_.append({text, channel});

    if (!flushTimer_)
    {
        flushPendingChunks();
        return;
    }

    if (!flushTimer_->isActive())
    {
        flushTimer_->start(kFlushIntervalMs);
    }

    if (pendingChunks_.size() >= 128)
    {
        flushTimer_->stop();
        flushPendingChunks();
    }
}

void TerminalPane::startManualCommand(const QString &command)
{
    const QString promptDir = manualWorkingDir_.isEmpty() ? QString() : QDir::toNativeSeparators(manualWorkingDir_);
    appendChunk(makePrompt(command, promptDir), Channel::System);
    manualProcess_ = new QProcess(this);
    manualProcess_->setInputChannelMode(QProcess::ManagedInputChannel);
    manualProcess_->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    manualProcess_->setProcessChannelMode(QProcess::SeparateChannels);
    if (!manualWorkingDir_.isEmpty())
    {
        manualProcess_->setWorkingDirectory(manualWorkingDir_);
    }

#ifdef Q_OS_WIN
    QString program = QStringLiteral("cmd.exe");
    QStringList args{QStringLiteral("/c"), command};
#else
    QString program = QStringLiteral("/bin/sh");
    QStringList args{QStringLiteral("-lc"), command};
#endif

    connect(manualProcess_, &QProcess::readyReadStandardOutput, this, &TerminalPane::handleManualStdout);
    connect(manualProcess_, &QProcess::readyReadStandardError, this, &TerminalPane::handleManualStderr);
    connect(manualProcess_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &TerminalPane::handleManualFinished);

    manualProcess_->start(program, args);
    if (!manualProcess_->waitForStarted(3000))
    {
        appendChunk(tr("Failed to start command: %1\n").arg(manualProcess_->errorString()), Channel::System);
        resetManualProcess(true);
        return;
    }

    manualRunning_ = true;
    updateControls();
}

void TerminalPane::sendInputToManualProcess(const QString &payload)
{
    if (!manualProcess_) return;

#ifdef Q_OS_WIN
    QByteArray bytes = payload.toLocal8Bit();
#else
    QByteArray bytes = payload.toUtf8();
#endif
    bytes.append('\n');

    if (manualProcess_->write(bytes) == -1)
    {
        appendChunk(tr("Failed to send input: %1\n").arg(manualProcess_->errorString()), Channel::System);
        return;
    }

    QString echoed = payload;
    echoed.append('\n');
    appendChunk(echoed, Channel::System);
}

void TerminalPane::resetManualProcess(bool interrupted)
{
    Q_UNUSED(interrupted);
    if (manualProcess_)
    {
        manualProcess_->disconnect(this);
        manualProcess_->deleteLater();
        manualProcess_ = nullptr;
    }
    manualRunning_ = false;
    updateControls();
}

void TerminalPane::updateControls()
{
    const bool allowInput = !externalRunning_;
    input_->setEnabled(allowInput);
    if (manualRunning_)
        input_->setPlaceholderText(tr("Send input and press Enter (click STOP to interrupt)"));
    else
        input_->setPlaceholderText(tr("Enter command and press Enter"));
    interruptButton_->setEnabled(manualRunning_ || externalRunning_);
}

void TerminalPane::flushPendingChunks()
{
    if (!output_) return;

    QVector<PendingChunk> chunks;
    chunks.swap(pendingChunks_);
    if (chunks.isEmpty())
    {
        if (flushTimer_ && flushTimer_->isActive()) flushTimer_->stop();
        return;
    }

    const bool stickToBottom = isAtBottom();
    output_->setUpdatesEnabled(false);

    QTextCursor cursor = output_->textCursor();
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::End);

    for (const PendingChunk &chunk : chunks)
    {
        if (chunk.text.isEmpty()) continue;

        QTextCharFormat format = cursor.charFormat();
        switch (chunk.channel)
        {
        case Channel::StdOut:
            format.setForeground(kStdOutGreen);
            break;
        case Channel::StdErr:
            format.setForeground(kStdErrRed);
            break;
        case Channel::System:
            format.setForeground(kSystemGray);
            break;
        }
        cursor.setCharFormat(format);
        cursor.insertText(chunk.text);
    }

    cursor.endEditBlock();
    output_->setTextCursor(cursor);

    trimToMaximum();

    output_->setUpdatesEnabled(true);

    if (stickToBottom) output_->ensureCursorVisible();

    if (flushTimer_ && flushTimer_->isActive()) flushTimer_->stop();

    if (!pendingChunks_.isEmpty() && flushTimer_)
    {
        flushTimer_->start(kFlushIntervalMs);
    }
}

void TerminalPane::trimToMaximum()
{
    if (!output_) return;
    QTextDocument *doc = output_->document();
    if (!doc) return;

    const int blockOverflow = doc->blockCount() - kMaxBlocks;
    const int charOverflow = doc->characterCount() - kMaxCharacters;
    if (blockOverflow <= 0 && charOverflow <= 0) return;

    const int totalBlocks = doc->blockCount();
    if (totalBlocks <= 1 && blockOverflow <= 0) return;

    int blocksToRemove = blockOverflow > 0 ? blockOverflow + kTrimBatchBlocks : 0;
    if (blocksToRemove <= 0 && charOverflow > 0 && totalBlocks > 0)
    {
        const int approxCharsPerBlock = qMax(1, doc->characterCount() / totalBlocks);
        blocksToRemove = (charOverflow / approxCharsPerBlock) + kTrimBatchBlocks;
    }

    blocksToRemove = qMax(1, blocksToRemove);
    if (totalBlocks > 0) blocksToRemove = qMin(blocksToRemove, totalBlocks - 1);
    if (blocksToRemove <= 0) return;

    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, blocksToRemove);
    cursor.removeSelectedText();
    cursor.endEditBlock();
}

bool TerminalPane::isAtBottom() const
{
    if (!output_) return true;
    if (QScrollBar *bar = output_->verticalScrollBar())
    {
        return (bar->maximum() - bar->value()) <= 2;
    }
    return true;
}
