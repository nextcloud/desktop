/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTUTILITY_H
#define MIRALL_TESTUTILITY_H

#include <QtTest>

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
        QCOMPARE(octetsToString(1000) , QString("1,000 B"));
        QCOMPARE(octetsToString(1010) , QString("1,010 B"));
        QCOMPARE(octetsToString(1024) , QString("1 KiB"));
        QCOMPARE(octetsToString(1110) , QString("1.1 KiB"));

        QCOMPARE(octetsToString(9110) , QString("8.9 KiB"));
        QCOMPARE(octetsToString(9910) , QString("9.7 KiB"));
        QCOMPARE(octetsToString(9999) , QString("9.8 KiB"));
        QCOMPARE(octetsToString(10240) , QString("10 KiB"));

        QCOMPARE(octetsToString(123456) , QString("121 KiB"));
        QCOMPARE(octetsToString(1234567) , QString("1.2 MiB"));
        QCOMPARE(octetsToString(12345678) , QString("12 MiB"));
        QCOMPARE(octetsToString(123456789) , QString("118 MiB"));
        QCOMPARE(octetsToString(1000LL*1000*1000 * 5) , QString("4.7 GiB"));
        QCOMPARE(octetsToString(1024LL*1024*1024 * 5) , QString("5 GiB"));

        QCOMPARE(octetsToString(1), QString("1 B"));
        QCOMPARE(octetsToString(2), QString("2 B"));
        QCOMPARE(octetsToString(1024), QString("1 KiB"));
        QCOMPARE(octetsToString(1024*1024), QString("1 MiB"));
        QCOMPARE(octetsToString(1024LL*1024*1024), QString("1 GiB"));
        QCOMPARE(octetsToString(1024LL*1024*1024*1024), QString("1 TiB"));
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

    void testToCSyncScheme()
    {
        QVERIFY(toCSyncScheme("http://example.com/owncloud/") ==
                              "owncloud://example.com/owncloud/");
        QVERIFY(toCSyncScheme("https://example.com/owncloud/") ==
                              "ownclouds://example.com/owncloud/");
    }

    void testDurationToDescriptiveString()
    {
        QLocale::setDefault(QLocale("C"));
        //NOTE: in order for the plural to work we would need to load the english translation

        quint64 sec = 1000;
        quint64 hour = 3600 * sec;

        QDateTime current = QDateTime::currentDateTime();

        QCOMPARE(durationToDescriptiveString(0), QString("0 seconds") );
        QCOMPARE(durationToDescriptiveString(5), QString("0 seconds") );
        QCOMPARE(durationToDescriptiveString(1000), QString("1 second(s)") );
        QCOMPARE(durationToDescriptiveString(1005), QString("1 second(s)") );
        QCOMPARE(durationToDescriptiveString(56123), QString("56 second(s)") );
        QCOMPARE(durationToDescriptiveString(90*sec), QString("1 minute(s) 30 second(s)") );
        QCOMPARE(durationToDescriptiveString(3*hour), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString(3*hour + 20*sec), QString("3 hour(s)") );
        QCOMPARE(durationToDescriptiveString(3*hour + 70*sec), QString("3 hour(s) 1 minute(s)") );
        QCOMPARE(durationToDescriptiveString(3*hour + 100*sec), QString("3 hour(s) 2 minute(s)") );
        QCOMPARE(durationToDescriptiveString(current.msecsTo(current.addYears(4).addMonths(5).addDays(2).addSecs(23*60*60))),
                 QString("4 year(s) 5 month(s)") );
        QCOMPARE(durationToDescriptiveString(current.msecsTo(current.addDays(2).addSecs(23*60*60))),
                 QString("2 day(s) 23 hour(s)") );


    }
};

#endif
