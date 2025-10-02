// CustomTabWidget
// 自定义的TabWidget 支持纵向显示选项卡
// 使用时可直接包含此头文件或在designer中将组件提升为CustomQPlainTextEdit类并设置正确的头文件路径
// 依赖qt5.15

#ifndef CUSTOMTABWIDGET_H
#define CUSTOMTABWIDGET_H

#include <QPainter>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabBar>

class CustomTabBar : public QTabBar
{
  public:
    QSize tabSizeHint(int index) const override
    {
        QSize s = QTabBar::tabSizeHint(index);
        s.transpose(); // 交换宽和高

        // 获取旋转后的文本尺寸
        QFontMetrics fm = fontMetrics();
        int textWidth = fm.height();               // 文本的"宽度"实际上是旋转前的高度
        int textHeight = fm.width(tabText(index)); // 文本的"高度"是旋转前的宽度
        // 确保尺寸足够显示旋转后的文本
        s.setWidth(qMax(s.width(), textHeight));  // 适应文本宽度
        s.setHeight(qMax(s.height(), textWidth)); // 适应文本高度
        return s;
    }

  protected:
    void paintEvent(QPaintEvent * /* event */) override
    {
        QStylePainter painter(this);
        QStyleOptionTab opt;

        for (int i = 0; i < count(); i++)
        {
            initStyleOption(&opt, i);
            painter.save();

            // 选项卡的矩形区域
            QRect rect = tabRect(i);
            painter.translate(rect.center());
            painter.rotate(90); // 旋转90度

            // 更新选项卡的位置，使其在旋转后正确对齐
            opt.rect = QRect(QPoint(-rect.height() / 2, -rect.width() / 2), rect.size().transposed());

            // 绘制选项卡
            painter.drawControl(QStyle::CE_TabBarTab, opt);
            painter.restore();
        }
    }
};

// 主要类
#include <QTabWidget>
class CustomTabWidget : public QTabWidget
{
  public:
    CustomTabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent)
    {
        setTabBar(new CustomTabBar());
        setTabPosition(QTabWidget::West);
    }
};
#endif // CUSTOMTABWIDGET_H
