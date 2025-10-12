#ifndef IMAGEINPUTBOX_H
#define IMAGEINPUTBOX_H

#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

class ImageInputBox : public QWidget
{
    Q_OBJECT
  public:
    QTextEdit *textEdit;
    QScrollArea *scrollArea;
    QWidget *thumbnailContainer;
    QGridLayout *thumbnailLayout;
    QVBoxLayout *mainLayout;
    QStringList filePaths; // 改为通用文件路径
    explicit ImageInputBox(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setupUI();
        // 禁止子部件处理拖放
        scrollArea->setAcceptDrops(false);
        textEdit->setAcceptDrops(false);
        thumbnailContainer->setAcceptDrops(false);
        textEdit->viewport()->setAcceptDrops(false);
        scrollArea->viewport()->setAcceptDrops(false); // 禁用视口
        this->setAcceptDrops(true);
        this->setMouseTracking(true);
        thumbnailContainer->installEventFilter(this); // 添加事件过滤器
        // 连接文本变化信号到调整高度的槽函数
        connect(textEdit, &QTextEdit::textChanged, this, &ImageInputBox::adjustHeight);
    }

    void addFiles(const QStringList &paths)
    {
        for (int i = 0; i < paths.size(); ++i)
        {
            QString path = paths[i];
            addFileThumbnail(path);
        }
    }

    // 检查是否是支持的文件类型
    bool isSupportedFile(const QString &path) const
    {
        static QStringList extensions = {"png", "jpg", "jpeg", "gif", "bmp", "wav", "mp3"};
        return extensions.contains(QFileInfo(path).suffix().toLower());
    }
    // 检查是否是图像文件
    bool isImageFile(const QString &path) const
    {
        static QStringList imageExtensions = {"png", "jpg", "jpeg", "gif", "bmp"};
        return imageExtensions.contains(QFileInfo(path).suffix().toLower());
    }
    // 检查是否是音频文件
    bool isAudioFile(const QString &path) const
    {
        static QStringList audioExtensions = {"wav", "mp3"};
        return audioExtensions.contains(QFileInfo(path).suffix().toLower());
    }

    // 新增：返回所有WAV文件路径
    QStringList wavFilePaths() const
    {
        QStringList wavPaths;
        for (const QString &path : filePaths)
        {
            if (isAudioFile(path))
            {
                wavPaths.append(path);
            }
        }
        return wavPaths;
    }
    // 返回所有图像文件路径
    QStringList imageFilePaths() const
    {
        QStringList imagePaths;
        for (const QString &path : filePaths)
        {
            if (isImageFile(path))
            {
                imagePaths.append(path);
            }
        }
        return imagePaths;
    }

    bool hasSupportedUrls(const QMimeData *mimeData) const
    {
        if (!mimeData->hasUrls()) return false;

        foreach (QUrl url, mimeData->urls())
        {
            if (!isSupportedFile(url.toLocalFile()))
            {
                return false;
            }
        }
        return true;
    }

