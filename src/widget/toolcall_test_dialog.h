#ifndef TOOLCALL_TEST_DIALOG_H
#define TOOLCALL_TEST_DIALOG_H

#include <QDialog>
#include <QStringList>
#include <functional>

class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTextBrowser;

class ToolCallTestDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit ToolCallTestDialog(QWidget *parent = nullptr);
    void displayReport(const QStringList &logLines, const QString &jsonDump, const QString &summary, bool success);
    void focusInput();
    void refreshTranslations();
    void setTranslator(const std::function<QString(const QString &, const QString &)> &translator);

  signals:
    void testRequested(const QString &inputText);

  private slots:
    void emitTestRequest();
    void clearAll();

  private:
    void setBusy(bool busy);
    QString trText(const QString &key, const QString &fallback) const;

    QPlainTextEdit *inputEdit_ = nullptr;
    QTextBrowser *logView_ = nullptr;
    QLabel *resultLabel_ = nullptr;
    QPushButton *testButton_ = nullptr;
    QLabel *hintLabel_ = nullptr;
    QPushButton *clearButton_ = nullptr;
    QPushButton *closeButton_ = nullptr;
    std::function<QString(const QString &, const QString &)> translator_;
    bool hasResult_ = false;
};

#endif // TOOLCALL_TEST_DIALOG_H
