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

class TestSystraySyncControl : public QObject
{
    Q_OBJECT

    SystraySyncControlTestHelper _helper;

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(_helper.initialize());
    }

    void cleanupTestCase()
    {
        _helper.cleanup();
    }

    /** @brief The QML application menu binds its sync entries to these properties. */
    void syncControlPropertiesFollowTheSyncControlState()
    {
        const auto systray = Systray::instance();
        auto syncControlStateSpy = QSignalSpy{systray, &Systray::syncControlStateChanged};

        // An account without classic folders is also the state used by a File Provider-only client.
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);
        QVERIFY(!systray->canPauseSync());
        QVERIFY(!systray->canResumeSync());

        QVERIFY(_helper.addClassicFolders());
        QVERIFY(!syncControlStateSpy.isEmpty());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);
        QVERIFY(systray->canPauseSync());
        QVERIFY(!systray->canResumeSync());

        syncControlStateSpy.clear();
        systray->setSyncIsPaused(true);
        QVERIFY(!syncControlStateSpy.isEmpty());
        QVERIFY(_helper.firstFolder()->syncPaused());
        QVERIFY(_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Resume);
        QVERIFY(!systray->canPauseSync());
        QVERIFY(systray->canResumeSync());

        syncControlStateSpy.clear();
        _helper.firstFolder()->setSyncPaused(false);
        QVERIFY(!syncControlStateSpy.isEmpty());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::PauseAndResume);
        QVERIFY(systray->canPauseSync());
        QVERIFY(systray->canResumeSync());

        systray->setSyncIsPaused(false);
        QVERIFY(!_helper.firstFolder()->syncPaused());
        QVERIFY(!_helper.secondFolder()->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);
        QVERIFY(systray->canPauseSync());
        QVERIFY(!systray->canResumeSync());
    }
};

QTEST_MAIN(TestSystraySyncControl)
#include "testsystraysynccontrol.moc"
