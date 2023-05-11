/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

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

    SyncFileItem createItem( const QString& file ) {
        SyncFileItem i;
        i._file = file;
        return i;
    }

    void testComparator_data() {
        QTest::addColumn<SyncFileItem>("a");
        QTest::addColumn<SyncFileItem>("b");
        QTest::addColumn<SyncFileItem>("c");

        QTest::newRow("a1") << createItem(QStringLiteral("client")) << createItem(QStringLiteral("client/build")) << createItem(QStringLiteral("client-build"));
        QTest::newRow("a2") << createItem(QStringLiteral("test/t1")) << createItem(QStringLiteral("test/t2")) << createItem(QStringLiteral("test/t3"));
        QTest::newRow("a3") << createItem(QStringLiteral("ABCD")) << createItem(QStringLiteral("abcd")) << createItem(QStringLiteral("zzzz"));

        SyncFileItem movedItem1;
        movedItem1._file = QStringLiteral("folder/source/file.f");
        movedItem1._renameTarget = QStringLiteral("folder/destination/file.f");
        movedItem1._instruction = CSYNC_INSTRUCTION_RENAME;

        QTest::newRow("move1") << createItem(QStringLiteral("folder/destination")) << movedItem1 << createItem(QStringLiteral("folder/destination-2"));
        QTest::newRow("move2") << createItem(QStringLiteral("folder/destination/1")) << movedItem1 << createItem(QStringLiteral("folder/source"));
        QTest::newRow("move3") << createItem(QStringLiteral("abc")) << movedItem1 << createItem(QStringLiteral("ijk"));
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

        QVERIFY(!(a < a));
        QVERIFY(!(b < b));
        QVERIFY(!(c < c));
    }
};

QTEST_APPLESS_MAIN(TestSyncFileItem)
#include "testsyncfileitem.moc"
