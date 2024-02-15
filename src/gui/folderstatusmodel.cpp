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

#include <QApplication>
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

    int64_t getQuotaOc10(const AccountStatePtr &accountState, FolderStatusModel::Columns type)
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

    class SubFolderInfo
    {
    public:
        class Progress
        {
        public:
            Progress() { }
            bool isNull() const { return _progressString.isEmpty() && _warningCount == 0 && _overallSyncString.isEmpty(); }
            QString _progressString;
            QString _overallSyncString;
            int _warningCount = 0;
            int _overallPercent = 0;
        };
        SubFolderInfo(Folder *folder)
            : _folder(folder)
        {
        }
        Folder *const _folder;

        Progress _progress;

        std::chrono::steady_clock::time_point _lastProgressUpdated = std::chrono::steady_clock::now();
        ProgressInfo::Status _lastProgressUpdateStatus = ProgressInfo::None;
    };

    void computeProgress(const ProgressInfo &progress, SubFolderInfo::Progress *pi)
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
            if (curItemProgress == -1 || (ProgressInfo::isSizeDependent(citm._item) && biggerItemSize < citm._item._size)) {
                curItemProgress = citm._progress.completed();
                curItem = citm._item;
                biggerItemSize = citm._item._size;
            }
            if (citm._item._direction != SyncFileItem::Up) {
                estimatedDownBw += progress.fileProgress(citm._item).estimatedBandwidth;
            } else {
                estimatedUpBw += progress.fileProgress(citm._item).estimatedBandwidth;
            }
            allFilenames.append(QApplication::translate("FolderStatus", "'%1'").arg(citm._item._file));
        }
        if (curItemProgress == -1) {
            curItemProgress = curItem._size;
        }

        const QString itemFileName = curItem._file;
        const QString kindString = Progress::asActionString(curItem);

        QString fileProgressString;
        if (ProgressInfo::isSizeDependent(curItem)) {
            // quint64 estimatedBw = progress.fileProgress(curItem).estimatedBandwidth;
            if (estimatedUpBw || estimatedDownBw) {
                /*
                //: Example text: "uploading foobar.png (1MB of 2MB) time left 2 minutes at a rate of 24Kb/s"
                fileProgressString = tr("%1 %2 (%3 of %4) %5 left at a rate of %6/s")
                    .arg(kindString, itemFileName, s1, s2,
                        Utility::durationToDescriptiveString(progress.fileProgress(curItem).estimatedEta),
                        Utility::octetsToString(estimatedBw) );
                */
                //: Example text: "Syncing 'foo.txt', 'bar.txt'"
                fileProgressString = QApplication::translate("FolderStatus", "Syncing %1").arg(allFilenames.join(QStringLiteral(", ")));
                if (estimatedDownBw > 0) {
                    fileProgressString.append(QStringLiteral(", "));
// ifdefs: https://github.com/owncloud/client/issues/3095#issuecomment-128409294
#ifdef Q_OS_WIN
                    //: Example text: "download 24Kb/s"   (%1 is replaced by 24Kb (translated))
                    fileProgressString.append(QApplication::translate("FolderStatus", "download %1/s").arg(Utility::octetsToString(estimatedDownBw)));
#else
                    fileProgressString.append(QApplication::translate("FolderStatus", "\u2193 %1/s").arg(Utility::octetsToString(estimatedDownBw)));
#endif
                }
                if (estimatedUpBw > 0) {
                    fileProgressString.append(QStringLiteral(", "));
#ifdef Q_OS_WIN
                    //: Example text: "upload 24Kb/s"   (%1 is replaced by 24Kb (translated))
                    fileProgressString.append(QApplication::translate("FolderStatus", " upload %1/s").arg(Utility::octetsToString(estimatedUpBw)));
#else
                    fileProgressString.append(QApplication::translate("FolderStatus", "\u2191 %1/s").arg(Utility::octetsToString(estimatedUpBw)));
#endif
                }
            } else {
                //: Example text: "uploading foobar.png (2MB of 2MB)"
                fileProgressString = QApplication::translate("FolderStatus", "%1 %2 (%3 of %4)")
                                         .arg(kindString, itemFileName, Utility::octetsToString(curItemProgress), Utility::octetsToString(curItem._size));
            }
        } else if (!kindString.isEmpty()) {
            //: Example text: "uploading foobar.png"
            fileProgressString = QApplication::translate("FolderStatus", "%1 %2").arg(kindString, itemFileName);
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
                overallSyncString = QApplication::translate("FolderStatus", "%5 left, %1 of %2, file %3 of %4")
                                        .arg(s1, s2)
                                        .arg(currentFile)
                                        .arg(totalFileCount)
                                        .arg(Utility::durationToDescriptiveString1(progress.totalProgress().estimatedEta));

            } else {
                //: Example text: "12 MB of 345 MB, file 6 of 7"
                overallSyncString = QApplication::translate("FolderStatus", "%1 of %2, file %3 of %4").arg(s1, s2).arg(currentFile).arg(totalFileCount);
            }
        } else if (totalFileCount > 0) {
            // Don't attempt to estimate the time left if there is no kb to transfer.
            overallSyncString = QApplication::translate("FolderStatus", "file %1 of %2").arg(currentFile).arg(totalFileCount);
        }

        pi->_overallSyncString = overallSyncString;

        int overallPercent = 0;
        if (totalFileCount > 0) {
            // Add one 'byte' for each file so the percentage is moving when deleting or renaming files
            overallPercent = qRound(double(completedSize + completedFile) / double(totalSize + totalFileCount) * 100.0);
        }
        pi->_overallPercent = qBound(0, overallPercent, 100);
    }
}


