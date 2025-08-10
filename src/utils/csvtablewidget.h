#ifndef CSVTABLEWIDGET_H
#define CSVTABLEWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QPainterPath>
#include <QTextEdit>
#include <QMessageBox>
#include <QTimer>
#include <cmath>
#include <QMouseEvent>
#include <QTime>
#include <QThread>
#include <QElapsedTimer>
#include <algorithm>
#include <QRandomGenerator>  // 新增：用于随机数生成
#include <QtMath>
// 3D向量结构
struct Vector3D {
    double x, y, z;
    
    Vector3D(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    
    // 向量旋转
    Vector3D rotatedX(double angle) const {
        double rad = angle * M_PI / 180.0;
        return Vector3D(
            x,
            y * cos(rad) - z * sin(rad),
            y * sin(rad) + z * cos(rad)
        );
    }
    
    Vector3D rotatedY(double angle) const {
        double rad = angle * M_PI / 180.0;
        return Vector3D(
            x * cos(rad) + z * sin(rad),
            y,
            -x * sin(rad) + z * cos(rad)
        );
    }
    
    Vector3D rotatedZ(double angle) const {
        double rad = angle * M_PI / 180.0;
        return Vector3D(
            x * cos(rad) - y * sin(rad),
            x * sin(rad) + y * cos(rad),
            z
        );
    }
};

/* 能识别数字的代理模型 */
class CustomProxyModel : public QSortFilterProxyModel
{
public:
    explicit CustomProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setFilterKeyColumn(-1);          // 全局过滤
    }

protected:
    /* 数值排序：能转 double 就按数值比，否则按字符串 */
    bool lessThan(const QModelIndex &left,
                  const QModelIndex &right) const override
    {
        QVariant l = left.data(Qt::DisplayRole);
        QVariant r = right.data(Qt::DisplayRole);

        bool okL = false, okR = false;
        double dL = l.toDouble(&okL);
        double dR = r.toDouble(&okR);

        if (okL && okR)                 // 都是数字
            return dL < dR;
        return l.toString() < r.toString();
    }

    /* 简化搜索：使用字符串包含匹配（不区分大小写） */
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override
    {
        QString filter = filterRegExp().pattern().trimmed(); // 去除首尾空格
        if (filter.isEmpty())
            return true;

        // 按空格拆分条件，过滤空字符串（处理连续空格的情况）
        QStringList conditions = filter.split(' ', Qt::SkipEmptyParts);
        if (conditions.isEmpty())
            return true;

        // 检查当前行是否满足所有条件
        for (const QString &condition : conditions) {
            bool conditionMet = false; // 标记当前条件是否被满足
            // 遍历所有列，检查是否有列包含该条件
            for (int c = 0; c < sourceModel()->columnCount(); ++c) {
                QModelIndex idx = sourceModel()->index(source_row, c, source_parent);
                QString txt = idx.data(Qt::DisplayRole).toString();
                if (txt.contains(condition, Qt::CaseInsensitive)) {
                    conditionMet = true;
                    break; // 该条件满足，检查下一个条件
                }
            }
            // 若有任何一个条件不满足，直接排除该行
            if (!conditionMet)
                return false;
        }

        // 所有条件都满足，保留该行
        return true;
    }
};

/* ---------------------------------------------------- */

// 极客绿色风格委托
class GeekDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
           const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        painter->save();

        // 绘制背景（降低透明度）
        QColor bgColor;
        if (opt.state & QStyle::State_Selected) {
            bgColor = QColor(0, 100, 0, 150); // 选中状态 - 半透明深绿色
        } else if (opt.state & QStyle::State_MouseOver) {
            bgColor = QColor(40, 60, 40, 100);   // 悬停状态 - 半透明深灰绿
        } else {
            bgColor = QColor(20, 35, 20, 80);    // 半透明更深绿灰
        }

        // 绘制矩形
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawRect(opt.rect);

        // 绘制边框（降低透明度）
        QColor borderColor = opt.state & QStyle::State_Selected ? 
                            QColor(0, 255, 100, 150) : QColor(80, 120, 80, 100);
        painter->setPen(QPen(borderColor, 1));
        painter->drawRect(opt.rect.adjusted(0, 0, -1, -1));

        // 绘制文本
        QRect textRect = opt.rect.adjusted(6, 0, -6, 0);
        painter->setPen(opt.state & QStyle::State_Selected ? 
                    QColor(0, 255, 150) : QColor(180, 255, 180));
        QFont f = opt.font;
        f.setPointSize(f.pointSize() - 1);
        painter->setFont(f);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, opt.text);

        painter->restore();
    }
    
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(24); // 更紧凑的行高
        return size;
    }
};

/* ---------------------------------------------------- */

