// CustomSwitchButton
// 自定义的状态按钮
// 使用时可直接包含此头文件或在designer中将组件提升为CustomSwitchButton类并设置正确的头文件路径
// 依赖qt5.15

#ifndef CUSTOMSWITCHBUTTON_H
#define CUSTOMSWITCHBUTTON_H

#include <QAbstractButton>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPropertyAnimation>
class CustomSwitchButton : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(int offset READ offset WRITE setOffset)

  public:
    explicit CustomSwitchButton(QWidget *parent = nullptr)
        : QAbstractButton(parent), m_offset(0), m_switchStatus(false), m_animation(new QPropertyAnimation(this, "offset", this))
    {
        setCheckable(true);
        // 创建阴影效果
        QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
        shadowEffect->setBlurRadius(10);
        shadowEffect->setXOffset(1);
        shadowEffect->setYOffset(1);
        shadowEffect->setColor(Qt::gray);

        // 应用到整个控件
        this->setGraphicsEffect(shadowEffect);
    }

    //默认宽高
    QSize sizeHint() const { return QSize(55, 25); }

  protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor borderColor = m_switchStatus ? Qt::green : Qt::gray;
        QColor buttonColor = Qt::white;

        p.setBrush(buttonColor);
        p.setPen(Qt::NoPen);

        p.drawEllipse(QRect(m_offset, 0, height(), height()));

        p.setBrush(borderColor);
        p.drawRoundedRect(0, 0, width(), height(), 5, 5); //圆角弧度5

        // 设置字体属性
        QFont font = p.font();
        font.setPointSize(12); // 设置字体大小
        font.setBold(true);    // 设置字体为粗体
        p.setFont(font);

        // 对于背景使用渐变色
        QLinearGradient backgroundGradient(0, 0, width(), 0);
        //有一种开关的感觉
        if (this->isChecked())
        {
            backgroundGradient.setColorAt(0, borderColor.lighter());
            backgroundGradient.setColorAt(1, borderColor.darker());
        }
        else
        {
            backgroundGradient.setColorAt(0, borderColor.darker());
            backgroundGradient.setColorAt(1, borderColor.lighter());
        }
        p.setBrush(backgroundGradient);
        p.drawRoundedRect(0, 0, width(), height(), 5, 5);

        // 绘制 "debug" 文字
        // 计算文字绘制的坐标,使其居中显示
        int textWidth = p.fontMetrics().width("debug");
        int textHeight = p.fontMetrics().height();
        int x = (width() - textWidth) / 2;
        int y = (height() + textHeight) / 2 - 2;
        p.setPen(Qt::black);
        p.drawText(x, y, "debug");
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_switchStatus = !m_switchStatus;
        m_animation->setStartValue(m_offset);
        m_animation->setEndValue(m_switchStatus ? width() - height() : 0);
        m_animation->setDuration(200);
        m_animation->start();
        QAbstractButton::mouseReleaseEvent(event);
    }

  private:
    int m_offset;
    bool m_switchStatus; // true for ON, false for OFF
    QPropertyAnimation *m_animation;

    int offset() const { return m_offset; }
    void setOffset(int offset)
    {
        m_offset = offset;
        update();
    }
};

#endif // CUSTOMSWITCHBUTTON_H
