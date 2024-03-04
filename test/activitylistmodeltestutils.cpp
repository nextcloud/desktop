/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "activitylistmodeltestutils.h"
#include "syncenginetestutils.h"

#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
static QByteArray fake404Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":404,"message":"Invalid query, please check the syntax. API specifications are here: http:\/\/www.freedesktop.org\/wiki\/Specifications\/open-collaboration-services.\n"},"data":[]}}
)";

static QByteArray fake400Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}}
)";

static QByteArray fake500Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":500,"message":"Internal Server Error.\n"},"data":[]}}
)";
}

namespace ActivityListModelTestUtils
{

QNetworkReply *almTestQnamOverride(FakeQNAM * const fakeQnam,
                                   const QNetworkAccessManager::Operation op,
                                   const QNetworkRequest &req,
                                   const QString &accountUrl,
                                   QObject * const parent,
                                   const int searchResultsReplyDelay,
                                   QIODevice * const device)
{
    Q_UNUSED(device);
    QNetworkReply *reply = nullptr;

    const auto urlQuery = QUrlQuery(req.url());
    const auto format = urlQuery.queryItemValue(QStringLiteral("format"));
    const auto since = urlQuery.queryItemValue(QStringLiteral("since")).toInt();
    const auto limit = urlQuery.queryItemValue(QStringLiteral("limit")).toInt();
    const auto path = req.url().path();

    if (!req.url().toString().startsWith(accountUrl)) {
        reply = new FakeErrorReply(op, req, parent, 404, fake404Response);
    }
    if (format != QStringLiteral("json")) {
        reply = new FakeErrorReply(op, req, parent, 400, fake400Response);
    }

    if (path.startsWith(QStringLiteral("/ocs/v2.php/apps/activity/api/v2/activity"))) {
        reply = new FakePayloadReply(op, req, FakeRemoteActivityStorage::instance()->activityJsonData(since, limit), searchResultsReplyDelay, fakeQnam);
    }

    if (!reply) {
        return qobject_cast<QNetworkReply*>(new FakeErrorReply(op, req, parent, 404, QByteArrayLiteral("{error: \"Not found!\"}")));
    }

    return reply;
}

// Activity comparison is done by checking type, id, and accName
// We need an activity with these details, at least

OCC::Activity exampleNotificationActivity(const QString &accountName, const int id)
{
    OCC::Activity testNotificationActivity;

    testNotificationActivity._accName = accountName;
    testNotificationActivity._id = id;
    testNotificationActivity._type = OCC::Activity::NotificationType;
    testNotificationActivity._dateTime = QDateTime::currentDateTime();
    testNotificationActivity._subject = QStringLiteral("Sample notification text");

    return testNotificationActivity;
}

OCC::Activity exampleSyncResultErrorActivity(const QString &accountName, const int id)
{
    OCC::Activity testSyncResultErrorActivity;

    testSyncResultErrorActivity._id = id;
    testSyncResultErrorActivity._type = OCC::Activity::SyncResultType;
    testSyncResultErrorActivity._syncResultStatus = OCC::SyncResult::Error;
    testSyncResultErrorActivity._dateTime = QDateTime::currentDateTime();
    testSyncResultErrorActivity._subject = QStringLiteral("Sample failed sync text");
    testSyncResultErrorActivity._message = QStringLiteral("/path/to/thingy");
    testSyncResultErrorActivity._link = QStringLiteral("/path/to/thingy");
    testSyncResultErrorActivity._accName = accountName;

    return testSyncResultErrorActivity;
}

OCC::Activity exampleSyncFileItemActivity(const QString &accountName, const QUrl &link, const int id)
{
    OCC::Activity testSyncFileItemActivity;

    testSyncFileItemActivity._id = id;
    testSyncFileItemActivity._type = OCC::Activity::SyncFileItemType; //client activity
    testSyncFileItemActivity._syncFileItemStatus = OCC::SyncFileItem::Success;
    testSyncFileItemActivity._dateTime = QDateTime::currentDateTime();
    testSyncFileItemActivity._message = QStringLiteral("Sample file successfully synced text");
    testSyncFileItemActivity._link = link;
    testSyncFileItemActivity._accName = accountName;
    testSyncFileItemActivity._file = QStringLiteral("xyz.pdf");

    return testSyncFileItemActivity;
}

OCC::Activity exampleFileIgnoredActivity(const QString &accountName, const QUrl &link, const int id)
{
    OCC::Activity testFileIgnoredActivity;

    testFileIgnoredActivity._id = id;
    testFileIgnoredActivity._type = OCC::Activity::SyncFileItemType;
    testFileIgnoredActivity._syncFileItemStatus = OCC::SyncFileItem::FileIgnored;
    testFileIgnoredActivity._dateTime = QDateTime::currentDateTime();
    testFileIgnoredActivity._subject = QStringLiteral("Sample ignored file sync text");
    testFileIgnoredActivity._link = link;
    testFileIgnoredActivity._accName = accountName;
    testFileIgnoredActivity._folder = QStringLiteral("thingy");
    testFileIgnoredActivity._file = QStringLiteral("test.txt");

    return testFileIgnoredActivity;
}

FakeRemoteActivityStorage *FakeRemoteActivityStorage::_instance = nullptr;

FakeRemoteActivityStorage* FakeRemoteActivityStorage::instance()
{
    if (!_instance) {
        _instance = new FakeRemoteActivityStorage();
        _instance->init();
    }

    return _instance;
}

void FakeRemoteActivityStorage::destroy()
{
    if (_instance) {
        delete _instance;
    }

    _instance = nullptr;
}

void FakeRemoteActivityStorage::init()
{
    if (!_activityData.isEmpty()) {
        return;
    }

    _metaSuccess = {{QStringLiteral("status"), QStringLiteral("ok")}, {QStringLiteral("statuscode"), 200},
        {QStringLiteral("message"), QStringLiteral("OK")}};

    initActivityData();
}

void FakeRemoteActivityStorage::initActivityData()
{
    _activityData = {};

    // Insert activity data
    for (quint32 i = 0; i <= _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("object_type"), "files");
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("type"), QStringLiteral("file"));
        activity.insert(QStringLiteral("subject"), QStringLiteral("You created %1.txt").arg(i));
        activity.insert(QStringLiteral("message"), QStringLiteral(""));
        activity.insert(QStringLiteral("object_name"), QStringLiteral("%1.txt").arg(i));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/apps/files/img/add-color.svg"));

