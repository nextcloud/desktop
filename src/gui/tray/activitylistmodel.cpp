/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "activitylistmodel.h"

#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "conflictdialog.h"
#include "folderman.h"
#include "owncloudgui.h"
#include "guiutility.h"
#include "invalidfilenamedialog.h"
#include "caseclashfilenamedialog.h"
#include "activitydata.h"
#include "systray.h"

#include <QtCore>
#include <QAbstractListModel>
#include <QDesktopServices>
#include <QWidget>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcActivity, "nextcloud.gui.activity", QtInfoMsg)

ActivityListModel::ActivityListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

ActivityListModel::ActivityListModel(AccountState *accountState,
    QObject *parent)
    : QAbstractListModel(parent)
    , _accountState(accountState)
{
    if (_accountState) {
        connect(_accountState, &AccountState::stateChanged,
                this, &ActivityListModel::accountStateChanged);
        _accountStateWasConnected = false;
    }
}

QHash<int, QByteArray> ActivityListModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[DisplayPathRole] = "displayPath";
    roles[PathRole] = "path";
    roles[OpenablePathRole] = "openablePath";
    roles[DisplayLocationRole] = "displayLocation";
    roles[LinkRole] = "link";
    roles[MessageRole] = "message";
    roles[ActionRole] = "type";
    roles[DarkIconRole] = "darkIcon";
    roles[LightIconRole] = "lightIcon";
    roles[ActionTextRole] = "subject";
    roles[ActionsLinksRole] = "links";
    roles[ActionsLinksContextMenuRole] = "linksContextMenu";
    roles[ActionsLinksForActionButtonsRole] = "linksForActionButtons";
    roles[ActionTextColorRole] = "activityTextTitleColor";
    roles[ObjectTypeRole] = "objectType";
    roles[ObjectIdRole] = "objectId";
    roles[ObjectNameRole] = "objectName";
    roles[PointInTimeRole] = "dateTime";
    roles[DisplayActions] = "displayActions";
    roles[ShowFileDetailsRole] = "showFileDetails";
    roles[ShareableRole] = "isShareable";
    roles[DismissableRole] = "isDismissable";
    roles[IsCurrentUserFileActivityRole] = "isCurrentUserFileActivity";
    roles[IsCurrentUserFileActivityRole] = "isCurrentUserFileActivity";
    roles[ThumbnailRole] = "thumbnail";
    roles[TalkNotificationConversationTokenRole] = "conversationToken";
    roles[TalkNotificationMessageIdRole] = "messageId";
    roles[TalkNotificationMessageSentRole] = "messageSent";
    roles[TalkNotificationUserAvatarRole] = "userAvatar";
    roles[ActivityIndexRole] = "activityIndex";
    roles[ActivityRole] = "activity";

    return roles;
}

void ActivityListModel::setAccountState(AccountState *state)
{
    if (_accountState == state) {
        return;
    }

    _accountState = state;
    Q_EMIT accountStateChanged();
    if (_accountState) {
        connect(_accountState, &AccountState::stateChanged,
                this, &ActivityListModel::accountStateHasChanged);
        _accountStateWasConnected = false;
    }
}

void ActivityListModel::setCurrentItem(const int currentItem)
{
    _currentItem = currentItem;
}

void ActivityListModel::setAndRefreshCurrentlyFetching(bool value)
{
    if (_currentlyFetching == value) {
        return;
    }
    _currentlyFetching = value;
    insertOrRemoveDummyFetchingActivity();
}

bool ActivityListModel::currentlyFetching() const
{
    return _currentlyFetching;
}

void ActivityListModel::setDoneFetching(bool value)
{
    _doneFetching = value;
}

void ActivityListModel::setHideOldActivities(bool value)
{
    _hideOldActivities = value;
}

