/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_FOLDERWIZARD_H
#define MIRALL_FOLDERWIZARD_H

#include <QWizard>
#include <QNetworkReply>
#include <QTimer>

#include "folder.h"
#include "accountfwd.h"

#include "ui_folderwizardsourcepage.h"
#include "ui_folderwizardtargetpage.h"

class QCheckBox;

namespace OCC {

class SelectiveSyncWidget;

class ownCloudInfo;

/**
 * @brief The FormatWarningsWizardPage class
 * @ingroup gui
 */
class FormatWarningsWizardPage : public QWizardPage
{
    Q_OBJECT
protected:
    [[nodiscard]] QString formatWarnings(const QStringList &warnings) const;
};

/**
 * @brief Page to ask for the local source folder
 * @ingroup gui
 */
class FolderWizardLocalPath : public FormatWarningsWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardLocalPath(const AccountPtr &account);
    ~FolderWizardLocalPath() override;

    [[nodiscard]] bool isComplete() const override;
    void initializePage() override;
    void cleanupPage() override;

    void setFolderMap(const Folder::Map &fm) { _folderMap = fm; }

signals:
    void initialFolderSelectionCanceled();

protected:
    void changeEvent(QEvent *) override;

protected slots:
    void slotChooseLocalFolder();

private:
    void changeStyle();

    Ui_FolderWizardSourcePage _ui{};
    Folder::Map _folderMap;
    AccountPtr _account;
    bool _initialFolderSelection = true;
};


/**
 * @brief page to ask for the target folder
 * @ingroup gui
 */

class FolderWizardRemotePath : public FormatWarningsWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardRemotePath(const AccountPtr &account);
    ~FolderWizardRemotePath() override;

    [[nodiscard]] bool isComplete() const override;

    void initializePage() override;
    void cleanupPage() override;

protected slots:
    void showWarn(const QString & = QString()) const;
    void slotAddRemoteFolder();
    void slotCreateRemoteFolder(const QString &);
    void slotCreateRemoteFolderFinished();
    void slotHandleMkdirNetworkError(QNetworkReply *);
    void slotHandleLsColNetworkError(QNetworkReply *);
    void slotUpdateDirectories(const QStringList &);
    void slotGatherEncryptedPaths(const QString &, const QMap<QString, QString> &);
    void slotRefreshFolders();
    void slotItemExpanded(QTreeWidgetItem *);
    void slotCurrentItemChanged(QTreeWidgetItem *);
    void slotFolderEntryEdited(const QString &text);
    void slotLsColFolderEntry();
    void slotTypedPathFound(const QStringList &subpaths);

protected:
    void changeEvent(QEvent *) override;

private slots:
    void changeStyle();

private:
    LsColJob *runLsColJob(const QString &path);
    void recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path);
    bool selectByPath(QString path);
    Ui_FolderWizardTargetPage _ui{};
    bool _warnWasVisible = false;
    AccountPtr _account;
    QTimer _lscolTimer;
    QStringList _encryptedPaths;
};

/**
 * @brief The FolderWizardSelectiveSync class
 * @ingroup gui
 */
class FolderWizardSelectiveSync : public QWizardPage
{
    Q_OBJECT
public:
    explicit FolderWizardSelectiveSync(const AccountPtr &account);
    ~FolderWizardSelectiveSync() override;

    bool validatePage() override;

    void initializePage() override;
    void cleanupPage() override;

private slots:
    void virtualFilesCheckboxClicked();

private:
    SelectiveSyncWidget *_selectiveSync;
    QCheckBox *_virtualFilesCheckBox = nullptr;
};

/**
 * @brief The FolderWizard class
 * @ingroup gui
 */
class FolderWizard : public QWizard
{
    Q_OBJECT
public:
    enum {
        Page_Source,
        Page_Target,
        Page_SelectiveSync
    };

    explicit FolderWizard(AccountPtr account, QWidget *parent = nullptr);
    ~FolderWizard() override;

    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    FolderWizardLocalPath *_folderWizardSourcePage;
    FolderWizardRemotePath *_folderWizardTargetPage = nullptr;
    FolderWizardSelectiveSync *_folderWizardSelectiveSyncPage;
};


} // namespace OCC

#endif
