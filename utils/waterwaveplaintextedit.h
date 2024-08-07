#include <QPainterPath>
#include <QApplication>
#include <QPlainTextEdit>
#include <QPainter>
#include <QTimer>
#include <QPaintEvent>
#include <cstdlib>
#include <ctime>

class WaterWavePlainTextEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    WaterWavePlainTextEdit(QWidget *parent = nullptr) 
        : QPlainTextEdit(parent), m_waveHeight(0), m_targetWaveOffset(0), m_animationRunning(false) {
        setAttribute(Qt::WA_OpaquePaintEvent);
        std::srand(std::time(nullptr));  // Initialize random seed
    }

    void startWaveAnimation() {
        if (!m_animationRunning) {
            m_animationRunning = true;
            m_timerId = startTimer(16);  // Increase frequency to 60 FPS (every 16ms)
        }
    }

    void stopWaveAnimation() {
        if (m_animationRunning) {
            m_animationRunning = false;
            killTimer(m_timerId);  // Stop the timer
        }
    }

    void clearWaterWave() {
        m_waveHeight = 0;  // Reset wave height
        m_waveOffset = 0;  // Reset wave offset
        m_targetWaveOffset = 0;  // Reset target wave offset
        viewport()->update();  // Request a repaint to clear the wave
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(viewport());

        // Draw the water wave background
        if (m_animationRunning) {
            drawWaterWaveBackground(painter);
        }

        // Draw the text on top of the background
        QPlainTextEdit::paintEvent(event);
    }

    void timerEvent(QTimerEvent *event) override {
        Q_UNUSED(event);
        if (!m_animationRunning) return;

        // Increase wave height gradually until it fills the viewport
        if (m_waveHeight < viewport()->height()) {
            m_waveHeight += 0.05;  // Smaller increment for smoother rising
        }

        // Gradually adjust wave offset towards the target offset
        if (std::abs(m_waveOffset - m_targetWaveOffset) < 0.05) {
            // Generate a new target offset if close enough to current offset
            m_targetWaveOffset = (std::rand() % 5 - 2) * 0.5;  // Randomly change between -1.0 and 1.0
        } else {
            // Smoothly move towards the target offset
            m_waveOffset += (m_targetWaveOffset - m_waveOffset) * 0.05;
        }

        viewport()->update();  // Request a repaint
    }

    void drawWaterWaveBackground(QPainter &painter) {
        painter.save();
        const int width = viewport()->width();
        const int height = viewport()->height();

        // Draw a rectangle that represents the water level with left-right height difference
        int leftHeight = height - m_waveHeight + m_waveOffset;
        int rightHeight = height - m_waveHeight - m_waveOffset;

        QPainterPath path;
        path.moveTo(0, leftHeight);
        path.lineTo(width, rightHeight);
        path.lineTo(width, height);
        path.lineTo(0, height);
        path.closeSubpath();

        painter.setBrush(QColor(255, 165, 0, 200));  // Orange color with transparency
        painter.setPen(Qt::NoPen);
        painter.drawPath(path);

        painter.restore();
    }

private:
    double m_waveHeight;  // Height of the water level, which increases over time
    double m_waveOffset = 0;  // Offset to create left-right height difference
    double m_targetWaveOffset;  // Target offset for smooth transition
    bool m_animationRunning;  // Whether the animation is running
    int m_timerId;  // Timer ID for controlling the animation
};
