/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QStandardPaths>
#include <QTest>

#include "account.h"
#include "accountmanager.h"
#include "configfile.h"
#include "logger.h"
#include "syncenginetestutils.h"
#include "theme.h"

// The goal of this test is to check whether the correct update channel is used when
// multiple accounts are set up.
//
// Note: The behaviour for a branded client isn't tested, because there is no
//       sane way, to have that reported (yet).
//
class TestUpdateChannel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testUpdateChannel()
    {
        QScopedPointer<FakeQNAM> fakeQnam(new FakeQNAM({}));

        // Set override for delete operation. This is needed, because by default FakeQNAM results in an
        // error reply, and also requires a filename in the url (which we don't have here).
        fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(req);
            Q_UNUSED(device);
            QNetworkReply *reply = nullptr;

            if (op == QNetworkAccessManager::DeleteOperation) {
                reply = new FakePayloadReply(op, req, QByteArray(), this);
                return reply;
            }

            return reply;
        });

        {
            auto config = OCC::ConfigFile();
            config.setUpdateChannel(UpdateChannel::Beta.toString());
        }

        {
            auto fakeCreds = new FakeCredentials{fakeQnam.data()};
            fakeCreds->setUserName("Beta");

            auto account = OCC::Account::create();
            account->setCredentials(fakeCreds);
            account->setServerHasValidSubscription(false);
            OCC::AccountManager::instance()->addAccount(account);
        }
        QCOMPARE(OCC::ConfigFile().currentUpdateChannel(), UpdateChannel::Beta.toString());

        {
            auto fakeCreds = new FakeCredentials{fakeQnam.data()};
            fakeCreds->setUserName("EnterpriseStable");

            auto account = OCC::Account::create();
            account->setCredentials(fakeCreds);
            account->setServerHasValidSubscription(true);
            account->setEnterpriseUpdateChannel(UpdateChannel::Stable);
            OCC::AccountManager::instance()->addAccount(account);
        }
        QCOMPARE(OCC::ConfigFile().currentUpdateChannel(), UpdateChannel::Stable.toString());

        {
            auto fakeCreds = new FakeCredentials{fakeQnam.data()};
            fakeCreds->setUserName("EnterpriseEnterprise");

            auto account = OCC::Account::create();
            account->setCredentials(fakeCreds);
            account->setServerHasValidSubscription(true);
            account->setEnterpriseUpdateChannel(UpdateChannel::Enterprise);
            OCC::AccountManager::instance()->addAccount(account);
        }
        QCOMPARE(OCC::ConfigFile().currentUpdateChannel(), UpdateChannel::Enterprise.toString());

        {
            auto account = OCC::AccountManager::instance()->account("EnterpriseEnterprise@");
            QVERIFY(account);
            OCC::AccountManager::instance()->deleteAccount(account.get());
        }
        QCOMPARE(OCC::ConfigFile().currentUpdateChannel(), UpdateChannel::Stable.toString());

        {
            auto account = OCC::AccountManager::instance()->account("EnterpriseStable@");
            QVERIFY(account);
            OCC::AccountManager::instance()->deleteAccount(account.get());
        }
        QCOMPARE(OCC::ConfigFile().currentUpdateChannel(), UpdateChannel::Beta.toString());
    }
};

QTEST_GUILESS_MAIN(TestUpdateChannel)
#include "testupdatechannel.moc"
