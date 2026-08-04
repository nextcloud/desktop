/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QMenu>

#include "systray.h"

#include "systraysynccontroltesthelper.h"

using namespace OCC;

class TestSystraySyncControlQt : public QObject
{
    Q_OBJECT

    SystraySyncControlTestHelper _helper;

    static QAction *syncControlAction(QMenu &menu, Systray *systray)
    {
        setupQtTrayContextMenu(&menu, systray);
        return menu.findChild<QAction *>(QStringLiteral("traySyncControlAction"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(_helper.initialize());
    }

    void cleanupTestCase()
    {
        _helper.cleanup();
    }

    void globalActionIsHiddenWithoutClassicFoldersAndTogglesAllFolders()
    {
        const auto systray = Systray::instance();

        // An account without classic folders is also the state used by a File Provider-only client.
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);
        systray->toggleSyncPaused();
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);

        auto unavailableMenu = QMenu{};
        QVERIFY(!syncControlAction(unavailableMenu, systray));

        QVERIFY(_helper.addClassicFolders());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);

        auto pauseMenu = QMenu{};
        const auto pauseAction = syncControlAction(pauseMenu, systray);
        QVERIFY(pauseAction);
        QCOMPARE(pauseAction->text(), Systray::tr("Pause sync for all"));
        pauseAction->trigger();

        QVERIFY(_helper.firstFolder()->syncPaused());
        QVERIFY(_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Resume);

        auto resumeMenu = QMenu{};
        const auto resumeAction = syncControlAction(resumeMenu, systray);
        QVERIFY(resumeAction);
        QCOMPARE(resumeAction->text(), Systray::tr("Resume sync for all"));
        resumeAction->trigger();

        QVERIFY(!_helper.firstFolder()->syncPaused());
        QVERIFY(!_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);
    }
};

QTEST_MAIN(TestSystraySyncControlQt)
#include "testsystraysynccontrolqt.moc"