        _activityData.push_back(activity);

        _startingId++;
    }

    // Insert notification data
    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), "calendar");
        activity.insert(QStringLiteral("type"), QStringLiteral("calendar-event"));
        activity.insert(
            QStringLiteral("subject"), QStringLiteral("You created event %1 in calendar Events").arg(i));
        activity.insert(QStringLiteral("message"), QStringLiteral(""));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/calendar.svg"));

        _activityData.push_back(activity);

        _startingId++;
    }

    // Insert notification data
    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), "chat");
        activity.insert(QStringLiteral("type"), QStringLiteral("chat"));
        activity.insert(QStringLiteral("subject"), QStringLiteral("You have received %1's message").arg(i));
        activity.insert(QStringLiteral("message"), QStringLiteral(""));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/talk.svg"));

        QJsonArray actionsArray;

        QJsonObject replyAction;
        replyAction.insert(QStringLiteral("label"), QStringLiteral("Reply"));
        replyAction.insert(QStringLiteral("link"), QStringLiteral(""));
        replyAction.insert(QStringLiteral("type"), QStringLiteral("REPLY"));
        replyAction.insert(QStringLiteral("primary"), true);
        actionsArray.push_back(replyAction);

        QJsonObject primaryAction;
        primaryAction.insert(QStringLiteral("label"), QStringLiteral("View chat"));
        primaryAction.insert(QStringLiteral("link"), QStringLiteral("http://cloud.example.de/call/9p4vjdzd"));
        primaryAction.insert(QStringLiteral("type"), QStringLiteral("WEB"));
        primaryAction.insert(QStringLiteral("primary"), false);
        actionsArray.push_back(primaryAction);

        QJsonObject additionalAction;
        additionalAction.insert(QStringLiteral("label"), QStringLiteral("Additional 1"));
        additionalAction.insert(QStringLiteral("link"), QStringLiteral("http://cloud.example.de/call/9p4vjdzd"));
        additionalAction.insert(QStringLiteral("type"), QStringLiteral("POST"));
        additionalAction.insert(QStringLiteral("primary"), false);
        actionsArray.push_back(additionalAction);
        additionalAction.insert(QStringLiteral("label"), QStringLiteral("Additional 2"));
        actionsArray.push_back(additionalAction);

        activity.insert(QStringLiteral("actions"), actionsArray);

        _activityData.push_back(activity);

        _startingId++;
    }

    // Insert notification data
    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), "room");
        activity.insert(QStringLiteral("type"), QStringLiteral("room"));
        activity.insert(QStringLiteral("subject"), QStringLiteral("You have been invited into room%1").arg(i));
        activity.insert(QStringLiteral("message"), QStringLiteral(""));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/talk.svg"));

        QJsonArray actionsArray;

        QJsonObject replyAction;
        replyAction.insert(QStringLiteral("label"), QStringLiteral("Reply"));
        replyAction.insert(QStringLiteral("link"), QStringLiteral(""));
        replyAction.insert(QStringLiteral("type"), QStringLiteral("REPLY"));
        replyAction.insert(QStringLiteral("primary"), true);
        actionsArray.push_back(replyAction);

        QJsonObject primaryAction;
        primaryAction.insert(QStringLiteral("label"), QStringLiteral("View chat"));
        primaryAction.insert(QStringLiteral("link"), QStringLiteral("http://cloud.example.de/call/9p4vjdzd"));
        primaryAction.insert(QStringLiteral("type"), QStringLiteral("WEB"));
        primaryAction.insert(QStringLiteral("primary"), false);
        actionsArray.push_back(primaryAction);

        activity.insert(QStringLiteral("actions"), actionsArray);

        _activityData.push_back(activity);

        _startingId++;
    }

    // Insert notification data
    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), "call");
        activity.insert(QStringLiteral("type"), QStringLiteral("call"));
        activity.insert(QStringLiteral("subject"), QStringLiteral("You have missed a %1's call").arg(i));
        activity.insert(QStringLiteral("message"), QStringLiteral(""));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/talk.svg"));

        QJsonArray actionsArray;

        QJsonObject primaryAction;
        primaryAction.insert(QStringLiteral("label"), QStringLiteral("Call back"));
        primaryAction.insert(QStringLiteral("link"), QStringLiteral("http://cloud.example.de/call/9p4vjdzd"));
        primaryAction.insert(QStringLiteral("type"), QStringLiteral("WEB"));
        primaryAction.insert(QStringLiteral("primary"), true);
        actionsArray.push_back(primaryAction);

        QJsonObject replyAction;
        replyAction.insert(QStringLiteral("label"), QStringLiteral("Reply"));
        replyAction.insert(QStringLiteral("link"), QStringLiteral(""));
        replyAction.insert(QStringLiteral("type"), QStringLiteral("REPLY"));
        replyAction.insert(QStringLiteral("primary"), false);
        actionsArray.push_back(replyAction);

        activity.insert(QStringLiteral("actions"), actionsArray);

        _activityData.push_back(activity);

        _startingId++;
    }

    // Insert notification data
    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), "2fa_id");
        activity.insert(QStringLiteral("subject"), QStringLiteral("Login attempt from 127.0.0.1"));
        activity.insert(QStringLiteral("message"), QStringLiteral("Please approve or deny the login attempt."));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/password.svg"));

        QJsonArray actionsArray;

        QJsonObject primaryAction;
        primaryAction.insert(QStringLiteral("label"), QStringLiteral("Approve"));
        primaryAction.insert(QStringLiteral("link"), QStringLiteral("/ocs/v2.php/apps/twofactor_nextcloud_notification/api/v1/attempt/39"));
        primaryAction.insert(QStringLiteral("type"), QStringLiteral("POST"));
        primaryAction.insert(QStringLiteral("primary"), true);
        actionsArray.push_back(primaryAction);

        QJsonObject secondaryAction;
        secondaryAction.insert(QStringLiteral("label"), QStringLiteral("Cancel"));
        secondaryAction.insert(QStringLiteral("link"),
            QString(QStringLiteral("/ocs/v2.php/apps/twofactor_nextcloud_notification/api/v1/attempt/39")));
        secondaryAction.insert(QStringLiteral("type"), QStringLiteral("DELETE"));
        secondaryAction.insert(QStringLiteral("primary"), false);
        actionsArray.push_back(secondaryAction);

        activity.insert(QStringLiteral("actions"), actionsArray);

        _activityData.push_back(activity);

        _startingId++;
    }

    // Insert notification data
    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), "create");
        activity.insert(QStringLiteral("subject"), QStringLiteral("Generate backup codes"));
        activity.insert(QStringLiteral("message"),
                        QStringLiteral("You enabled two-factor authentication but did not generate backup codes yet. They are needed to restore access to your "
                                       "account in case you lose your second factor."));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/password.svg"));

        QJsonArray actionsArray;

        QJsonObject secondaryAction;
        secondaryAction.insert(QStringLiteral("label"), QStringLiteral("Dismiss"));
        secondaryAction.insert(QStringLiteral("link"), QString(QStringLiteral("ocs/v2.php/apps/notifications/api/v2/notifications/19867")));
        secondaryAction.insert(QStringLiteral("type"), QStringLiteral("DELETE"));
        secondaryAction.insert(QStringLiteral("primary"), false);
        actionsArray.push_back(secondaryAction);

        activity.insert(QStringLiteral("actions"), actionsArray);

        _activityData.push_back(activity);

        _startingId++;
    }

    // Insert notification data
    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), QStringLiteral(""));
        activity.insert(QStringLiteral("type"), QStringLiteral("security"));
        activity.insert(QStringLiteral("subject"), QStringLiteral("You successfully logged in using two-factor authentication (Nextcloud Notification)"));
        activity.insert(QStringLiteral("message"), QStringLiteral(""));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/password.svg"));

        _activityData.push_back(activity);

        _startingId++;
    }

    for (quint32 i = 0; i < _numItemsToInsert; i++) {
        QJsonObject activity;
        activity.insert(QStringLiteral("activity_id"), _startingId);
        activity.insert(QStringLiteral("object_type"), "remote_share");
        activity.insert(QStringLiteral("subject"), QStringLiteral("You received document.docx as a remote share from admin@https://example.de"));
        activity.insert(QStringLiteral("subjectRich"), QStringLiteral("You received {share} as a remote share from {user}"));
        activity.insert(QStringLiteral("message"), QStringLiteral(""));
        activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
        activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
        activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/actions/share.svg"));

        QJsonArray actionsArray;

        QJsonObject primaryAction;
        primaryAction.insert(QStringLiteral("label"), QStringLiteral("Accept"));
        primaryAction.insert(QStringLiteral("link"), QStringLiteral("/ocs/v2.php/apps/files_sharing/api/v1/remote_shares/pending/6"));
        primaryAction.insert(QStringLiteral("type"), QStringLiteral("POST"));
        primaryAction.insert(QStringLiteral("primary"), true);
        actionsArray.push_back(primaryAction);

        QJsonObject secondaryAction;
        secondaryAction.insert(QStringLiteral("label"), QStringLiteral("Decline"));
        secondaryAction.insert(QStringLiteral("link"), QString(QStringLiteral("/ocs/v2.php/apps/files_sharing/api/v1/remote_shares/pending/6")));
        secondaryAction.insert(QStringLiteral("type"), QStringLiteral("DELETE"));
        secondaryAction.insert(QStringLiteral("primary"), false);
        actionsArray.push_back(secondaryAction);

        activity.insert(QStringLiteral("actions"), actionsArray);

        _activityData.push_back(activity);

        _startingId++;
    }

    _startingId--;
}

