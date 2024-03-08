#include "CutScreenDialog.h"

CutScreenDialog::CutScreenDialog(QWidget* parent)
:QDialog(parent)
{
    this->setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);

}

void CutScreenDialog::init_action(QString act1_name,QString act2_name)
{
    m_screenMenu.clear();
    m_screenMenu.addAction(act1_name, this, &CutScreenDialog::slot_saveCapturedScreen);
    m_screenMenu.addAction(act2_name, this, &CutScreenDialog::slot_saveFullScreen);
}
CutScreenDialog::~CutScreenDialog()
{

}

void CutScreenDialog::clearinformation()
{
    this->m_startPos = this->m_endPos = QPoint();
}

QRect CutScreenDialog::getCapturedRect(QPoint startpos, QPoint endpos)
{
    QRect ret;

    if(startpos.x()<endpos.x())
    {
        ret.setLeft(startpos.x());

    }
    else {
        ret.setLeft(endpos.x());

    }


    if(startpos.y() < endpos.y())
    {
        ret.setTop(startpos.y());

    }
    else {
        ret.setTop(endpos.y());

    }


    ret.setWidth(qAbs(startpos.x() - endpos.x()));
    ret.setHeight(qAbs(startpos.y() - endpos.y()));

    return ret;

}

void CutScreenDialog::showEvent(QShowEvent *event)
{
    QSize desktopSize = QApplication::desktop()->size();
    QScreen *pscreen = QApplication::primaryScreen();
    m_screenPicture = pscreen->grabWindow(QApplication::desktop()->winId(), 0, 0, desktopSize.width(), desktopSize.height());
    QPixmap pix(desktopSize.width(), desktopSize.height());
    pix.fill((QColor(255, 0, 0, 150)));
    backgroundPicture = m_screenPicture;
    QPainter painter(&backgroundPicture);
    painter.drawPixmap(0, 0, pix);

}

void CutScreenDialog::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    QPen pen;
    pen.setColor(Qt::red);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawPixmap(0, 0, backgroundPicture);
    QRect rect(getCapturedRect(m_startPos, m_endPos));
    
    if (rect.isValid()) {
        painter.drawPixmap(rect.x()/devicePixelRatioF(), rect.y()/devicePixelRatioF(), m_screenPicture.copy(rect));
    }

}

void CutScreenDialog::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton){
        m_isMousePressed = true;
        m_startPos = event->pos()*devicePixelRatioF();//devicePixelRatioF()是缩放系数
        //qDebug()<<"m_startPos"<<m_startPos;
    }
}

void CutScreenDialog::mouseMoveEvent(QMouseEvent *event)
{
    if(m_isMousePressed){
       m_endPos = event->pos()*devicePixelRatioF();//devicePixelRatioF()是缩放系数
       m_startPos_fixed = m_startPos;
       //qDebug()<<"m_endPos"<<m_endPos;
       update();  //产生绘图事件
    }
}

void CutScreenDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_isMousePressed = false;
    m_screenMenu.exec(cursor().pos());

}



void CutScreenDialog::contextMenuEvent(QContextMenuEvent *event)
{
    this->setCursor(Qt::ArrowCursor);
    m_screenMenu.exec(cursor().pos());
}

void CutScreenDialog::keyPressEvent(QKeyEvent *event)
{

    clearinformation();
    hide();

}

void CutScreenDialog::slot_saveCapturedScreen() //保存图片动作被点击
{
    QClipboard *clipboard = QApplication::clipboard();
    QRect rect(getCapturedRect(m_startPos_fixed, m_endPos));
    clipboard->setPixmap(m_screenPicture.copy(rect));
    clearinformation();

    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString dateTimeString = currentDateTime.toString("hh-mm-ss");
    QString currentDir = QDir::currentPath();
    QString cut_qimagepath = currentDir + "/" + dateTimeString + ".png";
    m_screenPicture.copy(rect).toImage().save(cut_qimagepath);
    cut2ui_qimagepath(cut_qimagepath);

    this->hide();
}

void CutScreenDialog::slot_saveFullScreen() //保存整个屏幕的图片
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setPixmap(m_screenPicture);
    clearinformation();

    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString dateTimeString = currentDateTime.toString("hh-mm-ss");
    QString currentDir = QDir::currentPath();
    QString cut_qimagepath = currentDir + "/" + dateTimeString + ".png";
    m_screenPicture.toImage().save(cut_qimagepath);
    cut2ui_qimagepath(cut_qimagepath);

    this->hide();
}
