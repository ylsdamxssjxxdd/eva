#include "toolcall_test_dialog.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextEdit>
#include <QVBoxLayout>

ToolCallTestDialog::ToolCallTestDialog(QWidget *parent) : QDialog(parent)
{
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(640, 520);

    auto *mainLayout = new QVBoxLayout(this);
    hintLabel_ = new QLabel(this);
    hintLabel_->setWordWrap(true);
    mainLayout->addWidget(hintLabel_);

    inputEdit_ = new QPlainTextEdit(this);
    inputEdit_->setTabChangesFocus(false);
    mainLayout->addWidget(inputEdit_, 3);

    auto *buttonLayout = new QHBoxLayout();
    testButton_ = new QPushButton(this);
    clearButton_ = new QPushButton(this);
    closeButton_ = new QPushButton(this);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(testButton_);
    buttonLayout->addWidget(clearButton_);
    buttonLayout->addWidget(closeButton_);
    mainLayout->addLayout(buttonLayout);

    resultLabel_ = new QLabel(this);
    resultLabel_->setWordWrap(true);
    mainLayout->addWidget(resultLabel_);

    logView_ = new QTextBrowser(this);
    logView_->setOpenExternalLinks(false);
    logView_->setLineWrapMode(QTextEdit::NoWrap);
    mainLayout->addWidget(logView_, 4);

    connect(testButton_, &QPushButton::clicked, this, &ToolCallTestDialog::emitTestRequest);
    connect(clearButton_, &QPushButton::clicked, this, &ToolCallTestDialog::clearAll);
    connect(closeButton_, &QPushButton::clicked, this, &ToolCallTestDialog::close);

    refreshTranslations();
    clearAll();
}

void ToolCallTestDialog::displayReport(const QStringList &logLines, const QString &jsonDump, const QString &summary, bool success)
{
    setBusy(false);
    hasResult_ = true;

    QStringList lines = logLines;
    if (!summary.isEmpty())
    {
        lines << QString() << trText(QStringLiteral("toolcall dialog summary header"), QStringLiteral("[Summary]")) << summary;
    }
    if (!jsonDump.isEmpty())
    {
        lines << QString() << trText(QStringLiteral("toolcall dialog json header"), QStringLiteral("[Parsed JSON]"));
        lines << jsonDump.split('\n');
    }
    if (lines.isEmpty())
    {
        lines << trText(QStringLiteral("toolcall dialog empty log"), QStringLiteral("No log lines to display."));
    }
    logView_->setPlainText(lines.join('\n'));

    resultLabel_->setText(success ? trText(QStringLiteral("toolcall dialog success label"), QStringLiteral("Tool call parsed successfully."))
                                  : trText(QStringLiteral("toolcall dialog failure label"), QStringLiteral("No valid tool call found.")));
    QPalette pal = resultLabel_->palette();
    pal.setColor(QPalette::WindowText, success ? QColor(22, 134, 76) : QColor(180, 39, 45));
    resultLabel_->setPalette(pal);
}

void ToolCallTestDialog::focusInput()
{
    inputEdit_->setFocus();
    inputEdit_->selectAll();
}

void ToolCallTestDialog::emitTestRequest()
{
    const QString input = inputEdit_->toPlainText();
    if (input.trimmed().isEmpty())
    {
        displayReport(QStringList{trText(QStringLiteral("toolcall dialog empty input log"), QStringLiteral("Input is empty; nothing to parse."))},
                      QString(), QString(), false);
        return;
    }
    setBusy(true);
    emit testRequested(input);
}

void ToolCallTestDialog::clearAll()
{
    hasResult_ = false;
    inputEdit_->clear();
    logView_->clear();
    if (resultLabel_)
    {
        resultLabel_->setText(trText(QStringLiteral("toolcall dialog idle label"), QStringLiteral("Waiting for input to parse.")));
    }
    setBusy(false);
}

void ToolCallTestDialog::setBusy(bool busy)
{
    if (!testButton_) return;
    testButton_->setEnabled(!busy);
    const QString key = busy ? QStringLiteral("toolcall dialog busy button") : QStringLiteral("toolcall dialog test button");
    const QString fallback = busy ? QStringLiteral("Parsing…") : QStringLiteral("Test");
    testButton_->setText(trText(key, fallback));
}

void ToolCallTestDialog::refreshTranslations()
{
    setWindowTitle(trText(QStringLiteral("toolcall dialog title"), QStringLiteral("Tool-call inspector")));
    if (hintLabel_)
    {
        hintLabel_->setText(trText(QStringLiteral("toolcall dialog hint"),
                                   QStringLiteral("Paste a tool-call response or type one manually, then click “Test” to inspect parsing.")));
    }
    if (inputEdit_)
    {
        inputEdit_->setPlaceholderText(
            trText(QStringLiteral("toolcall dialog placeholder"), QStringLiteral("<tool_call>{\"name\":\"\",\"arguments\":{}}</tool_call>")));
    }
    if (clearButton_)
    {
        clearButton_->setText(trText(QStringLiteral("toolcall dialog clear button"), QStringLiteral("Clear")));
    }
    if (closeButton_)
    {
        closeButton_->setText(trText(QStringLiteral("toolcall dialog close button"), QStringLiteral("Close")));
    }
    if (logView_)
    {
        logView_->setPlaceholderText(
            trText(QStringLiteral("toolcall dialog log placeholder"), QStringLiteral("Parser logs will appear here.")));
    }
    if (!hasResult_ && resultLabel_)
    {
        resultLabel_->setText(trText(QStringLiteral("toolcall dialog idle label"), QStringLiteral("Waiting for input to parse.")));
    }
    if (testButton_)
    {
        const bool busy = !testButton_->isEnabled();
        const QString key = busy ? QStringLiteral("toolcall dialog busy button") : QStringLiteral("toolcall dialog test button");
        const QString fallback = busy ? QStringLiteral("Parsing…") : QStringLiteral("Test");
        testButton_->setText(trText(key, fallback));
    }
}

void ToolCallTestDialog::setTranslator(const std::function<QString(const QString &, const QString &)> &translator)
{
    translator_ = translator;
    refreshTranslations();
}

QString ToolCallTestDialog::trText(const QString &key, const QString &fallback) const
{
    if (translator_)
    {
        const QString text = translator_(key, fallback);
        if (!text.isEmpty()) return text;
    }
    return fallback;
}
