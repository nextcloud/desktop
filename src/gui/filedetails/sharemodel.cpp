/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "sharemodel.h"

#include <QFileInfo>
#include <QTimeZone>

#include <array>
#include <random>
#include <openssl/rand.h>

#include "account.h"
#include "folderman.h"
#include "sharepermissions.h"
#include "theme.h"
#include "updatee2eefolderusersmetadatajob.h"
#include "wordlist.h"

namespace {

static const auto placeholderLinkShareId = QStringLiteral("__placeholderLinkShareId__");
static const auto internalLinkShareId = QStringLiteral("__internalLinkShareId__");
static const auto secureFileDropPlaceholderLinkShareId = QStringLiteral("__secureFileDropPlaceholderLinkShareId__");
}

namespace OCC
{
Q_LOGGING_CATEGORY(lcShareModel, "com.nextcloud.sharemodel")

ShareModel::ShareModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// ---------------------- QAbstractListModel methods ---------------------- //

int ShareModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !_accountState || _localPath.isEmpty()) {
        return 0;
    }

    return _shares.count();
}

QHash<int, QByteArray> ShareModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[ShareRole] = "share";
    roles[ShareTypeRole] = "shareType";
    roles[ShareIdRole] = "shareId";
    roles[IconUrlRole] = "iconUrl";
    roles[AvatarUrlRole] = "avatarUrl";
    roles[LinkRole] = "link";
    roles[LinkShareNameRole] = "linkShareName";
    roles[LinkShareLabelRole] = "linkShareLabel";
    roles[NoteEnabledRole] = "noteEnabled";
    roles[NoteRole] = "note";
    roles[ExpireDateEnabledRole] = "expireDateEnabled";
    roles[ExpireDateEnforcedRole] = "expireDateEnforced";
    roles[ExpireDateRole] = "expireDate";
    roles[EnforcedMaximumExpireDateRole] = "enforcedMaximumExpireDate";
    roles[PasswordProtectEnabledRole] = "passwordProtectEnabled";
    roles[PasswordRole] = "password";
    roles[PasswordEnforcedRole] = "passwordEnforced";
    roles[EditingAllowedRole] = "editingAllowed";
    roles[CurrentPermissionModeRole] = "currentPermissionMode";
    roles[SharedItemTypeRole] = "sharedItemType";
    roles[IsSharePermissionsChangeInProgress] = "isSharePermissionChangeInProgress";
    roles[HideDownloadEnabledRole] = "hideDownload";
    roles[IsHideDownloadEnabledChangeInProgress] = "isHideDownloadInProgress";
    roles[ResharingAllowedRole] = "resharingAllowed";

    return roles;
}

QVariant ShareModel::data(const QModelIndex &index, const int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid | QAbstractItemModel::CheckIndexOption::ParentIsInvalid));

    const auto share = _shares.at(index.row());

    if (!share) {
        return {};
    }

    // Some roles only provide values for the link and user/group share types
    if (const auto linkShare = share.objectCast<LinkShare>()) {
        switch (role) {
        case LinkRole:
            return linkShare->getLink();
        case LinkShareNameRole:
            return linkShare->getName();
        case LinkShareLabelRole:
            return linkShare->getLabel();
        case NoteEnabledRole:
            return !linkShare->getNote().isEmpty();
        case HideDownloadEnabledRole:
            return linkShare->getHideDownload();
        case NoteRole:
            return linkShare->getNote();
        case ExpireDateEnabledRole:
            return linkShare->getExpireDate().isValid();
        case ExpireDateRole: {
            const auto startOfExpireDayUTC = linkShare->getExpireDate().startOfDay(QTimeZone::utc());
            return startOfExpireDayUTC.toMSecsSinceEpoch();
        }
        }

    } else if (const auto userGroupShare = share.objectCast<UserGroupShare>()) {
        switch (role) {
        case NoteEnabledRole:
            return !userGroupShare->getNote().isEmpty();
        case NoteRole:
            return userGroupShare->getNote();
        case ExpireDateEnabledRole:
            return userGroupShare->getExpireDate().isValid();
        case ExpireDateRole: {
            const auto startOfExpireDayUTC = userGroupShare->getExpireDate().startOfDay(QTimeZone::utc());
            return startOfExpireDayUTC.toMSecsSinceEpoch();
        }
        }
    } else if (share->getShareType() == Share::TypeInternalLink && role == LinkRole) {
        return _privateLinkUrl;
    }

    switch (role) {
    case Qt::DisplayRole:
        return displayStringForShare(share);
    case ShareRole:
        return QVariant::fromValue(share);
    case ShareTypeRole:
        return share->getShareType();
    case ShareIdRole:
        return share->getId();
    case IconUrlRole:
        return iconUrlForShare(share);
    case AvatarUrlRole:
        return avatarUrlForShare(share);
    case ExpireDateEnforcedRole:
        return expireDateEnforcedForShare(share);
    case EnforcedMaximumExpireDateRole:
        return enforcedMaxExpireDateForShare(share);
    case CurrentPermissionModeRole: {
        if (share->getPermissions() == OCC::SharePermission::SharePermissionCreate) {
            return QVariant::fromValue(SharePermissionsMode::ModeFileDropOnly);
        } else if ((share->getPermissions() & SharePermissionRead) && (share->getPermissions() & SharePermissionCreate)
                   && (share->getPermissions() & SharePermissionUpdate) && (share->getPermissions() & SharePermissionDelete)) {
            return QVariant::fromValue(SharePermissionsMode::ModeUploadAndEditing);
        } else {
            return QVariant::fromValue(SharePermissionsMode::ModeViewOnly);
        }
    }
    case SharedItemTypeRole:
        return static_cast<int>(_sharedItemType);
    case IsSharePermissionsChangeInProgress:
        return _sharePermissionsChangeInProgress;
    case IsHideDownloadEnabledChangeInProgress:
        return _hideDownloadEnabledChangeInProgress;
    case PasswordProtectEnabledRole:
        return share->isPasswordSet();
    case PasswordRole:
        if (!share->isPasswordSet() || !_shareIdRecentlySetPasswords.contains(share->getId())) {
            return {};
        }
        return _shareIdRecentlySetPasswords.value(share->getId());
    case PasswordEnforcedRole:
        return _accountState && _accountState->account() && _accountState->account()->capabilities().isValid()
            && ((share->getShareType() == Share::TypeEmail && _accountState->account()->capabilities().shareEmailPasswordEnforced())
                || (share->getShareType() == Share::TypeLink && _accountState->account()->capabilities().sharePublicLinkEnforcePassword()));
    case EditingAllowedRole:
        return share->getPermissions().testFlag(SharePermissionUpdate);

    case ResharingAllowedRole:
        return share->getPermissions().testFlag(SharePermissionShare);

    // Deal with roles that only return certain values for link or user/group share types
    case NoteEnabledRole:
    case ExpireDateEnabledRole:
    case HideDownloadEnabledRole:
        return false;
    case LinkRole:
    case LinkShareNameRole:
    case LinkShareLabelRole:
    case NoteRole:
    case ExpireDateRole:
        return {};
    }

    return {};
}