FolderStatusModel::FolderStatusModel(QObject *parent)
    : QAbstractTableModel(parent)
    , _accountState(nullptr)
{
}

FolderStatusModel::~FolderStatusModel()
{
}

void FolderStatusModel::setAccountState(const AccountStatePtr &accountState)
{
    beginResetModel();
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

        _folders.push_back(std::make_unique<SubFolderInfo>(f));

        connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo, this, [f, this](Folder *folder, const ProgressInfo &progress) {
            if (folder == f) {
                slotSetProgress(progress, f);
            }
        });
    }

    Q_EMIT endResetModel();
}

QVariant FolderStatusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::EditRole)
        return QVariant();

    const Columns column = static_cast<Columns>(index.column());
    switch (column) {
    case Columns::IsUsingSpaces:
        return _accountState->supportsSpaces();
    default:
        break;
    }

    const auto &folderInfo = _folders.at(index.row());
    auto f = folderInfo->_folder;
    if (!f)
        return QVariant();

    const auto &progress = folderInfo->_progress;
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
                return QVariant::fromValue(getQuotaOc10(_accountState, column));
            }
        case Columns::IsUsingSpaces: // handled before
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
            return tr("%1\n%2").arg(Utility::enumToDisplayName(f->syncResult().status()), QDir::toNativeSeparators(folderInfo->_folder->path()));
        } else {
            return tr("Signed out\n%1").arg(QDir::toNativeSeparators(folderInfo->_folder->path()));
        }
    }
    }
    return QVariant();
}

Folder *FolderStatusModel::folder(const QModelIndex &index) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    return _folders.at(index.row())->_folder;
}

int FolderStatusModel::columnCount(const QModelIndex &) const
{
    return static_cast<int>(Columns::ColumnCount);
}

int FolderStatusModel::rowCount(const QModelIndex &parent) const
{
    Q_ASSERT(!parent.isValid());
    return static_cast<int>(_folders.size());
}

void FolderStatusModel::slotUpdateFolderState(Folder *folder)
{
    if (!folder)
        return;
    for (int i = 0; i < _folders.size(); ++i) {
        if (_folders.at(i)->_folder == folder) {
            emit dataChanged(index(i, 0), index(i, 0));
        }
    }
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
    const auto &folder = _folders.at(folderIndex);

    auto *pi = &folder->_progress;

    if (progress.status() == ProgressInfo::Done && !progress._lastCompletedItem.isEmpty()
        && Progress::isWarningKind(progress._lastCompletedItem._status)) {
        pi->_warningCount++;
    }
    // depending on the use of virtual files or small files this slot might be called very often.
    // throttle the model updates to prevent an needlessly high cpu usage used on ui updates.
    if (folder->_lastProgressUpdateStatus != progress.status() || (std::chrono::steady_clock::now() - folder->_lastProgressUpdated > progressUpdateTimeOutC)) {
        folder->_lastProgressUpdateStatus = progress.status();

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
        emit dataChanged(index(folderIndex, 0), index(folderIndex, 0));
        folder->_lastProgressUpdated = std::chrono::steady_clock::now();
    }
}

int FolderStatusModel::indexOf(Folder *f) const
{
    const auto found = std::find_if(_folders.cbegin(), _folders.cend(), [f](const auto &it) { return it->_folder == f; });
    if (found == _folders.cend()) {
        return -1;
    }
    return std::distance(_folders.cbegin(), found);
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

    auto &pi = _folders.at(folderIndex)->_progress;

    SyncResult::Status state = f->syncResult().status();
    if (!f->canSync()) {
        // Reset progress info.
        pi = {};
    } else if (state == SyncResult::NotYetStarted) {
        pi = {};
        pi._overallSyncString = tr("Queued");
    } else if (state == SyncResult::SyncPrepare) {
        pi = {};
        pi._overallSyncString = Utility::enumToDisplayName(SyncResult::SyncPrepare);
    } else if (state == SyncResult::Problem || state == SyncResult::Success || state == SyncResult::Error) {
        // Reset the progress info after a sync.
        pi = {};
    }

    // update the icon etc. now
    slotUpdateFolderState(f);
}

void FolderStatusModel::resetFolders()
{
    setAccountState(_accountState);
}

} // namespace OCC
