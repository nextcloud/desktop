/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#include "folderstatusmodel.h"
#include "folderman.h"
#include "accountstate.h"
#include "utility.h"
#include <theme.h>
#include <account.h>
#include "folderstatusdelegate.h"

#include <QFileIconProvider>

Q_DECLARE_METATYPE(QPersistentModelIndex)

namespace OCC {

static const char propertyParentIndexC[] = "oc_parentIndex";

FolderStatusModel::FolderStatusModel(QObject *parent)
    :QAbstractItemModel(parent), _dirty(false)
{
}

FolderStatusModel::~FolderStatusModel()
{ }


void FolderStatusModel::setAccountState(const AccountState* accountState)
{
    beginResetModel();
    _dirty = false;
    _folders.clear();
    _accountState = accountState;

    auto folders = FolderMan::instance()->map();
    foreach (auto f, folders) {
        if (f->accountState() != accountState)
            continue;
        SubFolderInfo info;
        info._pathIdx << _folders.size();
        info._name = f->alias();
        info._path = "/";
        info._folder = f;
        info._checked = Qt::PartiallyChecked;
        _folders << info;

        connect(f, SIGNAL(progressInfo(ProgressInfo)), this, SLOT(slotSetProgress(ProgressInfo)), Qt::UniqueConnection);
        connect(f, SIGNAL(syncStateChange()), this, SLOT(slotFolderSyncStateChange()), Qt::UniqueConnection);
        connect(f, SIGNAL(newSharedBigFolderDiscovered(QString)), this, SIGNAL(dirtyChanged()), Qt::UniqueConnection);
    }

    endResetModel();
    emit dirtyChanged();
}


Qt::ItemFlags FolderStatusModel::flags ( const QModelIndex &index  ) const
{
    switch (classify(index)) {
        case AddButton:
            return Qt::ItemIsEnabled;
        case RootFolder:
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        case SubFolder:
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    }
    return 0;
}

QVariant FolderStatusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::EditRole)
        return QVariant();

    switch(classify(index)) {
    case AddButton:
        if (role == FolderStatusDelegate::AddButton)
            return QVariant(true);
        return QVariant();
    case SubFolder:
    {
        const auto &x = static_cast<SubFolderInfo *>(index.internalPointer())->_subs[index.row()];
        switch (role) {
        case Qt::ToolTipRole:
        case Qt::DisplayRole:
            return tr("%1 (%2)").arg(x._name, Utility::octetsToString(x._size));
        case Qt::CheckStateRole:
            return x._checked;
        case Qt::DecorationRole:
            return QFileIconProvider().icon(QFileIconProvider::Folder);
        case Qt::ForegroundRole:
            if (x._isUndecided) {
                return QColor(Qt::red);
            }
            break;
        }
    }
        return QVariant();
    case RootFolder:
        break;
    }

    const SubFolderInfo & folderInfo = _folders.at(index.row());
    auto f = folderInfo._folder;
    if (!f)
        return QVariant();

    const SubFolderInfo::Progress & progress = folderInfo._progress;
    const bool accountConnected = _accountState->isConnected();

    switch (role) {
    case FolderStatusDelegate::FolderPathRole         : return  f->nativePath();
    case FolderStatusDelegate::FolderSecondPathRole   : return  f->remotePath();
    case FolderStatusDelegate::FolderAliasRole        : return  f->alias();
    case FolderStatusDelegate::FolderSyncPaused       : return  f->syncPaused();
    case FolderStatusDelegate::FolderAccountConnected : return  accountConnected;
    case Qt::ToolTipRole:
        return Theme::instance()->statusHeaderText(f->syncResult().status());
    case FolderStatusDelegate::FolderStatusIconRole:
        if ( accountConnected ) {
            auto theme = Theme::instance();
            auto status = f->syncResult().status();
            if( f->syncPaused() ) {
                return theme->folderDisabledIcon( );
            } else {
                if( status == SyncResult::SyncPrepare ) {
                    return theme->syncStateIcon(SyncResult::SyncRunning);
                } else if( status == SyncResult::Undefined ) {
                    return theme->syncStateIcon( SyncResult::SyncRunning);
                } else {
                    // kepp the previous icon for the prepare phase.
                    if( status == SyncResult::Problem) {
                        return theme->syncStateIcon( SyncResult::Success);
                    } else {
                        return theme->syncStateIcon( status );
                    }
                }
            }
        } else {
            return Theme::instance()->folderOfflineIcon();
        }
    case FolderStatusDelegate::AddProgressSpace:
        return !progress.isNull();
    case FolderStatusDelegate::SyncProgressItemString:
        return progress._progressString;
    case FolderStatusDelegate::WarningCount:
        return progress._warningCount;
    case FolderStatusDelegate::SyncProgressOverallPercent:
        return progress._overallPercent;
    case FolderStatusDelegate::SyncProgressOverallString:
        return progress._overallSyncString;
    }
    return QVariant();
}