// ---------------------- Internal model data methods ---------------------- //

void ShareModel::resetData()
{
    beginResetModel();

    _synchronizationFolder = nullptr;
    _sharePath.clear();
    _maxSharingPermissions = {};
    _numericFileId.clear();
    _privateLinkUrl.clear();
    _filelockState = {};
    _manager.clear();
    _shares.clear();
    _fetchOngoing = false;
    _hasInitialShareFetchCompleted = false;
    _sharees.clear();

    Q_EMIT sharePermissionsChanged();
    Q_EMIT fetchOngoingChanged();
    Q_EMIT hasInitialShareFetchCompletedChanged();
    Q_EMIT shareesChanged();

    endResetModel();
}

void ShareModel::updateData()
{
    resetData();

    if (_localPath.isEmpty() || !_accountState || _accountState->account().isNull()) {
        qCWarning(lcShareModel) << "Not updating share model data. Local path is:" << _localPath << "Is account state null:" << !_accountState;
        return;
    }

    if (!sharingEnabled()) {
        qCWarning(lcShareModel) << "Server does not support sharing";
        return;
    }

    _synchronizationFolder = FolderMan::instance()->folderForPath(_localPath);

    if (!_synchronizationFolder) {
        qCWarning(lcShareModel) << "Could not update share model data for" << _localPath << "no responsible folder found";
        resetData();
        return;
    }

    qCDebug(lcShareModel) << "Updating share model data now.";

    const auto relPath = _localPath.mid(_synchronizationFolder->cleanPath().length() + 1);
    _sharePath = _synchronizationFolder->remotePathTrailingSlash() + relPath;

    SyncJournalFileRecord fileRecord;
    auto resharingAllowed = true; // lets assume the good

    if (_synchronizationFolder->journalDb()->getFileRecord(relPath, &fileRecord) && fileRecord.isValid() && !fileRecord._remotePerm.isNull()
        && !fileRecord._remotePerm.hasPermission(RemotePermissions::CanReshare)) {
        qCInfo(lcShareModel) << "File record says resharing not allowed";
        resharingAllowed = false;
    }

    _maxSharingPermissions = resharingAllowed ? SharePermissions(_accountState->account()->capabilities().shareDefaultPermissions()) : SharePermissions({});
    Q_EMIT sharePermissionsChanged();

    _numericFileId = fileRecord.numericFileId();

    if (fileRecord.isDirectory()) {
        if (fileRecord.isE2eEncrypted()) {
            _sharedItemType = fileRecord.e2eMangledName().isEmpty() ? SharedItemType::SharedItemTypeEncryptedTopLevelFolder : SharedItemType::SharedItemTypeEncryptedFolder;
        } else {
            _sharedItemType = SharedItemType::SharedItemTypeFolder;
        }
    } else {
        _sharedItemType = fileRecord.isE2eEncrypted() ? SharedItemType::SharedItemTypeEncryptedFile : SharedItemType::SharedItemTypeFile;
    }

    const auto prevIsShareDisabledEncryptedFolder = _isShareDisabledEncryptedFolder;
    _isShareDisabledEncryptedFolder = fileRecord.isE2eEncrypted()
        && (_sharedItemType != SharedItemType::SharedItemTypeEncryptedTopLevelFolder
            || fileRecord._e2eEncryptionStatus < SyncJournalFileRecord::EncryptionStatus::EncryptedMigratedV2_0);
    if (prevIsShareDisabledEncryptedFolder != _isShareDisabledEncryptedFolder) {
        emit isShareDisabledEncryptedFolderChanged();
    }

    // Will get added when shares are fetched if no link shares are fetched
    _placeholderLinkShare.reset(new Share(_accountState->account(),
                                          placeholderLinkShareId,
                                          _accountState->account()->id(),
                                          _accountState->account()->davDisplayName(),
                                          _sharePath,
                                          Share::TypePlaceholderLink));

    _internalLinkShare.reset(new Share(_accountState->account(),
                                       internalLinkShareId,
                                       _accountState->account()->id(),
                                       _accountState->account()->davDisplayName(),
                                       _sharePath,
                                       Share::TypeInternalLink));

    _secureFileDropPlaceholderLinkShare.reset(new Share(_accountState->account(),
                                                        secureFileDropPlaceholderLinkShareId,
                                                        _accountState->account()->id(),
                                                        _accountState->account()->davDisplayName(),
                                                        _sharePath,
                                                        Share::TypeSecureFileDropPlaceholderLink));

    auto job = new PropfindJob(_accountState->account(), _sharePath);
    job->setProperties(QList<QByteArray>() << "http://open-collaboration-services.org/ns:share-permissions"
                                           << "http://owncloud.org/ns:fileid" // numeric file id for fallback private link generation
                                           << "http://owncloud.org/ns:privatelink");
    job->setTimeout(10 * 1000);
    connect(job, &PropfindJob::result, this, &ShareModel::slotPropfindReceived);
    connect(job, &PropfindJob::finishedWithError, this, [&](const QNetworkReply *reply) {
        qCWarning(lcShareModel) << "Propfind for" << _sharePath << "failed";
        _fetchOngoing = false;
        Q_EMIT fetchOngoingChanged();
        Q_EMIT serverError(reply->error(), reply->errorString());
    });

    _fetchOngoing = true;
    Q_EMIT fetchOngoingChanged();
    job->start();

    initShareManager();
}

