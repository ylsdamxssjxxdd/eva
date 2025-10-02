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

    int m_secondValue;
    QString show_text;

  protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRect rect = this->rect();
        QRect progressRect = rect;
        qreal radius = 4.0;

        // 明亮的背景
        QLinearGradient bgGradient(0, 0, 0, height());
        bgGradient.setColorAt(0, QColor(240, 240, 240));
        bgGradient.setColorAt(1, QColor(220, 220, 220));
        painter.setBrush(bgGradient);
        painter.setPen(QPen(QColor(180, 180, 180), 1));
        painter.drawRoundedRect(rect, radius, radius);

        int totalWidth = progressRect.width();
        int firstValue = value();
        int max = maximum();

        // 使用浮点数计算避免精度损失
        double firstRatio = max > 0 ? qBound(0.0, static_cast<double>(firstValue) / max, 1.0) : 0.0;
        double secondRatio = max > 0 ? qBound(0.0, static_cast<double>(m_secondValue) / max, 1.0 - firstRatio) : 0.0;

        // 计算两个进度条的宽度（使用四舍五入）
        int firstWidth = qRound(totalWidth * firstRatio);
        int secondWidth = qRound(totalWidth * secondRatio);

        // 确保最小可见宽度
        if (firstRatio > 0 && firstWidth == 0) firstWidth = 1;
        if (secondRatio > 0 && secondWidth == 0) secondWidth = 1;

        // 第一个进度条 - 明亮蓝色
        if (firstWidth > 0)
        {
            QLinearGradient blueGradient(0, 0, 0, height());
            blueGradient.setColorAt(0, QColor(100, 200, 255));  // 亮蓝色
            blueGradient.setColorAt(0.5, QColor(50, 150, 255)); // 主蓝色
            blueGradient.setColorAt(1, QColor(0, 120, 220));    // 深蓝色

            QRectF blueRect(progressRect.x(), progressRect.y(), firstWidth, progressRect.height());

            // 确定圆角: 左侧总是圆角，右侧只有在没有第二个进度条且到达末尾时才圆角
            qreal topRightRadius = 0.0;
            qreal bottomRightRadius = 0.0;

            if (m_secondValue == 0 && (firstValue >= max))
            {
                topRightRadius = radius;
                bottomRightRadius = radius;
            }

            // 创建自定义圆角矩形
            QPainterPath bluePath = createRoundedRect(
                blueRect,
                radius, // 左上角总是圆角
                topRightRadius,
                bottomRightRadius,
                radius // 左下角总是圆角
            );

            painter.fillPath(bluePath, blueGradient);
        }

        // 第二个进度条 - 明亮橙色（绘制在第一个之上）
        if (secondWidth > 0)
        {
            QLinearGradient orangeGradient(0, 0, 0, height());
            orangeGradient.setColorAt(0, QColor(255, 220, 100));  // 亮橙色
            orangeGradient.setColorAt(0.5, QColor(255, 180, 50)); // 主橙色
            orangeGradient.setColorAt(1, QColor(255, 140, 0));    // 深橙色

            QRectF orangeRect(
                progressRect.x() + firstWidth,
                progressRect.y(),
                secondWidth,
                progressRect.height());

            // 确定圆角:
            // - 左侧：当第一个进度条为0时使用圆角
            // - 右侧：当两个进度条总和达到最大值时使用圆角
            qreal topLeftRadius = (firstValue == 0) ? radius : 0;
            qreal bottomLeftRadius = (firstValue == 0) ? radius : 0;
            qreal topRightRadius = (firstValue + m_secondValue >= max) ? radius : 0;
            qreal bottomRightRadius = (firstValue + m_secondValue >= max) ? radius : 0;

            // 修复：当第一个进度为0且第二个满时，四个角都应该是圆角
            if (firstValue == 0 && (firstValue + m_secondValue >= max))
            {
                topLeftRadius = radius;
                bottomLeftRadius = radius;
                topRightRadius = radius;
                bottomRightRadius = radius;
            }

            // 创建自定义圆角矩形
            QPainterPath orangePath = createRoundedRect(
                orangeRect,
                topLeftRadius,
                topRightRadius,
                bottomRightRadius,
                bottomLeftRadius);

            painter.fillPath(orangePath, orangeGradient);
        }

        // 添加光泽效果
        QLinearGradient glossGradient(0, 0, 0, height() / 3);
        glossGradient.setColorAt(0, QColor(255, 255, 255, 120));
        glossGradient.setColorAt(1, QColor(255, 255, 255, 0));
        painter.setBrush(glossGradient);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(progressRect, radius, radius);

        // 文字绘制
        QString progressText = show_text + QString::number(firstValue + m_secondValue) + "%";
        QFont font = painter.font();
        // font.setBold(true);
        font.setPointSize(10);
        painter.setFont(font);
        painter.setPen(QColor(60, 60, 60)); // 深灰色文字
        painter.drawText(rect, Qt::AlignCenter, progressText);

        // 外边框
        painter.setPen(QPen(QColor(150, 150, 150), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect, radius, radius);
    }

  private:
    // 创建自定义圆角的矩形路径
    QPainterPath createRoundedRect(const QRectF &rect,
                                   qreal topLeftRadius, qreal topRightRadius,
                                   qreal bottomRightRadius, qreal bottomLeftRadius) const
    {
        QPainterPath path;

        // 限制圆角半径不超过矩形尺寸的一半
        topLeftRadius = qMin(topLeftRadius, qMin(rect.width() / 2, rect.height() / 2));
        topRightRadius = qMin(topRightRadius, qMin(rect.width() / 2, rect.height() / 2));
        bottomRightRadius = qMin(bottomRightRadius, qMin(rect.width() / 2, rect.height() / 2));
        bottomLeftRadius = qMin(bottomLeftRadius, qMin(rect.width() / 2, rect.height() / 2));

        // 从左上角开始
        path.moveTo(rect.left() + topLeftRadius, rect.top());

        // 上边线
        path.lineTo(rect.right() - topRightRadius, rect.top());

        // 右上角
        if (topRightRadius > 0)
        {
            path.arcTo(rect.right() - 2 * topRightRadius, rect.top(),
                       2 * topRightRadius, 2 * topRightRadius,
                       90, -90);
        }

        // 右边线
        path.lineTo(rect.right(), rect.bottom() - bottomRightRadius);

        // 右下角
        if (bottomRightRadius > 0)
        {
            path.arcTo(rect.right() - 2 * bottomRightRadius,
                       rect.bottom() - 2 * bottomRightRadius,
                       2 * bottomRightRadius, 2 * bottomRightRadius,
                       0, -90);
        }

        // 下边线
        path.lineTo(rect.left() + bottomLeftRadius, rect.bottom());

        // 左下角
        if (bottomLeftRadius > 0)
        {
            path.arcTo(rect.left(), rect.bottom() - 2 * bottomLeftRadius,
                       2 * bottomLeftRadius, 2 * bottomLeftRadius,
                       -90, -90);
        }

        // 左边线
        path.lineTo(rect.left(), rect.top() + topLeftRadius);

        // 左上角
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