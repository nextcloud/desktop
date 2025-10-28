/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <qglobal.h>
#include <QTemporaryDir>
#include <QtTest>

#include "common/utility.h"
#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include "configfile.h"
#include "testhelper.h"
#include "logger.h"

using namespace OCC;

class TestAccount: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testAccountDavPath_unitialized_noCrash()
    {
        AccountPtr account = Account::create();
        [[maybe_unused]] const auto davPath = account->davPath();
    }

    void testAccount_setLimitSettings_globalNetworkLimitFallback()
    {
        using LimitSetting = Account::AccountNetworkTransferLimitSetting;
        AccountPtr account = Account::create();

        const auto setLimitSettings = [account](const LimitSetting setting) -> void {
            account->setDownloadLimitSetting(setting);
            account->setUploadLimitSetting(setting);
        };

        const auto verifyLimitSettings = [account](const LimitSetting expectedSetting) -> void {
            QCOMPARE_EQ(expectedSetting, account->downloadLimitSetting());
            QCOMPARE_EQ(expectedSetting, account->uploadLimitSetting());
        };

        // the default setting should be NoLimit
        verifyLimitSettings(LimitSetting::NoLimit);

        // changing it to ManualLimit should succeed
        setLimitSettings(LimitSetting::ManualLimit);
        verifyLimitSettings(LimitSetting::ManualLimit);

        // changing it to AutoLimit should succeed
        setLimitSettings(LimitSetting::AutoLimit);
        verifyLimitSettings(LimitSetting::AutoLimit);

        // changing it to LegacyGlobalLimit (-2) should fall back to NoLimit
        setLimitSettings(LimitSetting::LegacyGlobalLimit);
        verifyLimitSettings(LimitSetting::NoLimit);
    }
};

QTEST_APPLESS_MAIN(TestAccount)
#include "testaccount.moc"
