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

#include "gui/tray/activitylistmodel.h"

#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "syncenginetestutils.h"
#include "syncresult.h"

#include <QAbstractItemModelTester>
#include <QDesktopServices>
#include <QSignalSpy>
#include <QTest>

namespace {
constexpr auto startingId = 90000;
}

static QByteArray fake404Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":404,"message":"Invalid query, please check the syntax. API specifications are here: http:\/\/www.freedesktop.org\/wiki\/Specifications\/open-collaboration-services.\n"},"data":[]}}
)";

static QByteArray fake400Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}}
)";

static QByteArray fake500Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":500,"message":"Internal Server Error.\n"},"data":[]}}
)";

class FakeRemoteActivityStorage
{
    FakeRemoteActivityStorage() = default;

public:
    static FakeRemoteActivityStorage *instance()
    {
        if (!_instance) {
            _instance = new FakeRemoteActivityStorage();
            _instance->init();
        }

        return _instance;
    }

    static void destroy()
    {
        if (_instance) {
            delete _instance;
        }

        _instance = nullptr;
    }

    void init()
    {
        if (!_activityData.isEmpty()) {
            return;
        }

        _metaSuccess = {{QStringLiteral("status"), QStringLiteral("ok")}, {QStringLiteral("statuscode"), 200},
            {QStringLiteral("message"), QStringLiteral("OK")}};

        initActivityData();
    }

    void initActivityData()
    {
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
            activity.insert(QStringLiteral("message"), QStringLiteral("You enabled two-factor authentication but did not generate backup codes yet. They are needed to restore access to your account in case you lose your second factor."));
            activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
            activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
            activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/password.svg"));

            QJsonArray actionsArray;

            QJsonObject secondaryAction;
            secondaryAction.insert(QStringLiteral("label"), QStringLiteral("Dismiss"));
            secondaryAction.insert(QStringLiteral("link"),
                                   QString(QStringLiteral("ocs/v2.php/apps/notifications/api/v2/notifications/19867")));
            secondaryAction.insert(QStringLiteral("type"), QStringLiteral("DELETE"));
            secondaryAction.insert(QStringLiteral("primary"), false);
            actionsArray.push_back(secondaryAction);

            activity.insert(QStringLiteral("actions"), actionsArray);

            _activityData.push_back(activity);

            _startingId++;
        }

        _startingId--;
    }

    const QByteArray activityJsonData(int sinceId, int limit)
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

    QJsonValue activityById(int id)
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

    [[nodiscard]] int startingIdLast() const { return _startingId; }

private:
    static FakeRemoteActivityStorage *_instance;
    QJsonArray _activityData;
    QVariantMap _metaSuccess;
    quint32 _numItemsToInsert = 30;
    int _startingId = startingId;
};

FakeRemoteActivityStorage *FakeRemoteActivityStorage::_instance = nullptr;

class TestingALM : public OCC::ActivityListModel
{
    Q_OBJECT

public:
    TestingALM() = default;

    void startFetchJob() override
    {
        auto *job = new OCC::JsonApiJob(
            accountState()->account(), QLatin1String("ocs/v2.php/apps/activity/api/v2/activity"), this);
        QObject::connect(this, &TestingALM::activityJobStatusCode, this, &TestingALM::slotProcessReceivedActivities);
        QObject::connect(job, &OCC::JsonApiJob::jsonReceived, this, &TestingALM::activitiesReceived);

        QUrlQuery params;
        params.addQueryItem(QLatin1String("since"), QString::number(currentItem()));
        params.addQueryItem(QLatin1String("limit"), QString::number(50));
        job->addQueryParams(params);

        setAndRefreshCurrentlyFetching(true);
        job->start();
    }

public slots:
    void slotProcessReceivedActivities()
    {
        if (rowCount() > _numRowsPrev) {
            auto finalListCopy = finalList();
            for (int i = _numRowsPrev; i < rowCount(); ++i) {
                const auto modelIndex = index(i, 0);
                auto activity = finalListCopy.at(modelIndex.row());
                if (activity._links.isEmpty()) {
                    const auto activityJsonObject = FakeRemoteActivityStorage::instance()->activityById(activity._id);

                    if (!activityJsonObject.isNull()) {
                        // because "_links" are normally populated within the notificationhandler.cpp, which we don't run as part of this unit test, we have to fill them here
                        // TODO: move the logic to populate "_links" to "activitylistmodel.cpp"
                        auto actions = activityJsonObject.toObject().value("actions").toArray();
                        foreach (auto action, actions) {
                            activity._links.append(OCC::ActivityLink::createFomJsonObject(action.toObject()));
                        }

                        finalListCopy[modelIndex.row()] = activity;
                    }
                }
            }

            setFinalList(finalListCopy);
        }
        _numRowsPrev = rowCount();
        setAndRefreshCurrentlyFetching(false);
        emit activitiesProcessed();
    }
signals:
    void activitiesProcessed();

private:
    int _numRowsPrev = 0;
};

