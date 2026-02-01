/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "common/utility.h"

#include <QtCore>
#include <QAbstractListModel>
#include <QDesktopServices>
#include <QWidget>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLoggingCategory>

using namespace Qt::StringLiterals;

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
    roles[IconRole] = "icon";
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
    roles[ActivityIntegrationRole] = "serverHasIntegration";

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

    const auto activity = _finalList.at(index.row());
    const auto accountState = AccountManager::instance()->account(activity._accName);
    if (!accountState && _accountState != accountState.data()) {
        return QVariant();
    }
    const auto getFilePath = [&]() {
        const auto fileName = activity._fileAction == QStringLiteral("file_renamed") ? activity._renamedFile
                                                                                     : activity._file;
        if (!fileName.isEmpty()) {
            const auto folder = FolderMan::instance()->folder(activity._folder);

            const QString relPath = folder ? folder->remotePathTrailingSlash() + fileName
                                           : fileName;

            const auto localFiles = FolderMan::instance()->findFileInLocalFolders(relPath, accountState->account());

            if (localFiles.isEmpty()) {
                return QString();
            }

            // If this is an E2EE file or folder, pretend we got no path, hiding the share button which is what we want
            if (folder) {
                SyncJournalFileRecord rec;
                if (!folder->journalDb()->getFileRecord(relPath.mid(1), &rec)) {
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

    const auto getDisplayPath = [&activity, &accountState]() {
        if (!activity._file.isEmpty()) {
            const auto folder = FolderMan::instance()->folder(activity._folder);

            QString relPath = folder ? folder->remotePathTrailingSlash() + activity._file : activity._file;

            const auto localFiles = FolderMan::instance()->findFileInLocalFolders(relPath, accountState->account());

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
        return displayPath == "." || displayPath == "/" ? QString() : Utility::escape(displayPath);
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
        auto colorIconPath = QStringLiteral("image://svgimage-custom-color/%1");
        if (activity._type == Activity::NotificationType && !activity._talkNotificationData.userAvatar.isEmpty()) {
            return QStringLiteral("image://svgimage-custom-color/talk-bordered.svg");
        } else if (activity._type == Activity::SyncResultType) {
            return colorIconPath.arg("error.svg");
        } else if (activity._type == Activity::SyncFileItemType) {
            if (activity._syncFileItemStatus == SyncFileItem::NormalError
                || activity._syncFileItemStatus == SyncFileItem::FatalError
                || activity._syncFileItemStatus == SyncFileItem::DetailError
                || activity._syncFileItemStatus == SyncFileItem::BlacklistedError) {
                return colorIconPath.arg("error.svg");
            } else if (activity._syncFileItemStatus == SyncFileItem::SoftError
                || activity._syncFileItemStatus == SyncFileItem::Conflict
                || activity._syncFileItemStatus == SyncFileItem::Restoration
                || activity._syncFileItemStatus == SyncFileItem::FileLocked
                || activity._syncFileItemStatus == SyncFileItem::FileNameInvalid
                || activity._syncFileItemStatus == SyncFileItem::FileNameInvalidOnServer
                || activity._syncFileItemStatus == SyncFileItem::FileNameClash) {
                return colorIconPath.arg("warning.svg");
            } else if (activity._syncFileItemStatus == SyncFileItem::FileIgnored) {
                return colorIconPath.arg("info.svg");
            } else {
                // File sync successful
                if (activity._fileAction == "file_created") {
                    return activity._previews.empty() ? colorIconPath.arg("add.svg")
                                                      : colorIconPath.arg("add-bordered.svg");
                } else if (activity._fileAction == "file_deleted") {
                    return activity._previews.empty() ? colorIconPath.arg("delete.svg")
                                                      : colorIconPath.arg("delete-bordered.svg");
                } else {
                    return activity._previews.empty() ? colorIconPath.arg("change.svg")
                                                      : colorIconPath.arg("change-bordered.svg");
                }
            }
        } else {
            // We have an activity
            if (activity._icon.isEmpty()) {
                return colorIconPath.arg("activity.svg");
            }

            // using tray-image-provider here as it can read from URLs
            return QStringLiteral("image://tray-image-provider/%1").arg(activity._icon);
        }
    };

    switch (role) {
    case DisplayPathRole:
        return getDisplayPath();
    case PathRole:
        return getFilePath();
    case OpenablePathRole:
        return getFilePath();
    case DisplayLocationRole:
        return displayLocation();
    case ActionsLinksRole: {
        QList<QVariant> customList;
        for (const auto &activityLink : std::as_const(activity._links)) {
            customList << QVariant::fromValue(activityLink);
        }
        return customList;
    }

    case ActionsLinksContextMenuRole: {
        return ActivityListModel::convertLinksToMenuEntries(activity);
    }

    case ActionsLinksForActionButtonsRole: {
        return ActivityListModel::convertLinksToActionButtons(activity);
    }

    case IconRole:
        return generateIconPath();
    case ObjectTypeRole:
        return activity._objectType;
    case ObjectIdRole:
        return activity._objectId;
    case ObjectNameRole:
        return activity._objectName;
    case ActionRole: {
        switch (activity._type) {
        case Activity::ActivityType:
        case Activity::DummyFetchingActivityType:
        case Activity::DummyMoreActivitiesAvailableType:
            return "Activity";
        case Activity::NotificationType:
        case Activity::OpenSettingsNotificationType:
            return "Notification";
        case Activity::SyncFileItemType:
            return "File";
        case Activity::SyncResultType:
            return "Sync";
        }
        break;
    }
    case ActionTextRole:
        if(activity._subjectDisplay.isEmpty()) {
            return activity._subject;
        }

        return activity._subjectDisplay;
    case ActionTextColorRole:
        return activity._id == -1 ? QLatin1String("#808080") : QLatin1String("#222");   // FIXME: This is a temporary workaround for _showMoreActivitiesAvailableEntry
    case MessageRole:
        return activity._message;
    case LinkRole: {
        if (activity._link.isEmpty()) {
            return "";
        } else {
            return activity._link.toString();
        }
    }
    case AccountRole:
        return activity._accName;
    case PointInTimeRole:
        //return a._id == -1 ? "" : QString("%1 - %2").arg(Utility::timeAgoInWords(a._dateTime.toLocalTime()), a._dateTime.toLocalTime().toString(Qt::DefaultLocaleShortDate));
        return activity._id == -1 ? "" : Utility::timeAgoInWords(activity._dateTime.toLocalTime());
    case AccountConnectedRole:
        return (accountState && accountState->isConnected());
    case DisplayActions:
        return _displayActions;
    case ShowFileDetailsRole:
        return _displayActions &&
                activity._objectType == QStringLiteral("files") &&
                activity._fileAction != "file_deleted" &&
                activity._syncFileItemStatus != SyncFileItem::FileIgnored &&
                !data(index, OpenablePathRole).toString().isEmpty();
    case DismissableRole:
        // Do not allow dismissal of things requiring user input regarding syncing
        return !activity._links.isEmpty() &&
                activity._syncFileItemStatus != SyncFileItem::FileNameClash &&
                activity._syncFileItemStatus != SyncFileItem::Conflict &&
                activity._syncFileItemStatus != SyncFileItem::FileNameInvalid &&
                activity._syncFileItemStatus != SyncFileItem::FileNameInvalidOnServer;
    case IsCurrentUserFileActivityRole:
        return activity._isCurrentUserFileActivity;
    case ThumbnailRole: {
        if ((activity._type == Activity::NotificationType || activity._type == Activity::OpenSettingsNotificationType) &&
            !activity._talkNotificationData.userAvatar.isEmpty()) {
            return generateAvatarThumbnailMap(activity._talkNotificationData.userAvatar);
        }

        if(activity._previews.empty()) {
            return {};
        }

        const auto preview = activity._previews[0];
        return(generatePreviewMap(preview));
    }
    case TalkNotificationConversationTokenRole:
        return activity._talkNotificationData.conversationToken;
    case TalkNotificationMessageIdRole:
        return activity._talkNotificationData.messageId;
    case TalkNotificationMessageSentRole:
        return replyMessageSent(activity);
    case TalkNotificationUserAvatarRole:
        return activity._talkNotificationData.userAvatar;
    case ActivityIndexRole:
        return index.row();
    case ActivityRole:
        return QVariant::fromValue(activity);
    case ActivityIntegrationRole:
        return accountState->account()->serverHasIntegration();
    }

    return {};
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
        _dummyFetchingActivities._icon = QLatin1String("image://svgimage-custom-color/change-bordered.svg/");

        addEntriesToActivityList({_dummyFetchingActivities});
    } else if (!_finalList.isEmpty() && _finalList.first()._objectType == dummyFetchingActivityObjectType) {
        removeActivityFromActivityList(_dummyFetchingActivities);
    }
}

void ActivityListModel::activitiesReceived(const QJsonDocument &json, int statusCode)
{
    const auto activities = json.object().value("ocs"_L1).toObject().value("data"_L1).toArray();

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
    if (activityList.isEmpty()) {
        return;
    }

    // filter activities that do not need to be added again
    ActivityList filteredList;
    std::copy_if(activityList.constBegin(), activityList.constEnd(), std::back_inserter(filteredList), [this](const auto &activity) -> bool {
        if (activity._type == Activity::NotificationType && _activeNotificationIds.contains(activity._id)) {
            return false;
        }
        return true;
    });

    if (filteredList.isEmpty()) {
        return;
    }

    const auto startRow = _finalList.count();

    beginInsertRows({}, startRow, startRow + filteredList.count() - 1);
    for(const auto &activity : std::as_const(filteredList)) {
        _finalList.append(activity);

        if (activity._syncFileItemStatus == SyncFileItem::Conflict) {
            _conflictsList.push_back(activity);
        }

        if (activity._type == Activity::NotificationType) {
            // new unseen notification, to avoid duplicate activities to appear add its id to the filter list
            _activeNotificationIds.insert(activity._id);
        }
    }
    endInsertRows();

    setHasSyncConflicts(!_conflictsList.isEmpty());
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
        qCWarning(lcActivity) << "Error successfully added to the notification list: " << type << activity._message << activity._subject << activity._syncResultStatus << activity._syncFileItemStatus;
        auto modifiedActivity = activity;
        if (type == ErrorType::NetworkError) {
            modifiedActivity._subject = tr("Network error occurred: client will retry syncing.");
        }
        addEntriesToActivityList({modifiedActivity});
        _notificationErrorsLists.prepend(modifiedActivity);
    }
}

void ActivityListModel::addNotificationToActivityList(const Activity &activity)
{
    qCDebug(lcActivity) << "Notification successfully added to the notification list: " << activity._subject;
    addEntriesToActivityList({activity});
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
}

void ActivityListModel::removeActivityFromActivityList(int row)
{
    Activity activity = _finalList.at(row);
    removeActivityFromActivityList(activity);
}

void ActivityListModel::removeActivityFromActivityList(const Activity &activity)
{
    const auto index = _finalList.indexOf(activity);
    if (index != -1) {
        beginRemoveRows({}, index, index);
        _finalList.removeAt(index);
        endRemoveRows();
    }

    if (activity._syncFileItemStatus == SyncFileItem::Conflict) {
        _conflictsList.removeOne(activity);
    }

    if (activity._type == Activity::NotificationType) {
        _activeNotificationIds.remove(activity._id);
    }

    if (activity._type != Activity::ActivityType &&
        activity._type != Activity::DummyFetchingActivityType &&
        activity._type != Activity::DummyMoreActivitiesAvailableType &&
        activity._type != Activity::NotificationType &&
        activity._type != Activity::OpenSettingsNotificationType) {

        const auto notificationErrorsListIndex = _notificationErrorsLists.indexOf(activity);
        if (notificationErrorsListIndex != -1) {
            _notificationErrorsLists.removeAt(notificationErrorsListIndex);
        }
    }
}

void ActivityListModel::removeOutdatedNotifications(const OCC::ActivityList &receivedNotifications)
{
    ActivityList activitiesToRemove;
    for (const auto &activity : std::as_const(_finalList)) {
        if (activity._type != Activity::NotificationType || receivedNotifications.contains(activity)) {
            continue;
        }

        qCDebug(lcActivity).nospace() << "marking notification activity for removal"
            << " activity.type=" << activity._type
            << " activity.id=" << activity._id
            << " activity.objectType=" << activity._objectType
            << " activity.accName=" << activity._accName;
        activitiesToRemove.push_back(activity);
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
        const auto fileLocation = activity._syncFileItemStatus == SyncFileItem::FileNameInvalidOnServer
            ? InvalidFilenameDialog::FileLocation::NewLocalFile
            : InvalidFilenameDialog::FileLocation::Default;
        const auto invalidMode = activity._syncFileItemStatus == SyncFileItem::FileNameInvalidOnServer
            ? InvalidFilenameDialog::InvalidMode::ServerInvalid
            : InvalidFilenameDialog::InvalidMode::SystemInvalid;

        _currentInvalidFilenameDialog = new InvalidFilenameDialog(_accountState->account(), folder,
            folder->filePath(activity._file), fileLocation, invalidMode);
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
    } else if (activity._type == Activity::OpenSettingsNotificationType) {
        Q_EMIT showSettingsDialog();
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
    } else if (((action._verb == "FIX_CONFLICT_LOCALLY" || action._verb == "RENAME_LOCAL_FILE") &&
                activity._type == Activity::SyncFileItemType &&
                (activity._syncFileItemStatus == SyncFileItem::Conflict ||
                 activity._syncFileItemStatus == SyncFileItem::FileNameClash ||
                 activity._syncFileItemStatus == SyncFileItem::FileNameInvalid ||
                 activity._syncFileItemStatus == SyncFileItem::FileNameInvalidOnServer))) {
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
    _conflictsList.clear();
    _activityLists.clear();
    _presentedActivities.clear();
    _activeNotificationIds.clear();
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
    return _conflictsList;
}

}
