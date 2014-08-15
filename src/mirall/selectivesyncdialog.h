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
namespace Mirall {

class Folder;

class SelectiveSyncTreeView : public QTreeWidget {
    Q_OBJECT
public:
    explicit SelectiveSyncTreeView(QWidget* parent = 0);
    QStringList createBlackList(QTreeWidgetItem* root = 0) const;
    void refreshFolders();
    void setFolderInfo(const QString &folderPath, const QString &rootName,
                       const QStringList &oldBlackList) {
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
    bool _inserting = false; // set to true when we are inserting new items on the list
};

class SelectiveSyncDialog : public QDialog {
    Q_OBJECT
public:
    explicit SelectiveSyncDialog(Folder *folder, QWidget* parent = 0, Qt::WindowFlags f = 0);

    virtual void accept() Q_DECL_OVERRIDE;

private:

    SelectiveSyncTreeView *_treeView;

    Folder *_folder;
};


}