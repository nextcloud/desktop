/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#ifndef MIRALL_TESTSYNCFILEITEM_H
#define MIRALL_TESTSYNCFILEITEM_H

#include <QtTest>

#include "syncfileitem.h"

using namespace OCC;

class TestSyncFileItem : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
    }

    void cleanupTestCase() {
    }

    void testComparator_data() {
        QTest::addColumn<SyncFileItem>("a");
        QTest::addColumn<SyncFileItem>("b");
        QTest::addColumn<SyncFileItem>("c");

        auto i = [](const QString &file) {
            SyncFileItem itm;
            itm._file = file;
            return itm;
        };

        QTest::newRow("a1") << i("client") << i("client/build") << i("client-build") ;
        QTest::newRow("a2") << i("test/t1") << i("test/t2") << i("test/t3") ;
        QTest::newRow("a3") << i("ABCD") << i("abcd") << i("zzzz");

        SyncFileItem movedItem1;
        movedItem1._file = "folder/source/file.f";
        movedItem1._renameTarget = "folder/destination/file.f";
        movedItem1._instruction = CSYNC_INSTRUCTION_RENAME;

        QTest::newRow("move1") << i("folder/destination") << movedItem1 << i("folder/destination-2");
        QTest::newRow("move2") << i("folder/destination/1") << movedItem1 << i("folder/source");
        QTest::newRow("move3") << i("abc") << movedItem1 << i("ijk");
    }

    void testComparator() {
        QFETCH( SyncFileItem , a );
        QFETCH( SyncFileItem , b );
        QFETCH( SyncFileItem , c );

        QVERIFY(a < b);
        QVERIFY(b < c);
        QVERIFY(a < c);

        QVERIFY(!(b < a));
        QVERIFY(!(c < b));
        QVERIFY(!(c < a));
    }
};

#endif
