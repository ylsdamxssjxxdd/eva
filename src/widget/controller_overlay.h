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

    // 在动作“执行完毕后”展示完成态提示（绿色）。
    // 设计目标：与 showHint() 的红色“即将执行”形成对比，帮助用户确认动作已发生。
    void showDoneHint(int globalX, int globalY, const QString &description, int durationMs = 2000);

    // 监视器工具：在主屏幕顶部居中显示“等待中 + 倒计时”，用于提示用户即将抓取截图。
    // waitMs 为等待时长（毫秒）；倒计时结束后自动隐藏。
    void showMonitorCountdown(int waitMs);

    // 立即隐藏（用于截图前清理叠加层）。
    void hideNow();

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    void updateVirtualGeometry();
    enum class OverlayMode
    {
        ActionHint,      // controller：目标框 + 描述
        MonitorCountdown // monitor：顶部居中倒计时提示
    };
    enum class HintState
    {
        Pending, // 即将执行：红色
        Done     // 执行完毕：绿色
    };

  private:
    QRect virtualGeometry_;
    QPoint targetGlobalPos_{-1, -1};
    QString description_;
    OverlayMode mode_ = OverlayMode::ActionHint;
    HintState hintState_ = HintState::Pending;
    QTimer hideTimer_;

    // monitor 倒计时：在 UI 线程里用 QTimer 刷新显示，不阻塞界面绘制。
    qint64 monitorDeadlineMs_ = 0;
    int monitorTotalMs_ = 0;
    QTimer monitorTickTimer_;

    static constexpr int kBoxSize = 80;
};
