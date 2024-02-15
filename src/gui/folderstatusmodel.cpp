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
#include "account.h"
#include "accountstate.h"
#include "common/asserts.h"
#include "folderman.h"
#include "folderstatusdelegate.h"
#include "gui/quotainfo.h"
#include "theme.h"

#include "resources/resources.h"

#include "libsync/graphapi/space.h"
#include "libsync/graphapi/spacesmanager.h"

#include <QDir>
#include <QFileIconProvider>
#include <QVarLengthArray>

#include <set>

using namespace std::chrono_literals;

Q_DECLARE_METATYPE(QPersistentModelIndex)

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderStatus, "gui.folder.model", QtInfoMsg)

namespace {
    // minimum delay between progress updates
    constexpr auto progressUpdateTimeOutC = 1s;

    const char propertyParentIndexC[] = "oc_parentIndex";
    const char propertyPermissionMap[] = "oc_permissionMap";

    int64_t getQuota(const AccountStatePtr &accountState, const QString &spaceId, FolderStatusModel::Columns type)
    {
        if (auto spacesManager = accountState->account()->spacesManager()) {
            const auto *space = spacesManager->space(spaceId);
            if (space) {
                const auto quota = space->drive().getQuota();
                if (quota.isValid()) {
                    switch (type) {
                    case FolderStatusModel::Columns::QuotaTotal:
                        return quota.getTotal();
                    case FolderStatusModel::Columns::QuotaUsed:
                        return quota.getUsed();
                    default:
                        Q_UNREACHABLE();
                    }
                }
            }
        }
        return {};
    }

    int64_t getQuotaOc10(const AccountStatePtr &accountState, const QUrl &davUrl, FolderStatusModel::Columns type)
    {
        switch (type) {
        case FolderStatusModel::Columns::QuotaTotal:
            return accountState->quotaInfo()->lastQuotaTotalBytes();
        case FolderStatusModel::Columns::QuotaUsed:
            return accountState->quotaInfo()->lastQuotaUsedBytes();
        default:
            Q_UNREACHABLE();
        }
    }
}

FolderStatusModel::FolderStatusModel(QObject *parent)
    : QAbstractItemModel(parent)
    , _accountState(nullptr)
    , _dirty(false)
{
    connect(this, &FolderStatusModel::rowsInserted, this, &FolderStatusModel::dirtyChanged);
}

FolderStatusModel::~FolderStatusModel()
{
}

void FolderStatusModel::setAccountState(const AccountStatePtr &accountState)
{
    beginResetModel();
    _dirty = false;
    _folders.clear();
    if (_accountState != accountState) {
        Q_ASSERT(!_accountState);
        _accountState = accountState;

        connect(FolderMan::instance(), &FolderMan::folderSyncStateChange, this, &FolderStatusModel::slotFolderSyncStateChange);

        if (accountState->supportsSpaces()) {
            connect(accountState->account()->spacesManager(), &GraphApi::SpacesManager::updated, this, [this] {
                beginResetModel();
                endResetModel();
            });
        }
    }
    for (const auto &f : FolderMan::instance()->folders()) {
        if (!accountState)
            break;
        if (f->accountState() != accountState)
            continue;
        SubFolderInfo info;
        // the name is not actually used with the root element
        info._name = QLatin1Char('/');
        info._path = QLatin1Char('/');
        info._folder = f;
        info._checked = Qt::PartiallyChecked;
        _folders << info;

        connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo, this, [f, this](Folder *folder, const ProgressInfo &progress) {
            if (folder == f) {
                slotSetProgress(progress, f);
            }
        });
    }

    // Set the root _pathIdx after the sorting
    for (int i = 0; i < _folders.size(); ++i) {
        _folders[i]._pathIdx << i;
    }

    Q_EMIT endResetModel();
    emit dirtyChanged();
}


Qt::ItemFlags FolderStatusModel::flags(const QModelIndex &index) const
{
    if (!_accountState) {
        return Qt::NoItemFlags;
    }

    // Always enable the item. If it isn't enabled, it cannot be in the selection model, so all
    // actions from the context menu and the pop-up menu will have some other model index than the
    // one under the mouse cursor!
    const auto flags = Qt::ItemIsEnabled;

    switch (classify(index)) {
    case FetchLabel:
        return flags | Qt::ItemNeverHasChildren;
    case RootFolder:
        return flags;
    case SubFolder:
        return flags | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable;
    }
    return Qt::NoItemFlags;
}

