#ifndef IMAGEINPUTBOX_H
#define IMAGEINPUTBOX_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QPixmap>
#include <QScrollArea>
#include <QPushButton>
#include <QtMath>
#include <QDebug>
#include <QTextEdit>
#include <QTextDocument>
#include <QTimer>
class ImageInputBox : public QWidget {
    Q_OBJECT
public:
    QTextEdit *textEdit;
    QScrollArea *scrollArea;
    QWidget *thumbnailContainer;
    QGridLayout *thumbnailLayout;
    QVBoxLayout *mainLayout;
    QStringList imagePaths;
    explicit ImageInputBox(QWidget *parent = nullptr) : QWidget(parent) {
        setupUI();
        // 禁止子部件处理拖放
        scrollArea->setAcceptDrops(false);
        textEdit->setAcceptDrops(false);
        thumbnailContainer->setAcceptDrops(false);
        textEdit->viewport()->setAcceptDrops(false);
        scrollArea->viewport()->setAcceptDrops(false); //禁用视口
        this->setAcceptDrops(true);
        this->setMouseTracking(true);
        thumbnailContainer->installEventFilter(this); // 添加事件过滤器
        // 连接文本变化信号到调整高度的槽函数
        connect(textEdit, &QTextEdit::textChanged, this, &ImageInputBox::adjustHeight);
    }

    void addImage(const QString &path) {
        addImageThumbnail(path);
    }
    bool isImageFile(const QString &path) const {
        static QStringList extensions = {"png", "jpg", "jpeg", "gif", "bmp"};
        return extensions.contains(QFileInfo(path).suffix().toLower());
    }

    bool hasImageUrls(const QMimeData *mimeData) const {
        if (!mimeData->hasUrls()) return false;

        foreach (QUrl url, mimeData->urls()) {
            if (!isImageFile(url.toLocalFile())) {
                return false;
            }
        }
        return true;
    }
    void addImageThumbnail(const QString &path) {
        if (imagePaths.contains(path)) return;

        QPixmap pixmap(path);
        if (pixmap.isNull()) return;

        QLabel *imageLabel = new QLabel;
        imageLabel->setPixmap(pixmap.scaled(THUMBNAIL_SIZE, THUMBNAIL_SIZE,
                                  Qt::KeepAspectRatio, Qt::SmoothTransformation));
        imageLabel->setFixedSize(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
        imageLabel->setStyleSheet("border: 1px solid #ccc; background: white;");
        imageLabel->setToolTip(path);

        QPushButton *closeBtn = new QPushButton("×", imageLabel);
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
            "QPushButton:hover { background: rgba(255, 0, 0, 200); }"
        );
        closeBtn->move(THUMBNAIL_SIZE - 15, 2);
        connect(closeBtn, &QPushButton::clicked, [this, path, imageLabel]() {
            imagePaths.removeAll(path);
            thumbnailLayout->removeWidget(imageLabel);
            imageLabel->deleteLater();
            updateLayout();
            adjustHeight();
        });

        thumbnailLayout->addWidget(imageLabel);
        imagePaths.append(path);
        adjustHeight();
        updateLayout();
    }

    // 重写 resizeEvent，当宽度变化时重新调整高度
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        adjustHeight();
    }

