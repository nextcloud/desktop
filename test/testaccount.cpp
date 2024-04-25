/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
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

using namespace OCC;

class TestAccount: public QObject
{
    Q_OBJECT

private slots:
    void testAccountDavPath_unitialized_noCrash()
    {
        AccountPtr account = Account::create();
        [[maybe_unused]] const auto davPath = account->davPath();
    }

    void testAccountMaxRequestSize_initial_minusOne()
    {
        QCOMPARE( Account::create()->getMaxRequestSize(), -1 );
    }

    void testAccountMaxRequestSize_readWrite()
    {
        auto account = Account::create();
        QVERIFY( account->getMaxRequestSize() == -1 );

        for (qint64 i = -2; i < 100000000000; i += 100000) {
            QVERIFY( i != account->getMaxRequestSize() );

            account->setMaxRequestSize(i);
            QCOMPARE( i, account->getMaxRequestSize() );
        }
    }

    void testAccountMaxRequestSize_writeIfLower()
    {
        using Test = std::pair<qint64,qint64>; // <input,expected-output>
        std::vector<Test> tests{
            Test(10000, 10000),
            Test(-1, -1), // reset with -1
            Test(1, 1),
            Test(2, 1),
            Test(0, 0), // reset with 0
            Test(1000, 1000),
            Test(1100, 1000),
            Test(900, 900),
            Test(80, 80),
            Test(1000, 80),
        };

        auto account = Account::create();
        for (auto test : tests) {
            account->setMaxRequestSizeIfLower(test.first);
            QCOMPARE( test.second, account->getMaxRequestSize() );
        }
    }
};

QTEST_APPLESS_MAIN(TestAccount)
#include "testaccount.moc"
