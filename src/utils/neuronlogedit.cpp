#include "neuronlogedit.h"

#include <QAbstractTextDocumentLayout>
#include <QDateTime>
#include <QPainterPath>
#include <QPalette>
#include <algorithm>
NeuronLogEdit::NeuronLogEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    // Transparent viewport so we can draw our custom background
    viewport()->setAutoFillBackground(false);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    QPalette pal = palette();
    pal.setColor(QPalette::Base, QColor(0, 0, 0, 0));
    pal.setColor(QPalette::Text, QColor(180, 255, 200)); // soft neon green
    setPalette(pal);

    // Monospace for logs
    QFont f = font();
    f.setStyleHint(QFont::Monospace);
    f.setFamily("Consolas"); // fallback picks a monospace on other OS
    setFont(f);

    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setReadOnly(true);

    // Init scene
    m_rand.seed(QDateTime::currentMSecsSinceEpoch());
    initNeuralNetwork();

    // Timers (do not start by default; start when setActive(true))
    m_animTimer.setInterval(30);
    connect(&m_animTimer, &QTimer::timeout, this, &NeuronLogEdit::onAnimTick);

    m_pauseTimer.setSingleShot(true);
    connect(&m_pauseTimer, &QTimer::timeout, this, &NeuronLogEdit::onPauseFinished);

    m_connTimer.setInterval(30);
    connect(&m_connTimer, &QTimer::timeout, this, &NeuronLogEdit::onConnTick);

    m_pulseTimer.setInterval(16); // ~60 FPS
    connect(&m_pulseTimer, &QTimer::timeout, this, &NeuronLogEdit::onPulseTick);
    // m_pulseTimer will be started in setActive(true)
    // m_clock will be started in setActive(true)

    m_highlightTimer.setInterval(220);
    connect(&m_highlightTimer, &QTimer::timeout, this, &NeuronLogEdit::spawnRandomHighlight);
    // m_highlightTimer will be started in setActive(true)
}

void NeuronLogEdit::setActive(bool on)
{
    if (on == m_active) return;
    m_active = on;
    if (on)
    {
        // Reset progression to provide a clean start each time
        m_connStep = 0.0;
        m_clock.restart();
        if (!m_animTimer.isActive()) m_animTimer.start();
        if (!m_connTimer.isActive()) m_connTimer.start();
        if (!m_pulseTimer.isActive()) m_pulseTimer.start();
        if (!m_highlightTimer.isActive()) m_highlightTimer.start();
        // Drop any lingering pause state
        if (m_pauseTimer.isActive()) m_pauseTimer.stop();
        update();
    }
    else
    {
        // Stop all timers to save resources
        if (m_animTimer.isActive()) m_animTimer.stop();
        if (m_connTimer.isActive()) m_connTimer.stop();
        if (m_pulseTimer.isActive()) m_pulseTimer.stop();
        if (m_highlightTimer.isActive()) m_highlightTimer.stop();
        if (m_pauseTimer.isActive()) m_pauseTimer.stop();
        // Keep current frame; no aggressive clear to allow quick resume
    }
}

void NeuronLogEdit::paintEvent(QPaintEvent *e)
{
    // Draw animated background (under the text)
    QPainter bg(viewport());
    bg.setRenderHint(QPainter::Antialiasing);
    drawBackground(bg);
    bg.end();

    // Let base class draw the text and cursor
    QPlainTextEdit::paintEvent(e);
}

void NeuronLogEdit::onAnimTick()
{
    m_rotY += 0.5 * m_animSpeed;
    m_rotX += 0.2 * m_animSpeed;
    m_rotZ += 0.1 * m_animSpeed;
    m_rotX = fmod(m_rotX, 360.0);
    m_rotY = fmod(m_rotY, 360.0);
    m_rotZ = fmod(m_rotZ, 360.0);
    update();
}

