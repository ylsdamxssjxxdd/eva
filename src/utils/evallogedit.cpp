#include "evallogedit.h"

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QFont>
#include <QPainterPath>
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
    drawBackground(p);
    drawTicks(p);
    drawSyncTubes(p);
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
    // Pure black background
    p.save();
    p.fillRect(viewport()->rect(), QColor(0, 0, 0));

    // Edge symbols: +/- in dim orange-white
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
    const QRect r = viewport()->rect();
    const int w = r.width();
    const int h = r.height();
    if (w <= 0 || h <= 0) return;

    // All waves share the same baseline; simple, clean lines only
    const double cy = h * 0.35;
    const double TAU = 6.283185307179586;
    const double k = TAU / m_waveLen; // spatial frequency

    // Global horizontal drift to move pattern; setActive/onAnimTick updates m_phase
    const double basePhase = m_phase;

    // Converge/diverge animation by modulating phase separation between lines (no vertical shift)
    const double t = m_clock.elapsed() / 1000.0;
    const double breath = 0.5 + 0.5 * sin(TAU * t / 8.0); // 8s period
    const double minSep = 0.12;  // radians between adjacent lines when close
    const double maxSep = 0.55;  // radians when spread out
    const double delta = minSep + (maxSep - minSep) * breath;

    // Simple single-stroke lines; no dash/glow/core
    QPen pen(QColor(255, 210, 120, 200));
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.save();
    p.setPen(pen);

    for (int i = 0; i < m_lines; ++i)
    {
        const double phi = basePhase + (i - (m_lines - 1) / 2.0) * delta;
        QPainterPath path;
        path.moveTo(0, cy + m_baseAmp * sin(phi));
        const int step = 6; // px sampling step
        for (int x = step; x <= w; x += step)
        {
            double y = cy + m_baseAmp * sin(k * x + phi);
            path.lineTo(x, y);
        }
        p.drawPath(path);
    }

    p.restore();
}

