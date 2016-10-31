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
        fakeFolder.localModifier().insert("A/a0", size);

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


        // Add a fake file to make sure it gets deleted
        fakeFolder.uploadState().children.first().insert("10000", size);
        QVERIFY(fakeFolder.syncOnce());



        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.uploadState().children.count(), 1); // The same chunk id was re-used
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->size, size);
    }
};

QTEST_GUILESS_MAIN(TestChunkingNG)
#include "testchunkingng.moc"