class CsvTableWidget : public QWidget
{
    Q_OBJECT
public:
    // 定义神经元层类型
    enum LayerType {
        InputLayer,
        HiddenLayer,
        OutputLayer
    };

    // 定义连线亮点结构
    struct ConnectionHighlight {
        LayerType fromLayer;  // 起始层
        int fromIndex;        // 起始节点索引
        LayerType toLayer;    // 目标层
        int toIndex;          // 目标节点索引
        double progress;      // 位置进度 (0~1)
        int direction;        // 移动方向 (1:正向, -1:反向)
        double speed;         // 移动速度 (随机)
        int size;             // 亮点大小 (随机)
        int fadeFactor;       // 透明度因子 (随机)
    };

    // 神经元脉冲动画状态
    struct Pulsation {
        int layer;          // 神经元所在的层 (0: input, 1: hidden, 2: output)
        int index;          // 神经元在该层中的索引
        qint64 startTime;   // 动画开始的时间戳 (ms)
        int duration;       // 动画总时长 (ms)
    };

    explicit CsvTableWidget(QWidget *parent = nullptr)
        : QWidget(parent),
          m_selectedRow(-1),
          m_rotX(30), m_rotY(45), m_rotZ(0),
          m_animationSpeed(0.2),
          m_connectionStep(0.0),
          m_direction(1),
          m_connAnimStep(0.05),
          m_parallelLines(12),
          m_maxHighlights(30)  // 最大亮点数量限制
    {
        // 初始化随机数生成器
        m_randGen.seed(QDateTime::currentMSecsSinceEpoch());
    }

    void init_all(){
        // 加载QSS样式
        loadStyleSheet(":/QSS/eva_green.qss");

        /* 基础 UI */
        m_view = new QTableView(this);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        
        m_srcModel = new QStandardItemModel(this);
        m_proxyModel = new CustomProxyModel(this);
        m_proxyModel->setSourceModel(m_srcModel);
        m_view->setModel(m_proxyModel);
        m_view->setSortingEnabled(true);
        m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        m_view->setItemDelegate(new GeekDelegate(this));
        m_view->setSelectionMode(QAbstractItemView::SingleSelection);
        m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_view->setShowGrid(false);
        
        m_view->verticalHeader()->setVisible(false);
        m_view->verticalHeader()->setDefaultSectionSize(26);

        /* 搜索框 */
        m_filterEdit = new QLineEdit;
        m_filterEdit->setPlaceholderText(tr("SEARCH..."));
        connect(m_filterEdit, &QLineEdit::textChanged,
                [this](const QString &txt){
                    m_proxyModel->setFilterFixedString(txt);
                });

        /* 信息展示框 */
        m_infoDisplay = new QTextEdit;
        m_infoDisplay->setReadOnly(true);
        m_infoDisplay->setPlaceholderText(tr("Select an item to view details..."));
        m_infoDisplay->setMinimumHeight(80);

        /* 下载按钮 */
        m_downloadBtn = new QPushButton(tr("DOWNLOAD"));
        m_downloadBtn->setEnabled(false);
        connect(m_downloadBtn, &QPushButton::clicked, this, &CsvTableWidget::openInBrowser);

        // 动画控制按钮
        m_toggleAnimBtn = new QPushButton(tr("PAUSE ANIM"));
        connect(m_toggleAnimBtn, &QPushButton::clicked, this, &CsvTableWidget::toggleAnimation);

        // 底部控制区布局
        QWidget *bottomWidget = new QWidget;
        bottomWidget->setStyleSheet("background: transparent;");
        QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(8);
        bottomLayout->addWidget(m_infoDisplay, 3);
        bottomLayout->addWidget(m_downloadBtn, 1);

        // 主布局
        auto *vlay = new QVBoxLayout(this);
        vlay->setSpacing(8);
        vlay->setContentsMargins(8, 8, 8, 8);
        
        QWidget *searchContainer = new QWidget;
        searchContainer->setStyleSheet("background: transparent;");
        QHBoxLayout *hLayout = new QHBoxLayout(searchContainer);
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->addWidget(m_filterEdit);
        
        vlay->addWidget(searchContainer);
        vlay->addWidget(m_view, 1);
        vlay->addWidget(bottomWidget);
        
        connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &CsvTableWidget::onSelectionChanged);
        // connect(m_view, &QTableView::clicked, this, &CsvTableWidget::handleCellClick);

        initNeuralNetwork();
        
        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(30);
        connect(m_animTimer, &QTimer::timeout, this, &CsvTableWidget::updateRotation);
        m_animTimer->start();

        m_pauseTimer = new QTimer(this);
        m_pauseTimer->setSingleShot(true);
        connect(m_pauseTimer, &QTimer::timeout, this, &CsvTableWidget::onPauseFinished);
        
        const double totalConn = m_inputNodes.size() * m_hiddenNodes.size()
                            + m_hiddenNodes.size() * m_outputNodes.size();
        
