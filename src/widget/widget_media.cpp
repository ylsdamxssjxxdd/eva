#include "widget.h"
#include "ui_widget.h"
#include "controller_overlay.h"
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QEventLoop>
#include <QPalette>

void Widget::ensureControllerOverlay()
{
    if (controllerOverlay_) return;
    controllerOverlay_ = new ControllerOverlay();
    controllerOverlay_->hideNow();
}

void Widget::hideControllerOverlay()
{
    if (!controllerOverlay_) return;
    controllerOverlay_->hideNow();
}

void Widget::setControllerScreenshotMaskVisible(bool visible)
{
    // 设计目标：桌面控制器的截图要尽量“干净”，避免 EVA 自己的对话内容出现在截图里干扰模型判断。
    // 注意：这里不改写 ui->output 的 document（否则会破坏 recordBar 的锚点与颜色渲染），
    // 仅在截图前临时盖一层同色遮罩，截图完成后立刻恢复显示。
    if (!ui || !ui->output) return;

    if (visible)
    {
        if (!controllerScreenshotMask_)
        {
            controllerScreenshotMask_ = new QWidget(ui->output);
            controllerScreenshotMask_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
            controllerScreenshotMask_->setAutoFillBackground(true);
            controllerScreenshotMask_->hide();
        }

        // 遮罩颜色尽量与输出区背景一致，看起来就像“清空了输出区”。
        const QColor bg = ui->output->palette().base().color();
        QPalette pal = controllerScreenshotMask_->palette();
        pal.setColor(QPalette::Window, bg);
        controllerScreenshotMask_->setPalette(pal);

        controllerScreenshotMask_->setGeometry(ui->output->rect());
        controllerScreenshotMask_->raise();
        controllerScreenshotMask_->show();
        controllerScreenshotMask_->repaint(); // 同步触发一次绘制，减少 grabWindow 截到旧帧的概率
        return;
    }

    if (controllerScreenshotMask_) controllerScreenshotMask_->hide();
}

void Widget::recv_controller_hint(int x, int y, const QString &description)
{
    ensureControllerOverlay();
    if (!controllerOverlay_) return;

    // 坐标系统一：tool 侧执行鼠标操作用的是“真实屏幕物理像素坐标”（GetSystemMetrics/SendInput），
    // 但 Qt UI（已开启 AA_EnableHighDpiScaling）使用的是“逻辑坐标”。
    // 若不做换算，叠加层位置会与最终鼠标落点不一致（尤其是 125%/150% 缩放时会明显偏移）。
    const QScreen *screen = QGuiApplication::primaryScreen();
    const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    const qreal safeDpr = (dpr <= 0.01) ? 1.0 : dpr;
    const int logicalX = int(qRound(double(x) / double(safeDpr)));
    const int logicalY = int(qRound(double(y) / double(safeDpr)));

    // 叠加提示用于“让用户确认即将发生的动作”，显示时间拉到 2 秒便于看清。
    constexpr int kHintDurationMs = 2000;
    controllerOverlay_->showHint(logicalX, logicalY, description, kHintDurationMs);
}

void Widget::recv_controller_hint_done(int x, int y, const QString &description)
{
    ensureControllerOverlay();
    if (!controllerOverlay_) return;

    // 坐标系统一：tool 侧为物理像素坐标；Qt 侧为逻辑坐标。
    // 这里复用与 recv_controller_hint() 相同的换算逻辑，确保完成态提示与真实落点一致。
    const QScreen *screen = QGuiApplication::primaryScreen();
    const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    const qreal safeDpr = (dpr <= 0.01) ? 1.0 : dpr;
    const int logicalX = int(qRound(double(x) / double(safeDpr)));
    const int logicalY = int(qRound(double(y) / double(safeDpr)));

    // 完成态提示：绿色，滞留 2 秒，让用户确认动作已发生。
    constexpr int kDoneDurationMs = 2000;
    controllerOverlay_->showDoneHint(logicalX, logicalY, description, kDoneDurationMs);
}