QByteArray FakeRemoteActivityStorage::activityJsonData(const int sinceId, const int limit)
{
    QJsonArray data;

    const auto itFound = std::find_if(
        std::cbegin(_activityData), std::cend(_activityData), [&sinceId](const QJsonValue &currentActivityValue) {
            const auto currentActivityId =
                currentActivityValue.toObject().value(QStringLiteral("activity_id")).toInt();
            return currentActivityId == sinceId;
        });

    const int startIndex = itFound != std::cend(_activityData)
        ? static_cast<int>(std::distance(std::cbegin(_activityData), itFound))
        : -1;

    if (startIndex > 0) {
        for (int dataIndex = startIndex, iteration = 0; dataIndex >= 0 && iteration < limit;
             --dataIndex, ++iteration) {
            if (_activityData[dataIndex].toObject().value(QStringLiteral("activity_id")).toInt()
                > sinceId - limit) {
                data.append(_activityData[dataIndex]);
            }
        }
    }

    QJsonObject root;
    QJsonObject ocs;
    ocs.insert(QStringLiteral("data"), data);
    root.insert(QStringLiteral("ocs"), ocs);

    return QJsonDocument(root).toJson();
}

QJsonValue FakeRemoteActivityStorage::activityById(const int id) const
{
    const auto itFound = std::find_if(
        std::cbegin(_activityData), std::cend(_activityData), [&id](const QJsonValue &currentActivityValue) {
            const auto currentActivityId =
                currentActivityValue.toObject().value(QStringLiteral("activity_id")).toInt();
            return currentActivityId == id;
        });

    if (itFound != std::cend(_activityData)) {
        return (*itFound);
    }

    return {};
}