class TestActivityListModel : public QObject
{
    Q_OBJECT

public:
    TestActivityListModel() = default;
    ~TestActivityListModel() override
    {
        OCC::AccountManager::instance()->deleteAccount(accountState.data());
    }

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;
    QScopedPointer<OCC::AccountState> accountState;

    OCC::Activity testNotificationActivity;
    OCC::Activity testSyncResultErrorActivity;
    OCC::Activity testSyncFileItemActivity;
    OCC::Activity testFileIgnoredActivity;

    static constexpr int searchResultsReplyDelay = 100;

    QSharedPointer<TestingALM> testingALM() {
        QSharedPointer<TestingALM> model(new TestingALM);
        model->setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(model.data());

        return model;
    }

    void testActivityAdd(void(OCC::ActivityListModel::*addingMethod)(const OCC::Activity&, OCC::ActivityListModel::ErrorType), OCC::Activity &activity) {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        (model.data()->*addingMethod)(activity, OCC::ActivityListModel::ErrorType::SyncError);
        QCOMPARE(model->rowCount(), 1);

        const auto index = model->index(0, 0);
        QVERIFY(index.isValid());
    }

    void testActivityAdd(void(OCC::ActivityListModel::*addingMethod)(const OCC::Activity&), OCC::Activity &activity) {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        (model.data()->*addingMethod)(activity);
        QCOMPARE(model->rowCount(), 1);

        const auto index = model->index(0, 0);
        QVERIFY(index.isValid());
    }