void Widget::recv_monitor_countdown(int waitMs)
{
    // monitor 工具调用：等待过程中在主屏幕顶部居中展示倒计时提示，告诉用户“即将截图”。
    // 注意：该叠加层会被 captureControllerFrame() 在截图前自动隐藏，避免污染模型输入。
    ensureControllerOverlay();
    if (!controllerOverlay_) return;
    controllerOverlay_->showMonitorCountdown(waitMs);
}

void Widget::recv_monitor_countdown_done()
{
    // monitor 工具结束/取消时：立即隐藏提示，避免遮挡用户视线。
    hideControllerOverlay();
}

void Widget::recv_controller_overlay(quint64 turnId, const QString &argsJson)
{
    // -----------------------------------------------------------------------------
    // 需求：为了更好观察模型的定位
    // - controller 工具被调用时，把模型传入的 bbox/action/description/to_bbox 等信息绘制到截图上
    // - 将标注后的图片保存到 EVA_TEMP/overlay 目录，便于回溯与复盘
    // -----------------------------------------------------------------------------

    // 1) 选择“作为底图的截图”：优先使用最近一次发给模型的控制器截图（归一化坐标系截图）
    QString basePath = lastControllerImagePathForModel_;
    if (basePath.isEmpty() || !QFileInfo::exists(basePath))
    {
        // 兜底：使用最近一次捕获的控制器截图
        if (!controllerFrames_.isEmpty())
        {
            const QString candidate = controllerFrames_.last().imagePath;
            if (!candidate.isEmpty() && QFileInfo::exists(candidate))
            {
                basePath = candidate;
            }
        }
    }
    if (basePath.isEmpty() || !QFileInfo::exists(basePath))
    {
        // 再兜底：现场抓一张（可能不是模型当时看到的那张，但至少能落盘观察 bbox 是否合理）
        const ControllerFrame frame = captureControllerFrame();
        if (!frame.imagePath.isEmpty() && QFileInfo::exists(frame.imagePath))
        {
            basePath = frame.imagePath;
        }
    }
    if (basePath.isEmpty() || !QFileInfo::exists(basePath))
    {
        qWarning().noquote() << "[controller-overlay] no base screenshot available";
        return;
    }

    // 2) 解析 arguments JSON，提取 bbox/action/description 等字段
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(argsJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning().noquote() << "[controller-overlay] invalid argsJson:" << parseError.errorString();
        return;
    }
    const QJsonObject obj = doc.object();

    auto toIntRounded = [](const QJsonValue &v, int fallback = 0) -> int {
        if (v.isDouble())
        {
            return qRound(v.toDouble());
        }
        if (v.isString())
        {
            bool ok = false;
            const double d = v.toString().toDouble(&ok);
            if (ok) return qRound(d);
        }
        return fallback;
    };

    auto parseBBox = [&](const char *key, bool required, QRect &outRect) -> bool {
        if (!obj.contains(QString::fromLatin1(key)))
        {
            return !required;
        }
        const QJsonValue v = obj.value(QString::fromLatin1(key));
        if (!v.isArray()) return !required;
        const QJsonArray a = v.toArray();
        if (a.size() != 4) return !required;
        const int x1 = toIntRounded(a.at(0));
        const int y1 = toIntRounded(a.at(1));
        const int x2 = toIntRounded(a.at(2));
        const int y2 = toIntRounded(a.at(3));
        outRect = QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
        return true;
    };

    QRect bbox;
    if (!parseBBox("bbox", true, bbox))
    {
        qWarning().noquote() << "[controller-overlay] missing/invalid bbox";
        return;
    }
    QRect toBbox;
    const bool hasToBbox = parseBBox("to_bbox", false, toBbox) && !toBbox.isNull();

    const QString action = obj.value(QStringLiteral("action")).toString().trimmed();
    const QString description = obj.value(QStringLiteral("description")).toString().trimmed();
    const QString key = obj.value(QStringLiteral("key")).toString().trimmed();
    const QString text = obj.value(QStringLiteral("text")).toString();
    const int delayMs = obj.contains(QStringLiteral("delay_ms")) ? toIntRounded(obj.value(QStringLiteral("delay_ms")), -1) : -1;
    const int durationMs = obj.contains(QStringLiteral("duration_ms")) ? toIntRounded(obj.value(QStringLiteral("duration_ms")), -1) : -1;
    const int scrollSteps = obj.contains(QStringLiteral("scroll_steps")) ? toIntRounded(obj.value(QStringLiteral("scroll_steps")), -1) : -1;

    // 3) 加载底图并绘制标注
    QImage img(basePath);
    if (img.isNull())
    {
        qWarning().noquote() << "[controller-overlay] failed to load base screenshot:" << QDir::toNativeSeparators(basePath);
        return;
    }
    if (img.format() != QImage::Format_ARGB32) img = img.convertToFormat(QImage::Format_ARGB32);

    const QRect imgRect(0, 0, img.width() - 1, img.height() - 1);
    auto clampRectToImage = [&](QRect r) -> QRect {
        if (imgRect.width() <= 0 || imgRect.height() <= 0) return QRect();
        r = r.normalized();
        return r.intersected(imgRect);
    };
    auto clampPointToImage = [&](QPoint p) -> QPoint {
        if (imgRect.width() <= 0 || imgRect.height() <= 0) return QPoint(0, 0);
        p.setX(qBound(imgRect.left(), p.x(), imgRect.right()));
        p.setY(qBound(imgRect.top(), p.y(), imgRect.bottom()));
        return p;
    };

    const QRect bboxClamped = clampRectToImage(bbox);
    const QRect toBboxClamped = hasToBbox ? clampRectToImage(toBbox) : QRect();
    const QPoint bboxCenter = clampPointToImage(bbox.center());
    const QPoint toBboxCenter = hasToBbox ? clampPointToImage(toBbox.center()) : QPoint();

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // bbox：蓝色框（与“系统相关为蓝色”的视觉约定一致）
    const QColor bboxBorder(0, 140, 255, 230);
    const QColor bboxFill(0, 140, 255, 40);
    p.setPen(QPen(bboxBorder, 3));
    p.setBrush(bboxFill);
    if (!bboxClamped.isNull()) p.drawRect(bboxClamped);

    // 中心点：十字 + 小圆
    const QColor centerColor(255, 255, 255, 240);
    p.setPen(QPen(centerColor, 2));
    p.setBrush(centerColor);
    p.drawEllipse(bboxCenter, 3, 3);
    p.drawLine(bboxCenter.x() - 12, bboxCenter.y(), bboxCenter.x() + 12, bboxCenter.y());
    p.drawLine(bboxCenter.x(), bboxCenter.y() - 12, bboxCenter.x(), bboxCenter.y() + 12);

    // to_bbox：橙色框（拖放终点等）
    if (hasToBbox && !toBboxClamped.isNull())
    {
        const QColor toBorder(255, 140, 0, 230);
        const QColor toFill(255, 140, 0, 30);
        QPen pen(toBorder, 3);
        pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(toFill);
        p.drawRect(toBboxClamped);

        // 用箭头线把起点与终点连起来
        QPen linkPen(toBorder, 2);
        p.setPen(linkPen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(bboxCenter, toBboxCenter);
        p.drawEllipse(toBboxCenter, 3, 3);
    }

    // 4) 文本信息：action/description/bbox 等，绘制在左上角，带半透明背景
    const QFileInfo baseInfo(basePath);
    const QString baseName = baseInfo.fileName().isEmpty() ? baseInfo.absoluteFilePath() : baseInfo.fileName();

    QStringList lines;
    lines << QStringLiteral("controller overlay");
    lines << QStringLiteral("turn: %1").arg(turnId == 0 ? QStringLiteral("-") : QString::number(turnId));
    lines << QStringLiteral("image: %1 (%2x%3)").arg(baseName).arg(img.width()).arg(img.height());
    if (!action.isEmpty()) lines << QStringLiteral("action: %1").arg(action);
    lines << QStringLiteral("bbox: [%1,%2,%3,%4] center=(%5,%6)")
                 .arg(bbox.left())
                 .arg(bbox.top())
                 .arg(bbox.right())
                 .arg(bbox.bottom())
                 .arg(bboxCenter.x())
                 .arg(bboxCenter.y());
    if (hasToBbox)
    {
        lines << QStringLiteral("to_bbox: [%1,%2,%3,%4] center=(%5,%6)")
                     .arg(toBbox.left())
                     .arg(toBbox.top())
                     .arg(toBbox.right())
                     .arg(toBbox.bottom())
                     .arg(toBboxCenter.x())
                     .arg(toBboxCenter.y());
    }
    if (!key.isEmpty()) lines << QStringLiteral("key: %1").arg(key);
    if (delayMs >= 0) lines << QStringLiteral("delay_ms: %1").arg(delayMs);
    if (durationMs >= 0) lines << QStringLiteral("duration_ms: %1").arg(durationMs);
    if (scrollSteps >= 0) lines << QStringLiteral("scroll_steps: %1").arg(scrollSteps);
    if (!text.isEmpty())
    {
        // 避免文本过长撑爆标注
        QString clipped = text;
        if (clipped.size() > 160) clipped = clipped.left(160) + QStringLiteral("...");
        lines << QStringLiteral("text: %1").arg(clipped);
    }
    if (!description.isEmpty())
    {
        QString clipped = description;
        if (clipped.size() > 240) clipped = clipped.left(240) + QStringLiteral("...");
        lines << QStringLiteral("desc: %1").arg(clipped);
    }

    QFont font = p.font();
    font.setBold(true);
    p.setFont(font);
    const QString overlayText = lines.join(QStringLiteral("\n"));
    const int margin = 10;
    const QRect textArea(margin, margin, img.width() - margin * 2, img.height() - margin * 2);
    QFontMetrics fm(font);
    QRect textRect = fm.boundingRect(textArea, Qt::TextWordWrap, overlayText);
    textRect = textRect.adjusted(-10, -8, 10, 8); // padding

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawRoundedRect(textRect, 8, 8);

    p.setPen(QColor(255, 255, 255, 240));
    p.drawText(textRect.adjusted(10, 8, -10, -8), Qt::TextWordWrap, overlayText);

    // 5) 落盘：EVA_TEMP/overlay
    const QString overlayDir = QDir(applicationDirPath).filePath(QStringLiteral("EVA_TEMP/overlay"));
    QDir().mkpath(overlayDir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));

    auto sanitize = [](QString s) -> QString {
        s = s.trimmed();
        if (s.isEmpty()) return QStringLiteral("controller");
        for (int i = 0; i < s.size(); ++i)
        {
            const QChar ch = s.at(i);
            const bool ok = ch.isLetterOrNumber() || ch == QLatin1Char('_') || ch == QLatin1Char('-');
            if (!ok) s[i] = QLatin1Char('_');
        }
        return s;
    };

    const QString turnPart = (turnId == 0) ? QStringLiteral("turn-") : QStringLiteral("turn%1").arg(turnId);
    const QString fileName = QStringLiteral("%1_%2_%3.png").arg(stamp, turnPart, sanitize(action));
    const QString outPath = QDir(overlayDir).filePath(fileName);
    if (!img.save(outPath))
    {
        qWarning().noquote() << "[controller-overlay] failed to save:" << QDir::toNativeSeparators(outPath);
        return;
    }
    qInfo().noquote() << "[controller-overlay]" << QDir::toNativeSeparators(outPath);
}

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
    // 控制器截图一定要“干净”：避免把叠加提示/对话内容也截进去，污染模型输入。
    hideControllerOverlay();

    // 截图时机调整：先让输出区在屏幕上“清屏”，再 grabWindow。
    // 这里用遮罩临时盖住 ui->output 的文字，而不是 clear()：
    // - 不破坏 recordBar 的 docFrom/docTo 锚点
    // - 不丢失输出区已经渲染的颜色与格式
    // - 截图后立即恢复显示，用户体验更稳定
    struct OutputMaskGuard
    {
        Widget *self = nullptr;
        explicit OutputMaskGuard(Widget *w) : self(w)
        {
            if (self) self->setControllerScreenshotMaskVisible(true);
        }
        void release()
        {
            if (!self) return;
            self->setControllerScreenshotMaskVisible(false);
            self = nullptr;
        }
        ~OutputMaskGuard() { release(); }
    } maskGuard(this);

    // 强制处理一次绘制事件：确保遮罩已经渲染到屏幕，grabWindow 才能截到“清屏后”的画面。
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    QScreen *screen = QApplication::primaryScreen();
    if (!screen)
    {
        reflash_state(QStringLiteral("ui:未检测到屏幕，无法附带桌面控制器截图"), WRONG_SIGNAL);
        return frame;
    }
    const qreal screenDpr = screen->devicePixelRatio(); // 物理像素比例，用于坐标换算
    // 归一化参数：约定窗口可配置，默认 1000×1000
    // 注意：这里不做“保持宽高比”，而是把屏幕强制映射到一个规则矩形，
    // 这样模型输出的坐标可以用线性比例直接换算为真实屏幕坐标，逻辑最简单稳定。
    const int normX = qBound(100, ui_controller_norm_x, 2048);
    const int normY = qBound(100, ui_controller_norm_y, 2048);

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    frame.tsMs = nowMs;
    // 捕获全屏图像
    QPixmap snapshot = screen->grabWindow(0);

    // 截图已获取，立刻恢复输出区显示（避免 UI 长时间空白）。
    maskGuard.release();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
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
    // 直接缩放到归一化尺寸（忽略宽高比）
    scaledImage = scaledImage.scaled(normX, normY, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // 映射：把 [0,srcMax] 映射到 [0,dstMax]（含端点），并进行夹取，避免越界
    const auto mapRange = [](int value, int srcMax, int dstMax) -> int {
        if (dstMax <= 0) return 0;
        if (srcMax <= 0) return qBound(0, value, dstMax);
        const int clamped = qBound(0, value, srcMax);
        const double t = double(clamped) / double(srcMax);
        return qBound(0, int(qRound(t * dstMax)), dstMax);
    };

    // 计算光标物理坐标与在归一化图上的像素位置
    const QPoint cursorLogical = QCursor::pos();
    const QPoint cursorPhysical(qRound(cursorLogical.x() * screenDpr), qRound(cursorLogical.y() * screenDpr));

    // 归一化坐标系：x in [0,normX]，y in [0,normY]
    frame.cursorX = mapRange(cursorPhysical.x(), physicalWidth - 1, normX);
    frame.cursorY = mapRange(cursorPhysical.y(), physicalHeight - 1, normY);
    // 控制器截屏仅用于“看图定位坐标”，这里保持截图原样（不再绘制鼠标标记/正交线/坐标网格），减少干扰与额外 token
    QImage baseImage = scaledImage.convertToFormat(QImage::Format_ARGB32);
    // 准备保存路径
    const QString baseDir = QDir(applicationDirPath).filePath(QStringLiteral("EVA_TEMP/controller_snapshots"));
    createTempDirectory(applicationDirPath + "/EVA_TEMP");
    createTempDirectory(baseDir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));
    const QString basePath = QDir(baseDir).filePath(stamp + QStringLiteral(".png"));
    if (!baseImage.save(basePath))
    {
        reflash_state(QStringLiteral("ui:保存控制截图失败"), WRONG_SIGNAL);
        return frame;
    }
    frame.imagePath = basePath;
    controllerFrames_.append(frame);
    cleanupControllerFrames();
    reflash_state(QStringLiteral("ui:控制截图已保存 %1").arg(QDir::toNativeSeparators(basePath)), SIGNAL_SIGNAL);
    return frame;
}

void Widget::cleanupControllerFrames()
{
    // 只保留最多 kMaxControllerFrames_ 份最新截图，超出即删除旧文件
    while (controllerFrames_.size() > kMaxControllerFrames_)
    {
        const ControllerFrame oldFrame = controllerFrames_.front();
        if (!oldFrame.imagePath.isEmpty()) QFile::remove(oldFrame.imagePath);
        controllerFrames_.pop_front();
    }
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
