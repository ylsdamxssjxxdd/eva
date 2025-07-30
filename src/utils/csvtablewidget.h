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
          m_animationSpeed(0.5), // 旋转动画速度
          m_connectionStep(0), // ADDED: 初始化
          m_direction(1) // ADDED: 动画方向（1: 添加, -1: 移除）
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
        
        // 设置动画定时器
        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(30);// 旋转动画速率
        connect(m_animTimer, &QTimer::timeout, this, &CsvTableWidget::updateRotation);
        m_animTimer->start(); // 启动动画

        // ① 先算出总连线数，记住以后要用
        const int totalConn =  m_inputNodes .size() * m_hiddenNodes .size()
                            + m_hiddenNodes.size() * m_outputNodes.size();

        // ② 连接计时器，让它以 50ms 的频率递增/递减步数（MODIFIED: 加速动画，添加移除阶段）
        m_connTimer = new QTimer(this);
        m_connTimer->setInterval(50); // MODIFIED: 更快速率，避免太慢导致跳跃感
        connect(m_connTimer, &QTimer::timeout, this, [=]() {
            m_connectionStep += m_direction;
            if (m_connectionStep >= totalConn) {
                m_connectionStep = totalConn;
                m_direction = -1; // 切换到移除
            } else if (m_connectionStep <= 0) {
                m_connectionStep = 0;
                m_direction = 1; // 切换到添加
            }
            update(); // 触发重绘
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
        Q_UNUSED(event)
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // 绘制深色背景
        painter.fillRect(rect(), QColor(10, 15, 10));

        // 获取旋转后的节点位置并投影到2D
        QVector<QPointF> projectedInput, projectedHidden, projectedOutput;
        projectNodes(projectedInput, projectedHidden, projectedOutput);

        // 保存未排序的投影，用于固定连接动画顺序（ADDED: 修复动画跳跃）
        QVector<QPointF> unsortedInput = projectedInput;
        QVector<QPointF> unsortedHidden = projectedHidden;
        QVector<QPointF> unsortedOutput = projectedOutput;

        // 按Z轴排序节点，实现正确的遮挡效果（MODIFIED: 引入sortedRotated用于depthFactor）
        QVector<Vector3D> sortedRotatedInput, sortedRotatedHidden, sortedRotatedOutput;
        sortNodesByDepth(projectedInput, m_inputNodes, sortedRotatedInput);
        sortNodesByDepth(projectedHidden, m_hiddenNodes, sortedRotatedHidden);
        sortNodesByDepth(projectedOutput, m_outputNodes, sortedRotatedOutput);

        // 绘制连接线（使用未排序投影，固定动画顺序；MODIFIED）
        // 计算当前步数对应的 from-hidden, hidden-out 两段各能画多少
        int maxIH = m_inputNodes.size()  * m_hiddenNodes.size();
        int maxHO = m_hiddenNodes.size() * m_outputNodes.size();

        int ihCount = qMin(m_connectionStep, maxIH);
        int hoCount = qMax(0, m_connectionStep - maxIH);   
        hoCount      = qMin(hoCount, maxHO);

        painter.setPen(QPen(QColor(0, 200, 100, 80), 0.8));
        drawConnections(painter, unsortedInput , unsortedHidden, ihCount);
        drawConnections(painter, unsortedHidden, unsortedOutput, hoCount);

        // 绘制节点（按深度顺序，后绘制的节点在上方；MODIFIED: 使用sortedRotated.z）
        for (int i = 0; i < projectedInput.size(); ++i) {
            double depthFactor = getDepthFactor(sortedRotatedInput[i].z);
            drawNeuron(&painter, projectedInput[i], 4 * depthFactor, 
                      QColor(0, 255, 100, 200 * depthFactor));
        }
        
        for (int i = 0; i < projectedHidden.size(); ++i) {
            double depthFactor = getDepthFactor(sortedRotatedHidden[i].z);
            drawNeuron(&painter, projectedHidden[i], 3.5 * depthFactor, 
                      QColor(100, 255, 150, 200 * depthFactor));
        }
        
        for (int i = 0; i < projectedOutput.size(); ++i) {
            double depthFactor = getDepthFactor(sortedRotatedOutput[i].z);
            drawNeuron(&painter, projectedOutput[i], 4 * depthFactor, 
                      QColor(0, 255, 100, 200 * depthFactor));
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
        int delta = event->angleDelta().y();
        m_animationSpeed += delta * 0.001;
        m_animationSpeed = qBound(0.1, m_animationSpeed, 5.0); // 限制速度范围
    }

private:
    // 初始化神经网络节点的3D位置
    void initNeuralNetwork() {
        const int inputCount = 8;
        const int hiddenCount = 12;
        const int outputCount = 4;
        
        // 层间距（3D空间）
        const double layerDepth = 150;
        const double startZ = -layerDepth;
        
        // 输入层节点（左侧）
        for (int i = 0; i < inputCount; ++i) {
            double yPos = -200 + (i * 60.0); // 垂直分布
            m_inputNodes.append(Vector3D(-300, yPos, startZ));
        }
        
        // 隐藏层节点（中间）
        for (int i = 0; i < hiddenCount; ++i) {
            double yPos = -250 + (i * 45.0);
            m_hiddenNodes.append(Vector3D(0, yPos, startZ + layerDepth));
        }
        
        // 输出层节点（右侧）
        for (int i = 0; i < outputCount; ++i) {
            double yPos = -150 + (i * 80.0);
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
        // 简单透视投影
        double zOffset = 1000; // 增加这个值会减小透视效果
        double factor = zOffset / (zOffset + point.z);
        
        return QPointF(
            center.x() + point.x * factor * scale,
            center.y() - point.y * factor * scale // Y轴反转，因为屏幕Y向下增长
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
    // from-layer 与 to-layer 共有 maxCount() 根连线，
    // 只画前 allowCount 根
    void drawConnections(QPainter &painter,
                        const QVector<QPointF> &from,
                        const QVector<QPointF> &to,
                        int allowCount)
    {
        int painted = 0;
        for (int i = 0; i < from.size(); ++i) {
            for (int j = 0; j < to.size(); ++j) {
                if (painted >= allowCount) return;      // 只画到上限就退出
                painter.drawLine(from[i], to[j]);
                ++painted;
            }
        }
    }
    
    // 获取深度因子（影响大小和透明度）
    double getDepthFactor(double z) {
        // 归一化z值到[0.5, 1.5]范围，使远处的节点小一些
        double minZ = -1000;
        double maxZ = 1000;
        double normalized = (z - minZ) / (maxZ - minZ);
        return 0.5 + normalized; // 范围 [0.5, 1.5]
    }

    // 绘制单个神经元节点（带渐变效果）
    void drawNeuron(QPainter *painter, const QPointF &center, qreal radius, const QColor &baseColor)
    {
        // 外圆（半透明）
        painter->setPen(Qt::NoPen);
        QRadialGradient gradient(center, radius, center - QPointF(radius*0.3, radius*0.3));
        gradient.setColorAt(0, baseColor.lighter(150)); // 中心亮
        gradient.setColorAt(1, baseColor.darker(120));  // 边缘暗
        painter->setBrush(gradient);
        painter->drawEllipse(center, radius, radius);

        // 内亮点（增强立体感）
        painter->setBrush(QColor(255, 255, 255, 100)); // 白色半透
        painter->drawEllipse(center - QPointF(radius*0.2, radius*0.2), radius*0.2, radius*0.2);
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
    int      m_connectionStep;       // 已经点亮的连线条数
    int      m_direction;            // ADDED: 方向 (1: 添加, -1: 移除)
};

#endif // CSVTABLEWIDGET_H
