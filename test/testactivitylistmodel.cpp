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

constexpr auto startingId = 90000;

static QByteArray fake404Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":404,"message":"Invalid query, please check the syntax. API specifications are here: http:\/\/www.freedesktop.org\/wiki\/Specifications\/open-collaboration-services.\n"},"data":[]}}
)";

static QByteArray fake400Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}}
)";

static QByteArray fake500Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":500,"message":"Internal Server Error.\n"},"data":[]}}
)";

class TestingALM : public OCC::ActivityListModel
{
    Q_OBJECT

public:
    TestingALM() = default;

    void startFetchJob() override
    {
        auto *job = new OCC::JsonApiJob(accountState()->account(), QLatin1String("ocs/v2.php/apps/activity/api/v2/activity"), this);
        QObject::connect(job, &OCC::JsonApiJob::jsonReceived,
            this, &TestingALM::activitiesReceived);

        QUrlQuery params;
        params.addQueryItem(QLatin1String("since"), QString::number(startingId));
        params.addQueryItem(QLatin1String("limit"), QString::number(50));
        job->addQueryParams(params);

        job->start();
    };
};

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
            _startingId++;

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
        }

        // Insert notification data
        for (quint32 i = 0; i < _numItemsToInsert; i++) {
            _startingId++;
            QJsonObject activity;
            activity.insert(QStringLiteral("activity_id"), _startingId);
            activity.insert(QStringLiteral("object_type"), "calendar");
            activity.insert(QStringLiteral("type"), QStringLiteral("calendar-event"));
            activity.insert(QStringLiteral("subject"), QStringLiteral("You created event %1 in calendar Events").arg(i));
            activity.insert(QStringLiteral("message"), QStringLiteral(""));
            activity.insert(QStringLiteral("object_name"), QStringLiteral(""));
            activity.insert(QStringLiteral("datetime"), QDateTime::currentDateTime().toString(Qt::ISODate));
            activity.insert(QStringLiteral("icon"), QStringLiteral("http://example.de/core/img/places/calendar.svg"));

            _activityData.push_back(activity);
        }
    }

    const QByteArray activityJsonData(int sinceId, int limit)
    {
        QJsonArray data;

        for(int dataIndex = _activityData.size() - 1, iteration = 0;
            dataIndex > 0 && iteration < limit;
            --dataIndex, ++iteration) {

            if(_activityData[dataIndex].toObject().value(QStringLiteral("activity_id")).toInt() > sinceId) {
                data.append(_activityData[dataIndex]);
            }
        }

        QJsonObject root;
        QJsonObject ocs;
        ocs.insert(QStringLiteral("data"), data);
        root.insert(QStringLiteral("ocs"), ocs);

        return QJsonDocument(root).toJson();
    }

private:
    static FakeRemoteActivityStorage *_instance;
    QJsonArray _activityData;
    QVariantMap _metaSuccess;
    quint32 _numItemsToInsert = 30;
    int _startingId = startingId;
};

FakeRemoteActivityStorage *FakeRemoteActivityStorage::_instance = nullptr;

class TestActivityListModel : public QObject
{
    Q_OBJECT

public:
    TestActivityListModel() = default;
    ~TestActivityListModel() override {
        OCC::AccountManager::instance()->deleteAccount(accountState.data());
    }

    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;
    QScopedPointer<OCC::AccountState> accountState;

    OCC::Activity testNotificationActivity;