private:
    const int THUMBNAIL_SIZE = 30;
    void setupUI() {
        mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        // 缩略图滚动区域
        scrollArea = new QScrollArea;
        scrollArea->setWidgetResizable(true);
        scrollArea->setMinimumHeight(THUMBNAIL_SIZE + 20);
        scrollArea->setMaximumHeight(THUMBNAIL_SIZE + 20);
        scrollArea->setStyleSheet("QScrollArea { border: none; }");

        // // 缩略图容器
        thumbnailContainer = new QWidget;
        thumbnailLayout = new QGridLayout(thumbnailContainer);
        thumbnailLayout->setSpacing(0);
        thumbnailLayout->setContentsMargins(10, 10, 10, 10);
        thumbnailLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        scrollArea->setWidget(thumbnailContainer);
        mainLayout->addWidget(scrollArea);

        // 文本输入框
        textEdit = new QTextEdit;
        mainLayout->addWidget(textEdit);
        updateLayout();
    }

    void updateLayout() {
        if (thumbnailLayout->count() == 0) {
            scrollArea->setVisible(false);
            return;
        }

        // 计算可用列数
        int availableWidth = thumbnailContainer->width()
                            - thumbnailLayout->contentsMargins().left()
                            - thumbnailLayout->contentsMargins().right();
        int itemWidth = THUMBNAIL_SIZE + thumbnailLayout->horizontalSpacing();
        int columns = qMax(1, availableWidth / itemWidth);

        // 重新排列所有缩略图
        QList<QWidget*> widgets;
        for (int i = 0; i < thumbnailLayout->count(); ++i) {
            if (QLayoutItem *item = thumbnailLayout->itemAt(i)) {
                if (QWidget *w = item->widget()) {
                    widgets.append(w);
                }
            }
        }

        // 清空布局并重新排列
        while (thumbnailLayout->count() > 0) {
            thumbnailLayout->takeAt(0);
        }

        for (int i = 0; i < widgets.size(); ++i) {
            int row = i / columns;
            int col = i % columns;
            thumbnailLayout->addWidget(widgets[i], row, col);
        }

        // 更新容器高度
        int rows = qCeil(static_cast<qreal>(widgets.size()) / columns);
        int rowHeight = THUMBNAIL_SIZE + thumbnailLayout->verticalSpacing();
        int totalHeight = rows * rowHeight
                        + thumbnailLayout->contentsMargins().top()
                        + thumbnailLayout->contentsMargins().bottom();
        thumbnailContainer->setMinimumHeight(totalHeight);

        scrollArea->setVisible(!widgets.isEmpty());
    }
    void dragEnterEvent(QDragEnterEvent *event) override{
//        qDebug()<<"hello";
        event->acceptProposedAction();
        setStyleSheet("background: #f0f0f0;");
//        qDebug()<<hasImageUrls(event->mimeData());
        event->accept();
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        event->accept();
    }
    void dropEvent(QDropEvent *event) override{
//        qDebug()<<"hello2"<<event->mimeData();
        const QMimeData *mimeData = event->mimeData();
        if (mimeData->hasUrls()) {
            foreach (QUrl url, mimeData->urls()) {
                QString path = url.toLocalFile();
                qDebug()<<path;
                if (isImageFile(path)) {
                    addImageThumbnail(path);
                }
            }
            event->acceptProposedAction();
            setStyleSheet(""); // 恢复样式
        }
    }
    void dragLeaveEvent(QDragLeaveEvent *event) override {
        setStyleSheet(""); // 恢复样式
    }

    bool eventFilter(QObject *watched, QEvent *event) override{
        if (watched == thumbnailContainer && event->type() == QEvent::Resize) {
            updateLayout();// 刷新
        }
        return QWidget::eventFilter(watched, event);
    }

    int m_minHeight=80;// 最小高度
    int m_maxHeight=300;// 最大高度
private slots:
    //添加图片进来
    void handleImageUpload(QStringList paths) {
        foreach (QString path, paths) {
            addImageThumbnail(path);

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
        if (imagePaths.size()!=0){thumbnail_height+=THUMBNAIL_SIZE*2;}
        // 计算新的高度，考虑边框和内边距
        int newHeight = static_cast<int>(docSize.height()) + textEdit->frameWidth() * 2 + textEdit->contentsMargins().top() + textEdit->contentsMargins().bottom() + thumbnail_height;

        // 限制高度在最小和最大值之间
        newHeight = qBound(m_minHeight, newHeight, m_maxHeight);

        // 设置新的固定高度
        textEdit->setFixedHeight(newHeight);
        setFixedHeight(newHeight+thumbnail_height);
    }
};

#endif // IMAGEINPUTBOX_H
