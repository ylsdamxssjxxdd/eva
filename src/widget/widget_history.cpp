#include "widget.h"
#include "ui_widget.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QInputDialog>
#include <QMessageBox>

void Widget::openHistoryManager()
{
    Widget *self = this;
    if (!history_)
    {
        self->reflash_state(self->jtr("history db error"), WRONG_SIGNAL);
        return;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(self->jtr("history sessions"));
    dlg.resize(600, 420);
    QVBoxLayout *v = new QVBoxLayout(&dlg);
    QDialog *d = &dlg;
    // search bar
    QLineEdit *search = new QLineEdit(&dlg);
    search->setPlaceholderText(self->jtr("search"));
    v->addWidget(search);
    // table
    QTableWidget *table = new QTableWidget(&dlg);
    table->setColumnCount(2);
    QStringList headers;
    // Swap columns: show time first, then title
    headers << self->jtr("time");
    headers << self->jtr("title");
    table->setHorizontalHeaderLabels(headers);
    // Make the first column (time) sized to contents, second column (title) stretch
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->addWidget(table);
    auto fill = [&, self](const QString &filter)
    {
        // Limit how long a title is shown to avoid extremely wide columns
        const int kTitleMaxChars = 48; // display limit; full title is available via tooltip
        table->setRowCount(0);
        auto items = self->history_->listRecent(1000);
        for (const auto &it : items)
        {
            const QString title = it.title.isEmpty() ? QStringLiteral("(untitled)") : it.title;
            const QString when = it.startedAt.toString("yyyy-MM-dd hh:mm");
            if (!filter.isEmpty())
            {
                const QString key = (title + " " + when).toLower();
                if (!key.contains(filter.toLower())) continue;
            }
            const int row = table->rowCount();
            table->insertRow(row);
            // Column 0: time (also keep the session id here for selection helpers)
            auto *c0 = new QTableWidgetItem(when);
            c0->setData(Qt::UserRole, it.id);
            // Column 1: title (trimmed for display)
            QString trimmed = title;
            trimmed.replace('\n', ' ').replace('\r', ' ');
            if (trimmed.size() > kTitleMaxChars) trimmed = trimmed.left(kTitleMaxChars) + "...";
            auto *c1 = new QTableWidgetItem(trimmed);
            c1->setToolTip(title); // show full title on hover
            table->setItem(row, 0, c0);
            table->setItem(row, 1, c1);
        }
        // No resize to contents; header resize modes above keep layout tidy
    };
    fill("");
    // buttons
    QHBoxLayout *h = new QHBoxLayout();
    QPushButton *restoreBtn = new QPushButton(self->jtr("restore"), &dlg);
    QPushButton *renameBtn = new QPushButton(self->jtr("rename"), &dlg);
    QPushButton *deleteBtn = new QPushButton(self->jtr("delete"), &dlg);
    QPushButton *clearBtn = new QPushButton(self->jtr("clear all history"), &dlg);
    QPushButton *closeBtn = new QPushButton(self->jtr("close"), &dlg);
    h->addWidget(restoreBtn);
    h->addWidget(renameBtn);
    h->addWidget(deleteBtn);
    h->addStretch(1);
    h->addWidget(clearBtn);
    h->addWidget(closeBtn);
    v->addLayout(h);
    auto currentId = [&]() -> QString
    {
        const auto ranges = table->selectedRanges();
        if (ranges.isEmpty()) return QString();
        const int row = ranges.first().topRow();
        if (!table->item(row, 0)) return QString();
        return table->item(row, 0)->data(Qt::UserRole).toString();
    };
    QObject::connect(search, &QLineEdit::textChanged, &dlg, [&, self](const QString &t)
                     { fill(t); });
    QObject::connect(table, &QTableWidget::itemDoubleClicked, &dlg, [self, &currentId, d](QTableWidgetItem *)
                     {
        const QString id = currentId();
        if (id.isEmpty()) return;
        self->restoreSessionById(id);
        d->accept(); });
    QObject::connect(restoreBtn, &QPushButton::clicked, &dlg, [self, &currentId, d]()
                     {
        const QString id = currentId();
        if (id.isEmpty()) return;
        self->restoreSessionById(id);
        d->accept(); });
    QObject::connect(renameBtn, &QPushButton::clicked, &dlg, [self, &currentId, d, table]()
                     {
        const QString id = currentId();
        if (id.isEmpty()) return;
        bool ok = false;
        const QString t = QInputDialog::getText(d, self->jtr("rename"), self->jtr("new title"), QLineEdit::Normal, QString(), &ok);
        if (!ok) return;
        if (self->history_->renameSession(id, t))
        {
            // update table
            const auto ranges = table->selectedRanges();
            if (!ranges.isEmpty())
            {
                const int row = ranges.first().topRow();
                // Column 1 holds display title now; update with trimmed text and tooltip
                if (auto *c1 = table->item(row, 1)) {
                    QString trimmed = t;
                    trimmed.replace('\n', ' ').replace('\r', ' ');
                    const int kTitleMaxChars = 48;
                    if (trimmed.size() > kTitleMaxChars) trimmed = trimmed.left(kTitleMaxChars) + "...";
                    c1->setText(trimmed);
                    c1->setToolTip(t);
                }
            }
            self->reflash_state(self->jtr("session title updated"), SUCCESS_SIGNAL);
        }
        else
        {
            self->reflash_state(self->jtr("history db error"), WRONG_SIGNAL);
        } });
    QObject::connect(deleteBtn, &QPushButton::clicked, &dlg, [self, &currentId, d, &fill, search]()
                     {
        const QString id = currentId();
        if (id.isEmpty()) return;
        auto btn = QMessageBox::question(d, self->jtr("delete"), self->jtr("confirm delete?"));
        if (btn != QMessageBox::Yes) return;
        if (self->history_->deleteSession(id))
        {
            fill(search->text());
            self->reflash_state(self->jtr("deleted"), SUCCESS_SIGNAL);
        }
        else
        {
            self->reflash_state(self->jtr("history db error"), WRONG_SIGNAL);
        } });
    QObject::connect(clearBtn, &QPushButton::clicked, &dlg, [self, &fill, search, d]()
                     {
        auto btn = QMessageBox::question(d, self->jtr("clear all history"), self->jtr("confirm delete?"));
        if (btn != QMessageBox::Yes) return;
        if (self->history_->purgeAll())
        {
            fill(search->text());
            self->reflash_state(self->jtr("deleted"), SUCCESS_SIGNAL);
        }
        else
        {
            self->reflash_state(self->jtr("history db error"), WRONG_SIGNAL);
        } });
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    dlg.exec();
}