void ActivityListModel::setDisplayActions(bool value)
{
    _displayActions = value;
}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid | QAbstractItemModel::CheckIndexOption::ParentIsInvalid));

    const auto a = _finalList.at(index.row());
    AccountStatePtr ast = AccountManager::instance()->account(a._accName);
    if (!ast && _accountState != ast.data())
        return QVariant();

    const auto getFilePath = [&]() {
        const auto fileName = a._fileAction == QStringLiteral("file_renamed") ? a._renamedFile : a._file;
        if (!fileName.isEmpty()) {
            const auto folder = FolderMan::instance()->folder(a._folder);

            const QString relPath = folder ? folder->remotePathTrailingSlash() + fileName : fileName;

            const auto localFiles = FolderMan::instance()->findFileInLocalFolders(relPath, ast->account());

            if (localFiles.isEmpty()) {
                return QString();
            }

            // If this is an E2EE file or folder, pretend we got no path, hiding the share button which is what we want
            if (folder) {
                SyncJournalFileRecord rec;
                if (!folder->journalDb()->getFileRecord(fileName.mid(1), &rec)) {
                    qCWarning(lcActivity) << "could not get file from local DB" << fileName.mid(1);
                }
                if (rec.isValid() && (rec.isE2eEncrypted() || !rec._e2eMangledName.isEmpty())) {
                    return QString();
                }
            }

            return localFiles.constFirst();
        }
        return QString();
    };

    const auto getDisplayPath = [&a, &ast]() {
        if (!a._file.isEmpty()) {
            const auto folder = FolderMan::instance()->folder(a._folder);

            QString relPath = folder ? folder->remotePathTrailingSlash() + a._file : a._file;

            const auto localFiles = FolderMan::instance()->findFileInLocalFolders(relPath, ast->account());

            if (localFiles.count() > 0) {
                if (relPath.startsWith('/') || relPath.startsWith('\\')) {
                    return relPath.remove(0, 1);
                } else {
                    return relPath;
                }
            }
        }
        return QString();
    };

    const auto displayLocation = [&]() {
        const auto displayPath = QFileInfo(getDisplayPath()).path();
        return displayPath == "." || displayPath == "/" ? QString() : displayPath;
    };

    const auto generatePreviewMap = [](const PreviewData &preview) {
        return(QVariantMap {
            {QStringLiteral("source"), QStringLiteral("image://tray-image-provider/").append(preview._source)},
               {QStringLiteral("link"), preview._link},
               {QStringLiteral("mimeType"), preview._mimeType},
               {QStringLiteral("fileId"), preview._fileId},
               {QStringLiteral("view"), preview._view},
               {QStringLiteral("isMimeTypeIcon"), preview._isMimeTypeIcon},
               {QStringLiteral("filename"), preview._filename},
               {QStringLiteral("isUserAvatar"), false},
        });
    };

    const auto generateAvatarThumbnailMap = [](const QString &avatarThumbnailUrl) {
        return QVariantMap {
            {QStringLiteral("source"), avatarThumbnailUrl},
            {QStringLiteral("isMimeTypeIcon"), false},
            {QStringLiteral("isUserAvatar"), true},
        };
    };

    const auto generateIconPath = [&]() {
        auto colorIconPath = role == DarkIconRole ? QStringLiteral("qrc:///client/theme/white/") : QStringLiteral("qrc:///client/theme/black/");
        if (a._type == Activity::NotificationType && !a._talkNotificationData.userAvatar.isEmpty()) {
            return QStringLiteral("qrc:///client/theme/colored/talk-bordered.svg");
        } else if (a._type == Activity::SyncResultType) {
            colorIconPath.append("state-error.svg");
            return colorIconPath;
        } else if (a._type == Activity::SyncFileItemType) {
            if (a._syncFileItemStatus == SyncFileItem::NormalError
                || a._syncFileItemStatus == SyncFileItem::FatalError
                || a._syncFileItemStatus == SyncFileItem::DetailError
                || a._syncFileItemStatus == SyncFileItem::BlacklistedError) {
                colorIconPath.append("state-error.svg");
                return colorIconPath;
            } else if (a._syncFileItemStatus == SyncFileItem::SoftError
                || a._syncFileItemStatus == SyncFileItem::Conflict
                || a._syncFileItemStatus == SyncFileItem::Restoration
                || a._syncFileItemStatus == SyncFileItem::FileLocked
                || a._syncFileItemStatus == SyncFileItem::FileNameInvalid
                || a._syncFileItemStatus == SyncFileItem::FileNameInvalidOnServer
                || a._syncFileItemStatus == SyncFileItem::FileNameClash) {
                colorIconPath.append("state-warning.svg");
                return colorIconPath;
            } else if (a._syncFileItemStatus == SyncFileItem::FileIgnored) {
                colorIconPath.append("state-info.svg");
                return colorIconPath;
            } else {
                // File sync successful
                if (a._fileAction == "file_created") {
                    return a._previews.empty() ? QStringLiteral("qrc:///client/theme/colored/add.svg")
                                               : QStringLiteral("qrc:///client/theme/colored/add-bordered.svg");
                } else if (a._fileAction == "file_deleted") {
                    return a._previews.empty() ? QStringLiteral("qrc:///client/theme/colored/delete.svg")
                                               : QStringLiteral("qrc:///client/theme/colored/delete-bordered.svg");
                } else {
                    return a._previews.empty() ? colorIconPath % QStringLiteral("change.svg")
                                               : QStringLiteral("qrc:///client/theme/colored/change-bordered.svg");
                }
            }
        } else {
            // We have an activity
            if (a._icon.isEmpty()) {
                colorIconPath.append("activity.svg");
                return colorIconPath;
            }

            const QString basePath = QStringLiteral("image://tray-image-provider/") % a._icon % QStringLiteral("/");
            return role == DarkIconRole ? QString(basePath + QStringLiteral("white")) : QString(basePath + QStringLiteral("black"));
        }
    };

    switch (role) {
    case DisplayPathRole:
        return getDisplayPath();
    case PathRole:
        return QFileInfo(getFilePath()).path();
    case OpenablePathRole:
        return a._isMultiObjectActivity ? QFileInfo(getFilePath()).canonicalPath() : QFileInfo(getFilePath()).canonicalFilePath();
    case DisplayLocationRole:
        return displayLocation();
    case ActionsLinksRole: {
        QList<QVariant> customList;
        foreach (ActivityLink activityLink, a._links) {
            customList << QVariant::fromValue(activityLink);
        }
        return customList;
    }

    case ActionsLinksContextMenuRole: {
        return ActivityListModel::convertLinksToMenuEntries(a);
    }

    case ActionsLinksForActionButtonsRole: {
        return ActivityListModel::convertLinksToActionButtons(a);
    }

    case DarkIconRole:
    case LightIconRole:
        return generateIconPath();
    case ObjectTypeRole:
        return a._objectType;
    case ObjectIdRole:
        return a._objectId;
    case ObjectNameRole:
        return a._objectName;
    case ActionRole: {
        switch (a._type) {
        case Activity::ActivityType:
        case Activity::DummyFetchingActivityType:
        case Activity::DummyMoreActivitiesAvailableType:
            return "Activity";
        case Activity::NotificationType:
            return "Notification";
        case Activity::SyncFileItemType:
            return "File";
        case Activity::SyncResultType:
            return "Sync";
        default:
            return QVariant();
        }
    }
    case ActionTextRole:
        if(a._subjectDisplay.isEmpty()) {
            return a._subject;
        }

        return a._subjectDisplay;
    case ActionTextColorRole:
        return a._id == -1 ? QLatin1String("#808080") : QLatin1String("#222");   // FIXME: This is a temporary workaround for _showMoreActivitiesAvailableEntry
    case MessageRole:
        return a._message;
    case LinkRole: {
        if (a._link.isEmpty()) {
            return "";
        } else {
            return a._link.toString();
        }
    }
    case AccountRole:
        return a._accName;
    case PointInTimeRole:
        //return a._id == -1 ? "" : QString("%1 - %2").arg(Utility::timeAgoInWords(a._dateTime.toLocalTime()), a._dateTime.toLocalTime().toString(Qt::DefaultLocaleShortDate));
        return a._id == -1 ? "" : Utility::timeAgoInWords(a._dateTime.toLocalTime());
    case AccountConnectedRole:
        return (ast && ast->isConnected());
    case DisplayActions:
        return _displayActions;
    case ShowFileDetailsRole:
        return _displayActions &&
                a._objectType == QStringLiteral("files") &&
                a._fileAction != "file_deleted" &&
                a._syncFileItemStatus != SyncFileItem::FileIgnored &&
                !data(index, OpenablePathRole).toString().isEmpty();
    case DismissableRole:
        // Do not allow dismissal of things requiring user input regarding syncing
        return !a._links.isEmpty() &&
                a._syncFileItemStatus != SyncFileItem::FileNameClash &&
                a._syncFileItemStatus != SyncFileItem::Conflict &&
                a._syncFileItemStatus != SyncFileItem::FileNameInvalid &&
                a._syncFileItemStatus != SyncFileItem::FileNameInvalidOnServer;
    case IsCurrentUserFileActivityRole:
        return a._isCurrentUserFileActivity;
    case ThumbnailRole: {
        if (a._type == Activity::NotificationType && !a._talkNotificationData.userAvatar.isEmpty()) {
            return generateAvatarThumbnailMap(a._talkNotificationData.userAvatar);
        }

        if(a._previews.empty()) {
            return {};
        }

        const auto preview = a._previews[0];
        return(generatePreviewMap(preview));
    }
    case TalkNotificationConversationTokenRole:
        return a._talkNotificationData.conversationToken;
    case TalkNotificationMessageIdRole:
        return a._talkNotificationData.messageId;
    case TalkNotificationMessageSentRole:
        return replyMessageSent(a);
    case TalkNotificationUserAvatarRole:
        return a._talkNotificationData.userAvatar;
    case ActivityIndexRole:
        return index.row();
    case ActivityRole:
        return QVariant::fromValue(a);
    }

    return QVariant();
}