    void testActivityAdd(void(OCC::ActivityListModel::*addingMethod)(const OCC::Activity&, OCC::ActivityListModel::ErrorType), OCC::Activity &activity, OCC::ActivityListModel::ErrorType type) {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        (model.data()->*addingMethod)(activity, type);
        QCOMPARE(model->rowCount(), 1);

        const auto index = model->index(0, 0);
        QVERIFY(index.isValid());
    }

private slots:
    void initTestCase()
    {
        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));

        accountState.reset(new OCC::AccountState(account));

        fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device);
            QNetworkReply *reply = nullptr;

            const auto urlQuery = QUrlQuery(req.url());
            const auto format = urlQuery.queryItemValue(QStringLiteral("format"));
            const auto since = urlQuery.queryItemValue(QStringLiteral("since")).toInt();
            const auto limit = urlQuery.queryItemValue(QStringLiteral("limit")).toInt();
            const auto path = req.url().path();

            if (!req.url().toString().startsWith(accountState->account()->url().toString())) {
                reply = new FakeErrorReply(op, req, this, 404, fake404Response);
            }
            if (format != QStringLiteral("json")) {
                reply = new FakeErrorReply(op, req, this, 400, fake400Response);
            }

            if (path.startsWith(QStringLiteral("/ocs/v2.php/apps/activity/api/v2/activity"))) {
                reply = new FakePayloadReply(op, req, FakeRemoteActivityStorage::instance()->activityJsonData(since, limit), searchResultsReplyDelay, fakeQnam.data());
            }

            if (!reply) {
                return qobject_cast<QNetworkReply*>(new FakeErrorReply(op, req, this, 404, QByteArrayLiteral("{error: \"Not found!\"}")));
            }

            return reply;
        });

        OCC::AccountManager::instance()->addAccount(account);

        // Activity comparison is done by checking type, id, and accName
        // We need an activity with these details, at least
        testNotificationActivity._accName = accountState->account()->displayName();
        testNotificationActivity._id = 1;
        testNotificationActivity._type = OCC::Activity::NotificationType;
        testNotificationActivity._dateTime = QDateTime::currentDateTime();
        testNotificationActivity._subject = QStringLiteral("Sample notification text");

        testSyncResultErrorActivity._id = 2;
        testSyncResultErrorActivity._type = OCC::Activity::SyncResultType;
        testSyncResultErrorActivity._syncResultStatus = OCC::SyncResult::Error;
        testSyncResultErrorActivity._dateTime = QDateTime::currentDateTime();
        testSyncResultErrorActivity._subject = QStringLiteral("Sample failed sync text");
        testSyncResultErrorActivity._message = QStringLiteral("/path/to/thingy");
        testSyncResultErrorActivity._link = QStringLiteral("/path/to/thingy");
        testSyncResultErrorActivity._accName = accountState->account()->displayName();

        testSyncFileItemActivity._id = 3;
        testSyncFileItemActivity._type = OCC::Activity::SyncFileItemType; //client activity
        testSyncFileItemActivity._syncFileItemStatus = OCC::SyncFileItem::Success;
        testSyncFileItemActivity._dateTime = QDateTime::currentDateTime();
        testSyncFileItemActivity._message = QStringLiteral("Sample file successfully synced text");
        testSyncFileItemActivity._link = accountState->account()->url();
        testSyncFileItemActivity._accName = accountState->account()->displayName();
        testSyncFileItemActivity._file = QStringLiteral("xyz.pdf");

        testFileIgnoredActivity._id = 4;
        testFileIgnoredActivity._type = OCC::Activity::SyncFileItemType;
        testFileIgnoredActivity._syncFileItemStatus = OCC::SyncFileItem::FileIgnored;
        testFileIgnoredActivity._dateTime = QDateTime::currentDateTime();
        testFileIgnoredActivity._subject = QStringLiteral("Sample ignored file sync text");
        testFileIgnoredActivity._link = accountState->account()->url();
        testFileIgnoredActivity._accName = accountState->account()->displayName();
        testFileIgnoredActivity._folder = QStringLiteral("thingy");
        testFileIgnoredActivity._file = QStringLiteral("test.txt");
    };

    // Test receiving activity from server
    void testFetchingRemoteActivity() {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        model->setCurrentItem(FakeRemoteActivityStorage::instance()->startingIdLast());
        model->startFetchJob();
        QSignalSpy activitiesJob(model.data(), &TestingALM::activitiesProcessed);
        QVERIFY(activitiesJob.wait(3000));
        QCOMPARE(model->rowCount(), 50);
    };

    // Test receiving activity from local user action
    void testLocalSyncFileAction() {
        testActivityAdd(&TestingALM::addSyncFileItemToActivityList, testSyncFileItemActivity);
    };

    void testAddNotification() {
        testActivityAdd(&TestingALM::addNotificationToActivityList, testNotificationActivity);
    };

    void testAddError() {
        testActivityAdd(&TestingALM::addErrorToActivityList, testSyncResultErrorActivity, OCC::ActivityListModel::ErrorType::SyncError);
    };

    void testAddIgnoredFile() {
        testActivityAdd(&TestingALM::addIgnoredFileToList, testFileIgnoredActivity);
    };

    // Test removing activity from list
    void testRemoveActivityWithRow() {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        model->addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model->rowCount(), 1);

        model->removeActivityFromActivityList(0);
        QCOMPARE(model->rowCount(), 0);
    }

    void testRemoveActivityWithActivity() {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        model->addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model->rowCount(), 1);

        model->removeActivityFromActivityList(testNotificationActivity);
        QCOMPARE(model->rowCount(), 0);
    }

    void testDummyFetchingActivitiesActivity() {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        model->setCurrentItem(FakeRemoteActivityStorage::instance()->startingIdLast());
        model->startFetchJob();

        // Check for the dummy before activities have arrived
        QCOMPARE(model->rowCount(), 1);

        QSignalSpy activitiesJob(model.data(), &TestingALM::activitiesProcessed);
        QVERIFY(activitiesJob.wait(3000));
        // Test the dummy was removed
        QCOMPARE(model->rowCount(), 50);
    }

    // Test getting the data from the model
    void testData() {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        model->setCurrentItem(FakeRemoteActivityStorage::instance()->startingIdLast());
        model->startFetchJob();
        QSignalSpy activitiesJob(model.data(), &TestingALM::activitiesProcessed);
        QVERIFY(activitiesJob.wait(3000));
        QCOMPARE(model->rowCount(), 50);

        model->addSyncFileItemToActivityList(testSyncFileItemActivity);
        QCOMPARE(model->rowCount(), 51);

        model->addErrorToActivityList(testSyncResultErrorActivity, OCC::ActivityListModel::ErrorType::SyncError);
        QCOMPARE(model->rowCount(), 52);

        model->addIgnoredFileToList(testFileIgnoredActivity);
        QCOMPARE(model->rowCount(), 53);

        model->addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model->rowCount(), 54);

        // Test all rows for things in common
        for (int i = 0; i < model->rowCount(); i++) {
            const auto index = model->index(i, 0);

            auto text = index.data(OCC::ActivityListModel::ActionTextRole).toString();

            QVERIFY(index.data(OCC::ActivityListModel::ActionRole).canConvert<int>());
            const auto type = index.data(OCC::ActivityListModel::ActionRole).toInt();
            QVERIFY(type >= OCC::Activity::DummyFetchingActivityType);

            QVERIFY(!index.data(OCC::ActivityListModel::AccountRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::ActionTextColorRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::DarkIconRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::LightIconRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::PointInTimeRole).toString().isEmpty());

            QVERIFY(index.data(OCC::ActivityListModel::ObjectTypeRole).canConvert<int>());
            QVERIFY(index.data(OCC::ActivityListModel::ObjectNameRole).canConvert<QString>());
            QVERIFY(index.data(OCC::ActivityListModel::ObjectIdRole).canConvert<int>());
            QVERIFY(index.data(OCC::ActivityListModel::ActionsLinksRole).canConvert<QList<QVariant>>());
            QVERIFY(index.data(OCC::ActivityListModel::ActionTextRole).canConvert<QString>());
            QVERIFY(index.data(OCC::ActivityListModel::MessageRole).canConvert<QString>());
            QVERIFY(index.data(OCC::ActivityListModel::LinkRole).canConvert<QUrl>());

            QVERIFY(index.data(OCC::ActivityListModel::ActionsLinksForActionButtonsRole).canConvert<QList<QVariant>>());

            QVERIFY(index.data(OCC::ActivityListModel::AccountConnectedRole).canConvert<bool>());
            QVERIFY(index.data(OCC::ActivityListModel::DisplayActions).canConvert<bool>());

            QVERIFY(index.data(OCC::ActivityListModel::TalkNotificationConversationTokenRole).canConvert<QString>());
            QVERIFY(index.data(OCC::ActivityListModel::TalkNotificationMessageIdRole).canConvert<QString>());
            QVERIFY(index.data(OCC::ActivityListModel::TalkNotificationMessageSentRole).canConvert<QString>());

            QVERIFY(index.data(OCC::ActivityListModel::ActivityRole).canConvert<OCC::Activity>());

            // Unfortunately, trying to check anything relating to filepaths causes a crash
            // when the folder manager is invoked by the model to look for the relevant file
        }
    };

    void testActivityActionsData()
    {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);
        model->setCurrentItem(FakeRemoteActivityStorage::instance()->startingIdLast());

        int prevModelRowCount = model->rowCount();

        do {
            prevModelRowCount = model->rowCount();
            model->startFetchJob();
            QSignalSpy activitiesJob(model.data(), &TestingALM::activitiesProcessed);
            QVERIFY(activitiesJob.wait(3000));


            for (int i = prevModelRowCount; i < model->rowCount(); i++) {
                const auto index = model->index(i, 0);

                const auto actionsLinks = index.data(OCC::ActivityListModel::ActionsLinksRole).toList();
                if (!actionsLinks.isEmpty()) {
                    const auto actionsLinksContextMenu =
                        index.data(OCC::ActivityListModel::ActionsLinksContextMenuRole).toList();

                    // context menu must be shorter than total action links
                    QVERIFY(actionsLinksContextMenu.isEmpty() || actionsLinksContextMenu.size() < actionsLinks.size());

                    // context menu must not contain the primary action
                    QVERIFY(std::find_if(std::begin(actionsLinksContextMenu), std::end(actionsLinksContextMenu),
                                [](const QVariant &entry) { return entry.value<OCC::ActivityLink>()._primary; })
                        == std::end(actionsLinksContextMenu));

                    const auto objectType = index.data(OCC::ActivityListModel::ObjectTypeRole).toString();

                    const auto actionButtonsLinks =
                        index.data(OCC::ActivityListModel::ActionsLinksForActionButtonsRole).toList();

                    // Login attempt notification
                    if (objectType == QStringLiteral("2fa_id")) {
                        QVERIFY(actionsLinks.size() == 2);
                        QVERIFY(actionsLinks[0].value<OCC::ActivityLink>()._primary);
                        QVERIFY(!actionsLinks[1].value<OCC::ActivityLink>()._primary);
                        QVERIFY(actionsLinksContextMenu.isEmpty());
                    }

                    // Generate 2FA backup codes notification
                    if (objectType == QStringLiteral("create")) {
                        QVERIFY(actionsLinks.size() == 1);
                        QVERIFY(!actionsLinks[0].value<OCC::ActivityLink>()._primary);
                        QVERIFY(actionsLinksContextMenu.isEmpty());
                    }

                    if ((objectType == QStringLiteral("chat") || objectType == QStringLiteral("call")
                            || objectType == QStringLiteral("room"))) {

                        auto replyActionPos = 0;
                        if (objectType == QStringLiteral("call")) {
                            replyActionPos = 1;
                        }

                        // both action links and buttons must contain a "REPLY" verb element as secondary action
                        QVERIFY(actionsLinks[replyActionPos].value<OCC::ActivityLink>()._verb == QStringLiteral("REPLY"));
                        //QVERIFY(actionButtonsLinks[replyActionPos].value<OCC::ActivityLink>()._verb == QStringLiteral("REPLY"));

                        // the first action button for chat must have image set
                        //QVERIFY(!actionButtonsLinks[replyActionPos].value<OCC::ActivityLink>()._imageSource.isEmpty());
                        //QVERIFY(!actionButtonsLinks[replyActionPos].value<OCC::ActivityLink>()._imageSourceHovered.isEmpty());

                        // logic for "chat" and other types of activities with multiple actions
                        if ((objectType == QStringLiteral("chat")
                                || (objectType != QStringLiteral("room") && objectType != QStringLiteral("call")))) {

                            // button's label for "chat" must be renamed to "Reply"
                            QVERIFY(actionButtonsLinks[0].value<OCC::ActivityLink>()._label == QObject::tr("Reply"));

                            if (static_cast<quint32>(actionsLinks.size()) > OCC::ActivityListModel::maxActionButtons()) {
                                // in case total actions is longer than ActivityListModel::maxActionButtons, only one button must be present in a list of action buttons
                                QVERIFY(actionButtonsLinks.size() == 1);
                                const auto actionButtonsAndContextMenuEntries = actionButtonsLinks + actionsLinksContextMenu;
                                // in case total actions is longer than ActivityListModel::maxActionButtons, then a sum of action buttons and action menu entries must be equal to a total of action links
                                QVERIFY(actionButtonsLinks.size() + actionsLinksContextMenu.size() == actionsLinks.size());
                            }
                        } else if ((objectType == QStringLiteral("call"))) {
                            QVERIFY(
                                actionButtonsLinks[0].value<OCC::ActivityLink>()._label == QStringLiteral("Call back"));
                        }
                    }
                }
            }

        } while (prevModelRowCount < model->rowCount());
    };
};

QTEST_MAIN(TestActivityListModel)
#include "testactivitylistmodel.moc"