int FakeRemoteActivityStorage::startingIdLast() const
{
    return _startingId;
}

int FakeRemoteActivityStorage::numItemsToInsert() const {
    return _numItemsToInsert;
}

int FakeRemoteActivityStorage::totalNumActivites() const {
    return _activityData.count();
}


void TestingALM::startFetchJobWithNumActivities(const int numActivities)
{
    auto *job = new OCC::JsonApiJob(
        accountState()->account(), QLatin1String("ocs/v2.php/apps/activity/api/v2/activity"), this);
    QObject::connect(this, &TestingALM::activityJobStatusCode, this, &TestingALM::slotProcessReceivedActivities);
    QObject::connect(job, &OCC::JsonApiJob::jsonReceived, this, &TestingALM::activitiesReceived);

    QUrlQuery params;
    params.addQueryItem(QLatin1String("since"), QString::number(currentItem()));
    params.addQueryItem(QLatin1String("limit"), QString::number(numActivities));
    job->addQueryParams(params);

    setAndRefreshCurrentlyFetching(true);
    job->start();
}

void TestingALM::startFetchJob()
{
    startFetchJobWithNumActivities();
}

void TestingALM::startMaxActivitiesFetchJob()
{
    startFetchJobWithNumActivities(_maxActivities + 1);
}

void TestingALM::slotProcessReceivedActivities()
{
    auto finalListCopy = _finalList;
    for (int i = _numRowsPrev; i < rowCount(); ++i) {
        const auto modelIndex = index(i, 0);
        auto activity = finalListCopy.at(modelIndex.row());
        if (activity._links.isEmpty()) {
            const auto activityJsonObject = FakeRemoteActivityStorage::instance()->activityById(activity._id);

            if (!activityJsonObject.isNull()) {
                // because "_links" are normally populated within the notificationhandler.cpp, which we don't run as part of this unit test, we have to fill them here
                // TODO: move the logic to populate "_links" to "activitylistmodel.cpp"
                const auto actions = activityJsonObject.toObject().value("actions").toArray();
                for (const auto &action : actions) {
                    activity._links.append(OCC::ActivityLink::createFomJsonObject(action.toObject()));
                }
            }
        }

        finalListCopy[modelIndex.row()] = activity;
    }

    _finalList = finalListCopy;
    emit allConflictsChanged();

    setAndRefreshCurrentlyFetching(false);
    emit activitiesProcessed();
}

}
