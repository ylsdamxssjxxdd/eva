#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QWidget>

// -----------------------------------------------------------------------------
// 桌面控制器叠加层
// -----------------------------------------------------------------------------
// 设计目标：
// 1) 作为“透明置顶窗口”覆盖整个虚拟桌面，不抢焦点、不拦截鼠标事件；
// 2) 在指定屏幕坐标处绘制中心点 + 80x80 目标框，并绘制描述文字；
// 3) 自动定时隐藏，避免被后续截图捕获、减少干扰。
// -----------------------------------------------------------------------------
class ControllerOverlay final : public QWidget
{
    Q_OBJECT

  public:
    explicit ControllerOverlay(QWidget *parent = nullptr);

    // 在全局屏幕坐标 (globalX/globalY) 处展示提示。
    // durationMs 为自动隐藏的时间（毫秒）。
    void showHint(int globalX, int globalY, const QString &description, int durationMs = 3000);

    // 立即隐藏（用于截图前清理叠加层）。
    void hideNow();

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    void updateVirtualGeometry();

  private:
    QRect virtualGeometry_;
    QPoint targetGlobalPos_{-1, -1};
    QString description_;
    QTimer hideTimer_;

    static constexpr int kBoxSize = 80;
};
