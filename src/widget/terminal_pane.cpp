#include "terminal_pane.h"

#include <QBoxLayout>
#include <QColor>
#include <QKeySequence>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QShortcut>
#include <QtGlobal>
#include <QTextCursor>
#include <QTextCharFormat>

namespace
{
constexpr int kMaxBlocks = 4000;

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
    output_ = new QPlainTextEdit(this);
    output_->setReadOnly(true);
    output_->setFrameShape(QFrame::NoFrame);
    output_->setMaximumBlockCount(kMaxBlocks);
    output_->setUndoRedoEnabled(false);

    input_ = new QLineEdit(this);
    input_->setPlaceholderText(tr("Enter command and press Enter"));

    interruptButton_ = new QPushButton(tr("Stop"), this);
    interruptButton_->setToolTip(tr("Send Ctrl+C to stop the current command"));

    auto *controls = new QHBoxLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(8);
    controls->addWidget(input_);
    controls->addWidget(interruptButton_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(output_);
    layout->addLayout(controls);

    connect(input_, &QLineEdit::returnPressed, this, &TerminalPane::handleReturnPressed);
    connect(interruptButton_, &QPushButton::clicked, this, &TerminalPane::handleInterrupt);

    auto *shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, &TerminalPane::handleInterrupt);

    updateControls();
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
    if (manualRunning_ || externalRunning_)
    {
        appendChunk(tr("A command is already running. Interrupt it before starting a new one.\n"), Channel::System);
        return;
    }

    const QString command = input_->text().trimmed();
    if (command.isEmpty()) return;
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
    QTextCursor cursor = output_->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat format = cursor.charFormat();
    switch (channel)
    {
    case Channel::StdOut:
        format.setForeground(QColor(Qt::black));
        break;
    case Channel::StdErr:
        format.setForeground(QColor(202, 62, 71));
        break;
    case Channel::System:
        format.setForeground(QColor(70, 86, 122));
        break;
    }

    cursor.setCharFormat(format);
    cursor.insertText(text);
    output_->setTextCursor(cursor);
    output_->ensureCursorVisible();
}

void TerminalPane::startManualCommand(const QString &command)
{
    appendChunk(makePrompt(command, QString()), Channel::System);
    manualProcess_ = new QProcess(this);
    manualProcess_->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    manualProcess_->setProcessChannelMode(QProcess::SeparateChannels);

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
    const bool busy = manualRunning_ || externalRunning_;
    input_->setEnabled(!busy);
    interruptButton_->setEnabled(busy);
}