int ActivityListModel::rowCount(const QModelIndex &parent) const
{
    if(parent.isValid()) {
        return 0;
    }

    return _finalList.count();
}

bool ActivityListModel::canFetchMore(const QModelIndex &) const
{
    // We need to be connected to be able to fetch more
    if (_accountState && _accountState->isConnected() && Systray::instance()->isOpen()) {
        // If the fetching is reported to be done or we are currently fetching we can't fetch more
        if (!_doneFetching && !currentlyFetching()) {
            return true;
        }
    }

    return false;
}

void ActivityListModel::startFetchJob()
{
    if (!_accountState->isConnected() || currentlyFetching()) {
        return;
    }
    auto *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/apps/activity/api/v2/activity"), this);
    QObject::connect(job, &JsonApiJob::jsonReceived,
        this, &ActivityListModel::activitiesReceived);

    QUrlQuery params;
    params.addQueryItem(QLatin1String("previews"), QLatin1String("true"));
    params.addQueryItem(QLatin1String("since"), QString::number(_currentItem));
    params.addQueryItem(QLatin1String("limit"), QString::number(50));
    job->addQueryParams(params);

    setAndRefreshCurrentlyFetching(true);
    qCInfo(lcActivity) << "Start fetching activities for " << _accountState->account()->displayName();
    job->start();
}