QVariant FolderStatusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::EditRole)
        return QVariant();

    const Columns column = static_cast<Columns>(index.column());
    const auto itemType = classify(index);
    switch (column) {
    case Columns::ItemType:
        return itemType;
    case Columns::IsUsingSpaces:
        return _accountState->supportsSpaces();
    default:
        break;
    }

    switch (itemType) {
    case SubFolder: {
        const auto &x = static_cast<SubFolderInfo *>(index.internalPointer())->_subs.at(index.row());

        switch (role) {
        case Qt::DisplayRole:
            switch (column) {
            case Columns::FolderPathRole: {
                auto f = x._folder;
                if (!f)
                    return QVariant();
                return QVariant(f->path() + x._path);
            }
            case Columns::IsReady:
                return x._folder->isReady();
            case Columns::HeaderRole:
                return x._size < 0 ? x._name : tr("%1 (%2)", "filename (size)").arg(x._name, Utility::octetsToString(x._size));
            default:
                return {};
            }
        case Qt::ToolTipRole:
            return QString(QLatin1String("<qt>") + Utility::escape(x._size < 0 ? x._name : tr("%1 (%2)", "filename (size)").arg(x._name, Utility::octetsToString(x._size))) + QLatin1String("</qt>"));
        case Qt::CheckStateRole:
            return x._checked;
        case Qt::DecorationRole:
            if (x._isExternal) {
                return QFileIconProvider().icon(QFileIconProvider::Network);
            }
            return Resources::getCoreIcon(QStringLiteral("folder-sync"));
        case Qt::ForegroundRole:
            if (x._isUndecided) {
                return QColor(Qt::red);
            }
            break;
        }
    }
        return QVariant();
    case FetchLabel: {
        const auto x = static_cast<SubFolderInfo *>(index.internalPointer());
        switch (role) {
        case Qt::DisplayRole:
            if (x->_hasError) {
                return QVariant(tr("Error while loading the list of folders from the server.")
                    + QStringLiteral("\n") + x->_lastErrorString);
            } else {
                return tr("Fetching folder list from server...");
            }
            break;
        default:
            return QVariant();
        }
    }
    case RootFolder:
        break;
    }

    const SubFolderInfo &folderInfo = _folders.at(index.row());
    auto f = folderInfo._folder;
    if (!f)
        return QVariant();

    const SubFolderInfo::Progress &progress = folderInfo._progress;
    const bool accountConnected = _accountState->isConnected();

    const auto getSpace = [&]() -> GraphApi::Space * {
        if (_accountState->supportsSpaces()) {
            return _accountState->account()->spacesManager()->space(f->spaceId());
        }
        return nullptr;
    };

    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case Columns::FolderPathRole:
            return f->shortGuiLocalPath();
        case Columns::FolderSecondPathRole:
            return f->remotePath();
        case Columns::FolderConflictMsg:
            return (f->syncResult().hasUnresolvedConflicts())
                ? QStringList(tr("There are unresolved conflicts. Click for details."))
                : QStringList();
        case Columns::FolderErrorMsg: {
            auto errors = f->syncResult().errorStrings();
            const auto legacyError = FolderMan::instance()->unsupportedConfiguration(f->path());
            if (!legacyError) {
                // the error message might contain new lines, the delegate only expect multiple single line values
                errors.append(legacyError.error().split(QLatin1Char('\n')));
            }
            if (f->isReady() && f->virtualFilesEnabled() && f->vfs().mode() == Vfs::Mode::WithSuffix) {
                errors.append({
                    tr("The suffix VFS plugin is deprecated and will be removed in the 7.0 release."),
                    tr("Please use the context menu and select \"Disable virtual file support\" to  ensure future access to your synced files."),
                    tr("You are going to lose access to your sync folder if you do not do so!"),
                });
            }
            return errors;
        }
        case Columns::SyncRunning:
            return f->syncResult().status() == SyncResult::SyncRunning;
        case Columns::HeaderRole: {
            if (auto *space = getSpace()) {
                return space->displayName();
            }
            return f->displayName();
        }
        case Columns::FolderImage:
            if (auto *space = getSpace()) {
                return space->image();
            }
            return Resources::getCoreIcon(QStringLiteral("folder-sync"));
        case Columns::FolderSyncPaused:
            return f->syncPaused();
        case Columns::FolderAccountConnected:
            return accountConnected;
        case Columns::FolderStatusIconRole: {
            auto status = f->syncResult();
            if (!accountConnected) {
                status.setStatus(SyncResult::Status::Offline);
            } else if (f->syncPaused() || f->accountState()->state() == AccountState::PausedDueToMetered) {
                status.setStatus(SyncResult::Status::Paused);
            }
            return Theme::instance()->syncStateIconName(status);
        }
        case Columns::SyncProgressItemString:
            return progress._progressString;
        case Columns::WarningCount:
            return progress._warningCount;
        case Columns::SyncProgressOverallPercent:
            return progress._overallPercent;
        case Columns::SyncProgressOverallString:
            return progress._overallSyncString;
        case Columns::FolderSyncText: {
            if (auto *space = getSpace()) {
                if (!space->drive().getDescription().isEmpty()) {
                    return space->drive().getDescription();
                }
            }
            return tr("Local folder: %1").arg(f->shortGuiLocalPath());
        }
        case Columns::IsReady:
            return f->isReady();
        case Columns::IsDeployed:
            return f->isDeployed();
        case Columns::Priority:
            return f->priority();
        case Columns::QuotaTotal:
            [[fallthrough]];
        case Columns::QuotaUsed:
            if (_accountState->supportsSpaces()) {
                return QVariant::fromValue(getQuota(_accountState, f->spaceId(), column));
            } else {
                return QVariant::fromValue(getQuotaOc10(_accountState, f->webDavUrl(), column));
            }
        case Columns::IsUsingSpaces: // handled before
            [[fallthrough]];
        case Columns::ItemType: // handled before
            [[fallthrough]];
        case Columns::ColumnCount:
            Q_UNREACHABLE();
            break;
        }
        break;
    case Qt::ToolTipRole: {
        if (!progress.isNull()) {
            return progress._progressString;
        }
        if (accountConnected) {
            return tr("%1\n%2").arg(Utility::enumToDisplayName(f->syncResult().status()), QDir::toNativeSeparators(folderInfo._folder->path()));
        } else {
            return tr("Signed out\n%1").arg(QDir::toNativeSeparators(folderInfo._folder->path()));
        }
    }
    }
    return QVariant();
}

