/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QDialog>
#include <QTreeWidget>
#include "accountfwd.h"

#include "csync_exclude.h"

class QTreeWidgetItem;
class QTreeWidget;
class QNetworkReply;
class QLabel;
namespace OCC {

class Folder;

/**
 * @brief The SelectiveSyncWidget contains a folder tree with labels
 * @ingroup gui
 */
class SelectiveSyncWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SelectiveSyncWidget(AccountPtr account, QWidget *parent = nullptr);

    /// Returns a list of blacklisted paths, each including the trailing /
    QStringList createBlackList(QTreeWidgetItem *root = nullptr) const;

    /** Returns the oldBlackList passed into setFolderInfo(), except that
     *  a "/" entry is expanded to all top-level folder names.
     */
    [[nodiscard]] QStringList oldBlackList() const;

    // Estimates the total size of checked items (recursively)
    qint64 estimatedSize(QTreeWidgetItem *root = nullptr);

    // oldBlackList is a list of excluded paths, each including a trailing /
    void setFolderInfo(const QString &folderPath, const QString &rootName,
        const QStringList &oldBlackList = QStringList());

    [[nodiscard]] QSize sizeHint() const override;

private slots:
    void slotUpdateDirectories(QStringList);
    void slotUpdateRootFolderFilesSize(const QStringList &subfolders);
    void slotItemExpanded(QTreeWidgetItem *);
    void slotItemChanged(QTreeWidgetItem *, int);
    void slotLscolFinishedWithError(QNetworkReply *);
    void slotGatherEncryptedPaths(const QString &, const QMap<QString, QString> &);

private:
    void refreshFolders();
    void recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path, qint64 size);

    AccountPtr _account;

    QString _folderPath;
    QString _rootName;
    QStringList _oldBlackList;

    bool _inserting = false; // set to true when we are inserting new items on the list
    QLabel *_loading;

    QTreeWidget *_folderTree;

    // During account setup we want to filter out excluded folders from the
    // view without having a Folder.SyncEngine.ExcludedFiles instance.
    ExcludedFiles _excludedFiles;

    QStringList _encryptedPaths;

    qint64 _rootFilesSize = 0;
};

/**
 * @brief The SelectiveSyncDialog class
 * @ingroup gui
 */
class SelectiveSyncDialog : public QDialog
{
    Q_OBJECT
public:
    // Dialog for a specific folder (used from the account settings button)
    explicit SelectiveSyncDialog(AccountPtr account, Folder *folder, QWidget *parent = nullptr, Qt::WindowFlags f = {});

    // Dialog for the whole account (Used from the wizard)
    explicit SelectiveSyncDialog(AccountPtr account, const QString &folder, const QStringList &blacklist, QWidget *parent = nullptr, Qt::WindowFlags f = {});

    void accept() override;

    [[nodiscard]] QStringList createBlackList() const;
    [[nodiscard]] QStringList oldBlackList() const;

    // Estimate the size of the total of sync'ed files from the server
    qint64 estimatedSize();

private:
    void init(const AccountPtr &account);

    SelectiveSyncWidget *_selectiveSync = nullptr;

    Folder *_folder;
    QPushButton *_okButton = nullptr;
};
}
