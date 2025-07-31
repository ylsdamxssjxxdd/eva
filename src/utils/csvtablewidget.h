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
// 3D向量结构
struct Vector3D {
    double x, y, z;
    
    Vector3D(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    
    // 向量旋转
    Vector3D rotatedX(double angle) const {
        double rad = angle * 3.1415 / 180.0;
        return Vector3D(
            x,
            y * cos(rad) - z * sin(rad),
            y * sin(rad) + z * cos(rad)
        );
    }
    
    Vector3D rotatedY(double angle) const {
        double rad = angle * 3.1415 / 180.0;
        return Vector3D(
            x * cos(rad) + z * sin(rad),
            y,
            -x * sin(rad) + z * cos(rad)
        );
    }
    
    Vector3D rotatedZ(double angle) const {
        double rad = angle * 3.1415 / 180.0;
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
    bool filterAcceptsRow(int source_row,
                          const QModelIndex &source_parent) const override
    {
        QString filter = filterRegExp().pattern();
        if (filter.isEmpty())
            return true;

        for (int c = 0; c < sourceModel()->columnCount(); ++c) {
            QModelIndex idx = sourceModel()->index(source_row, c, source_parent);
            QString txt = idx.data(Qt::DisplayRole).toString();
            if (txt.contains(filter, Qt::CaseInsensitive))  // 简单包含匹配
                return true;
        }
        return false;
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
    explicit CsvTableWidget(QWidget *parent = nullptr)
        : QWidget(parent),
          m_selectedRow(-1),
          m_rotX(30), m_rotY(45), m_rotZ(0),
          m_animationSpeed(0.2), // 旋转动画速度
          m_connectionStep(0.0), // MODIFIED: 改为 double，支持小数进度
          m_direction(1), // ADDED: 动画方向（1: 添加, -1: 移除）
          m_connAnimStep(0.05), // ADDED: 每 tick 的连接延伸步长（0.1 表示10%）
          m_parallelLines(12) // ADDED: 同时延伸的线条数（增加并行感）
    {
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
        m_view->verticalHeader()->setDefaultSectionSize(26); // 紧凑行高

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
        // bottomLayout->addWidget(m_toggleAnimBtn, 1);

        // 主布局
        auto *vlay = new QVBoxLayout(this);
        vlay->setSpacing(8);
        vlay->setContentsMargins(8, 8, 8, 8);
        
        // 搜索框容器
        QWidget *searchContainer = new QWidget;
        searchContainer->setStyleSheet("background: transparent;");
        QHBoxLayout *hLayout = new QHBoxLayout(searchContainer);
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->addWidget(m_filterEdit);
        
        vlay->addWidget(searchContainer);
        vlay->addWidget(m_view, 1);
        vlay->addWidget(bottomWidget);
        
        // 监听选择变化
        connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &CsvTableWidget::onSelectionChanged);
        connect(m_view, &QTableView::clicked, this, &CsvTableWidget::handleCellClick);

        // 初始化神经网络节点的3D位置
        initNeuralNetwork();
        
        // 设置动画定时器 (旋转)
        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(30); // 旋转动画速率
        connect(m_animTimer, &QTimer::timeout, this, &CsvTableWidget::updateRotation);
        m_animTimer->start(); // 启动动画

        m_pauseTimer = new QTimer(this);
        m_pauseTimer->setSingleShot(true); // 只触发一次
        connect(m_pauseTimer, &QTimer::timeout, this, &CsvTableWidget::onPauseFinished);
        // 总线数
        const double totalConn = m_inputNodes.size() * m_hiddenNodes.size()
                            + m_hiddenNodes.size() * m_outputNodes.size();
        // ② 连接计时器，让它以 30ms 的频率递增/递减步数（MODIFIED: 支持延伸动画）
        m_connTimer = new QTimer(this);
        m_connTimer->setInterval(30);           // 帧率
        connect(m_connTimer, &QTimer::timeout, this, [=] {
            m_connectionStep += m_connAnimStep; // 递增动画进度
            // 计算动画结束阈值（总连线数 + 并行线条数）
            const double restartThreshold = totalConn - 1 + m_parallelLines;
            if (m_connectionStep >= restartThreshold) {
                // 动画结束：停止连接动画，启动10秒暂停
                m_connTimer->stop();
                m_pauseTimer->start(10000); // 10000毫秒 = 10秒
            } else {
                // 动画运行中：继续更新画面
                update();
            }
        });
        m_connTimer->start();

    }

    void openCsv(const QString &path)
    {
        if (path.isEmpty()) return;

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
        
        // 设置紧凑列宽
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

        // 绘制深色背景
        painter.fillRect(rect(), QColor(10, 15, 10));

        // 获取旋转后的节点位置并投影到2D
        QVector<QPointF> projectedInput, projectedHidden, projectedOutput;
        projectNodes(projectedInput, projectedHidden, projectedOutput);

        // 保存未排序的投影，用于固定连接动画顺序
        QVector<QPointF> unsortedInput = projectedInput;
        QVector<QPointF> unsortedHidden = projectedHidden;
        QVector<QPointF> unsortedOutput = projectedOutput;

        // 按Z轴排序节点，实现正确的遮挡效果
        QVector<Vector3D> sortedRotatedInput, sortedRotatedHidden, sortedRotatedOutput;
        sortNodesByDepth(projectedInput, m_inputNodes, sortedRotatedInput);
        sortNodesByDepth(projectedHidden, m_hiddenNodes, sortedRotatedHidden);
        sortNodesByDepth(projectedOutput, m_outputNodes, sortedRotatedOutput);

        // 绘制连接线
        // 计算第一阶段（输入层 -> 隐藏层）的连接总数
        double maxIH = m_inputNodes.size() * m_hiddenNodes.size();

        // 获取当前的全局动画进度
        double current = m_connectionStep;

        // 【修复】将动画进度视为一个连续的流
        // 第一阶段的进度就是当前总进度
        double ihCount = current;
        // 第二阶段的进度是总进度减去第一阶段的连接数
        // 当 current < maxIH 时，hoCount 为负，drawConnections 会正确地不绘制任何内容
        double hoCount = current - maxIH;

        painter.setPen(QPen(QColor(0, 200, 100, 80), 0.8));
        drawConnections(painter, unsortedInput, unsortedHidden, ihCount);
        drawConnections(painter, unsortedHidden, unsortedOutput, hoCount);


        // 绘制节点（按深度顺序，后绘制的节点在上方）
        for (int i = 0; i < projectedInput.size(); ++i) {
            double depthFactor = getDepthFactor(sortedRotatedInput[i].z);
            drawNeuron(&painter, projectedInput[i], 12 * depthFactor, 
                    QColor(0, 255, 100, 200 * depthFactor), m_rotX, m_rotY, m_rotZ);
        }

        for (int i = 0; i < projectedHidden.size(); ++i) {
            double depthFactor = getDepthFactor(sortedRotatedHidden[i].z);
            drawNeuron(&painter, projectedHidden[i], 12 * depthFactor, 
                    QColor(0, 255, 100, 200 * depthFactor), m_rotX, m_rotY, m_rotZ);
        }

        for (int i = 0; i < projectedOutput.size(); ++i) {
            double depthFactor = getDepthFactor(sortedRotatedOutput[i].z);
            drawNeuron(&painter, projectedOutput[i], 12 * depthFactor, 
                    QColor(0, 255, 100, 200 * depthFactor), m_rotX, m_rotY, m_rotZ);
        }
        // 绘制微弱网格
        painter.setPen(QPen(QColor(50, 80, 50, 30), 0.5));
        for (int x = 0; x < width(); x += 30) {
            painter.drawLine(x, 0, x, height());
        }
        for (int y = 0; y < height(); y += 30) {
            painter.drawLine(0, y, width(), y);
        }
    }
    
    // 鼠标交互控制旋转
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
            update(); // 重绘
        }
    }
    