int ActivityListModel::currentItem() const
{
    return _currentItem;
}

void ActivityListModel::ingestActivities(const QJsonArray &activities)
{
    ActivityList list;

    QDateTime oldestDate = QDateTime::currentDateTime();
    oldestDate = oldestDate.addDays(static_cast<qint64>(_maxActivitiesDays) * -1);

    for (const auto &activity : activities) {
        const auto json = activity.toObject();

        auto a = Activity::fromActivityJson(json, _accountState->account());

        if(_presentedActivities.contains(a._id)) {
            continue;
        }

        list.append(a);
        _presentedActivities.insert(a._id);
        _currentItem = list.last()._id;

        if (_presentedActivities.count() >= _maxActivities
            || (_hideOldActivities && a._dateTime < oldestDate)) {
            _showMoreActivitiesAvailableEntry = true;
            _doneFetching = true;
            break;
        }
    }

    if (list.size() > 0) {
        addEntriesToActivityList(list);
        appendMoreActivitiesAvailableEntry();
        _activityLists.append(list);
    }
}

void ActivityListModel::appendMoreActivitiesAvailableEntry()
{
    const QString moreActivitiesEntryObjectType = QLatin1String("activity_fetch_more_activities");
    if (_showMoreActivitiesAvailableEntry && !_finalList.isEmpty()
        && _finalList.last()._objectType != moreActivitiesEntryObjectType) {

        Activity a;
        a._type = Activity::DummyMoreActivitiesAvailableType;
        a._accName = _accountState->account()->displayName();
        a._id = -1;
        a._objectType = moreActivitiesEntryObjectType;
        a._subject = tr("For more activities please open the Activity app.");
        a._dateTime = QDateTime::currentDateTime();

        if (const auto *app = _accountState->findApp(QLatin1String("activity"))) {
            a._link = app->url();
        }

        addEntriesToActivityList({a});
    }
}

void ActivityListModel::insertOrRemoveDummyFetchingActivity()
{
    const QString dummyFetchingActivityObjectType = QLatin1String("dummy_fetching_activity");

    if (_currentlyFetching && _finalList.isEmpty()) {
        _dummyFetchingActivities._type = Activity::DummyFetchingActivityType;
        _dummyFetchingActivities._accName = _accountState->account()->displayName();
        _dummyFetchingActivities._id = -2;
        _dummyFetchingActivities._objectType = dummyFetchingActivityObjectType;
        _dummyFetchingActivities._subject = tr("Fetching activities …");
        _dummyFetchingActivities._dateTime = QDateTime::currentDateTime();
        _dummyFetchingActivities._icon = QLatin1String("qrc:///client/theme/colored/change-bordered.svg");

        addEntriesToActivityList({_dummyFetchingActivities});
    } else if (!_finalList.isEmpty() && _finalList.first()._objectType == dummyFetchingActivityObjectType) {
        removeActivityFromActivityList(_dummyFetchingActivities);
    }
}

