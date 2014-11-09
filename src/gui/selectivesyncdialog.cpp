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
#include <QHeaderView>
#include <QDebug>
#include <QSettings>
#include <QScopedValueRollback>
#include <QLabel>

namespace OCC {

SelectiveSyncTreeView::SelectiveSyncTreeView(Account *account, QWidget* parent)
    : QTreeWidget(parent), _inserting(false), _account(account)
{
    _loading = new QLabel(tr("Loading ..."), this);
    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(slotItemExpanded(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(slotItemChanged(QTreeWidgetItem*,int)));
    header()->hide();
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
}

void SelectiveSyncTreeView::refreshFolders()
{
    LsColJob *job = new LsColJob(_account, _folderPath, this);
    connect(job, SIGNAL(directoryListing(QStringList)),
            this, SLOT(slotUpdateDirectories(QStringList)));
    job->start();
    clear();
    _loading->show();
    _loading->move(10,10);
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

void SelectiveSyncTreeView::recursiveInsert(QTreeWidgetItem* parent, QStringList pathTrail, QString path)
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
            if (parent->checkState(0) == Qt::Checked
                    || parent->checkState(0) == Qt::PartiallyChecked) {
                item->setCheckState(0, Qt::Checked);
                foreach(const QString &str , _oldBlackList) {
                    if (str == path) {
                        item->setCheckState(0, Qt::Unchecked);
                        break;
                    } else if (str.startsWith(path)) {
                        item->setCheckState(0, Qt::PartiallyChecked);
                    }
                }
            } else if (parent->checkState(0) == Qt::Unchecked) {
                item->setCheckState(0, Qt::Unchecked);
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

void SelectiveSyncTreeView::slotUpdateDirectories(const QStringList&list)
{
    QScopedValueRollback<bool> isInserting(_inserting);
    _inserting = true;

    _loading->hide();

    QTreeWidgetItem *root = topLevelItem(0);
    if (!root) {
        root = new QTreeWidgetItem(this);
        root->setText(0, _rootName);
        root->setIcon(0, Theme::instance()->applicationIcon());
        root->setData(0, Qt::UserRole, QString());
        if (_oldBlackList.isEmpty()) {
            root->setCheckState(0, Qt::Checked);
        } else {
            root->setCheckState(0, Qt::PartiallyChecked);
        }
    }

    QUrl url = _account->davUrl();
    QString pathToRemove = url.path();
    if (!pathToRemove.endsWith('/')) {
        pathToRemove.append('/');
    }
    pathToRemove.append(_folderPath);
    if (!_folderPath.isEmpty())
        pathToRemove.append('/');

    foreach (QString path, list) {
        path.remove(pathToRemove);
        QStringList paths = path.split('/');
        if (paths.last().isEmpty()) paths.removeLast();
        if (paths.isEmpty())
            continue;
        if (!path.endsWith('/')) {
            path.append('/');
        }
        recursiveInsert(root, paths, path);
    }
    root->setExpanded(true);
}

void SelectiveSyncTreeView::slotItemExpanded(QTreeWidgetItem *item)
{
    QString dir = item->data(0, Qt::UserRole).toString();
    if (dir.isEmpty()) return;
    QString prefix;
    if (!_folderPath.isEmpty()) {
        prefix = _folderPath + QLatin1Char('/');
    }
    LsColJob *job = new LsColJob(_account, prefix + dir, this);
    connect(job, SIGNAL(directoryListing(QStringList)),
            SLOT(slotUpdateDirectories(QStringList)));
    job->start();
}

void SelectiveSyncTreeView::slotItemChanged(QTreeWidgetItem *item, int col)
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
        // also check all the children
        for (int i = 0; i < item->childCount(); ++i) {
            if (item->child(i)->checkState(0) != Qt::Checked) {
                item->child(i)->setCheckState(0, Qt::Checked);
            }
        }
    }

    if (item->checkState(0) == Qt::Unchecked) {
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) == Qt::Checked) {
            parent->setCheckState(0, Qt::PartiallyChecked);
        }

        // Uncheck all the children
        for (int i = 0; i < item->childCount(); ++i) {
            if (item->child(i)->checkState(0) != Qt::Unchecked) {
                item->child(i)->setCheckState(0, Qt::Unchecked);
            }
        }

        // Can't uncheck the root.
        if (!parent) {
            item->setCheckState(0, Qt::PartiallyChecked);
        }
    }