void ShareModel::initShareManager()
{
    if (!_accountState || _accountState->account().isNull()) {
        return;
    }

    bool sharingPossible = true;
    if (!canShare()) {
        qCWarning(lcSharing) << "The file cannot be shared because it does not have sharing permission.";
        sharingPossible = false;
    }

    if (_manager.isNull() && sharingPossible) {
        _manager.reset(new ShareManager(_accountState->account(), this));
        connect(_manager.data(), &ShareManager::sharesFetched, this, &ShareModel::slotSharesFetched);
        connect(_manager.data(), &ShareManager::shareCreated, this, [&] {
            _manager->fetchShares(_sharePath);
        });
        connect(_manager.data(), &ShareManager::linkShareCreated, this, &ShareModel::slotAddShare);
        connect(_manager.data(), &ShareManager::linkShareRequiresPassword, this, &ShareModel::requestPasswordForLinkShare);
        connect(_manager.data(), &ShareManager::serverError, this, [this](const int code, const QString &message) {
            _hasInitialShareFetchCompleted = true;
            Q_EMIT hasInitialShareFetchCompletedChanged();
            emit serverError(code, message);
        });

        _manager->fetchShares(_sharePath);
    }
}

void ShareModel::handlePlaceholderLinkShare()
{
    // We want to add the placeholder if there are no link shares and
    // if we are not already showing the placeholder link share
    auto linkSharePresent = false;
    auto placeholderLinkSharePresent = false;

    for (const auto &share : qAsConst(_shares)) {
        const auto shareType = share->getShareType();

        if (!linkSharePresent && shareType == Share::TypeLink) {
            linkSharePresent = true;
        } else if (!placeholderLinkSharePresent && shareType == Share::TypePlaceholderLink) {
            placeholderLinkSharePresent = true;
        }

        if (linkSharePresent && placeholderLinkSharePresent) {
            break;
        }
    }

    if (linkSharePresent && placeholderLinkSharePresent) {
        slotRemoveShareWithId(placeholderLinkShareId);
    } else if (!linkSharePresent && !placeholderLinkSharePresent && publicLinkSharesEnabled()) {
        slotAddShare(_placeholderLinkShare);
    }

    Q_EMIT sharesChanged();
}

void ShareModel::handleSecureFileDropLinkShare()
{
    // We want to add the placeholder if there are no link shares and
    // if we are not already showing the placeholder link share
    auto linkSharePresent = false;
    auto secureFileDropLinkSharePresent = false;

    for (const auto &share : qAsConst(_shares)) {
        const auto shareType = share->getShareType();

        if (!linkSharePresent && shareType == Share::TypeLink) {
            linkSharePresent = true;
        } else if (!secureFileDropLinkSharePresent && shareType == Share::TypeSecureFileDropPlaceholderLink) {
            secureFileDropLinkSharePresent = true;
        }

        if (linkSharePresent && secureFileDropLinkSharePresent) {
            break;
        }
    }

    if (linkSharePresent && secureFileDropLinkSharePresent) {
        slotRemoveShareWithId(secureFileDropPlaceholderLinkShareId);
    } else if (!linkSharePresent && !secureFileDropLinkSharePresent) {
        slotAddShare(_secureFileDropPlaceholderLinkShare);
    }
}

void ShareModel::handleLinkShare()
{
    if (!isEncryptedItem()) {
        handlePlaceholderLinkShare();
    } else if (isSecureFileDropSupportedFolder()) {
        handleSecureFileDropLinkShare();
    }
}

void ShareModel::slotPropfindReceived(const QVariantMap &result)
{
    _fetchOngoing = false;
    Q_EMIT fetchOngoingChanged();

    const QVariant receivedPermissions = result["share-permissions"];
    if (!receivedPermissions.toString().isEmpty()) {
        const auto oldCanShare = canShare();

        _maxSharingPermissions = static_cast<SharePermissions>(receivedPermissions.toInt());
        Q_EMIT sharePermissionsChanged();
        qCInfo(lcShareModel) << "Received sharing permissions for" << _sharePath << _maxSharingPermissions;

        if (!oldCanShare && canShare()) {
            qCInfo(lcShareModel) << "Received updated sharing data that says we have permission to share now."
                                 << "Trying to init share manager again.";
            initShareManager();
        }
    }

    const auto privateLinkUrl = result["privatelink"].toString();
    _fileRemoteId = result["fileid"].toByteArray();

    if (!privateLinkUrl.isEmpty()) {
        qCInfo(lcShareModel) << "Received private link url for" << _sharePath << privateLinkUrl;
        _privateLinkUrl = privateLinkUrl;
    } else if (!_fileRemoteId.isEmpty()) {
        qCInfo(lcShareModel) << "Received numeric file id for" << _sharePath << _fileRemoteId;
        _privateLinkUrl = _accountState->account()->deprecatedPrivateLinkUrl(_fileRemoteId).toString(QUrl::FullyEncoded);
    }

    setupInternalLinkShare();
}

void ShareModel::slotSharesFetched(const QList<SharePtr> &shares)
{
    if(!_hasInitialShareFetchCompleted) {
        _hasInitialShareFetchCompleted = true;
        Q_EMIT hasInitialShareFetchCompletedChanged();
    }

    qCInfo(lcSharing) << "Fetched" << shares.count() << "shares";

    for (const auto &share : shares) {
        if (share.isNull() ||
            share->account().isNull() ||
            share->getUidOwner() != share->account()->davUser()) {

            continue;
        }

        slotAddShare(share);
    }

    handleLinkShare();
}

void ShareModel::setupInternalLinkShare()
{
    if (!_accountState ||
        _accountState->account().isNull() ||
        _localPath.isEmpty() ||
        _privateLinkUrl.isEmpty() ||
        isEncryptedItem()) {
        return;
    }

    beginInsertRows({}, _shares.count(), _shares.count());
    _shares.append(_internalLinkShare);
    endInsertRows();
    Q_EMIT internalLinkReady();
}

