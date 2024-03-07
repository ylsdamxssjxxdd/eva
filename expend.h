#ifndef EXPEND_H
#define EXPEND_H

#include <QWidget>
#include <QJsonObject>
#include <QScrollBar>
#include <QTimer>
#include <QDebug>
namespace Ui {
class Expend;
}

class Expend : public QWidget
{
    Q_OBJECT

public:
    Expend(QWidget *parent = nullptr,QJsonObject wordsObj_= QJsonObject(), QString vocab=nullptr, QStringList model_logs = QStringList());
    ~Expend();
    QJsonObject wordsObj_;
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
    Ui::Expend *ui;
};

#endif // EXPEND_H
