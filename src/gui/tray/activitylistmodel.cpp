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
    roles[AbsolutePathRole] = "absolutePath";
    roles[DisplayLocationRole] = "displayLocation";
    roles[LinkRole] = "link";
    roles[MessageRole] = "message";
    roles[ActionRole] = "type";
    roles[ActionIconRole] = "icon";
    roles[ActionTextRole] = "subject";
    roles[ActionsLinksRole] = "links";
    roles[ActionsLinksContextMenuRole] = "linksContextMenu";
    roles[ActionsLinksForActionButtonsRole] = "linksForActionButtons";
    roles[ActionTextColorRole] = "activityTextTitleColor";
    roles[ObjectTypeRole] = "objectType";
    roles[PointInTimeRole] = "dateTime";
    roles[DisplayActions] = "displayActions";
    roles[ShareableRole] = "isShareable";
    roles[IsCurrentUserFileActivityRole] = "isCurrentUserFileActivity";
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

void ActivityListModel::setCurrentlyFetching(bool value)
{
    _currentlyFetching = value;
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

    switch (role) {
    case DisplayPathRole:
        return getDisplayPath();
    case PathRole:
        return QFileInfo(getFilePath()).path();
    case AbsolutePathRole:
        return getFilePath();
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

    case ActionIconRole: {
        if (a._type == Activity::NotificationType) {
            return "qrc:///client/theme/black/bell.svg";
        } else if (a._type == Activity::SyncResultType) {
            return "qrc:///client/theme/black/state-error.svg";
        } else if (a._type == Activity::SyncFileItemType) {
            if (a._status == SyncFileItem::NormalError
                || a._status == SyncFileItem::FatalError
                || a._status == SyncFileItem::DetailError
                || a._status == SyncFileItem::BlacklistedError) {
                return "qrc:///client/theme/black/state-error.svg";
            } else if (a._status == SyncFileItem::SoftError
                || a._status == SyncFileItem::Conflict
                || a._status == SyncFileItem::Restoration
                || a._status == SyncFileItem::FileLocked
                || a._status == SyncFileItem::FileNameInvalid) {
                return "qrc:///client/theme/black/state-warning.svg";
            } else if (a._status == SyncFileItem::FileIgnored) {
                return "qrc:///client/theme/black/state-info.svg";
            } else {
                // File sync successful
                if (a._fileAction == "file_created") {
                    return "qrc:///client/theme/colored/add.svg";
                } else if (a._fileAction == "file_deleted") {
                    return "qrc:///client/theme/colored/delete.svg";
                } else {
                    return "qrc:///client/theme/change.svg";
                }
            }
        } else {
            // We have an activity
            if (a._icon.isEmpty()) {
                return "qrc:///client/theme/black/activity.svg";
            }

            return a._icon;
        }
    }
    case ObjectTypeRole:
        return a._objectType;
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
    if (_accountState && _accountState->isConnected()) {
        // If the fetching is reported to be done or we are currently fetching we can't fetch more
        if (!_doneFetching && !_currentlyFetching) {
            return true;
        }
    }

    return false;
}

void ActivityListModel::startFetchJob()
{
    if (!_accountState->isConnected()) {
        return;
    }
    auto *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/apps/activity/api/v2/activity"), this);
    QObject::connect(job, &JsonApiJob::jsonReceived,
        this, &ActivityListModel::activitiesReceived);

    QUrlQuery params;
    params.addQueryItem(QLatin1String("since"), QString::number(_currentItem));
    params.addQueryItem(QLatin1String("limit"), QString::number(50));
    job->addQueryParams(params);

    _currentlyFetching = true;
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

void ActivityListModel::activitiesReceived(const QJsonDocument &json, int statusCode)
{
    auto activities = json.object().value("ocs").toObject().value("data").toArray();

    ActivityList list;
    auto ast = _accountState;
    if (!ast) {
        return;
    }

    if (activities.size() == 0) {
        _doneFetching = true;
    }

    _currentlyFetching = false;

    QDateTime oldestDate = QDateTime::currentDateTime();
    oldestDate = oldestDate.addDays(_maxActivitiesDays * -1);

    foreach (auto activ, activities) {
        auto json = activ.toObject();

        Activity a;
        const auto activityUser = json.value(QStringLiteral("user")).toString();
        a._type = Activity::ActivityType;
        a._objectType = json.value(QStringLiteral("object_type")).toString();
        a._accName = ast->account()->displayName();
        a._id = json.value(QStringLiteral("activity_id")).toInt();
        a._fileAction = json.value(QStringLiteral("type")).toString();
        a._subject = json.value(QStringLiteral("subject")).toString();
        a._message = json.value(QStringLiteral("message")).toString();
        a._file = json.value(QStringLiteral("object_name")).toString();
        a._link = QUrl(json.value(QStringLiteral("link")).toString());
        a._dateTime = QDateTime::fromString(json.value(QStringLiteral("datetime")).toString(), Qt::ISODate);
        a._icon = json.value(QStringLiteral("icon")).toString();
        a._isCurrentUserFileActivity = a._objectType == QStringLiteral("files") && activityUser == ast->account()->davUser();

        auto richSubjectData = json.value(QStringLiteral("subject_rich")).toArray();

        if(richSubjectData.size() > 1) {
            a._subjectRich = richSubjectData[0].toString();
            auto parameters = richSubjectData[1].toObject();
            const QRegularExpression subjectRichParameterRe(QStringLiteral("({[a-zA-Z0-9]*})"));
            const QRegularExpression subjectRichParameterBracesRe(QStringLiteral("[{}]"));

            for (auto i = parameters.begin(); i != parameters.end(); ++i) {
                const auto parameterJsonObject = i.value().toObject();
                const Activity::RichSubjectParameter parameter = {
                    parameterJsonObject.value(QStringLiteral("type")).toString(),
                    parameterJsonObject.value(QStringLiteral("id")).toString(),
                    parameterJsonObject.value(QStringLiteral("name")).toString(),
                    parameterJsonObject.contains(QStringLiteral("path")) ? parameterJsonObject.value(QStringLiteral("path")).toString() : QString(),
                    parameterJsonObject.contains(QStringLiteral("link")) ? QUrl(parameterJsonObject.value(QStringLiteral("link")).toString()) : QUrl(),
                };

                a._subjectRichParameters[i.key()] = parameter;
            }

            auto displayString = a._subjectRich;
            auto i = subjectRichParameterRe.globalMatch(displayString);

            while (i.hasNext()) {
                const auto match = i.next();
                auto word = match.captured(1);
                word.remove(subjectRichParameterBracesRe);

                Q_ASSERT(a._subjectRichParameters.contains(word));
                displayString = displayString.replace(match.captured(1), a._subjectRichParameters[word].name);
            }

            a._subjectDisplay = displayString;
        }

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

    combineActivityLists();

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
        if (index != -1)
            _activityLists.removeAt(index);
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
        customList << ActivityListModel::convertLinkToActionButton(activity, activity._links.first());
        return customList;
    }

    for (const auto &activityLink : activity._links) {
        if (activityLink._verb == QStringLiteral("DELETE")
            || (activity._objectType == QStringLiteral("chat") || activity._objectType == QStringLiteral("call")
                || activity._objectType == QStringLiteral("room"))) {
            customList << ActivityListModel::convertLinkToActionButton(activity, activityLink);
        }
    }

    return customList;
}

QVariant ActivityListModel::convertLinkToActionButton(const OCC::Activity &activity, const OCC::ActivityLink &activityLink)
{
    auto activityLinkCopy = activityLink;

    const auto isReplyIconApplicable = activityLink._verb == QStringLiteral("WEB")
        && (activity._objectType == QStringLiteral("chat") || activity._objectType == QStringLiteral("call")
            || activity._objectType == QStringLiteral("room"));

    const QString replyButtonPath = QStringLiteral("image://svgimage-custom-color/reply.svg");

    if (isReplyIconApplicable) {
        activityLinkCopy._imageSource =
            QString(replyButtonPath + "/" + OCC::Theme::instance()->wizardHeaderBackgroundColor().name());
        activityLinkCopy._imageSourceHovered =
            QString(replyButtonPath + "/" + OCC::Theme::instance()->wizardHeaderTitleColor().name());
    }

    const auto isReplyLabelApplicable = activityLink._verb == QStringLiteral("WEB")
        && (activity._objectType == QStringLiteral("chat")
        || (activity._objectType != QStringLiteral("room") && activity._objectType != QStringLiteral("call")));

    if (activityLink._verb == QStringLiteral("DELETE")) {
        activityLinkCopy._label = QObject::tr("Mark as read");
    } else if (isReplyLabelApplicable) {
        activityLinkCopy._label = QObject::tr("Reply");
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

        if(_showMoreActivitiesAvailableEntry) {
            Activity a;
            a._type = Activity::ActivityType;
            a._accName = _accountState->account()->displayName();
            a._id = -1;
            a._subject = tr("For more activities please open the Activity app.");
            a._dateTime = QDateTime::currentDateTime();

            AccountApp *app = _accountState->findApp(QLatin1String("activity"));
            if(app) {
                a._link = app->url();
            }

            resultList.append(a);
        }
    }

    beginResetModel();
    _finalList = resultList;
    endResetModel();
}

bool ActivityListModel::canFetchActivities() const
{
    return _accountState->isConnected() && _accountState->account()->capabilities().hasActivities();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    if (canFetchActivities() && !_currentlyFetching) {
        startFetchJob();
    }
}

void ActivityListModel::slotRefreshActivity()
{
    _activityLists.clear();
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

void ActivityListModel::slotRemoveAccount()
{
    _finalList.clear();
    _activityLists.clear();
    _currentlyFetching = false;
    _doneFetching = false;
    _currentItem = 0;
    _totalActivitiesFetched = 0;
    _showMoreActivitiesAvailableEntry = false;
}

}