void ActivityListModel::activitiesReceived(const QJsonDocument &json, int statusCode)
{
    const auto activities = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toArray();

    if (!_accountState) {
        return;
    }

    if (activities.empty()) {
        _doneFetching = true;
    }

    setAndRefreshCurrentlyFetching(false);

    ingestActivities(activities);

    emit activityJobStatusCode(statusCode);
}

void ActivityListModel::addEntriesToActivityList(const ActivityList &activityList)
{
    if(activityList.isEmpty()) {
        return;
    }

    const auto startRow = _finalList.count();

    beginInsertRows({}, startRow, startRow + activityList.count() - 1);
    for(const auto &activity : activityList) {
        _finalList.append(activity);
    }
    endInsertRows();

    const auto deselectedConflictIt = std::find_if(_finalList.constBegin(), _finalList.constEnd(), [] (const auto activity) {
        return activity._syncFileItemStatus == SyncFileItem::Conflict;
    });
    const auto conflictsFound = (deselectedConflictIt != _finalList.constEnd());

    setHasSyncConflicts(conflictsFound);
}

void ActivityListModel::accountStateHasChanged()
{
    if (!_accountState) {
        return;
    }

    if (_accountStateWasConnected == _accountState->isConnected()) {
        return;
    }

    if (!_accountState->isConnected()) {
        _durationSinceDisconnection.start();
    } else {
        _durationSinceDisconnection.invalidate();
    }

    _accountStateWasConnected = _accountState->isConnected();
}

void ActivityListModel::addErrorToActivityList(const Activity &activity, const ErrorType type)
{
    auto shouldAddError = false;

    switch (type)
    {
    case ErrorType::NetworkError:
        if (_durationSinceDisconnection.isValid() && _durationSinceDisconnection.hasExpired(3 * 60 *1000)) {
            shouldAddError = true;
        }
        break;
    case ErrorType::SyncError:
        shouldAddError = true;
        break;
    }

    if (shouldAddError) {
        qCDebug(lcActivity) << "Error successfully added to the notification list: " << type << activity._message << activity._subject << activity._syncResultStatus << activity._syncFileItemStatus;
        auto modifiedActivity = activity;
        if (type == ErrorType::NetworkError) {
            modifiedActivity._subject = tr("Network error occurred: client will retry syncing.");
        }
        addEntriesToActivityList({modifiedActivity});
        _notificationErrorsLists.prepend(modifiedActivity);
    }
}

void ActivityListModel::addIgnoredFileToList(const Activity &newActivity)
{
    qCInfo(lcActivity) << "First checking for duplicates then add file to the notification list of ignored files: " << newActivity._file;

    bool duplicate = false;
    if (_listOfIgnoredFiles.size() == 0) {
        _notificationIgnoredFiles = newActivity;
        _notificationIgnoredFiles._subject = tr("Files from the ignore list as well as symbolic links are not synced.");
        addEntriesToActivityList({_notificationIgnoredFiles});
        _listOfIgnoredFiles.append(newActivity);
        return;
    }

    foreach (Activity activity, _listOfIgnoredFiles) {
        if (activity._file == newActivity._file) {
            duplicate = true;
            break;
        }
    }

    if (!duplicate) {
        _notificationIgnoredFiles._message.append(", " + newActivity._file);
    }
}

void ActivityListModel::addNotificationToActivityList(const Activity &activity)
{
    qCDebug(lcActivity) << "Notification successfully added to the notification list: " << activity._subject;
    addEntriesToActivityList({activity});
    _notificationLists.prepend(activity);
    for (const auto &link : activity._links) {
        if (link._verb == QByteArrayLiteral("POST")
            || link._verb == QByteArrayLiteral("REPLY")
            || link._verb == QByteArrayLiteral("WEB")) {
            emit interactiveActivityReceived();
        }
    }
}

void ActivityListModel::addSyncFileItemToActivityList(const Activity &activity)
{
    qCDebug(lcActivity) << "Successfully added to the activity list: " << activity._subject;
    addEntriesToActivityList({activity});
    _syncFileItemLists.prepend(activity);
}

