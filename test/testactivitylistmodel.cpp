/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "activitylistmodeltestutils.h"
#include "syncenginetestutils.h"
#include "syncresult.h"

#include <QAbstractItemModelTester>
#include <QDesktopServices>
#include <QSignalSpy>
#include <QTest>

using namespace ActivityListModelTestUtils;

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
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

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
                                       this,
                                       searchResultsReplyDelay);
        });

        OCC::AccountManager::instance()->addAccount(account);

        const auto accName = accountState->account()->displayName();
        const auto accUrl = accountState->account()->url();

        testNotificationActivity = exampleNotificationActivity(accName);
        testSyncResultErrorActivity = exampleSyncResultErrorActivity(accName);
        testSyncFileItemActivity = exampleSyncFileItemActivity(accName, accUrl);
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

    void testRemoveOutdatedNotifications() {
        const auto model = testingALM();
        QCOMPARE(model->rowCount(), 0);

        const auto removeAndAddNotifications = [&model](const OCC::ActivityList &activities) -> void {
            // this is pretty much the core logic of dealing with the retrieved notifications from User::slotBuildNotificationDisplay
            model->removeOutdatedNotifications(activities);

            for (const auto &activity : activities) {
                model->addNotificationToActivityList(activity);
            }
        };

        const auto accName = accountState->account()->displayName();
        const auto notificationOne = exampleNotificationActivity(accName, 1);
        const auto notificationTwo = exampleNotificationActivity(accName, 2);
        const auto notificationThree = exampleNotificationActivity(accName, 3);

        removeAndAddNotifications({notificationOne});
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->activityList().count(notificationOne), 1);
        QCOMPARE(model->activityList().count(notificationTwo), 0);
        QCOMPARE(model->activityList().count(notificationThree), 0);

        removeAndAddNotifications({notificationOne, notificationTwo});
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->activityList().count(notificationOne), 1);
        QCOMPARE(model->activityList().count(notificationTwo), 1);
        QCOMPARE(model->activityList().count(notificationThree), 0);

        removeAndAddNotifications({notificationOne, notificationThree});
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->activityList().count(notificationOne), 1);
        QCOMPARE(model->activityList().count(notificationTwo), 0);
        QCOMPARE(model->activityList().count(notificationThree), 1);

        removeAndAddNotifications({notificationThree});
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->activityList().count(notificationOne), 0);
        QCOMPARE(model->activityList().count(notificationTwo), 0);
        QCOMPARE(model->activityList().count(notificationThree), 1);

        removeAndAddNotifications({});
        QCOMPARE(model->rowCount(), 0);
        QCOMPARE(model->activityList().count(notificationOne), 0);
        QCOMPARE(model->activityList().count(notificationTwo), 0);
        QCOMPARE(model->activityList().count(notificationThree), 0);
    }

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

        model->addNotificationToActivityList(testNotificationActivity);
        QCOMPARE(model->rowCount(), 53);

        // Test all rows for things in common
        for (int i = 0; i < model->rowCount(); i++) {
            const auto index = model->index(i, 0);

            auto text = index.data(OCC::ActivityListModel::ActionTextRole).toString();

            QVERIFY(index.data(OCC::ActivityListModel::ActionRole).canConvert<int>());
            const auto type = index.data(OCC::ActivityListModel::ActionRole).toInt();
            QVERIFY(type >= OCC::Activity::DummyFetchingActivityType);

            QVERIFY(!index.data(OCC::ActivityListModel::AccountRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::ActionTextColorRole).toString().isEmpty());
            QVERIFY(!index.data(OCC::ActivityListModel::IconRole).toString().isEmpty());
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
                    const auto actionsLinksContextMenu = index.data(OCC::ActivityListModel::ActionsLinksContextMenuRole).toList();

                    // context menu must be shorter than total action links
                    QVERIFY(actionsLinksContextMenu.isEmpty() || actionsLinksContextMenu.size() < actionsLinks.size());

                    // context menu must not contain the primary action
                    QVERIFY(std::find_if(std::begin(actionsLinksContextMenu), std::end(actionsLinksContextMenu),
                                [](const QVariant &entry) { return entry.value<OCC::ActivityLink>()._primary; })
                        == std::end(actionsLinksContextMenu));

                    const auto objectType = index.data(OCC::ActivityListModel::ObjectTypeRole).toString();

                    const auto actionButtonsLinks = index.data(OCC::ActivityListModel::ActionsLinksForActionButtonsRole).toList();

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

                    // remote shares must have 'Accept' and 'Decline' actions
                    if (objectType == QStringLiteral("remote_share")) {
                        QVERIFY(actionsLinks.size() == 2);
                        QVERIFY(actionsLinks[0].value<OCC::ActivityLink>()._primary);
                        QVERIFY(actionsLinks[0].value<OCC::ActivityLink>()._verb == QStringLiteral("POST"));
                        QVERIFY(actionsLinks[1].value<OCC::ActivityLink>()._verb == QStringLiteral("DELETE"));
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
                                QCOMPARE(actionButtonsLinks.size(), OCC::ActivityListModel::maxActionButtons());
                                // in case total actions is longer than ActivityListModel::maxActionButtons, then a sum of action buttons and action menu entries must be equal to a total of action links
                                QCOMPARE(actionButtonsLinks.size() + actionsLinksContextMenu.size(), actionsLinks.size());
                            }
                        } else if ((objectType == QStringLiteral("call"))) {
                            QVERIFY(actionButtonsLinks[0].value<OCC::ActivityLink>()._label == QStringLiteral("Call back"));
                        }
                    }
                }
            }

        } while (prevModelRowCount < model->rowCount());
    };
};

QTEST_MAIN(TestActivityListModel)
#include "testactivitylistmodel.moc"
