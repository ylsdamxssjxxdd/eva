#include "introanimedit.h"

#include <QAbstractTextDocumentLayout>
#include <QPalette>
#include <QtMath>

IntroAnimEdit::IntroAnimEdit(QWidget *parent)
    : QTextEdit(parent)
{
    // Transparent viewport so we can draw our custom background
    viewport()->setAutoFillBackground(false);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);

    // Soft blue text on dark navy background (content draws above our bg)
    QPalette pal = palette();
    pal.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    pal.setColor(QPalette::Text, QColor(210, 230, 255));
    setPalette(pal);

    setFrameShape(QFrame::NoFrame);
    setAcceptRichText(true);
    setWordWrapMode(QTextOption::WordWrap);

    // Do not start timers automatically; caller controls lifecycle
    m_timer.setInterval(30); // ~33 FPS keeps CPU low
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        qint64 ms = m_clock.elapsed();
        // Phase advances slowly for a relaxed feel
        m_phase = fmod(m_phase + 0.0025 * ms, 1.0);
        m_angle += 0.15; // slow rotation
        if (m_angle >= 360.0) m_angle -= 360.0;
        // Restart clock to avoid numerical blow-up
        m_clock.restart();
        update();
    });
}

void IntroAnimEdit::setActive(bool on)
{
    if (on == m_active) return;
    m_active = on;
    if (on)
    {
        m_clock.restart();
        if (!m_timer.isActive()) m_timer.start();
        update();
    }
    else
    {
        if (m_timer.isActive()) m_timer.stop();
        // Keep the last frame; cheap when hidden
    }
}

void IntroAnimEdit::paintEvent(QPaintEvent *e)
{
    // Draw animated background (under the content)
    QPainter bg(viewport());
    bg.setRenderHint(QPainter::Antialiasing);
    drawBackground(bg);
    bg.end();

    // Let base class draw the rich text
    QTextEdit::paintEvent(e);
}

void IntroAnimEdit::drawBackground(QPainter &p)
{
    const QRect r = viewport()->rect();
    // EVA system (blue) themed vignette
    QRadialGradient g(r.center(), r.width() / 1.3);
    g.setColorAt(0, QColor(12, 22, 40));
    g.setColorAt(1, QColor(4, 8, 16));
    p.fillRect(r, g);

    // Subtle hex grid
    drawHexGrid(p, 34, QColor(80, 180, 255, 32));

    // Diagonal scanlines
    drawScanLines(p, QColor(80, 180, 255, 28));

    // Rotating rings
    drawRings(p, QColor(60, 140, 220, 50));
}

void IntroAnimEdit::drawHexGrid(QPainter &p, qreal hexSize, const QColor &color)
{
    p.save();
    p.setPen(QPen(color, 0.7));
    const qreal hexHeight = sqrt(3.0) * hexSize;
    const qreal hexWidth = 2 * hexSize;
    const qreal vertDist = hexHeight;
    const qreal horizDist = hexWidth * 0.75;
    QRect r = viewport()->rect().adjusted(-hexWidth, -hexHeight, hexWidth, hexHeight);
    for (qreal y = r.top(); y < r.bottom() + hexHeight; y += vertDist)
    {
        for (qreal x = r.left(); x < r.right() + hexWidth; x += horizDist)
        {
            qreal offset = (static_cast<int>((y - r.top()) / vertDist) % 2) ? (hexWidth * 0.5) : 0.0;
            QPointF c(x + offset, y);
            QPolygonF poly;
            for (int i = 0; i < 6; ++i)
            {
                qreal ang = i * 2 * M_PI / 6;
                poly << QPointF(c.x() + hexSize * cos(ang), c.y() + hexSize * sin(ang));
            }
            p.drawPolygon(poly);
        }
    }
    p.restore();
}

void IntroAnimEdit::drawScanLines(QPainter &p, const QColor &color)
{
    p.save();
    p.setPen(Qt::NoPen);
    QRect r = viewport()->rect();
    // Move bands diagonally using phase
    const qreal spacing = 18.0;
    const qreal band = 8.0;
    const qreal dx = spacing * m_phase * 2.0;
    QTransform tr;
    tr.translate(r.center().x(), r.center().y());
    tr.rotate(-28.0);
    tr.translate(-r.center().x(), -r.center().y());
    p.setTransform(tr);
    for (qreal x = r.left() - 200 + fmod(dx, spacing); x < r.right() + 200; x += spacing)
    {
        QRectF stripe(x, r.top() - 200, band, r.height() + 400);
        QLinearGradient lg(stripe.topLeft(), stripe.topRight());
        lg.setColorAt(0.0, QColor(0, 0, 0, 0));
        lg.setColorAt(0.5, color);
        lg.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.fillRect(stripe, lg);
    }
    p.resetTransform();
    p.restore();
}

void IntroAnimEdit::drawRings(QPainter &p, const QColor &color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QRect r = viewport()->rect();
    QPointF c = r.center();
    p.translate(c);
    p.rotate(m_angle);

    QVector<qreal> radii = { qMin(r.width(), r.height()) * 0.18,
                              qMin(r.width(), r.height()) * 0.30,
                              qMin(r.width(), r.height()) * 0.42 };
    for (int i = 0; i < radii.size(); ++i)
    {
        qreal rad = radii[i];
        QColor ringCol = color;
        ringCol.setAlpha(30 + 18 * i);
        p.setPen(QPen(ringCol, 1.2));
        p.drawEllipse(QPointF(0, 0), rad, rad);

        // Tick marks
        const int ticks = 36;
        for (int t = 0; t < ticks; ++t)
        {
            qreal a = (t * 360.0 / ticks) * M_PI / 180.0;
            qreal inner = rad * 0.96;
            qreal outer = rad * (t % 6 == 0 ? 1.04 : 1.02);
            QPointF p1(inner * cos(a), inner * sin(a));
            QPointF p2(outer * cos(a), outer * sin(a));
            p.setPen(QPen(ringCol, t % 6 == 0 ? 1.6 : 1.0));
            p.drawLine(p1, p2);
        }
    }
    p.restore();
}