    if (item->checkState(0) == Qt::PartiallyChecked) {
        QTreeWidgetItem *parent = item->parent();
        if (parent && parent->checkState(0) != Qt::PartiallyChecked) {
            parent->setCheckState(0, Qt::PartiallyChecked);
        }
    }
}

QStringList SelectiveSyncTreeView::createBlackList(QTreeWidgetItem* root) const
{
    if (!root) {
        root = topLevelItem(0);
    }
    if (!root) return QStringList();

    switch(root->checkState(0)) {
    case Qt::Unchecked:
        return QStringList(root->data(0, Qt::UserRole).toString() + "/");
    case  Qt::Checked:
        return QStringList();
    case Qt::PartiallyChecked:
        break;
    }

    QStringList result;
    if (root->childCount()) {
        for (int i = 0; i < root->childCount(); ++i) {
            result += createBlackList(root->child(i));
        }
    } else {
        // We did not load from the server so we re-use the one from the old black list
        QString path = root->data(0, Qt::UserRole).toString();
        foreach (const QString & it, _oldBlackList) {
            if (it.startsWith(path))
                result += it;
        }
    }
    return result;
}

SelectiveSyncDialog::SelectiveSyncDialog(Account * account, Folder* folder, QWidget* parent, Qt::WindowFlags f)
    :   QDialog(parent, f), _folder(folder)
{
    init(account);
    _treeView->setFolderInfo(_folder->remotePath(), _folder->alias(), _folder->selectiveSyncBlackList());

    // Make sure we don't get crashes if the folder is destroyed while we are still open
    connect(_folder, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
}

SelectiveSyncDialog::SelectiveSyncDialog(Account* account, const QStringList& blacklist, QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f), _folder(0)
{
    init(account);
    _treeView->setFolderInfo(QString(), QString(), blacklist);
}

void SelectiveSyncDialog::init(Account *account)
{
    setWindowTitle(tr("Choose What to Sync"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    _treeView = new SelectiveSyncTreeView(account, this);
    QLabel *label = new QLabel(tr("Unchecked folders will be <b>removed</b> from your local file system and will not be synchronized to this computer anymore"));
    label->setWordWrap(true);
    layout->addWidget(label);
    layout->addWidget(_treeView);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
    QPushButton *button;
    button = buttonBox->addButton(QDialogButtonBox::Ok);
    connect(button, SIGNAL(clicked()), this, SLOT(accept()));
    button = buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(button, SIGNAL(clicked()), this, SLOT(reject()));
    layout->addWidget(buttonBox);
}

void SelectiveSyncDialog::accept()
{
    if (_folder) {
        auto oldBlackListSet = _folder->selectiveSyncBlackList().toSet();
        QStringList blackList = _treeView->createBlackList();
        _folder->setSelectiveSyncBlackList(blackList);

        // FIXME: Use ConfigFile
        QSettings settings(_folder->configFile(), QSettings::IniFormat);
        settings.beginGroup(FolderMan::escapeAlias(_folder->alias()));
        settings.setValue("blackList", blackList);
        FolderMan *folderMan = FolderMan::instance();
        if (_folder->isBusy()) {
            _folder->slotTerminateSync();
        }

        //The part that changed should not be read from the DB on next sync because there might be new folders
        // (the ones that are no longer in the blacklist)
        auto blackListSet = blackList.toSet();
        auto changes = (oldBlackListSet - blackListSet) + (blackListSet - oldBlackListSet);
        foreach(const auto &it, changes) {
            _folder->journalDb()->avoidReadFromDbOnNextSync(it);
        }

        folderMan->slotScheduleSync(_folder->alias());
    }
    QDialog::accept();
}

QStringList SelectiveSyncDialog::createBlackList() const
{
    return _treeView->createBlackList();
}



}

