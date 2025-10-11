#ifndef RECORDBAR_H
#define RECORDBAR_H

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyleOption>
#include <QToolTip>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

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
    };

    int addNode(const QColor &color, const QString &tooltip)
    {
        Node n{color, tooltip};
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
        update();
    }

  signals:
    void nodeClicked(int index);
    void nodeDoubleClicked(int index);

  protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        // draw styled background to match UI theme
        QStyleOption opt;
        opt.init(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
        const int N = nodes_.size();
        if (chipRectsCache_.size() != N) chipRectsCache_.resize(N);
        if (N <= 0) return;
        const int H = height();
        const int y = 2;
        const int h = H - 4;
        int x = margin_ - scrollX_;
        for (int i = 0; i < N; ++i)
        {
            const QRect r(x, y, chipW_, h);
            if (r.right() >= 0 && r.left() <= width())
            {
                p.setPen(QPen(QColor(0, 0, 0), 1));
                p.setBrush(nodes_[i].color);
                p.drawRect(r.adjusted(0, 0, -1, -1));
            }
            chipRectsCache_[i] = QRect(r);
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
            QToolTip::showText(ev->globalPos(), nodes_[idx].tooltip, this);
        }
        else
        {
            QToolTip::hideText();
            ev->ignore();
        }
    }

    void mousePressEvent(QMouseEvent *ev) override
    {
        if (ev->button() != Qt::LeftButton)
        {
            ev->ignore();
            return;
        }
        const int idx = indexAt(ev->pos());
        if (idx >= 0) emit nodeClicked(idx);
    }

    void mouseDoubleClickEvent(QMouseEvent *ev) override
    {
        if (ev->button() != Qt::LeftButton)
        {
            ev->ignore();
            return;
        }
        const int idx = indexAt(ev->pos());
        if (idx >= 0) emit nodeDoubleClicked(idx);
    }

  private:
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
    int chipW_ = 14;
    int spacing_ = 4;
    int margin_ = 6;
};

#endif // RECORDBAR_H