void NeuronLogEdit::onConnTick()
{
    const double totalConn = m_inputNodes.size() * m_hiddenNodes.size() + m_hiddenNodes.size() * m_outputNodes.size();
    double oldStep = m_connStep;
    m_connStep += m_connStepDelta;
    double newStep = m_connStep;

    // Pulses: Input->Hidden
    int ih_idx = 0;
    for (int i = 0; i < m_inputNodes.size(); ++i)
    {
        for (int j = 0; j < m_hiddenNodes.size(); ++j, ++ih_idx)
        {
            double departure = ih_idx + mPulseEps;
            if (oldStep < departure && newStep >= departure)
                startPulsation(0, i);
            double arrival = ih_idx + m_parallelLines - mPulseEps;
            if (oldStep < arrival && newStep >= arrival)
                startPulsation(1, j);
        }
    }

    // Pulses: Hidden->Output
    int ho_idx_base = static_cast<int>(m_inputNodes.size() * m_hiddenNodes.size());
    int ho_idx = 0;
    for (int i = 0; i < m_hiddenNodes.size(); ++i)
    {
        for (int j = 0; j < m_outputNodes.size(); ++j, ++ho_idx)
        {
            double departure = ho_idx_base + ho_idx + mPulseEps;
            if (oldStep < departure && newStep >= departure)
                startPulsation(1, i);
            double arrival = departure + m_parallelLines - 2 * mPulseEps;
            if (oldStep < arrival && newStep >= arrival)
                startPulsation(2, j);
        }
    }

    const double restartThreshold = totalConn - 1 + m_parallelLines;
    if (m_connStep >= restartThreshold)
    {
        m_connTimer.stop();
        m_pauseTimer.start(9000);
    }
}

void NeuronLogEdit::onPauseFinished()
{
    m_connStep = 0.0;
    m_connTimer.start();
}

void NeuronLogEdit::onPulseTick()
{
    qint64 now = m_clock.elapsed();
    bool needs = !m_pulsations.isEmpty() || !m_highlights.isEmpty();

    // Trim finished pulses
    auto it = std::remove_if(m_pulsations.begin(), m_pulsations.end(), [&](const Pulsation &p)
                             { return (now - p.startTime) > p.duration; });
    if (it != m_pulsations.end()) m_pulsations.erase(it, m_pulsations.end());

    // Update highlights; drop invalid/expired
    QMutableListIterator<ConnectionHighlight> itH(m_highlights);
    while (itH.hasNext())
    {
        ConnectionHighlight &h = itH.next();
        int idx = getConnectionIndex(h.fromLayer, h.fromIndex, h.toLayer, h.toIndex);
        if (idx == -1 || m_connStep < idx)
        {
            itH.remove();
            m_highlightCount--;
            continue;
        }
        h.progress += h.speed * h.direction;
        if (h.progress < 0.0)
        {
            h.progress = 0.0;
            h.direction = 1;
        }
        else if (h.progress > 1.0)
        {
            h.progress = 1.0;
            h.direction = -1;
        }
        if (m_rand.bounded(100) < 2)
        {
            itH.remove();
            m_highlightCount--;
        }
    }

    if (needs) update();
}

void NeuronLogEdit::spawnRandomHighlight()
{
    if (m_highlightCount >= m_maxHighlights) return;

    bool i2h = m_rand.bounded(2) == 0;
    ConnectionHighlight h;
    int connIndex = -1;
    if (i2h)
    {
        h.fromLayer = InputLayer;
        h.toLayer = HiddenLayer;
        h.fromIndex = m_rand.bounded(m_inputNodes.size());
        h.toIndex = m_rand.bounded(m_hiddenNodes.size());
        connIndex = getConnectionIndex(InputLayer, h.fromIndex, HiddenLayer, h.toIndex);
    }
    else
    {
        h.fromLayer = HiddenLayer;
        h.toLayer = OutputLayer;
        h.fromIndex = m_rand.bounded(m_hiddenNodes.size());
        h.toIndex = m_rand.bounded(m_outputNodes.size());
        connIndex = getConnectionIndex(HiddenLayer, h.fromIndex, OutputLayer, h.toIndex);
    }

    if (connIndex == -1 || m_connStep < connIndex) return;

    h.progress = m_rand.bounded(100) / 100.0;
    h.direction = m_rand.bounded(2) == 0 ? 1 : -1;
    h.speed = 0.002 + (m_rand.bounded(80) / 10000.0);
    h.size = 1 + m_rand.bounded(3);
    h.fadeFactor = 60 + m_rand.bounded(41);
    m_highlights.append(h);
    m_highlightCount++;
}

