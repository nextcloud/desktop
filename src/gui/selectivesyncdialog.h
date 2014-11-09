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

class QTreeWidgetItem;
class QTreeWidget;
class QLabel;
namespace OCC {

class Account;

class Folder;

class SelectiveSyncTreeView : public QTreeWidget {
    Q_OBJECT
public:
    explicit SelectiveSyncTreeView(Account *account, QWidget* parent = 0);

    /// Returns a list of blacklisted paths, each including the trailing /
    QStringList createBlackList(QTreeWidgetItem* root = 0) const;
    void refreshFolders();

    // oldBlackList is a list of excluded paths, each including a trailing /
    void setFolderInfo(const QString &folderPath, const QString &rootName,
                       const QStringList &oldBlackList = QStringList()) {
        _folderPath = folderPath;
        _rootName = rootName;
        _oldBlackList = oldBlackList;
        refreshFolders();
    }
private slots:
    void slotUpdateDirectories(const QStringList &);
    void slotItemExpanded(QTreeWidgetItem *);
    void slotItemChanged(QTreeWidgetItem*,int);
private:
    void recursiveInsert(QTreeWidgetItem* parent, QStringList pathTrail, QString path);
    QString _folderPath;
    QString _rootName;
    QStringList _oldBlackList;
    bool _inserting; // set to true when we are inserting new items on the list
    Account *_account;
    QLabel *_loading;
};

class SelectiveSyncDialog : public QDialog {
    Q_OBJECT
public:
    // Dialog for a specific folder (used from the account settings button)
    explicit SelectiveSyncDialog(Account *account, Folder *folder, QWidget* parent = 0, Qt::WindowFlags f = 0);

    // Dialog for the whole account (Used from the wizard)
    explicit SelectiveSyncDialog(Account *account, const QStringList &blacklist, QWidget* parent = 0, Qt::WindowFlags f = 0);

    virtual void accept() Q_DECL_OVERRIDE;

    QStringList createBlackList() const;

private:

    void init(Account *account);

    SelectiveSyncTreeView *_treeView;

    Folder *_folder;
};

}
