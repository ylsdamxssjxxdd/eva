#include "introanimedit.h"

#include <QAbstractTextDocumentLayout>
#include <QPainterPath>
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
    connect(&m_timer, &QTimer::timeout, this, [this]()
            {
        qint64 ms = m_clock.elapsed();
        const qreal dt = qBound<qint64>(0, ms, 100) / 1000.0; // clamp to avoid spikes
        // Smooth, gentle motion
        m_phase += 0.6 * dt;              // bobbing phase
        m_angle = 5.0 * qSin(m_phase*0.8); // sway angle in degrees
        // Restart clock to avoid numerical blow-up
        m_clock.restart();
        update(); });
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

    // Kabbalah Tree of Life keeps its canonical size regardless of window changes
    const QSizeF treeSize(640.0, 860.0);
    QPointF center = r.center();
    const qreal upwardBias = qMin<qreal>(r.height() * 0.02, 32.0);
    center.setY(center.y() - upwardBias); // keep tree slightly above true center

    const qreal minLeft = r.right() - treeSize.width();
    const qreal maxLeft = qMax(minLeft, static_cast<qreal>(r.left()));
    qreal left = center.x() - treeSize.width() * 0.5;
    if (left < r.left()) left = r.left();
    if (left > maxLeft) left = maxLeft;

    const qreal minTopMargin = qBound<qreal>(32.0, r.height() * 0.07, 140.0);
    qreal top = center.y() - treeSize.height() * 0.5;
    if (top < r.top() + minTopMargin) top = r.top() + minTopMargin;
    const qreal maxTopInside = r.bottom() - treeSize.height();
    if (top > maxTopInside && maxTopInside >= r.top() + minTopMargin)
        top = qMin(top, maxTopInside);

    QRectF treeRect(left, top, treeSize.width(), treeSize.height());

    drawKabbalahTree(p, treeRect.toRect());
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

void IntroAnimEdit::drawKabbalahTree(QPainter &p, const QRect &area)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    // Geometry helpers
    const qreal w = area.width();
    const qreal h = area.height();
    const qreal x = area.left();
    const qreal y = area.top();

    auto rel = [&](qreal fx, qreal fy)
    {
        return QPointF(x + fx * w, y + fy * h);
    };

    // Vertical stretch of the tree (slightly taller overall)
    const qreal _yMin = 0.04, _yMax = 0.88;
    const qreal _yMid = (_yMin + _yMax) * 0.5;
    const qreal _yStretch = 1.08; // 8% taller as requested
    auto relY = [&](qreal fy) -> qreal
    { return _yMid + (fy - _yMid) * _yStretch; };

    // Sephiroth positions (10 nodes)
    QVector<QPointF> S(10);
    S[0] = rel(0.50, relY(0.04)); // Kether
    S[1] = rel(0.76, relY(0.16)); // Chokmah
    S[2] = rel(0.24, relY(0.16)); // Binah
    S[3] = rel(0.76, relY(0.32)); // Chesed
    S[4] = rel(0.24, relY(0.32)); // Geburah
    S[5] = rel(0.50, relY(0.46)); // Tiphareth
    S[6] = rel(0.76, relY(0.60)); // Netzach
    S[7] = rel(0.24, relY(0.60)); // Hod
    S[8] = rel(0.50, relY(0.74)); // Yesod
    S[9] = rel(0.50, relY(0.88)); // Malkuth

    // Connections per UI request: center column fully linked; bottom center connects to left/right bottoms; left/right pillars 2nd to 3rd linked
    QVector<QPair<int, int>> E = {
        {0, 1}, {0, 2}, {1, 2}, {0, 5}, {1, 3}, {2, 4}, {3, 4}, {3, 5}, {4, 5}, {3, 6}, {4, 7}, {5, 6}, {5, 7}, {6, 7}, {5, 8}, {6, 8}, {7, 8}, {8, 9}, {9, 6}, {9, 7}, {1, 5}, {2, 5}};

    // Base paths
    p.setPen(QPen(QColor(80, 180, 255, 110), 2.0, Qt::SolidLine, Qt::RoundCap));
    for (const auto &e : E)
        p.drawLine(S[e.first], S[e.second]);

    // Traveling glow along each path
    for (int i = 0; i < E.size(); ++i)
    {
        const auto &e = E[i];
        QPointF a = S[e.first];
        QPointF b = S[e.second];
        qreal t = fmod(m_phase * 0.35 + i * 0.07, 1.0);
        QPointF cpt = a + t * (b - a);
        qreal rad = qMax<qreal>(3.0, 5.0 * (1.0 + 0.2 * qSin(6 * m_phase + i)));
        QRadialGradient glow(cpt, rad);
        glow.setColorAt(0.0, QColor(190, 255, 255, 220));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(cpt, rad, rad);
    }

    // Sephiroth nodes
    for (int i = 0; i < S.size(); ++i)
    {
        qreal base = qMin(w, h) * (i == 0 || i == 9 ? 0.038 : 0.034);
        qreal pulse = 1.0 + 0.10 * qSin(m_phase * 2.0 + i * 0.7);
        qreal R = base * pulse;

        // Outer soft aura
        QRadialGradient aura(S[i], R * 1.8);
        aura.setColorAt(0.0, QColor(120, 210, 255, 90));
        aura.setColorAt(1.0, QColor(0, 0, 0, 0));
        p.setBrush(aura);
        p.setPen(Qt::NoPen);
        p.drawEllipse(S[i], R * 1.6, R * 1.6);

        // Core sphere
        QRadialGradient core(S[i], R);
        core.setColorAt(0.0, QColor(230, 245, 255, 240));
        core.setColorAt(0.6, QColor(140, 210, 255, 220));
        core.setColorAt(1.0, QColor(20, 40, 60, 220));
        p.setBrush(core);
        p.setPen(QPen(QColor(180, 230, 255, 200), 1.4));
        p.drawEllipse(S[i], R, R);
    }

    p.restore();
}
