#include "evallogedit.h"

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QFont>
#include <QPainterPath>
#include <QLinearGradient>
#include <QPalette>

EvalLogEdit::EvalLogEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    // Transparent viewport, we'll paint background ourselves
    viewport()->setAutoFillBackground(false);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    QPalette pal = palette();
    pal.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    // Warm orange-ish text for eval logs
    pal.setColor(QPalette::Text, QColor(255, 200, 120));
    setPalette(pal);

    QFont f = font();
    f.setStyleHint(QFont::Monospace);
    f.setFamily("Consolas");
    setFont(f);

    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setReadOnly(true);

    m_animTimer.setInterval(30); // ~33 FPS is enough
    connect(&m_animTimer, &QTimer::timeout, this, &EvalLogEdit::onAnimTick);
}

void EvalLogEdit::setActive(bool on)
{
    if (on == m_active) return;
    m_active = on;
    if (on)
    {
        m_clock.restart();
        if (!m_animTimer.isActive()) m_animTimer.start();
        update();
    }
    else
    {
        if (m_animTimer.isActive()) m_animTimer.stop();
        // keep last frame for a quick resume
    }
}

void EvalLogEdit::paintEvent(QPaintEvent *e)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    // 1) black background
    drawBackground(p);
    // 2) tubes under overlays
    drawSyncTubes(p);
    // 3) grid ticks and edge symbols over tubes
    drawTicks(p);
    drawSymbols(p);
    p.end();
    QPlainTextEdit::paintEvent(e);
}

void EvalLogEdit::resizeEvent(QResizeEvent *e)
{
    m_cachedSize = QSize(); // force re-layout of symbols
    QPlainTextEdit::resizeEvent(e);
}

void EvalLogEdit::onAnimTick()
{
    // Gentle phase drift and breathing spread
    double t = m_clock.elapsed() / 1000.0; // seconds
    m_phase = fmod(m_phase + m_phaseSpeed * m_animTimer.interval() / 1000.0, 2 * M_PI);
    // Repaint
    update();
}

void EvalLogEdit::ensureSymbols()
{
    const QSize sz = viewport()->size();
    if (m_cachedSize == sz) return;
    m_cachedSize = sz;
    m_plusLeft.clear(); m_minusLeft.clear(); m_plusRight.clear(); m_minusRight.clear();
    m_plusTop.clear(); m_minusTop.clear(); m_plusBottom.clear(); m_minusBottom.clear();

    // Place +/- along edges at fixed spacing
    const int w = sz.width();
    const int h = sz.height();
    const int step = 28;
    for (int y = 14; y < h - 8; y += step)
    {
        if (((y / step) % 2) == 0) m_plusLeft << QPoint(6, y); else m_minusLeft << QPoint(6, y);
        if (((y / step) % 2) == 0) m_minusRight << QPoint(w - 12, y); else m_plusRight << QPoint(w - 12, y);
    }
    for (int x = 16; x < w - 8; x += step)
    {
        if (((x / step) % 2) == 0) m_plusTop << QPoint(x, 10); else m_minusTop << QPoint(x, 10);
        if (((x / step) % 2) == 0) m_minusBottom << QPoint(x, h - 12); else m_plusBottom << QPoint(x, h - 12);
    }
}

void EvalLogEdit::drawBackground(QPainter &p)
{
    // Pure black background only
    p.save();
    p.fillRect(viewport()->rect(), QColor(0, 0, 0));
    p.restore();
}

void EvalLogEdit::drawSymbols(QPainter &p)
{
    // Edge symbols: +/- in dim orange-white (overlay above tubes)
    p.save();
    ensureSymbols();
    QFont f = font();
    f.setPointSizeF(qMax(8.0, f.pointSizeF()));
    p.setFont(f);
    auto drawList = [&](const QVector<QPoint> &pts, const QString &s, const QColor &c)
    {
        p.setPen(c);
        for (const auto &pt : pts) p.drawText(pt, s);
    };
    const QColor cPlus(255, 210, 120, 150);
    const QColor cMinus(255, 190, 100, 140);
    drawList(m_plusLeft, "+", cPlus);
    drawList(m_minusLeft, "-", cMinus);
    drawList(m_plusRight, "+", cPlus);
    drawList(m_minusRight, "-", cMinus);
    drawList(m_plusTop, "+", cPlus);
    drawList(m_minusTop, "-", cMinus);
    drawList(m_plusBottom, "+", cPlus);
    drawList(m_minusBottom, "-", cMinus);
    p.restore();
}

void EvalLogEdit::drawTicks(QPainter &p)
{
    p.save();
    const QRect r = viewport()->rect();
    const int w = r.width();
    const int h = r.height();
    const QColor tick(255, 200, 120, 90);
    p.setPen(QPen(tick, 1));
    // top/bottom
    for (int x = 0; x < w; x += 10)
    {
        int len = (x % 50 == 0) ? 8 : 4;
        p.drawLine(QPoint(x, 0), QPoint(x, len));
        p.drawLine(QPoint(x, h - 1), QPoint(x, h - 1 - len));
    }
    // left/right
    for (int y = 0; y < h; y += 10)
    {
        int len = (y % 50 == 0) ? 8 : 4;
        p.drawLine(QPoint(0, y), QPoint(len, y));
        p.drawLine(QPoint(w - 1, y), QPoint(w - 1 - len, y));
    }
    p.restore();
}

