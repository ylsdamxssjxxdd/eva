#include "elidelabel.h"

#include <QEvent>
#include <QResizeEvent>
#include <QtGlobal>

ElideLabel::ElideLabel(QWidget *parent)
    : QLabel(parent)
{
    // 标签用于填满可用空间，避免以文本长度作为最小宽度
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumWidth(0);
    setWordWrap(false);
}

void ElideLabel::setContentText(const QString &text)
{
    fullText_ = text;
    applyElide();
}

void ElideLabel::setElideMode(Qt::TextElideMode mode)
{
    elideMode_ = mode;
    applyElide();
}

void ElideLabel::refreshElide()
{
    applyElide();
}

void ElideLabel::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    applyElide();
}

void ElideLabel::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    if (event && (event->type() == QEvent::FontChange || event->type() == QEvent::ContentsRectChange))
    {
        // 字体或内容区域变化时重新计算省略结果，保持展示一致
        applyElide();
    }
}

QSize ElideLabel::minimumSizeHint() const
{
    // 宽度返回0以允许布局自由压缩，避免因文本过长撑开父布局
    QSize size = QLabel::minimumSizeHint();
    size.setWidth(0);
    return size;
}

void ElideLabel::applyElide()
{
    const int margins = contentsMargins().left() + contentsMargins().right();
    const int availableWidth = qMax(0, width() - margins);
    const QString source = fullText_.isEmpty() ? QLabel::text() : fullText_;
    const QString elided = fontMetrics().elidedText(source, elideMode_, availableWidth);
    QLabel::setText(elided);
}
