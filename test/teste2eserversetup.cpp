/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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