void NeuronLogEdit::initNeuralNetwork()
{
    const int inputCount = 7;
    const int hiddenCount = 11;
    const int outputCount = 7;

    const double layerDepth = 140.0;
    const double startZ = -layerDepth;
    const double inputSpacing = 75.0;
    const double hiddenSpacing = 60.0;
    const double outputSpacing = 75.0;

    m_inputNodes.clear();
    m_hiddenNodes.clear();
    m_outputNodes.clear();

    for (int i = 0; i < inputCount; ++i)
    {
        double y = (i - (inputCount - 1) / 2.0) * inputSpacing;
        m_inputNodes.append(NLVector3D(-280, y, startZ));
    }
    for (int i = 0; i < hiddenCount; ++i)
    {
        double y = (i - (hiddenCount - 1) / 2.0) * hiddenSpacing;
        m_hiddenNodes.append(NLVector3D(0, y, startZ + layerDepth));
    }
    for (int i = 0; i < outputCount; ++i)
    {
        double y = (i - (outputCount - 1) / 2.0) * outputSpacing;
        m_outputNodes.append(NLVector3D(280, y, startZ + 2 * layerDepth));
    }
}

void NeuronLogEdit::projectNodes(QVector<QPointF> &input, QVector<QPointF> &hidden, QVector<QPointF> &output)
{
    input.clear();
    hidden.clear();
    output.clear();
    QPoint center(viewport()->width() / 2, viewport()->height() / 2);
    const double scale = 1.0;
    for (const auto &n : m_inputNodes)
    {
        NLVector3D r = n.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
        input.append(projectPoint(r, center, scale));
    }
    for (const auto &n : m_hiddenNodes)
    {
        NLVector3D r = n.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
        hidden.append(projectPoint(r, center, scale));
    }
    for (const auto &n : m_outputNodes)
    {
        NLVector3D r = n.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
        output.append(projectPoint(r, center, scale));
    }
}

QPointF NeuronLogEdit::projectPoint(const NLVector3D &p, const QPoint &center, double scale)
{
    double zOffset = 800.0;
    double f = zOffset / (zOffset + p.z);
    return QPointF(center.x() + p.x * f * scale * m_zoom, center.y() - p.y * f * scale * m_zoom);
}

QVector<int> NeuronLogEdit::sortNodesByDepth(QVector<QPointF> &projected, const QVector<NLVector3D> &baseNodes, QVector<NLVector3D> &sortedRotated)
{
    int n = projected.size();
    QVector<NLVector3D> rotated(n);
    for (int i = 0; i < n; ++i)
        rotated[i] = baseNodes[i].rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);

    QVector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b)
              { return rotated[a].z < rotated[b].z; });

    QVector<QPointF> sortedProj(n);
    sortedRotated.resize(n);
    for (int i = 0; i < n; ++i)
    {
        int oi = order[i];
        sortedProj[i] = projected[oi];
        sortedRotated[i] = rotated[oi];
    }
    projected = sortedProj;
    return order;
}

void NeuronLogEdit::drawBackground(QPainter &p)
{
    // Vignette background
    const QRect r = viewport()->rect();
    QRadialGradient g(r.center(), r.width() / 1.4);
    g.setColorAt(0, QColor(18, 30, 18));
    g.setColorAt(1, QColor(8, 12, 8));
    p.fillRect(r, g);

    // Subtle hex grid
    drawHexGrid(p, 36, QColor(0, 255, 0, 35));

    // Prepare nodes
    QVector<QPointF> in, hid, out;
    projectNodes(in, hid, out);
    QVector<QPointF> inU = in, hidU = hid, outU = out;

    QVector<NLVector3D> inR, hidR, outR;
    QVector<int> inIdx = sortNodesByDepth(in, m_inputNodes, inR);
    QVector<int> hidIdx = sortNodesByDepth(hid, m_hiddenNodes, hidR);
    QVector<int> outIdx = sortNodesByDepth(out, m_outputNodes, outR);

    // Connections
    double maxIH = m_inputNodes.size() * m_hiddenNodes.size();
    double cur = m_connStep;
    double ihCount = cur;
    double hoCount = cur - maxIH;
    p.setPen(QPen(QColor(0, 200, 100, 80), 0.8));
    drawConnections(p, inU, hidU, ihCount);
    drawConnections(p, hidU, outU, hoCount);

    // Glowing particles along established links
    drawHighlightPoints(p, inU, hidU, outU);

    // Neurons (sorted by depth; pulsating)
    for (int i = 0; i < in.size(); ++i)
    {
        int oi = inIdx[i];
        double scale = getPulsationScale(0, oi);
        double depth = getDepthFactor(inR[i].z);
        drawNeuron(&p, in[i], 11 * depth * scale, QColor(0, 255, 100, 200 * depth));
    }
    for (int i = 0; i < hid.size(); ++i)
    {
        int oi = hidIdx[i];
        double scale = getPulsationScale(1, oi);
        double depth = getDepthFactor(hidR[i].z);
        drawNeuron(&p, hid[i], 11 * depth * scale, QColor(0, 255, 100, 200 * depth));
    }
    for (int i = 0; i < out.size(); ++i)
    {
        int oi = outIdx[i];
        double scale = getPulsationScale(2, oi);
        double depth = getDepthFactor(outR[i].z);
        drawNeuron(&p, out[i], 11 * depth * scale, QColor(0, 255, 100, 200 * depth));
    }
}

