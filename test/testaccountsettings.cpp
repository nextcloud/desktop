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

#include "accountsettings.h"

using namespace OCC;

class TestAccountSettings : public QObject
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

    void test_whenAccountStateIsNotConnected_doesNotCrash()
    {
        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        accountState->setStateForTesting(OCC::AccountState::SignedOut);
        QCOMPARE_EQ(accountState->state(), OCC::AccountState::SignedOut);
        AccountSettings a(accountState);
    }

    void test_whenAccountStateIsConnected_doesNotCrash()
    {
        // this occurred because ConnectionValidator used to set the account
        // inside a Account's _e2e member, instead of letting Account itself
        // do that.

        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        QCOMPARE_EQ(accountState->state(), OCC::AccountState::Connected);
        AccountSettings a(accountState);
    }
};

QTEST_MAIN(TestAccountSettings)
#include "testaccountsettings.moc"