void ActivityListModel::removeActivityFromActivityList(int row)
{
    Activity activity = _finalList.at(row);
    removeActivityFromActivityList(activity);
}

void ActivityListModel::removeActivityFromActivityList(const Activity &activity)
{
    qCInfo(lcActivity) << "Activity/Notification/Error successfully dismissed: " << activity._subject;
    qCInfo(lcActivity) << "Trying to remove Activity/Notification/Error from view... ";

    const auto index = _finalList.indexOf(activity);
    if (index != -1) {
        qCInfo(lcActivity) << "Activity/Notification/Error successfully removed from the list.";
        qCInfo(lcActivity) << "Updating Activity/Notification/Error view.";

        beginRemoveRows({}, index, index);
        _finalList.removeAt(index);
        endRemoveRows();
    }

    if (activity._type != Activity::ActivityType &&
            activity._type != Activity::DummyFetchingActivityType &&
            activity._type != Activity::DummyMoreActivitiesAvailableType &&
            activity._type != Activity::NotificationType) {

        const auto notificationErrorsListIndex = _notificationErrorsLists.indexOf(activity);
        if (notificationErrorsListIndex != -1)
            _notificationErrorsLists.removeAt(notificationErrorsListIndex);
    }
}

void ActivityListModel::checkAndRemoveSeenActivities(const OCC::ActivityList &newActivities)
{
    ActivityList activitiesToRemove;
    for (const auto &activity : _finalList) {
        const auto isTalkActiity = activity._objectType == QStringLiteral("chat") ||
            activity._objectType == QStringLiteral("call");
        if (isTalkActiity && !newActivities.contains(activity)) {
            activitiesToRemove.push_back(activity);
        }
    }

    for (const auto &toRemove : activitiesToRemove) {
        removeActivityFromActivityList(toRemove);
    }
}

void ActivityListModel::slotTriggerDefaultAction(const int activityIndex)
{
    if (activityIndex < 0 || activityIndex >= _finalList.size()) {
        qCWarning(lcActivity) << "Couldn't trigger default action at index" << activityIndex << "/ final list size:" << _finalList.size();
        return;
    }

    const auto modelIndex = index(activityIndex);
    const auto path = data(modelIndex, PathRole).toString();

    const auto activity = _finalList.at(activityIndex);
    if (activity._syncFileItemStatus == SyncFileItem::Conflict) {
        displaySingleConflictDialog(activity);

        return;
    } else if (activity._syncFileItemStatus == SyncFileItem::FileNameClash) {
        triggerCaseClashAction(activity);
        return;
    } else if (activity._syncFileItemStatus == SyncFileItem::FileNameInvalid
               || activity._syncFileItemStatus == SyncFileItem::FileNameInvalidOnServer) {
        if (!_currentInvalidFilenameDialog.isNull()) {
            _currentInvalidFilenameDialog->close();
        }

        auto folder = FolderMan::instance()->folder(activity._folder);
        const auto folderDir = QDir(folder->path());
        const auto fileLocation = activity._syncFileItemStatus == SyncFileItem::FileNameInvalidOnServer
            ? InvalidFilenameDialog::FileLocation::NewLocalFile
            : InvalidFilenameDialog::FileLocation::Default;

        _currentInvalidFilenameDialog = new InvalidFilenameDialog(_accountState->account(), folder,
            folderDir.filePath(activity._file), fileLocation);
        connect(_currentInvalidFilenameDialog, &InvalidFilenameDialog::accepted, folder, [folder]() {
            folder->scheduleThisFolderSoon();
        });
        connect(_currentInvalidFilenameDialog, &InvalidFilenameDialog::acceptedInvalidName, folder, [folder](const QString& filePath) {
            folder->acceptInvalidFileName(filePath);
            folder->scheduleThisFolderSoon();
        });
        _currentInvalidFilenameDialog->open();
        ownCloudGui::raiseDialog(_currentInvalidFilenameDialog);
        return;
    }

    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else {
        const auto link = data(modelIndex, LinkRole).toUrl();
        Utility::openBrowser(link);
    }
}