        m_connTimer = new QTimer(this);
        m_connTimer->setInterval(30);
        connect(m_connTimer, &QTimer::timeout, this, [=] {
            double oldStep = m_connectionStep;
            m_connectionStep += m_connAnimStep;
            double newStep = m_connectionStep;

            // 触发脉冲动画
            int ih_idx = 0;
            for (int i = 0; i < m_inputNodes.size(); ++i) {
                for (int j = 0; j < m_hiddenNodes.size(); ++j, ++ih_idx) {
                // departure 触发：加 ε
                double departureTime = ih_idx + kPulseEps;
                if (oldStep <  departureTime && newStep >= departureTime)
                    startPulsation(0, i);

                // arrival 触发：减 ε
                double arrivalTime  = ih_idx + m_parallelLines - kPulseEps;
                if (oldStep <  arrivalTime && newStep >= arrivalTime)
                    startPulsation(1, j);
                }
            }

            // 检查 Hidden -> Output 层的连接
            double maxIH = m_inputNodes.size() * m_hiddenNodes.size();
            int ho_idx_base = static_cast<int>(maxIH);
            int ho_idx = 0;
            for (int i = 0; i < m_hiddenNodes.size(); ++i) {
                for (int j = 0; j < m_outputNodes.size(); ++j, ++ho_idx) {
                    double departureTime = ho_idx_base + ho_idx + kPulseEps;
                    if (oldStep <  departureTime && newStep >= departureTime)
                        startPulsation(1, i);

                    double arrivalTime   = departureTime + m_parallelLines - 2*kPulseEps;
                    if (oldStep <  arrivalTime && newStep >= arrivalTime)
                        startPulsation(2, j);
                }
            }

            const double restartThreshold = totalConn - 1 + m_parallelLines;
            if (m_connectionStep >= restartThreshold) {
                m_connTimer->stop();
                m_pauseTimer->start(10000);
            }
        });
        m_connTimer->start();

        // 脉冲动画计时器
        m_pulsationTimer = new QTimer(this);
        m_pulsationTimer->setInterval(16); // ~60 FPS
        connect(m_pulsationTimer, &QTimer::timeout, this, &CsvTableWidget::updatePulsations);
        m_pulsationTimer->start();
        m_animationClock.start();

