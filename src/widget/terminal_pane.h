#ifndef TERMINAL_PANE_H
#define TERMINAL_PANE_H

#include <QProcess>
#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// Simple command runner panel with streaming output and interrupt support.
class TerminalPane : public QWidget
{
    Q_OBJECT

  public:
    explicit TerminalPane(QWidget *parent = nullptr);

    void setManualWorkingDirectory(const QString &path);
    void handleExternalStart(const QString &command, const QString &workingDir);
    void handleExternalStdout(const QString &chunk);
    void handleExternalStderr(const QString &chunk);
    void handleExternalFinished(int exitCode, bool interrupted);

  signals:
    void interruptRequested();

  private slots:
    void handleReturnPressed();
    void handleManualStdout();
    void handleManualStderr();
    void handleManualFinished(int exitCode, QProcess::ExitStatus status);
    void handleInterrupt();

  private:
    enum class Channel
    {
        StdOut,
        StdErr,
        System
    };

    void appendChunk(const QString &text, Channel channel);
    void startManualCommand(const QString &command);
    void resetManualProcess(bool interrupted);
    void updateControls();

    QPlainTextEdit *output_ = nullptr;
    QLineEdit *input_ = nullptr;
    QPushButton *interruptButton_ = nullptr;
    QProcess *manualProcess_ = nullptr;
    bool manualRunning_ = false;
    bool externalRunning_ = false;
    QString manualWorkingDir_;
};

#endif // TERMINAL_PANE_H
