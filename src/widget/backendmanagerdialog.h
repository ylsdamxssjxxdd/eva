#ifndef EVA_BACKEND_MANAGER_DIALOG_H
#define EVA_BACKEND_MANAGER_DIALOG_H

#include <QDialog>
#include <QMap>
#include <QVector>
#include <functional>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QPushButton;

class BackendManagerDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit BackendManagerDialog(std::function<QString(const QString &)> translator,
                                  std::function<QMap<QString, QString>()> overridesProvider,
                                  std::function<void(const QString &, const QString &)> overrideSetter,
                                  std::function<void(const QString &)> overrideClearer,
                                  QWidget *parent = nullptr);
    void refresh();
    void focusRole(const QString &roleId);
    void refreshTranslations();

  signals:
    void overridesChanged();

  private:
    struct RoleInfo
    {
        QString id;
        QString label;
        QString binary;
    };

    void buildUi();
    void populateRoles();
    void rebuildTree();
    void rebuildTreeForRole(const QString &roleId);
    void updateCurrentOverrideLabel();
    void updateButtons();
    void applyOverride(const QString &path);
    void selectItemByPath(const QString &path);
    QString selectedExecutablePath() const;
    QString currentRoleId() const;

    void handleSelectionChanged();
    void handleDoubleClick(QTreeWidgetItem *item, int column);
    void handleUseSelected();
    void handleAdd();
    void handleDelete();
    void handleReset();
    QString trKey(const QString &key, const QString &fallback = QString()) const;

    QLabel *infoLabel_ = nullptr;
    QLabel *currentOverrideLabel_ = nullptr;
    QComboBox *roleCombo_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QPushButton *useButton_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *resetButton_ = nullptr;
    QPushButton *closeButton_ = nullptr;

    QVector<RoleInfo> roles_;
    std::function<QString(const QString &)> translator_;
    std::function<QMap<QString, QString>()> overridesProvider_;
    std::function<void(const QString &, const QString &)> overrideSetter_;
    std::function<void(const QString &)> overrideClearer_;
};

#endif // EVA_BACKEND_MANAGER_DIALOG_H