        // 新增：亮点生成计时器
        m_highlightTimer = new QTimer(this);
        m_highlightTimer->setInterval(200); // 每200ms尝试生成新亮点
        connect(m_highlightTimer, &QTimer::timeout, this, &CsvTableWidget::spawnRandomHighlight);
        m_highlightTimer->start();
    }

    void openCsv(const QString &path)
    {
        if (path.isEmpty()) return;
        if(!is_init)
        {
            is_init = true;
            init_all();
        }
        
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open CSV file:" << path;
            return;
        }

        m_srcModel->clear();
        QTextStream in(&f);
        bool first = true;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.isEmpty()) continue;
            QStringList parts = line.split(',');
            if (first) {
                m_srcModel->setHorizontalHeaderLabels(parts);
                first = false;
                continue;
            }
            QList<QStandardItem*> items;
            for (const QString &s : parts) {
                QStandardItem* item = new QStandardItem(s);
                item->setEditable(false);
                item->setForeground(QBrush(QColor(180, 255, 200)));
                items << item;
            }
            m_srcModel->appendRow(items);
        }
        f.close();
        
        for (int col = 0; col < m_srcModel->columnCount(); ++col) {
            m_view->resizeColumnToContents(col);
            int width = m_view->columnWidth(col);
            m_view->setColumnWidth(col, qMin(width + 10, 200));
        }
        if (m_srcModel->columnCount() > 0) {
            m_view->sortByColumn(0, Qt::AscendingOrder);
        }
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRadialGradient gradient(rect().center(), width() / 1.5);
        gradient.setColorAt(0, QColor(20, 35, 20));
        gradient.setColorAt(1, QColor(10, 15, 10));
        painter.fillRect(rect(), gradient);

        drawHexGrid(painter, 40, QColor(0, 255, 0, 40)); // 亮绿色，半透明

        QVector<QPointF> projectedInput, projectedHidden, projectedOutput;
        projectNodes(projectedInput, projectedHidden, projectedOutput);

        QVector<QPointF> unsortedInput = projectedInput;
        QVector<QPointF> unsortedHidden = projectedHidden;
        QVector<QPointF> unsortedOutput = projectedOutput;

        QVector<Vector3D> sortedRotatedInput, sortedRotatedHidden, sortedRotatedOutput;
        QVector<int> inputIndices = sortNodesByDepth(projectedInput, m_inputNodes, sortedRotatedInput);
        QVector<int> hiddenIndices = sortNodesByDepth(projectedHidden, m_hiddenNodes, sortedRotatedHidden);
        QVector<int> outputIndices = sortNodesByDepth(projectedOutput, m_outputNodes, sortedRotatedOutput);

        double maxIH = m_inputNodes.size() * m_hiddenNodes.size();
        double current = m_connectionStep;
        double ihCount = current;
        double hoCount = current - maxIH;

        // 绘制连线（先绘制线，再绘制亮点）
        painter.setPen(QPen(QColor(0, 200, 100, 80), 0.8));
        drawConnections(painter, unsortedInput, unsortedHidden, ihCount);
        drawConnections(painter, unsortedHidden, unsortedOutput, hoCount);

        // 新增：绘制连线亮点
        drawHighlightPoints(painter, unsortedInput, unsortedHidden, unsortedOutput);

        // 绘制神经元
        for (int i = 0; i < projectedInput.size(); ++i) {
            int originalIndex = inputIndices[i];
            double scale = getPulsationScale(0, originalIndex);
            double depthFactor = getDepthFactor(sortedRotatedInput[i].z);
            drawNeuron(&painter, projectedInput[i], 12 * depthFactor * scale, 
                    QColor(0, 255, 100, 200 * depthFactor), m_rotX, m_rotY, m_rotZ);
        }

        for (int i = 0; i < projectedHidden.size(); ++i) {
            int originalIndex = hiddenIndices[i];
            double scale = getPulsationScale(1, originalIndex);
            double depthFactor = getDepthFactor(sortedRotatedHidden[i].z);
            drawNeuron(&painter, projectedHidden[i], 12 * depthFactor * scale, 
                    QColor(0, 255, 100, 200 * depthFactor), m_rotX, m_rotY, m_rotZ);
        }

        for (int i = 0; i < projectedOutput.size(); ++i) {
            int originalIndex = outputIndices[i];
            double scale = getPulsationScale(2, originalIndex);
            double depthFactor = getDepthFactor(sortedRotatedOutput[i].z);
            drawNeuron(&painter, projectedOutput[i], 12 * depthFactor * scale, 
                    QColor(0, 255, 100, 200 * depthFactor), m_rotX, m_rotY, m_rotZ);
        }
    }
    
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_lastMousePos = event->pos();
            m_isDragging = true;
        }
    }
    
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            m_isDragging = false;
        }
    }
    
    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_isDragging) {
            QPoint delta = event->pos() - m_lastMousePos;
            m_rotY += delta.x() * 0.5;
            m_rotX += delta.y() * 0.5;
            m_lastMousePos = event->pos();
            update();
        }
    }
    
    void wheelEvent(QWheelEvent *event) override {
        // if (event->modifiers() & Qt::ControlModifier) {
        //     int delta = event->angleDelta().y();
        //     double zoomDelta = delta > 0 ? 1.1 : 1.0 / 1.1;
        //     m_zoomFactor *= zoomDelta;
        //     m_zoomFactor = qBound(0.3, m_zoomFactor, 3.0);
        // } else {
        //     int delta = event->angleDelta().y();
        //     m_animationSpeed += delta * 0.001;
        //     m_animationSpeed = qBound(0.0, m_animationSpeed, 5.0);
        // }
        // update();
    }

