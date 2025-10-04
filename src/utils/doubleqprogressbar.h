#ifndef DOUBLEQPROGRESSBAR_H
#define DOUBLEQPROGRESSBAR_H

#include <QBrush>
#include <QDebug>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProgressBar>

class DoubleQProgressBar : public QProgressBar
{
    Q_OBJECT

  public:
    explicit DoubleQProgressBar(QWidget *parent = nullptr)
        : QProgressBar(parent), m_secondValue(0), show_text("Progress: ") {}

    void setSecondValue(int value)
    {
        m_secondValue = value;
        update();
    }

    void setShowText(const QString &text)
    {
        show_text = text;
        update();
    }

    void setCenterText(const QString &text)
    {
        center_text = text;
        update();
    }

    int m_secondValue;
    QString show_text;
    QString center_text;

  protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRect rect = this->rect();
        QRect progressRect = rect;
        qreal radius = 4.0;

        // background
        QLinearGradient bgGradient(0, 0, 0, height());
        bgGradient.setColorAt(0, QColor(240, 240, 240));
        bgGradient.setColorAt(1, QColor(220, 220, 220));
        painter.setBrush(bgGradient);
        painter.setPen(QPen(QColor(180, 180, 180), 1));
        painter.drawRoundedRect(rect, radius, radius);

        int totalWidth = progressRect.width();
        int firstValue = value();
        int max = maximum();

        // avoid precision loss
        double firstRatio = max > 0 ? qBound(0.0, static_cast<double>(firstValue) / max, 1.0) : 0.0;
        double secondRatio = max > 0 ? qBound(0.0, static_cast<double>(m_secondValue) / max, 1.0 - firstRatio) : 0.0;

        // compute widths
        int firstWidth = qRound(totalWidth * firstRatio);
        int secondWidth = qRound(totalWidth * secondRatio);

        // ensure visible minimum
        if (firstRatio > 0 && firstWidth == 0) firstWidth = 1;
        if (secondRatio > 0 && secondWidth == 0) secondWidth = 1;

        // first segment - blue
        if (firstWidth > 0)
        {
            QLinearGradient blueGradient(0, 0, 0, height());
            blueGradient.setColorAt(0, QColor(100, 200, 255));
            blueGradient.setColorAt(0.5, QColor(50, 150, 255));
            blueGradient.setColorAt(1, QColor(0, 120, 220));

            QRectF blueRect(progressRect.x(), progressRect.y(), firstWidth, progressRect.height());

            // rounded corners
            qreal topRightRadius = 0.0;
            qreal bottomRightRadius = 0.0;

            if (m_secondValue == 0 && (firstValue >= max))
            {
                topRightRadius = radius;
                bottomRightRadius = radius;
            }

            QPainterPath bluePath = createRoundedRect(
                blueRect,
                radius, // top-left
                topRightRadius,
                bottomRightRadius,
                radius // bottom-left
            );

            painter.fillPath(bluePath, blueGradient);
        }

        // second segment - orange on top of first
        if (secondWidth > 0)
        {
            QLinearGradient orangeGradient(0, 0, 0, height());
            orangeGradient.setColorAt(0, QColor(255, 220, 100));
            orangeGradient.setColorAt(0.5, QColor(255, 180, 50));
            orangeGradient.setColorAt(1, QColor(255, 140, 0));

            QRectF orangeRect(
                progressRect.x() + firstWidth,
                progressRect.y(),
                secondWidth,
                progressRect.height());

            // rounded corners
            qreal topLeftRadius = 0.0;
            qreal topRightRadius = 0.0;
            qreal bottomRightRadius = 0.0;
            qreal bottomLeftRadius = 0.0;

            if (firstWidth == 0)
            {
                topLeftRadius = radius;
                bottomLeftRadius = radius;
            }
            if (firstValue + m_secondValue >= max)
            {
                topRightRadius = radius;
                bottomRightRadius = radius;
            }

            QPainterPath orangePath = createRoundedRect(
                orangeRect,
                topLeftRadius,
                topRightRadius,
                bottomRightRadius,
                bottomLeftRadius);

            painter.fillPath(orangePath, orangeGradient);
        }

        // gloss effect
        QLinearGradient glossGradient(0, 0, 0, height() / 3);
        glossGradient.setColorAt(0, QColor(255, 255, 255, 120));
        glossGradient.setColorAt(1, QColor(255, 255, 255, 0));
        painter.setBrush(glossGradient);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(progressRect, radius, radius);

        // center text: either custom center_text or default percent
        QString progressText = center_text.isEmpty() ? (show_text + QString::number(firstValue + m_secondValue) + "%") : center_text;
        QFont font = painter.font();
        font.setPointSize(10);
        painter.setFont(font);
        painter.setPen(QColor(60, 60, 60));
        painter.drawText(rect, Qt::AlignCenter, progressText);

        // border
        painter.setPen(QPen(QColor(150, 150, 150), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect, radius, radius);
    }

  private:
    // helper to create rounded rect path with independent corner radii
    QPainterPath createRoundedRect(const QRectF &rect,
                                   qreal topLeftRadius, qreal topRightRadius,
                                   qreal bottomRightRadius, qreal bottomLeftRadius) const
    {
        QPainterPath path;

        // clamp radii to half of width/height
        topLeftRadius = qMin(topLeftRadius, qMin(rect.width() / 2, rect.height() / 2));
        topRightRadius = qMin(topRightRadius, qMin(rect.width() / 2, rect.height() / 2));
        bottomRightRadius = qMin(bottomRightRadius, qMin(rect.width() / 2, rect.height() / 2));
        bottomLeftRadius = qMin(bottomLeftRadius, qMin(rect.width() / 2, rect.height() / 2));

        // top
        path.moveTo(rect.left() + topLeftRadius, rect.top());
        path.lineTo(rect.right() - topRightRadius, rect.top());

        // top-right corner
        if (topRightRadius > 0)
        {
            path.arcTo(rect.right() - 2 * topRightRadius, rect.top(),
                       2 * topRightRadius, 2 * topRightRadius,
                       90, -90);
        }

        // right
        path.lineTo(rect.right(), rect.bottom() - bottomRightRadius);

        // bottom-right corner
        if (bottomRightRadius > 0)
        {
            path.arcTo(rect.right() - 2 * bottomRightRadius,
                       rect.bottom() - 2 * bottomRightRadius,
                       2 * bottomRightRadius, 2 * bottomRightRadius,
                       0, -90);
        }

        // bottom
        path.lineTo(rect.left() + bottomLeftRadius, rect.bottom());

        // bottom-left corner
        if (bottomLeftRadius > 0)
        {
            path.arcTo(rect.left(), rect.bottom() - 2 * bottomLeftRadius,
                       2 * bottomLeftRadius, 2 * bottomLeftRadius,
                       -90, -90);
        }

        // left
        path.lineTo(rect.left(), rect.top() + topLeftRadius);

        // top-left corner
        if (topLeftRadius > 0)
        {
            path.arcTo(rect.left(), rect.top(),
                       2 * topLeftRadius, 2 * topLeftRadius,
                       180, -90);
        }

        path.closeSubpath();
        return path;
    }
};

#endif // DOUBLEQPROGRESSBAR_H