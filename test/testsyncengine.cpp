/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"

using namespace OCC;

bool itemDidComplete(const QSignalSpy &spy, const QString &path)
{
    for(const QList<QVariant> &args : spy) {
        SyncFileItem item = args[0].value<SyncFileItem>();
        if (item.destination() == path)
            return true;
    }
    return false;
}

bool itemDidCompleteSuccessfully(const QSignalSpy &spy, const QString &path)
{
    for(const QList<QVariant> &args : spy) {
        SyncFileItem item = args[0].value<SyncFileItem>();
        if (item.destination() == path)
            return item._status == SyncFileItem::Success;
    }
    return false;
}

class TestSyncEngine : public QObject
{
    Q_OBJECT

private slots:
    void testFileDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
        fakeFolder.remoteModifier().insert("A/a0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testFileUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
        fakeFolder.localModifier().insert("A/a0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
        fakeFolder.remoteModifier().mkdir("Y");
        fakeFolder.remoteModifier().mkdir("Z");
        fakeFolder.remoteModifier().insert("Z/d0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Y"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z/d0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
        fakeFolder.localModifier().mkdir("Y");
        fakeFolder.localModifier().mkdir("Z");
        fakeFolder.localModifier().insert("Z/d0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Y"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z/d0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testLocalDelete() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
        fakeFolder.remoteModifier().remove("A/a1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteDelete() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
        fakeFolder.localModifier().remove("A/a1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testEmlLocalChecksum() {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.localModifier().insert("a1.eml", 64, 'A');
        fakeFolder.localModifier().insert("a2.eml", 64, 'A');
        fakeFolder.localModifier().insert("a3.eml", 64, 'A');
        // Upload and calculate the checksums
        // fakeFolder.syncOnce();
        fakeFolder.syncOnce();

        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItem &, const PropagatorJob &)));
        // Touch the file without changing the content, shouldn't upload
        fakeFolder.localModifier().setContents("a1.eml", 'A');
        // Change the content/size
        fakeFolder.localModifier().setContents("a2.eml", 'B');
        fakeFolder.localModifier().appendByte("a3.eml");
        fakeFolder.syncOnce();

        QVERIFY(!itemDidComplete(completeSpy, "a1.eml"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "a2.eml"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "a3.eml"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteChangeInMovedFolder() {
        // issue #5192
        FakeFolder fakeFolder{FileInfo{ QString(), {
            FileInfo { QStringLiteral("folder"), {
                FileInfo{ QStringLiteral("folderA"), { { QStringLiteral("file.txt"), 400 } } },
                QStringLiteral("folderB")
            }
        }}}};

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Edit a file in a moved directory.
        fakeFolder.remoteModifier().setContents("folder/folderA/file.txt", 'a');
        fakeFolder.remoteModifier().rename("folder/folderA", "folder/folderB/folderA");
        fakeFolder.syncOnce();
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto oldState = fakeFolder.currentLocalState();
        QVERIFY(oldState.find(PathComponents("folder/folderB/folderA/file.txt")));
        QVERIFY(!oldState.find(PathComponents("folder/folderA/file.txt")));

        // This sync should not remove the file
        fakeFolder.syncOnce();
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentLocalState(), oldState);

    }
};

QTEST_GUILESS_MAIN(TestSyncEngine)
#include "testsyncengine.moc"