Folder *FolderStatusModel::folder(const QModelIndex &index) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    return _folders.at(index.row())._folder;
}

bool FolderStatusModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::CheckStateRole) {
        auto info = infoForIndex(index);
        Qt::CheckState checked = static_cast<Qt::CheckState>(value.toInt());

        if (info && info->_checked != checked) {
            info->_checked = checked;
            if (checked == Qt::Checked) {
                // If we are checked, check that we may need to check the parent as well if
                // all the siblings are also checked
                QModelIndex parent = index.parent();
                auto parentInfo = infoForIndex(parent);
                if (parentInfo && parentInfo->_checked != Qt::Checked) {
                    bool hasUnchecked = false;
                    for (const auto &sub : qAsConst(parentInfo->_subs)) {
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
                    if (info->_subs.at(i)._checked != Qt::Checked) {
                        setData(this->index(i, 0, index), Qt::Checked, Qt::CheckStateRole);
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
                    if (info->_subs.at(i)._checked != Qt::Unchecked) {
                        setData(this->index(i, 0, index), Qt::Unchecked, Qt::CheckStateRole);
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
        emit dataChanged(index, index, { role });
        return true;
    }
    return QAbstractItemModel::setData(index, value, role);
}


int FolderStatusModel::columnCount(const QModelIndex &) const
{
    return static_cast<int>(Columns::ColumnCount);
}

int FolderStatusModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return _folders.count();
    }
    auto info = infoForIndex(parent);
    if (!info)
        return 0;
    if (info->hasLabel())
        return 1;
    return info->_subs.count();
}

FolderStatusModel::ItemType FolderStatusModel::classify(const QModelIndex &index) const
{
    if (auto sub = static_cast<SubFolderInfo *>(index.internalPointer())) {
        if (sub->hasLabel()) {
            return FetchLabel;
        } else {
            return SubFolder;
        }
    }

    return RootFolder;
}

FolderStatusModel::SubFolderInfo *FolderStatusModel::infoForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    if (auto parentInfo = static_cast<SubFolderInfo *>(index.internalPointer())) {
        if (parentInfo->hasLabel()) {
            return nullptr;
        }
        if (index.row() >= parentInfo->_subs.size()) {
            return nullptr;
        }
        return &parentInfo->_subs[index.row()];
    } else {
        if (index.row() >= _folders.count()) {
            // AddButton
            return nullptr;
        }
        return const_cast<SubFolderInfo *>(&_folders[index.row()]);
    }
}

QModelIndex FolderStatusModel::indexForPath(Folder *f, const QString &path) const
{
    if (!f) {
        return QModelIndex();
    }

    int slashPos = path.lastIndexOf(QLatin1Char('/'));
    if (slashPos == -1) {
        // first level folder
        for (int i = 0; i < _folders.size(); ++i) {
            auto &info = _folders.at(i);
            if (info._folder == f) {
                if (path.isEmpty()) { // the folder object
                    return index(i, 0);
                }
                for (int j = 0; j < info._subs.size(); ++j) {
                    const QString subName = info._subs.at(j)._name;
                    if (subName == path) {
                        return index(j, 0, index(i));
                    }
                }
                return QModelIndex();
            }
        }
        return QModelIndex();
    }

    auto parent = indexForPath(f, path.left(slashPos));
    if (!parent.isValid())
        return parent;

    if (slashPos == path.size() - 1) {
        // The slash is the last part, we found our index
        return parent;
    }

    auto parentInfo = infoForIndex(parent);
    if (!parentInfo) {
        return QModelIndex();
    }
    for (int i = 0; i < parentInfo->_subs.size(); ++i) {
        if (parentInfo->_subs.at(i)._name == path.mid(slashPos + 1)) {
            return index(i, 0, parent);
        }
    }

    return QModelIndex();
}

QModelIndex FolderStatusModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return createIndex(row, column /*, nullptr*/);
    }
    switch (classify(parent)) {
    case FetchLabel:
        return QModelIndex();
    case RootFolder:
        if (_folders.count() <= parent.row())
            return QModelIndex(); // should not happen
        return createIndex(row, column, const_cast<SubFolderInfo *>(&_folders[parent.row()]));
    case SubFolder: {
        auto pinfo = static_cast<SubFolderInfo *>(parent.internalPointer());
        if (pinfo->_subs.count() <= parent.row())
            return QModelIndex(); // should not happen
        auto &info = pinfo->_subs[parent.row()];
        if (!info.hasLabel()
            && info._subs.count() <= row)
            return QModelIndex(); // should not happen
        return createIndex(row, column, &info);
    }
    }
    return QModelIndex();
}