void NeuronLogEdit::drawHexGrid(QPainter &p, qreal hexSize, const QColor &color)
{
    p.save();
    p.setPen(QPen(color, 0.7));
    const qreal hexHeight = sqrt(3.0) * hexSize;
    const qreal hexWidth = 2 * hexSize;
    const qreal vertDist = hexHeight;
    const qreal horizDist = hexWidth * 0.75;
    const int extra = static_cast<int>(hexSize * 2);
    const QRect r = viewport()->rect();
    for (int row = -2; (row * vertDist) < r.height() + extra; ++row)
    {
        for (int col = -2; (col * horizDist) < r.width() + extra; ++col)
        {
            qreal cx = col * horizDist;
            qreal cy = row * vertDist;
            if (qAbs(col) % 2 == 1) cy += vertDist / 2.0;
            QPolygonF hex;
            for (int i = 0; i < 6; ++i)
            {
                qreal ang = 60.0 * i * M_PI / 180.0;
                hex << QPointF(cx + hexSize * cos(ang), cy + hexSize * sin(ang));
            }
            p.drawPolygon(hex);
        }
    }
    p.restore();
}

void NeuronLogEdit::drawConnections(QPainter &p, const QVector<QPointF> &from, const QVector<QPointF> &to, double allowCount)
{
    if (allowCount <= 0) return;
    const int fromSize = from.size();
    const int toSize = to.size();
    const double window = m_parallelLines;
    int idx = 0;
    for (int i = 0; i < fromSize; ++i)
    {
        for (int j = 0; j < toSize; ++j, ++idx)
        {
            double offset = allowCount - idx;
            if (offset < 0.0) continue;
            if (offset >= window)
            {
                p.drawLine(from[i], to[j]);
                continue;
            }
            double t = offset / window;
            double prog = easeInOut(t);
            QPointF end = from[i] + prog * (to[j] - from[i]);
            p.drawLine(from[i], end);
        }
    }
}

void NeuronLogEdit::drawHighlightPoints(QPainter &p, const QVector<QPointF> &input, const QVector<QPointF> &hidden, const QVector<QPointF> &output)
{
    p.save();
    for (const auto &h : m_highlights)
    {
        QPointF s, e;
        bool valid = true;
        switch (h.fromLayer)
        {
        case InputLayer:
            valid = h.fromIndex >= 0 && h.fromIndex < input.size();
            if (valid) s = input[h.fromIndex];
            break;
        case HiddenLayer:
            valid = h.fromIndex >= 0 && h.fromIndex < hidden.size();
            if (valid) s = hidden[h.fromIndex];
            break;
        default: valid = false; break;
        }
        switch (h.toLayer)
        {
        case HiddenLayer:
            valid = valid && h.toIndex >= 0 && h.toIndex < hidden.size();
            if (valid) e = hidden[h.toIndex];
            break;
        case OutputLayer:
            valid = valid && h.toIndex >= 0 && h.toIndex < output.size();
            if (valid) e = output[h.toIndex];
            break;
        default: valid = false; break;
        }
        if (!valid) continue;
        QPointF pos = s + h.progress * (e - s);
        QRadialGradient grad(pos, h.size);
        grad.setColorAt(0, QColor(255, 255, 255, 255 * h.fadeFactor / 100));
        grad.setColorAt(1, QColor(255, 255, 255, 0));
        p.setBrush(grad);
        p.setPen(Qt::NoPen);
        p.drawEllipse(pos, h.size, h.size);
    }
    p.restore();
}

