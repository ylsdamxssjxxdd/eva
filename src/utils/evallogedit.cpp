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

    // Compute breathing spread between lines (0..1)
    double t = m_clock.elapsed() / 1000.0;
    // Slow 8s period breathing
    double breath = 0.5 + 0.5 * sin(t * (2 * M_PI / 8.0));
    double spread = m_spreadMin + (m_spreadMax - m_spreadMin) * breath;

    // Vertical center around 35% height so text is readable
    double cy = h * 0.35;

    // Prepare dashed pen for segmented tubes
    QVector<qreal> dashes; dashes << 18 << 8; // segment/gap in px

    // One geometry shared phase so segments do not pulse; minor optional phase drift kept minimal
    double k = 2 * M_PI / m_waveLen;
    double phase = m_phase; // very slow drift

    // Draw back-to-front: outer glow then inner core for each line
    for (int i = 0; i < m_lines; ++i)
    {
        double offset = (i - (m_lines - 1) / 2.0) * spread;
        // Build path across width
        QPainterPath path;
        path.moveTo(0, cy + offset + m_baseAmp * sin(0 * k + phase));
        int step = 6; // px
        for (int x = step; x <= w; x += step)
        {
            double y = cy + offset + m_baseAmp * sin(x * k + phase);
            path.lineTo(x, y);
        }

        // Outer glow
        QPen glowPen(tubeColorOuter(i));
        glowPen.setWidthF(10.0);
        glowPen.setCapStyle(Qt::RoundCap);
        glowPen.setJoinStyle(Qt::RoundJoin);
        glowPen.setDashPattern(dashes);
        p.setPen(glowPen);
        p.drawPath(path);

        // Middle tube
        QPen midPen(QColor(255, 190, 110, 130));
        midPen.setWidthF(6.0);
        midPen.setCapStyle(Qt::RoundCap);
        midPen.setJoinStyle(Qt::RoundJoin);
        midPen.setDashPattern(dashes);
        p.setPen(midPen);
        p.drawPath(path);

        // Inner bright core
        QPen corePen(tubeColorInner(i));
        corePen.setWidthF(3.0);
        corePen.setCapStyle(Qt::RoundCap);
        corePen.setJoinStyle(Qt::RoundJoin);
        corePen.setDashPattern(dashes);
        p.setPen(corePen);
        p.drawPath(path);
    }
}