void ShareModel::setSharePermissionChangeInProgress(const QString &shareId, const bool isInProgress)
{
    if (isInProgress == _sharePermissionsChangeInProgress) {
        return;
    }

    _sharePermissionsChangeInProgress = isInProgress;
    
    const auto shareIndex = _shareIdIndexHash.value(shareId);
    Q_EMIT dataChanged(shareIndex, shareIndex, {IsSharePermissionsChangeInProgress});
}

void ShareModel::setHideDownloadEnabledChangeInProgress(const QString &shareId, const bool isInProgress)
{
    if (isInProgress == _hideDownloadEnabledChangeInProgress) {
        return;
    }

    _hideDownloadEnabledChangeInProgress = isInProgress;

    const auto shareIndex = _shareIdIndexHash.value(shareId);
    Q_EMIT dataChanged(shareIndex, shareIndex, {IsHideDownloadEnabledChangeInProgress});
}

void ShareModel::slotAddShare(const SharePtr &share)
{
    if (share.isNull()) {
        return;
    }

    const auto shareId = share->getId();
    QModelIndex shareModelIndex;

    if (_shareIdIndexHash.contains(shareId)) {
        const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
        const auto shareIndex = sharePersistentModelIndex.row();

        _shares.replace(shareIndex, share);

        shareModelIndex = index(sharePersistentModelIndex.row());
        Q_EMIT dataChanged(shareModelIndex, shareModelIndex);
    } else {
        const auto shareIndex = _shares.count();

        beginInsertRows({}, _shares.count(), _shares.count());
        _shares.append(share);
        endInsertRows();

        slotAddSharee(share->getShareWith());

        shareModelIndex = index(shareIndex);
    }

    const QPersistentModelIndex sharePersistentIndex(shareModelIndex);
    _shareIdIndexHash.insert(shareId, sharePersistentIndex);

    connect(share.data(), &Share::serverError, this, &ShareModel::slotServerError);
    connect(share.data(), &Share::passwordSetError, this, [this, shareId](const int code, const QString &message) {
        _shareIdRecentlySetPasswords.remove(shareId);
        slotSharePasswordSet(shareId);
        Q_EMIT passwordSetError(shareId, code, message);
    });

    // Passing shareId by reference here will cause crashing, so we pass by value
    connect(share.data(), &Share::shareDeleted, this, [this, shareId]{ slotRemoveShareWithId(shareId); });
    connect(share.data(), &Share::permissionsSet, this, [this, shareId]{ slotSharePermissionsSet(shareId); });
    connect(share.data(), &Share::passwordSet, this, [this, shareId]{ slotSharePasswordSet(shareId); });

    if (const auto linkShare = share.objectCast<LinkShare>()) {
        connect(linkShare.data(), &LinkShare::noteSet, this, [this, shareId]{ slotShareNoteSet(shareId); });
        connect(linkShare.data(), &LinkShare::nameSet, this, [this, shareId]{ slotShareNameSet(shareId); });
        connect(linkShare.data(), &LinkShare::labelSet, this, [this, shareId]{ slotShareLabelSet(shareId); });
        connect(linkShare.data(), &LinkShare::expireDateSet, this, [this, shareId]{ slotShareExpireDateSet(shareId); });
        connect(linkShare.data(), &LinkShare::hideDownloadSet, this, [this, shareId] { slotHideDownloadSet(shareId); });
    } else if (const auto userGroupShare = share.objectCast<UserGroupShare>()) {
        connect(userGroupShare.data(), &UserGroupShare::noteSet, this, [this, shareId]{ slotShareNoteSet(shareId); });
        connect(userGroupShare.data(), &UserGroupShare::expireDateSet, this, [this, shareId]{ slotShareExpireDateSet(shareId); });
    }

    if (_manager) {
        connect(_manager.data(), &ShareManager::serverError, this, &ShareModel::slotServerError);
    }

    handleLinkShare();
    Q_EMIT sharesChanged();
}