QModelIndex FolderStatusModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }
    switch (classify(child)) {
    case RootFolder:
        return QModelIndex();
    case SubFolder:
        [[fallthrough]];
    case FetchLabel:
        break;
    }
    auto pathIdx = static_cast<SubFolderInfo *>(child.internalPointer())->_pathIdx;
    int i = 1;
    OC_ASSERT(pathIdx.at(0) < _folders.count());
    if (pathIdx.count() == 1) {
        return createIndex(pathIdx.at(0), 0 /*, nullptr*/);
    }

    const SubFolderInfo *info = &_folders[pathIdx.at(0)];
    while (i < pathIdx.count() - 1) {
        OC_ASSERT(pathIdx.at(i) < info->_subs.count());
        info = &info->_subs.at(pathIdx.at(i));
        ++i;
    }
    return createIndex(pathIdx.at(i), 0, const_cast<SubFolderInfo *>(info));
}

bool FolderStatusModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return true;

    auto info = infoForIndex(parent);
    if (!info)
        return false;

    if (info->_folder && !info->_folder->supportsSelectiveSync())
        return false;

    if (!info->_fetched)
        return true;

    if (info->_subs.isEmpty())
        return false;

    return true;
}


bool FolderStatusModel::canFetchMore(const QModelIndex &parent) const
{
    if (!_accountState) {
        return false;
    }
    if (_accountState->state() != AccountState::Connected) {
        return false;
    }
    auto info = infoForIndex(parent);
    if (!info || info->_fetched || info->_fetchingJob)
        return false;
    if (info->_hasError) {
        // Keep showing the error to the user, it will be hidden when the account reconnects
        return false;
    }
    if (info->_folder && info->_folder->isReady() && !info->_folder->supportsSelectiveSync()) {
        // Selective sync is hidden in that case
        return false;
    }
    return true;
}


void FolderStatusModel::fetchMore(const QModelIndex &parent)
{
    {
        const auto isReady = parent.siblingAtColumn(static_cast<int>(Columns::IsReady)).data();

        Q_ASSERT(isReady.isValid());

        if (!isReady.toBool()) {
            return;
        }
    }

    auto info = infoForIndex(parent);

    if (!info || info->_fetched || info->_fetchingJob)
        return;
    info->resetSubs(this, parent);
    QString path = info->_folder->remotePathTrailingSlash();
    if (info->_path != QLatin1String("/")) {
        path += info->_path;
    }
    PropfindJob *job = new PropfindJob(_accountState->account(), info->_folder->webDavUrl(), path, PropfindJob::Depth::One, this);
    info->_fetchingJob = job;
    job->setProperties({ QByteArrayLiteral("resourcetype"), QByteArrayLiteral("http://owncloud.org/ns:size"), QByteArrayLiteral("http://owncloud.org/ns:permissions") });
    job->setTimeout(60s);
    connect(job, &PropfindJob::directoryListingSubfolders,
        this, &FolderStatusModel::slotUpdateDirectories);
    connect(job, &PropfindJob::finishedWithError,
        this, &FolderStatusModel::slotLscolFinishedWithError);
    connect(job, &PropfindJob::directoryListingIterated,
        this, &FolderStatusModel::slotGatherPermissions);

    job->start();

    QPersistentModelIndex persistentIndex(parent);
    job->setProperty(propertyParentIndexC, QVariant::fromValue(persistentIndex));

    // Show 'fetching data...' hint after a while.
    _fetchingItems[persistentIndex].start();
    QTimer::singleShot(1s, this, &FolderStatusModel::slotShowFetchProgress);
}

void FolderStatusModel::resetAndFetch(const QModelIndex &parent)
{
    auto info = infoForIndex(parent);
    info->resetSubs(this, parent);
    fetchMore(parent);
}

void FolderStatusModel::slotGatherPermissions(const QString &href, const QMap<QString, QString> &map)
{
    auto it = map.find(QStringLiteral("permissions"));
    if (it == map.end())
        return;

    auto job = sender();
    auto permissionMap = job->property(propertyPermissionMap).toMap();
    job->setProperty(propertyPermissionMap, QVariant()); // avoid a detach of the map while it is modified
    OC_ASSERT_X(!href.endsWith(QLatin1Char('/')), "LsColXMLParser::parse should remove the trailing slash before calling us.");
    permissionMap[href] = *it;
    job->setProperty(propertyPermissionMap, permissionMap);
}

