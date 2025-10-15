#ifndef EVALLOGEDIT_H
#define EVALLOGEDIT_H

#include <QElapsedTimer>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTimer>
#include <QtMath>

// EvalLogEdit - QPlainTextEdit with orange EVA-style sync lines background
// - 7 sine-like parallel tubes (segmented, no pulse), slowly converge/diverge
// - Pure black background with +/- at edges and fine border ticks
// - Animation is lazy-started via setActive(true) and paused via setActive(false)
class EvalLogEdit : public QPlainTextEdit
{
    Q_OBJECT
  public:
    explicit EvalLogEdit(QWidget *parent = nullptr);
    void setActive(bool on); // start/stop animation timers
    bool isActive() const { return m_active; }
  protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
  private slots:
    void onAnimTick();
  private:
    void ensureSymbols();
    void drawBackground(QPainter &p);
    void drawTicks(QPainter &p);
    void drawSymbols(QPainter &p); // edge +/- symbols overlay
    void drawSyncTubes(QPainter &p);
    // Helpers
    QColor tubeColorOuter(int i) const; // outer glow color per line
    QColor tubeColorInner(int i) const; // inner core color per line

  private:
    QTimer m_animTimer;
    QElapsedTimer m_clock;
    bool m_active = false;
    bool m_showTubes = false; // draw sine tubes or not (off per user request)

    // Wave params
    int m_lines = 7; // number of parallel tubes when enabled
    double m_baseAmp = 16.0;      // sine amplitude in px
    double m_waveLen = 440.0;     // wavelength in px (doubled for longer curves)
    double m_spreadMax = 12.0;    // max offset between lines
    double m_spreadMin = 2.5;     // min offset between lines (near overlap)
    double m_phase = 0.0;         // optional gentle drift
    double m_phaseSpeed = 0.12;   // rad/s (slow)

    // Cached symbols layout for +/- on edges
    QSize m_cachedSize;
    QVector<QPoint> m_plusLeft, m_minusLeft, m_plusRight, m_minusRight, m_plusTop, m_minusTop, m_plusBottom, m_minusBottom;
};

#endif // EVALLOGEDIT_H