private:
    void initNeuralNetwork() {
        const int inputCount = 8;
        const int hiddenCount = 12;
        const int outputCount = 8;
        
        const double layerDepth = 150;
        const double startZ = -layerDepth;
        const double inputSpacing = 80.0;
        const double hiddenSpacing = 65.0;
        const double outputSpacing = 80.0;

        for (int i = 0; i < inputCount; ++i) {
            double yPos = (i - (inputCount - 1) / 2.0) * inputSpacing;
            m_inputNodes.append(Vector3D(-300, yPos, startZ));
        }
        
        for (int i = 0; i < hiddenCount; ++i) {
            double yPos = (i - (hiddenCount - 1) / 2.0) * hiddenSpacing;
            m_hiddenNodes.append(Vector3D(0, yPos, startZ + layerDepth));
        }
        
        for (int i = 0; i < outputCount; ++i) {
            double yPos = (i - (outputCount - 1) / 2.0) * outputSpacing;
            m_outputNodes.append(Vector3D(300, yPos, startZ + 2 * layerDepth));
        }
    }
    
    void projectNodes(QVector<QPointF>& input, QVector<QPointF>& hidden, QVector<QPointF>& output) {
        input.clear();
        hidden.clear();
        output.clear();
        
        QPoint center(width() / 2, height() / 2);
        const double scale = 1;
        
        for (const auto& node : m_inputNodes) {
            Vector3D rotated = node.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
            input.append(projectPoint(rotated, center, scale));
        }
        
        for (const auto& node : m_hiddenNodes) {
            Vector3D rotated = node.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
            hidden.append(projectPoint(rotated, center, scale));
        }
        
        for (const auto& node : m_outputNodes) {
            Vector3D rotated = node.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
            output.append(projectPoint(rotated, center, scale));
        }
    }
    
    QPointF projectPoint(const Vector3D& point, const QPoint& center, double scale) {
        double zOffset = 800;
        double factor = zOffset / (zOffset + point.z);
        
        return QPointF(
            center.x() + point.x * factor * scale * m_zoomFactor,
            center.y() - point.y * factor * scale * m_zoomFactor
        );
    }

    void drawHexGrid(QPainter &painter, qreal hexSize, const QColor &color)
    {
        painter.setPen(QPen(color, 0.7));

        const qreal hexHeight = sqrt(3.0) * hexSize;
        const qreal hexWidth = 2 * hexSize;
        const qreal vertDist = hexHeight;
        const qreal horizDist = hexWidth * 0.75;

        const int extraMargin = static_cast<int>(hexSize * 2);

        for (int row = -2; (row * vertDist) < height() + extraMargin; ++row) {
            for (int col = -2; (col * horizDist) < width() + extraMargin; ++col) {
                qreal cx = col * horizDist;
                qreal cy = row * vertDist;

                if (abs(col) % 2 == 1) {
                    cy += vertDist / 2.0;
                }

                QPolygonF hexagon;
                for (int i = 0; i < 6; ++i) {
                    qreal angle_rad = 60.0 * i * M_PI / 180.0;
                    hexagon << QPointF(cx + hexSize * cos(angle_rad),
                                       cy + hexSize * sin(angle_rad));
                }
                painter.drawPolygon(hexagon);
            }
        }
    }

    QVector<int> sortNodesByDepth(QVector<QPointF>& projected, const QVector<Vector3D>& baseNodes, QVector<Vector3D>& sortedRotated) {
        int n = projected.size();
        QVector<int> indices(n);
        for (int i = 0; i < n; ++i) indices[i] = i;
        
        QVector<Vector3D> rotated(n);
        for (int i = 0; i < n; ++i) {
            rotated[i] = baseNodes[i].rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
        }
        
        QVector<int> sort_indices(n);
        for(int i=0; i<n; ++i) sort_indices[i] = i;

        std::sort(sort_indices.begin(), sort_indices.end(), [&](int a, int b) {
            return rotated[a].z < rotated[b].z;
        });
        
        QVector<QPointF> sortedProjected(n);
        sortedRotated.resize(n);
        QVector<int> originalIndices(n);
        for (int i = 0; i < n; ++i) {
            int original_idx = sort_indices[i];
            sortedProjected[i] = projected[original_idx];
            sortedRotated[i] = rotated[original_idx];
            originalIndices[i] = original_idx;
        }
        
        projected = sortedProjected;
        return originalIndices;
    }
    
    void drawConnections(QPainter &painter,
                         const QVector<QPointF> &from,
                         const QVector<QPointF> &to,
                         double allowCount)
    {
        if (allowCount <= 0) return;

        const int fromSize = from.size();
        const int toSize   = to.size();
        const double window = m_parallelLines;

        int idx = 0;
        for (int i = 0; i < fromSize; ++i) {
            for (int j = 0; j < toSize; ++j, ++idx) {

                double offset = allowCount - idx;

                if (offset < 0.0)
                    continue;

                if (offset >= window) {
                    painter.drawLine(from[i], to[j]);
                    continue;
                }

                double t = offset / window;
                double prog = easeInOut(t);
                QPointF end = from[i] + prog * (to[j] - from[i]);
                painter.drawLine(from[i], end);
            }
        }
    }

    // 新增：绘制连线亮点
    void drawHighlightPoints(QPainter &painter,
                           const QVector<QPointF> &input,
                           const QVector<QPointF> &hidden,
                           const QVector<QPointF> &output)
    {
        painter.save();
        
        for (const auto &highlight : m_highlights) {
            // 根据亮点的层信息获取起点和终点坐标
            QPointF start, end;
            bool valid = true;

            switch (highlight.fromLayer) {
                case InputLayer:
                    if (highlight.fromIndex >= 0 && highlight.fromIndex < input.size())
                        start = input[highlight.fromIndex];
                    else valid = false;
                    break;
                case HiddenLayer:
                    if (highlight.fromIndex >= 0 && highlight.fromIndex < hidden.size())
                        start = hidden[highlight.fromIndex];
                    else valid = false;
                    break;
                default: valid = false;
            }

            switch (highlight.toLayer) {
                case HiddenLayer:
                    if (highlight.toIndex >= 0 && highlight.toIndex < hidden.size())
                        end = hidden[highlight.toIndex];
                    else valid = false;
                    break;
                case OutputLayer:
                    if (highlight.toIndex >= 0 && highlight.toIndex < output.size())
                        end = output[highlight.toIndex];
                    else valid = false;
                    break;
                default: valid = false;
            }

            if (!valid) continue;

            // 计算亮点当前位置
            QPointF pos = start + highlight.progress * (end - start);

            // 绘制亮点（带渐变的白点）
            QRadialGradient grad(pos, highlight.size);
            grad.setColorAt(0, QColor(255, 255, 255, 255 * highlight.fadeFactor / 100));
            grad.setColorAt(1, QColor(255, 255, 255, 0));
            painter.setBrush(grad);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(pos, highlight.size, highlight.size);
        }
        
        painter.restore();
    }

    double easeInOut(double t) {
        if (t < 0.5) {
            return 2 * t * t;
        } else {
            return -1 + (4 - 2 * t) * t;
        }
    }
    
    double getDepthFactor(double z) {
        double minZ = -1000;
        double maxZ = 1000;
        double normalized = (z - minZ) / (maxZ - minZ);
        return 1.5 - normalized;
    }

    void drawNeuron(QPainter *painter, const QPointF &center, qreal radius,
                const QColor &baseColor, double /*rotX*/, double /*rotY*/, double /*rotZ*/)
    {
        uint t = m_animationClock.elapsed() / 10;
        QColor dynamicColor = baseColor;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // ===================== 投影（加压迫感） =====================
        qreal shadowOffset = radius * 0.35;
        QRadialGradient shadowGrad(center + QPointF(shadowOffset, shadowOffset), radius * 1.6);
        shadowGrad.setColorAt(0, QColor(0, 0, 0, 110));
        shadowGrad.setColorAt(1, QColor(0, 0, 0, 0));
        painter->setBrush(shadowGrad);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(center + QPointF(shadowOffset, shadowOffset),
                            radius * 1.3, radius * 1.3);

        // ===================== 外膜 =====================
        QRadialGradient bodyGrad(center, radius * 1.2);
        bodyGrad.setColorAt(0.0, dynamicColor.lighter(180));  // 中心亮
        bodyGrad.setColorAt(0.65, dynamicColor);
        bodyGrad.setColorAt(1.0, QColor(10, 20, 10, 220));    // 外层更暗
        painter->setBrush(bodyGrad);
        painter->setPen(Qt::NoPen);

        // 轻微膜抖动
        QPainterPath membranePath;
        QPolygonF poly;
        for (int i = 0; i < 40; ++i) {
            double ang = i * 2 * M_PI / 40;
            double offs = qSin(ang * 3 + t / 50.0) * radius * 0.05;
            poly << QPointF(center.x() + qCos(ang) * (radius + offs),
                            center.y() + qSin(ang) * (radius + offs));
        }
        membranePath.addPolygon(poly);
        painter->drawPath(membranePath);

        // ===================== 内核 =====================
        qreal nucleusRad = radius * 0.45;
        QRadialGradient nucGrad(center, nucleusRad);
        nucGrad.setColorAt(0, QColor(0, 255, 150, 220));
        nucGrad.setColorAt(0.5, QColor(0, 180, 80, 180));
        nucGrad.setColorAt(1, QColor(0, 100, 50, 120));
        painter->setBrush(nucGrad);
        painter->drawEllipse(center, nucleusRad, nucleusRad);

        painter->restore();
    }


    void loadStyleSheet(const QString &fileName)
    {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly)) {
            QString styleSheet = QLatin1String(file.readAll());
            setStyleSheet(styleSheet);
            file.close();
        } else {
            qWarning() << "Failed to load style sheet:" << fileName;
            qWarning() << "Error:" << file.errorString();
        }
    }

    void startPulsation(int layer, int index) {
        for (auto& p : m_activePulsations) {
            if (p.layer == layer && p.index == index) {
                p.startTime = m_animationClock.elapsed();
                return;
            }
        }
        m_activePulsations.append({layer, index, m_animationClock.elapsed(), 600});
    }

    double getPulsationScale(int layer, int index) {
        qint64 currentTime = m_animationClock.elapsed();
        for (const auto& p : m_activePulsations) {
            if (p.layer == layer && p.index == index) {
                double progress = (double)(currentTime - p.startTime) / p.duration;
                if (progress >= 0.0 && progress <= 1.0) {
                    return 1.0 + 0.4 * sin(progress * M_PI);
                }
            }
        }
        return 1.0;
    }

    // 新增：随机生成连线亮点
    void spawnRandomHighlight() {
    // 如果亮点数量已达上限，不再生成
    if (m_highlightCount >= m_maxHighlights) return;

    // 50%概率选择输入-隐藏层或隐藏-输出层
    bool isInputToHidden = m_randGen.bounded(2) == 0;

    ConnectionHighlight highlight;
    int connIndex = -1; // 连线索引

    if (isInputToHidden) {
        // 输入层到隐藏层的连线
        highlight.fromLayer = InputLayer;
        highlight.toLayer = HiddenLayer;
        highlight.fromIndex = m_randGen.bounded(m_inputNodes.size());
        highlight.toIndex = m_randGen.bounded(m_hiddenNodes.size());
        connIndex = getConnectionIndex(InputLayer, highlight.fromIndex, HiddenLayer, highlight.toIndex);
    } else {
        // 隐藏层到输出层的连线
        highlight.fromLayer = HiddenLayer;
        highlight.toLayer = OutputLayer;
        highlight.fromIndex = m_randGen.bounded(m_hiddenNodes.size());
        highlight.toIndex = m_randGen.bounded(m_outputNodes.size());
        connIndex = getConnectionIndex(HiddenLayer, highlight.fromIndex, OutputLayer, highlight.toIndex);
    }

    // 关键判断：仅当连线已建立（当前进度 >= 连线索引）时才生成亮点
    if (connIndex == -1 || m_connectionStep < connIndex) {
        return; // 连线未建立，不生成亮点
    }

    // 随机初始位置（0~1）
    highlight.progress = m_randGen.bounded(100) / 100.0;
    // 随机方向（50%正向，50%反向）
    highlight.direction = m_randGen.bounded(2) == 0 ? 1 : -1;
    // 随机速度（0.002~0.01）
    highlight.speed = 0.002 + (m_randGen.bounded(80) / 10000.0);
    // 随机大小（1~3像素）
    highlight.size = 1 + m_randGen.bounded(3);
    // 随机透明度（60%~100%）
    highlight.fadeFactor = 60 + m_randGen.bounded(41);

    m_highlights.append(highlight);
    m_highlightCount++;
}

