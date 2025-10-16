// FlowProgressBar - animated progress bar with gentle sheen highlight while running
// EVA-blue glossy style; the sweeping highlight is confined to the filled chunk only.
#pragma once

#include <QProgressBar>
#include <QTimer>
#include <QPainter>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainterPath>
#include <algorithm>

class FlowProgressBar : public QProgressBar
{
    Q_OBJECT
  public:
    explicit FlowProgressBar(QWidget *parent = nullptr)
        : QProgressBar(parent)
        , m_flowing(false)
        , m_offset(0)
    {
        // Smooth but calm animation
        m_timer.setInterval(50); // ~20 FPS
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            if (!m_flowing) return;
            m_offset += 2; // ~40 px/s at 20 FPS
            update();
        });
        setTextVisible(true);
        setMinimumHeight(22);
    }

    void setFlowing(bool on)
    {
        if (m_flowing == on) return;
        m_flowing = on;
        if (m_flowing) m_timer.start(); else m_timer.stop();
        update();
    }
    bool isFlowing() const { return m_flowing; }

  protected:
    void paintEvent(QPaintEvent *ev) override
    {
        Q_UNUSED(ev);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF rc = rect();
        const qreal radiusOuter = 5.0;

        // Background: EVA system blue gradient
        QLinearGradient bg(rc.topLeft(), rc.bottomLeft());
        bg.setColorAt(0.0, QColor(0x0b, 0x1e, 0x33));
        bg.setColorAt(1.0, QColor(0x0f, 0x2b, 0x4a));
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(rc.adjusted(0.5, 0.5, -0.5, -0.5), radiusOuter, radiusOuter);

        // Progress fraction
        const int mn = minimum();
        const int mx = maximum();
        const int v = value();
        const double denom = (mx > mn ? double(mx - mn) : 1.0);
        const double r = std::max(0.0, std::min(1.0, (double(v - mn) / denom)));
        const QRectF chunkRect(rc.x() + 1, rc.y() + 1, std::max(0.0, rc.width() * r) - 2, rc.height() - 2);

        // Filled chunk
        if (chunkRect.width() > 0.0)
        {
            QLinearGradient g(chunkRect.topLeft(), chunkRect.bottomLeft());
            g.setColorAt(0.00, QColor(0x6f, 0xc1, 0xff));
            g.setColorAt(0.45, QColor(0x48, 0x9e, 0xf7));
            g.setColorAt(0.55, QColor(0x2e, 0x7f, 0xd6));
            g.setColorAt(1.00, QColor(0x1e, 0x62, 0xb0));
            p.setBrush(g);
            p.setPen(QPen(QColor(255,255,255,20)));
            p.drawRoundedRect(chunkRect, 9, 9);

            // Top gloss highlight (static)
            QRectF gloss = chunkRect;
            gloss.setHeight(gloss.height() * 0.45);
            QLinearGradient hi(gloss.topLeft(), gloss.bottomLeft());
            hi.setColorAt(0.0, QColor(255,255,255,70));
            hi.setColorAt(1.0, QColor(255,255,255,0));
            p.setBrush(hi);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(gloss, 9, 9);
        }

        // Sweeping sheen highlight only inside the filled chunk while flowing
        if (m_flowing && chunkRect.width() > 0.0)
        {
            const int bandW = std::max(24, int(chunkRect.width() * 0.30));
            const int span = int(chunkRect.width()) + bandW;
            const int x = (span > 0 ? (m_offset % span) - bandW : -bandW);
            const QRectF bandRect(chunkRect.x() + x, chunkRect.y(), bandW, chunkRect.height());

            QLinearGradient sg(bandRect.topLeft(), bandRect.topRight());
            sg.setColorAt(0.00, QColor(255,255,255,0));
            sg.setColorAt(0.50, QColor(255,255,255,55));
            sg.setColorAt(1.00, QColor(255,255,255,0));

            p.save();
            QPainterPath clip;
            clip.addRoundedRect(chunkRect, 9, 9);
            p.setClipPath(clip);
            p.setBrush(sg);
            p.setPen(Qt::NoPen);
            p.drawRect(bandRect);
            p.restore();
        }

        // Centered text
        if (isTextVisible())
        {
            p.setPen(QColor(0xE8,0xF2,0xFF));
            p.drawText(rc.adjusted(4, 0, -4, 0), Qt::AlignCenter, text());
        }
    }

  private:
    QTimer m_timer;
    bool m_flowing;
    int m_offset; // pixel phase for sheen sweep
};
