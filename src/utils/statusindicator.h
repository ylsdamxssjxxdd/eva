// StatusIndicator.h
#ifndef STATUSINDICATOR_H
#define STATUSINDICATOR_H

#include "../xconfig.h"
#include <QHBoxLayout>
#include <QMainWindow>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QVBoxLayout>
#include <QWidget>
// 状态灯
class StatusLed : public QWidget
{
    Q_OBJECT
  public:
    explicit StatusLed(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setState(MCP_CONNECT_MISS);
        setMinimumSize(24, 24);

        setAttribute(Qt::WA_TranslucentBackground);
        setAutoFillBackground(false); // 关闭自动填充背景
    }

    void setState(MCP_CONNECT_STATE newState)
    {
        m_state = newState;
        switch (m_state)
        {
        case MCP_CONNECT_MISS:
            m_color = QColor(255, 0, 0); // 红色
            break;
        case MCP_CONNECT_WIP:
            m_color = QColor(255, 255, 0); // 黄色
            break;
        case MCP_CONNECT_LINK:
            m_color = QColor(0, 255, 0); // 绿色
            break;
        }
        update();
    }

    QSize sizeHint() const override
    {
        return QSize(24, 24);
    }

  protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // 绘制背景
        // painter.fillRect(rect(), palette().window());

        // 计算绘制区域
        int diameter = qMin(width(), height()) - 4;
        int x = (width() - diameter) / 2;
        int y = (height() - diameter) / 2;

        // 绘制发光效果
        QRadialGradient gradient(x + diameter / 2, y + diameter / 2, diameter / 2);
        gradient.setColorAt(0, m_color.lighter(150));
        gradient.setColorAt(1, m_color.darker(200));

        painter.setBrush(gradient);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(x, y, diameter, diameter);
    }

  private:
    MCP_CONNECT_STATE m_state;
    QColor m_color;
};

#endif