private slots:
    void updateRotation() {
        m_rotY += 0.5 * m_animationSpeed;
        m_rotX += 0.2 * m_animationSpeed;
        m_rotZ += 0.1 * m_animationSpeed;
        
        m_rotX = fmod(m_rotX, 360);
        m_rotY = fmod(m_rotY, 360);
        m_rotZ = fmod(m_rotZ, 360);
    }
    
    void toggleAnimation() {
        if (m_animTimer->isActive()) {
            m_animTimer->stop();
            m_toggleAnimBtn->setText(tr("PLAY ANIM"));
            m_highlightTimer->stop();  // 暂停亮点生成
        } else {
            m_animTimer->start();
            m_toggleAnimBtn->setText(tr("PAUSE ANIM"));
            m_highlightTimer->start(); // 恢复亮点生成
        }
    }

    // void handleCellClick(const QModelIndex &index)
    // {
    //     if (index.isValid()) {
    //         onSelectionChanged(m_view->selectionModel()->selection(), QItemSelection());
    //     }
    // }

    void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
    {
        Q_UNUSED(deselected);
        QModelIndexList selectedIndexes = selected.indexes();
        if (selectedIndexes.isEmpty()) {
            m_infoDisplay->clear();
            m_infoDisplay->setPlaceholderText(tr("Select an item to view details..."));
            m_downloadBtn->setEnabled(false);
            m_selectedRow = -1;
            return;
        }

        QModelIndex proxyIndex = selectedIndexes.first();
        QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
        if (!srcIndex.isValid()) return;

        m_selectedRow = srcIndex.row();

        QString info;
        int columnCount = m_srcModel->columnCount();
        for (int col = 0; col < columnCount; ++col) {
            QString header = m_srcModel->headerData(col, Qt::Horizontal).toString();
            QString value = m_srcModel->data(m_srcModel->index(m_selectedRow, col)).toString();
            value = value.replace("\\n", "<br>");
            info += QString("<b>%1:</b> %2<br>").arg(header).arg(value);
        }
        m_infoDisplay->setHtml(info);
        m_downloadBtn->setEnabled(true);
    }

    void openInBrowser()
    {
        if (m_selectedRow < 0) return;
        
        QString urlStr = m_srcModel->data(m_srcModel->index(m_selectedRow, 16)).toString();
        QStringList urlList = urlStr.split("\\n", Qt::SkipEmptyParts);
        
        if (urlList.isEmpty()) {
            QMessageBox::warning(this, tr("No URLs Found"), tr("Selected item does not contain any valid URLs"));
            return;
        }
        
        int validCount = 0;
        int errorCount = 0;
        
        for (const QString& singleUrl : urlList) {
            QString trimmedUrl = singleUrl.trimmed();
            
            if (!trimmedUrl.startsWith("http://", Qt::CaseInsensitive) && 
                !trimmedUrl.startsWith("https://", Qt::CaseInsensitive)) {
                errorCount++;
                continue;
            }
            
            QUrl url(trimmedUrl);
            if (!url.isValid()) {
                errorCount++;
                continue;
            }
            
            if (QDesktopServices::openUrl(url)) {
                validCount++;
            } else {
                errorCount++;
            }
        }
        
        if (validCount > 0) {
            QMessageBox::information(this, tr("Success"), 
                                tr("Successfully opened %1 URL(s) in the browser").arg(validCount));
        }
        
        if (errorCount > 0) {
            QMessageBox::warning(this, tr("Error"), 
                            tr("Failed to open %1 URL(s) due to invalid format or system issues").arg(errorCount));
        }
    }

    void onPauseFinished() {
        m_connectionStep = 0.0;
        m_connTimer->start();
    }
    
    void updatePulsations() {
        qint64 currentTime = m_animationClock.elapsed();
        bool needsUpdate = !m_activePulsations.isEmpty() || !m_highlights.isEmpty();

        // 移除已结束的脉冲动画
        auto itPulse = std::remove_if(m_activePulsations.begin(), m_activePulsations.end(),
            [&](const Pulsation& p) {
                return (currentTime - p.startTime) > p.duration;
            });
        
        if (itPulse != m_activePulsations.end()) {
            m_activePulsations.erase(itPulse, m_activePulsations.end());
        }

        // 更新亮点位置和状态，同时移除未建立连线的亮点
        QMutableListIterator<ConnectionHighlight> itHighlight(m_highlights);
        while (itHighlight.hasNext()) {
            ConnectionHighlight &h = itHighlight.next();
            
            // 计算当前亮点对应连线的索引
            int connIndex = getConnectionIndex(h.fromLayer, h.fromIndex, h.toLayer, h.toIndex);
            
            // 如果连线未建立（索引无效或进度不足），移除亮点
            if (connIndex == -1 || m_connectionStep < connIndex) {
                itHighlight.remove();
                m_highlightCount--;
                continue;
            }

            // 更新位置进度
            h.progress += h.speed * h.direction;
            
            // 边界检测：反转方向
            if (h.progress < 0.0) {
                h.progress = 0.0;
                h.direction = 1;  // 反向
            } else if (h.progress > 1.0) {
                h.progress = 1.0;
                h.direction = -1; // 正向
            }

            // 随机移除一些亮点（增加随机性）
            if (m_randGen.bounded(100) < 2) {  // 2%概率移除
                itHighlight.remove();
                m_highlightCount--;
            }
        }

        // 触发重绘
        if (m_animTimer->isActive() || m_connTimer->isActive() || needsUpdate) {
            update();
        }
    }
    // 计算连线索引（用于判断连线是否已建立）
    int getConnectionIndex(LayerType fromLayer, int fromIndex, LayerType toLayer, int toIndex) const {
        if (fromLayer == InputLayer && toLayer == HiddenLayer) {
            // 输入层 -> 隐藏层：索引 = inputIndex * 隐藏层节点数 + hiddenIndex
            return fromIndex * m_hiddenNodes.size() + toIndex;
        } else if (fromLayer == HiddenLayer && toLayer == OutputLayer) {
            // 隐藏层 -> 输出层：索引 = 输入-隐藏总连线数 + hiddenIndex * 输出层节点数 + outputIndex
            int ihTotal = m_inputNodes.size() * m_hiddenNodes.size();
            return ihTotal + fromIndex * m_outputNodes.size() + toIndex;
        }
        return -1; // 无效连线
    }
