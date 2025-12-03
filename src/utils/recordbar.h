#ifndef RECORDBAR_H
#define RECORDBAR_H

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyleOption>
#include <QToolTip>
#include <QVector>
#include <QWheelEvent>
// A thin horizontal bar that displays key conversation nodes as colored chips.
// - Hover: shows tooltip text (content snippet)
// - Single click: emit nodeClicked(index)
// - Double click: emit nodeDoubleClicked(index)
// - Horizontal scroll via mouse wheel; no visible scroll bar
class RecordBar : public QWidget
{
    Q_OBJECT
  public:
    explicit RecordBar(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMouseTracking(true);
        setMinimumHeight(18);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAttribute(Qt::WA_StyledBackground, true);
    }

    struct Node
    {
        QColor color;
        QString tooltip; // full or truncated content
        QString badge;   // short label (e.g., S/U/M/T)
    };

    int addNode(const QColor &color,
                const QString &tooltip,
                const QString &badge = QString())
    {
        Node n{color, tooltip, badge};
        nodes_.push_back(n);
        if (chipRectsCache_.size() != nodes_.size()) chipRectsCache_.resize(nodes_.size());
        // auto-scroll to latest when overflow occurs
        scrollX_ = maxScroll();
        updateGeometry();
        update();
        return nodes_.size() - 1;
    }

    void updateNode(int index, const QString &tooltip)
    {
        if (index < 0 || index >= nodes_.size()) return;
        nodes_[index].tooltip = tooltip;
        update();
    }

    void setNodeColor(int index, const QColor &c)
    {
        if (index < 0 || index >= nodes_.size()) return;
        nodes_[index].color = c;
        update();
    }

    void clearNodes()
    {
        nodes_.clear();
        chipRectsCache_.clear();
        scrollX_ = 0;
        selectedIndex_ = -1;
        hoveredIndex_ = -1;
        update();
    }

    // Programmatically set selection highlight and auto-scroll into view
    void setSelectedIndex(int idx)
    {
        if (idx < 0 || idx >= nodes_.size())
        {
            selectedIndex_ = -1;
        }
        else
        {
            selectedIndex_ = idx;
            // Ensure selected chip is visible
            if (selectedIndex_ >= 0 && chipW_ > 0)
            {
                const int stride = chipW_ + spacing_;
                const int xStart = margin_ + selectedIndex_ * stride;
                const int xEnd = xStart + chipW_;

                if (xStart - scrollX_ < 0)
                    scrollX_ = qMax(0, xStart);
                else if (xEnd - scrollX_ > width())
                    scrollX_ = qMin(maxScroll(), xEnd - width());
            }
        }
        update();
    }

