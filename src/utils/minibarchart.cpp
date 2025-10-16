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
    // Labels: TTFB / Generation / Common QA / Logic / Tools / Overall
    m_labels << QStringLiteral("首次响应")
             << QStringLiteral("生成速度")
             << QStringLiteral("常识问答")
             << QStringLiteral("逻辑推理")
             << QStringLiteral("工具调用")
             << QStringLiteral("同步率");
    setMinimumHeight(120);
}

void MiniBarChart::setScores(double s1, double s2, double s3, double s4, double s5, double s6)
{
    m_scores = {s1, s2, s3, s4, s5, s6};
    update();
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
    const int leftPad = 12;
    const int rightPad = 12;
    const int topPad = 10;
    const int bottomPad = 28; // for labels
    const QRectF plot(leftPad, topPad, rc.width() - leftPad - rightPad, rc.height() - topPad - bottomPad);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // Axis frame
    p.setPen(QPen(QColor(200, 200, 200)));
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
        QColor c = colNA;
        if (val >= 0)
        {
            const bool low = (val < 60.0);
            const bool overall = (i == 5); // 6th bar is Overall/同步率
            if (low) c = colGray; else c = (overall ? colOrange : colBlue);
        }
        p.setBrush(QBrush(c));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(x, y, barW, h), 2, 2);

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
}