    // 滚轮控制旋转速度
    void wheelEvent(QWheelEvent *event) override {
        if (event->modifiers() & Qt::ControlModifier) { // Ctrl + Wheel to Zoom
            int delta = event->angleDelta().y();
            double zoomDelta = delta > 0 ? 1.1 : 1.0 / 1.1;
            m_zoomFactor *= zoomDelta;
            m_zoomFactor = qBound(0.3, m_zoomFactor, 3.0);
        } else { // Just Wheel to change speed
            int delta = event->angleDelta().y();
            m_animationSpeed += delta * 0.001;
            m_animationSpeed = qBound(0.1, m_animationSpeed, 5.0);
        }
        update(); // Trigger redraw for both cases
    }

private:
    // 初始化神经网络节点的3D位置
    void initNeuralNetwork() {
        const int inputCount = 8;
        const int hiddenCount = 12;
        const int outputCount = 8;
        
        // Layer properties
        const double layerDepth = 150;
        const double startZ = -layerDepth;
        const double inputSpacing = 80.0;
        const double hiddenSpacing = 65.0;
        const double outputSpacing = 80.0;

        // Input layer nodes (centered around Y=0)
        for (int i = 0; i < inputCount; ++i) {
            // Calculate yPos relative to the center
            double yPos = (i - (inputCount - 1) / 2.0) * inputSpacing;
            m_inputNodes.append(Vector3D(-300, yPos, startZ));
        }
        
        // Hidden layer nodes (centered around Y=0)
        for (int i = 0; i < hiddenCount; ++i) {
            double yPos = (i - (hiddenCount - 1) / 2.0) * hiddenSpacing;
            m_hiddenNodes.append(Vector3D(0, yPos, startZ + layerDepth));
        }
        
        // Output layer nodes (centered around Y=0)
        for (int i = 0; i < outputCount; ++i) {
            double yPos = (i - (outputCount - 1) / 2.0) * outputSpacing;
            m_outputNodes.append(Vector3D(300, yPos, startZ + 2 * layerDepth));
        }
    }
    
