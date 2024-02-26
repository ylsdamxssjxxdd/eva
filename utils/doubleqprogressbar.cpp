#include "doubleqprogressbar.h"

DoubleQProgressBar::DoubleQProgressBar(QWidget *parent)
{

}

void DoubleQProgressBar::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿渲染（更平滑的圆角）

    const QRect rect = this->rect();
    const QRect progressRect = rect;
    const qreal radius = 0; // 圆角的半径

    // 创建蓝色渐变
    QLinearGradient blueGradient(progressRect.topLeft(), progressRect.topRight());
    blueGradient.setColorAt(0, QColor(135, 206, 250)); // 浅蓝色
    blueGradient.setColorAt(1, QColor(120, 190, 240));     // 深蓝色

    // 创建橘黄色渐变
    QLinearGradient orangeGradient(progressRect.topLeft(), progressRect.topRight());
    orangeGradient.setColorAt(0, QColor(255, 200, 0)); // 浅橘黄色
    orangeGradient.setColorAt(1, QColor(255, 140, 0)); // 深橘黄色

    // 计算第一个颜色的宽度
    int firstValueWidth = static_cast<int>((progressRect.width() * value()) / static_cast<double>(maximum()));
    // 计算第二个颜色的宽度
    float secondValueWidth = (progressRect.width() * m_secondValue) / static_cast<double>(maximum());
    if(secondValueWidth>0 && secondValueWidth<1){secondValueWidth=1;}

    painter.setPen(Qt::NoPen);

    // 绘制背景，通常是灰色或者控件的背景色
    painter.setBrush(palette().color(QPalette::Background));
    painter.drawRoundedRect(progressRect, radius, radius);

    // 绘制蓝色进度
    painter.setBrush(blueGradient);
    
    QRectF blueRect(progressRect.x(), progressRect.y(), firstValueWidth, progressRect.height());
    painter.drawRoundedRect(blueRect, radius, radius);

    //绘制橘黄色进度
    painter.setBrush(orangeGradient);
    QRectF orangeRect(progressRect.x() + firstValueWidth, progressRect.y(), secondValueWidth, progressRect.height());
    painter.drawRoundedRect(orangeRect, radius, radius);

    // 绘制百分比文本
    QString progressText = message + QString::number(m_secondValue+value()) + "%";
    QFont font = painter.font();
    font.setFamily("Arial");
    font.setPointSize(9); // 设置字体大小
    painter.setFont(font);
    painter.setPen(Qt::black); // 设置文本颜色为黑色
    painter.drawText(progressRect, Qt::AlignCenter, progressText);

    // 绘制边框（如果需要）
    painter.setPen(QPalette::WindowText);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect, radius, radius);


}

void DoubleQProgressBar::setSecondValue(int value)
{
    m_secondValue = value;
    update(); // 更新进度条的显示
}
