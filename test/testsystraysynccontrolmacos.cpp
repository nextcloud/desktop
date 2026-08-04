/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include "systray.h"

#include "systraysynccontroltesthelper.h"

using namespace OCC;

class TestSystraySyncControlMacOS : public QObject
{
    Q_OBJECT

    SystraySyncControlTestHelper _helper;

private slots:
    void initTestCase()
    {
        QVERIFY(_helper.initialize());
    }

    void cleanupTestCase()
    {
        _helper.cleanup();
    }

    void globalControlIsUnavailableWithoutClassicFoldersAndTogglesAllFolders()
    {
        const auto systray = Systray::instance();

        // An account without classic folders is also the state used by a File Provider-only client.
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);
        systray->toggleSyncPaused();
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);

        QVERIFY(_helper.addClassicFolders());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);

        systray->toggleSyncPaused();

        QVERIFY(_helper.firstFolder()->syncPaused());
        QVERIFY(_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Resume);

        systray->toggleSyncPaused();

        QVERIFY(!_helper.firstFolder()->syncPaused());
        QVERIFY(!_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);
    }
};

QTEST_MAIN(TestSystraySyncControlMacOS)
#include "testsystraysynccontrolmacos.moc"
