// mediaresultwidget.h - Simple media result view for images and videos
// Qt 5.15, C++17
// This widget replaces the QTextEdit-based result area so we can show videos (AVI/MP4) inline.

#ifndef MEDIARESULTWIDGET_H
#define MEDIARESULTWIDGET_H

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <QVideoWidget>

// Lightweight item widget for image display
class ImageItem : public QWidget
{
  public:
    explicit ImageItem(const QString &path, QWidget *parent = nullptr)
        : QWidget(parent), path_(path)
    {
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(6, 6, 6, 6);
        lay->setSpacing(4);
        // Title bar with close button
        auto *bar = new QWidget(this);
        auto *barLay = new QHBoxLayout(bar);
        barLay->setContentsMargins(0, 0, 0, 0);
        barLay->setSpacing(4);
        title_ = new QLabel(QFileInfo(path).fileName(), bar);
        title_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        title_->setStyleSheet("color:#888");
        auto *closeBtn = new QToolButton(bar);
        closeBtn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        closeBtn->setAutoRaise(true);
        closeBtn->setToolTip("Close");
        barLay->addWidget(title_);
        barLay->addStretch(1);
        barLay->addWidget(closeBtn);
        bar->setLayout(barLay);
        view_ = new QLabel(this);
        view_->setAlignment(Qt::AlignCenter);
        view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        lay->addWidget(bar);
        lay->addWidget(view_);
        QPixmap pm(path);
        view_->setPixmap(pm);
        connect(closeBtn, &QToolButton::clicked, this, [this]
                { this->deleteLater(); });
    }

  private:
    QString path_;
    QLabel *title_ = nullptr;
    QLabel *view_ = nullptr;
};

// Lightweight item widget for video playback (uses QtMultimediaWidgets)
class VideoItem : public QWidget
{
  public:
    explicit VideoItem(const QString &path, QWidget *parent = nullptr)
        : QWidget(parent), path_(path)
    {
        auto *lay = new QVBoxLayout(this);
        lay->setContentsMargins(6, 6, 6, 6);
        lay->setSpacing(4);
        // Title bar with close button
        auto *bar = new QWidget(this);
        auto *barLay = new QHBoxLayout(bar);
        barLay->setContentsMargins(0, 0, 0, 0);
        barLay->setSpacing(4);
        title_ = new QLabel(QFileInfo(path).fileName(), bar);
        title_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        title_->setStyleSheet("color:#888");
        auto *closeBtn = new QToolButton(bar);
        closeBtn->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        closeBtn->setAutoRaise(true);
        closeBtn->setToolTip("Close");
        barLay->addWidget(title_);
        barLay->addStretch(1);
        barLay->addWidget(closeBtn);
        bar->setLayout(barLay);
        videoWidget_ = new QVideoWidget(this);
        // Start with a reasonable minimum height; adjust to exact frame later
        videoWidget_->setMinimumHeight(240);
        videoWidget_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        player_ = new QMediaPlayer(this);
        player_->setVideoOutput(videoWidget_);
        player_->setMedia(QUrl::fromLocalFile(path));
        // Controls
        auto *ctrl = new QWidget(this);
        auto *hl = new QHBoxLayout(ctrl);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);
        auto *playBtn = new QToolButton(ctrl);
        playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        auto *pauseBtn = new QToolButton(ctrl);
        pauseBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        auto *stopBtn = new QToolButton(ctrl);
        stopBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
        status_ = new QLabel("ready", ctrl);
        status_->setStyleSheet("color:#888");
        hl->addWidget(playBtn);
        hl->addWidget(pauseBtn);
        hl->addWidget(stopBtn);
        hl->addWidget(status_, 1);
        ctrl->setLayout(hl);
        // Wire controls
        connect(playBtn, &QToolButton::clicked, this, [this]
                { player_->play(); });
        connect(pauseBtn, &QToolButton::clicked, this, [this]
                { player_->pause(); });
        connect(stopBtn, &QToolButton::clicked, this, [this]
                { player_->stop(); });
        connect(player_, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State s)
                {
            switch (s) { case QMediaPlayer::PlayingState: status_->setText("playing"); break; case QMediaPlayer::PausedState: status_->setText("paused"); break; default: status_->setText("stopped"); break; } });
        // Adjust to native video resolution when available
        auto tryAdjust = [this]()
        {
            QVariant v = player_->metaData("Resolution");
            if (v.isValid())
            {
                const QSize res = v.toSize();
                if (res.isValid() && res.width() > 0 && res.height() > 0)
                {
                    videoWidget_->setFixedSize(res);
                    this->updateGeometry();
                }
            }
        };
        connect(player_, &QMediaPlayer::mediaStatusChanged, this, [tryAdjust](QMediaPlayer::MediaStatus s)
                {
            if (s == QMediaPlayer::LoadedMedia || s == QMediaPlayer::BufferedMedia) tryAdjust(); });
        connect(player_, &QMediaPlayer::videoAvailableChanged, this, [tryAdjust](bool ok)
                { if (ok) tryAdjust(); });
        QTimer::singleShot(200, this, [tryAdjust]
                           { tryAdjust(); });
        lay->addWidget(bar);
        lay->addWidget(videoWidget_);
        lay->addWidget(ctrl);
        setLayout(lay);
        connect(closeBtn, &QToolButton::clicked, this, [this]
                { if (player_) player_->stop(); this->deleteLater(); });
    }
    ~VideoItem() override
    {
        if (player_) player_->stop();
    }

  private:
    QString path_;
    QLabel *title_ = nullptr;
    QLabel *status_ = nullptr;
    QVideoWidget *videoWidget_ = nullptr;
    QMediaPlayer *player_ = nullptr;
};

// Scrollable container that stacks media items vertically
class MediaResultWidget : public QScrollArea
{
  public:
    explicit MediaResultWidget(QWidget *parent = nullptr)
        : QScrollArea(parent)
    {
        setWidgetResizable(true);
        content_ = new QWidget(this);
        layout_ = new QVBoxLayout(content_);
        layout_->setContentsMargins(0, 0, 0, 0);
        layout_->setSpacing(8);
        content_->setLayout(layout_);
        setWidget(content_);
    }
    void addImage(const QString &path)
    {
        layout_->addWidget(new ImageItem(path, content_));
        ensureVisibleBottom();
    }
    void addVideo(const QString &path)
    {
        layout_->addWidget(new VideoItem(path, content_));
        ensureVisibleBottom();
    }
    void ensureVisibleBottom()
    {
        QTimer::singleShot(0, this, [this]
                           { verticalScrollBar()->setValue(verticalScrollBar()->maximum()); });
    }

  private:
    QWidget *content_ = nullptr;
    QVBoxLayout *layout_ = nullptr;
};

#endif // MEDIARESULTWIDGET_H
