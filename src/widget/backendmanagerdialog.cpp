#include "backendmanagerdialog.h"

#include "../utils/devicemanager.h"
#include "../xconfig.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <utility>

BackendManagerDialog::BackendManagerDialog(std::function<QString(const QString &)> translator,
                                           std::function<QMap<QString, QString>()> overridesProvider,
                                           std::function<void(const QString &, const QString &)> overrideSetter,
                                           std::function<void(const QString &)> overrideClearer,
                                           QWidget *parent)
    : QDialog(parent), translator_(std::move(translator)), overridesProvider_(std::move(overridesProvider)),
      overrideSetter_(std::move(overrideSetter)), overrideClearer_(std::move(overrideClearer))
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(false);
    buildUi();
    refreshTranslations();
    populateRoles();
    if (roleCombo_->count() > 0)
    {
        roleCombo_->setCurrentIndex(0);
    }
    rebuildTree();
    updateCurrentOverrideLabel();
    updateButtons();
}

void BackendManagerDialog::buildUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    infoLabel_ = new QLabel(this);
    infoLabel_->setWordWrap(true);
    layout->addWidget(infoLabel_);

    roleCombo_ = new QComboBox(this);
    layout->addWidget(roleCombo_);

    currentOverrideLabel_ = new QLabel(this);
    layout->addWidget(currentOverrideLabel_);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderLabels(QStringList() << QString() << QString());
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(tree_, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    useButton_ = new QPushButton(this);
    addButton_ = new QPushButton(this);
    deleteButton_ = new QPushButton(this);
    buttonLayout->addWidget(useButton_);
    buttonLayout->addWidget(addButton_);
    buttonLayout->addWidget(deleteButton_);
    layout->addLayout(buttonLayout);

    connect(roleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int)
            {
                rebuildTree();
                updateCurrentOverrideLabel();
                updateButtons();
            });
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &BackendManagerDialog::handleSelectionChanged);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, &BackendManagerDialog::handleDoubleClick);
    connect(useButton_, &QPushButton::clicked, this, &BackendManagerDialog::handleUseSelected);
    connect(addButton_, &QPushButton::clicked, this, &BackendManagerDialog::handleAdd);
    connect(deleteButton_, &QPushButton::clicked, this, &BackendManagerDialog::handleDelete);
}

void BackendManagerDialog::refresh()
{
    refreshTranslations();
    const QString current = currentRoleId();
    populateRoles();
    focusRole(current);
    rebuildTree();
    updateCurrentOverrideLabel();
    updateButtons();
}

void BackendManagerDialog::focusRole(const QString &roleId)
{
    if (!roleCombo_) return;
    const QString normalized = roleId.trimmed().toLower();
    for (int i = 0; i < roleCombo_->count(); ++i)
    {
        if (roleCombo_->itemData(i).toString() == normalized)
        {
            roleCombo_->setCurrentIndex(i);
            return;
        }
    }
    if (roleCombo_->count() > 0 && roleCombo_->currentIndex() < 0)
    {
        roleCombo_->setCurrentIndex(0);
    }
}

void BackendManagerDialog::populateRoles()
{
    if (!roleCombo_) return;
    const QString previous = currentRoleId();
    roles_.clear();
    roleCombo_->blockSignals(true);
    roleCombo_->clear();
    const auto dmRoles = DeviceManager::managedRoles();
    for (const auto &role : dmRoles)
    {
        RoleInfo info;
        info.id = role.id;
        info.binary = role.binary;
        info.label = role.label.isEmpty() ? role.id : role.label;
        roles_.append(info);
        QString display = info.label;
        if (!info.binary.isEmpty())
        {
            display = QStringLiteral("%1 (%2)").arg(info.label, info.binary);
        }
        roleCombo_->addItem(display, info.id);
    }
    roleCombo_->blockSignals(false);
    if (!previous.isEmpty())
    {
        focusRole(previous);
    }
    else if (roleCombo_->count() > 0)
    {
        roleCombo_->setCurrentIndex(0);
    }
}

QString BackendManagerDialog::currentRoleId() const
{
    if (!roleCombo_ || roleCombo_->currentIndex() < 0) return QString();
    return roleCombo_->currentData().toString();
}