private:
    bool is_init = false;
    QStandardItemModel *m_srcModel;
    CustomProxyModel   *m_proxyModel;
    QTableView         *m_view;
    QLineEdit          *m_filterEdit;
    QTextEdit          *m_infoDisplay;
    QPushButton        *m_downloadBtn;
    QPushButton        *m_toggleAnimBtn;
    int                 m_selectedRow;
    
    QTimer* m_animTimer;
    double m_rotX, m_rotY, m_rotZ;
    double m_animationSpeed;
    bool m_isDragging = false;
    QPoint m_lastMousePos;
    
    QVector<Vector3D> m_inputNodes;
    QVector<Vector3D> m_hiddenNodes;
    QVector<Vector3D> m_outputNodes;

    QTimer *m_connTimer = nullptr;
    double m_connectionStep;
    int m_direction;
    double m_connAnimStep;
    int m_parallelLines;
    double m_zoomFactor = 1;
    QTimer *m_pauseTimer;

    // 脉冲动画相关
    QTimer* m_pulsationTimer;
    QList<Pulsation> m_activePulsations;
    QElapsedTimer m_animationClock;
    double kPulseEps = 1;

    // 新增：亮点相关
    QList<ConnectionHighlight> m_highlights;  // 存储所有亮点
    QTimer* m_highlightTimer;                 // 用于生成新亮点的计时器
    QRandomGenerator m_randGen;               // 随机数生成器
    int m_highlightCount = 0;                 // 当前亮点数量
    const int m_maxHighlights;                // 最大亮点数量
};

#endif // CSVTABLEWIDGET_H

