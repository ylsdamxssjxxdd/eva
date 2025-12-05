#pragma once

#include <QLabel>

/**
 * @brief 带省略展示的标签，用于在可用宽度不足时自动添加省略号。
 * 通过记录完整文本并在尺寸或字体变化时重新计算显示内容，避免长字符串撑开布局。
 */
class ElideLabel : public QLabel
{
    Q_OBJECT
  public:
    explicit ElideLabel(QWidget *parent = nullptr);

    // 设置需要省略的完整文本，组件内部会根据当前宽度自动截断显示
    void setContentText(const QString &text);
    // 配置省略模式，默认为右侧省略
    void setElideMode(Qt::TextElideMode mode);
    // 主动刷新省略结果，外部在布局宽度调整后可调用
    void refreshElide();

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    QSize minimumSizeHint() const override;

  private:
    void applyElide();

    QString fullText_;                 // 原始完整文本
    Qt::TextElideMode elideMode_ = Qt::ElideRight; // 省略方向
};