void BackendManagerDialog::rebuildTree()
{
    tree_->clear();
    const QString roleId = currentRoleId();
    QString binaryName;
    for (const auto &role : roles_)
    {
        if (role.id == roleId)
        {
            binaryName = role.binary;
            break;
        }
    }
    QVector<DeviceManager::BackendExecutableInfo> entries;
    if (!binaryName.isEmpty())
        entries = DeviceManager::enumerateExecutables(binaryName);
    else
        entries = DeviceManager::enumerateExecutables();

    if (entries.isEmpty())
    {
        QTreeWidgetItem *placeholder = new QTreeWidgetItem(tree_);
        placeholder->setText(0, trKey(QStringLiteral("backend manager empty"),
                                      QStringLiteral("No executable found for the current role.")));
        placeholder->setFirstColumnSpanned(true);
        placeholder->setFlags(Qt::ItemIsEnabled);
        tree_->setEnabled(false);
    }
    else
    {
        tree_->setEnabled(true);
        for (const auto &entry : entries)
        {
            QTreeWidgetItem *item = new QTreeWidgetItem(tree_);
            const QString project = entry.project.isEmpty() ? trKey(QStringLiteral("backend manager project unknown"), QStringLiteral("Unknown")) : entry.project;
            const QString deviceInfo = QStringLiteral("%1/%2/%3").arg(entry.arch, entry.os, entry.device);
            const QString rowLabel = trKey(QStringLiteral("backend manager project format"), QStringLiteral("%1 [%2]")).arg(project, deviceInfo);
            item->setText(0, rowLabel);
            item->setText(1, entry.absolutePath);
            item->setData(0, Qt::UserRole, entry.absolutePath);
        }
    }
    selectItemByPath(DeviceManager::programOverride(roleId));
}

void BackendManagerDialog::updateCurrentOverrideLabel()
{
    if (!currentOverrideLabel_) return;
    const QString role = currentRoleId();
    if (role.isEmpty())
    {
        currentOverrideLabel_->setText(trKey(QStringLiteral("backend manager current auto"), QStringLiteral("Current selection: auto")));
        return;
    }
    QString path;
    if (overridesProvider_)
    {
        const QMap<QString, QString> overrides = overridesProvider_();
        path = overrides.value(role);
    }
    if (path.isEmpty())
    {
        path = DeviceManager::programOverride(role);
    }
    if (path.isEmpty())
    {
        path = DeviceManager::programPath(role);
    }
    if (path.isEmpty())
    {
        currentOverrideLabel_->setText(trKey(QStringLiteral("backend manager current auto"), QStringLiteral("Current selection: auto")));
    }
    else
    {
        currentOverrideLabel_->setText(trKey(QStringLiteral("backend manager current path"), QStringLiteral("Current selection: %1")).arg(path));
    }
}

