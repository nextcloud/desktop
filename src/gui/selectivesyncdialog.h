/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once
#include <QDialog>
#include <QTreeWidget>
#include "QtWidgets/qboxlayout.h"
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

protected:
    QVBoxLayout *_layout;
    QLabel *_header;
    QTreeWidget *_folderTree;

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
