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
#include "selectivesyncdialog.h"
#include "folder.h"
#include "account.h"
#include "networkjobs.h"
#include "theme.h"
#include "folderman.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <qpushbutton.h>
#include <QFileIconProvider>
#include <QDebug>
#include <QSettings>
#include <QScopedValueRollback>

namespace Mirall {

SelectiveSyncDialog::SelectiveSyncDialog(Folder* folder, QWidget* parent, Qt::WindowFlags f)
    :   QDialog(parent, f), _folder(folder)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    _treeView = new QTreeWidget;
    connect(_treeView, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(slotItemExpanded(QTreeWidgetItem*)));
    connect(_treeView, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(slotItemChanged(QTreeWidgetItem*,int)));
    layout->addWidget(_treeView);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
    QPushButton *button;
    button = buttonBox->addButton(QDialogButtonBox::Ok);
    connect(button, SIGNAL(clicked()), this, SLOT(accept()));
    button = buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(button, SIGNAL(clicked()), this, SLOT(reject()));
    layout->addWidget(buttonBox);

    // Make sure we don't get crashes if the folder is destroyed while we are still open
    connect(_folder, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));

    refreshFolders();
}

void SelectiveSyncDialog::refreshFolders()
{
    LsColJob *job = new LsColJob(AccountManager::instance()->account(), _folder->remotePath(), this);
    connect(job, SIGNAL(directoryListing(QStringList)),
            this, SLOT(slotUpdateDirectories(QStringList)));
    job->start();
    _treeView->clear();

}

static QTreeWidgetItem* findFirstChild(QTreeWidgetItem *parent, const QString& text)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(0) == text) {
            return child;
        }
    }
    return 0;
}

void SelectiveSyncDialog::recursiveInsert(QTreeWidgetItem* parent, QStringList pathTrail,
                                                  QString path)
{
    QFileIconProvider prov;
    QIcon folderIcon = prov.icon(QFileIconProvider::Folder);
    if (pathTrail.size() == 0) {
        if (path.endsWith('/')) {
            path.chop(1);
        }
        parent->setToolTip(0, path);
        parent->setData(0, Qt::UserRole, path);
    } else {
        QTreeWidgetItem *item = findFirstChild(parent, pathTrail.first());
        if (!item) {
            item = new QTreeWidgetItem(parent);
            if (parent->checkState(0) == Qt::Checked) {
                item->setCheckState(0, Qt::Checked);
            } else if (parent->checkState(0) == Qt::Unchecked) {
                item->setCheckState(0, Qt::Unchecked);
            } else {
                item->setCheckState(0, Qt::Unchecked);
                foreach(const QString &str , _folder->selectiveSyncList()) {
                    if (str + "/" == path) {
                        item->setCheckState(0, Qt::Checked);
                        break;
                    } else if (str.startsWith(path)) {
                        item->setCheckState(0, Qt::PartiallyChecked);
                    }
                }
            }
            item->setIcon(0, folderIcon);
            item->setText(0, pathTrail.first());
//            item->setData(0, Qt::UserRole, pathTrail.first());
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        pathTrail.removeFirst();
        recursiveInsert(item, pathTrail, path);
    }
}

void SelectiveSyncDialog::slotUpdateDirectories(const QStringList &list)
{
    QScopedValueRollback<bool> isInserting(_inserting);
    _inserting = true;

    QTreeWidgetItem *root = _treeView->topLevelItem(0);
    if (!root) {
        root = new QTreeWidgetItem(_treeView);
        root->setText(0, _folder->alias());
        root->setIcon(0, Theme::instance()->applicationIcon());
        root->setData(0, Qt::UserRole, _folder->remotePath());
        if (_folder->selectiveSyncList().isEmpty() || _folder->selectiveSyncList().contains(QString())) {
            root->setCheckState(0, Qt::Checked);
        } else {
            root->setCheckState(0, Qt::PartiallyChecked);
        }
    }
    const QString folderPath = _folder->remoteUrl().path();
    foreach (QString path, list) {
        path.remove(folderPath);
        QStringList paths = path.split('/');
        if (paths.last().isEmpty()) paths.removeLast();
        recursiveInsert(root, paths, path);
    }
    root->setExpanded(true);
}

void SelectiveSyncDialog::slotItemExpanded(QTreeWidgetItem *item)
{
    QString dir = item->data(0, Qt::UserRole).toString();
    LsColJob *job = new LsColJob(AccountManager::instance()->account(), dir, this);
    connect(job, SIGNAL(directoryListing(QStringList)),
            SLOT(slotUpdateDirectories(QStringList)));
    job->start();
}

void SelectiveSyncDialog::slotItemChanged(QTreeWidgetItem *item, int col)
{
    if (col != 0 || _inserting)
        return;

    if (item->checkState(0) == Qt::Checked) {
        // If we are checked, check that we may need to check the parent as well if
        // all the sibilings are also checked
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) != Qt::Checked) {
            bool hasUnchecked = false;
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->child(i)->checkState(0) != Qt::Checked) {
                    hasUnchecked = true;
                    break;
                }
            }
            if (!hasUnchecked) {
                parent->setCheckState(0, Qt::Checked);
            } else if (parent->checkState(0) == Qt::Unchecked) {
                parent->setCheckState(0, Qt::PartiallyChecked);
            }
        }
        // also check all the childs
        for (int i = 0; i < item->childCount(); ++i) {
            if (item->child(i)->checkState(0) != Qt::Checked) {
                item->child(i)->setCheckState(0, Qt::Checked);
            }
        }
    }

    if (item->checkState(0) == Qt::Unchecked) {
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) != Qt::Unchecked) {
            bool hasChecked = false;
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->child(i)->checkState(0) != Qt::Unchecked) {
                    hasChecked = true;
                    break;
                }
            }
            if (!hasChecked) {
                parent->setCheckState(0, Qt::Unchecked);
            } else if (parent->checkState(0) == Qt::Checked) {
                parent->setCheckState(0, Qt::PartiallyChecked);
            }
        }

        // Uncheck all the childs
        for (int i = 0; i < item->childCount(); ++i) {
            if (item->child(i)->checkState(0) != Qt::Unchecked) {
                item->child(i)->setCheckState(0, Qt::Unchecked);
            }
        }
    }

    if (item->checkState(0) == Qt::PartiallyChecked) {
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) != Qt::PartiallyChecked) {
            parent->setCheckState(0, Qt::PartiallyChecked);
        }
    }
}

QStringList SelectiveSyncDialog::createWhiteList(QTreeWidgetItem* root) const
{
    if (!root) {
        root = _treeView->topLevelItem(0);
    }
    if (!root) return {};

    switch(root->checkState(0)) {
    case  Qt::Checked:
        return { root->data(0, Qt::UserRole).toString() };
    case Qt::Unchecked:
        return {};
    case Qt::PartiallyChecked:
        break;
    }

    QStringList result;
    for (int i = 0; i < root->childCount(); ++i) {
        result += createWhiteList(root->child(i));
    }
    return result;
}

void SelectiveSyncDialog::accept()
{
    QStringList whiteList = createWhiteList();
    _folder->setSelectiveSyncList(whiteList);

    // FIXME: Use MirallConfigFile
    QSettings settings(_folder->configFile(), QSettings::IniFormat);
    settings.beginGroup(FolderMan::escapeAlias(_folder->alias()));
    settings.setValue("whiteList", whiteList);

    QDialog::accept();
}



}