void FolderStatusModel::slotUpdateDirectories(const QStringList &list)
{
    auto job = qobject_cast<PropfindJob *>(sender());
    OC_ASSERT(job);
    QModelIndex idx = qvariant_cast<QPersistentModelIndex>(job->property(propertyParentIndexC));
    auto parentInfo = infoForIndex(idx);
    if (!parentInfo) {
        return;
    }
    if (!parentInfo->_folder->supportsSelectiveSync()) {
        return;
    }
    OC_ASSERT(parentInfo->_fetchingJob == job);
    OC_ASSERT(parentInfo->_subs.isEmpty());

    if (parentInfo->hasLabel()) {
        beginRemoveRows(idx, 0, 0);
        parentInfo->_hasError = false;
        parentInfo->_fetchingLabel = false;
        endRemoveRows();
    }

    parentInfo->_lastErrorString.clear();
    parentInfo->_fetchingJob = nullptr;
    parentInfo->_fetched = true;

    QUrl url = parentInfo->_folder->remoteUrl();
    QString pathToRemove = url.path();
    if (!pathToRemove.endsWith(QLatin1Char('/')))
        pathToRemove += QLatin1Char('/');

    QSet<QString> selectiveSyncBlackList;
    bool ok1 = true;
    bool ok2 = true;
    if (parentInfo->_checked == Qt::PartiallyChecked) {
        selectiveSyncBlackList = parentInfo->_folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok1);
    }
    // qlist for historic reasons
    QStringList selectiveSyncUndecidedList = [&] {
        const auto list = parentInfo->_folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok2);
        return QStringList{list.cbegin(), list.cend()};
    }();

    if (!(ok1 && ok2)) {
        qCWarning(lcFolderStatus) << "Could not retrieve selective sync info from journal";
        return;
    }

    std::set<QString> selectiveSyncUndecidedSet; // not QSet because it's not sorted
    for (const auto &str : qAsConst(selectiveSyncUndecidedList)) {
        if (str.startsWith(parentInfo->_path) || parentInfo->_path == QLatin1String("/")) {
            selectiveSyncUndecidedSet.insert(str);
        }
    }
    const auto permissionMap = job->property(propertyPermissionMap).toMap();

    QStringList sortedSubfolders = list;
    if (!sortedSubfolders.isEmpty())
        sortedSubfolders.removeFirst(); // skip the parent item (first in the list)
    Utility::sortFilenames(sortedSubfolders);

    QVarLengthArray<int, 10> undecidedIndexes;

    QVector<SubFolderInfo> newSubs;
    newSubs.reserve(sortedSubfolders.size());
    for (const auto &path : qAsConst(sortedSubfolders)) {
        auto relativePath = path.mid(pathToRemove.size());
        if (parentInfo->_folder->isFileExcludedRelative(relativePath)) {
            continue;
        }

        SubFolderInfo newInfo;
        newInfo._folder = parentInfo->_folder;
        newInfo._pathIdx = parentInfo->_pathIdx;
        newInfo._pathIdx << newSubs.size();
        newInfo._size = job->sizes().value(path);
        newInfo._isExternal = permissionMap.value(Utility::stripTrailingSlash(path)).toString().contains(QLatin1String("M"));
        newInfo._path = relativePath;
        newInfo._name = Utility::stripTrailingSlash(relativePath).split(QLatin1Char('/')).last();

        if (relativePath.isEmpty())
            continue;

        if (parentInfo->_checked == Qt::Unchecked) {
            newInfo._checked = Qt::Unchecked;
        } else if (parentInfo->_checked == Qt::Checked) {
            newInfo._checked = Qt::Checked;
        } else {
            for (const auto &str : qAsConst(selectiveSyncBlackList)) {
                if (str == relativePath || str == QLatin1String("/")) {
                    newInfo._checked = Qt::Unchecked;
                    break;
                } else if (str.startsWith(relativePath)) {
                    newInfo._checked = Qt::PartiallyChecked;
                }
            }
        }

        auto it = selectiveSyncUndecidedSet.lower_bound(relativePath);
        if (it != selectiveSyncUndecidedSet.end()) {
            if (*it == relativePath) {
                newInfo._isUndecided = true;
                selectiveSyncUndecidedSet.erase(it);
            } else if ((*it).startsWith(relativePath)) {
                undecidedIndexes.append(newInfo._pathIdx.last());

                // Remove all the items from the selectiveSyncUndecidedSet that starts with this path
                QString relativePathNext = relativePath;
                relativePathNext[relativePathNext.length() - 1].unicode()++;
                auto it2 = selectiveSyncUndecidedSet.lower_bound(relativePathNext);
                selectiveSyncUndecidedSet.erase(it, it2);
            }
        }
        newSubs.append(newInfo);
    }

    if (!newSubs.isEmpty()) {
        Q_EMIT beginInsertRows(idx, 0, newSubs.size() - 1);
        parentInfo->_subs = std::move(newSubs);
        Q_EMIT endInsertRows();
    }

    for (auto it = undecidedIndexes.begin(); it != undecidedIndexes.end(); ++it) {
        Q_EMIT suggestExpand(index(*it, 0, idx));
    }
    /* Try to remove the the undecided lists the items that are not on the server. */
    auto it = std::remove_if(selectiveSyncUndecidedList.begin(), selectiveSyncUndecidedList.end(),
        [&](const QString &s) { return selectiveSyncUndecidedSet.count(s); });
    if (it != selectiveSyncUndecidedList.end()) {
        selectiveSyncUndecidedList.erase(it, selectiveSyncUndecidedList.end());

        parentInfo->_folder->journalDb()->setSelectiveSyncList(
            SyncJournalDb::SelectiveSyncUndecidedList, {selectiveSyncUndecidedList.cbegin(), selectiveSyncUndecidedList.cend()});
        emit dirtyChanged();
    }
}

void FolderStatusModel::slotLscolFinishedWithError(QNetworkReply *r)
{
    auto job = qobject_cast<PropfindJob *>(sender());
    OC_ASSERT(job);
    QModelIndex idx = qvariant_cast<QPersistentModelIndex>(job->property(propertyParentIndexC));
    if (!idx.isValid()) {
        return;
    }
    auto parentInfo = infoForIndex(idx);
    if (parentInfo) {
        qCDebug(lcFolderStatus) << r->errorString();
        parentInfo->_lastErrorString = r->errorString();
        auto error = r->error();

        parentInfo->resetSubs(this, idx);

        if (error == QNetworkReply::ContentNotFoundError) {
            parentInfo->_fetched = true;
        } else {
            OC_ASSERT(!parentInfo->hasLabel());
            beginInsertRows(idx, 0, 0);
            parentInfo->_hasError = true;
            endInsertRows();
        }
    }
}

