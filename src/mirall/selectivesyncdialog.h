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

class QTreeWidgetItem;
class QTreeWidget;
namespace Mirall {

class Folder;

class SelectiveSyncDialog : public QDialog {
    Q_OBJECT
public:
    explicit SelectiveSyncDialog(Folder *folder, QWidget* parent = 0, Qt::WindowFlags f = 0);

    virtual void accept() Q_DECL_OVERRIDE;
    QStringList createBlackList(QTreeWidgetItem* root = 0) const;

private slots:
    void refreshFolders();
    void slotUpdateDirectories(const QStringList &);
    void slotItemExpanded(QTreeWidgetItem *);
    void slotItemChanged(QTreeWidgetItem*,int);

private:
    void recursiveInsert(QTreeWidgetItem* parent, QStringList pathTrail, QString path);

    Folder *_folder;
    QTreeWidget *_treeView;
    bool _inserting = false; // set to true when we are inserting new items on the list
};


}