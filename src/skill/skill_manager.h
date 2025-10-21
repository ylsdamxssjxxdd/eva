#pragma once

#include <QObject>
#include <QDir>
#include <QSet>
#include <QStringList>
#include <QVector>

class SkillManager : public QObject
{
    Q_OBJECT
public:
    struct SkillRecord
    {
        QString id;
        QString description;
        QString license;
        QString frontmatterBlock;
        QString skillRootPath;
        QString skillFilePath;
        bool enabled = false;
    };

    struct ImportResult
    {
        bool ok = false;
        QString skillId;
        QString message;
    };

    explicit SkillManager(QObject *parent = nullptr);

    void setApplicationDir(const QString &appDir);
    QString applicationDir() const { return applicationDir_; }
    QString skillsRoot() const { return skillsRoot_; }

    bool loadFromDisk();
    ImportResult importSkillArchive(const QString &zipPath);
    bool removeSkill(const QString &skillId, QString *error = nullptr);
    bool setSkillEnabled(const QString &skillId, bool enabled);
    void restoreEnabledSet(const QSet<QString> &enabled);
    QStringList enabledSkillIds() const;

    const QVector<SkillRecord> &skills() const { return skills_; }

    QString composePromptBlock(const QString &engineerWorkDir, bool engineerActive) const;
    QString composeEngineerAppendix(const QString &engineerWorkDir, bool engineerActive) const;

signals:
    void skillsChanged();
    void skillImported(const QString &skillId);
    void skillOperationFailed(const QString &message);

private:
    QString applicationDir_;
    QString skillsRoot_;
    QVector<SkillRecord> skills_;

    bool ensureSkillsRoot();
    bool loadSkillFromDirectory(const QString &skillDir, SkillRecord &record, QString *error = nullptr) const;
    static QString normalizeFrontmatter(const QString &rawContent, QString *frontmatterBody = nullptr);
    static QString extractYamlScalar(const QString &frontmatter, const QString &key);
    static QString sanitizePathForCommand(const QString &path);
    static bool copyDirectory(const QString &sourceDir, const QString &targetDir, QString *error = nullptr);
    static bool removeDirectory(const QString &path, QString *error = nullptr);
    bool extractArchive(const QString &zipPath, const QString &destination, QString *error = nullptr) const;
    QString relativeToWorkdir(const QString &engineerWorkDir, const QString &path) const;
    void sortSkills();
};

