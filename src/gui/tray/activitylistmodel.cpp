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

#include <QtCore>
#include <QAbstractListModel>
#include <QDesktopServices>
#include <QWidget>
#include <QJsonObject>
#include <QJsonDocument>
#include <qloggingcategory.h>

#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "conflictdialog.h"
#include "folderman.h"
#include "iconjob.h"
#include "accessmanager.h"
#include "owncloudgui.h"
#include "guiutility.h"
#include "invalidfilenamedialog.h"

#include "activitydata.h"
#include "activitylistmodel.h"
#include "systray.h"
#include "tray/usermodel.h"

#include "theme.h"

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
}

QHash<int, QByteArray> ActivityListModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[DisplayPathRole] = "displayPath";
    roles[PathRole] = "path";
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
    roles[ShareableRole] = "isShareable";
    roles[IsCurrentUserFileActivityRole] = "isCurrentUserFileActivity";
    roles[ThumbnailRole] = "thumbnail";
    roles[TalkNotificationConversationTokenRole] = "conversationToken";
    roles[TalkNotificationMessageIdRole] = "messageId";
    roles[TalkNotificationMessageSentRole] = "messageSent";
    roles[TalkNotificationUserAvatarRole] = "userAvatar";

    return roles;
}

void ActivityListModel::setAccountState(AccountState *state)
{
    _accountState = state;
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
    Activity a;

    if (!index.isValid())
        return QVariant();

    a = _finalList.at(index.row());
    AccountStatePtr ast = AccountManager::instance()->account(a._accName);
    if (!ast && _accountState != ast.data())
        return QVariant();

    const auto getFilePath = [&]() {
        const auto fileName = a._fileAction == QStringLiteral("file_renamed") ? a._renamedFile : a._file;
        if (!fileName.isEmpty()) {
            const auto folder = FolderMan::instance()->folder(a._folder);

            const QString relPath = folder ? folder->remotePath() + fileName : fileName;

            const auto localFiles = FolderMan::instance()->findFileInLocalFolders(relPath, ast->account());

            if (localFiles.isEmpty()) {
                return QString();
            }

            // If this is an E2EE file or folder, pretend we got no path, hiding the share button which is what we want
            if (folder) {
                SyncJournalFileRecord rec;
                folder->journalDb()->getFileRecord(fileName.mid(1), &rec);
                if (rec.isValid() && (rec._isE2eEncrypted || !rec._e2eMangledName.isEmpty())) {
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

            QString relPath = folder ? folder->remotePath() + a._file : a._file;

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
            if (a._status == SyncFileItem::NormalError
                || a._status == SyncFileItem::FatalError
                || a._status == SyncFileItem::DetailError
                || a._status == SyncFileItem::BlacklistedError) {
                colorIconPath.append("state-error.svg");
                return colorIconPath;
            } else if (a._status == SyncFileItem::SoftError
                || a._status == SyncFileItem::Conflict
                || a._status == SyncFileItem::Restoration
                || a._status == SyncFileItem::FileLocked
                || a._status == SyncFileItem::FileNameInvalid) {
                colorIconPath.append("state-warning.svg");
                return colorIconPath;
            } else if (a._status == SyncFileItem::FileIgnored) {
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
            if (a._darkIcon.isEmpty()) {
                colorIconPath.append("activity.svg");
                return colorIconPath;
            }
            return role == DarkIconRole ? a._darkIcon : a._lightIcon;
        }
    };

    switch (role) {
    case DisplayPathRole:
        return getDisplayPath();
    case PathRole:
        return QFileInfo(getFilePath()).path();
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
    case ShareableRole:
        return !data(index, PathRole).toString().isEmpty() && a._objectType == QStringLiteral("files") && _displayActions && a._fileAction != "file_deleted" && a._status != SyncFileItem::FileIgnored;
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
    default:
        return QVariant();
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

void ActivityListModel::setFinalList(const ActivityList &finalList)
{
    _finalList = finalList;
}

const ActivityList &ActivityListModel::finalList() const
{
    return _finalList;
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

    for (const auto &activ : activities) {
        const auto json = activ.toObject();

        auto a = Activity::fromActivityJson(json, _accountState->account());

        list.append(a);
        _currentItem = list.last()._id;

        _totalActivitiesFetched++;
        if (_totalActivitiesFetched == _maxActivities
            || (_hideOldActivities && a._dateTime < oldestDate)) {
            _showMoreActivitiesAvailableEntry = true;
            _doneFetching = true;
            break;
        }
    }

    _activityLists.append(list);

    if (list.size() > 0) {
        std::sort(list.begin(), list.end());
        beginInsertRows({}, _finalList.size(), _finalList.size() + list.size() - 1);
        _finalList.append(list);
        endInsertRows();

        appendMoreActivitiesAvailableEntry();
    }
}

void ActivityListModel::appendMoreActivitiesAvailableEntry()
{
    const QString moreActivitiesEntryObjectType = QLatin1String("activity_fetch_more_activities");
    if (_showMoreActivitiesAvailableEntry && !_finalList.isEmpty()
        && _finalList.last()._objectType != moreActivitiesEntryObjectType) {
        Activity a;
        a._type = Activity::ActivityType;
        a._accName = _accountState->account()->displayName();
        a._id = -1;
        a._objectType = moreActivitiesEntryObjectType;
        a._subject = tr("For more activities please open the Activity app.");
        a._dateTime = QDateTime::currentDateTime();

        if (const auto *app = _accountState->findApp(QLatin1String("activity"))) {
            a._link = app->url();
        }

        beginInsertRows({}, _finalList.size(), _finalList.size());
        _finalList.append(a);
        endInsertRows();
    }
}

void ActivityListModel::insertOrRemoveDummyFetchingActivity()
{
    const QString dummyFetchingActivityObjectType = QLatin1String("dummy_fetching_activity");
    if (_currentlyFetching && _finalList.isEmpty()) {
        Activity a;
        a._type = Activity::ActivityType;
        a._accName = _accountState->account()->displayName();
        a._id = -2;
        a._objectType = dummyFetchingActivityObjectType;
        a._subject = tr("Fetching activities…");
        a._dateTime = QDateTime::currentDateTime();
        a._darkIcon = QLatin1String("qrc:///client/theme/colored/change-bordered.svg");
        a._lightIcon = QLatin1String("qrc:///client/theme/colored/change-bordered.svg");

        beginInsertRows({}, 0, 0);
        _finalList.prepend(a);
        endInsertRows();
    } else if (!_finalList.isEmpty() && _finalList.first()._objectType == dummyFetchingActivityObjectType) {
        beginRemoveRows({}, 0, 0);
        _finalList.removeAt(0);
        endRemoveRows();
    }
}

void ActivityListModel::clearActivities()
{
    _activityLists.clear();
    if (!_finalList.isEmpty()) {
        const auto firstActivityIt = std::find_if(std::begin(_finalList), std::end(_finalList),
            [&](const Activity &activity) { return activity._type == Activity::ActivityType; });

        if (firstActivityIt != std::end(_finalList)) {
            const auto lastActivityItReverse = std::find_if(std::rbegin(_finalList), std::rend(_finalList),
                    [&](const Activity &activity) { return activity._type == Activity::ActivityType; });

            const auto lastActivityIt = (lastActivityItReverse + 1).base();

            if (lastActivityIt != std::end(_finalList)) {
                const int beginRemoveIndex = std::distance(std::begin(_finalList), firstActivityIt);
                const int endRemoveIndex = std::distance(std::begin(_finalList), lastActivityIt);
                beginRemoveRows({}, beginRemoveIndex, endRemoveIndex);
                _finalList.erase(firstActivityIt, std::end(_finalList));
                endRemoveRows();
            }
        }
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

void ActivityListModel::addErrorToActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Error successfully added to the notification list: " << activity._subject;
    _notificationErrorsLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::addIgnoredFileToList(Activity newActivity)
{
    qCInfo(lcActivity) << "First checking for duplicates then add file to the notification list of ignored files: " << newActivity._file;

    bool duplicate = false;
    if (_listOfIgnoredFiles.size() == 0) {
        _notificationIgnoredFiles = newActivity;
        _notificationIgnoredFiles._subject = tr("Files from the ignore list as well as symbolic links are not synced.");
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

void ActivityListModel::addNotificationToActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Notification successfully added to the notification list: " << activity._subject;
    _notificationLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::clearNotifications()
{
    qCInfo(lcActivity) << "Clear the notifications";
    _notificationLists.clear();
    combineActivityLists();
}

void ActivityListModel::removeActivityFromActivityList(int row)
{
    Activity activity = _finalList.at(row);
    removeActivityFromActivityList(activity);
    combineActivityLists();
}

void ActivityListModel::addSyncFileItemToActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Successfully added to the activity list: " << activity._subject;
    _syncFileItemLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::removeActivityFromActivityList(Activity activity)
{
    qCInfo(lcActivity) << "Activity/Notification/Error successfully dismissed: " << activity._subject;
    qCInfo(lcActivity) << "Trying to remove Activity/Notification/Error from view... ";

    int index = -1;
    if (activity._type == Activity::ActivityType) {
        index = _activityLists.indexOf(activity);
        if (index != -1) {
            _activityLists.removeAt(index);
            const auto indexInFinalList = _finalList.indexOf(activity);
            if (indexInFinalList != -1) {
                beginRemoveRows({}, indexInFinalList, indexInFinalList);
                _finalList.removeAt(indexInFinalList);
                endRemoveRows();
            }
        }
    } else if (activity._type == Activity::NotificationType) {
        index = _notificationLists.indexOf(activity);
        if (index != -1)
            _notificationLists.removeAt(index);
    } else {
        index = _notificationErrorsLists.indexOf(activity);
        if (index != -1)
            _notificationErrorsLists.removeAt(index);
    }

    if (index != -1) {
        qCInfo(lcActivity) << "Activity/Notification/Error successfully removed from the list.";
        qCInfo(lcActivity) << "Updating Activity/Notification/Error view.";
        combineActivityLists();
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
    if (activity._status == SyncFileItem::Conflict) {
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
        return;
    } else if (activity._status == SyncFileItem::FileNameInvalid) {
        if (!_currentInvalidFilenameDialog.isNull()) {
            _currentInvalidFilenameDialog->close();
        }

        auto folder = FolderMan::instance()->folder(activity._folder);
        const auto folderDir = QDir(folder->path());
        _currentInvalidFilenameDialog = new InvalidFilenameDialog(_accountState->account(), folder,
            folderDir.filePath(activity._file));
        connect(_currentInvalidFilenameDialog, &InvalidFilenameDialog::accepted, folder, [folder]() {
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
    }

    emit sendNotificationRequest(activity._accName, action._link, action._verb, activityIndex);
}

void ActivityListModel::slotTriggerDismiss(const int activityIndex)
{
    if (activityIndex < 0 || activityIndex >= _finalList.size()) {
        qCWarning(lcActivity) << "Couldn't trigger action on activity at index" << activityIndex << "/ final list size:" << _finalList.size();
        return;
    }

    const auto activityLinks = _finalList[activityIndex]._links;

    const auto foundActivityLinkIt = std::find_if(std::cbegin(activityLinks), std::cend(activityLinks), [](const ActivityLink &link) {
        return link._verb == QStringLiteral("DELETE");
    });

    if (foundActivityLinkIt == std::cend(activityLinks)) {
        qCWarning(lcActivity) << "Couldn't find dismiss action in activity at index" << activityIndex
                              << " links.size() " << activityLinks.size();
        return;
    }

    const auto actionIndex = static_cast<int>(std::distance(activityLinks.begin(), foundActivityLinkIt));

    if (actionIndex < 0 || actionIndex > activityLinks.size()) {
        qCWarning(lcActivity) << "Couldn't find dismiss action in activity at index" << activityIndex
                              << " actionIndex found " << actionIndex;
        return;
    }

    slotTriggerAction(activityIndex, actionIndex);
}

AccountState *ActivityListModel::accountState() const
{
    return _accountState;
}

QVariantList ActivityListModel::convertLinksToActionButtons(const Activity &activity)
{
    QVariantList customList;

    if (activity._links.size() == 1) {
        return customList;
    }

    if (static_cast<quint32>(activity._links.size()) > maxActionButtons()) {
        customList << ActivityListModel::convertLinkToActionButton(activity._links.first());
        return customList;
    }

    for (const auto &activityLink : activity._links) {
        if (activityLink._verb == QStringLiteral("DELETE")
            || (activity._objectType == QStringLiteral("chat") || activity._objectType == QStringLiteral("call")
                || activity._objectType == QStringLiteral("room"))) {
            customList << ActivityListModel::convertLinkToActionButton(activityLink);
        }
    }

    return customList;
}

QVariant ActivityListModel::convertLinkToActionButton(const OCC::ActivityLink &activityLink)
{
    auto activityLinkCopy = activityLink;

    const auto isReplyIconApplicable = activityLink._verb == QStringLiteral("REPLY");

    const QString replyButtonPath = QStringLiteral("image://svgimage-custom-color/reply.svg");

    if (isReplyIconApplicable) {
        activityLinkCopy._imageSource =
            QString(replyButtonPath + "/" + OCC::Theme::instance()->wizardHeaderBackgroundColor().name());
        activityLinkCopy._imageSourceHovered =
            QString(replyButtonPath + "/" + OCC::Theme::instance()->wizardHeaderTitleColor().name());
    }

    return QVariant::fromValue(activityLinkCopy);
}

QVariantList ActivityListModel::convertLinksToMenuEntries(const Activity &activity)
{
    QVariantList customList;

    if (static_cast<quint32>(activity._links.size()) > maxActionButtons()) {
        for (int i = 0; i < activity._links.size(); ++i) {
            const auto &activityLink = activity._links[i];
            if (!activityLink._primary) {
                customList << QVariantMap{
                    {QStringLiteral("actionIndex"), i}, {QStringLiteral("label"), activityLink._label}};
            }
        }
    }

    return customList;
}

void ActivityListModel::combineActivityLists()
{
    ActivityList resultList;

    if (_notificationErrorsLists.count() > 0) {
        std::sort(_notificationErrorsLists.begin(), _notificationErrorsLists.end());
        resultList.append(_notificationErrorsLists);
    }
    if (_listOfIgnoredFiles.size() > 0)
        resultList.append(_notificationIgnoredFiles);

    if (_notificationLists.count() > 0) {
        std::sort(_notificationLists.begin(), _notificationLists.end());
        resultList.append(_notificationLists);
    }

    if (_syncFileItemLists.count() > 0) {
        std::sort(_syncFileItemLists.begin(), _syncFileItemLists.end());
        resultList.append(_syncFileItemLists);
    }

    if (_activityLists.count() > 0) {
        std::sort(_activityLists.begin(), _activityLists.end());
        resultList.append(_activityLists);
    }

    if (_finalList.isEmpty() && !resultList.isEmpty()) {
        beginInsertRows({}, 0, resultList.size() - 1);
        _finalList = resultList;
        endInsertRows();
    } else if (!_finalList.isEmpty()) {
        beginResetModel();
        _finalList = resultList;
        endResetModel();
    }

    if (_activityLists.size() > 0) {
        appendMoreActivitiesAvailableEntry();
    }
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
    clearActivities();
    _doneFetching = false;
    _currentItem = 0;
    _totalActivitiesFetched = 0;
    _showMoreActivitiesAvailableEntry = false;

    if (canFetchActivities()) {
        startFetchJob();
    } else {
        _doneFetching = true;
        combineActivityLists();
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
    setAndRefreshCurrentlyFetching(false);
    _doneFetching = false;
    _currentItem = 0;
    _totalActivitiesFetched = 0;
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
}