double NeuronLogEdit::easeInOut(double t)
{
    if (t < 0.5) return 2 * t * t;
    return -1 + (4 - 2 * t) * t;
}

double NeuronLogEdit::getDepthFactor(double z)
{
    double minZ = -1000.0, maxZ = 1000.0;
    double norm = (z - minZ) / (maxZ - minZ);
    return 1.5 - norm;
}

void NeuronLogEdit::drawNeuron(QPainter *p, const QPointF &center, qreal radius, const QColor &baseColor)
{
    uint t = m_clock.elapsed() / 10;
    QColor dynamicColor = baseColor;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // Soft drop shadow
    qreal shadowOffset = radius * 0.35;
    QRadialGradient shadowGrad(center + QPointF(shadowOffset, shadowOffset), radius * 1.6);
    shadowGrad.setColorAt(0, QColor(0, 0, 0, 110));
    shadowGrad.setColorAt(1, QColor(0, 0, 0, 0));
    p->setBrush(shadowGrad);
    p->setPen(Qt::NoPen);
    p->drawEllipse(center + QPointF(shadowOffset, shadowOffset), radius * 1.3, radius * 1.3);

    // Membrane
    QRadialGradient bodyGrad(center, radius * 1.2);
    bodyGrad.setColorAt(0.0, dynamicColor.lighter(180));
    bodyGrad.setColorAt(0.65, dynamicColor);
    bodyGrad.setColorAt(1.0, QColor(10, 20, 10, 220));
    p->setBrush(bodyGrad);
    p->setPen(Qt::NoPen);
    QPainterPath membranePath;
    QPolygonF poly;
    for (int i = 0; i < 38; ++i)
    {
        double ang = i * 2 * M_PI / 38;
        double offs = qSin(ang * 3 + t / 50.0) * radius * 0.05;
        poly << QPointF(center.x() + qCos(ang) * (radius + offs), center.y() + qSin(ang) * (radius + offs));
    }
    membranePath.addPolygon(poly);
    p->drawPath(membranePath);

    // Nucleus
    qreal nucleusRad = radius * 0.45;
    QRadialGradient nucGrad(center, nucleusRad);
    nucGrad.setColorAt(0, QColor(0, 255, 150, 220));
    nucGrad.setColorAt(0.5, QColor(0, 180, 80, 180));
    nucGrad.setColorAt(1, QColor(0, 100, 50, 120));
    p->setBrush(nucGrad);
    p->drawEllipse(center, nucleusRad, nucleusRad);

    p->restore();
}

void NeuronLogEdit::startPulsation(int layer, int index)
{
    for (auto &p : m_pulsations)
    {
        if (p.layer == layer && p.index == index)
        {
            p.startTime = m_clock.elapsed();
            return;
        }
    }
    m_pulsations.append({layer, index, m_clock.elapsed(), 600});
}

double NeuronLogEdit::getPulsationScale(int layer, int index)
{
    qint64 now = m_clock.elapsed();
    for (const auto &p : m_pulsations)
    {
        if (p.layer == layer && p.index == index)
        {
            double prog = (double)(now - p.startTime) / p.duration;
            if (prog >= 0.0 && prog <= 1.0)
                return 1.0 + 0.4 * sin(prog * M_PI);
        }
    }
    return 1.0;
}

int NeuronLogEdit::getConnectionIndex(LayerType fromLayer, int fromIndex, LayerType toLayer, int toIndex) const
{
    if (fromLayer == InputLayer && toLayer == HiddenLayer)
        return fromIndex * m_hiddenNodes.size() + toIndex;
    if (fromLayer == HiddenLayer && toLayer == OutputLayer)
    {
        int ihTotal = m_inputNodes.size() * m_hiddenNodes.size();
        return ihTotal + fromIndex * m_outputNodes.size() + toIndex;
    }
    return -1;
}