bool FolderStatusModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role == Qt::CheckStateRole) {
        auto info = infoForIndex(index);
        Qt::CheckState checked = static_cast<Qt::CheckState>(value.toInt());

        if (info && info->_checked != checked) {
            info->_checked = checked;
            if (checked == Qt::Checked) {
                // If we are checked, check that we may need to check the parent as well if
                // all the sibilings are also checked
                QModelIndex parent = index.parent();
                auto parentInfo = infoForIndex(parent);
                if (parentInfo && parentInfo->_checked != Qt::Checked) {
                    bool hasUnchecked = false;
                    foreach(const auto &sub, parentInfo->_subs) {
                        if (sub._checked != Qt::Checked) {
                            hasUnchecked = true;
                            break;
                        }
                    }
                    if (!hasUnchecked) {
                        setData(parent, Qt::Checked, Qt::CheckStateRole);
                    } else if (parentInfo->_checked == Qt::Unchecked) {
                        setData(parent, Qt::PartiallyChecked, Qt::CheckStateRole);
                    }
                }
                // also check all the children
                for (int i = 0; i < info->_subs.count(); ++i) {
                    if (info->_subs[i]._checked != Qt::Checked) {
                        setData(index.child(i, 0), Qt::Checked, Qt::CheckStateRole);
                    }
                }
            }

            if (checked == Qt::Unchecked) {
                QModelIndex parent = index.parent();
                auto parentInfo = infoForIndex(parent);
                if (parentInfo && parentInfo->_checked == Qt::Checked) {
                    setData(parent, Qt::PartiallyChecked, Qt::CheckStateRole);
                }

                // Uncheck all the children
                for (int i = 0; i < info->_subs.count(); ++i) {
                    if (info->_subs[i]._checked != Qt::Unchecked) {
                        setData(index.child(i, 0), Qt::Unchecked, Qt::CheckStateRole);
                    }
                }
            }

            if (checked == Qt::PartiallyChecked) {
                QModelIndex parent = index.parent();
                auto parentInfo = infoForIndex(parent);
                if (parentInfo && parentInfo->_checked != Qt::PartiallyChecked) {
                    setData(parent, Qt::PartiallyChecked, Qt::CheckStateRole);
                }
            }

        }
        _dirty = true;
        emit dirtyChanged();
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    return QAbstractItemModel::setData(index, value, role);
}


int FolderStatusModel::columnCount(const QModelIndex&) const
{
    return 1;
}

int FolderStatusModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return _folders.count() + 1;
    }

    auto info = infoForIndex(parent);
    if (!info)
        return 0;
    return info->_subs.count();
}

FolderStatusModel::ItemType FolderStatusModel::classify(const QModelIndex& index) const
{
    if (index.internalPointer()) {
        return SubFolder;
    }
    if (index.row() < _folders.count()) {
        return RootFolder;
    }
    return AddButton;
}

