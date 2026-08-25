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

private Q_SLOTS:
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
        QVERIFY(entries[0] == TrayAccountMenuPolicy::Entry::LocalFolder);
        QVERIFY(entries[1] == TrayAccountMenuPolicy::Entry::Separator);
        QVERIFY(entries[2] == TrayAccountMenuPolicy::Entry::Reconnect);
    }

    void testDisconnectedPublicShareShowsOnlyLocalFolder()
    {
        const auto policy = TrayAccountMenuPolicy{false, false};
        const auto entries = policy.disconnectedEntries();

        QVERIFY(!policy.showConnectedSections());
        QVERIFY(!policy.fetchActivityPreview());
        QCOMPARE_EQ(entries.size(), std::size_t{1});
        QVERIFY(entries[0] == TrayAccountMenuPolicy::Entry::LocalFolder);
    }

    void testReconnectMode()
    {
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(false, true, true),
            TrayAccountMenuPolicy::ReconnectMode::SignIn);
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(false, false, true),
            TrayAccountMenuPolicy::ReconnectMode::RetryConnection);
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(true, false, true),
            TrayAccountMenuPolicy::ReconnectMode::None);
        QCOMPARE_EQ(
            TrayAccountMenuPolicy::reconnectMode(false, true, false),
            TrayAccountMenuPolicy::ReconnectMode::None);
    }
};

QTEST_APPLESS_MAIN(TestTrayAccountMenuPolicy)
#include "testtrayaccountmenupolicy.moc"
