#include "skill_drop_area.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QHBoxLayout>
#include <QSet>
#include <QStringList>

#include "../utils/toggleswitch.h"

namespace
{
bool isZipFile(const QUrl &url)
{
    if (!url.isLocalFile()) return false;
    const QString path = url.toLocalFile();
    return path.endsWith(".zip", Qt::CaseInsensitive);
}

} // namespace

SkillDropArea::SkillDropArea(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(QStringLiteral(
        "#skillCard {"
        "  border: 1px solid rgba(76, 177, 255, 0.65);"
        "  background-color: rgba(33, 150, 243, 0.30);"
        "  border-radius: 12px;"
        "}"
        "#skillCard:hover {"
        "  border: 1px solid rgba(129, 212, 250, 0.95);"
        "  background-color: rgba(41, 121, 255, 0.42);"
        "}"
        "#skillTitle {"
        "  font-weight: 600;"
        "  color: #E3F2FD;"
        "  letter-spacing: 0.4px;"
        "}"
        "#skillEmptyHint {"
        "  color: rgba(227, 242, 253, 0.7);"
        "  font-style: italic;"
        "}"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(10);

    emptyHintLabel_ = new QLabel(tr("暂无技能。拖拽技能压缩包到此区域完成挂载。"), this);
    emptyHintLabel_->setAlignment(Qt::AlignCenter);
    emptyHintLabel_->setObjectName(QStringLiteral("skillEmptyHint"));
    emptyHintLabel_->setStyleSheet(QStringLiteral("color: rgba(227,242,253,0.7); font-style: italic;"));
    rootLayout->addWidget(emptyHintLabel_);

    QWidget *cardsContainer = new QWidget(this);
    cardsLayout_ = new QVBoxLayout(cardsContainer);
    cardsLayout_->setContentsMargins(0, 0, 0, 0);
    cardsLayout_->setSpacing(12);
    cardsLayout_->setAlignment(Qt::AlignTop);
    rootLayout->addWidget(cardsContainer);
    rootLayout->addStretch();

    ensureEmptyHint();
}

void SkillDropArea::setSkills(const QVector<SkillManager::SkillRecord> &skills)
{
    QSet<QString> remaining;
    remaining.reserve(skills.size());

    for (const auto &rec : skills)
    {
        remaining.insert(rec.id);
        createOrUpdateCard(rec);
    }

    removeObsoleteCards(remaining);
    ensureEmptyHint();
}

void SkillDropArea::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        for (const QUrl &url : event->mimeData()->urls())
        {
            if (isZipFile(url))
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void SkillDropArea::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        for (const QUrl &url : event->mimeData()->urls())
        {
            if (isZipFile(url))
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void SkillDropArea::dropEvent(QDropEvent *event)
{
    QStringList paths;
    if (event->mimeData()->hasUrls())
    {
        for (const QUrl &url : event->mimeData()->urls())
        {
            if (isZipFile(url))
            {
                paths << url.toLocalFile();
            }
        }
    }

    if (!paths.isEmpty())
    {
        event->acceptProposedAction();
        emit skillDropRequested(paths);
    }
    else
    {
        event->ignore();
    }
}

void SkillDropArea::contextMenuEvent(QContextMenuEvent *event)
{
    const QString skillId = skillIdFromWidget(childAt(event->pos()));
    if (skillId.isEmpty())
    {
        QWidget::contextMenuEvent(event);
        return;
    }

    auto it = cards_.find(skillId);
    if (it == cards_.end())
    {
        QWidget::contextMenuEvent(event);
        return;
    }

    CardWidgets &card = it.value();
    QMenu menu(this);
    QAction *toggleAction = menu.addAction(card.toggle->isChecked() ? tr("Disable Skill") : tr("Enable Skill"));
    QAction *removeAction = menu.addAction(tr("Remove Skill"));

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) return;
    if (chosen == toggleAction)
    {
        QSignalBlocker blocker(card.toggle);
        card.toggle->setChecked(!card.toggle->isChecked());
        emitToggleForCard(skillId, card.toggle);
    }
    else if (chosen == removeAction)
    {
        emit skillRemoveRequested(skillId);
    }
}

void SkillDropArea::ensureEmptyHint()
{
    const bool empty = cards_.isEmpty();
    if (emptyHintLabel_) emptyHintLabel_->setVisible(empty);
}

void SkillDropArea::createOrUpdateCard(const SkillManager::SkillRecord &rec)
{
    auto it = cards_.find(rec.id);
    if (it != cards_.end())
    {
        CardWidgets &card = it.value();
        card.title->setText(rec.id);
        const QString tooltip = metadataTooltip(rec);
        card.frame->setToolTip(tooltip);
        card.title->setToolTip(tooltip);
        card.toggle->setToolTip(tooltip);
        if (card.toggle->isChecked() != rec.enabled)
        {
            QSignalBlocker blocker(card.toggle);
            card.toggle->setChecked(rec.enabled);
        }
        card.toggle->setHandlePosition(rec.enabled ? 1.0 : 0.0);
        return;
    }

    CardWidgets card;
    card.frame = new QFrame(this);
    card.frame->setObjectName(QStringLiteral("skillCard"));
    card.frame->setProperty("skillId", rec.id);

    auto *cardLayout = new QHBoxLayout(card.frame);
    cardLayout->setContentsMargins(18, 14, 18, 14);
    cardLayout->setSpacing(14);

    card.title = new QLabel(rec.id, card.frame);
    card.title->setObjectName(QStringLiteral("skillTitle"));
    cardLayout->addWidget(card.title, 1, Qt::AlignVCenter);

    card.toggle = new ToggleSwitch(card.frame);
    card.toggle->setChecked(rec.enabled);
    card.toggle->setHandlePosition(rec.enabled ? 1.0 : 0.0);
    cardLayout->addWidget(card.toggle, 0, Qt::AlignVCenter);

    const QString tooltip = metadataTooltip(rec);
    card.frame->setToolTip(tooltip);
    card.title->setToolTip(tooltip);
    card.toggle->setToolTip(tooltip);

    connect(card.toggle, &QAbstractButton::toggled, this, [this, skillId = rec.id](bool checked)
            { emit skillToggleRequested(skillId, checked); });

    cardsLayout_->addWidget(card.frame);
    cards_.insert(rec.id, card);
}

void SkillDropArea::removeObsoleteCards(const QSet<QString> &remaining)
{
    auto it = cards_.begin();
    while (it != cards_.end())
    {
        if (!remaining.contains(it.key()))
        {
            CardWidgets card = it.value();
            cardsLayout_->removeWidget(card.frame);
            card.frame->deleteLater();
            it = cards_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SkillDropArea::emitToggleForCard(const QString &skillId, ToggleSwitch *toggle)
{
    if (!toggle) return;
    emit skillToggleRequested(skillId, toggle->isChecked());
}

QString SkillDropArea::skillIdFromWidget(QWidget *child) const
{
    QWidget *current = child;
    while (current)
    {
        const QVariant value = current->property("skillId");
        if (value.isValid()) return value.toString();
        current = current->parentWidget();
    }
    return {};
}

QString SkillDropArea::metadataTooltip(const SkillManager::SkillRecord &rec) const
{
    QStringList parts;
    parts << tr("Skill: %1").arg(rec.id);
    if (!rec.description.isEmpty()) parts << tr("Description: %1").arg(rec.description);
    if (!rec.license.isEmpty()) parts << tr("License: %1").arg(rec.license);
    if (!rec.frontmatterBody.isEmpty())
    {
        parts << tr("Metadata:");
        parts << rec.frontmatterBody.trimmed();
    }
    return parts.join(QStringLiteral("\n"));
}
