// CutScreenDialog
// 自定义的Dialog 支持局部截图和全屏截图，截图完成保存在EVA_TEMP文件夹并发送信号传递图像路径
// 使用时可直接包含此头文件
// 依赖qt5.15

#ifndef CUTSCREENDIALOG_H
#define CUTSCREENDIALOG_H

#include <math.h>
#include <qapplication.h>
#include <qmenu.h>
#include <qscreen.h>

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QDialog>
#include <QDir>
#include <QJsonObject>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QSize>
#include <QWidget>
#include <string>

class CutScreenDialog : public QDialog {
    Q_OBJECT

   private:
    QPixmap m_screenPicture;
    QPixmap backgroundPicture;
    bool m_isMousePressed = false;
    QPoint m_startPos, m_endPos, m_startPos_fixed;
    QMenu m_screenMenu;

   public:
    //初始化
    CutScreenDialog(QWidget *parent) : QDialog(parent) {
        this->setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);  //无边框窗口
    }
    //析构函数
    ~CutScreenDialog() { ; }

    //获得截图动作的名称，主要为了转换中英文
    void init_action(QString act1_name, QString act2_name) {
        m_screenMenu.clear();
        m_screenMenu.addAction(act1_name, this, &CutScreenDialog::slot_saveCapturedScreen);
        m_screenMenu.addAction(act2_name, this, &CutScreenDialog::slot_saveFullScreen);
    }

    //创建临时文件夹EVA_TEMP
    bool createTempDirectory(const QString &path) {
        QDir dir;
        // 检查路径是否存在
        if (dir.exists(path)) {
            return false;
        } else {
            // 尝试创建目录
            if (dir.mkpath(path)) {
                return true;
            } else {
                return false;
            }
        }
    }

    void clearinformation() { this->m_startPos = this->m_endPos = QPoint(); }

    QRect getCapturedRect(QPoint startpos, QPoint endpos) {
        QRect ret;

        if (startpos.x() < endpos.x()) {
            ret.setLeft(startpos.x());

        } else {
            ret.setLeft(endpos.x());
        }

        if (startpos.y() < endpos.y()) {
            ret.setTop(startpos.y());

        } else {
            ret.setTop(endpos.y());
        }

        ret.setWidth(qAbs(startpos.x() - endpos.x()));
        ret.setHeight(qAbs(startpos.y() - endpos.y()));

        return ret;
    }

   protected:
    void showEvent(QShowEvent *event) {
        QSize desktopSize = QApplication::desktop()->size();
        QScreen *pscreen = QApplication::primaryScreen();
        m_screenPicture = pscreen->grabWindow(QApplication::desktop()->winId(), 0, 0, desktopSize.width(), desktopSize.height());
        QPixmap pix(desktopSize.width(), desktopSize.height());
        pix.fill((QColor(255, 0, 0, 150)));
        backgroundPicture = m_screenPicture;
        QPainter painter(&backgroundPicture);
        painter.drawPixmap(0, 0, pix);
    }

    void paintEvent(QPaintEvent *event) {
        QPainter painter(this);
        QPen pen;
        pen.setColor(Qt::red);
        pen.setWidth(1);
        painter.setPen(pen);
        painter.drawPixmap(0, 0, backgroundPicture);
        QRect rect(getCapturedRect(m_startPos, m_endPos));

        if (rect.isValid()) {
            painter.drawPixmap(rect.x() / devicePixelRatioF(), rect.y() / devicePixelRatioF(), m_screenPicture.copy(rect));
        }
    }

    void mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            m_isMousePressed = true;
            m_startPos = event->pos() * devicePixelRatioF();  // devicePixelRatioF()是缩放系数
            // qDebug()<<"m_startPos"<<m_startPos;
        }
    }

    void mouseMoveEvent(QMouseEvent *event) {
        if (m_isMousePressed) {
            m_endPos = event->pos() * devicePixelRatioF();  // devicePixelRatioF()是缩放系数
            m_startPos_fixed = m_startPos;
            // qDebug()<<"m_endPos"<<m_endPos;
            update();  //产生绘图事件
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) {
        m_isMousePressed = false;
        m_screenMenu.exec(cursor().pos());
    }

    void contextMenuEvent(QContextMenuEvent *event) {
        this->setCursor(Qt::ArrowCursor);
        m_screenMenu.exec(cursor().pos());
    }

    void keyPressEvent(QKeyEvent *event) {
        clearinformation();
        hide();
    }

   public:
    // 为了适配llava，判断截取的图像长边除以短边如果比值大于3，则向图像的短边的下面或右面添加空白部分直到比值小于等于3，请使用原子操作每个像素避免损失
    QImage adjustImageAspectRatio(const QImage &originalImage) {
        int width = originalImage.width();
        int height = originalImage.height();
        
        int longSide = qMax(width, height);
        int shortSide = qMin(width, height);
        
        // 计算长短边比值
        double aspectRatio = static_cast<double>(longSide) / shortSide;
        
        // 如果长短边比值大于3，则添加空白部分
        if (aspectRatio > 3) {
            // 计算需要扩展的大小
            int newShortSide = longSide / 3;
            int padding = newShortSide - shortSide;
            
            QImage newImage;
            
            if (width < height) {
                // 宽度短，增加宽度
                newImage = QImage(width + padding, height, originalImage.format());
            } else {
                // 高度短，增加高度
                newImage = QImage(width, height + padding, originalImage.format());
            }
            
            // 填充空白部分为白色
            newImage.fill(Qt::white);
            
            // 逐像素复制原图像数据到新图像
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    newImage.setPixel(x, y, originalImage.pixel(x, y));
                }
            }
            
            return newImage;
        }
        
        return originalImage;
    }

    void slot_saveCapturedScreen()  //保存图片动作被点击
    {
        QClipboard *clipboard = QApplication::clipboard();
        QRect rect(getCapturedRect(m_startPos_fixed, m_endPos));
        QImage capturedImage = m_screenPicture.copy(rect).toImage();
        
        // 调整图像的宽高比
        QImage adjustedImage = adjustImageAspectRatio(capturedImage);
        
        // 保存调整后的图像
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString dateTimeString = currentDateTime.toString("yyyy-hh-mm-ss");
        QString currentDir = QDir::currentPath();
        QString cut_qimagepath = currentDir + "/EVA_TEMP/" + "EVA_" + dateTimeString + ".png";
        adjustedImage.save(cut_qimagepath);
        
        // 传递保存路径
        cut2ui_qimagepath(cut_qimagepath);
        
        // 更新剪贴板内容
        clipboard->setPixmap(QPixmap::fromImage(adjustedImage));
        
        // 清除临时信息
        clearinformation();

        this->hide();
    }

    void slot_saveFullScreen()  //保存整个屏幕的图片
    {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setPixmap(m_screenPicture);


        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString dateTimeString = currentDateTime.toString("hh-mm-ss");
        QString currentDir = QDir::currentPath();
        QString cut_qimagepath = currentDir + "/EVA_TEMP/" + "EVA_" + dateTimeString + ".png";
        m_screenPicture.toImage().save(cut_qimagepath);
        cut2ui_qimagepath(cut_qimagepath);

        this->hide();
    }

   signals:
    void cut2ui_qimagepath(QString cut_imagepath_);
};

#endif  // CUTSCREENDIALOG_H
