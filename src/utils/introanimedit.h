// IntroAnimEdit - QTextEdit with animated blue EVA-style background
// Qt 5.15, C++17
// Lightweight animation, explicitly start/stop via setActive(true/false) for lazy-load

#ifndef INTROANIMEDIT_H
#define INTROANIMEDIT_H

#include <QElapsedTimer>
#include <QPainter>
#include <QTextEdit>
#include <QTimer>

class IntroAnimEdit : public QTextEdit
{
    Q_OBJECT
  public:
    explicit IntroAnimEdit(QWidget *parent = nullptr);
    // Control animation lifecycle; call with true when the tab is visible
    void setActive(bool on);
    bool isActive() const { return m_active; }

  protected:
    void paintEvent(QPaintEvent *e) override;

  private:
    void drawBackground(QPainter &p);
    void drawHexGrid(QPainter &p, qreal hexSize, const QColor &color);
    void drawKabbalahTree(QPainter &p, const QRect &area);

  private:
    QTimer m_timer;        // frame timer (~30–60 FPS)
    QElapsedTimer m_clock; // for smooth motion
    bool m_active = false; // timers running
    qreal m_phase = 0.0;   // general phase (bobbing)
    qreal m_angle = 0.0;   // subtle head sway angle (deg)
};

#endif // INTROANIMEDIT_H
