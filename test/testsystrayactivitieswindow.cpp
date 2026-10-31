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
};

QTEST_MAIN(TestSystrayActivitiesWindow)
#include "testsystrayactivitieswindow.moc"
