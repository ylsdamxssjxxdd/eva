// mediaresultwidget.h - Simple media result view for images and videos
// Qt 5.15, C++17
// This widget replaces the QTextEdit-based result area so we can show videos (AVI/MP4) inline.

#ifndef MEDIARESULTWIDGET_H
#define MEDIARESULTWIDGET_H

#include <QScrollArea>
#include <QPointer>
#include <QVBoxLayout>
#include <QLabel>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QToolButton>
#include <QStyle>
#include <QFileInfo>
#include <QScrollBar>
#include <QTimer>

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
        title_ = new QLabel(QFileInfo(path).fileName(), this);
        title_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        title_->setStyleSheet("color:#888");
        view_ = new QLabel(this);
        view_->setAlignment(Qt::AlignCenter);
        view_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        lay->addWidget(title_);
        lay->addWidget(view_);
        QPixmap pm(path);
        view_->setPixmap(pm);
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
        title_ = new QLabel(QFileInfo(path).fileName(), this);
        title_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        title_->setStyleSheet("color:#888");
        videoWidget_ = new QVideoWidget(this);
        videoWidget_->setMinimumHeight(240);
        player_ = new QMediaPlayer(this);
        player_->setVideoOutput(videoWidget_);
        player_->setMedia(QUrl::fromLocalFile(path));
        // Controls
        auto *ctrl = new QWidget(this);
        auto *hl = new QHBoxLayout(ctrl);
        hl->setContentsMargins(0,0,0,0);
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
        connect(playBtn, &QToolButton::clicked, this, [this]{ player_->play(); });
        connect(pauseBtn, &QToolButton::clicked, this, [this]{ player_->pause(); });
        connect(stopBtn, &QToolButton::clicked, this, [this]{ player_->stop(); });
        connect(player_, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State s){
            switch (s) { case QMediaPlayer::PlayingState: status_->setText("playing"); break; case QMediaPlayer::PausedState: status_->setText("paused"); break; default: status_->setText("stopped"); break; }
        });
        lay->addWidget(title_);
        lay->addWidget(videoWidget_);
        lay->addWidget(ctrl);
        setLayout(lay);
    }
    ~VideoItem() override { if (player_) player_->stop(); }
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
        layout_->setContentsMargins(0,0,0,0);
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
        QTimer::singleShot(0, this, [this]{ verticalScrollBar()->setValue(verticalScrollBar()->maximum()); });
    }
  private:
    QWidget *content_ = nullptr;
    QVBoxLayout *layout_ = nullptr;
};

#endif // MEDIARESULTWIDGET_H
