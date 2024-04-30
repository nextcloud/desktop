/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>

#include "updater/updater.h"
#include "updater/ocupdater.h"
#include "logger.h"

using namespace OCC;

class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testVersionToInt()
    {
        qint64 lowVersion = Updater::Helper::versionToInt(1,2,80,3000);
        QCOMPARE(Updater::Helper::stringVersionToInt("1.2.80.3000"), lowVersion);

        qint64 highVersion = Updater::Helper::versionToInt(99,2,80,3000);
        qint64 currVersion = Updater::Helper::currentVersionToInt();
        QVERIFY(currVersion > 0);
        QVERIFY(currVersion > lowVersion);
        QVERIFY(currVersion < highVersion);
    }

};

QTEST_APPLESS_MAIN(TestUpdater)
#include "testupdater.moc"
