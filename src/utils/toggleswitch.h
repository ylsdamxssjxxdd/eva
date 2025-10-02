#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

// 防止重复包含头文件

#include <QAbstractButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>

// 包含必要的Qt头文件

class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
    // 定义一个属性 "handlePosition"，用于控制滑块的位置
    Q_PROPERTY(qreal handlePosition READ handlePosition WRITE setHandlePosition)

  public:
    // explicit 构造函数，防止隐式转换
    explicit ToggleSwitch(QWidget *parent = nullptr)
        : QAbstractButton(parent)
    {
        // 设置控件可选中（checkable）
        setCheckable(true);
        // 设置鼠标指针为手型
        setCursor(Qt::PointingHandCursor);

        // 初始化滑块位置
        m_handlePosition = isChecked() ? 1.0 : 0.0;

        // 连接 toggled 信号到开始切换动画的槽函数
        connect(this, &QAbstractButton::toggled, this, &ToggleSwitch::startToggleAnimation);
    }

    // 返回滑块位置
    qreal handlePosition() const { return m_handlePosition; }

    // 设置滑块位置
    void setHandlePosition(qreal position)
    {
        m_handlePosition = position;
        update(); // 触发重绘
    }

    // 重写 sizeHint，提供控件的推荐大小
    QSize sizeHint() const override
    {
        return QSize(40, 20); // 默认大小，可根据需要调整
    }

    // QString mcp_record;//记录对应的mcp服务工具名

  protected:
    // 绘制控件
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing); // 开启抗锯齿

        // 定义尺寸
        int width = this->width();
        int height = this->height();
        int handleDiameter = height - 4; // 滑块直径，留出上下边距

        // 绘制背景
        QRectF backgroundRect(0, 0, width, height);
        p.setPen(Qt::NoPen);
        // 根据 checked 状态设置背景颜色
        p.setBrush(isChecked() ? QColor("#4CAF50") : QColor("#A0A0A0"));
        p.drawRoundedRect(backgroundRect, height / 2, height / 2); // 绘制圆角矩形

        // 绘制滑块
        qreal handleX = m_handlePosition * (width - handleDiameter - 4) + 2;
        QRectF handleRect(handleX, 2, handleDiameter, handleDiameter);
        p.setBrush(Qt::white);
        p.drawEllipse(handleRect); // 绘制滑块（圆形）
    }

    // 处理鼠标释放事件
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            if (rect().contains(event->pos()))
            {
                // 切换选中状态
                setChecked(!isChecked());
                // 发出 clicked 信号
                emit clicked(isChecked());
            }
            event->accept();
        }
        else
        {
            event->ignore();
        }
    }

  private slots:
    // 开始切换动画
    void startToggleAnimation(bool checked)
    {
        // 如果控件不可见，直接设置滑块位置
        if (!isVisible())
        {
            setHandlePosition(checked ? 1.0 : 0.0);
            return;
        }

        // 创建属性动画，目标属性为 handlePosition
        QPropertyAnimation *animation = new QPropertyAnimation(this, "handlePosition");
        animation->setDuration(200); // 动画持续时间 200 毫秒
        animation->setStartValue(m_handlePosition);
        animation->setEndValue(checked ? 1.0 : 0.0);
        animation->setEasingCurve(QEasingCurve::InOutCubic);     // 使用缓入缓出曲线
        animation->start(QAbstractAnimation::DeleteWhenStopped); // 动画结束后自动删除
    }

  private:
    qreal m_handlePosition; // 滑块位置，范围从 0.0 到 1.0
};

#endif // TOGGLESWITCH_H
