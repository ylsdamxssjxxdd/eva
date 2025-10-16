// MiniBarChart - compact 6-bar chart for evaluation scores
#include "minibarchart.h"
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <algorithm>

MiniBarChart::MiniBarChart(QWidget *parent)
    : QWidget(parent)
{
    // Initialize scores as N/A
    m_scores = QVector<double>(6, -1.0);
    // Default labels; UI may override via setLabels()
    m_labels << QStringLiteral("TTFB")
             << QStringLiteral("Gen")
             << QStringLiteral("QA")
             << QStringLiteral("Logic")
             << QStringLiteral("Tools")
             << QStringLiteral("Sync");
    setMinimumHeight(120);
}

void MiniBarChart::setScores(double s1, double s2, double s3, double s4, double s5, double s6)
{
    m_scores = {s1, s2, s3, s4, s5, s6};
    update();
}

void MiniBarChart::setLabels(const QStringList &labels)
{
    if (labels.size() == 6) {
        m_labels = labels;
        update();
    }
}

void MiniBarChart::paintEvent(QPaintEvent *ev)
{
    Q_UNUSED(ev);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rc = rect();
    p.fillRect(rc, palette().base());

    // Chart paddings
    const int n = m_scores.size();
    if (n <= 0) return;
    const int leftPad = 28;   // allow ticks/labels
    const int rightPad = 12;
    const int topPad = 10;
    const int bottomPad = 28; // for labels
    const QRectF plot(leftPad, topPad, rc.width() - leftPad - rightPad, rc.height() - topPad - bottomPad);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // Subtle plot background with faint vertical depth
    QLinearGradient bg(plot.topLeft(), plot.bottomLeft());
    bg.setColorAt(0.0, palette().base().color().darker(102));
    bg.setColorAt(1.0, palette().base().color().darker(108));
    p.fillRect(plot, bg);

    // Grid lines (5 horizontal bands at 0/25/50/75/100%)
    p.save();
    const QColor grid = QColor(255, 255, 255, 22);
    p.setPen(QPen(grid, 1));
    for (int i = 0; i <= 4; ++i) {
        const qreal y = plot.y() + plot.height() * (1.0 - i / 4.0);
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
    p.restore();

    // Axis frame (very light)
    p.setPen(QPen(QColor(200, 200, 200, 90)));
    p.drawRect(plot);

    // Bar geometry
    const double gap = std::max(4.0, plot.width() * 0.02);
    const double barW = std::max(12.0, (plot.width() - gap * (n + 1)) / n);
    const double x0 = plot.x() + gap;

    // Colors
    const QColor colBlue(80, 160, 255);     // metrics
    const QColor colOrange(255, 165, 0);    // overall (同步率)
    const QColor colGray(180, 180, 180);    // low score (<60)
    const QColor colNA(220, 220, 220);      // N/A

    // Draw bars and labels
    QFont f = font();
    QFontMetrics fm(f);
    for (int i = 0; i < n; ++i)
    {
        const double val = m_scores[i];
        const double ratio = (val < 0 ? 0.0 : std::min(100.0, std::max(0.0, val)) / 100.0);
        const double h = plot.height() * ratio;
        const double x = x0 + i * (barW + gap);
        const double y = plot.y() + (plot.height() - h);

        // Color per spec: overall(同步率) orange; others blue; <60 gray
        QColor base = colNA;
        if (val >= 0)
        {
            const bool low = (val < 60.0);
            const bool overall = (i == 5); // 6th bar is Overall/同步率
            if (low) base = colGray; else base = (overall ? colOrange : colBlue);
        }

        // Build a vertical gradient from a lighter tone to the base to add depth
        auto tone = [&](const QColor &c, int lighter) {
            QColor t = c;
            return t.lighter(lighter);
        };
        QLinearGradient grad(QPointF(x, y), QPointF(x, y + h));
        grad.setColorAt(0.0, tone(base, 140));
        grad.setColorAt(0.5, base);
        grad.setColorAt(1.0, base.darker(115));

        // Bar body
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        const qreal radius = 3.0;
        p.drawRoundedRect(QRectF(x, y, barW, h), radius, radius);

        // Gloss highlight (top 40%)
        if (h > 6.0) {
            QRectF gloss(x, y, barW, h * 0.4);
            QLinearGradient hi(gloss.topLeft(), gloss.bottomLeft());
            hi.setColorAt(0.0, QColor(255,255,255,65));
            hi.setColorAt(1.0, QColor(255,255,255,0));
            p.setBrush(hi);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(gloss, radius, radius);
        }

        // Value label on top
        p.setPen(QPen(palette().text().color()));
        const QString vt = (val >= 0 ? QString::number(val, 'f', 0) : QStringLiteral("-"));
        const QRectF vrc(x - 4, plot.y() - 2, barW + 8, 16);
        p.drawText(vrc, Qt::AlignHCenter | Qt::AlignBottom, vt);

        // X label
        const QString lb = (i < m_labels.size() ? m_labels[i] : QString("#%1").arg(i + 1));
        const QRectF lrc(x - 8, rc.bottom() - bottomPad + 4, barW + 16, bottomPad - 4);
        p.drawText(lrc, Qt::AlignHCenter | Qt::AlignTop, lb);
    }

    // Left ticks 0/50/100 (small, unobtrusive)
    p.setPen(QPen(QColor(200,200,200,120)));
    for (int i : {0, 50, 100}) {
        const qreal y = plot.y() + plot.height() * (1.0 - i / 100.0);
        p.drawText(QRectF(2, y - 8, leftPad - 6, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(i));
    }
}
