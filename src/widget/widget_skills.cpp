#include "widget.h"
#include "ui_widget.h"

void Widget::refreshSkillsUI()
{
    if (!date_ui || !date_ui->skills_list || !skillManager) return;
    date_ui->skills_list->setSkills(skillManager->skills());
}

void Widget::rebuildSkillPrompts()
{
    ui_extra_prompt = create_extra_prompt();
    if (date_ui) get_date();
}

void Widget::updateSkillVisibility(bool engineerEnabled)
{
    if (!date_ui) return;
    if (date_ui->skills_box) date_ui->skills_box->setVisible(engineerEnabled);
    if (date_ui->dockerSandbox_checkbox) date_ui->dockerSandbox_checkbox->setVisible(engineerEnabled);
    if (date_ui->docker_image_label) date_ui->docker_image_label->setVisible(engineerEnabled);
    if (date_ui->docker_image_comboBox) date_ui->docker_image_comboBox->setVisible(engineerEnabled);
    if (engineerEnabled)
    {
        refreshSkillsUI();
    }
    rebuildSkillPrompts();
}

void Widget::onSkillsChanged()
{
    refreshSkillsUI();
    rebuildSkillPrompts();
    if (date_ui) auto_save_user();
}

void Widget::onSkillDropRequested(const QStringList &paths)
{
    if (!skillManager) return;
    for (const QString &path : paths)
    {
        const QString displayName = QFileInfo(path).fileName();
        reflash_state(QString::fromUtf8("ui:skill import queued -> ") + displayName, SIGNAL_SIGNAL);
        skillManager->importSkillArchiveAsync(path);
    }
}

void Widget::onSkillToggleRequested(const QString &skillId, bool enabled)
{
    if (!skillManager || skillId.isEmpty()) return;
    skillManager->setSkillEnabled(skillId, enabled);
}

void Widget::onSkillRemoveRequested(const QString &skillId)
{
    if (!skillManager || skillId.isEmpty()) return;
    QString error;
    if (!skillManager->removeSkill(skillId, &error))
    {
        const QString msg = error.isEmpty() ? QStringLiteral("failed to remove skill -> ") + skillId : error;
        reflash_state(QString::fromUtf8("ui:") + msg, WRONG_SIGNAL);
    }
    else
    {
        reflash_state(QString::fromUtf8("ui:skill removed -> ") + skillId, SIGNAL_SIGNAL);
    }
    auto_save_user();
}

void Widget::restoreSkillSelection(const QStringList &skills)
{
    if (!skillManager) return;
    QSet<QString> enabled;
    for (const QString &id : skills)
    {
        enabled.insert(id.trimmed());
    }
    skillManager->restoreEnabledSet(enabled);
    if (date_ui && date_ui->engineer_checkbox)
    {
        updateSkillVisibility(date_ui->engineer_checkbox->isChecked());
    }
    else
    {
        rebuildSkillPrompts();
    }
}
