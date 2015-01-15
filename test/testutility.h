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
        QCOMPARE(octetsToString(1024) , QString("1 kB"));
        QCOMPARE(octetsToString(1110) , QString("1.1 kB"));

        QCOMPARE(octetsToString(9110) , QString("8.9 kB"));
        QCOMPARE(octetsToString(9910) , QString("9.7 kB"));
        QCOMPARE(octetsToString(9999) , QString("9.8 kB"));
        QCOMPARE(octetsToString(10240) , QString("10 kB"));

        QCOMPARE(octetsToString(123456) , QString("121 kB"));
        QCOMPARE(octetsToString(1234567) , QString("1.2 MB"));
        QCOMPARE(octetsToString(12345678) , QString("12 MB"));
        QCOMPARE(octetsToString(123456789) , QString("118 MB"));
        QCOMPARE(octetsToString(1000LL*1000*1000 * 5) , QString("4.7 GB"));
        QCOMPARE(octetsToString(1024LL*1024*1024 * 5) , QString("5 GB"));

        QCOMPARE(octetsToString(1), QString("1 B"));
        QCOMPARE(octetsToString(2), QString("2 B"));
        QCOMPARE(octetsToString(1024), QString("1 kB"));
        QCOMPARE(octetsToString(1024*1024), QString("1 MB"));
        QCOMPARE(octetsToString(1024LL*1024*1024), QString("1 GB"));
        QCOMPARE(octetsToString(1024LL*1024*1024*1024), QString("1 TB"));
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
};

#endif
