/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "activity/activitylistmodel.h"
#include "activity/sortedactivitylistmodel.h"
#include "activity/syncstatussummary.h"
#include "activitylistmodeltestutils.h"
#include "systray.h"
#include "syncenginetestutils.h"
#include "wheelhandler.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QPointer>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QTest>

using namespace ActivityListModelTestUtils;

class TestActivitiesWindow : public QObject
{
    Q_OBJECT

public:
    TestActivitiesWindow()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
    }

    ~TestActivitiesWindow() override
    {
        if (accountState) {
            OCC::AccountManager::instance()->deleteAccount(accountState.data());
        }
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        qmlRegisterType<OCC::SyncStatusSummary>("com.nextcloud.desktopclient", 1, 0, "SyncStatusSummary");
        qmlRegisterType<OCC::SortedActivityListModel>("com.nextcloud.desktopclient", 1, 0, "SortedActivityListModel");
        qmlRegisterType<WheelHandler>("com.nextcloud.desktopclient", 1, 0, "WheelHandler");
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Systray", OCC::Systray::instance());

        fakeQnam.reset(new FakeQNAM({}));
        account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(QStringLiteral("https://example.com")));
        accountState = OCC::AccountManager::instance()->addAccount(account);
        QVERIFY(accountState);
    }

    void testOpeningResetsViewportWithoutInterruptingLiveUpdates()
    {
        TestingALM activityModel;
        activityModel.setAccountState(accountState.data());

        const auto accountName = account->displayName();
        for (auto id = 1; id <= 40; ++id) {
            const auto activity = exampleNotificationActivity(accountName, id);
            activityModel.addNotificationToActivityList(activity);
        }

        QQmlApplicationEngine engine;
        engine.addImportPath(QStringLiteral("qrc:/qml/theme"));

        QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/src/gui/activity/qml/ActivitiesWindow.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        QScopedPointer<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("activityModel"), QVariant::fromValue(&activityModel)},
        }));
        QVERIFY2(object, qPrintable(component.errorString()));

        const auto window = qobject_cast<QQuickWindow *>(object.data());
        QVERIFY(window);

        window->show();
        QTRY_VERIFY(window->isVisible());

        const auto activityList = window->findChild<QObject *>(QStringLiteral("activityList"));
        const auto activityListView = window->findChild<QObject *>(QStringLiteral("activityListView"));
        const auto newActivitiesButtonLoader = window->findChild<QObject *>(QStringLiteral("newActivitiesButtonLoader"));
        QVERIFY(activityList);
        QVERIFY(activityListView);
        QVERIFY(newActivitiesButtonLoader);
        QTRY_VERIFY(activityListView->property("contentHeight").toReal() > activityListView->property("height").toReal());

        QVERIFY(QMetaObject::invokeMethod(activityListView, "positionViewAtEnd"));
        QTRY_VERIFY(!activityList->property("atYBeginning").toBool());

        window->hide();
        QTRY_VERIFY(!window->isVisible());
        window->show();
        QTRY_VERIFY(window->isVisible());
        QTRY_VERIFY(activityList->property("atYBeginning").toBool());

        QVERIFY(QMetaObject::invokeMethod(activityListView, "positionViewAtEnd"));
        QTRY_VERIFY(!activityList->property("atYBeginning").toBool());

        auto interactiveActivity = exampleNotificationActivity(accountName, 41);
        OCC::ActivityLink action;
        action._verb = QByteArrayLiteral("POST");
        interactiveActivity._links.push_back(action);
        activityModel.addNotificationToActivityList(interactiveActivity);

        QTRY_VERIFY(!activityList->property("atYBeginning").toBool());
        QTRY_VERIFY(newActivitiesButtonLoader->property("active").toBool());
    }

private:
    QScopedPointer<FakeQNAM> fakeQnam;
    OCC::AccountPtr account;
    QPointer<OCC::AccountState> accountState;
};

QTEST_MAIN(TestActivitiesWindow)
#include "testactivitieswindow.moc"