QColor EvalLogEdit::tubeColorOuter(int i) const
{
    // Slightly different alpha per line; orange hue
    int base = 180 + (i % 3) * 10; // 180..200
    return QColor(255, base, 120, 80);
}

QColor EvalLogEdit::tubeColorInner(int i) const
{
    // Inner bright core tends to white
    int base = 210 + (i % 3) * 8; // 210..226
    return QColor(255, base, 200, 200);
}

void EvalLogEdit::drawSyncTubes(QPainter &p)
{
    // Sine tubes are disabled by default as requested; keep function for possible reuse.
    if (!m_showTubes) return;
    // Render several EVA‑style sine "tubes" that look hollow and segmented.
    // Implementation notes:
    // 1) Wide translucent outer stroke (orange/white) gives the tube rim.
    // 2) A thinner dark stroke on top carves a hollow center.
    // 3) Short crossbars are drawn perpendicular to the curve at regular spacing
    //    to give a segmented, section-by-section feel similar to the reference.

    const QRect r = viewport()->rect();
    const int w = r.width();
    const int h = r.height();
    if (w <= 0 || h <= 0) return;

    // Peak-to-peak occupies about one-third of the window height
    const double amp = qMax(10.0, h / 6.0);
    double cy = h * 0.45;
    if (cy < amp + 6) cy = amp + 6;
    if (cy > h - amp - 6) cy = h - amp - 6;

    const double TAU = 6.283185307179586;
    const double k = TAU / m_waveLen; // spatial frequency

    // Global horizontal drift handled by m_phase (updated in onAnimTick)
    const double basePhase = m_phase;

    // Lines converge/diverge via phase separation (no vertical offsets)
    const double t = m_clock.elapsed() / 1000.0;
    const double breath = 0.5 + 0.5 * sin(TAU * t / 8.0); // ~8s period
    const double minSep = 0.28; // radians when close (avoid merging)
    const double maxSep = 0.75; // radians when spread out
    const double delta = minSep + (maxSep - minSep) * breath;

    // Visual parameters of the tube (orange-white rim + hollow center)
    const qreal outerW = 24.0;    // outer rim width (3x thicker)
    const qreal innerW = 9.0;     // central hollow width (carve out the core)
    const qreal barHalf = 7.5;    // crossbar half length (slightly larger for readability)
    // Adaptive sampling: coarser at large widths to reduce CPU cost
    const int sampleDx = qBound(6, w / 220, 10);
    const int barStep = 22;       // crossbar spacing in px along x

    p.save();

    for (int i = 0; i < m_lines; ++i)
    {
        const double phi = basePhase + (i - (m_lines - 1) / 2.0) * delta;
        QPainterPath path;
        path.moveTo(0, cy + amp * sin(phi));
        for (int x = sampleDx; x <= w; x += sampleDx)
        {
            const double y = cy + amp * sin(k * x + phi);
            path.lineTo(x, y);
        }

        // Prebuild pens (reuse per line)
        static const QPen spineTpl(QColor(255, 245, 220, 150), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        static const QPen barTpl(QColor(255, 240, 200, 170), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        // 1) Outer translucent rim (orange/white gradient)
        QLinearGradient g(0, 0, 0, h);
        g.setColorAt(0.00, QColor(255, 220, 160, 180));
        g.setColorAt(0.50, QColor(255, 210, 140, 220));
        g.setColorAt(1.00, QColor(255, 230, 190, 180));
        QPen rim(g, outerW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(rim);
        p.drawPath(path);

        // Optional inner highlight dashed spine to hint segments along the path
        QPen spine = spineTpl;
        spine.setStyle(Qt::CustomDashLine);
        spine.setDashPattern(QVector<qreal>{7.0, 12.0});
        p.setPen(spine);
        p.drawPath(path);

        // 2) Hollow center carve (dark line)
        QPen carve(QColor(0, 0, 0, 230));
        carve.setWidthF(innerW);
        carve.setCapStyle(Qt::RoundCap);
        carve.setJoinStyle(Qt::RoundJoin);
        p.setPen(carve);
        p.drawPath(path);

        // 3) Crossbars: short segments perpendicular to the curve
        // Keep default composition for speed
        QPen barPen = barTpl;
        p.setPen(barPen);
        for (int x = 0; x <= w; x += barStep)
        {
            // y = A sin(kx + phi); y' = A k cos(kx + phi)
            const double phase = k * x + phi;
            const double y = cy + amp * sin(phase);
            const double dydx = amp * k * cos(phase);
            // Normal vector to the curve ~ (-dydx, 1)
            double nx = -dydx, ny = 1.0;
            const double invLen = 1.0 / qMax(1e-6, std::sqrt(nx * nx + ny * ny));
            nx *= invLen; ny *= invLen;
            const QPointF c(x, y);
            const QPointF a = c - QPointF(nx * barHalf, ny * barHalf);
            const QPointF b = c + QPointF(nx * barHalf, ny * barHalf);
            p.drawLine(a, b);
        }
        // end crossbars
    }

    p.restore();
}

