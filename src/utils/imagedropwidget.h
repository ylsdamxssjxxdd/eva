#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QContextMenuEvent>
#include <QMenu>

// Simple drop/click image upload area with preview and clear button.
// - Click to open file dialog; drag-and-drop supported
// - Right-click to clear; top-right clear button appears when image is set
class ImageDropWidget : public QWidget {
public:
    explicit ImageDropWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setAcceptDrops(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(150);
        setAutoFillBackground(true);

        QVBoxLayout* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->setSpacing(4);

        preview_ = new QLabel(this);
        preview_->setAlignment(Qt::AlignCenter);
        preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        preview_->setScaledContents(false);

        hint_ = new QLabel(tr("Click or drop image here"), this);
        hint_->setAlignment(Qt::AlignCenter);
        hint_->setStyleSheet("color:#777; font-size:12px;");

        lay->addWidget(preview_, /*stretch*/1);
        lay->addWidget(hint_);

        clearBtn_ = new QPushButton("x", this);
        clearBtn_->setFixedSize(20,20);
        clearBtn_->setToolTip(tr("Clear image"));
        clearBtn_->setStyleSheet("QPushButton{background:rgba(0,0,0,120);color:#fff;border:none;border-radius:10px;} QPushButton:hover{background:rgba(30,30,30,160);} ");
        clearBtn_->hide();
        QObject::connect(clearBtn_, &QPushButton::clicked, this, [this]{ clear(); });

        updateVisual();
    }

    QString imagePath() const { return imagePath_; }
    QImage image() const { return image_; }
    bool hasImage() const { return !image_.isNull(); }

    void setPlaceholderText(const QString& t) { hint_->setText(t); }

    void clear() {
        image_ = QImage();
        imagePath_.clear();
        updateVisual();
    }

    void setImageFromFile(const QString& path) {
        if (path.isEmpty()) { clear(); return; }
        QImage img(path);
        if (img.isNull()) return; // ignore invalid
        image_ = img;
        imagePath_ = QFileInfo(path).absoluteFilePath();
        updateVisual();
    }

protected:
    void dragEnterEvent(QDragEnterEvent* e) override {
        if (accepts(e->mimeData())) { e->acceptProposedAction(); hover_ = true; update(); }
    }
    void dragLeaveEvent(QDragLeaveEvent* e) override { Q_UNUSED(e); hover_ = false; update(); }
    void dropEvent(QDropEvent* e) override {
        const QMimeData* m = e->mimeData(); hover_ = false; update();
        if (m->hasUrls()) {
            for (const QUrl& u : m->urls()) {
                const QString p = u.toLocalFile();
                if (isImageFile(p)) { setImageFromFile(p); break; }
            }
        } else if (m->hasImage()) {
            QImage img = qvariant_cast<QImage>(m->imageData());
            if (!img.isNull()) { image_ = img; imagePath_.clear(); updateVisual(); }
        }
        e->acceptProposedAction();
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            QString filter = tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)");
            QString p = QFileDialog::getOpenFileName(this, tr("Select image"), QString(), filter);
            if (!p.isEmpty()) setImageFromFile(p);
        }
        QWidget::mousePressEvent(e);
    }
    void resizeEvent(QResizeEvent* e) override { QWidget::resizeEvent(e); updatePreviewPixmap(); updateClearButton(); }
    void paintEvent(QPaintEvent* ev) override {
        QWidget::paintEvent(ev);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = palette().base().color();
        if (bg.lightness() > 128) { bg = bg.lighter(102); } else { bg = bg.darker(102); }
        if (hover_) bg = QColor(200, 220, 255);
        p.fillRect(rect(), bg);
        QPen pen(hover_ ? QColor(45, 120, 220) : QColor(170, 170, 170));
        pen.setStyle(Qt::DashLine); pen.setWidth(1);
        p.setPen(pen);
        QRect r = rect().adjusted(2, 2, -2, -2);
        p.drawRoundedRect(r, 6, 6);
        updateClearButton();
    }
    void contextMenuEvent(QContextMenuEvent* e) override {
        QMenu m(this);
        QAction* actClear = m.addAction(tr("Clear image"));
        QAction* sel = m.exec(e->globalPos());
        if (sel == actClear) clear();
    }

private:
    bool accepts(const QMimeData* m) const {
        if (m->hasUrls()) { for (const QUrl& u : m->urls()) if (isImageFile(u.toLocalFile())) return true; }
        if (m->hasImage()) return true;
        return false;
    }
    static bool isImageFile(const QString& path) {
        const QString ext = QFileInfo(path).suffix().toLower();
        return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif" || ext == "webp";
    }
    void updatePreviewPixmap() {
        if (image_.isNull()) { preview_->clear(); return; }
        QPixmap px = QPixmap::fromImage(image_);
        const QSize avail = size() - QSize(16, 28); // margins + hint
        if (avail.width() > 16 && avail.height() > 16) {
            QSize target = px.size();
            target.scale(avail, Qt::KeepAspectRatio);
            preview_->setPixmap(px.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            preview_->setPixmap(px);
        }
    }
    void updateClearButton() {
        if (!clearBtn_) return;
        clearBtn_->setVisible(hasImage());
        int x = width() - clearBtn_->width() - 6;
        int y = 6;
        clearBtn_->move(x, y);
        clearBtn_->raise();
    }
    void updateVisual() {
        const bool has = !image_.isNull();
        hint_->setVisible(!has);
        updatePreviewPixmap();
        updateClearButton();
        update();
    }

private:
    QLabel* preview_ = nullptr;
    QLabel* hint_ = nullptr;
    QImage image_;
    QString imagePath_;
    bool hover_ = false;
    QPushButton* clearBtn_ = nullptr;
};