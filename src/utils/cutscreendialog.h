#ifndef CUTSCREENDIALOG_H
#define CUTSCREENDIALOG_H

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDialog>
#include <QDir>
#include <QImage>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QWidget>

class CutScreenDialog : public QDialog
{
    Q_OBJECT

  private:
    QPixmap m_screenPicture;                      // 屏幕截图图片
    QPixmap m_backgroundPicture;                  // 背景图片（带遮罩的屏幕截图）
    bool m_isMousePressed = false;                // 鼠标是否按下
    QPoint m_startPos, m_endPos, m_fixedStartPos; // 截图区域的起始、结束点以及固定起始点
    QMenu m_screenMenu;                           // 截图时的右键菜单

  public:
    // 构造函数，初始化界面
    explicit CutScreenDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint); // 设置窗口总是显示在最上层
    }

    // 析构函数，默认不做处理
    ~CutScreenDialog() override = default;

    // 初始化右键菜单的动作
    void initAction(const QString &action1Name, const QString &action2Name)
    {
        m_screenMenu.clear();                                                            // 清除旧菜单
        m_screenMenu.addAction(action1Name, this, &CutScreenDialog::saveCapturedScreen); // 添加截图保存菜单项
        m_screenMenu.addAction(action2Name, this, &CutScreenDialog::saveFullScreen);     // 添加全屏截图保存菜单项
    }

    // 如果临时目录不存在，则创建
    bool createTempDirectory(const QString &path)
    {
        QDir dir;
        if (!dir.exists(path))
        {
            return dir.mkpath(path); // 创建目录
        }
        return false; // 目录已存在
    }

    // 清除截图区域信息
    void clearInformation()
    {
        m_startPos = m_endPos = QPoint(); // 重置起始和结束点
    }

    // 获取选定区域的矩形框
    QRect getCapturedRect(const QPoint &start, const QPoint &end) const
    {
        return QRect(QPoint(qMin(start.x(), end.x()), qMin(start.y(), end.y())),
                     QSize(qAbs(start.x() - end.x()), qAbs(start.y() - end.y())));
    }

  protected:
    // 显示事件处理，初始化截图背景
    void showEvent(QShowEvent *event) override
    {
        QSize desktopSize = QApplication::desktop()->size();                                                                     // 获取桌面尺寸
        QScreen *screen = QApplication::primaryScreen();                                                                         // 获取主屏幕
        m_screenPicture = screen->grabWindow(QApplication::desktop()->winId(), 0, 0, desktopSize.width(), desktopSize.height()); // 截取屏幕
        QPixmap overlay(desktopSize.width(), desktopSize.height());                                                              // 创建一个与屏幕同尺寸的图层
        overlay.fill(QColor(255, 165, 0, 150));                                                                                  // 填充一个半透明的红色遮罩
        m_backgroundPicture = m_screenPicture;                                                                                   // 设置背景图片
        QPainter painter(&m_backgroundPicture);                                                                                  // 在背景图上绘制遮罩
        painter.drawPixmap(0, 0, overlay);
    }

    // 绘制事件处理，绘制截图框和截图内容
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        QPen pen(Qt::red); // 设置绘制的笔为红色
        pen.setWidth(1);   // 设置笔宽
        painter.setPen(pen);
        painter.drawPixmap(0, 0, m_backgroundPicture);      // 绘制背景图
        QRect rect = getCapturedRect(m_startPos, m_endPos); // 获取选定区域矩形
        if (rect.isValid())
        {
            painter.drawPixmap(rect.x() / devicePixelRatioF(), rect.y() / devicePixelRatioF(),
                               m_screenPicture.copy(rect)); // 绘制选中的截图区域
        }
    }

    // 鼠标按下事件处理，开始截图
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            m_isMousePressed = true;
            m_startPos = event->pos() * devicePixelRatioF();
            m_endPos = m_startPos; // 👈 关键修改：让起点和终点一致，避免闪动
            update();              // 可选：立即刷新，确保画面同步
        }
    }

    // 鼠标移动事件处理，更新截图区域
    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_isMousePressed)
        {
            m_endPos = event->pos() * devicePixelRatioF();
            m_fixedStartPos = m_startPos;
            update();
        }
    }

    // 鼠标释放事件处理，显示右键菜单
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_isMousePressed = false;
        m_screenMenu.exec(cursor().pos()); // 弹出右键菜单
        clearInformation();                // 清除起点终点数据
        update();                          // 立即刷新画面
    }

    // 右键菜单事件处理
    void contextMenuEvent(QContextMenuEvent *event) override
    {
        setCursor(Qt::ArrowCursor);        // 设置箭头光标
        m_screenMenu.exec(cursor().pos()); // 弹出右键菜单
    }

    // 键盘按下事件处理，取消截图并隐藏窗口
    void keyPressEvent(QKeyEvent *event) override
    {
        clearInformation(); // 清除截图区域信息
        hide();             // 隐藏窗口
    }

  public:
    // 调整图片的长宽比，如果长边与短边比大于3，则进行调整
    // QImage adjustImageAspectRatio(const QImage &image) {
    //     int width = image.width();
    //     int height = image.height();
    //     int longSide = qMax(width, height);  // 长边
    //     int shortSide = qMin(width, height);  // 短边
    //     double aspectRatio = static_cast<double>(longSide) / shortSide;  // 计算长宽比
    //     if (aspectRatio > 3) {
    //         int newShortSide = longSide / 3;  // 新的短边
    //         int padding = newShortSide - shortSide;  // 需要填充的宽度
    //         QImage newImage = (width < height)
    //                           ? QImage(width + padding, height, image.format())
    //                           : QImage(width, height + padding, image.format());
    //         newImage.fill(Qt::white);  // 用白色填充
    //         for (int y = 0; y < height; ++y) {
    //             for (int x = 0; x < width; ++x) {
    //                 newImage.setPixel(x, y, image.pixel(x, y));  // 将原图像素复制到新图像
    //             }
    //         }
    //         return newImage;
    //     }
    //     return image;  // 长宽比不需要调整时，返回原图
    // }

    // 保存图片到文件和剪贴板
    void saveImage(const QImage &image)
    {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setPixmap(QPixmap::fromImage(image));                                     // 将图片复制到剪贴板
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");    // 获取当前时间戳
        QString filePath = QDir::currentPath() + "/EVA_TEMP/" + "EVA_" + timestamp + ".png"; // 生成文件路径
        image.save(filePath);                                                                // 保存图片到文件
        emit cut2ui_qimagepath(filePath);                                                    // 发送图片路径信号
    }

  public slots:
    // 保存选定区域截图
    void saveCapturedScreen()
    {
        QRect rect = getCapturedRect(m_fixedStartPos, m_endPos);     // 获取选定区域矩形
        QImage capturedImage = m_screenPicture.copy(rect).toImage(); // 获取选定区域的图像
        // QImage adjustedImage = adjustImageAspectRatio(capturedImage);  // 调整长宽比
        saveImage(capturedImage); // 保存图片
        clearInformation();       // 清除截图信息
        hide();                   // 隐藏窗口
    }

    // 保存全屏截图
    void saveFullScreen()
    {
        saveImage(m_screenPicture.toImage()); // 保存全屏截图
        hide();                               // 隐藏窗口
    }

  signals:
    // 发送截图路径信号
    void cut2ui_qimagepath(const QString &cutImagePath);
};

#endif // CUTSCREENDIALOG_H
