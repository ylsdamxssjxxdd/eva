#include "widget.h"
#include "ui_widget.h"
#include <QImage>
#include <QPixmap>
#include <QScreen>
#include <QCursor>
#include <QPainter>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QTextCursor>
#include <QAudioDeviceInfo>
#include <QScrollBar>

void Widget::showImages(QStringList images_filepath)
{
    static const QString imageBadge = QStringLiteral("[IMG]");
    // 若前一行没有换行，则先补一行，避免与用户文本紧贴在同一行
    if (!images_filepath.isEmpty() && ui && ui->output)
    {
        const QString current = ui->output->toPlainText();
        if (!current.isEmpty() && !current.endsWith(QLatin1Char('\n')))
        {
            output_scroll(QStringLiteral("\n"));
        }
    }
    for (int i = 0; i < images_filepath.size(); ++i)
    {
        const QString rawPath = images_filepath[i];
        QFileInfo info(rawPath);
        const QString displayPath = info.exists() ? info.absoluteFilePath() : rawPath;
        const QString line = QStringLiteral("%1 %2").arg(imageBadge, QDir::toNativeSeparators(displayPath));
        output_scroll(line + QStringLiteral("\n"));
    }
}

void Widget::recordAudio()
{
    reflash_state("ui:" + jtr("recoding") + "... ");
    ui_state_recoding();
    audioRecorder.record();  // 在这之前检查是否可用
    audio_timer->start(100); // 每隔100毫秒刷新一次输入区
}

void Widget::monitorAudioLevel()
{
    audio_time += 100;
    ui_state_recoding(); // 更新输入区
}

void Widget::stop_recordAudio()
{
    QString wav_path = applicationDirPath + "/EVA_TEMP/" + QString("EVA_") + ".wav";
    is_recodering = false;
    audioRecorder.stop();
    audio_timer->stop();
    reflash_state("ui:" + jtr("recoding over") + " " + QString::number(float(audio_time) / 1000.0, 'f', 2) + "s");
    audio_time = 0;
    // 录音已直接以 16kHz/mono 输出，无需再做重采样
    emit ui2expend_speechdecode(wav_path, "txt"); // 传一个wav文件开始解码
}

