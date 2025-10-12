// NeuronLogEdit - QPlainTextEdit with animated 3D neuron background
// Reference: drawing logic adapted from CsvTableWidget (src/utils/csvtablewidget.h)
// Qt 5.15, C++17

#ifndef NEURONLOGEDIT_H
#define NEURONLOGEDIT_H

#include <QElapsedTimer>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRandomGenerator>
#include <QTimer>
#include <QtMath>

// Lightweight 3D vector
struct NLVector3D
{
    double x, y, z;
    NLVector3D(double x_ = 0, double y_ = 0, double z_ = 0) : x(x_), y(y_), z(z_) {}
    NLVector3D rotatedX(double angle) const
    {
        double rad = angle * M_PI / 180.0;
        return NLVector3D(x, y * cos(rad) - z * sin(rad), y * sin(rad) + z * cos(rad));
    }
    NLVector3D rotatedY(double angle) const
    {
        double rad = angle * M_PI / 180.0;
        return NLVector3D(x * cos(rad) + z * sin(rad), y, -x * sin(rad) + z * cos(rad));
    }
    NLVector3D rotatedZ(double angle) const
    {
        double rad = angle * M_PI / 180.0;
        return NLVector3D(x * cos(rad) - y * sin(rad), x * sin(rad) + y * cos(rad), z);
    }
};

class NeuronLogEdit : public QPlainTextEdit
{
    Q_OBJECT
  public:
    enum LayerType
    {
        InputLayer,
        HiddenLayer,
        OutputLayer
    };

    struct ConnectionHighlight
    {
        LayerType fromLayer;
        int fromIndex;
        LayerType toLayer;
        int toIndex;
        double progress; // 0~1
        int direction;   // 1 or -1
        double speed;    // 0.002~0.01 (random)
        int size;        // radius px
        int fadeFactor;  // 60~100
    };

    struct Pulsation
    {
        int layer;        // 0/1/2
        int index;        // neuron index in layer
        qint64 startTime; // ms
        int duration;     // ms
    };

    explicit NeuronLogEdit(QWidget *parent = nullptr);

  protected:
    void paintEvent(QPaintEvent *e) override;

  private slots:
    void onAnimTick();
    void onConnTick();
    void onPauseFinished();
    void onPulseTick();
    void spawnRandomHighlight();

  private:
    // Scene setup
    void initNeuralNetwork();
    void projectNodes(QVector<QPointF> &input, QVector<QPointF> &hidden, QVector<QPointF> &output);
    QPointF projectPoint(const NLVector3D &p, const QPoint &center, double scale);
    QVector<int> sortNodesByDepth(QVector<QPointF> &projected, const QVector<NLVector3D> &baseNodes, QVector<NLVector3D> &sortedRotated);

    // Drawing helpers
    void drawBackground(QPainter &p);
    void drawHexGrid(QPainter &painter, qreal hexSize, const QColor &color);
    void drawConnections(QPainter &painter, const QVector<QPointF> &from, const QVector<QPointF> &to, double allowCount);
    void drawHighlightPoints(QPainter &painter, const QVector<QPointF> &input, const QVector<QPointF> &hidden, const QVector<QPointF> &output);
    double easeInOut(double t);
    double getDepthFactor(double z);
    void drawNeuron(QPainter *p, const QPointF &center, qreal radius, const QColor &baseColor);

    // Pulsation
    void startPulsation(int layer, int index);
    double getPulsationScale(int layer, int index);

    // Connection indexing (to gate highlight spawning)
    int getConnectionIndex(LayerType fromLayer, int fromIndex, LayerType toLayer, int toIndex) const;

  private:
    // Animation state
    QTimer m_animTimer;
    QTimer m_connTimer;
    QTimer m_pauseTimer;
    QTimer m_pulseTimer;
    QTimer m_highlightTimer;
    QElapsedTimer m_clock;

    double m_rotX = 28.0;
    double m_rotY = 38.0;
    double m_rotZ = 0.0;
    double m_animSpeed = 0.18;
    double m_zoom = 1.0;
    double m_connStep = 0.0;
    double m_connStepDelta = 0.05;
    int m_parallelLines = 10;

    QVector<NLVector3D> m_inputNodes;
    QVector<NLVector3D> m_hiddenNodes;
    QVector<NLVector3D> m_outputNodes;

    QList<Pulsation> m_pulsations;
    double mPulseEps = 1.0;

    QList<ConnectionHighlight> m_highlights;
    QRandomGenerator m_rand;
    int m_highlightCount = 0;
    const int m_maxHighlights = 28; // cap to protect performance
};

#endif // NEURONLOGEDIT_H

