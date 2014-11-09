/*
   This software is in the public domain, furnished "as is", without technical
   support, and with no warranty, express or implied, as to its usefulness for
   any purpose.
*/

#ifndef MIRALL_TESTUTILITY_H
#define MIRALL_TESTUTILITY_H

#include <QtTest>

#include "updater/updater.h"
#include "updater/ocupdater.h"

using namespace OCC;

class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
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

#endif