FolderStatusModel::SubFolderInfo* FolderStatusModel::infoForIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return 0;
    if (auto parentInfo = index.internalPointer()) {
        return &static_cast<SubFolderInfo*>(parentInfo)->_subs[index.row()];
    } else {
        if (index.row() >= _folders.count()) {
            // AddButton
            return 0;
        }
        return const_cast<SubFolderInfo *>(&_folders[index.row()]);
    }
}


QModelIndex FolderStatusModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return createIndex(row, column, nullptr);
    }
    switch(classify(parent)) {
        case AddButton: return QModelIndex();
        case RootFolder:
            if (_folders.count() <= parent.row())
                return QModelIndex(); // should not happen
            return createIndex(row, column, const_cast<SubFolderInfo *>(&_folders[parent.row()]));
        case SubFolder:
            //return QModelIndex();
            if (static_cast<SubFolderInfo*>(parent.internalPointer())->_subs.count() <= parent.row())
                return QModelIndex(); // should not happen
            if (static_cast<SubFolderInfo*>(parent.internalPointer())->_subs.at(parent.row())._subs.count() <= row)
                return QModelIndex(); // should not happen
            return createIndex(row, column, &static_cast<SubFolderInfo*>(parent.internalPointer())->_subs[parent.row()]);
    }
    return QModelIndex();
}

QModelIndex FolderStatusModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }
    switch(classify(child)) {
        case RootFolder:
        case AddButton:
            return QModelIndex();
        case SubFolder:
            break;
    }
    auto pathIdx = static_cast<SubFolderInfo*>(child.internalPointer())->_subs[child.row()]._pathIdx;
    int i = 1;
    Q_ASSERT(pathIdx.at(0) < _folders.count());
    if (pathIdx.count() == 2) {
        return createIndex(pathIdx.at(0), 0, nullptr);
    }

    const SubFolderInfo *info = &_folders[pathIdx.at(0)];
    while (i < pathIdx.count() - 2) {
        Q_ASSERT(pathIdx.at(i) < info->_subs.count());
        info = &info->_subs[pathIdx.at(i)];
        ++i;
    }
    return createIndex(pathIdx.at(i), 0, const_cast<SubFolderInfo *>(info));
}

bool FolderStatusModel::hasChildren(const QModelIndex& parent) const
{
    if (!parent.isValid())
        return true;

    auto info = infoForIndex(parent);
    if (!info)
        return false;

    if (!info->_fetched)
        return true;

    if (info->_subs.isEmpty())
        return false;

    return true;
}


bool FolderStatusModel::canFetchMore(const QModelIndex& parent) const
{
    auto info = infoForIndex(parent);
    if (!info || info->_fetched || info->_fetching)
        return false;
    return true;
}


void FolderStatusModel::fetchMore(const QModelIndex& parent)
{
    auto info = infoForIndex(parent);

    if (!info || info->_fetched || info->_fetching)
        return;

    info->_fetching = true;
    QString path = info->_folder->remotePath();
    if (info->_path != QLatin1String("/")) {
        if (!path.endsWith(QLatin1Char('/'))) {
            path += QLatin1Char('/');
        }
        path += info->_path;
    }
    LsColJob *job = new LsColJob(_accountState->account(), path, this);
    job->setProperties(QList<QByteArray>() << "resourcetype" << "quota-used-bytes");
    job->setTimeout(5 * 1000);
    connect(job, SIGNAL(directoryListingSubfolders(QStringList)),
            SLOT(slotUpdateDirectories(QStringList)));
    connect(job, SIGNAL(finishedWithError(QNetworkReply*)),
            this, SLOT(slotLscolFinishedWithError(QNetworkReply*)));
    job->start();
    job->setProperty(propertyParentIndexC , QVariant::fromValue<QPersistentModelIndex>(parent));
}

