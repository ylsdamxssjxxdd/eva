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
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
#ifdef _WIN32
// -----------------------------------------------------------------------------
// Windows 桌面截图（GDI/BitBlt）
// -----------------------------------------------------------------------------
// 背景：Qt 的 `QScreen::grabWindow(0)` 在极少数机器/驱动环境下可能出现“偶发卡死”，
//       表现为调用线程被阻塞，UI 无法响应（重置按钮也无效）。
// 目标：提供一个不依赖 Qt 平台插件截图链路的兜底实现，尽量避免 UI 被截图卡死。
//
// 说明：
// - 这里只抓取“主屏幕”(0,0)-(SM_CXSCREEN,SM_CYSCREEN)，与 controller 工具层
//   使用 `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)` 的坐标系保持一致。
// - 返回的 QImage 为“物理像素”尺寸，不设置 devicePixelRatio；调用方如需按 Qt 逻辑坐标绘制，
//   可以自行对 QPixmap 设置 devicePixelRatio。
// -----------------------------------------------------------------------------
static QImage capturePrimaryScreenWin32(int *outWidth, int *outHeight, QString *errorMessage)
{
    if (outWidth) *outWidth = 0;
    if (outHeight) *outHeight = 0;

    const int width = GetSystemMetrics(SM_CXSCREEN);
    const int height = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0)
    {
        if (errorMessage) *errorMessage = QStringLiteral("GetSystemMetrics returned invalid size");
        return {};
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc)
    {
        if (errorMessage) *errorMessage = QStringLiteral("GetDC(nullptr) failed");
        return {};
    }

    HDC memDc = CreateCompatibleDC(screenDc);
    if (!memDc)
    {
        ReleaseDC(nullptr, screenDc);
        if (errorMessage) *errorMessage = QStringLiteral("CreateCompatibleDC failed");
        return {};
    }

    HBITMAP bmp = CreateCompatibleBitmap(screenDc, width, height);
    if (!bmp)
    {
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        if (errorMessage) *errorMessage = QStringLiteral("CreateCompatibleBitmap failed");
        return {};
    }

    HGDIOBJ old = SelectObject(memDc, bmp);

    // CAPTUREBLT：尽量包含 DWM/分层窗口内容（controller 截图前会隐藏 overlay，但加上更稳妥）
    const BOOL bltOk = BitBlt(memDc, 0, 0, width, height, screenDc, 0, 0, SRCCOPY | CAPTUREBLT);
    if (!bltOk)
    {
        const DWORD err = GetLastError();
        SelectObject(memDc, old);
        DeleteObject(bmp);
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        if (errorMessage) *errorMessage = QStringLiteral("BitBlt failed (err=%1)").arg(err);
        return {};
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // 负数表示 top-down DIB，避免后续翻转
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<uchar> buffer(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    const int scanlines = GetDIBits(memDc, bmp, 0, static_cast<UINT>(height), buffer.data(), &bmi, DIB_RGB_COLORS);

    SelectObject(memDc, old);
    DeleteObject(bmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);

    if (scanlines <= 0)
    {
        if (errorMessage) *errorMessage = QStringLiteral("GetDIBits failed");
        return {};
    }

    // QImage::Format_ARGB32 在 little-endian 下内存布局为 BGRA，与 DIB buffer 兼容。
    // 但 GDI 截屏的 alpha 可能是未定义值，所以这里转一次 RGB32，强制 alpha=255。
    QImage img(buffer.data(), width, height, QImage::Format_ARGB32);
    QImage out = img.convertToFormat(QImage::Format_RGB32);

    if (outWidth) *outWidth = width;
    if (outHeight) *outHeight = height;
    return out;
}
#endif
} // namespace

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
        // 兼容：有些模型只会返回中心点 [cx,cy]（2 个参数）。
        // 工具层会把它退化为 1x1 bbox；这里也同步兼容，保证 overlay 能落盘复盘。
        if (a.size() != 4 && a.size() != 2) return !required;
        const int x1 = toIntRounded(a.at(0));
        const int y1 = toIntRounded(a.at(1));
        const int x2 = (a.size() == 4) ? toIntRounded(a.at(2)) : x1;
        const int y2 = (a.size() == 4) ? toIntRounded(a.at(3)) : y1;
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

    // 注意：QRect(int x,int y,int w,int h) 这里的第三、四参数是宽高而不是 right/bottom，
    // 直接用 img.width()/img.height() 才能让 right/bottom 精确落在 (w-1)/(h-1) 上。
    const QRect imgRect(0, 0, img.width(), img.height());
    auto clampRectToImage = [&](QRect r) -> QRect {
        if (imgRect.width() <= 0 || imgRect.height() <= 0) return QRect();
        r = r.normalized();

        // 这里不要用 intersected()：当 bbox 完全越界时会变成 null 矩形，导致“只有准星、没有蓝框”。
        // 作为调试落盘图，更希望“至少能看到一个落在边缘的可视化框”，便于快速定位问题。
        const int x1 = qBound(imgRect.left(), r.left(), imgRect.right());
        const int y1 = qBound(imgRect.top(), r.top(), imgRect.bottom());
        const int x2 = qBound(imgRect.left(), r.right(), imgRect.right());
        const int y2 = qBound(imgRect.top(), r.bottom(), imgRect.bottom());
        return QRect(QPoint(x1, y1), QPoint(x2, y2)).normalized();
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

    // 调试可视化：当 bbox 太小（例如只有中心点）时，保证至少有一个“看得见的蓝色方框”。
    const int minBoxSize = qBound(12, qMin(img.width(), img.height()) / 25, 60);
    auto ensureMinVisibleRect = [&](QRect r) -> QRect {
        if (imgRect.width() <= 0 || imgRect.height() <= 0) return QRect();
        r = r.normalized();

        const QPoint c = clampPointToImage(r.center());
        int targetW = qMax(r.width(), minBoxSize);
        int targetH = qMax(r.height(), minBoxSize);
        targetW = qMin(targetW, imgRect.width());
        targetH = qMin(targetH, imgRect.height());

        QRect out(0, 0, targetW, targetH);
        out.moveCenter(c);
        if (out.left() < imgRect.left()) out.moveLeft(imgRect.left());
        if (out.top() < imgRect.top()) out.moveTop(imgRect.top());
        if (out.right() > imgRect.right()) out.moveRight(imgRect.right());
        if (out.bottom() > imgRect.bottom()) out.moveBottom(imgRect.bottom());
        return out;
    };

    const QRect bboxDraw = ensureMinVisibleRect(bboxClamped);
    const QRect toBboxDraw = hasToBbox ? ensureMinVisibleRect(toBboxClamped) : QRect();

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // bbox：蓝色框（与“系统相关为蓝色”的视觉约定一致）
    const QColor bboxBorder(0, 140, 255, 230);
    const QColor bboxFill(0, 140, 255, 40);
    p.setPen(QPen(bboxBorder, 3));
    p.setBrush(bboxFill);
    if (!bboxDraw.isNull()) p.drawRect(bboxDraw);

    // 中心点：十字 + 小圆
    const QColor centerColor(255, 255, 255, 240);
    p.setPen(QPen(centerColor, 2));
    p.setBrush(centerColor);
    p.drawEllipse(bboxCenter, 3, 3);
    p.drawLine(bboxCenter.x() - 12, bboxCenter.y(), bboxCenter.x() + 12, bboxCenter.y());
    p.drawLine(bboxCenter.x(), bboxCenter.y() - 12, bboxCenter.x(), bboxCenter.y() + 12);

    // to_bbox：橙色框（拖放终点等）
    if (hasToBbox && !toBboxDraw.isNull())
    {
        const QColor toBorder(255, 140, 0, 230);
        const QColor toFill(255, 140, 0, 30);
        QPen pen(toBorder, 3);
        pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(toFill);
        p.drawRect(toBboxDraw);

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
            // 附件信息属于“次要信息”，统一使用思考区的灰色，降低对正文阅读的干扰
            output_scroll(QStringLiteral("\n"), themeThinkColor());
        }
    }
    for (int i = 0; i < images_filepath.size(); ++i)
    {
        const QString rawPath = images_filepath[i];
        QFileInfo info(rawPath);
        const QString displayPath = info.exists() ? info.absoluteFilePath() : rawPath;
        const QString line = QStringLiteral("%1 %2").arg(imageBadge, QDir::toNativeSeparators(displayPath));
        // 将“请求附带的图片路径/工具产物路径”等统一用思考灰色渲染，避免输出区出现大量黑字路径刷屏
        output_scroll(line + QStringLiteral("\n"), themeThinkColor());
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
    if (!screen) return {};
    // 获取屏幕几何信息
    qreal devicePixelRatio = screen->devicePixelRatio();
    // qDebug() << "逻辑尺寸:" << screenGeometry.width() << screenGeometry.height();
    // qDebug() << "缩放比例:" << devicePixelRatio;
    // 直接使用 grabWindow 获取完整屏幕截图（会自动处理DPI）
    // 截图：优先走 Win32/GDI 的稳定实现，避免 Qt grabWindow 偶发阻塞 UI。
    QPixmap m_screenPicture;
#ifdef _WIN32
    {
        QString err;
        int w = 0;
        int h = 0;
        const QImage snap = capturePrimaryScreenWin32(&w, &h, &err);
        if (snap.isNull())
        {
            // 兜底：失败时不再回退到 grabWindow，避免把“卡死风险”带回来。
            reflash_state(QStringLiteral("ui:截屏失败（GDI）%1").arg(err.isEmpty() ? QString() : (QStringLiteral(" -> ") + err)), WRONG_SIGNAL);
            return {};
        }
        m_screenPicture = QPixmap::fromImage(snap);
        // 关键：把 devicePixelRatio 设置回 Qt 屏幕 DPR，使后续绘制/坐标换算逻辑保持一致。
        m_screenPicture.setDevicePixelRatio(devicePixelRatio);
    }
#else
    // 其他平台仍使用 Qt 原生截图
    m_screenPicture = screen->grabWindow(0);
#endif
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
    // 控制器截图一定要“干净”：避免把叠加提示截进去，污染模型输入。
    hideControllerOverlay();

    // 说明：此前这里会临时遮挡 ui->output 以避免“把对话内容一起截进桌面截图里”。
    // 但遮挡层会导致界面闪烁（每次自动截图都会盖一下再恢复），影响体验；按需求移除此逻辑。
    // 现在截图将如实反映当前桌面画面（包含 EVA 自身窗口），仅保证叠加提示不被截入。
    //
    // 处理一次事件：让“隐藏叠加层”的绘制尽快生效，降低 grabWindow 截到叠加层的概率。
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

    // 捕获全屏图像（controller 坐标系以主屏幕为基准）
    QImage scaledImage;
    int physicalWidth = 0;
    int physicalHeight = 0;
#ifdef _WIN32
    {
        struct WinCaptureResult
        {
            QImage image;
            int width = 0;
            int height = 0;
            QString error;
        };

        // turnId 守护：截图等待过程中允许用户点击“重置”，此时应立即放弃截图并返回。
        const quint64 guardTurnId = activeTurnId_;

        // 截图放到工作线程里做，并设置超时，避免极端情况下截图调用卡死 UI。
        // 注意：这里用“等待 + 事件循环”的方式让 UI 仍可响应（例如用户点击重置）。
        QFuture<WinCaptureResult> future = QtConcurrent::run([]() -> WinCaptureResult {
            WinCaptureResult result;
            result.image = capturePrimaryScreenWin32(&result.width, &result.height, &result.error);
            return result;
        });

        QFutureWatcher<WinCaptureResult> watcher;
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QTimer turnGuard;
        turnGuard.setInterval(30);
        turnGuard.setTimerType(Qt::PreciseTimer);
        QObject::connect(&watcher, &QFutureWatcher<WinCaptureResult>::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&turnGuard, &QTimer::timeout, &loop, [this, guardTurnId, &loop]() {
            if (guardTurnId == 0 || activeTurnId_ != guardTurnId)
            {
                loop.quit();
            }
        });
        watcher.setFuture(future);
        timeout.start(DEFAULT_CONTROLLER_SCREENSHOT_TIMEOUT_MS);
        turnGuard.start();
        loop.exec();
        turnGuard.stop();

        if (guardTurnId == 0 || activeTurnId_ != guardTurnId)
        {
            // 用户在等待截图时触发了 reset：放弃截图，直接返回空 frame。
            return frame;
        }

        if (!future.isFinished())
        {
            reflash_state(QStringLiteral("ui:桌面控制器截图超时（%1ms），本轮将不附带截图")
                              .arg(DEFAULT_CONTROLLER_SCREENSHOT_TIMEOUT_MS),
                          WRONG_SIGNAL);
            return frame;
        }

        const WinCaptureResult result = future.result();
        scaledImage = result.image;
        physicalWidth = result.width;
        physicalHeight = result.height;
        if (scaledImage.isNull())
        {
            reflash_state(QStringLiteral("ui:截屏失败，桌面控制器未能附带截图%1")
                              .arg(result.error.isEmpty() ? QString() : (QStringLiteral(" -> ") + result.error)),
                          WRONG_SIGNAL);
            return frame;
        }
    }
#else
    QPixmap snapshot = screen->grabWindow(0);
    if (snapshot.isNull())
    {
        reflash_state(QStringLiteral("ui:截屏失败，桌面控制器未能附带截图"), WRONG_SIGNAL);
        return frame;
    }
    scaledImage = snapshot.toImage();
    physicalWidth = int(qRound(snapshot.width() * screenDpr));
    physicalHeight = int(qRound(snapshot.height() * screenDpr));
#endif
    if (scaledImage.isNull())
    {
        reflash_state(QStringLiteral("ui:截屏数据为空，桌面控制器未能附带截图"), WRONG_SIGNAL);
        return frame;
    }

    // 若超过 1920x1080 则等比压缩，兼顾体积与可读性
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
    // reflash_state(QStringLiteral("ui:控制截图已保存 %1").arg(QDir::toNativeSeparators(basePath)), SIGNAL_SIGNAL);
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
