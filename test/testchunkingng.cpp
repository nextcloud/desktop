/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

/* Upload a 1/3 of a file of given size.
 * fakeFolder needs to be synchronized */
static void partialUpload(FakeFolder &fakeFolder, const QString &name, int size)
{
    QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    QCOMPARE(fakeFolder.uploadState().children.count(), 0); // The state should be clean

    fakeFolder.localModifier().insert(name, size);
    // Abort when the upload is at 1/3
    int sizeWhenAbort = -1;
    auto con = QObject::connect(&fakeFolder.syncEngine(),  &SyncEngine::transmissionProgress,
                                    [&](const ProgressInfo &progress) {
                if (progress.completedSize() > (progress.totalSize() /3 )) {
                    sizeWhenAbort = progress.completedSize();
                    fakeFolder.syncEngine().abort();
                }
    });

    QVERIFY(!fakeFolder.syncOnce()); // there should have been an error
    QObject::disconnect(con);
    QVERIFY(sizeWhenAbort > 0);
    QVERIFY(sizeWhenAbort < size);

    QCOMPARE(fakeFolder.uploadState().children.count(), 1); // the transfer was done with chunking
    auto upStateChildren = fakeFolder.uploadState().children.first().children;
    QCOMPARE(sizeWhenAbort, std::accumulate(upStateChildren.cbegin(), upStateChildren.cend(), 0,
                                            [](int s, const FileInfo &i) { return s + i.size; }));
}


class TestChunkingNG : public QObject
{
    Q_OBJECT

private slots:

    void testFileUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        const int size = 300 * 1000 * 1000; // 300 MB
        fakeFolder.localModifier().insert("A/a0", size);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.uploadState().children.count(), 1); // the transfer was done with chunking
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->size, size);

        // Check that another upload of the same file also work.
        fakeFolder.localModifier().appendByte("A/a0");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.uploadState().children.count(), 2); // the transfer was done with chunking
    }


    void testResume () {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        const int size = 300 * 1000 * 1000; // 300 MB
        partialUpload(fakeFolder, "A/a0", size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;

        // Add a fake file to make sure it gets deleted
        fakeFolder.uploadState().children.first().insert("10000", size);
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->size, size);
        // The same chunk id was re-used
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QCOMPARE(fakeFolder.uploadState().children.first().name, chunkingId);
    }

    // We modify the file locally after it has been partially uploaded
    void testRemoveStale1() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        const int size = 300 * 1000 * 1000; // 300 MB
        partialUpload(fakeFolder, "A/a0", size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;


        fakeFolder.localModifier().setContents("A/a0", 'B');
        fakeFolder.localModifier().appendByte("A/a0");

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->size, size + 1);
        // A different chunk id was used, and the previous one is removed
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QVERIFY(fakeFolder.uploadState().children.first().name != chunkingId);
    }

    // We remove the file locally after it has been partially uploaded
    void testRemoveStale2() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        const int size = 300 * 1000 * 1000; // 300 MB
        partialUpload(fakeFolder, "A/a0", size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);

        fakeFolder.localModifier().remove("A/a0");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.uploadState().children.count(), 0);
    }


    void testCreateConflictWhileSyncing() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        const int size = 150 * 1000 * 1000; // 150 MB

        // Put a file on the server and download it.
        fakeFolder.remoteModifier().insert("A/a0", size);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Modify the file localy and start the upload
        fakeFolder.localModifier().setContents("A/a0", 'B');
        fakeFolder.localModifier().appendByte("A/a0");

        // But in the middle of the sync, modify the file on the server
        QMetaObject::Connection con = QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::transmissionProgress,
                                    [&](const ProgressInfo &progress) {
                if (progress.completedSize() > (progress.totalSize() / 2 )) {
                    fakeFolder.remoteModifier().setContents("A/a0", 'C');
                    QObject::disconnect(con);
                }
        });

        QVERIFY(!fakeFolder.syncOnce());
        // There was a precondition failed error, this means wen need to sync again
        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), ImmediateFollowUp);

        QCOMPARE(fakeFolder.uploadState().children.count(), 1); // We did not clean the chunks at this point

        // Now we will download the server file and create a conflict
        QVERIFY(fakeFolder.syncOnce());
        auto localState = fakeFolder.currentLocalState();

        // A0 is the one from the server
        QCOMPARE(localState.find("A/a0")->size, size);
        QCOMPARE(localState.find("A/a0")->contentChar, 'C');

        // There is a conflict file with our version
        auto &stateAChildren = localState.find("A")->children;
        auto it = std::find_if(stateAChildren.cbegin(), stateAChildren.cend(), [&](const FileInfo &fi) {
            return fi.name.startsWith("a0_conflict");
        });
        QVERIFY(it != stateAChildren.cend());
        QCOMPARE(it->contentChar, 'B');
        QCOMPARE(it->size, size+1);

        // Remove the conflict file so the comparison works!
        fakeFolder.localModifier().remove("A/" + it->name);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QCOMPARE(fakeFolder.uploadState().children.count(), 0); // The last sync cleaned the chunks
    }

};

QTEST_GUILESS_MAIN(TestChunkingNG)
#include "testchunkingng.moc"