void FolderStatusModel::slotUpdateDirectories(const QStringList &list_)
{
    auto job = qobject_cast<LsColJob *>(sender());
    Q_ASSERT(job);
    QModelIndex idx = qvariant_cast<QPersistentModelIndex>(job->property(propertyParentIndexC));
    auto parentInfo = infoForIndex(idx);
    if (!parentInfo) {
        return;
    }

    auto list = list_;
    list.removeFirst(); // remove the parent item

    beginInsertRows(idx, 0, list.count());

    QUrl url = parentInfo->_folder->remoteUrl();
    QString pathToRemove = url.path();
    if (!pathToRemove.endsWith('/'))
        pathToRemove += '/';

    parentInfo->_fetched = true;
    parentInfo->_fetching = false;

    QStringList selectiveSyncBlackList;
    if (parentInfo->_checked == Qt::PartiallyChecked) {
        selectiveSyncBlackList = parentInfo->_folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList);
    }
    auto selectiveSyncUndecidedList = parentInfo->_folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList);

    int i = 0;
    foreach (QString path, list) {
        SubFolderInfo newInfo;
        newInfo._folder = parentInfo->_folder;
        newInfo._pathIdx = parentInfo->_pathIdx;
        newInfo._pathIdx << i++;
        auto size = job ? job->_sizes.value(path) : 0;
        newInfo._size = size;
        path.remove(pathToRemove);
        newInfo._path = path;
        newInfo._name = path.split('/', QString::SkipEmptyParts).last();

        if (path.isEmpty())
            continue;

        if (parentInfo->_checked == Qt::Unchecked) {
            newInfo._checked = Qt::Unchecked;
        } else if (parentInfo->_checked == Qt::Checked) {
            newInfo._checked = Qt::Checked;
        } else {
            foreach(const QString &str , selectiveSyncBlackList) {
                if (str == path || str == QLatin1String("/")) {
                    newInfo._checked = Qt::Unchecked;
                    break;
                } else if (str.startsWith(path)) {
                    newInfo._checked = Qt::PartiallyChecked;
                }
            }
        }
        newInfo._isUndecided = selectiveSyncUndecidedList.contains(path);
        parentInfo->_subs.append(newInfo);
    }

    endInsertRows();
}

void FolderStatusModel::slotLscolFinishedWithError(QNetworkReply* r)
{
/*
    if (r->error() == QNetworkReply::ContentNotFoundError) {
        _loading->setText(tr("No subfolders currently on the server."));
    } else {
        _loading->setText(tr("An error occured while loading the list of sub folders."));
    }
    _loading->resize(_loading->sizeHint()); // because it's not in a layout
*/
    auto job = qobject_cast<LsColJob *>(sender());
    Q_ASSERT(job);
    QModelIndex idx = qvariant_cast<QPersistentModelIndex>(job->property(propertyParentIndexC));
    if (!idx.isValid()) {
        return;
    }
    auto parentInfo = infoForIndex(idx);
    if (parentInfo) {
        parentInfo->_fetching = false;
        if (r->error() == QNetworkReply::ContentNotFoundError) {
            parentInfo->_fetched = true;
        }
    }
}

QStringList FolderStatusModel::createBlackList(FolderStatusModel::SubFolderInfo *root,
                                               const QStringList &oldBlackList) const
{
    if (!root) return QStringList();

    switch(root->_checked) {
        case Qt::Unchecked:
            return QStringList(root->_path);
        case  Qt::Checked:
            return QStringList();
        case Qt::PartiallyChecked:
            break;
    }

    QStringList result;
    if (root->_fetched) {
        for (int i = 0; i < root->_subs.count(); ++i) {
            result += createBlackList(&root->_subs[i], oldBlackList);
        }
    } else {
        // We did not load from the server so we re-use the one from the old black list
        QString path = root->_path;
        foreach (const QString & it, oldBlackList) {
            if (it.startsWith(path))
                result += it;
        }
    }
    return result;
}

void FolderStatusModel::slotUpdateFolderState(Folder *folder)
{
    if( ! folder ) return;
    for (int i = 0; i < _folders.count(); ++i) {
        if (_folders.at(i)._folder == folder) {
            emit dataChanged(index(i), index(i),
                             QVector<int>() << FolderStatusDelegate::AddProgressSpace);
        }
    }
}