  signals:
    void nodeClicked(int index);
    void nodeDoubleClicked(int index);

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false); // crisp EVA style (no soft edges)

        // Respect QSS/theming background
        QStyleOption opt;
        opt.init(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
        // Subtle textured background for better depth
        QRect bg = rect();
        QColor base = palette().color(QPalette::Window);
        // Vertical gradient overlay (very light)
        QLinearGradient g(0, 0, 0, bg.height());
        QColor top = base.lighter(108);
        top.setAlpha(64);
        QColor mid = base;
        mid.setAlpha(32);
        QColor bot = base.darker(108);
        bot.setAlpha(64);
        g.setColorAt(0.0, top);
        g.setColorAt(0.5, mid);
        g.setColorAt(1.0, bot);
        p.fillRect(bg, g);
        // Diagonal hatch with tiny alpha to add micro-texture
        QPixmap hatch(8, 8);
        hatch.fill(Qt::transparent);
        {
            QPainter hp(&hatch);
            hp.setRenderHint(QPainter::Antialiasing, false);
            QColor line1 = base.lighter(120);
            line1.setAlpha(18);
            QColor line2 = base.darker(120);
            line2.setAlpha(12);
            hp.setPen(QPen(line1, 1));
            for (int i = -8; i < 8; ++i) hp.drawLine(i, 7, i + 7, 0);
            hp.setPen(QPen(line2, 1));
            for (int i = -8; i < 8; ++i) hp.drawLine(i, 8, i + 8, 0); // slight offset
        }
        p.fillRect(bg, QBrush(hatch));
        // Top/bottom hairlines for separation
        p.setPen(QPen(base.lighter(120), 1));
        p.drawLine(bg.left(), 0, bg.right(), 0);
        p.setPen(QPen(base.darker(120), 1));
        p.drawLine(bg.left(), bg.bottom(), bg.right(), bg.bottom());

        const int N = nodes_.size();
        if (chipRectsCache_.size() != N) chipRectsCache_.resize(N);
        if (N <= 0) return;

        const int H = height();
        const int y = 2;
        const int h = H - 4;
        int x = margin_ - scrollX_;
        const int sl = qBound(0, slant_, chipW_ / 2);

        for (int i = 0; i < N; ++i)
        {
            const QRect r(x, y, chipW_, h);
            chipRectsCache_[i] = QRect(r);

            if (r.right() >= 0 && r.left() <= width())
            {
                const QColor base = nodes_[i].color;
                const bool isSel = (i == selectedIndex_);
                const bool isHover = (i == hoveredIndex_);

                // EVA: clean parallelogram (no radius, no shadow, no gradient)
                QPolygon poly;
                poly << QPoint(r.left() + sl, r.top())
                     << QPoint(r.right() + sl, r.top())
                     << QPoint(r.right() - sl, r.bottom())
                     << QPoint(r.left() - sl, r.bottom());

                QColor fill = base;
                if (isSel)
                    fill = base.lighter(140);
                else if (isHover)
                    fill = base.lighter(115);

                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawPolygon(poly);

                // Outline: crisp contour; thicker when selected
                QPen outline(isSel ? base.lighter(110) : base.darker(160), isSel ? 2 : 1);
                outline.setJoinStyle(Qt::MiterJoin);
                p.setPen(outline);
                p.setBrush(Qt::NoBrush);
                p.drawPolygon(poly);

                // Badge: initials to quickly identify role/tool
                if (!nodes_[i].badge.isEmpty())
                {
                    QFont f = p.font();
                    f.setBold(true);
                    f.setPointSize(qMax(6, f.pointSize() - 1));
                    QColor textColor = badgeTextColor(nodes_[i].color);
                    p.setFont(f);
                    p.setPen(textColor);
                    p.drawText(r, Qt::AlignCenter, nodes_[i].badge);
                }
            }

            x += chipW_ + spacing_;
        }
    }

    void resizeEvent(QResizeEvent *e) override
    {
        QWidget::resizeEvent(e);
        chipRectsCache_.resize(nodes_.size());
        // Clamp scroll offset to new max after width changes to keep chips responsive
        scrollX_ = qMin(maxScroll(), qMax(0, scrollX_));
        update();
    }

    void wheelEvent(QWheelEvent *ev) override
    {
        // Horizontal scroll with mouse wheel; shift accelerates
        int delta = ev->angleDelta().y();
        if (delta == 0) delta = ev->angleDelta().x();
        const int step = (ev->modifiers() & Qt::ShiftModifier) ? 60 : 30;
        if (delta < 0)
            scrollX_ = qMin(maxScroll(), scrollX_ + step);
        else if (delta > 0)
            scrollX_ = qMax(0, scrollX_ - step);
        update();
        ev->accept();
    }

    void mouseMoveEvent(QMouseEvent *ev) override
    {
        const int idx = indexAt(ev->pos());
        if (idx >= 0)
        {
            hoveredIndex_ = idx;
            QToolTip::showText(ev->globalPos(), nodes_[idx].tooltip, this);
        }
        else
        {
            hoveredIndex_ = -1;
            QToolTip::hideText();
            ev->ignore();
        }
        update();
    }

    void leaveEvent(QEvent *) override
    {
        hoveredIndex_ = -1;
        update();
    }

    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->button() != Qt::LeftButton)
        {
            ev->ignore();
            return;
        }
        const int idx = indexAt(ev->pos());
        if (idx >= 0)
        {
            selectedIndex_ = idx; // persist highlight on click
            update();
            emit nodeClicked(idx);
        }
    }

    void mouseDoubleClickEvent(QMouseEvent *ev) override
    {
        if (ev->button() != Qt::LeftButton)
        {
            ev->ignore();
            return;
        }
        const int idx = indexAt(ev->pos());
        if (idx >= 0)
        {
            selectedIndex_ = idx;
            update();
            emit nodeDoubleClicked(idx);
        }
    }

  private:
    QColor badgeTextColor(const QColor &bg) const
    {
        // Simple contrast heuristic
        const int l = bg.toHsl().lightness();
        return (l < 128) ? QColor(Qt::white) : QColor(Qt::black);
    }

    int indexAt(const QPoint &pt) const
    {
        // Compute index based on geometry and scroll; prefer cache if initialized
        if (chipRectsCache_.size() == nodes_.size())
        {
            for (int i = 0; i < chipRectsCache_.size(); ++i)
            {
                const QRect r = chipRectsCache_[i];
                if (r.contains(pt)) return i;
            }
        }
        // fallback math
        const int x = pt.x() + scrollX_ - margin_;
        if (x < 0) return -1;
        const int stride = chipW_ + spacing_;
        const int idx = x / stride;
        const int local = x % stride;
        if (idx >= 0 && idx < nodes_.size() && local < chipW_) return idx;
        return -1;
    }

    int maxScroll() const
    {
        const int totalW = margin_ * 2 + nodes_.size() * chipW_ + qMax(0, nodes_.size() - 1) * spacing_;
        return qMax(0, totalW - width());
    }

  private:
    QVector<Node> nodes_;
    QVector<QRect> chipRectsCache_;
    int scrollX_ = 0;
    // visuals
    int chipW_ = 18;
    int spacing_ = 3;
    int margin_ = 6;
    int slant_ = 4; // EVA style tilt (px)
    // interaction state
    int selectedIndex_ = -1;
    int hoveredIndex_ = -1;
};

#endif // RECORDBAR_H