    void addFileThumbnail(const QString &path)
    {
        if (filePaths.contains(path)) return;

        QLabel *previewLabel = new QLabel;
        QPixmap pixmap;

        if (isAudioFile(path))
        {
            // 音频文件使用图标
            QIcon audioIcon(":/logo/wav.png");
            if (audioIcon.isNull())
            {
                // 内置备用图标
                QPixmap audioPixmap(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
                audioPixmap.fill(Qt::transparent);
                QPainter painter(&audioPixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setBrush(QColor(70, 130, 180)); // 钢蓝色
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(5, 5, THUMBNAIL_SIZE - 10, THUMBNAIL_SIZE - 10);
                painter.setBrush(Qt::white);
                painter.drawEllipse(10, 10, THUMBNAIL_SIZE - 20, THUMBNAIL_SIZE - 20);
                painter.end();
                pixmap = audioPixmap;
            }
            else
            {
                pixmap = audioIcon.pixmap(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
            }
            previewLabel->setToolTip("音频文件: " + path);
        }
        else
        {
            // 图像文件
            pixmap = QPixmap(path);
            if (pixmap.isNull()) return;
            pixmap = pixmap.scaled(THUMBNAIL_SIZE, THUMBNAIL_SIZE,
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
            previewLabel->setToolTip("图像文件: " + path);
        }

        previewLabel->setPixmap(pixmap);
        previewLabel->setFixedSize(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
        previewLabel->setStyleSheet("border: 1px solid #ccc; background: white;");

        QPushButton *closeBtn = new QPushButton("×", previewLabel);
        closeBtn->setStyleSheet(
            "QPushButton {"
            "  background: rgba(255, 0, 0, 150);"
            "  color: white;"
            "  border: none;"
            "  border-radius: 2px;"
            "  min-width: 4px;"
            "  max-width: 4px;"
            "  min-height: 4px;"
            "  max-height: 4px;"
            "}"
            "QPushButton:hover { background: rgba(255, 0, 0, 200); }");
        closeBtn->move(THUMBNAIL_SIZE - 15, 2);
        connect(closeBtn, &QPushButton::clicked, [this, path, previewLabel]()
                {
            filePaths.removeAll(path);
            thumbnailLayout->removeWidget(previewLabel);
            previewLabel->deleteLater();
            updateLayout();
            adjustHeight(); });

        thumbnailLayout->addWidget(previewLabel);
        filePaths.append(path);
        adjustHeight();
        updateLayout();
    }

    // 重写 resizeEvent，当宽度变化时重新调整高度
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        adjustHeight();
    }

    // 添加清空缩略图函数
    void clearThumbnails()
    {
        // 移除所有缩略图部件
        QLayoutItem *child;
        while ((child = thumbnailLayout->takeAt(0)) != nullptr)
        {
            delete child->widget();
            delete child;
        }
        filePaths.clear();
        updateLayout();
        adjustHeight();
    }

  private:
    const int THUMBNAIL_SIZE = 30;
    void setupUI()
    {
        mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 缩略图滚动区域
        scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setMinimumHeight(THUMBNAIL_SIZE + 20);
        scrollArea->setMaximumHeight(THUMBNAIL_SIZE + 20);
        scrollArea->setStyleSheet(
            "QScrollArea {"
            "    border: 1px solid #3498db;" // 现代蓝色边框
            "    border-radius: 4px;"
            "    background: #f8fafc;"
            "}"
            "QScrollBar:horizontal {"
            "    height: 8px;"
            "    background: transparent;"
            "    margin: 0 2px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "    background: #a0c4e4;" // 浅蓝色滚动条
            "    border-radius: 4px;"
            "    min-width: 30px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "    background: #7fb2e0;" // 悬停时稍深的蓝色
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "    background: none;"
            "}");

        // 缩略图容器
        thumbnailContainer = new QWidget;
        thumbnailContainer->setStyleSheet(
            "background: #f8fafc;" // 非常浅的蓝色背景
            "border: none;");

        thumbnailLayout = new QGridLayout(thumbnailContainer);
        thumbnailLayout->setSpacing(12);                   // 增加间距
        thumbnailLayout->setContentsMargins(12, 8, 12, 8); // 调整边距
        thumbnailLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        scrollArea->setWidget(thumbnailContainer);
        mainLayout->addWidget(scrollArea);

        // 文本输入框
        textEdit = new QTextEdit;
        mainLayout->addWidget(textEdit);
        updateLayout();
    }

    void updateLayout()
    {
        if (thumbnailLayout->count() == 0)
        {
            scrollArea->setVisible(false);
            return;
        }

        // 计算可用列数
        int availableWidth = thumbnailContainer->width() - thumbnailLayout->contentsMargins().left() - thumbnailLayout->contentsMargins().right();
        int itemWidth = THUMBNAIL_SIZE + thumbnailLayout->horizontalSpacing();
        int columns = qMax(1, availableWidth / itemWidth);

        // 重新排列所有缩略图
        QList<QWidget *> widgets;
        for (int i = 0; i < thumbnailLayout->count(); ++i)
        {
            if (QLayoutItem *item = thumbnailLayout->itemAt(i))
            {
                if (QWidget *w = item->widget())
                {
                    widgets.append(w);
                }
            }
        }

        // 清空布局并重新排列
        while (thumbnailLayout->count() > 0)
        {
            thumbnailLayout->takeAt(0);
        }

        for (int i = 0; i < widgets.size(); ++i)
        {
            int row = i / columns;
            int col = i % columns;
            thumbnailLayout->addWidget(widgets[i], row, col);
        }

        // 更新容器高度
        int rows = qCeil(static_cast<qreal>(widgets.size()) / columns);
        int rowHeight = THUMBNAIL_SIZE + thumbnailLayout->verticalSpacing();
        int totalHeight = rows * rowHeight + thumbnailLayout->contentsMargins().top() + thumbnailLayout->contentsMargins().bottom();
        thumbnailContainer->setMinimumHeight(totalHeight);

        scrollArea->setVisible(!widgets.isEmpty());
    }
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (hasSupportedUrls(event->mimeData()))
        {
            event->acceptProposedAction();
            setStyleSheet("background: #f0f0f0;");
        }
    }
    void dragMoveEvent(QDragMoveEvent *event) override
    {
        event->accept();
    }
    void dropEvent(QDropEvent *event) override
    {
        const QMimeData *mimeData = event->mimeData();
        if (mimeData->hasUrls())
        {
            foreach (QUrl url, mimeData->urls())
            {
                QString path = url.toLocalFile();
                if (isSupportedFile(path))
                {
                    addFileThumbnail(path);
                }
            }
            event->acceptProposedAction();
            setStyleSheet(""); // 恢复样式
        }
    }
    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        Q_UNUSED(event);
        setStyleSheet(""); // 恢复样式
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == thumbnailContainer && event->type() == QEvent::Resize)
        {
            updateLayout(); // 刷新
        }
        return QWidget::eventFilter(watched, event);
    }

    int m_minHeight = 80;  // 最小高度
    int m_maxHeight = 300; // 最大高度
  private slots:
    // 添加文件进来
    void handleFileUpload(QStringList paths)
    {
        foreach (QString path, paths)
        {
            addFileThumbnail(path);
        }
    }

    // 调整高度的槽函数
    void adjustHeight()
    {
        // 获取文档内容高度
        QTextDocument *doc = textEdit->document();
        doc->setTextWidth(textEdit->viewport()->width());
        QSizeF docSize = doc->size();

        int thumbnail_height = 0;
        if (!filePaths.isEmpty()) { thumbnail_height += THUMBNAIL_SIZE * 2; }
        // 计算新的高度，考虑边框和内边距
        int newHeight = static_cast<int>(docSize.height()) + textEdit->frameWidth() * 2 + textEdit->contentsMargins().top() + textEdit->contentsMargins().bottom() + thumbnail_height;

        // 限制高度在最小和最大值之间
        newHeight = qBound(m_minHeight, newHeight, m_maxHeight);

        // 设置新的固定高度
        textEdit->setFixedHeight(newHeight);
        setFixedHeight(newHeight + thumbnail_height);
    }
};

#endif // IMAGEINPUTBOX_H