void BackendManagerDialog::selectItemByPath(const QString &path)
{
    tree_->blockSignals(true);
    tree_->clearSelection();
    bool matched = false;
    if (!path.isEmpty())
    {
        const QString normalized = QFileInfo(path).absoluteFilePath();
        const int count = tree_->topLevelItemCount();
        for (int i = 0; i < count; ++i)
        {
            QTreeWidgetItem *item = tree_->topLevelItem(i);
            const QString itemPath = QFileInfo(item->data(0, Qt::UserRole).toString()).absoluteFilePath();
            if (itemPath == normalized)
            {
                tree_->setCurrentItem(item);
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            QTreeWidgetItem *customItem = new QTreeWidgetItem(tree_);
            customItem->setText(0, trKey(QStringLiteral("backend manager custom entry"), QStringLiteral("Custom")));
            customItem->setText(1, normalized);
            customItem->setData(0, Qt::UserRole, normalized);
            tree_->setCurrentItem(customItem);
        }
    }
    tree_->blockSignals(false);
    updateButtons();
}

QString BackendManagerDialog::selectedExecutablePath() const
{
    const QList<QTreeWidgetItem *> selection = tree_->selectedItems();
    if (selection.isEmpty()) return QString();
    const QTreeWidgetItem *item = selection.first();
    if (item->childCount() > 0) return QString();
    return item->data(0, Qt::UserRole).toString();
}

void BackendManagerDialog::updateButtons()
{
    const bool hasSelection = !selectedExecutablePath().isEmpty();
    if (useButton_) useButton_->setEnabled(hasSelection);
    const QString role = currentRoleId();
    QString overridePath;
    if (overridesProvider_)
    {
        const QMap<QString, QString> overrides = overridesProvider_();
        overridePath = overrides.value(role);
    }
    const bool hasOverride = !overridePath.isEmpty();
    if (addButton_) addButton_->setEnabled(true);
    bool canDelete = false;
    if (deleteButton_)
    {
        if (hasOverride && !overridePath.isEmpty())
        {
            const QString selected = selectedExecutablePath();
            if (!selected.isEmpty())
            {
                const QFileInfo sel(selected);
                const QFileInfo overrideInfo(overridePath);
                canDelete = sel.absoluteFilePath() == overrideInfo.absoluteFilePath();
            }
        }
        deleteButton_->setEnabled(canDelete);
    }
}

void BackendManagerDialog::handleSelectionChanged()
{
    updateButtons();
}

void BackendManagerDialog::handleDoubleClick(QTreeWidgetItem *item, int)
{
    if (!item || item->childCount() > 0) return;
    handleUseSelected();
}

void BackendManagerDialog::handleUseSelected()
{
    const QString path = selectedExecutablePath();
    if (path.isEmpty()) return;
    applyOverride(path);
}

void BackendManagerDialog::handleAdd()
{
    const QString role = currentRoleId();
    QString startDir;
    QString current;
    if (overridesProvider_) current = overridesProvider_().value(role);
    if (!current.isEmpty())
    {
        startDir = QFileInfo(current).absolutePath();
    }
    if (startDir.isEmpty())
    {
        startDir = DeviceManager::backendsRootDir();
    }
    const QString filter = trKey(QStringLiteral("backend manager select filter"), QStringLiteral("Executables (*%1);;All files (*)"))
                               .arg(QStringLiteral(SFX_NAME));
    const QString picked = QFileDialog::getOpenFileName(this,
                                                        trKey(QStringLiteral("backend manager select title"), QStringLiteral("Select executable")),
                                                        startDir, filter);
    if (picked.isEmpty()) return;
    applyOverride(picked);
}

void BackendManagerDialog::handleDelete()
{
    const QString role = currentRoleId();
    if (role.isEmpty()) return;
    if (!overrideClearer_) return;
    QString overridePath;
    if (overridesProvider_) overridePath = overridesProvider_().value(role);
    if (overridePath.isEmpty()) return;
    const QString selected = selectedExecutablePath();
    if (selected.isEmpty()) return;
    const QFileInfo selInfo(selected);
    const QFileInfo overrideInfo(overridePath);
    if (selInfo.absoluteFilePath() != overrideInfo.absoluteFilePath()) return;
    overrideClearer_(role);
    emit overridesChanged();
    updateCurrentOverrideLabel();
    selectItemByPath(QString());
    updateButtons();
}

void BackendManagerDialog::refreshTranslations()
{
    setWindowTitle(trKey(QStringLiteral("backend manager title"), QStringLiteral("Backend Manager")));
    if (infoLabel_) infoLabel_->setText(trKey(QStringLiteral("backend manager hint"),
                                              QStringLiteral("List available executables under EVA_BACKEND and pin a custom binary per role.")));
    if (tree_) tree_->setHeaderLabels(
            QStringList() << trKey(QStringLiteral("backend manager column project"), QStringLiteral("Project"))
                          << trKey(QStringLiteral("backend manager column executable"), QStringLiteral("Executable")));
    if (useButton_) useButton_->setText(trKey(QStringLiteral("backend manager button use"), QStringLiteral("Use Selected")));
    if (addButton_) addButton_->setText(trKey(QStringLiteral("backend manager button add"), QStringLiteral("Add...")));
    if (deleteButton_) deleteButton_->setText(trKey(QStringLiteral("backend manager button delete"), QStringLiteral("Delete")));
    updateCurrentOverrideLabel();
}

void BackendManagerDialog::applyOverride(const QString &path)
{
    if (path.isEmpty()) return;
    const QString role = currentRoleId();
    if (role.isEmpty()) return;
    QFileInfo fi(path);
    if (!fi.exists())
    {
        QMessageBox::warning(this, trKey(QStringLiteral("backend manager invalid title"), QStringLiteral("Invalid executable")),
                             trKey(QStringLiteral("backend manager invalid body"), QStringLiteral("The selected executable does not exist.")));
        return;
    }
    if (overrideSetter_) overrideSetter_(role, fi.absoluteFilePath());
    emit overridesChanged();
    updateCurrentOverrideLabel();
    selectItemByPath(fi.absoluteFilePath());
    updateButtons();
}

QString BackendManagerDialog::trKey(const QString &key, const QString &fallback) const
{
    if (translator_)
    {
        const QString resolved = translator_(key);
        if (!resolved.isEmpty()) return resolved;
    }
    return fallback.isEmpty() ? key : fallback;
}
