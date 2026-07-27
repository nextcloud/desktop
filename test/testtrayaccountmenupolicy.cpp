/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "tray/trayaccountmenupolicy.h"

#include <QtTest>

#include <cstddef>

using namespace OCC;

class TestTrayAccountMenuPolicy : public QObject
{
    Q_OBJECT

private slots:
    void testConnectedAccountShowsServerBackedSections()
    {
        const auto policy = TrayAccountMenuPolicy{true, true};

        QVERIFY(policy.showConnectedSections());
        QVERIFY(policy.disconnectedEntries().empty());
        QVERIFY(policy.fetchActivityPreview());
    }

    void testDisconnectedAccountShowsOnlyLocalFolderSeparatorAndReconnect()
    {
        const auto policy = TrayAccountMenuPolicy{false, true};
        const auto entries = policy.disconnectedEntries();

        QVERIFY(!policy.showConnectedSections());
        QVERIFY(!policy.fetchActivityPreview());
        QCOMPARE_EQ(entries.size(), std::size_t{3});
        QVERIFY(entries[0] == TrayAccountMenuEntry::LocalFolder);
        QVERIFY(entries[1] == TrayAccountMenuEntry::Separator);
        QVERIFY(entries[2] == TrayAccountMenuEntry::Reconnect);
    }

    void testDisconnectedPublicShareShowsOnlyLocalFolder()
    {
        const auto policy = TrayAccountMenuPolicy{false, false};
        const auto entries = policy.disconnectedEntries();

        QVERIFY(!policy.showConnectedSections());
        QVERIFY(!policy.fetchActivityPreview());
        QCOMPARE_EQ(entries.size(), std::size_t{1});
        QVERIFY(entries[0] == TrayAccountMenuEntry::LocalFolder);
    }

    void testReconnectMode()
    {
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(false, true, true),
            TrayAccountReconnectMode::SignIn);
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(false, false, true),
            TrayAccountReconnectMode::RetryConnection);
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(true, false, true),
            TrayAccountReconnectMode::None);
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(false, true, false),
            TrayAccountReconnectMode::None);
    }
};

QTEST_APPLESS_MAIN(TestTrayAccountMenuPolicy)
#include "testtrayaccountmenupolicy.moc"