QSet<QString> FolderStatusModel::createBlackList(const FolderStatusModel::SubFolderInfo &root, const QSet<QString> &oldBlackList) const
{
    switch (root._checked) {
    case Qt::Unchecked:
        return {root._path};
    case Qt::Checked:
        return {};
    case Qt::PartiallyChecked:
        break;
    }

    QSet<QString> result;
    if (root._fetched) {
        for (int i = 0; i < root._subs.count(); ++i) {
            result += createBlackList(root._subs.at(i), oldBlackList);
        }
    } else {
        // We did not load from the server so we re-use the one from the old black list
        const QString path = root._path;
        for (const auto &it : oldBlackList) {
            if (it.startsWith(path))
                result += it;
        }
    }
    return result;
}

void FolderStatusModel::slotUpdateFolderState(Folder *folder)
{
    if (!folder)
        return;
    for (int i = 0; i < _folders.count(); ++i) {
        if (_folders.at(i)._folder == folder) {
            emit dataChanged(index(i), index(i));
        }
    }
}

void FolderStatusModel::slotApplySelectiveSync()
{
    for (const auto &folderInfo : qAsConst(_folders)) {
        if (!folderInfo._fetched) {
            folderInfo._folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, {});
            continue;
        }
        const auto folder = folderInfo._folder;

        bool ok;
        auto oldBlackListSet = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
        if (!ok) {
            qCWarning(lcFolderStatus) << "Could not read selective sync list from db.";
            continue;
        }
        auto blackListSet = createBlackList(folderInfo, oldBlackListSet);
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackListSet);


        // The folders that were undecided or blacklisted and that are now checked should go on the white list.
        // The user confirmed them already just now.
        QSet<QString> toAddToWhiteList =
            ((oldBlackListSet + folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok)) - blackListSet);

        if (!toAddToWhiteList.isEmpty()) {
            auto whiteList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok);
            if (ok) {
                whiteList += toAddToWhiteList;
                folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, whiteList);
            }
        }
        // clear the undecided list
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, {});

        // do the sync if there were changes
        const auto changes = (oldBlackListSet - blackListSet) + (blackListSet - oldBlackListSet);
        if (!changes.isEmpty()) {
            if (folder->isSyncRunning()) {
                folder->slotTerminateSync(tr("Selective sync list changed"));
            }
            //The part that changed should not be read from the DB on next sync because there might be new folders
            // (the ones that are no longer in the blacklist)
            for (const auto &it : changes) {
                folder->journalDb()->schedulePathForRemoteDiscovery(it);
                folder->schedulePathForLocalDiscovery(it);
            }
            // Also make sure we see the local file that had been ignored before
            folder->slotNextSyncFullLocalDiscovery();
            FolderMan::instance()->scheduler()->enqueueFolder(folder);
        }
    }

    resetFolders();
}

void FolderStatusModel::slotSetProgress(const ProgressInfo &progress, Folder *f)
{
    if (!qobject_cast<QWidget *>(QObject::parent())->isVisible()) {
        return; // for https://github.com/owncloud/client/issues/2648#issuecomment-71377909
    }

    const int folderIndex = indexOf(f);
    if (folderIndex < 0) {
        return;
    }
    auto &folder = _folders[folderIndex];

    auto *pi = &folder._progress;

    if (progress.status() == ProgressInfo::Done && !progress._lastCompletedItem.isEmpty()
        && Progress::isWarningKind(progress._lastCompletedItem._status)) {
        pi->_warningCount++;
    }
    // depending on the use of virtual files or small files this slot might be called very often.
    // throttle the model updates to prevent an needlessly high cpu usage used on ui updates.
    if (folder._lastProgressUpdateStatus != progress.status() || (std::chrono::steady_clock::now() - folder._lastProgressUpdated > progressUpdateTimeOutC)) {
        folder._lastProgressUpdateStatus = progress.status();

        switch (progress.status()) {
        case ProgressInfo::None:
            Q_UNREACHABLE();
            break;
        case ProgressInfo::Discovery:
            if (!progress._currentDiscoveredRemoteFolder.isEmpty()) {
                pi->_overallSyncString = tr("Checking for changes in remote '%1'").arg(progress._currentDiscoveredRemoteFolder);
            } else if (!progress._currentDiscoveredLocalFolder.isEmpty()) {
                pi->_overallSyncString = tr("Checking for changes in local '%1'").arg(progress._currentDiscoveredLocalFolder);
            }
            break;
        case ProgressInfo::Reconcile:
            pi->_overallSyncString = tr("Reconciling changes");
            break;
        case ProgressInfo::Propagation:
            Q_FALLTHROUGH();
        case ProgressInfo::Done:
            computeProgress(progress, pi);
        }
        emit dataChanged(index(folderIndex), index(folderIndex));
        folder._lastProgressUpdated = std::chrono::steady_clock::now();
    }
}