void ShareModel::slotRemoveShareWithId(const QString &shareId)
{
    if (_shares.empty() || shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    _shareIdRecentlySetPasswords.remove(shareId);
    const auto shareIndex = _shareIdIndexHash.take(shareId);

    if (!checkIndex(shareIndex, QAbstractItemModel::CheckIndexOption::IndexIsValid | QAbstractItemModel::CheckIndexOption::ParentIsInvalid)) {
        qCWarning(lcShareModel) << "Won't remove share with id:" << shareId
                                << ", invalid share index: " << shareIndex;
        return;
    }

    const auto share = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
    const auto sharee = share->getShareWith();
    slotRemoveSharee(sharee);

    beginRemoveRows({}, shareIndex.row(), shareIndex.row());
    _shares.removeAt(shareIndex.row());
    endRemoveRows();

    handleLinkShare();

    Q_EMIT sharesChanged();
}

void ShareModel::slotServerError(const int code, const QString &message)
{
    qCWarning(lcShareModel) << "Error from server" << code << message;
    Q_EMIT serverError(code, message);
}

void ShareModel::slotAddSharee(const ShareePtr &sharee)
{
    if(!sharee) {
        return;
    }

    _sharees.append(sharee);
    Q_EMIT shareesChanged();
}

void ShareModel::slotRemoveSharee(const ShareePtr &sharee)
{
    _sharees.removeAll(sharee);
    Q_EMIT shareesChanged();
}

QString ShareModel::displayStringForShare(const SharePtr &share) const
{
    if (const auto linkShare = share.objectCast<LinkShare>()) {

        const auto isSecureFileDropShare = isSecureFileDropSupportedFolder() && linkShare->getPermissions().testFlag(OCC::SharePermission::SharePermissionCreate);

        const auto displayString = isSecureFileDropShare ? tr("Secure file drop link") : tr("Share link");

        if (!linkShare->getLabel().isEmpty()) {
            return QStringLiteral("%1 (%2)").arg(displayString, linkShare->getLabel());
        }

        return displayString;
    } else if (share->getShareType() == Share::TypePlaceholderLink) {
        return tr("Link share");
    } else if (share->getShareType() == Share::TypeInternalLink) {
        return tr("Internal link");
    } else if (share->getShareType() == Share::TypeSecureFileDropPlaceholderLink) {
        return tr("Secure file drop");
    } else if (share->getShareWith()) {
        return share->getShareWith()->format();
    }

    qCWarning(lcShareModel) << "Unable to provide good display string for share";
    return QStringLiteral("Share");
}

QString ShareModel::iconUrlForShare(const SharePtr &share) const
{
    const auto iconsPath = QStringLiteral("image://svgimage-custom-color/");

    switch(share->getShareType()) {
    case Share::TypeInternalLink:
        return QString(iconsPath + QStringLiteral("external.svg"));
    case Share::TypePlaceholderLink:
    case Share::TypeSecureFileDropPlaceholderLink:
    case Share::TypeLink:
        return QString(iconsPath + QStringLiteral("public.svg"));
    case Share::TypeEmail:
        return QString(iconsPath + QStringLiteral("email.svg"));
    case Share::TypeRoom:
        return QString(iconsPath + QStringLiteral("wizard-talk.svg"));
    case Share::TypeUser:
        return QString(iconsPath + QStringLiteral("user.svg"));
    case Share::TypeGroup:
        return QString(iconsPath + QStringLiteral("wizard-groupware.svg"));
    default:
        return {};
    }
}

QString ShareModel::avatarUrlForShare(const SharePtr &share) const
{
    if (share->getShareWith() && share->getShareWith()->type() == Sharee::User && _accountState && _accountState->account()) {
        const auto provider = QStringLiteral("image://tray-image-provider/");
        const auto userId = share->getShareWith()->shareWith();
        const auto avatarUrl = Utility::concatUrlPath(_accountState->account()->url(),
                                                      QString("remote.php/dav/avatars/%1/%2.png").arg(userId, QString::number(64))).toString();
        return QString(provider + avatarUrl);
    }

    return {};
}

long long ShareModel::enforcedMaxExpireDateForShare(const SharePtr &share) const
{
    if (!_accountState || !_accountState->account() || !_accountState->account()->capabilities().isValid()) {
        return {};
    }

    auto expireDays = -1;

    // Both public links and emails count as "public" shares
    if ((share->getShareType() == Share::TypeLink || share->getShareType() == Share::TypeEmail)
        && _accountState->account()->capabilities().sharePublicLinkEnforceExpireDate()) {
        expireDays = _accountState->account()->capabilities().sharePublicLinkExpireDateDays();

    } else if (share->getShareType() == Share::TypeRemote && _accountState->account()->capabilities().shareRemoteEnforceExpireDate()) {
        expireDays = _accountState->account()->capabilities().shareRemoteExpireDateDays();

    } else if ((share->getShareType() == Share::TypeUser ||
                share->getShareType() == Share::TypeGroup ||
                share->getShareType() == Share::TypeCircle ||
                share->getShareType() == Share::TypeRoom) &&
               _accountState->account()->capabilities().shareInternalEnforceExpireDate()) {
        expireDays = _accountState->account()->capabilities().shareInternalExpireDateDays();

    } else {
        return {};
    }

    const auto expireDateTime = QDate::currentDate().addDays(expireDays).startOfDay(QTimeZone::utc());
    return expireDateTime.toMSecsSinceEpoch();
}

bool ShareModel::expireDateEnforcedForShare(const SharePtr &share) const
{
    if(!_accountState || !_accountState->account() || !_accountState->account()->capabilities().isValid()) {
        return false;
    }

    // Both public links and emails count as "public" shares
    if (share->getShareType() == Share::TypeLink ||
        share->getShareType() == Share::TypeEmail) {
        return _accountState->account()->capabilities().sharePublicLinkEnforceExpireDate();

    } else if (share->getShareType() == Share::TypeRemote) {
        return _accountState->account()->capabilities().shareRemoteEnforceExpireDate();

    } else if (share->getShareType() == Share::TypeUser ||
               share->getShareType() == Share::TypeGroup ||
               share->getShareType() == Share::TypeCircle ||
               share->getShareType() == Share::TypeRoom) {
        return _accountState->account()->capabilities().shareInternalEnforceExpireDate();

    }

    return false;
}

// ----------------- Shares modified signal handling slots ----------------- //

void ShareModel::slotSharePermissionsSet(const QString &shareId)
{
    if (shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
    const auto shareModelIndex = index(sharePersistentModelIndex.row());
    Q_EMIT dataChanged(shareModelIndex, shareModelIndex, { EditingAllowedRole, CurrentPermissionModeRole });
    setSharePermissionChangeInProgress(shareId, false);
}

void ShareModel::slotSharePasswordSet(const QString &shareId)
{
    if (shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
    const auto shareModelIndex = index(sharePersistentModelIndex.row());
    Q_EMIT dataChanged(shareModelIndex, shareModelIndex, { PasswordProtectEnabledRole, PasswordRole });
}

void ShareModel::slotHideDownloadSet(const QString &shareId)
{
    if (shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
    const auto shareModelIndex = index(sharePersistentModelIndex.row());
    Q_EMIT dataChanged(shareModelIndex, shareModelIndex, {HideDownloadEnabledRole});
    setHideDownloadEnabledChangeInProgress(shareId, false);
}

void ShareModel::slotShareNoteSet(const QString &shareId)
{
    if (shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
    const auto shareModelIndex = index(sharePersistentModelIndex.row());
    Q_EMIT dataChanged(shareModelIndex, shareModelIndex, { NoteEnabledRole, NoteRole });
}

void ShareModel::slotShareNameSet(const QString &shareId)
{
    if (shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
    const auto shareModelIndex = index(sharePersistentModelIndex.row());
    Q_EMIT dataChanged(shareModelIndex, shareModelIndex, { LinkShareNameRole });
}

void ShareModel::slotShareLabelSet(const QString &shareId)
{
    if (shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
    const auto shareModelIndex = index(sharePersistentModelIndex.row());
    Q_EMIT dataChanged(shareModelIndex, shareModelIndex, { Qt::DisplayRole, LinkShareLabelRole });
}

void ShareModel::slotShareExpireDateSet(const QString &shareId)
{
    if (shareId.isEmpty() || !_shareIdIndexHash.contains(shareId)) {
        return;
    }

    const auto sharePersistentModelIndex = _shareIdIndexHash.value(shareId);
    const auto shareModelIndex = index(sharePersistentModelIndex.row());
    Q_EMIT dataChanged(shareModelIndex, shareModelIndex, { ExpireDateEnabledRole, ExpireDateRole });
}

void ShareModel::slotDeleteE2EeShare(const SharePtr &share) const
{
    const auto account = accountState()->account();
    QString folderAlias;
    for (const auto &f : FolderMan::instance()->map()) {
        if (f->accountState()->account() != account) {
            continue;
        }
        const auto folderPath = f->remotePath();
        if (share->path().startsWith(folderPath) && (share->path() == folderPath || folderPath.endsWith('/') || share->path()[folderPath.size()] == '/')) {
            folderAlias = f->alias();
        }
    }

    auto folder = FolderMan::instance()->folder(folderAlias);
    if (!folder || !folder->journalDb()) {
        emit serverError(404, tr("Could not find local folder for %1").arg(share->path()));
        return;
    }

    const auto removeE2eeShareJob = new UpdateE2eeFolderUsersMetadataJob(account,
                                                                         folder->journalDb(),
                                                                         folder->remotePath(),
                                                                         UpdateE2eeFolderUsersMetadataJob::Remove,
                                                                         share->path(),
                                                                         share->getShareWith()->shareWith());
    removeE2eeShareJob->setParent(_manager.data());
    removeE2eeShareJob->start();
    connect(removeE2eeShareJob, &UpdateE2eeFolderUsersMetadataJob::finished, this, [share, this](int code, const QString &message) {
        if (code != 200) {
            qCWarning(lcShareModel) << "Could not remove share from E2EE folder's metadata!";
            emit serverError(code, message);
            return;
        }
        share->deleteShare();
    });
}

// ----------------------- Shares modification slots ----------------------- //

void ShareModel::toggleShareAllowEditing(const SharePtr &share, const bool enable)
{
    if (share.isNull() || _sharePermissionsChangeInProgress) {
        return;
    }

    auto permissions = share->getPermissions();
    enable ? permissions |= SharePermissionUpdate : permissions &= ~SharePermissionUpdate;

    setSharePermissionChangeInProgress(share->getId(), true);
    share->setPermissions(permissions);
}

void ShareModel::toggleShareAllowEditingFromQml(const QVariant &share, const bool enable)
{
    const auto ptr = share.value<SharePtr>();
    toggleShareAllowEditing(ptr, enable);
}

void ShareModel::toggleShareAllowResharing(const SharePtr &share, const bool enable)
{
    if (share.isNull() || _sharePermissionsChangeInProgress) {
        return;
    }

    auto permissions = share->getPermissions();
    enable ? permissions |= SharePermissionShare : permissions &= ~SharePermissionShare;

    setSharePermissionChangeInProgress(share->getId(), true);
    share->setPermissions(permissions);
}

void ShareModel::toggleHideDownloadFromQml(const QVariant &share, const bool enable)
{
    const auto sharePtr = share.value<SharePtr>();
    if (sharePtr.isNull() || _hideDownloadEnabledChangeInProgress) {
        return;
    }

    const auto linkShare = sharePtr.objectCast<LinkShare>();

    if (linkShare.isNull()) {
        return;
    }

    setHideDownloadEnabledChangeInProgress(linkShare->getId(), true);
    linkShare->setHideDownload(enable);
}

void ShareModel::toggleShareAllowResharingFromQml(const QVariant &share, const bool enable)
{
    const auto ptr = share.value<SharePtr>();
    toggleShareAllowResharing(ptr, enable);
}

void ShareModel::toggleSharePasswordProtect(const SharePtr &share, const bool enable)
{
    if (share.isNull()) {
        return;
    }

    if(!enable) {
        share->setPassword({});
        return;
    }

    const auto randomPassword = generatePassword();
    _shareIdRecentlySetPasswords.insert(share->getId(), randomPassword);
    share->setPassword(randomPassword);
}

void ShareModel::toggleSharePasswordProtectFromQml(const QVariant &share, const bool enable)
{
    const auto ptr = share.value<SharePtr>();
    toggleSharePasswordProtect(ptr, enable);
}

void ShareModel::toggleShareExpirationDate(const SharePtr &share, const bool enable) const
{
    if (share.isNull()) {
        return;
    }

    const auto expireDate = enable ? QDate::currentDate().addDays(1) : QDate();

    if (const auto linkShare = share.objectCast<LinkShare>()) {
        linkShare->setExpireDate(expireDate);
    } else if (const auto userGroupShare = share.objectCast<UserGroupShare>()) {
        userGroupShare->setExpireDate(expireDate);
    }
}

void ShareModel::toggleShareExpirationDateFromQml(const QVariant &share, const bool enable) const
{
    const auto ptr = share.value<SharePtr>();
    toggleShareExpirationDate(ptr, enable);
}

void ShareModel::toggleShareNoteToRecipient(const SharePtr &share, const bool enable) const
{
    if (share.isNull()) {
        return;
    }

    const QString note = enable ? tr("Enter a note for the recipient") : QString();
    if (const auto linkShare = share.objectCast<LinkShare>()) {
        linkShare->setNote(note);
    } else if (const auto userGroupShare = share.objectCast<UserGroupShare>()) {
        userGroupShare->setNote(note);
    }
}

void ShareModel::toggleShareNoteToRecipientFromQml(const QVariant &share, const bool enable) const
{
    const auto ptr = share.value<SharePtr>();
    toggleShareNoteToRecipient(ptr, enable);
}

void ShareModel::changePermissionModeFromQml(const QVariant &share, const OCC::ShareModel::SharePermissionsMode permissionMode)
{
    const auto sharePtr = share.value<SharePtr>();
    if (sharePtr.isNull() || _sharePermissionsChangeInProgress) {
        return;
    }

    const auto shareIndex = _shareIdIndexHash.value(sharePtr->getId());

    if (!checkIndex(shareIndex, QAbstractItemModel::CheckIndexOption::IndexIsValid | QAbstractItemModel::CheckIndexOption::ParentIsInvalid)) {
        qCWarning(lcShareModel) << "Can't change permission mode for:" << sharePtr->getId() << ", invalid share index: " << shareIndex;
        return;
    }

    const auto currentPermissionMode = shareIndex.data(ShareModel::CurrentPermissionModeRole).value<SharePermissionsMode>();

    if (currentPermissionMode == permissionMode) {
        return;
    }

    SharePermissions perm = SharePermissionRead;
    switch (permissionMode) {
    case SharePermissionsMode::ModeViewOnly:
        break;
    case SharePermissionsMode::ModeUploadAndEditing:
        perm |= SharePermissionCreate | SharePermissionUpdate | SharePermissionDelete;
        break;
    case SharePermissionsMode::ModeFileDropOnly:
        perm = SharePermissionCreate;
        break;
    }

    setSharePermissionChangeInProgress(sharePtr->getId(), true);
    sharePtr->setPermissions(perm);
}

void ShareModel::setLinkShareLabel(const QSharedPointer<LinkShare> &linkShare, const QString &label) const
{
    if (linkShare.isNull()) {
        return;
    }

    linkShare->setLabel(label);
}

void ShareModel::setLinkShareLabelFromQml(const QVariant &linkShare, const QString &label) const
{
    // All of our internal share pointers are SharePtr, so cast to LinkShare for this method
    const auto ptr = linkShare.value<SharePtr>().objectCast<LinkShare>();
    setLinkShareLabel(ptr, label);
}

void ShareModel::setShareExpireDate(const SharePtr &share, const qint64 milliseconds) const
{
    if (share.isNull()) {
        return;
    }

    const auto date = QDateTime::fromMSecsSinceEpoch(milliseconds, QTimeZone::utc()).date();

    if (const auto linkShare = share.objectCast<LinkShare>()) {
        linkShare->setExpireDate(date);
    } else if (const auto userGroupShare = share.objectCast<UserGroupShare>()) {
        userGroupShare->setExpireDate(date);
    }
}

void ShareModel::setShareExpireDateFromQml(const QVariant &share, const QVariant milliseconds) const
{
    const auto ptr = share.value<SharePtr>();
    const auto millisecondsLL = milliseconds.toLongLong();
    setShareExpireDate(ptr, millisecondsLL);
}

void ShareModel::setSharePassword(const SharePtr &share, const QString &password)
{
    if (share.isNull()) {
        return;
    }

    _shareIdRecentlySetPasswords.insert(share->getId(), password);
    share->setPassword(password);
}

void ShareModel::setSharePasswordFromQml(const QVariant &share, const QString &password)
{
    const auto ptr = share.value<SharePtr>();
    setSharePassword(ptr, password);
}

void ShareModel::setShareNote(const SharePtr &share, const QString &note) const
{
    if (share.isNull()) {
        return;
    }

    if (const auto linkShare = share.objectCast<LinkShare>()) {
        linkShare->setNote(note);
    } else if (const auto userGroupShare = share.objectCast<UserGroupShare>()) {
        userGroupShare->setNote(note);
    }
}

void ShareModel::setShareNoteFromQml(const QVariant &share, const QString &note) const
{
    const auto ptr = share.value<SharePtr>();
    setShareNote(ptr, note);
}

// ------------------- Share creation and deletion slots ------------------- //

void ShareModel::createNewLinkShare() const
{
    if (isEncryptedItem() && !isSecureFileDropSupportedFolder()) {
        qCWarning(lcShareModel) << "Attempt to create a link share for non-root encrypted folder or a file.";
        return;
    }

    if (_manager) {
        const auto askOptionalPassword = _accountState->account()->capabilities().sharePublicLinkAskOptionalPassword();
        const auto password = askOptionalPassword ? generatePassword() : QString();
        if (isSecureFileDropSupportedFolder()) {
            _manager->createSecureFileDropShare(_sharePath, {}, password);
            return;
        }
        _manager->createLinkShare(_sharePath, {}, password);
    }
}

void ShareModel::createNewLinkShareWithPassword(const QString &password) const
{
    if (_manager) {
        _manager->createLinkShare(_sharePath, QString(), password);
    }
}

void ShareModel::createNewUserGroupShare(const ShareePtr &sharee)
{
    if (sharee.isNull()) {
        return;
    }

    qCInfo(lcShareModel) << "Creating new user/group share for sharee: " << sharee->format();

    if (sharee->type() == Sharee::Email &&
        _accountState &&
        !_accountState->account().isNull() &&
        _accountState->account()->capabilities().isValid() &&
        _accountState->account()->capabilities().shareEmailPasswordEnforced()) {

        Q_EMIT requestPasswordForEmailSharee(sharee);
        return;
    }

    if (isSecureFileDropSupportedFolder()) {
        if (!_synchronizationFolder) {
            qCWarning(lcShareModel) << "Could not share an E2EE folder" << _localPath << "no responsible folder found";
            return;
        }
        _manager->createE2EeShareJob(_sharePath, sharee, _maxSharingPermissions, {});
    } else {
        _manager->createShare(_sharePath, Share::ShareType(sharee->type()), sharee->shareWith(), _maxSharingPermissions, {});
    }
}

void ShareModel::createNewUserGroupShareWithPassword(const ShareePtr &sharee, const QString &password) const
{
    if (sharee.isNull()) {
        return;
    }

    _manager->createShare(_sharePath,
                          Share::ShareType(sharee->type()),
                          sharee->shareWith(),
                          _maxSharingPermissions,
                          password);
}

void ShareModel::createNewUserGroupShareFromQml(const QVariant &sharee)
{
    const auto ptr = sharee.value<ShareePtr>();
    createNewUserGroupShare(ptr);
}

void ShareModel::createNewUserGroupShareWithPasswordFromQml(const QVariant &sharee, const QString &password) const
{
    const auto ptr = sharee.value<ShareePtr>();
    createNewUserGroupShareWithPassword(ptr, password);
}

void ShareModel::deleteShare(const SharePtr &share) const
{
    if(share.isNull()) {
        return;
    }

    if (isEncryptedItem() && Share::isShareTypeUserGroupEmailRoomOrRemote(share->getShareType())) {
        slotDeleteE2EeShare(share);
    } else {
        share->deleteShare();
    }
}

void ShareModel::deleteShareFromQml(const QVariant &share) const
{
    const auto ptr = share.value<SharePtr>();
    deleteShare(ptr);
}

// --------------------------- QPROPERTY methods --------------------------- //

QString ShareModel::localPath() const
{
    return _localPath;
}

void ShareModel::setLocalPath(const QString &localPath)
{
    if (localPath == _localPath) {
        return;
    }

    _localPath = localPath;
    Q_EMIT localPathChanged();
    updateData();
}

AccountState *ShareModel::accountState() const
{
    return _accountState;
}

void ShareModel::setAccountState(AccountState *accountState)
{
    if (accountState == _accountState) {
        return;
    }

    _accountState = accountState;

    // Change the server and account-related properties
    connect(_accountState, &AccountState::stateChanged, this, &ShareModel::accountConnectedChanged);
    connect(_accountState, &AccountState::stateChanged, this, &ShareModel::sharingEnabledChanged);
    connect(_accountState, &AccountState::stateChanged, this, &ShareModel::publicLinkSharesEnabledChanged);
    connect(_accountState, &AccountState::stateChanged, this, &ShareModel::userGroupSharingEnabledChanged);

    Q_EMIT accountStateChanged();
    Q_EMIT accountConnectedChanged();
    Q_EMIT sharingEnabledChanged();
    Q_EMIT publicLinkSharesEnabledChanged();
    Q_EMIT userGroupSharingEnabledChanged();
    Q_EMIT serverAllowsResharingChanged();
    updateData();
}

bool ShareModel::accountConnected() const
{
    return _accountState && _accountState->isConnected();
}

bool ShareModel::validCapabilities() const
{
    return _accountState &&
            _accountState->account() &&
            _accountState->account()->capabilities().isValid();
}

bool ShareModel::isSecureFileDropSupportedFolder() const
{
    return _sharedItemType == SharedItemType::SharedItemTypeEncryptedTopLevelFolder && _accountState->account()->secureFileDropSupported();
}

bool ShareModel::isEncryptedItem() const
{
    return _sharedItemType == SharedItemType::SharedItemTypeEncryptedFile || _sharedItemType == SharedItemType::SharedItemTypeEncryptedFolder
        || _sharedItemType == SharedItemType::SharedItemTypeEncryptedTopLevelFolder;
}

bool ShareModel::sharingEnabled() const
{
    return validCapabilities() &&
            _accountState->account()->capabilities().shareAPI();
}

bool ShareModel::publicLinkSharesEnabled() const
{
    return Theme::instance()->linkSharing() &&
            validCapabilities() &&
            _accountState->account()->capabilities().sharePublicLink();
}

bool ShareModel::userGroupSharingEnabled() const
{
    return Theme::instance()->userGroupSharing();
}

bool ShareModel::fetchOngoing() const
{
    return _fetchOngoing;
}

bool ShareModel::hasInitialShareFetchCompleted() const
{
    return _hasInitialShareFetchCompleted;
}

bool ShareModel::canShare() const
{
    return _maxSharingPermissions & SharePermissionShare;
}

bool ShareModel::serverAllowsResharing() const
{
    return _accountState && _accountState->account() && _accountState->account()->capabilities().isValid()
        && _accountState->account()->capabilities().shareResharing();
}

bool ShareModel::isShareDisabledEncryptedFolder() const
{
    return _isShareDisabledEncryptedFolder;
}

QVariantList ShareModel::sharees() const
{
    QVariantList returnSharees;
    for (const auto &sharee : _sharees) {
        returnSharees.append(QVariant::fromValue(sharee));
    }
    return returnSharees;
}

QString ShareModel::generatePassword()
{
    constexpr auto asciiMin = 33;
    constexpr auto asciiMax = 126;
    constexpr auto asciiRange = asciiMax - asciiMin;
    static constexpr auto numChars = 24;

    static constexpr std::string_view lowercaseAlphabet = "abcdefghijklmnopqrstuvwxyz";
    static constexpr std::string_view uppercaseAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr std::string_view numbers = "0123456789";
    static constexpr std::string_view specialChars = R"(ªº\\/|"'*+-_´¨{}·#$%&()=\[\]<>;:@~)";

    static const QRegularExpression lowercaseMatch("[a-z]");
    static const QRegularExpression uppercaseMatch("[A-Z]");
    static const QRegularExpression numberMatch("[0-9]");
    static const QRegularExpression specialCharMatch(QString("[%1]").arg(specialChars.data()));

    static const std::map<std::string_view, QRegularExpression> matchMap{
        {lowercaseAlphabet, lowercaseMatch},
        {uppercaseAlphabet, uppercaseMatch},
        {numbers, numberMatch},
        {specialChars, specialCharMatch},
    };

    std::random_device rand_dev;
    std::mt19937 rng(rand_dev());

    QString passwd;
    std::array<unsigned char, numChars> unsignedCharArray;

    RAND_bytes(unsignedCharArray.data(), numChars);

    for (const auto newChar : unsignedCharArray) {
        // Ensure byte is within asciiRange
        const auto byte = (newChar % (asciiRange + 1)) + asciiMin;
        passwd.append(byte);
    }

    for (const auto &charsWithMatcher : matchMap) {
        const auto selectionChars = charsWithMatcher.first;
        const auto matcher = charsWithMatcher.second;
        Q_ASSERT(matcher.isValid());

        if (matcher.match(passwd).hasMatch()) {
            continue;
        }

        // add random required character at random position
        std::uniform_int_distribution<std::mt19937::result_type> passwdDist(0, passwd.length() - 1);
        std::uniform_int_distribution<std::mt19937::result_type> charsDist(0, selectionChars.length() - 1);

        const auto passwdInsertIndex = passwdDist(rng);
        const auto charToInsertIndex = charsDist(rng);
        const auto charToInsert = selectionChars.at(charToInsertIndex);

        passwd.insert(passwdInsertIndex, charToInsert);
    }

    return passwd;
}

} // namespace OCC
