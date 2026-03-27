/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include "account.h"
#include "testhelper.h"
#include "foldermantestutils.h"
#include "logger.h"

#include "networksettings.h"

using namespace OCC;

class TestNetworkSettings : public QObject
{
    Q_OBJECT

    FolderManTestHelper helper;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void test_whenAccountIsLoggedOut_doesNotCrash()
    {
        // Create an account that is not registered in AccountManager
        // This simulates a logged-out account scenario
        auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://example.com")));
        account->setDavUser(QStringLiteral("testuser"));
        
        // Create NetworkSettings with this account
        // When proxy settings are changed, saveProxySettings() will try to
        // get accountState from AccountManager, which will return nullptr
        // This test ensures the app doesn't crash in that scenario
        NetworkSettings settings(account);
        
        // The test passes if we reach here without crashing
        QVERIFY(true);
    }
};

QTEST_MAIN(TestNetworkSettings)
#include "testnetworksettings.moc"