void FolderStatusModel::computeProgress(const ProgressInfo &progress, SubFolderInfo::Progress *pi)
{
    // find the single item to display:  This is going to be the bigger item, or the last completed
    // item if no items are in progress.
    SyncFileItem curItem = progress._lastCompletedItem;
    qint64 curItemProgress = -1; // -1 means finished
    qint64 biggerItemSize = 0;
    quint64 estimatedUpBw = 0;
    quint64 estimatedDownBw = 0;
    QStringList allFilenames;
    for (const auto &citm : progress._currentItems) {
        if (curItemProgress == -1 || (ProgressInfo::isSizeDependent(citm._item)
                                         && biggerItemSize < citm._item._size)) {
            curItemProgress = citm._progress.completed();
            curItem = citm._item;
            biggerItemSize = citm._item._size;
        }
        if (citm._item._direction != SyncFileItem::Up) {
            estimatedDownBw += progress.fileProgress(citm._item).estimatedBandwidth;
        } else {
            estimatedUpBw += progress.fileProgress(citm._item).estimatedBandwidth;
        }
        auto fileName = QFileInfo(citm._item._file).fileName();
        allFilenames.append(tr("'%1'").arg(fileName));
    }
    if (curItemProgress == -1) {
        curItemProgress = curItem._size;
    }

    const QString itemFileName = curItem._file;
    const QString kindString = Progress::asActionString(curItem);

    QString fileProgressString;
    if (ProgressInfo::isSizeDependent(curItem)) {
        //quint64 estimatedBw = progress.fileProgress(curItem).estimatedBandwidth;
        if (estimatedUpBw || estimatedDownBw) {
            /*
            //: Example text: "uploading foobar.png (1MB of 2MB) time left 2 minutes at a rate of 24Kb/s"
            fileProgressString = tr("%1 %2 (%3 of %4) %5 left at a rate of %6/s")
                .arg(kindString, itemFileName, s1, s2,
                    Utility::durationToDescriptiveString(progress.fileProgress(curItem).estimatedEta),
                    Utility::octetsToString(estimatedBw) );
            */
            //: Example text: "Syncing 'foo.txt', 'bar.txt'"
            fileProgressString = tr("Syncing %1").arg(allFilenames.join(QStringLiteral(", ")));
            if (estimatedDownBw > 0) {
                fileProgressString.append(tr(", "));
// ifdefs: https://github.com/owncloud/client/issues/3095#issuecomment-128409294
#ifdef Q_OS_WIN
                //: Example text: "download 24Kb/s"   (%1 is replaced by 24Kb (translated))
                fileProgressString.append(tr("download %1/s").arg(Utility::octetsToString(estimatedDownBw)));
#else
                fileProgressString.append(tr("\u2193 %1/s")
                                              .arg(Utility::octetsToString(estimatedDownBw)));
#endif
            }
            if (estimatedUpBw > 0) {
                fileProgressString.append(tr(", "));
#ifdef Q_OS_WIN
                //: Example text: "upload 24Kb/s"   (%1 is replaced by 24Kb (translated))
                fileProgressString.append(tr("upload %1/s").arg(Utility::octetsToString(estimatedUpBw)));
#else
                fileProgressString.append(tr("\u2191 %1/s")
                                              .arg(Utility::octetsToString(estimatedUpBw)));
#endif
            }
        } else {
            //: Example text: "uploading foobar.png (2MB of 2MB)"
            fileProgressString = tr("%1 %2 (%3 of %4)").arg(kindString, itemFileName, Utility::octetsToString(curItemProgress), Utility::octetsToString(curItem._size));
        }
    } else if (!kindString.isEmpty()) {
        //: Example text: "uploading foobar.png"
        fileProgressString = tr("%1 %2").arg(kindString, itemFileName);
    }
    pi->_progressString = fileProgressString;

    // overall progress
    qint64 completedSize = progress.completedSize();
    qint64 completedFile = progress.completedFiles();
    qint64 currentFile = progress.currentFile();
    qint64 totalSize = qMax(completedSize, progress.totalSize());
    qint64 totalFileCount = qMax(currentFile, progress.totalFiles());
    QString overallSyncString;
    if (totalSize > 0) {
        const QString s1 = Utility::octetsToString(completedSize);
        const QString s2 = Utility::octetsToString(totalSize);

        if (progress.trustEta()) {
            //: Example text: "5 minutes left, 12 MB of 345 MB, file 6 of 7"
            overallSyncString = tr("%5 left, %1 of %2, file %3 of %4")
                                    .arg(s1, s2)
                                    .arg(currentFile)
                                    .arg(totalFileCount)
                                    .arg(Utility::durationToDescriptiveString1(progress.totalProgress().estimatedEta));

        } else {
            //: Example text: "12 MB of 345 MB, file 6 of 7"
            overallSyncString = tr("%1 of %2, file %3 of %4")
                                    .arg(s1, s2)
                                    .arg(currentFile)
                                    .arg(totalFileCount);
        }
    } else if (totalFileCount > 0) {
        // Don't attempt to estimate the time left if there is no kb to transfer.
        overallSyncString = tr("file %1 of %2").arg(currentFile).arg(totalFileCount);
    }

    pi->_overallSyncString = overallSyncString;

    int overallPercent = 0;
    if (totalFileCount > 0) {
        // Add one 'byte' for each file so the percentage is moving when deleting or renaming files
        overallPercent = qRound(double(completedSize + completedFile) / double(totalSize + totalFileCount) * 100.0);
    }
    pi->_overallPercent = qBound(0, overallPercent, 100);
}