    // 投影3D节点到2D屏幕
    void projectNodes(QVector<QPointF>& input, QVector<QPointF>& hidden, QVector<QPointF>& output) {
        input.clear();
        hidden.clear();
        output.clear();
        
        // 获取控件中心作为3D空间原点
        QPoint center(width() / 2, height() / 2);
        const double scale = 1; // 缩放因子
        
        // 投影输入层
        for (const auto& node : m_inputNodes) {
            Vector3D rotated = node.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
            input.append(projectPoint(rotated, center, scale));
        }
        
        // 投影隐藏层
        for (const auto& node : m_hiddenNodes) {
            Vector3D rotated = node.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
            hidden.append(projectPoint(rotated, center, scale));
        }
        
        // 投影输出层
        for (const auto& node : m_outputNodes) {
            Vector3D rotated = node.rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
            output.append(projectPoint(rotated, center, scale));
        }
    }
    
    // 3D点到2D屏幕的透视投影
    QPointF projectPoint(const Vector3D& point, const QPoint& center, double scale) {
        double zOffset = 800;
        double factor = zOffset / (zOffset + point.z);
        
        return QPointF(
            center.x() + point.x * factor * scale * m_zoomFactor,
            center.y() - point.y * factor * scale * m_zoomFactor
        );
    }
    
