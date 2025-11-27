#ifndef IMAGEINPUTBOX_H
#define IMAGEINPUTBOX_H

#include <QColor>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QIcon>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStyle>
#include <QToolButton>
#include <QString>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVariant>
#include <QVector>
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
    explicit ImageInputBox(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground, true);
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
        for (const QString &path : paths)
        {
            addFileThumbnail(path);
        }
    }

    // 检查是否是支持的文件类型
    bool isSupportedFile(const QString &path) const
    {
        return classifyAttachment(path) != AttachmentKind::Unknown;
    }
    // 检查是否是图像文件
    bool isImageFile(const QString &path) const
    {
        return classifyAttachment(path) == AttachmentKind::Image;
    }
    // 检查是否是音频文件
    bool isAudioFile(const QString &path) const
    {
        return classifyAttachment(path) == AttachmentKind::Audio;
    }
    // 检查是否是文档文件
    bool isDocumentFile(const QString &path) const
    {
        return classifyAttachment(path) == AttachmentKind::Document;
    }

    // 新增：返回所有音频文件路径
    QStringList wavFilePaths() const
    {
        return filePathsByKind(AttachmentKind::Audio);
    }
    // 返回所有图像文件路径
    QStringList imageFilePaths() const
    {
        return filePathsByKind(AttachmentKind::Image);
    }
    // 返回所有文档文件路径
    QStringList documentFilePaths() const
    {
        return filePathsByKind(AttachmentKind::Document);
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
        const AttachmentKind kind = classifyAttachment(path);
        if (kind == AttachmentKind::Unknown) return;
        if (containsAttachment(path)) return;

        QWidget *itemWidget = new QWidget;
        itemWidget->setObjectName(QStringLiteral("imageInputPreviewItem"));
        itemWidget->setMinimumWidth(THUMBNAIL_TEXT_WIDTH);
        QVBoxLayout *itemLayout = new QVBoxLayout(itemWidget);
        itemLayout->setContentsMargins(2, 2, 2, 2);
        itemLayout->setSpacing(2);

        QLabel *previewLabel = new QLabel(itemWidget);
        previewLabel->setAlignment(Qt::AlignCenter);
        previewLabel->setFixedSize(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
        previewLabel->setObjectName(QStringLiteral("imageInputPreview"));
        previewLabel->setAttribute(Qt::WA_StyledBackground, true);

        QLabel *captionLabel = new QLabel(itemWidget);
        captionLabel->setAlignment(Qt::AlignCenter);
        QFont captionFont = captionLabel->font();
        captionFont.setPointSizeF(8.0);
        captionLabel->setFont(captionFont);
        captionLabel->setWordWrap(false);
        captionLabel->setStyleSheet(QStringLiteral("font-size:8pt; margin:0px; padding:0px;"));
        captionLabel->setObjectName(QStringLiteral("imageInputPreviewCaption"));
        captionLabel->setFixedWidth(THUMBNAIL_TEXT_WIDTH);
        captionLabel->setText(elidedAttachmentName(path));
        captionLabel->setToolTip(path);

        QPixmap pixmap;

        if (kind == AttachmentKind::Audio)
        {
            QIcon audioIcon(":/logo/wav.png");
            if (audioIcon.isNull())
            {
                QPixmap audioPixmap(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
                audioPixmap.fill(Qt::transparent);
                QPainter painter(&audioPixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setBrush(QColor(70, 130, 180));
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
            previewLabel->setToolTip(tr("Audio: %1").arg(path));
        }
        else if (kind == AttachmentKind::Image)
        {
            pixmap = QPixmap(path);
            if (pixmap.isNull()) return;
            pixmap = pixmap.scaled(THUMBNAIL_SIZE, THUMBNAIL_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            previewLabel->setToolTip(tr("Image: %1").arg(path));
        }
        else
        {
            const QString suffix = QFileInfo(path).suffix();
            pixmap = documentBadgeForSuffix(suffix);
            previewLabel->setToolTip(tr("Document: %1").arg(path));
        }

        if (!pixmap.isNull())
        {
            pixmap = pixmap.scaled(THUMBNAIL_SIZE, THUMBNAIL_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        previewLabel->setPixmap(pixmap);

        QToolButton *closeBtn = new QToolButton(previewLabel);
        closeBtn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        closeBtn->setIconSize(QSize(8, 8));
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setAutoRaise(true);
        closeBtn->setFixedSize(12, 12);
        closeBtn->setStyleSheet("QToolButton { border: 1px solid rgba(220,0,0,180); border-radius: 2px; background: rgba(255,255,255,180); }"
                                "QToolButton:hover { background: rgba(255,200,200,200); border-color: rgb(255,0,0); }");
        closeBtn->move(previewLabel->width() - closeBtn->width(), 0);
        closeBtn->show();
        connect(closeBtn, &QToolButton::clicked, [this, path, itemWidget]()
                {
            removeAttachment(path);
            thumbnailLayout->removeWidget(itemWidget);
            itemWidget->deleteLater();
            updateLayout();
            adjustHeight(); });

        itemLayout->addWidget(previewLabel, 0, Qt::AlignHCenter);
        itemLayout->addWidget(captionLabel, 0, Qt::AlignHCenter);

        thumbnailLayout->addWidget(itemWidget);
        attachments_.append({path, kind});
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
        attachments_.clear();
        updateLayout();
        adjustHeight();
    }

  private:
    enum class AttachmentKind
    {
        Image,
        Audio,
        Document,
        Unknown
    };
    struct AttachmentEntry
    {
        QString path;
        AttachmentKind kind;
    };
    const int THUMBNAIL_SIZE = 40;
    const int THUMBNAIL_TEXT_WIDTH = THUMBNAIL_SIZE + 28;
    QVector<AttachmentEntry> attachments_;
    QHash<QString, QPixmap> docIconCache_;
    void updateDragHighlight(bool active)
    {
        const QByteArray value = active ? QByteArray("true") : QByteArray();
        auto applyState = [&](QWidget *target)
        {
            if (!target) return;
            if (value.isEmpty())
            {
                target->setProperty("dragActive", QVariant());
            }
            else
            {
                target->setProperty("dragActive", QString::fromUtf8(value));
            }
            if (QStyle *style = target->style())
            {
                style->unpolish(target);
                style->polish(target);
            }
            target->update();
        };

        applyState(this);
        applyState(textEdit);
    }
    bool containsAttachment(const QString &path) const
    {
        for (const AttachmentEntry &entry : attachments_)
        {
            if (entry.path == path) return true;
        }
        return false;
    }
    QStringList filePathsByKind(AttachmentKind kind) const
    {
        QStringList paths;
        for (const AttachmentEntry &entry : attachments_)
        {
            if (entry.kind == kind)
            {
                paths.append(entry.path);
            }
        }
        return paths;
    }
    void removeAttachment(const QString &path)
    {
        for (int i = 0; i < attachments_.size(); ++i)
        {
            if (attachments_.at(i).path == path)
            {
                attachments_.removeAt(i);
                break;
            }
        }
    }
    AttachmentKind classifyAttachment(const QString &path) const
    {
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (suffix.isEmpty()) return AttachmentKind::Unknown;
        if (imageExtensions().contains(suffix)) return AttachmentKind::Image;
        if (audioExtensions().contains(suffix)) return AttachmentKind::Audio;
        if (documentExtensions().contains(suffix)) return AttachmentKind::Document;
        return AttachmentKind::Unknown;
    }
    QPixmap documentBadgeForSuffix(const QString &suffix)
    {
        const QString normalized = suffix.trimmed().toLower();
        const QString cacheKey = normalized.isEmpty() ? QStringLiteral("__unknown") : normalized;
        const auto it = docIconCache_.constFind(cacheKey);
        if (it != docIconCache_.constEnd()) return it.value();

        QPixmap pixmap;
        const QString resourcePath = docIconResourcePath(normalized);
        if (!resourcePath.isEmpty())
        {
            pixmap.load(resourcePath);
        }
        if (pixmap.isNull())
        {
            pixmap.load(QStringLiteral(":/logo/doc-icons/other.ico"));
        }
        if (pixmap.isNull())
        {
            pixmap = buildDocBadge(QStringLiteral("DOC"), QColor(96, 125, 139));
        }
        docIconCache_.insert(cacheKey, pixmap);
        return pixmap;
    }
    QString docIconResourcePath(const QString &suffix) const
    {
        const QString lower = suffix.toLower();
        static const QHash<QString, QString> iconMap = {
            {QStringLiteral("ckt"), QStringLiteral(":/logo/doc-icons/ckt.ico")},
            {QStringLiteral("dbt"), QStringLiteral(":/logo/doc-icons/dbt.ico")},
            {QStringLiteral("doc"), QStringLiteral(":/logo/doc-icons/doc.ico")},
            {QStringLiteral("docx"), QStringLiteral(":/logo/doc-icons/docx.ico")},
            {QStringLiteral("docm"), QStringLiteral(":/logo/doc-icons/docx.ico")},
            {QStringLiteral("dot"), QStringLiteral(":/logo/doc-icons/dot.ico")},
            {QStringLiteral("dotx"), QStringLiteral(":/logo/doc-icons/dot.ico")},
            {QStringLiteral("dps"), QStringLiteral(":/logo/doc-icons/dps.ico")},
            {QStringLiteral("dpt"), QStringLiteral(":/logo/doc-icons/dpt.ico")},
            {QStringLiteral("epub"), QStringLiteral(":/logo/doc-icons/e-book.ico")},
            {QStringLiteral("mobi"), QStringLiteral(":/logo/doc-icons/e-book.ico")},
            {QStringLiteral("azw"), QStringLiteral(":/logo/doc-icons/e-book.ico")},
            {QStringLiteral("azw3"), QStringLiteral(":/logo/doc-icons/e-book.ico")},
            {QStringLiteral("et"), QStringLiteral(":/logo/doc-icons/et.ico")},
            {QStringLiteral("ett"), QStringLiteral(":/logo/doc-icons/ett.ico")},
            {QStringLiteral("form"), QStringLiteral(":/logo/doc-icons/form.ico")},
            {QStringLiteral("html"), QStringLiteral(":/logo/doc-icons/html.ico")},
            {QStringLiteral("htm"), QStringLiteral(":/logo/doc-icons/html.ico")},
            {QStringLiteral("ksheet"), QStringLiteral(":/logo/doc-icons/ksheet.ico")},
            {QStringLiteral("csv"), QStringLiteral(":/logo/doc-icons/ksheet.ico")},
            {QStringLiteral("tsv"), QStringLiteral(":/logo/doc-icons/ksheet.ico")},
            {QStringLiteral("kw"), QStringLiteral(":/logo/doc-icons/kw.ico")},
            {QStringLiteral("ofd"), QStringLiteral(":/logo/doc-icons/ofd.ico")},
            {QStringLiteral("opg"), QStringLiteral(":/logo/doc-icons/opg.ico")},
            {QStringLiteral("otl"), QStringLiteral(":/logo/doc-icons/otl.ico")},
            {QStringLiteral("pdf"), QStringLiteral(":/logo/doc-icons/pdf.ico")},
            {QStringLiteral("pot"), QStringLiteral(":/logo/doc-icons/pot.ico")},
            {QStringLiteral("potx"), QStringLiteral(":/logo/doc-icons/pot.ico")},
            {QStringLiteral("ppt"), QStringLiteral(":/logo/doc-icons/ppt.ico")},
            {QStringLiteral("pptx"), QStringLiteral(":/logo/doc-icons/pptx.ico")},
            {QStringLiteral("pptm"), QStringLiteral(":/logo/doc-icons/pptx.ico")},
            {QStringLiteral("processon_flow"), QStringLiteral(":/logo/doc-icons/processon_flow.ico")},
            {QStringLiteral("flow"), QStringLiteral(":/logo/doc-icons/processon_flow.ico")},
            {QStringLiteral("processon_mind"), QStringLiteral(":/logo/doc-icons/processon_mind.ico")},
            {QStringLiteral("mind"), QStringLiteral(":/logo/doc-icons/processon_mind.ico")},
            {QStringLiteral("resh"), QStringLiteral(":/logo/doc-icons/resh.ico")},
            {QStringLiteral("txt"), QStringLiteral(":/logo/doc-icons/txt.ico")},
            {QStringLiteral("log"), QStringLiteral(":/logo/doc-icons/txt.ico")},
            {QStringLiteral("md"), QStringLiteral(":/logo/doc-icons/txt.ico")},
            {QStringLiteral("markdown"), QStringLiteral(":/logo/doc-icons/txt.ico")},
            {QStringLiteral("uot"), QStringLiteral(":/logo/doc-icons/uot.ico")},
            {QStringLiteral("wps"), QStringLiteral(":/logo/doc-icons/wps.ico")},
            {QStringLiteral("wpsnote"), QStringLiteral(":/logo/doc-icons/wpsnote.ico")},
            {QStringLiteral("wpt"), QStringLiteral(":/logo/doc-icons/wpt.ico")},
            {QStringLiteral("xls"), QStringLiteral(":/logo/doc-icons/xls.ico")},
            {QStringLiteral("xlsx"), QStringLiteral(":/logo/doc-icons/xlsx.ico")},
            {QStringLiteral("xlsm"), QStringLiteral(":/logo/doc-icons/xlsx.ico")},
            {QStringLiteral("xlt"), QStringLiteral(":/logo/doc-icons/xlt.ico")},
            {QStringLiteral("xltx"), QStringLiteral(":/logo/doc-icons/xlt.ico")}
        };
        const QString defaultIcon = QStringLiteral(":/logo/doc-icons/other.ico");
        const auto it = iconMap.constFind(lower);
        if (it != iconMap.constEnd()) return it.value();
        if (lower.isEmpty()) return defaultIcon;
        if (wordExtensions().contains(lower)) return QStringLiteral(":/logo/doc-icons/doc.ico");
        if (presentationExtensions().contains(lower)) return QStringLiteral(":/logo/doc-icons/ppt.ico");
        if (sheetExtensions().contains(lower)) return QStringLiteral(":/logo/doc-icons/xls.ico");
        if (markdownExtensions().contains(lower) || textExtensions().contains(lower)) return QStringLiteral(":/logo/doc-icons/txt.ico");
        if (htmlExtensions().contains(lower)) return QStringLiteral(":/logo/doc-icons/html.ico");
        if (codeExtensions().contains(lower) || configExtensions().contains(lower)) return QStringLiteral(":/logo/doc-icons/txt.ico");
        return defaultIcon;
    }
    QString attachmentDisplayName(const QString &path) const
    {
        QFileInfo info(path);
        if (!info.fileName().isEmpty()) return info.fileName();
        return path;
    }
    QString elidedAttachmentName(const QString &path) const
    {
        const QString name = attachmentDisplayName(path);
        QFontMetrics fm(font());
        return fm.elidedText(name, Qt::ElideMiddle, THUMBNAIL_TEXT_WIDTH);
    }
    QPixmap buildDocBadge(const QString &label, const QColor &bg) const
    {
        QPixmap pixmap(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QRect rect = pixmap.rect().adjusted(2, 2, -2, -2);
        painter.setBrush(bg);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect, 4, 4);
        QFont font = painter.font();
        font.setBold(true);
        const int length = label.size();
        const qreal divisor = (length <= 3) ? 2.2 : 2.8;
        font.setPointSizeF(qMax<qreal>(8.0, THUMBNAIL_SIZE / divisor));
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(rect, Qt::AlignCenter, label);
        painter.end();
        return pixmap;
    }
    static const QSet<QString> &imageExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                           QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp")};
        return exts;
    }
    static const QSet<QString> &audioExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("flac")};
        return exts;
    }
    static const QSet<QString> &textExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("txt"), QStringLiteral("log"), QStringLiteral("text")};
        return exts;
    }
    static const QSet<QString> &wordExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("doc"),  QStringLiteral("docx"), QStringLiteral("docm"), QStringLiteral("dot"),
                                           QStringLiteral("dotx"), QStringLiteral("dotm"), QStringLiteral("odt"),  QStringLiteral("rtf"),
                                           QStringLiteral("wps"),  QStringLiteral("wpt"),  QStringLiteral("kw"),   QStringLiteral("uot")};
        return exts;
    }
    static const QSet<QString> &presentationExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("ppt"),  QStringLiteral("pptx"), QStringLiteral("pptm"), QStringLiteral("odp"),
                                           QStringLiteral("pps"),  QStringLiteral("ppsx"), QStringLiteral("pot"),  QStringLiteral("potx"),
                                           QStringLiteral("dps"),  QStringLiteral("dpt")};
        return exts;
    }
    static const QSet<QString> &sheetExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("xls"),  QStringLiteral("xlsx"), QStringLiteral("xlsm"), QStringLiteral("xlt"),
                                           QStringLiteral("xltx"), QStringLiteral("ods"),  QStringLiteral("csv"),  QStringLiteral("tsv"),
                                           QStringLiteral("et"),   QStringLiteral("ett"),  QStringLiteral("ksheet")};
        return exts;
    }
    static const QSet<QString> &markdownExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("mdown"), QStringLiteral("mkd")};
        return exts;
    }
    static const QSet<QString> &htmlExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("html"), QStringLiteral("htm")};
        return exts;
    }
    static const QSet<QString> &codeExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("c"),
                                           QStringLiteral("h"),   QStringLiteral("hpp"), QStringLiteral("py"),
                                           QStringLiteral("js"),  QStringLiteral("ts"),  QStringLiteral("css")};
        return exts;
    }
    static const QSet<QString> &configExtensions()
    {
        static const QSet<QString> exts = {QStringLiteral("json"), QStringLiteral("ini"), QStringLiteral("cfg"),
                                           QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("toml")};
        return exts;
    }
    static const QSet<QString> &documentExtensions()
    {
        static const QSet<QString> exts = []()
        {
            QSet<QString> all = textExtensions();
            all.unite(wordExtensions());
            all.unite(presentationExtensions());
            all.unite(sheetExtensions());
            all.unite(markdownExtensions());
            all.unite(htmlExtensions());
            all.unite(codeExtensions());
            all.unite(configExtensions());
            all.insert(QStringLiteral("pdf"));
            return all;
        }();
        return exts;
    }
    void setupUI()
    {
        mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // 缩略图滚动区域
        scrollArea = new QScrollArea;
        scrollArea->setObjectName(QStringLiteral("imageInputPreviewScroll"));
        scrollArea->setWidgetResizable(true);
        const int previewHeight = THUMBNAIL_SIZE + 20;
        scrollArea->setMinimumHeight(previewHeight);
        scrollArea->setMaximumHeight(previewHeight);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setFrameShadow(QFrame::Plain);
        scrollArea->viewport()->setAttribute(Qt::WA_StyledBackground, true);

        // 缩略图容器
        thumbnailContainer = new QWidget;
        thumbnailContainer->setObjectName(QStringLiteral("imageInputPreviewContainer"));
        thumbnailContainer->setAttribute(Qt::WA_StyledBackground, true);

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
        int itemWidth = THUMBNAIL_TEXT_WIDTH + thumbnailLayout->horizontalSpacing();
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
        int rowHeight = THUMBNAIL_SIZE + 35 + thumbnailLayout->verticalSpacing();
        int totalHeight = rows * rowHeight + thumbnailLayout->contentsMargins().top() + thumbnailLayout->contentsMargins().bottom();
        thumbnailContainer->setMinimumHeight(totalHeight);

        scrollArea->setVisible(!widgets.isEmpty());
    }
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (hasSupportedUrls(event->mimeData()))
        {
            event->acceptProposedAction();
            updateDragHighlight(true);
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
            updateDragHighlight(false);
        }
        else
        {
            updateDragHighlight(false);
        }
    }
    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        Q_UNUSED(event);
        updateDragHighlight(false);
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

        // 计算新的高度，考虑边框和内边距
        int baseHeight = static_cast<int>(docSize.height()) + textEdit->frameWidth() * 2 + textEdit->contentsMargins().top() + textEdit->contentsMargins().bottom();

        // 限制高度在最小和最大值之间
        baseHeight = qBound(m_minHeight, baseHeight, m_maxHeight);

        // 设置新的固定高度
        textEdit->setFixedHeight(baseHeight);
        int extra = (scrollArea->isVisible() ? scrollArea->minimumHeight() : 0);
        setFixedHeight(baseHeight + extra);
    }
};

#endif // IMAGEINPUTBOX_H
