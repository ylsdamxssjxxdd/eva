#include "controller_overlay.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

ControllerOverlay::ControllerOverlay(QWidget *parent)
    : QWidget(nullptr) // 顶层窗口：不跟随主窗口的坐标系
{
    Q_UNUSED(parent);

    // 作为“提示层”存在：不进入任务栏、不抢焦点、置顶、无边框、透明背景、不拦截鼠标
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFocusPolicy(Qt::NoFocus);

    hideTimer_.setSingleShot(true);
    connect(&hideTimer_, &QTimer::timeout, this, &ControllerOverlay::hide);

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

    targetGlobalPos_ = QPoint(globalX, globalY);
    description_ = description;

    if (durationMs <= 0) durationMs = 400;
    hideTimer_.start(durationMs);

    // show/raise 不激活窗口（WA_ShowWithoutActivating），避免输入焦点被抢走
    show();
    raise();
    update();
}

void ControllerOverlay::hideNow()
{
    hideTimer_.stop();
    hide();
}

void ControllerOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (targetGlobalPos_.x() < 0 || targetGlobalPos_.y() < 0) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPoint local = targetGlobalPos_ - virtualGeometry_.topLeft();
    const int half = kBoxSize / 2;
    QRect box(local.x() - half, local.y() - half, kBoxSize, kBoxSize);

    // 颜色：偏系统蓝（与“系统相关为蓝色”的视觉约定一致），并加一点透明度
    const QColor border(0, 140, 255, 220);
    const QColor fill(0, 140, 255, 30);
    const QColor centerColor(255, 255, 255, 230);
    const QColor textBg(0, 0, 0, 170);
    const QColor textFg(255, 255, 255, 240);

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
    p.setFont(font);
    QFontMetrics fm(font);

    const int paddingX = 10;
    const int paddingY = 6;
    const QRect rawRect = fm.boundingRect(text);
    QSize textSize(rawRect.width() + paddingX * 2, rawRect.height() + paddingY * 2);

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
    p.setPen(Qt::NoPen);
    p.setBrush(textBg);
    p.drawRoundedRect(textRect, 6, 6);

    p.setPen(textFg);
    p.drawText(textRect.adjusted(paddingX, paddingY, -paddingX, -paddingY),
               Qt::AlignLeft | Qt::AlignVCenter,
               text);
}
