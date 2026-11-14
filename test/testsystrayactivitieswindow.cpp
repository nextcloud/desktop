/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QGuiApplication>
#include <QQuickWindow>

#include "activity/activitylistmodel.h"
#include "activity/sortedactivitylistmodel.h"
#include "activity/syncstatussummary.h"
#include "systray.h"
#include "theme.h"
#include "tray/usermodel.h"
#include "wheelhandler.h"

#include "systraysynccontroltesthelper.h"

using namespace OCC;

class TestSystrayActivitiesWindow : public QObject
{
    Q_OBJECT

    SystraySyncControlTestHelper _helper;

    static QList<QQuickWindow *> activitiesWindows()
    {
        auto windows = QList<QQuickWindow *>{};
        const auto topLevelWindows = QGuiApplication::topLevelWindows();
        for (const auto window : topLevelWindows) {
            const auto quickWindow = qobject_cast<QQuickWindow *>(window);
            if (quickWindow && quickWindow->property("activityModel").isValid()) {
                windows.append(quickWindow);
            }
        }
        return windows;
    }

    static QQuickWindow *visibleActivitiesWindow()
    {
        const auto windows = activitiesWindows();
        for (const auto window : windows) {
            if (window->isVisible()) {
                return window;
            }
        }
        return nullptr;
    }

    static QQuickWindow *activitiesWindowForUser(const int userId)
    {
        const auto activityModel = UserModel::instance()->user(userId)->getActivityModel();
        const auto windows = activitiesWindows();
        for (const auto window : windows) {
            if (window->property("activityModel").value<ActivityListModel *>() == activityModel) {
                return window;
            }
        }
        return nullptr;
    }

private Q_SLOTS:
    void initTestCase()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        qmlRegisterType<SortedActivityListModel>("com.nextcloud.desktopclient", 1, 0, "SortedActivityListModel");
        qmlRegisterType<SyncStatusSummary>("com.nextcloud.desktopclient", 1, 0, "SyncStatusSummary");
        qmlRegisterType<WheelHandler>("com.nextcloud.desktopclient", 1, 0, "WheelHandler");
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "UserModel", UserModel::instance());
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Theme", Theme::instance());
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Systray", Systray::instance());

        QVERIFY(_helper.initialize());
        QVERIFY(_helper.addClassicFolders());
        Systray::instance()->createTrayEngine();
    }

    void cleanupTestCase()
    {
        _helper.cleanup();
    }

    void cleanup()
    {
        const auto windows = activitiesWindows();
        for (const auto window : windows) {
            window->hide();
        }
        UserModel::instance()->setCurrentUserId(0);
    }

    void testToggleShowsAndHidesTheWindow()
    {
        const auto systray = Systray::instance();
        QVERIFY(!visibleActivitiesWindow());

        systray->toggleActivitiesWindow();
        const auto window = visibleActivitiesWindow();
        QVERIFY(window);
        QVERIFY(systray->isActivitySurfaceVisible());

        systray->toggleActivitiesWindow();
        QVERIFY(!window->isVisible());
        QVERIFY(!systray->isActivitySurfaceVisible());

        systray->toggleActivitiesWindow();
        QCOMPARE(visibleActivitiesWindow(), window);
        QCOMPARE(activitiesWindows().size(), 1);
    }

    void testToggleHidesAWindowOpenedElsewhere()
    {
        const auto systray = Systray::instance();
        systray->showActivitiesWindow();
        const auto window = visibleActivitiesWindow();
        QVERIFY(window);

        systray->toggleActivitiesWindow();
        QVERIFY(!window->isVisible());
    }

    void testToggleRestoresAMinimizedWindow()
    {
        const auto systray = Systray::instance();
        systray->showActivitiesWindow();
        const auto window = visibleActivitiesWindow();
        QVERIFY(window);
        window->setWindowStates(Qt::WindowMinimized);

        systray->toggleActivitiesWindow();
        QVERIFY(window->isVisible());
        QVERIFY(!window->windowStates().testFlag(Qt::WindowMinimized));
    }

    void testToggleOnlyAffectsTheCurrentUsersWindow()
    {
        const auto systray = Systray::instance();
        const auto userModel = UserModel::instance();
        QCOMPARE(userModel->count(), 2);

        systray->showActivitiesWindow(0);
        const auto firstUserWindow = activitiesWindowForUser(0);
        QVERIFY(firstUserWindow);
        QVERIFY(firstUserWindow->isVisible());

        userModel->setCurrentUserId(1);
        systray->toggleActivitiesWindow();
        const auto secondUserWindow = activitiesWindowForUser(1);
        QVERIFY(secondUserWindow);
        QVERIFY(secondUserWindow != firstUserWindow);
        QVERIFY(secondUserWindow->isVisible());
        QVERIFY(firstUserWindow->isVisible());

        systray->toggleActivitiesWindow();
        QVERIFY(!secondUserWindow->isVisible());
        QVERIFY(firstUserWindow->isVisible());

        userModel->setCurrentUserId(0);
        systray->toggleActivitiesWindow();
        QVERIFY(!firstUserWindow->isVisible());
        QVERIFY(!secondUserWindow->isVisible());
    }

    void testToggleWithoutCurrentUserOpensNoWindow()
    {
        const auto systray = Systray::instance();
        UserModel::instance()->setCurrentUserId(-1);

        systray->toggleActivitiesWindow();
        QVERIFY(!visibleActivitiesWindow());
    }
};

QTEST_MAIN(TestSystrayActivitiesWindow)
#include "testsystrayactivitieswindow.moc"
