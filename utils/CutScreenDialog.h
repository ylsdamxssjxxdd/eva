#ifndef CUTSCREENDIALOG_H
#define CUTSCREENDIALOG_H

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <qmenu.h>
#include <qapplication.h>
#include <QSize>
#include <QDesktopWidget>
#include <QPainter>
#include <QMouseEvent>
#include <string>
#include <math.h>
#include <QClipboard>
#include <qscreen.h>
#include <QDateTime>
#include <QDir>
#include <QDeBug>
#include <QJsonObject>

class CutScreenDialog:public QDialog
{
    Q_OBJECT
public:
    CutScreenDialog(QWidget *parent = nullptr);
    ~CutScreenDialog();
    void init_action(QString act1_name,QString act2_name);
    bool createTempDirectory(const QString &path);
private:
    QPixmap m_screenPicture;
    QPixmap backgroundPicture;
    bool m_isMousePressed = false;
    QPoint m_startPos, m_endPos, m_startPos_fixed;
    QMenu m_screenMenu;


private:
    void clearinformation();

public:
    QRect getCapturedRect(QPoint startpos, QPoint endpos);

protected:
    void showEvent(QShowEvent *event);
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void contextMenuEvent(QContextMenuEvent *event);
    void keyPressEvent(QKeyEvent *event);
public:
    void slot_saveCapturedScreen();
    void slot_saveFullScreen();
signals:
    void cut2ui_qimagepath(QString cut_imagepath_);
};

#endif // CUTSCREENDIALOG_H