    // 按深度排序节点（z值小的在前面，即远处先绘；MODIFIED: 不修改baseNodes，添加sortedRotated输出）
    void sortNodesByDepth(QVector<QPointF>& projected, const QVector<Vector3D>& baseNodes, QVector<Vector3D>& sortedRotated) {
        int n = projected.size();
        QVector<int> indices(n);
        for (int i = 0; i < n; ++i) indices[i] = i;
        
        // 计算所有旋转后的节点（ADDED）
        QVector<Vector3D> rotated(n);
        for (int i = 0; i < n; ++i) {
            rotated[i] = baseNodes[i].rotatedX(m_rotX).rotatedY(m_rotY).rotatedZ(m_rotZ);
        }
        
        // 按旋转后Z值排序（z小的先，即远处先）
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            return rotated[a].z < rotated[b].z;
        });
        
        // 排序投影点和旋转节点
        QVector<QPointF> sortedProjected(n);
        sortedRotated.resize(n);
        for (int i = 0; i < n; ++i) {
            sortedProjected[i] = projected[indices[i]];
            sortedRotated[i] = rotated[indices[i]];
        }
        
        projected = sortedProjected;
        // 注意：不修改baseNodes
    }
    
    // 绘制神经元连接
   void drawConnections(QPainter &painter,
                                     const QVector<QPointF> &from,
                                     const QVector<QPointF> &to,
                                     double allowCount)
    {
        if (allowCount <= 0) return;

        const int fromSize = from.size();
        const int toSize   = to.size();
        const double window = m_parallelLines;         // 同时生长的“批”宽度

        int idx = 0;                                   // 全局顺序索引
        for (int i = 0; i < fromSize; ++i) {
            for (int j = 0; j < toSize; ++j, ++idx) {

                double offset = allowCount - idx;      // >0 已经开始

                /* ① 还没轮到它 */
                if (offset < 0.0)
                    continue;

                /* ② 已经完全长完 */
                if (offset >= window) {
                    painter.drawLine(from[i], to[j]);
                    continue;
                }

                /* ③ 正在生长（0~window） */
                double t = offset / window;            // 0~1
                double prog = easeInOut(t);            // 缓动
                QPointF end = from[i] + prog * (to[j] - from[i]);
                painter.drawLine(from[i], end);
            }
        }
    }

    // ADDED: 缓动函数 (ease-in-out quadratic)，使动画非线性、更平滑
    double easeInOut(double t) {
        if (t < 0.5) {
            return 2 * t * t;
        } else {
            return -1 + (4 - 2 * t) * t;
        }
    }
    
    // 获取深度因子（影响大小和透明度）
    double getDepthFactor(double z) {
        // 修正为近大远小：随z增加，factor减小
        // 归一化z值到[0,1]，然后反转并调整范围到[0.5, 1.5]
        double minZ = -1000;
        double maxZ = 1000;
        double normalized = (z - minZ) / (maxZ - minZ);
        return 1.5 - normalized; // 远处(z大) factor小，近处(z小) factor大
    }

    // 绘制单个神经元节点（带渐变效果，修改为更像胞体）
    // 绘制单个神经元节点（带渐变效果，修改为更像胞体）
void drawNeuron(QPainter *painter, const QPointF &center, qreal radius, const QColor &baseColor, double rotX, double rotY, double rotZ)
{
    // 使用固定的种子，确保每个神经元的内部结构在每帧保持一致
    uint seed = qHash(center.x() + center.y() * 1000);
    
    // 绘制胞体（主体）：略微不规则的椭圆形状，使用径向渐变
    painter->setPen(Qt::NoPen);
    QRadialGradient bodyGradient(center, radius, center - QPointF(radius * 0.3, radius * 0.3));
    bodyGradient.setColorAt(0, baseColor.lighter(130)); // 中心较亮（模拟细胞质）
    bodyGradient.setColorAt(0.7, baseColor);            // 中间渐变
    bodyGradient.setColorAt(1, baseColor.darker(150));  // 边缘较暗（模拟细胞膜）
    painter->setBrush(bodyGradient);

    // 创建椭圆路径
    QPainterPath bodyPath;
    bodyPath.addEllipse(center, radius * 1.1, radius); // 略微拉伸为椭圆

    // 使用固定的随机数生成器创建稳定的不规则形状
    QPolygonF polygon = bodyPath.toFillPolygon();
    for (int i = 0; i < polygon.size(); ++i) {
        // 使用索引计算固定的偏移值
        double angle = 2 * 3.14 * i / polygon.size();
        double offset = radius * 0.05;
        polygon[i] += QPointF(
            cos(angle) * offset,
            sin(angle) * offset
        );
    }
    bodyPath = QPainterPath();
    bodyPath.addPolygon(polygon);
    painter->drawPath(bodyPath);

    // 绘制细胞核（nucleus）：考虑3D旋转
    qreal nucleusRadius = radius * 0.4;
    
    // 细胞核的3D偏移（相对于胞体中心）
    Vector3D nucleusOffset3D(0, radius * 0.2, 0);
    
    // 应用与胞体相同的旋转
    Vector3D rotatedNucleusOffset = nucleusOffset3D
        .rotatedX(rotX)
        .rotatedY(rotY)
        .rotatedZ(rotZ);
    
    // 投影到2D（只使用x和y分量）
    QPointF nucleusCenter = center + QPointF(rotatedNucleusOffset.x, -rotatedNucleusOffset.y);
    
    // 根据z深度调整细胞核大小（近大远小）
    double nucleusDepthFactor = 1.0 + rotatedNucleusOffset.z * 0.001;
    qreal adjustedNucleusRadius = nucleusRadius * nucleusDepthFactor;
    
    QRadialGradient nucleusGradient(nucleusCenter, adjustedNucleusRadius);
    nucleusGradient.setColorAt(0, QColor(0, 100, 50, 200));   // 核中心深绿
    nucleusGradient.setColorAt(1, QColor(0, 50, 20, 150));    // 核边缘暗绿
    painter->setBrush(nucleusGradient);
    painter->drawEllipse(nucleusCenter, adjustedNucleusRadius, adjustedNucleusRadius);

    // 内亮点（高光，也需要考虑旋转）
    Vector3D highlightOffset3D(-radius * 0.2, -radius * 0.2, radius * 0.1);
    Vector3D rotatedHighlight = highlightOffset3D
        .rotatedX(rotX)
        .rotatedY(rotY)
        .rotatedZ(rotZ);
    
    QPointF highlightPos = center + QPointF(rotatedHighlight.x, -rotatedHighlight.y);
    double highlightDepthFactor = 1.0 + rotatedHighlight.z * 0.001;
    qreal highlightSize = radius * 0.2 * highlightDepthFactor;
    
    // 根据深度调整高光强度
    int highlightAlpha = 100 * (1.0 + rotatedHighlight.z * 0.0008);
    painter->setBrush(QColor(255, 255, 255, qBound(0, highlightAlpha, 200)));
    painter->drawEllipse(highlightPos, highlightSize, highlightSize);
}



    // 加载QSS样式表
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

