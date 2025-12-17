#include "controller_overlay.h"

#include <QApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

ControllerOverlay::ControllerOverlay(QWidget *parent)
    : QWidget(nullptr) // 顶层窗口：不跟随主窗口的坐标系
{
    Q_UNUSED(parent);

    // 作为“提示层”存在：不进入任务栏、不抢焦点、置顶、无边框、透明背景、不拦截鼠标
    // 关键点：必须做到“系统级输入透明”，否则在部分平台/窗口管理器下仍可能参与命中测试，导致真实点击被挡住。
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus |
                   Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    hideTimer_.setSingleShot(true);
    connect(&hideTimer_, &QTimer::timeout, this, &ControllerOverlay::hideNow);

    // monitor 倒计时刷新：只负责触发重绘，不做任何阻塞等待。
    monitorTickTimer_.setInterval(120);
    monitorTickTimer_.setTimerType(Qt::PreciseTimer);
    connect(&monitorTickTimer_, &QTimer::timeout, this, [this]() {
        if (mode_ != OverlayMode::MonitorCountdown)
        {
            monitorTickTimer_.stop();
            return;
        }
        // 倒计时文本在 paintEvent 中按当前时间计算，这里只需要重绘即可。
        repaint();
    });

    updateVirtualGeometry();
    hide();
}

void ControllerOverlay::updateVirtualGeometry()
{
    QRect rect;
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *s : screens)
    {
        if (!s) continue;
        rect = rect.isNull() ? s->geometry() : rect.united(s->geometry());
    }
    if (rect.isNull())
    {
        // 兜底：至少覆盖主屏
        QScreen *primary = QGuiApplication::primaryScreen();
        rect = primary ? primary->geometry() : QRect(0, 0, 1, 1);
    }
    virtualGeometry_ = rect;
    setGeometry(virtualGeometry_);
}

void ControllerOverlay::showHint(int globalX, int globalY, const QString &description, int durationMs)
{
    updateVirtualGeometry();

    mode_ = OverlayMode::ActionHint;
    monitorTickTimer_.stop();
    targetGlobalPos_ = QPoint(globalX, globalY);
    description_ = description;
    hintState_ = HintState::Pending;

    if (durationMs <= 0) durationMs = 400;
    hideTimer_.start(durationMs);

    // show/raise 不激活窗口（WA_ShowWithoutActivating），避免输入焦点被抢走
    show();
    raise();
    // 使用 repaint() 立即绘制，尽量避免“发出提示但用户看不到”的闪烁感。
    // 该函数由 UI 线程调用，叠加层内容很轻量，直接同步绘制成本可控。
    repaint();
}

void ControllerOverlay::showDoneHint(int globalX, int globalY, const QString &description, int durationMs)
{
    updateVirtualGeometry();

    mode_ = OverlayMode::ActionHint;
    monitorTickTimer_.stop();
    targetGlobalPos_ = QPoint(globalX, globalY);
    description_ = description;
    hintState_ = HintState::Done;

    // 完成态提示默认停留 1s；同样允许传入 0/负数时做一个小兜底，避免“瞬间消失”。
    if (durationMs <= 0) durationMs = 400;
    hideTimer_.start(durationMs);

    show();
    raise();
    repaint();
}

void ControllerOverlay::showMonitorCountdown(int waitMs)
{
    updateVirtualGeometry();

    mode_ = OverlayMode::MonitorCountdown;
    hintState_ = HintState::Pending;
    targetGlobalPos_ = QPoint(-1, -1);
    description_.clear();

    // 保护：避免异常值导致非常长的“置顶遮挡”体验。
    monitorTotalMs_ = qBound(0, waitMs, 30000);
    monitorDeadlineMs_ = QDateTime::currentMSecsSinceEpoch() + monitorTotalMs_;

    // waitMs=0 时依然显示一个很短的提示，避免“没有反馈”的割裂感。
    const int hideAfterMs = (monitorTotalMs_ <= 0) ? 400 : monitorTotalMs_;
    hideTimer_.start(hideAfterMs);

    monitorTickTimer_.start();

    show();
    raise();
    repaint();
}

void ControllerOverlay::hideNow()
{
    hideTimer_.stop();
    monitorTickTimer_.stop();
    hide();
}

void ControllerOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    // -----------------------------------------------------------------------------
    // monitor：顶部居中倒计时
    // -----------------------------------------------------------------------------
    if (mode_ == OverlayMode::MonitorCountdown)
    {
        // 计算剩余秒数（向上取整，保证用户看到的是“还剩几秒”）
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 remainMs = monitorDeadlineMs_ - now;
        if (remainMs < 0) remainMs = 0;
        const int remainSec = (remainMs <= 0) ? 0 : int((remainMs + 999) / 1000);

        QString text;
        if (monitorTotalMs_ <= 0 || remainSec <= 0)
        {
            text = QStringLiteral("等待中：即将截图");
        }
        else
        {
            text = QStringLiteral("等待中：%1 秒后截图").arg(remainSec);
        }

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        // 位置：主屏幕顶部居中（更符合用户注视区域）
        QRect anchor = virtualGeometry_;
        if (QScreen *primary = QGuiApplication::primaryScreen())
        {
            anchor = primary->geometry();
        }
        const int globalX = anchor.center().x();
        const int globalY = anchor.top() + 24;
        const QPoint local = QPoint(globalX, globalY) - virtualGeometry_.topLeft();

        // 字体：加粗放大，配合阴影提高复杂背景可读性
        QFont font = this->font();
        font.setBold(true);
        const qreal basePt = font.pointSizeF();
        if (basePt > 0.0)
        {
            font.setPointSizeF(qMax(20.0, basePt + 8.0));
        }
        else if (font.pixelSize() > 0)
        {
            font.setPixelSize(qMax(28, font.pixelSize() + 10));
        }
        else
        {
            font.setPointSize(20);
        }
        p.setFont(font);
        const QFontMetrics fm(font);
        const QSize textSize = fm.size(Qt::TextSingleLine, text);

        // 顶部居中：x 居中，y 固定偏移；并做边界保护避免越界
        int x = local.x() - textSize.width() / 2;
        int y = local.y();
        x = qBound(0, x, qMax(0, width() - textSize.width() - 1));
        y = qBound(0, y, qMax(0, height() - textSize.height() - 1));
        const QRect textRect(QPoint(x, y), textSize);

        const QColor textColor(40, 160, 255, 240); // 蓝色：偏“系统提示”语义
        const QColor shadow(0, 0, 0, 160);
        p.setPen(shadow);
        p.drawText(textRect.translated(2, 2), Qt::AlignLeft | Qt::AlignVCenter, text);
        p.setPen(textColor);
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        return;
    }

    // -----------------------------------------------------------------------------
    // controller：目标框 + 描述
    // -----------------------------------------------------------------------------
    if (targetGlobalPos_.x() < 0 || targetGlobalPos_.y() < 0) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPoint local = targetGlobalPos_ - virtualGeometry_.topLeft();
    const int half = kBoxSize / 2;
    QRect box(local.x() - half, local.y() - half, kBoxSize, kBoxSize);

    // 颜色规范：
    // - Pending（红色）：执行前确认“即将发生的动作”
    // - Done（绿色）：执行后确认“动作已完成”
    QColor border(255, 50, 50, 230);
    QColor fill(255, 50, 50, 50);
    QColor centerColor(255, 80, 80, 240);
    if (hintState_ == HintState::Done)
    {
        border = QColor(60, 220, 120, 230);
        fill = QColor(60, 220, 120, 50);
        centerColor = QColor(80, 255, 160, 240);
    }

    // 目标框
    p.setPen(QPen(border, 3));
    p.setBrush(fill);
    p.drawRect(box);

    // 中心点（小圆 + 十字）
    p.setPen(QPen(centerColor, 2));
    p.setBrush(centerColor);
    p.drawEllipse(local, 3, 3);
    p.drawLine(local.x() - 10, local.y(), local.x() + 10, local.y());
    p.drawLine(local.x(), local.y() - 10, local.x(), local.y() + 10);

    // 描述文本：尽量显示在框的上方，若空间不足则显示在下方
    QString text = description_.trimmed();
    if (text.isEmpty()) text = QStringLiteral("controller");

    QFont font = this->font();
    font.setBold(true);
    // 描述文字：用于让用户确认“将要执行/已执行”的动作，因此需要更大字号。
    // 这里按当前字体大小做增量放大，并设置一个最小值，避免在高分辨率/高 DPI 下仍显得过小。
    const qreal basePt = font.pointSizeF();
    if (basePt > 0.0)
    {
        font.setPointSizeF(qMax(18.0, basePt + 6.0));
    }
    else if (font.pixelSize() > 0)
    {
        font.setPixelSize(qMax(26, font.pixelSize() + 8));
    }
    else
    {
        font.setPointSize(18);
    }
    p.setFont(font);
    QFontMetrics fm(font);

    // 无背景：仅绘制文字（按需求移除黑色底框），因此尺寸按单行文本计算即可。
    const QSize textSize = fm.size(Qt::TextSingleLine, text);

    QPoint textTopLeft(box.left(), box.top() - textSize.height() - 8);
    if (textTopLeft.y() < 0)
    {
        textTopLeft.setY(box.bottom() + 8);
    }
    // 水平越界保护
    if (textTopLeft.x() + textSize.width() > width())
    {
        textTopLeft.setX(width() - textSize.width() - 1);
    }
    if (textTopLeft.x() < 0) textTopLeft.setX(0);

    QRect textRect(textTopLeft, textSize);

    // 文字颜色与状态一致（Pending=红，Done=绿），并用轻微阴影提高在复杂背景上的可读性。
    const QColor textColor = border;
    const QColor shadow(0, 0, 0, 160);
    p.setPen(shadow);
    p.drawText(textRect.translated(2, 2), Qt::AlignLeft | Qt::AlignVCenter, text);
    p.setPen(textColor);
    p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
}
