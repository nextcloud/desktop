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
#include "syncresult.h"
#include "tray/sortedactivitylistmodel.h"

#include <QAbstractItemModelTester>
#include <QDesktopServices>
#include <QSignalSpy>
#include <QTest>

using namespace ActivityListModelTestUtils;

class TestSortedActivityListModel : public QObject
{
    Q_OBJECT

public:
    TestSortedActivityListModel() = default;
    ~TestSortedActivityListModel() override
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

    QSharedPointer<OCC::SortedActivityListModel> testingSortedALM()
    {
        const auto model = new TestingALM;
        model->setAccountState(accountState.data());

        QSharedPointer<OCC::SortedActivityListModel> sortedModel(new OCC::SortedActivityListModel);
        sortedModel->setActivityListModel(model);
        QAbstractItemModelTester sortedModelTester(sortedModel.data());

        return sortedModel;
    }

    void addActivity(QSharedPointer<OCC::SortedActivityListModel> model,
                     void(OCC::ActivityListModel::*addingMethod)(const OCC::Activity&),
                     OCC::Activity &activity)
    {
        const auto originalRowCount = model->rowCount();
        const auto sourceModel = dynamic_cast<TestingALM*>(model->sourceModel());

        (sourceModel->*addingMethod)(activity);
        QCOMPARE(model->rowCount(), originalRowCount + 1);
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
            Q_UNUSED(device)
            return almTestQnamOverride(fakeQnam.data(),
                                       op,
                                       req,
                                       accountState->account()->url().toString(),
                                       this);
        });

        OCC::AccountManager::instance()->addAccount(account);

        const auto accName = accountState->account()->displayName();
        const auto accUrl = accountState->account()->url();

        testNotificationActivity = exampleNotificationActivity(accName);
        testSyncResultErrorActivity = exampleSyncResultErrorActivity(accName);
        testSyncFileItemActivity = exampleSyncFileItemActivity(accName, accUrl);
        testFileIgnoredActivity = exampleFileIgnoredActivity(accName, accUrl);
    };

    void testMatchingRowCounts()
    {
        const auto model = testingSortedALM();
        const auto sourceModel = dynamic_cast<TestingALM*>(model->sourceModel());
        QCOMPARE(sourceModel->rowCount(), 0);
        QCOMPARE(model->rowCount(), sourceModel->rowCount());

        sourceModel->setCurrentItem(FakeRemoteActivityStorage::instance()->startingIdLast());
        sourceModel->startFetchJob();
        QSignalSpy activitiesJob(sourceModel, &TestingALM::activitiesProcessed);
        QVERIFY(activitiesJob.wait(3000));
        QCOMPARE(sourceModel->rowCount(), 50);
        QCOMPARE(model->rowCount(), sourceModel->rowCount());
    }

    void testUpdate()
    {
        const auto model = testingSortedALM();
        const auto sourceModel = dynamic_cast<TestingALM*>(model->sourceModel());

        sourceModel->setCurrentItem(FakeRemoteActivityStorage::instance()->startingIdLast());
        sourceModel->startFetchJob();
        QSignalSpy activitiesJob(sourceModel, &TestingALM::activitiesProcessed);
        QVERIFY(activitiesJob.wait(3000));
        QCOMPARE(sourceModel->rowCount(), 50);

        addActivity(model, &TestingALM::addSyncFileItemToActivityList, testSyncFileItemActivity);
        addActivity(model, &TestingALM::addNotificationToActivityList, testNotificationActivity);
        addActivity(model, &TestingALM::addErrorToActivityList, testSyncResultErrorActivity);
        addActivity(model, &TestingALM::addIgnoredFileToList, testFileIgnoredActivity);
    }

    void testSort()
    {
        const auto model = testingSortedALM();
        const auto sourceModel = dynamic_cast<TestingALM*>(model->sourceModel());

        sourceModel->setCurrentItem(FakeRemoteActivityStorage::instance()->startingIdLast());
        sourceModel->startMaxActivitiesFetchJob();
        QSignalSpy activitiesJob(sourceModel, &TestingALM::activitiesProcessed);
        QVERIFY(activitiesJob.wait(3000));
        QCOMPARE(sourceModel->rowCount(), sourceModel->maxPossibleActivities());

        auto errorSyncFileItemActivity = exampleSyncFileItemActivity(accountState->account()->displayName(), {});
        errorSyncFileItemActivity._message = QStringLiteral("Something went wrong and eveything exploded!");
        errorSyncFileItemActivity._syncFileItemStatus = OCC::SyncFileItem::FatalError;

        addActivity(model, &TestingALM::addSyncFileItemToActivityList, errorSyncFileItemActivity);
        addActivity(model, &TestingALM::addSyncFileItemToActivityList, testSyncFileItemActivity);
        addActivity(model, &TestingALM::addNotificationToActivityList, testNotificationActivity);
        addActivity(model, &TestingALM::addErrorToActivityList, testSyncResultErrorActivity);
        addActivity(model, &TestingALM::addIgnoredFileToList, testFileIgnoredActivity);

        const QVector<OCC::Activity::Type> activityDefaultTypeOrder {
            OCC::Activity::DummyFetchingActivityType,
            OCC::Activity::NotificationType,
            OCC::Activity::SyncResultType,
            OCC::Activity::SyncFileItemType,
            OCC::Activity::ActivityType,
            OCC::Activity::DummyMoreActivitiesAvailableType};
        auto currentTypeSection = 1;
        auto previousType = activityDefaultTypeOrder[currentTypeSection];

        for (auto i = 0; i < model->rowCount(); ++i) {
            const auto index = model->index(i, 0);
            const auto activity = index.data(OCC::ActivityListModel::ActivityRole).value<OCC::Activity>();

            qDebug() << i << activity._type << activity._subject << activity._message;
            if (i == 0) { // Error syncresult activity should be at top
                QCOMPARE(activity._type, OCC::Activity::SyncResultType);
                QCOMPARE(activity._syncResultStatus, OCC::SyncResult::Error);
            } else if (i == 1) { // Error syncfileitem activity should be next up
                QCOMPARE(activity._type, OCC::Activity::SyncFileItemType);
                QCOMPARE(activity._syncFileItemStatus, OCC::SyncFileItem::FatalError);
            } else if (i == 2) { // Ignored file syncfileitem activity should be next up
                QCOMPARE(activity._type, OCC::Activity::SyncFileItemType);
                QCOMPARE(activity._syncFileItemStatus, OCC::SyncFileItem::FileIgnored);
            } else { // Now normal type order
                while (i != 3 && activity._type != previousType) {
                    ++currentTypeSection;
                    previousType = activityDefaultTypeOrder[currentTypeSection];
                }

                QCOMPARE(activity._type, activityDefaultTypeOrder[currentTypeSection]);
            }
        }
    }
};

QTEST_MAIN(TestSortedActivityListModel)
#include "testsortedactivitylistmodel.moc"