void ActivityListModel::triggerCaseClashAction(Activity activity)
{
    qCInfo(lcActivity) << "case clash conflict" << activity._file << activity._syncFileItemStatus;

    if (!_currentCaseClashFilenameDialog.isNull()) {
        _currentCaseClashFilenameDialog->close();
    }

    auto folder = FolderMan::instance()->folder(activity._folder);
    const auto conflictedRelativePath = activity._file;
    const auto conflictRecord = folder->journalDb()->caseConflictRecordByBasePath(conflictedRelativePath);

    const auto dir = QDir(folder->path());
    const auto conflictedPath = dir.filePath(conflictedRelativePath);
    const auto conflictTaggedPath = dir.filePath(conflictRecord.path);

    _currentCaseClashFilenameDialog = new CaseClashFilenameDialog(_accountState->account(),
                                                                  folder,
                                                                  conflictedPath,
                                                                  conflictTaggedPath);
    connect(_currentCaseClashFilenameDialog, &CaseClashFilenameDialog::successfulRename, folder, [folder, activity](const QString& filePath) {
        qCInfo(lcActivity) << "successfulRename" << filePath << activity._message;
        folder->acceptCaseClashConflictFileName(activity._message);
        folder->scheduleThisFolderSoon();
    });
    _currentCaseClashFilenameDialog->open();
    ownCloudGui::raiseDialog(_currentCaseClashFilenameDialog);
}

void ActivityListModel::displaySingleConflictDialog(const Activity &activity)
{
    Q_ASSERT(!activity._file.isEmpty());
    Q_ASSERT(!activity._folder.isEmpty());
    Q_ASSERT(Utility::isConflictFile(activity._file));

    const auto folder = FolderMan::instance()->folder(activity._folder);

    const auto conflictedRelativePath = activity._file;
    const auto baseRelativePath = folder->journalDb()->conflictFileBaseName(conflictedRelativePath.toUtf8());

    const auto dir = QDir(folder->path());
    const auto conflictedPath = dir.filePath(conflictedRelativePath);
    const auto basePath = dir.filePath(baseRelativePath);

    const auto baseName = QFileInfo(basePath).fileName();

    if (!_currentConflictDialog.isNull()) {
        _currentConflictDialog->close();
    }
    _currentConflictDialog = new ConflictDialog;
    _currentConflictDialog->setBaseFilename(baseName);
    _currentConflictDialog->setLocalVersionFilename(conflictedPath);
    _currentConflictDialog->setRemoteVersionFilename(basePath);
    _currentConflictDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(_currentConflictDialog, &ConflictDialog::accepted, folder, [folder]() {
        folder->scheduleThisFolderSoon();
    });
    _currentConflictDialog->open();
    ownCloudGui::raiseDialog(_currentConflictDialog);
}

void ActivityListModel::setHasSyncConflicts(bool conflictsFound)
{
    if (_hasSyncConflicts != conflictsFound) {
        _hasSyncConflicts = conflictsFound;
        emit hasSyncConflictsChanged();
    }
}

void ActivityListModel::slotTriggerAction(const int activityIndex, const int actionIndex)
{
    if (activityIndex < 0 || activityIndex >= _finalList.size()) {
        qCWarning(lcActivity) << "Couldn't trigger action on activity at index" << activityIndex << "/ final list size:" << _finalList.size();
        return;
    }

    const auto activity = _finalList[activityIndex];

    if (actionIndex < 0 || actionIndex >= activity._links.size()) {
        qCWarning(lcActivity) << "Couldn't trigger action at index" << actionIndex << "/ actions list size:" << activity._links.size();
        return;
    }

    const auto action = activity._links[actionIndex];

    if (action._verb == "WEB") {
        Utility::openBrowser(QUrl(action._link));
        return;
    } else if (action._verb == "FIX_CONFLICT_LOCALLY" &&
               activity._type == Activity::SyncFileItemType &&
               (activity._syncFileItemStatus == SyncFileItem::Conflict || activity._syncFileItemStatus == SyncFileItem::FileNameClash)) {
        slotTriggerDefaultAction(activityIndex);
        return;
    } else if (action._verb == ActivityLink::WhitelistFolderVerb && !activity._file.isEmpty()) { // _folder == folder alias/name, _file == folder/file path
        FolderMan::instance()->whitelistFolderPath(activity._file);
        removeActivityFromActivityList(activity);
        return;
    } else if (action._verb == ActivityLink::BlacklistFolderVerb && !activity._file.isEmpty()) {
        FolderMan::instance()->blacklistFolderPath(activity._file);
        removeActivityFromActivityList(activity);
        return;
    }

    emit sendNotificationRequest(activity._accName, action._link, action._verb, activityIndex);
}

