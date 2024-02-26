#ifndef CUSTOMPLAINTEXTEDIT_H
#define CUSTOMPLAINTEXTEDIT_H

#include <QPlainTextEdit>
#include <QDebug>
#include <QToolTip>
#include <QTextBlock>
#include <QTextCursor>



class customPlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT
public:
    customPlainTextEdit(QWidget *parent = nullptr);
    void mouseMoveEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool isInSpecialText(QTextCursor &cursor, QString targetText);
    QString device_tooltip;//设备支持的提示消息

signals:
    void creatVersionlog();
};

#endif // CUSTOMPLAINTEXTEDIT_H
