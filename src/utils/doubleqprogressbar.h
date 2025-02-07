#ifndef DOUBLEQPROGRESSBAR_H
#define DOUBLEQPROGRESSBAR_H

#include <QProgressBar>
#include <QPainter>
#include <QLinearGradient>
#include <QFont>
#include <QPalette>
#include <QDebug>

class DoubleQProgressBar : public QProgressBar {
    Q_OBJECT

public:
    explicit DoubleQProgressBar(QWidget *parent = nullptr) : QProgressBar(parent), m_secondValue(0) {}

    // 设置第二个值的大小
    void setSecondValue(int value) {
        m_secondValue = value;
        update();  // 更新进度条的显示
    }

    // 设置显示的文本
    void setShowText(const QString& text) {
        show_text = text;
        update();  // 更新进度条的显示
    }
    int m_secondValue;  // 第二个值
    QString show_text;  // 显示的文本

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿渲染

        const QRect rect = this->rect();
        const QRect progressRect = rect.adjusted(0, 0, 0, 0);
        const qreal radius = 0; // 圆角半径

        // 创建蓝色渐变
        QLinearGradient blueGradient(progressRect.topLeft(), progressRect.topRight());
        blueGradient.setColorAt(0, QColor(135, 206, 250));  // 浅蓝色
        blueGradient.setColorAt(1, QColor(120, 190, 240));  // 深蓝色

        // 创建橘黄色渐变
        QLinearGradient orangeGradient(progressRect.topLeft(), progressRect.topRight());
        orangeGradient.setColorAt(0, QColor(255, 200, 0));  // 浅橘黄色
        orangeGradient.setColorAt(1, QColor(255, 140, 0));  // 深橘黄色

        // 计算进度条宽度
        int firstValueWidth = static_cast<int>((progressRect.width() * value()) / static_cast<double>(maximum()));
        float secondValueWidth = (progressRect.width() * m_secondValue) / static_cast<double>(maximum());

        if (secondValueWidth > 0 && secondValueWidth < 1) {
            secondValueWidth = 1;  // 避免进度条过于细小
        }

        painter.setPen(Qt::NoPen);  // 不绘制边框

        // 绘制背景
        painter.setBrush(palette().color(QPalette::Background));
        painter.drawRoundedRect(progressRect, radius, radius);

        // 绘制蓝色进度
        painter.setBrush(blueGradient);
        QRectF blueRect(progressRect.x(), progressRect.y(), firstValueWidth, progressRect.height());
        painter.drawRoundedRect(blueRect, radius, radius);

        // 绘制橘黄色进度
        painter.setBrush(orangeGradient);
        QRectF orangeRect(progressRect.x() + firstValueWidth, progressRect.y(), secondValueWidth, progressRect.height());
        painter.drawRoundedRect(orangeRect, radius, radius);

        // 绘制百分比文本
        QString progressText = show_text + QString::number(m_secondValue + value()) + "%";
        QFont font = painter.font();
        font.setFamily("Arial");
        font.setPointSize(10);  // 设置字体大小
        painter.setFont(font);
        painter.setPen(Qt::black);  // 设置文本颜色为黑色
        painter.drawText(progressRect, Qt::AlignCenter, progressText);

        // 绘制边框
        painter.setPen(QPalette::WindowText);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect, radius, radius);
    }



};

#endif // DOUBLEQPROGRESSBAR_H