void ActivityListModel::slotTriggerDismiss(const int activityIndex)
{
    if (activityIndex < 0 || activityIndex >= _finalList.size()) {
        qCWarning(lcActivity) << "Couldn't trigger action on activity at index" << activityIndex << "/ final list size:" << _finalList.size();
        return;
    }

    constexpr auto deleteVerb = "DELETE";
    const auto activity = _finalList[activityIndex];

    emit sendNotificationRequest(activity._accName, Utility::concatUrlPath(accountState()->account()->url(), "ocs/v2.php/apps/notifications/api/v2/notifications/" + QString::number(activity._id)).toString(), deleteVerb, activityIndex);
}

AccountState *ActivityListModel::accountState() const
{
    return _accountState;
}

QVariantList ActivityListModel::convertLinksToActionButtons(const Activity &activity)
{
    QVariantList customList;

    for (int i = 0; i < activity._links.size() && static_cast<quint32>(i) <= maxActionButtons(); ++i) {
        const auto activityLink = activity._links[i];

        // Use the isDismissable model role to present custom dismiss button if needed
        // Also don't show "View chat" for talk activities, default action will open chat anyway
        const auto isUseCustomDeleteAction = activityLink._verb == "DELETE"
            && activity._objectType != QStringLiteral("remote_share");
        if (isUseCustomDeleteAction || (activityLink._verb == "WEB" && activity._objectType == "chat")) {
            continue;
        }

        customList << ActivityListModel::convertLinkToActionButton(activityLink);
    }

    return customList;
}

QVariant ActivityListModel::convertLinkToActionButton(const OCC::ActivityLink &activityLink)
{
    auto activityLinkCopy = activityLink;

    const auto isReplyIconApplicable = activityLink._verb == QStringLiteral("REPLY");

    const QString replyButtonPath = QStringLiteral("image://svgimage-custom-color/reply.svg");

    if (isReplyIconApplicable) {
        activityLinkCopy._imageSource = QString(replyButtonPath + "/");
        activityLinkCopy._imageSourceHovered = QString(replyButtonPath + "/");
    }

    return QVariant::fromValue(activityLinkCopy);
}

QVariantList ActivityListModel::convertLinksToMenuEntries(const Activity &activity)
{
    if (static_cast<quint32>(activity._links.size()) <= maxActionButtons()) {
        return {};
    }

    QVariantList customList;

    for (int i = maxActionButtons(); i < activity._links.size(); ++i) {
        const auto activityLinkLabel = activity._links[i]._label;
        const auto menuEntry = QVariantMap{{"actionIndex", i}, {"label", activityLinkLabel}};
        customList << menuEntry;
    }

    return customList;
}

bool ActivityListModel::canFetchActivities() const
{
    return _accountState->isConnected() && _accountState->account()->capabilities().hasActivities();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    if (canFetchActivities()) {
        startFetchJob();
    }
}

void ActivityListModel::slotRefreshActivity()
{
    _doneFetching = false;
    _currentItem = 0;
    _showMoreActivitiesAvailableEntry = false;

    if (canFetchActivities()) {
        startFetchJob();
    } else {
        _doneFetching = true;
    }
}

void ActivityListModel::slotRefreshActivityInitial()
{
    if (_activityLists.isEmpty() && !currentlyFetching()) {
        slotRefreshActivity();
    }
}

void ActivityListModel::slotRemoveAccount()
{
    _finalList.clear();
    _activityLists.clear();
    _presentedActivities.clear();
    setAndRefreshCurrentlyFetching(false);
    _doneFetching = false;
    _currentItem = 0;
    _showMoreActivitiesAvailableEntry = false;
}

void ActivityListModel::setReplyMessageSent(const int activityIndex, const QString &message)
{
    if (activityIndex < 0 || activityIndex >= _finalList.size()) {
        qCWarning(lcActivity) << "Couldn't trigger action on activity at index" << activityIndex << "/ final list size:" << _finalList.size();
        return;
    }

    _finalList[activityIndex]._talkNotificationData.messageSent = message;

    emit dataChanged(index(activityIndex, 0), index(activityIndex, 0), {ActivityListModel::TalkNotificationMessageSentRole});
}

QString ActivityListModel::replyMessageSent(const Activity &activity) const
{
    return activity._talkNotificationData.messageSent;
}

bool ActivityListModel::hasSyncConflicts() const
{
    return _hasSyncConflicts;
}

ActivityList ActivityListModel::allConflicts() const
{
    auto result = ActivityList{};

    for(const auto &activity : _finalList) {
        if (activity._syncFileItemStatus == SyncFileItem::Conflict) {
            result.push_back(activity);
        }
    }

    return result;
}

}
