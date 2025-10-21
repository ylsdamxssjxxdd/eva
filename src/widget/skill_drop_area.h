#pragma once

#include <QWidget>
#include <QHash>
#include <QSet>

#include "../skill/skill_manager.h"

class QVBoxLayout;
class QLabel;
class QFrame;
class ToggleSwitch;

class SkillDropArea : public QWidget
{
    Q_OBJECT
public:
    explicit SkillDropArea(QWidget *parent = nullptr);

    void setSkills(const QVector<SkillManager::SkillRecord> &skills);

signals:
    void skillDropRequested(const QStringList &paths);
    void skillToggleRequested(const QString &skillId, bool enabled);
    void skillRemoveRequested(const QString &skillId);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct CardWidgets
    {
        QFrame *frame = nullptr;
        QLabel *title = nullptr;
        ToggleSwitch *toggle = nullptr;
    };

    void ensureEmptyHint();
    void createOrUpdateCard(const SkillManager::SkillRecord &rec);
    void removeObsoleteCards(const QSet<QString> &remaining);
    void emitToggleForCard(const QString &skillId, ToggleSwitch *toggle);
    QString skillIdFromWidget(QWidget *child) const;
    QString metadataTooltip(const SkillManager::SkillRecord &rec) const;

    QVBoxLayout *cardsLayout_ = nullptr;
    QLabel *emptyHintLabel_ = nullptr;
    QHash<QString, CardWidgets> cards_;
};
