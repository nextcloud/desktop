/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QObject>
#include <QTest>
#include <QSignalSpy>

#include "gui/accountstate.h"
#include "logger.h"

#include "endtoendtestutils.h"

#include <QStandardPaths>

class E2eServerSetupTest : public QObject
{
    Q_OBJECT

public:
    E2eServerSetupTest() = default;

private:
    EndToEndTestHelper _helper;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        QSignalSpy accountReady(&_helper, &EndToEndTestHelper::accountReady);
        _helper.startAccountConfig();
        QVERIFY(accountReady.wait(3000));

        const auto accountState = _helper.accountState();
        QSignalSpy accountConnected(accountState.data(), &OCC::AccountState::isConnectedChanged);
        QVERIFY(accountConnected.wait(30000));
    }

    void testBasicPropfind()
    {
        const auto account = _helper.account();
        auto job = new OCC::PropfindJob(account, "/", this);
        QSignalSpy result(job, &OCC::PropfindJob::result);

        job->setProperties(QList<QByteArray>() << "getlastmodified");
        job->start();

        QVERIFY(result.wait(10000));
    }
};

QTEST_MAIN(E2eServerSetupTest)
#include "teste2eserversetup.moc"