QString Widget::saveScreen()
{
    QScreen *screen = QApplication::primaryScreen();
    // 获取屏幕几何信息
    qreal devicePixelRatio = screen->devicePixelRatio();
    // qDebug() << "逻辑尺寸:" << screenGeometry.width() << screenGeometry.height();
    // qDebug() << "缩放比例:" << devicePixelRatio;
    // 直接使用 grabWindow 获取完整屏幕截图（会自动处理DPI）
    QPixmap m_screenPicture = screen->grabWindow(0);
    // qDebug() << "截图实际尺寸:" << m_screenPicture.width() << m_screenPicture.height();
    // 获取鼠标位置（使用逻辑坐标，不需要手动缩放）
    QPoint cursorPos = QCursor::pos();
    // 将逻辑坐标转换为截图中的物理坐标
    cursorPos.setX(cursorPos.x() * devicePixelRatio);
    cursorPos.setY(cursorPos.y() * devicePixelRatio);
    // 创建光标图标
    QPixmap cursorPixmap;
    // 尝试获取当前光标
    if (QApplication::overrideCursor())
    {
        cursorPixmap = QApplication::overrideCursor()->pixmap();
    }
    // 如果没有获取到光标，创建默认箭头光标
    if (cursorPixmap.isNull())
    {
        // 光标大小按DPI缩放
        int baseSize = 16;
        int cursorSize = baseSize * devicePixelRatio;
        cursorPixmap = QPixmap(cursorSize, cursorSize);
        cursorPixmap.fill(Qt::transparent);
        cursorPixmap.setDevicePixelRatio(devicePixelRatio);
        QPainter cursorPainter(&cursorPixmap);
        cursorPainter.setRenderHint(QPainter::Antialiasing);
        cursorPainter.setPen(QPen(Qt::black, 1));
        cursorPainter.setBrush(Qt::white);
        // 绘制箭头（使用逻辑坐标，QPainter会自动处理缩放）
        QPolygonF arrow;
        arrow << QPointF(0, 0) << QPointF(0, 10) << QPointF(3, 7)
              << QPointF(7, 11) << QPointF(9, 9)
              << QPointF(5, 5) << QPointF(10, 0);
        cursorPainter.drawPolygon(arrow);
        cursorPainter.end();
    }
    else
    {
        // 如果获取到了系统光标，确保它有正确的DPI设置
        cursorPixmap.setDevicePixelRatio(devicePixelRatio);
    }
    // 将光标绘制到截图上
    QPainter painter(&m_screenPicture);
    painter.setRenderHint(QPainter::Antialiasing);
    // 绘制光标时，考虑到cursorPixmap可能已经有devicePixelRatio设置
    // 所以绘制位置需要调整
    QPoint drawPos = cursorPos;
    if (cursorPixmap.devicePixelRatio() > 1.0)
    {
        // 如果光标pixmap已经设置了devicePixelRatio，绘制位置需要相应调整
        drawPos.setX(cursorPos.x() / devicePixelRatio);
        drawPos.setY(cursorPos.y() / devicePixelRatio);
    }
    painter.drawPixmap(drawPos, cursorPixmap);
    painter.end();
    QImage image = m_screenPicture.toImage();
    // 逐步缩小图片直到尺寸 <= 1920x1080
    while (image.width() > 1920 || image.height() > 1080)
    {
        // 计算缩放比例，保持宽高比
        qreal scaleRatio = qMin(1920.0 / image.width(), 1080.0 / image.height());
        int newWidth = image.width() * scaleRatio;
        int newHeight = image.height() * scaleRatio;
        image = image.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
    QString filePath = QDir::currentPath() + "/EVA_TEMP/screen_cut/" + timestamp + ".png";
    createTempDirectory(QDir::currentPath() + "/EVA_TEMP/screen_cut");
    image.save(filePath);
    return filePath;
}

Widget::ControllerFrame Widget::captureControllerFrame()
{
    ControllerFrame frame;
    QScreen *screen = QApplication::primaryScreen();
    if (!screen)
    {
        reflash_state(QStringLiteral("ui:未检测到屏幕，无法附带桌面控制器截图"), WRONG_SIGNAL);
        return frame;
    }
    const qreal screenDpr = screen->devicePixelRatio(); // 物理像素比例，用于坐标换算
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    frame.tsMs = nowMs;
    // 捕获全屏图像
    QPixmap snapshot = screen->grabWindow(0);
    if (snapshot.isNull())
    {
        reflash_state(QStringLiteral("ui:截屏失败，桌面控制器未能附带截图"), WRONG_SIGNAL);
        return frame;
    }
    QImage scaledImage = snapshot.toImage();
    if (scaledImage.isNull())
    {
        reflash_state(QStringLiteral("ui:截屏数据为空，桌面控制器未能附带截图"), WRONG_SIGNAL);
        return frame;
    }
    // 若超过 1920x1080 则等比压缩，兼顾体积与可读性
    const int physicalWidth = int(qRound(snapshot.width() * screenDpr));
    const int physicalHeight = int(qRound(snapshot.height() * screenDpr));
    double logicalScale = 1.0; // 作用于逻辑坐标的缩放
    const int kMaxW = 1920;
    const int kMaxH = 1080;
    if (scaledImage.width() > kMaxW || scaledImage.height() > kMaxH)
    {
        logicalScale = qMin(double(kMaxW) / scaledImage.width(), double(kMaxH) / scaledImage.height());
        scaledImage = scaledImage.scaled(int(scaledImage.width() * logicalScale),
                                         int(scaledImage.height() * logicalScale),
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
    }
    // 物理坐标 -> 缩放后图像坐标的换算比例
    const double scaleRatio = logicalScale / screenDpr;
    // 计算光标在缩放图上的位置
    const QPoint cursorLogical = QCursor::pos();
    const QPoint cursorPhysical(qRound(cursorLogical.x() * screenDpr), qRound(cursorLogical.y() * screenDpr));
    const QPoint cursorScaled(qRound(cursorPhysical.x() * scaleRatio), qRound(cursorPhysical.y() * scaleRatio));
    frame.cursorX = cursorPhysical.x();
    frame.cursorY = cursorPhysical.y();
    // 绘制光标，确保模型能看到当前指针位置
    QImage baseImage = scaledImage.convertToFormat(QImage::Format_ARGB32);
    {
        QPainter painter(&baseImage);
        painter.setRenderHint(QPainter::Antialiasing);
        // 使用黄色十字线高亮光标位置，便于对齐
        QPen cursorPen(QColor(255, 215, 0, 230), qMax(2, int(logicalScale)));
        painter.setPen(cursorPen);
        painter.drawLine(cursorScaled.x(), 0, cursorScaled.x(), baseImage.height());
        painter.drawLine(0, cursorScaled.y(), baseImage.width(), cursorScaled.y());
        // 画一个小圆点标注中心
        painter.setBrush(QColor(255, 215, 0, 200));
        painter.drawEllipse(cursorScaled, qMax(4, int(4 * scaleRatio)), qMax(4, int(4 * scaleRatio)));
    }
    // 准备保存路径
    const QString baseDir = QDir(applicationDirPath).filePath(QStringLiteral("EVA_TEMP/controller_snapshots"));
    const QString overlayDir = QDir(baseDir).filePath(QStringLiteral("overlay"));
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    createTempDirectory(baseDir);
    createTempDirectory(overlayDir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));
    const QString basePath = QDir(baseDir).filePath(stamp + QStringLiteral(".png"));
    if (!baseImage.save(basePath))
    {
        reflash_state(QStringLiteral("ui:保存控制截图失败"), WRONG_SIGNAL);
        return frame;
    }
    // 生成带位置信息的标注图：十字线 + 文本信息
    QImage overlayImage = baseImage;
    QPainter overlayPainter(&overlayImage);
    overlayPainter.setRenderHint(QPainter::Antialiasing);
    const QColor gridColor(0, 122, 255, 160); // 半透明蓝色网格
    QPen gridPen(gridColor, qMax(1, int(logicalScale)));
    overlayPainter.setPen(gridPen);
    // 以物理 50 像素为步长绘制网格，数字标签改为每 200 像素标注一次，避免文字过密
    const int stepPhysical = 50;
    const int labelStepPhysical = 200;
    QFont gridFont = overlayPainter.font();
    gridFont.setPointSize(qMax(9, int(10 * scaleRatio)));
    overlayPainter.setFont(gridFont);
    QFontMetrics fm(gridFont);

    for (int px = stepPhysical; px < physicalWidth; px += stepPhysical)
    {
        const int x = int(qRound(px * scaleRatio));
        overlayPainter.drawLine(x, 0, x, overlayImage.height());
        if (px % labelStepPhysical == 0)
        {
            const QString label = QString::number(px);
            overlayPainter.setPen(Qt::white);
            overlayPainter.drawText(x + 4, fm.ascent() + 4, label);
            overlayPainter.setPen(gridPen);
        }
    }
    for (int py = stepPhysical; py < physicalHeight; py += stepPhysical)
    {
        const int y = int(qRound(py * scaleRatio));
        overlayPainter.drawLine(0, y, overlayImage.width(), y);
        if (py % labelStepPhysical == 0)
        {
            const QString label = QString::number(py);
            overlayPainter.setPen(Qt::white);
            overlayPainter.drawText(4, y + fm.ascent() + 4, label);
            overlayPainter.setPen(gridPen);
        }
    }
    // 更醒目的黄色十字线标注当前光标位置（覆盖蓝网格）
    QPen cursorPen(QColor(255, 215, 0, 230), qMax(3, int(2 * logicalScale)));
    overlayPainter.setPen(cursorPen);
    overlayPainter.drawLine(cursorScaled.x(), 0, cursorScaled.x(), overlayImage.height());
    overlayPainter.drawLine(0, cursorScaled.y(), overlayImage.width(), cursorScaled.y());
    // 仅在左上角标注(0,0)，避免遮挡
    overlayPainter.setPen(Qt::white);
    overlayPainter.drawText(4, fm.ascent() + 4, QStringLiteral("(0,0)"));
    overlayPainter.end();
    const QString overlayPath = QDir(overlayDir).filePath(stamp + QStringLiteral("_overlay.png"));
    overlayImage.save(overlayPath);

    frame.imagePath = basePath;
    frame.overlayPath = overlayPath;
    controllerFrames_.append(frame);
    cleanupControllerFrames();
    reflash_state(QStringLiteral("ui:控制截图已保存 base=%1 overlay=%2")
                      .arg(QDir::toNativeSeparators(basePath))
                      .arg(QDir::toNativeSeparators(overlayPath)),
                  SIGNAL_SIGNAL);
    return frame;
}

void Widget::cleanupControllerFrames()
{
    // 只保留最多 kMaxControllerFrames_ 份最新截图，超出即删除旧文件
    while (controllerFrames_.size() > kMaxControllerFrames_)
    {
        const ControllerFrame oldFrame = controllerFrames_.front();
        if (!oldFrame.imagePath.isEmpty()) QFile::remove(oldFrame.imagePath);
        if (!oldFrame.overlayPath.isEmpty()) QFile::remove(oldFrame.overlayPath);
        controllerFrames_.pop_front();
    }
}

void Widget::recv_monitor_decode_ok()
{
    is_monitor = false; // 解锁
}

bool Widget::checkAudio()
{
    audioSettings.setCodec("audio/x-raw");
    audioSettings.setSampleRate(16000);
    audioSettings.setBitRate(128000);
    audioSettings.setChannelCount(1);
    audioSettings.setQuality(QMultimedia::HighQuality);
    audioRecorder.setEncodingSettings(audioSettings);
    audioRecorder.setContainerFormat("audio/x-wav");
    audioRecorder.setOutputLocation(QUrl::fromLocalFile(applicationDirPath + "/EVA_TEMP/" + QStringLiteral("EVA_") + ".wav"));

    const QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    if (devices.isEmpty())
    {
        qDebug() << "No audio input devices available.";
        return false;
    }
    return true;
}
