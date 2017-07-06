/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#include <QtTest>
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
#include <QTemporaryDir>
#endif

#include "utility.h"

using namespace OCC::Utility;

class TestUtility : public QObject
{
    Q_OBJECT

private slots:
    void testFormatFingerprint()
    {
        QVERIFY2(formatFingerprint("68ac906495480a3404beee4874ed853a037a7a8f")
                 == "68:ac:90:64:95:48:0a:34:04:be:ee:48:74:ed:85:3a:03:7a:7a:8f",
		"Utility::formatFingerprint() is broken");
    }
    void testOctetsToString()
    {
        QLocale::setDefault(QLocale("en"));
        QCOMPARE(octetsToString(999) , QString("999 B"));
        QCOMPARE(octetsToString(1024) , QString("1 KB"));
        QCOMPARE(octetsToString(1364) , QString("1 KB"));

        QCOMPARE(octetsToString(9110) , QString("9 KB"));
        QCOMPARE(octetsToString(9910) , QString("10 KB"));
        QCOMPARE(octetsToString(10240) , QString("10 KB"));

        QCOMPARE(octetsToString(123456) , QString("121 KB"));
        QCOMPARE(octetsToString(1234567) , QString("1.2 MB"));
        QCOMPARE(octetsToString(12345678) , QString("12 MB"));
        QCOMPARE(octetsToString(123456789) , QString("118 MB"));
        QCOMPARE(octetsToString(1000LL*1000*1000 * 5) , QString("4.7 GB"));

        QCOMPARE(octetsToString(1), QString("1 B"));
        QCOMPARE(octetsToString(2), QString("2 B"));
        QCOMPARE(octetsToString(1024), QString("1 KB"));
        QCOMPARE(octetsToString(1024*1024), QString("1 MB"));
        QCOMPARE(octetsToString(1024LL*1024*1024), QString("1 GB"));
    }

    void testLaunchOnStartup()
    {
        qsrand(QDateTime::currentDateTime().toTime_t());
        QString postfix = QString::number(qrand());

        const QString appName = QString::fromLatin1("testLaunchOnStartup.%1").arg(postfix);
        const QString guiName = "LaunchOnStartup GUI Name";

        QVERIFY(hasLaunchOnStartup(appName) == false);
        setLaunchOnStartup(appName, guiName, true);
        QVERIFY(hasLaunchOnStartup(appName) == true);
        setLaunchOnStartup(appName, guiName, false);
        QVERIFY(hasLaunchOnStartup(appName) == false);
    }

