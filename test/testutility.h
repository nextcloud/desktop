/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTUTILITY_H
#define MIRALL_TESTUTILITY_H

#include <QtTest>

#include "mirall/utility.h"

using namespace Mirall::Utility;

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
        QVERIFY(octetsToString(1) == "1 byte");
        QVERIFY(octetsToString(2) == "2 bytes");
        QVERIFY(octetsToString(1024) == "1 KB");
        QVERIFY(octetsToString(1024*1024) == "1 MB");
        QVERIFY(octetsToString(1024LL*1024*1024) == "1 GB");
        QVERIFY(octetsToString(1024LL*1024*1024*1024) == "1 TB");
    }

    void testLaunchOnStartup()
    {
 	const QString appName = "testLaunchOnStartup";
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