    static constexpr int searchResultsReplyDelay = 100;

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
    };

    // Test receiving activity from server
    void testFetchingRemoteActivity() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        model.startFetchJob();
        QSignalSpy activitiesJob(&model, &TestingALM::activityJobStatusCode);
        QVERIFY(activitiesJob.wait(3000));
        QCOMPARE(model.rowCount(), 50);
    };

    // Test receiving activity from local user action
    void testLocalSyncFileAction() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        OCC::Activity activity;

        model.addSyncFileItemToActivityList(activity);
        QCOMPARE(model.rowCount(), 1);

        const auto index = model.index(0, 0);
        QVERIFY(index.isValid());
    };

    void testAddNotification() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        model.addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model.rowCount(), 1);

        const auto index = model.index(0, 0);
        QVERIFY(index.isValid());
    };

    void testAddError() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        OCC::Activity activity;

        model.addErrorToActivityList(activity);
        QCOMPARE(model.rowCount(), 1);

        const auto index = model.index(0, 0);
        QVERIFY(index.isValid());
    };

    void testAddIgnoredFile() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        OCC::Activity activity;
        activity._folder = QStringLiteral("thingy");
        activity._file = QStringLiteral("test.txt");

        model.addIgnoredFileToList(activity);
        // We need to add another activity to the model for the combineActivityLists method to be called
        model.addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model.rowCount(), 2);

        const auto index = model.index(0, 0);
        QVERIFY(index.isValid());
    };

    // Test removing activity from list
    void testRemoveActivityWithRow() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        model.addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model.rowCount(), 1);

        model.removeActivityFromActivityList(0);
        QCOMPARE(model.rowCount(), 0);
    }

    void testRemoveActivityWithActivity() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        model.addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model.rowCount(), 1);

        model.removeActivityFromActivityList(testNotificationActivity);
        QCOMPARE(model.rowCount(), 0);
    }

    // Test getting the data from the model
    void testData() {
        TestingALM model;
        model.setAccountState(accountState.data());
        QAbstractItemModelTester modelTester(&model);

        QCOMPARE(model.rowCount(), 0);

        model.startFetchJob();
        QSignalSpy activitiesJob(&model, &TestingALM::activityJobStatusCode);
        QVERIFY(activitiesJob.wait(3000));
        QCOMPARE(model.rowCount(), 50);

        model.addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model.rowCount(), 51);

        OCC::Activity syncResultActivity;
        syncResultActivity._id = 2;
        syncResultActivity._type = OCC::Activity::SyncResultType;
        syncResultActivity._status = OCC::SyncResult::Error;
        syncResultActivity._dateTime = QDateTime::currentDateTime();
        syncResultActivity._subject = QStringLiteral("Sample failed sync text");
        syncResultActivity._message = QStringLiteral("/path/to/thingy");
        syncResultActivity._link = QStringLiteral("/path/to/thingy");
        syncResultActivity._accName = accountState->account()->displayName();
        model.addSyncFileItemToActivityList(syncResultActivity);
        QCOMPARE(model.rowCount(), 52);

        OCC::Activity syncFileItemActivity;
        syncFileItemActivity._id = 3;
        syncFileItemActivity._type = OCC::Activity::SyncFileItemType; //client activity
        syncFileItemActivity._status = OCC::SyncFileItem::Success;
        syncFileItemActivity._dateTime = QDateTime::currentDateTime();
        syncFileItemActivity._message = QStringLiteral("You created xyz.pdf");
        syncFileItemActivity._link = accountState->account()->url();
        syncFileItemActivity._accName = accountState->account()->displayName();
        syncFileItemActivity._file = QStringLiteral("xyz.pdf");
        syncFileItemActivity._fileAction = "";
        model.addSyncFileItemToActivityList(syncFileItemActivity);
        QCOMPARE(model.rowCount(), 53);

        // Test all rows for things in common
        for (int i = 0; i < model.rowCount(); i++) {
            const auto index = model.index(i, 0);

            QVERIFY(index.data(OCC::ActivityListModel::ObjectTypeRole).canConvert<int>());
            const auto type = index.data(OCC::ActivityListModel::ObjectTypeRole).toInt();
            QVERIFY(type >= OCC::Activity::ActivityType);

            QVERIFY(!index.data(OCC::ActivityListModel::ObjectTypeRole).toInt());
            QVERIFY(!index.data(OCC::ActivityListModel::AccountRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::ActionTextColorRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::ActionIconRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::PointInTimeRole).toString().isEmpty());

            QVERIFY(index.data(OCC::ActivityListModel::ActionsLinksRole).canConvert<QList<QVariant>>());
            QVERIFY(index.data(OCC::ActivityListModel::ActionTextRole).canConvert<QString>());
            QVERIFY(index.data(OCC::ActivityListModel::MessageRole).canConvert<QString>());
            QVERIFY(index.data(OCC::ActivityListModel::LinkRole).canConvert<QUrl>());
            QVERIFY(index.data(OCC::ActivityListModel::AccountConnectedRole).canConvert<bool>());
            QVERIFY(index.data(OCC::ActivityListModel::DisplayActions).canConvert<bool>());

            // Unfortunately, trying to check anything relating to filepaths causes a crash
            // when the folder manager is invoked by the model to look for the relevant file
        }
    };

};

QTEST_MAIN(TestActivityListModel)
#include "testactivitylistmodel.moc"