int FolderStatusModel::indexOf(Folder *f) const
{
    const auto it = std::find_if(_folders.cbegin(), _folders.cend(), [f](auto &it) {
        return it._folder == f;
    });
    if (it == _folders.cend()) {
        return -1;
    }
    return std::distance(_folders.cbegin(), it);
}

void FolderStatusModel::slotFolderSyncStateChange(Folder *f)
{
    if (!f) {
        return;
    }

    const int folderIndex = indexOf(f);
    if (folderIndex < 0) {
        return;
    }

    auto &pi = _folders[folderIndex]._progress;

    SyncResult::Status state = f->syncResult().status();
    if (!f->canSync()) {
        // Reset progress info.
        pi = SubFolderInfo::Progress();
    } else if (state == SyncResult::NotYetStarted) {
        pi._overallSyncString = tr("Queued");
        pi = SubFolderInfo::Progress();
    } else if (state == SyncResult::SyncPrepare) {
        pi = SubFolderInfo::Progress();
        pi._overallSyncString = Utility::enumToDisplayName(SyncResult::SyncPrepare);
    } else if (state == SyncResult::Problem || state == SyncResult::Success) {
        // Reset the progress info after a sync.
        pi = SubFolderInfo::Progress();
    } else if (state == SyncResult::Error) {
        pi = SubFolderInfo::Progress();
    }

    // update the icon etc. now
    slotUpdateFolderState(f);

    if (f->syncResult().folderStructureWasChanged()
        && (state == SyncResult::Success || state == SyncResult::Problem)) {
        // There is a new or a removed folder. reset all data
        resetAndFetch(index(folderIndex));
    }
}

void FolderStatusModel::resetFolders()
{
    setAccountState(_accountState);
}

void FolderStatusModel::slotSyncAllPendingBigFolders()
{
    for (int i = 0; i < _folders.count(); ++i) {
        if (!_folders[i]._fetched) {
            _folders[i]._folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, {});
            continue;
        }
        auto folder = _folders.at(i)._folder;

        bool ok;
        const auto &undecidedList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, &ok);
        if (!ok) {
            qCWarning(lcFolderStatus) << "Could not read selective sync list from db.";
            return;
        }

        // If this folder had no undecided entries, skip it.
        if (undecidedList.isEmpty()) {
            continue;
        }

        // Remove all undecided folders from the blacklist
        auto blackList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
        if (!ok) {
            qCWarning(lcFolderStatus) << "Could not read selective sync list from db.";
            return;
        }
        blackList -= undecidedList;
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);

        // Add all undecided folders to the white list
        auto whiteList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok);
        if (!ok) {
            qCWarning(lcFolderStatus) << "Could not read selective sync list from db.";
            return;
        }
        whiteList += undecidedList;
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, whiteList);

        // Clear the undecided list
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, {});

        // Trigger a sync
        if (folder->isSyncRunning()) {
            folder->slotTerminateSync(tr("User triggered sync-all for selective synced folder"));
        }
        // The part that changed should not be read from the DB on next sync because there might be new folders
        // (the ones that are no longer in the blacklist)
        for (const auto &it : undecidedList) {
            folder->journalDb()->schedulePathForRemoteDiscovery(it);
            folder->schedulePathForLocalDiscovery(it);
        }
        // Also make sure we see the local file that had been ignored before
        folder->slotNextSyncFullLocalDiscovery();
        FolderMan::instance()->scheduler()->enqueueFolder(folder);
    }

    resetFolders();
}

void FolderStatusModel::slotSyncNoPendingBigFolders()
{
    for (int i = 0; i < _folders.count(); ++i) {
        auto folder = _folders.at(i)._folder;

        // clear the undecided list
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncUndecidedList, {});
    }

    resetFolders();
}

void FolderStatusModel::slotShowFetchProgress()
{
    QMutableMapIterator<QPersistentModelIndex, QElapsedTimer> it(_fetchingItems);
    while (it.hasNext()) {
        it.next();
        if (it.value().elapsed() > 800) {
            auto idx = it.key();
            auto *info = infoForIndex(idx);
            if (info && info->_fetchingJob) {
                bool add = !info->hasLabel();
                if (add) {
                    beginInsertRows(idx, 0, 0);
                }
                info->_fetchingLabel = true;
                if (add) {
                    endInsertRows();
                }
            }
            it.remove();
        }
    }
}

bool FolderStatusModel::SubFolderInfo::hasLabel() const
{
    return _hasError || _fetchingLabel;
}

void FolderStatusModel::SubFolderInfo::resetSubs(FolderStatusModel *model, const QModelIndex &index)
{
    _fetched = false;
    if (_fetchingJob) {
        disconnect(_fetchingJob, nullptr, model, nullptr);
        _fetchingJob->deleteLater();
        _fetchingJob.clear();
    }
    if (hasLabel()) {
        model->beginRemoveRows(index, 0, 0);
        _fetchingLabel = false;
        _hasError = false;
        model->endRemoveRows();
    } else if (!_subs.isEmpty()) {
        model->beginRemoveRows(index, 0, _subs.count() - 1);
        _subs.clear();
        model->endRemoveRows();
    }
}


} // namespace OCC