void FolderStatusModel::slotApplySelectiveSync()
{
    for (int i = 0; i < _folders.count(); ++i) {
        if (!_folders[i]._fetched) {
            _folders[i]._folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, QStringList());
            continue;
        }
        auto folder = _folders.at(i)._folder;

        auto oldBlackList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList);
        QStringList blackList = createBlackList(&_folders[i], oldBlackList);
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);

        // The folders that were undecided should be part of the white list if they are not in the blacklist
        QStringList toAddToWhiteList;
        foreach (const auto &undec, folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList)) {
            if (!blackList.contains(undec)) {
                toAddToWhiteList.append(undec);
            }
        }
        if (!toAddToWhiteList.isEmpty()) {
            auto whiteList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList);
            whiteList += toAddToWhiteList;
            folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, whiteList);
        }
        // clear the undecided list
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, QStringList());

        // do the sync if there was changes
        auto blackListSet = blackList.toSet();
        auto oldBlackListSet = oldBlackList.toSet();
        auto changes = (oldBlackListSet - blackListSet) + (blackListSet - oldBlackListSet);
        if (!changes.isEmpty()) {
            if (folder->isBusy()) {
                folder->slotTerminateSync();
            }
            //The part that changed should not be read from the DB on next sync because there might be new folders
            // (the ones that are no longer in the blacklist)
            foreach(const auto &it, changes) {
                folder->journalDb()->avoidReadFromDbOnNextSync(it);
            }
            FolderMan::instance()->slotScheduleSync(folder);
        }
    }

    resetFolders();
}

static QString shortenFilename( Folder *f, const QString& file )
{
    // strip off the server prefix from the file name
    QString shortFile(file);
    if( shortFile.isEmpty() ) {
        return QString::null;
    }

    if(shortFile.startsWith(QLatin1String("ownclouds://")) ||
            shortFile.startsWith(QLatin1String("owncloud://")) ) {
        // rip off the whole ownCloud URL.
        if( f ) {
            QString remotePathUrl = f->remoteUrl().toString();
            shortFile.remove(Utility::toCSyncScheme(remotePathUrl));
        }
    }
    return shortFile;
}

