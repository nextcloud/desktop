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
#include "accountfwd.h"
#include <QDialog>
#include <QTreeWidget>
#include <QUrl>

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
    QSet<QString> createBlackList(QTreeWidgetItem *root = nullptr) const;

    /** Returns the oldBlackList passed into setFolderInfo(), except that
     *  a "/" entry is expanded to all top-level folder names.
     */
    QSet<QString> oldBlackList() const;

    // Estimates the total size of checked items (recursively)
    qint64 estimatedSize(QTreeWidgetItem *root = nullptr);

    // oldBlackList is a list of excluded paths, each including a trailing /
    void setFolderInfo(const QString &folderPath, const QString &rootName, const QSet<QString> &oldBlackList = {});

    QSize sizeHint() const override;

    void setDavUrl(const QUrl &davUrl);

private slots:
    void slotUpdateDirectories(QStringList);
    void slotItemExpanded(QTreeWidgetItem *);
    void slotItemChanged(QTreeWidgetItem *, int);
    void slotLscolFinishedWithError(QNetworkReply *);

private:
    void refreshFolders();
    void recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path, qint64 size);
    QUrl davUrl() const;

private:
    AccountPtr _account;

    QString _folderPath;
    QString _rootName;
    QSet<QString> _oldBlackList;

    QUrl _davUrl;

    bool _inserting; // set to true when we are inserting new items on the list
    QLabel *_loading;

    QTreeWidget *_folderTree;

    // During account setup we want to filter out excluded folders from the
    // view without having a Folder.SyncEngine.ExcludedFiles instance.
    ExcludedFiles _excludedFiles;
};

}