    void testDurationToDescriptiveString()
    {
        QLocale::setDefault(QLocale("C"));
        //NOTE: in order for the plural to work we would need to load the english translation

        quint64 sec = 1000;
        quint64 hour = 3600 * sec;

        QDateTime current = QDateTime::currentDateTimeUtc();

        QCOMPARE(durationToDescriptiveString2(0), QString("0 second(s)") );
        QCOMPARE(durationToDescriptiveString2(5), QString("0 second(s)") );
        QCOMPARE(durationToDescriptiveString2(1000), QString("1 second(s)") );
        QCOMPARE(durationToDescriptiveString2(1005), QString("1 second(s)") );
        QCOMPARE(durationToDescriptiveString2(56123), QString("56 second(s)") );
        QCOMPARE(durationToDescriptiveString2(90*sec), QString("1 minute(s) 30 second(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour + 20*sec), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour + 70*sec), QString("3 hour(s) 1 minute(s)") );
        QCOMPARE(durationToDescriptiveString2(3*hour + 100*sec), QString("3 hour(s) 2 minute(s)") );
        QCOMPARE(durationToDescriptiveString2(current.msecsTo(current.addYears(4).addMonths(5).addDays(2).addSecs(23*60*60))),
                 QString("4 year(s) 5 month(s)") );
        QCOMPARE(durationToDescriptiveString2(current.msecsTo(current.addDays(2).addSecs(23*60*60))),
                 QString("2 day(s) 23 hour(s)") );

        QCOMPARE(durationToDescriptiveString1(0), QString("0 second(s)") );
        QCOMPARE(durationToDescriptiveString1(5), QString("0 second(s)") );
        QCOMPARE(durationToDescriptiveString1(1000), QString("1 second(s)") );
        QCOMPARE(durationToDescriptiveString1(1005), QString("1 second(s)") );
        QCOMPARE(durationToDescriptiveString1(56123), QString("56 second(s)") );
        QCOMPARE(durationToDescriptiveString1(90*sec), QString("2 minute(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour + 20*sec), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour + 70*sec), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(3*hour + 100*sec), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString1(current.msecsTo(current.addYears(4).addMonths(5).addDays(2).addSecs(23*60*60))),
                 QString("4 year(s)") );
        QCOMPARE(durationToDescriptiveString1(current.msecsTo(current.addDays(2).addSecs(23*60*60))),
                 QString("3 day(s)") );

    }

    void testVersionOfInstalledBinary()
    {
	if( isLinux() ) {
            if ( qgetenv("DISPLAY").isEmpty() ) {
                // Current requires an X-Server
                return;
            }
            // pass the binary name owncloud to the next call. This brakes branding,
            // but branding is not supposed to work with this.
            QString ver = versionOfInstalledBinary(OWNCLOUD_BIN_PATH+QLatin1String("/owncloud"));
	    qDebug() << "Version of installed ownCloud Binary: " << ver;
	    QVERIFY( !ver.isEmpty());

	    QRegExp rx( "ownCloud version \\d+\\.\\d+\\.\\d+.*" );
            QVERIFY( rx.exactMatch(ver));
	} else {
	    QVERIFY( versionOfInstalledBinary().isEmpty());
	}
    }

    void testTimeAgo()
    {
        // Both times in same timezone
        QDateTime d1 = QDateTime::fromString("2015-01-24T09:20:30+01:00", Qt::ISODate);
        QDateTime d2 = QDateTime::fromString("2015-01-23T09:20:30+01:00", Qt::ISODate);
        QString s = timeAgoInWords(d2, d1);
        QCOMPARE(s, QLatin1String("1 day(s) ago"));

        // Different timezones
        QDateTime earlyTS = QDateTime::fromString("2015-01-24T09:20:30+01:00", Qt::ISODate);
        QDateTime laterTS = QDateTime::fromString("2015-01-24T09:20:30-01:00", Qt::ISODate);
        s = timeAgoInWords(earlyTS, laterTS);
        QCOMPARE(s, QLatin1String("2 hour(s) ago"));

        // 'Now' in whatever timezone
        earlyTS = QDateTime::currentDateTime();
        laterTS = earlyTS;
        s = timeAgoInWords(earlyTS, laterTS );
        QCOMPARE(s, QLatin1String("now"));

        earlyTS = earlyTS.addSecs(-6);
        s = timeAgoInWords(earlyTS, laterTS );
        QCOMPARE(s, QLatin1String("Less than a minute ago"));
    }

    void testFsCasePreserving()
    {
        qputenv("OWNCLOUD_TEST_CASE_PRESERVING", "1");
        QVERIFY(fsCasePreserving());
        qputenv("OWNCLOUD_TEST_CASE_PRESERVING", "0");
        QVERIFY(! fsCasePreserving());
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
        qunsetenv("OWNCLOUD_TEST_CASE_PRESERVING");
        QVERIFY(isMac() || isWindows() ? fsCasePreserving() : ! fsCasePreserving());
#endif
    }

    void testFileNamesEqual()
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath("test"));
        if( !fsCasePreserving() ) {
        QVERIFY(dir2.mkpath("TEST"));
        }
        QVERIFY(dir2.mkpath("test/TESTI"));
        QVERIFY(dir2.mkpath("TESTI"));

        QString a = dir.path();
        QString b = dir.path();

        QVERIFY(fileNamesEqual(a, b));

        QVERIFY(fileNamesEqual(a+"/test", b+"/test")); // both exist
        QVERIFY(fileNamesEqual(a+"/test/TESTI", b+"/test/../test/TESTI")); // both exist

        qputenv("OWNCLOUD_TEST_CASE_PRESERVING", "1");
        QVERIFY(fileNamesEqual(a+"/test", b+"/TEST")); // both exist

        QVERIFY(!fileNamesEqual(a+"/test", b+"/test/TESTI")); // both are different

        dir.remove();
        qunsetenv("OWNCLOUD_TEST_CASE_PRESERVING");
#endif
    }


};

QTEST_GUILESS_MAIN(TestUtility)
#include "testutility.moc"
