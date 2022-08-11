/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "testutils/syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

class TestSyncDelete : public QObject
{
    Q_OBJECT

private slots:

    void testDeleteDirectoryWithNewFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        // Remove a directory on the server with new files on the client
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.localModifier().insert(QStringLiteral("A/hello.txt"));

        // Symetry
        fakeFolder.localModifier().remove(QStringLiteral("B"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B/hello.txt"));

        QVERIFY(fakeFolder.syncOnce());

        // A/a1 must be gone because the directory was removed on the server, but hello.txt must be there
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(fakeFolder.currentRemoteState().find("A/hello.txt"));

        // Symetry
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b1"));
        QVERIFY(fakeFolder.currentRemoteState().find("B/hello.txt"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void issue1329()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        fakeFolder.localModifier().remove(QStringLiteral("B"));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Add a directory that was just removed in the previous sync:
        fakeFolder.localModifier().mkdir(QStringLiteral("B"));
        fakeFolder.localModifier().insert(QStringLiteral("B/b1"));
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("B/b1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestSyncDelete)
#include "testsyncdelete.moc"
