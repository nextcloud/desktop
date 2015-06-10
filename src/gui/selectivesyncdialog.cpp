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
#include <QTreeWidgetItem>
#include <QLabel>

namespace OCC {


class SelectiveSyncTreeViewItem : public QTreeWidgetItem {
public:
    SelectiveSyncTreeViewItem(int type = QTreeWidgetItem::Type)
        : QTreeWidgetItem(type) { }
    SelectiveSyncTreeViewItem(const QStringList &strings, int type = QTreeWidgetItem::Type)
        : QTreeWidgetItem(strings, type) { }
    SelectiveSyncTreeViewItem(QTreeWidget *view, int type = QTreeWidgetItem::Type)
        : QTreeWidgetItem(view, type) { }
    SelectiveSyncTreeViewItem(QTreeWidgetItem *parent, int type = QTreeWidgetItem::Type)
        : QTreeWidgetItem(parent, type) { }

private:
    bool operator<(const QTreeWidgetItem &other)const {
        int column = treeWidget()->sortColumn();
        if (column == 1) {
            return data(1, Qt::UserRole).toLongLong() < other.data(1, Qt::UserRole).toLongLong();
        }
        return QTreeWidgetItem::operator <(other);
    }
};

SelectiveSyncTreeView::SelectiveSyncTreeView(AccountPtr account, QWidget* parent)
    : QTreeWidget(parent), _inserting(false), _account(account)
{
    _loading = new QLabel(tr("Loading ..."), this);
    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)), this, SLOT(slotItemExpanded(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(slotItemChanged(QTreeWidgetItem*,int)));
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
    setColumnCount(2);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    header()->setSectionResizeMode(0, QHeaderView::QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(1, QHeaderView::QHeaderView::ResizeToContents);
#endif
    header()->setStretchLastSection(true);
    headerItem()->setText(0, tr("Name"));
    headerItem()->setText(1, tr("Size"));
}

QSize SelectiveSyncTreeView::sizeHint() const
{
    return QTreeView::sizeHint().expandedTo(QSize(400, 400));
}

void SelectiveSyncTreeView::refreshFolders()
{
    LsColJob *job = new LsColJob(_account, _folderPath, this);
    job->setProperties(QList<QByteArray>() << "resourcetype" << "quota-used-bytes");
    connect(job, SIGNAL(directoryListingSubfolders(QStringList)),
            this, SLOT(slotUpdateDirectories(QStringList)));
    connect(job, SIGNAL(finishedWithError(QNetworkReply*)),
            this, SLOT(slotLscolFinishedWithError(QNetworkReply*)));
    job->start();
    clear();
    _loading->show();
    _loading->move(10,header()->height() + 10);
}

void SelectiveSyncTreeView::setFolderInfo(const QString& folderPath, const QString& rootName, const QStringList& oldBlackList)
{
    _folderPath = folderPath;
    if (_folderPath.startsWith(QLatin1Char('/'))) {
        // remove leading '/'
        _folderPath = folderPath.mid(1);
    }
    _rootName = rootName;
    _oldBlackList = oldBlackList;
    refreshFolders();
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

void SelectiveSyncTreeView::recursiveInsert(QTreeWidgetItem* parent, QStringList pathTrail, QString path, qint64 size)
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
        SelectiveSyncTreeViewItem *item = static_cast<SelectiveSyncTreeViewItem*>(findFirstChild(parent, pathTrail.first()));
        if (!item) {
            item = new SelectiveSyncTreeViewItem(parent);
            if (parent->checkState(0) == Qt::Checked
                    || parent->checkState(0) == Qt::PartiallyChecked) {
                item->setCheckState(0, Qt::Checked);
                foreach(const QString &str , _oldBlackList) {
                    if (str == path || str == QLatin1String("/")) {
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
            item->setText(1, Utility::octetsToString(size));
            item->setData(1, Qt::UserRole, size);
//            item->setData(0, Qt::UserRole, pathTrail.first());
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        pathTrail.removeFirst();
        recursiveInsert(item, pathTrail, path, size);
    }
}

void SelectiveSyncTreeView::slotUpdateDirectories(const QStringList&list)
{
    auto job = qobject_cast<LsColJob *>(sender());

    QScopedValueRollback<bool> isInserting(_inserting);
    _inserting = true;

    SelectiveSyncTreeViewItem *root = static_cast<SelectiveSyncTreeViewItem*>(topLevelItem(0));

    if (!root && list.size() <= 1) {
        _loading->setText(tr("No subfolders currently on the server."));
        _loading->resize(_loading->sizeHint()); // because it's not in a layout
        return;
    } else {
        _loading->hide();
    }

    if (!root) {
        root = new SelectiveSyncTreeViewItem(this);
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
        auto size = job ? job->_sizes.value(path) : 0;
        path.remove(pathToRemove);
        QStringList paths = path.split('/');
        if (paths.last().isEmpty()) paths.removeLast();
        if (paths.isEmpty())
            continue;
        if (!path.endsWith('/')) {
            path.append('/');
        }
        recursiveInsert(root, paths, path, size);
    }

    root->setExpanded(true);
}

void SelectiveSyncTreeView::slotLscolFinishedWithError(QNetworkReply *r)
{
    if (r->error() == QNetworkReply::ContentNotFoundError) {
        _loading->setText(tr("No subfolders currently on the server."));
    } else {
        _loading->setText(tr("An error occured while loading the list of sub folders."));
    }
    _loading->resize(_loading->sizeHint()); // because it's not in a layout
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
    connect(job, SIGNAL(directoryListingSubfolders(QStringList)),
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

qint64 SelectiveSyncTreeView::estimatedSize(QTreeWidgetItem* root)
{
    if (!root) {
        root = topLevelItem(0);
    }
    if (!root) return -1;


    switch(root->checkState(0)) {
        case Qt::Unchecked:
            return 0;
        case  Qt::Checked:
            return root->data(1, Qt::UserRole).toLongLong();
        case Qt::PartiallyChecked:
            break;
    }

    qint64 result = 0;
    if (root->childCount()) {
        for (int i = 0; i < root->childCount(); ++i) {
            auto r = estimatedSize(root->child(i));
            if (r < 0) return r;
            result += r;
        }
    } else {
        // We did not load from the server so we have no idea how much we will sync from this branch
        return -1;
    }
    return result;
}


SelectiveSyncDialog::SelectiveSyncDialog(AccountPtr account, Folder* folder, QWidget* parent, Qt::WindowFlags f)
    :   QDialog(parent, f), _folder(folder)
{
    init(account, tr("Unchecked folders will be <b>removed</b> from your local file system and will not be synchronized to this computer anymore"));
    _treeView->setFolderInfo(_folder->remotePath(), _folder->alias(),
                             _folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList));

    // Make sure we don't get crashes if the folder is destroyed while we are still open
    connect(_folder, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
}

SelectiveSyncDialog::SelectiveSyncDialog(AccountPtr account, const QString &folder,
                                         const QStringList& blacklist, QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f), _folder(0)
{
    init(account,
         Theme::instance()->wizardSelectiveSyncDefaultNothing() ?
            tr("Choose What to Sync: Select remote subfolders you wish to synchronize.") :
            tr("Choose What to Sync: Deselect remote subfolders you do not wish to synchronize."));
    _treeView->setFolderInfo(folder, folder, blacklist);
}

void SelectiveSyncDialog::init(const AccountPtr &account, const QString &labelText)
{
    setWindowTitle(tr("Choose What to Sync"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    _treeView = new SelectiveSyncTreeView(account, this);
    auto label = new QLabel(labelText);
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
        auto oldBlackListSet = _folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList).toSet();
        QStringList blackList = _treeView->createBlackList();
        _folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);

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

        folderMan->slotScheduleSync(_folder);
    }
    QDialog::accept();
}

QStringList SelectiveSyncDialog::createBlackList() const
{
    return _treeView->createBlackList();
}

qint64 SelectiveSyncDialog::estimatedSize()
{
    return _treeView->estimatedSize();
}


}

