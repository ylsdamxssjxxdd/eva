#ifndef VERSIONLOG_H
#define VERSIONLOG_H

#include <QDialog>

namespace Ui {
class Versionlog;
}

class Versionlog : public QDialog
{
    Q_OBJECT

public:
    Versionlog(QWidget *parent = nullptr,QString vocab=nullptr,QStringList model_logs = QStringList());
    ~Versionlog();
    QString vocab_;
    QString model_logs_;
    bool is_show_vocab = false;
    bool is_show_logs = false;
    bool load_percent_tag = false;
public slots:
    void recv_log(QString log);
    void recv_vocab(QString vocab);
private slots:
    void on_tabWidget_tabBarClicked(int index);

private:
    Ui::Versionlog *ui;
};

#endif // VERSIONLOG_H
