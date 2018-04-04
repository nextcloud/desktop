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
        const auto &chunkMap = fakeFolder.uploadState().children.first().children;
        quint64 uploadedSize = std::accumulate(chunkMap.begin(), chunkMap.end(), 0LL, [](quint64 s, const FileInfo &f) { return s + f.size; });
        QVERIFY(uploadedSize > 50 * 1000 * 1000); // at least 50 MB

        // Add a fake file to make sure it gets deleted
        fakeFolder.uploadState().children.first().insert("10000", size);

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                // Test that we properly resuming and are not sending past data again.
                Q_ASSERT(request.rawHeader("OC-Chunk-Offset").toULongLong() >= uploadedSize);
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->size, size);
        // The same chunk id was re-used
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QCOMPARE(fakeFolder.uploadState().children.first().name, chunkingId);
    }

    // Check what happens when we abort during the final MOVE and the
    // the final MOVE takes longer than the abort-delay
    void testLateAbortHard()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "chunking", "1.0" } } }, { "checksums", QVariantMap{ { "supportedTypes", QStringList() << "SHA1" } } } });
        const int size = 150 * 1000 * 1000;

        // Make the MOVE never reply, but trigger a client-abort and apply the change remotely
        auto parent = new QObject;
        QByteArray moveChecksumHeader;
        int nGET = 0;
        int responseDelay = 10000; // bigger than abort-wait timeout
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                QTimer::singleShot(50, parent, [&]() { fakeFolder.syncEngine().abort(); });
                moveChecksumHeader = request.rawHeader("OC-Checksum");
                return new DelayedReply<FakeChunkMoveReply>(responseDelay, fakeFolder.uploadState(), fakeFolder.remoteModifier(), op, request, parent);
            } else if (op == QNetworkAccessManager::GetOperation) {
                nGET++;
            }
            return nullptr;
        });


        // Test 1: NEW file aborted
        fakeFolder.localModifier().insert("A/a0", size);
        QVERIFY(!fakeFolder.syncOnce()); // error: abort!

        // Now the next sync gets a NEW/NEW conflict and since there's no checksum
        // it just becomes a UPDATE_METADATA
        auto checkEtagUpdated = [&](SyncFileItemVector &items) {
            QCOMPARE(items.size(), 1);
            QCOMPARE(items[0]->_file, QLatin1String("A"));
            SyncJournalFileRecord record;
            QVERIFY(fakeFolder.syncJournal().getFileRecord(QByteArray("A/a0"), &record));
            QCOMPARE(record._etag, fakeFolder.remoteModifier().find("A/a0")->etag.toUtf8());
        };
        auto connection = connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, checkEtagUpdated);
        QVERIFY(fakeFolder.syncOnce());
        disconnect(connection);
        QCOMPARE(nGET, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        // Test 2: modified file upload aborted
        fakeFolder.localModifier().appendByte("A/a0");
        QVERIFY(!fakeFolder.syncOnce()); // error: abort!

        // An EVAL/EVAL conflict is also UPDATE_METADATA when there's no checksums
        connection = connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, checkEtagUpdated);
        QVERIFY(fakeFolder.syncOnce());
        disconnect(connection);
        QCOMPARE(nGET, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        // Test 3: modified file upload aborted, with good checksums
        fakeFolder.localModifier().appendByte("A/a0");
        QVERIFY(!fakeFolder.syncOnce()); // error: abort!

        // Set the remote checksum -- the test setup doesn't do it automatically
        QVERIFY(!moveChecksumHeader.isEmpty());
        fakeFolder.remoteModifier().find("A/a0")->checksums = moveChecksumHeader;

        QVERIFY(fakeFolder.syncOnce());
        disconnect(connection);
        QCOMPARE(nGET, 0); // no new download, just a metadata update!
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        // Test 4: New file, that gets deleted locally before the next sync
        fakeFolder.localModifier().insert("A/a3", size);
        QVERIFY(!fakeFolder.syncOnce()); // error: abort!
        fakeFolder.localModifier().remove("A/a3");

        // bug: in this case we must expect a re-download of A/A3
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 1);
        QVERIFY(fakeFolder.currentLocalState().find("A/a3"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Check what happens when we abort during the final MOVE and the
    // the final MOVE is short enough for the abort-delay to help
    void testLateAbortRecoverable()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "chunking", "1.0" } } }, { "checksums", QVariantMap{ { "supportedTypes", QStringList() << "SHA1" } } } });
        const int size = 150 * 1000 * 1000;

        // Make the MOVE never reply, but trigger a client-abort and apply the change remotely
        auto parent = new QObject;
        QByteArray moveChecksumHeader;
        int nGET = 0;
        int responseDelay = 2000; // smaller than abort-wait timeout
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                QTimer::singleShot(50, parent, [&]() { fakeFolder.syncEngine().abort(); });
                moveChecksumHeader = request.rawHeader("OC-Checksum");
                return new DelayedReply<FakeChunkMoveReply>(responseDelay, fakeFolder.uploadState(), fakeFolder.remoteModifier(), op, request, parent);
            } else if (op == QNetworkAccessManager::GetOperation) {
                nGET++;
            }
            return nullptr;
        });


        // Test 1: NEW file aborted
        fakeFolder.localModifier().insert("A/a0", size);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Test 2: modified file upload aborted
        fakeFolder.localModifier().appendByte("A/a0");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
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
            return fi.name.startsWith("a0 (conflicted copy");
        });
        QVERIFY(it != stateAChildren.cend());
        QCOMPARE(it->contentChar, 'B');
        QCOMPARE(it->size, size+1);

        // Remove the conflict file so the comparison works!
        fakeFolder.localModifier().remove("A/" + it->name);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QCOMPARE(fakeFolder.uploadState().children.count(), 0); // The last sync cleaned the chunks
    }

    void testModifyLocalFileWhileUploading() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        const int size = 150 * 1000 * 1000; // 150 MB

        fakeFolder.localModifier().insert("A/a0", size);

        // middle of the sync, modify the file
        QMetaObject::Connection con = QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::transmissionProgress,
                                    [&](const ProgressInfo &progress) {
                if (progress.completedSize() > (progress.totalSize() / 2 )) {
                    fakeFolder.localModifier().setContents("A/a0", 'B');
                    fakeFolder.localModifier().appendByte("A/a0");
                    QObject::disconnect(con);
                }
        });

        QVERIFY(!fakeFolder.syncOnce());

        // There should be a followup sync
        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), ImmediateFollowUp);

        QCOMPARE(fakeFolder.uploadState().children.count(), 1); // We did not clean the chunks at this point
        auto chunkingId = fakeFolder.uploadState().children.first().name;

        // Now we make a new sync which should upload the file for good.
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->size, size+1);

        // A different chunk id was used, and the previous one is removed
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QVERIFY(fakeFolder.uploadState().children.first().name != chunkingId);
    }


    void testResumeServerDeletedChunks() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        const int size = 300 * 1000 * 1000; // 300 MB
        partialUpload(fakeFolder, "A/a0", size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;

        // Delete the chunks on the server
        fakeFolder.uploadState().children.clear();
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->size, size);

        // A different chunk id was used
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QVERIFY(fakeFolder.uploadState().children.first().name != chunkingId);
    }

    // Check what happens when the connection is dropped on the PUT (non-chunking) or MOVE (chunking)
    // for on the issue #5106
    void connectionDroppedBeforeEtagRecieved_data()
    {
        QTest::addColumn<bool>("chunking");
        QTest::newRow("big file") << true;
        QTest::newRow("small file") << false;
    }
    void connectionDroppedBeforeEtagRecieved()
    {
        QFETCH(bool, chunking);
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "chunking", "1.0" } } }, { "checksums", QVariantMap{ { "supportedTypes", QStringList() << "SHA1" } } } });
        const int size = chunking ? 150 * 1000 * 1000 : 300;

        // Make the MOVE never reply, but trigger a client-abort and apply the change remotely
        QByteArray checksumHeader;
        int nGET = 0;
        QScopedValueRollback<int> setHttpTimeout(AbstractNetworkJob::httpTimeout, 1);
        int responseDelay = AbstractNetworkJob::httpTimeout * 1000 * 1000; // much bigger than http timeout (so a timeout will occur)
        // This will perform the operation on the server, but the reply will not come to the client
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            if (!chunking) {
                Q_ASSERT(!request.url().path().contains("/uploads/")
                    && "Should not touch uploads endpoint when not chunking");
            }
            if (!chunking && op == QNetworkAccessManager::PutOperation) {
                checksumHeader = request.rawHeader("OC-Checksum");
                return new DelayedReply<FakePutReply>(responseDelay, fakeFolder.remoteModifier(), op, request, outgoingData->readAll(), &fakeFolder.syncEngine());
            } else if (chunking && request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                checksumHeader = request.rawHeader("OC-Checksum");
                return new DelayedReply<FakeChunkMoveReply>(responseDelay, fakeFolder.uploadState(), fakeFolder.remoteModifier(), op, request, &fakeFolder.syncEngine());
            } else if (op == QNetworkAccessManager::GetOperation) {
                nGET++;
            }
            return nullptr;
        });


        // Test 1: a NEW file
        fakeFolder.localModifier().insert("A/a0", size);
        QVERIFY(!fakeFolder.syncOnce()); // timeout!
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState()); // but the upload succeeded
        QVERIFY(!checksumHeader.isEmpty());
        fakeFolder.remoteModifier().find("A/a0")->checksums = checksumHeader; // The test system don't do that automatically
        // Should be resolved properly
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Test 2: Modify the file further
        fakeFolder.localModifier().appendByte("A/a0");
        QVERIFY(!fakeFolder.syncOnce()); // timeout!
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState()); // but the upload succeeded
        fakeFolder.remoteModifier().find("A/a0")->checksums = checksumHeader;
        // modify again, should not cause conflict
        fakeFolder.localModifier().appendByte("A/a0");
        QVERIFY(!fakeFolder.syncOnce()); // now it's trying to upload the modified file
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        fakeFolder.remoteModifier().find("A/a0")->checksums = checksumHeader;
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);
    }
};

QTEST_GUILESS_MAIN(TestChunkingNG)
#include "testchunkingng.moc"