private slots:
    // 更新旋转角度，创建动画效果
    void updateRotation() {
        m_rotY += 0.5 * m_animationSpeed;
        m_rotX += 0.2 * m_animationSpeed;
        m_rotZ += 0.1 * m_animationSpeed;
        
        // 确保角度在0-360范围内
        m_rotX = fmod(m_rotX, 360);
        m_rotY = fmod(m_rotY, 360);
        m_rotZ = fmod(m_rotZ, 360);
        
        update(); // 触发重绘
    }
    
    // 切换动画状态（播放/暂停）
    void toggleAnimation() {
        if (m_animTimer->isActive()) {
            m_animTimer->stop();
            m_toggleAnimBtn->setText(tr("PLAY ANIM"));
        } else {
            m_animTimer->start();
            m_toggleAnimBtn->setText(tr("PAUSE ANIM"));
        }
    }

    // 处理鼠标点击
    void handleCellClick(const QModelIndex &index)
    {
        if (index.isValid()) {
            onSelectionChanged(m_view->selectionModel()->selection(), QItemSelection());
        }
    }

    // 处理所有选择变化（包括键盘导航）
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

    // 在浏览器中打开链接
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
            m_connectionStep = 0.0; // 重置动画进度
            m_connTimer->start();   // 重启连接动画
            update();               // 触发重绘
        }
private:
    QStandardItemModel *m_srcModel;
    CustomProxyModel   *m_proxyModel;
    QTableView         *m_view;
    QLineEdit          *m_filterEdit;
    QTextEdit          *m_infoDisplay;
    QPushButton        *m_downloadBtn;
    QPushButton        *m_toggleAnimBtn; // 动画控制按钮
    int                 m_selectedRow;
    
    // 3D旋转相关
    QTimer* m_animTimer;       // 动画定时器
    double m_rotX, m_rotY, m_rotZ; // 旋转角度
    double m_animationSpeed;   // 动画速度
    bool m_isDragging = false; // MODIFIED: 初始化
    QPoint m_lastMousePos;     // 上次鼠标位置
    
    // 神经网络3D节点
    QVector<Vector3D> m_inputNodes;   // 输入层节点
    QVector<Vector3D> m_hiddenNodes;  // 隐藏层节点
    QVector<Vector3D> m_outputNodes;  // 输出层节点
    // 连接动画相关
    QTimer *m_connTimer = nullptr;   // 控制“逐条连线”动画
    double m_connectionStep;      // MODIFIED: double，支持小数进度
    int m_direction;              // 方向 (1: 添加, -1: 移除)
    double m_connAnimStep;        // ADDED: 每 tick 的步长（控制延伸速度）
    int m_parallelLines;   // ADDED: 同时延伸的线条数（默认3）
    double m_zoomFactor = 1;  // 添加缩放因子成员变量
    QTimer *m_pauseTimer;  // 用于控制动画结束后的暂停
};

#endif // CSVTABLEWIDGET_H