void FolderStatusModel::slotSetProgress(const ProgressInfo &progress)
{
    auto par = qobject_cast<QWidget*>(QObject::parent());
    if (!par->isVisible()) {
        return; // for https://github.com/owncloud/client/issues/2648#issuecomment-71377909
    }

    Folder *f = qobject_cast<Folder*>(sender());
    if( !f ) { return; }

    int folderIndex = -1;
    for (int i = 0; i < _folders.count(); ++i) {
        if (_folders.at(i)._folder == f) {
            folderIndex = i;
            break;
        }
    }
    if (folderIndex < 0) { return; }

    auto *pi = &_folders[folderIndex]._progress;

    QVector<int> roles;
    roles << FolderStatusDelegate::AddProgressSpace << FolderStatusDelegate::SyncProgressItemString
        << FolderStatusDelegate::WarningCount;

    if (!progress._currentDiscoveredFolder.isEmpty()) {
        pi->_progressString = tr("Discovering '%1'").arg(progress._currentDiscoveredFolder);
        emit dataChanged(index(folderIndex), index(folderIndex), roles);
        return;
    }

    if(!progress._lastCompletedItem.isEmpty()
            && Progress::isWarningKind(progress._lastCompletedItem._status)) {
        pi->_warningCount++;
    }

    // find the single item to display:  This is going to be the bigger item, or the last completed
    // item if no items are in progress.
    SyncFileItem curItem = progress._lastCompletedItem;
    qint64 curItemProgress = -1; // -1 means finished
    quint64 biggerItemSize = -1;
    foreach(const ProgressInfo::ProgressItem &citm, progress._currentItems) {
        if (curItemProgress == -1 || (ProgressInfo::isSizeDependent(citm._item)
                                      && biggerItemSize < citm._item._size)) {
            curItemProgress = citm._progress.completed();
            curItem = citm._item;
            biggerItemSize = citm._item._size;
        }
    }
    if (curItemProgress == -1) {
        curItemProgress = curItem._size;
    }

    QString itemFileName = shortenFilename(f, curItem._file);
    QString kindString = Progress::asActionString(curItem);

    QString fileProgressString;
    if (ProgressInfo::isSizeDependent(curItem)) {
        QString s1 = Utility::octetsToString( curItemProgress );
        QString s2 = Utility::octetsToString( curItem._size );
        quint64 estimatedBw = progress.fileProgress(curItem).estimatedBandwidth;
        if (estimatedBw) {
            //: Example text: "uploading foobar.png (1MB of 2MB) time left 2 minutes at a rate of 24Kb/s"
            fileProgressString = tr("%1 %2 (%3 of %4) %5 left at a rate of %6/s")
                .arg(kindString, itemFileName, s1, s2,
                    Utility::durationToDescriptiveString(progress.fileProgress(curItem).estimatedEta),
                    Utility::octetsToString(estimatedBw) );
        } else {
            //: Example text: "uploading foobar.png (2MB of 2MB)"
            fileProgressString = tr("%1 %2 (%3 of %4)") .arg(kindString, itemFileName, s1, s2);
        }
    } else if (!kindString.isEmpty()) {
        //: Example text: "uploading foobar.png"
        fileProgressString = tr("%1 %2").arg(kindString, itemFileName);
    }
    pi->_progressString = fileProgressString;

    // overall progress
    quint64 completedSize = progress.completedSize();
    quint64 completedFile = progress.completedFiles();
    quint64 currentFile = progress.currentFile();
    if (currentFile == ULLONG_MAX)
        currentFile = 0;
    quint64 totalSize = qMax(completedSize, progress.totalSize());
    quint64 totalFileCount = qMax(currentFile, progress.totalFiles());
    QString overallSyncString;
    if (totalSize > 0) {
        QString s1 = Utility::octetsToString( completedSize );
        QString s2 = Utility::octetsToString( totalSize );
        overallSyncString = tr("%1 of %2, file %3 of %4\nTotal time left %5")
            .arg(s1, s2)
            .arg(currentFile).arg(totalFileCount)
            .arg( Utility::durationToDescriptiveString(progress.totalProgress().estimatedEta) );
    } else if (totalFileCount > 0) {
        // Don't attemt to estimate the time left if there is no kb to transfer.
        overallSyncString = tr("file %1 of %2") .arg(currentFile).arg(totalFileCount);
    }

    pi->_overallSyncString =  overallSyncString;

    int overallPercent = 0;
    if( totalFileCount > 0 ) {
        // Add one 'byte' for each files so the percentage is moving when deleting or renaming files
        overallPercent = qRound(double(completedSize + completedFile)/double(totalSize + totalFileCount) * 100.0);
    }
    pi->_overallPercent = qBound(0, overallPercent, 100);
    emit dataChanged(index(folderIndex), index(folderIndex), roles);
}

void FolderStatusModel::slotFolderSyncStateChange()
{
    Folder *f = qobject_cast<Folder*>(sender());
    if( !f ) { return; }

    int folderIndex = -1;
    for (int i = 0; i < _folders.count(); ++i) {
        if (_folders.at(i)._folder == f) {
            folderIndex = i;
            break;
        }
    }
    if (folderIndex < 0) { return; }

    SyncResult::Status state = f->syncResult().status();
    if (state == SyncResult::SyncPrepare
            || state == SyncResult::Problem
            || state == SyncResult::Success)  {
        // Reset the progress info before and after a sync.
        _folders[folderIndex]._progress = SubFolderInfo::Progress();
    } else if (state == SyncResult::Error) {
        _folders[folderIndex]._progress._progressString = f->syncResult().errorString();
    }

    // update the icon etc. now
    slotUpdateFolderState(f);
}

void FolderStatusModel::resetFolders()
{
    setAccountState(_accountState);
}

} // namespace OCC
