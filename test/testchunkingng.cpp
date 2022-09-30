/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */
#include "testutils/syncenginetestutils.h"
#include "testutils/testutils.h"

#include "common/filesystembase.h"
#include "libsync/syncengine.h"

#include <QtTest>

using namespace std::chrono_literals;
using namespace OCC::FileSystem::SizeLiterals;
using namespace OCC;

namespace {

/* Upload a 1/3 of a file of given size.
 * fakeFolder needs to be synchronized */
void partialUpload(FakeFolder &fakeFolder, const QString &name, qint64 size)
{
    QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    QCOMPARE(fakeFolder.uploadState().children.count(), 0); // The state should be clean

    fakeFolder.localModifier().insert(name, size);
    QVERIFY(fakeFolder.applyLocalModificationsWithoutSync()); // non-vfs test, so force modifications to disk immediately

    // Abort when the upload is at 1/3
    qint64 sizeWhenAbort = -1;
    auto con = QObject::connect(&fakeFolder.syncEngine(),  &SyncEngine::transmissionProgress,
                                    [&](const ProgressInfo &progress) {
                if (progress.completedSize() > (progress.totalSize() /3 )) {
                    sizeWhenAbort = progress.completedSize();
                    fakeFolder.syncEngine().abort();
                }
    });

    QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // there should have been an error
    QObject::disconnect(con);
    QVERIFY(sizeWhenAbort > 0);
    QVERIFY(sizeWhenAbort < size);

    QCOMPARE(fakeFolder.uploadState().children.count(), 1); // the transfer was done with chunking
    auto upStateChildren = fakeFolder.uploadState().children.first().children;
    QCOMPARE(sizeWhenAbort, std::accumulate(upStateChildren.cbegin(), upStateChildren.cend(), 0, [](int s, const FileInfo &i) { return s + i.contentSize; }));
}

// Reduce max chunk size a bit so we get more chunks
void setChunkSize(SyncEngine &engine, qint64 size)
{
    SyncOptions options = engine.syncOptions();
    options._maxChunkSize = size;
    options._initialChunkSize = size;
    options._minChunkSize = size;
    engine.setSyncOptions(options);
}

} // anonymous namespace

class TestChunkingNG : public QObject
{
    Q_OBJECT

private slots:

    void testFileUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);
        const int size = 10 * 1000 * 1000; // 10 MB

        fakeFolder.localModifier().insert(QStringLiteral("A/a0"), size);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.uploadState().children.count(), 1); // the transfer was done with chunking
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size);

        // Check that another upload of the same file also work.
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.uploadState().children.count(), 2); // the transfer was done with chunking
    }

    // Test resuming when there's a confusing chunk added
    void testResume1() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 10 * 1000 * 1000; // 10 MB
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;
        const auto &chunkMap = fakeFolder.uploadState().children.first().children;
        qint64 uploadedSize = std::accumulate(chunkMap.begin(), chunkMap.end(), 0LL, [](qint64 s, const FileInfo &f) { return s + f.contentSize; });
        QVERIFY(uploadedSize > 2 * 1000 * 1000); // at least 2 MB

        // Add a fake chunk to make sure it gets deleted
        fakeFolder.uploadState().children.first().insert(QStringLiteral("10000"), size);

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::DeleteOperation) {
                Q_ASSERT(request.url().path().endsWith("/10000"));
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size);
        // The same chunk id was re-used
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QCOMPARE(fakeFolder.uploadState().children.first().name, chunkingId);
    }

    // Test resuming when one of the uploaded chunks got removed
    void testResume2() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);
        const int size = 150 * 1000 * 1000; // 30 MB
        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;
        const auto &chunkMap = fakeFolder.uploadState().children.first().children;
        qint64 uploadedSize = std::accumulate(chunkMap.begin(), chunkMap.end(), 0LL, [](qint64 s, const FileInfo &f) { return s + f.contentSize; });
        QVERIFY(uploadedSize > 2 * 1000 * 1000); // at least 50 MB
        QVERIFY(chunkMap.size() >= 3); // at least three chunks

        QStringList chunksToDelete;

        // Remove the second chunk, so all further chunks will be deleted and resent
        auto firstChunk = chunkMap.first();
        auto secondChunk = *(chunkMap.begin() + 1);
        for (auto it = chunkMap.cbegin() + 2; it != chunkMap.cend(); ++it) {
            chunksToDelete.append(it.key());
        }
        fakeFolder.uploadState().children.first().remove(secondChunk.name);

        QStringList deletedPaths;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::DeleteOperation) {
                deletedPaths.append(request.url().path());
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        for (const auto& toDelete : chunksToDelete) {
            bool wasDeleted = false;
            for (const auto& deleted : deletedPaths) {
                if (deleted.mid(deleted.lastIndexOf('/') + 1) == toDelete) {
                    wasDeleted = true;
                    break;
                }
            }
            QVERIFY(wasDeleted);
        }

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size);
        // The same chunk id was re-used
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QCOMPARE(fakeFolder.uploadState().children.first().name, chunkingId);
    }

    // Test resuming when all chunks are already present
    void testResume3() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 30 * 1000 * 1000; // 30 MB
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;
        const auto &chunkMap = fakeFolder.uploadState().children.first().children;
        qint64 uploadedSize = std::accumulate(chunkMap.begin(), chunkMap.end(), 0LL, [](qint64 s, const FileInfo &f) { return s + f.contentSize; });
        QVERIFY(uploadedSize > 5 * 1000 * 1000); // at least 5 MB

        // Add a chunk that makes the file completely uploaded
        fakeFolder.uploadState().children.first().insert(
            QString::number(uploadedSize).rightJustified(16, '0'), size - uploadedSize);

        bool sawPut = false;
        bool sawDelete = false;
        bool sawMove = false;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                sawPut = true;
            } else if (op == QNetworkAccessManager::DeleteOperation) {
                sawDelete = true;
            } else if (request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                sawMove = true;
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(sawMove);
        QVERIFY(!sawPut);
        QVERIFY(!sawDelete);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size);
        // The same chunk id was re-used
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QCOMPARE(fakeFolder.uploadState().children.first().name, chunkingId);
    }

    // Test resuming (or rather not resuming!) for the error case of the sum of
    // chunk sizes being larger than the file size
    void testResume4() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 30 * 1000 * 1000; // 300 MB
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;
        const auto &chunkMap = fakeFolder.uploadState().children.first().children;
        qint64 uploadedSize = std::accumulate(chunkMap.begin(), chunkMap.end(), 0LL, [](qint64 s, const FileInfo &f) { return s + f.contentSize; });
        QVERIFY(uploadedSize > 5 * 1000 * 1000); // at least 5 MB

        // Add a chunk that makes the file more than completely uploaded
        fakeFolder.uploadState().children.first().insert(
            QString::number(uploadedSize).rightJustified(16, '0'), size - uploadedSize + 100);

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
    }

    // Check what happens when we abort during the final MOVE and the
    // the final MOVE takes longer than the abort-delay
    void testLateAbortHard()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        const int size = 15 * 1000 * 1000;
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        // Make the MOVE never reply, but trigger a client-abort and apply the change remotely
        QObject parent;
        QByteArray moveChecksumHeader;
        int nGET = 0;
        const auto responseDelay = 24h; // bigger than abort-wait timeout
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                QTimer::singleShot(50ms, &parent, [&]() { fakeFolder.syncEngine().abort(); });
                moveChecksumHeader = request.rawHeader("OC-Checksum");
                return new DelayedReply<FakeChunkMoveReply>(responseDelay, fakeFolder.uploadState(), fakeFolder.remoteModifier(), op, request, &parent);
            } else if (op == QNetworkAccessManager::GetOperation) {
                nGET++;
            }
            return nullptr;
        });


        // Test 1: NEW file aborted
        fakeFolder.localModifier().insert(QStringLiteral("A/a0"), size);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // error: abort!

        // Now the next sync gets a NEW/NEW conflict and since there's no checksum
        // it just becomes a UPDATE_METADATA
        auto checkIsUpdateMetaData = [&](const SyncFileItemSet &items) {
            QCOMPARE(items.size(), 2);
            auto it = items.cbegin();
            QCOMPARE(it->get()->_file, QLatin1String("A"));
            QCOMPARE(it->get()->_instruction, CSYNC_INSTRUCTION_UPDATE_METADATA);
            it++;
            QCOMPARE(it->get()->_file, QLatin1String("A/a0"));
            QCOMPARE(it->get()->_instruction, CSYNC_INSTRUCTION_UPDATE_METADATA);
            QCOMPARE(it->get()->_etag, fakeFolder.remoteModifier().find("A/a0")->etag);
        };
        auto checkEtagUpdated = [&](const SyncFileItemPtr &item) {
            if (item->_file == QLatin1String("A/a0")) {
                SyncJournalFileRecord record;
                QVERIFY(fakeFolder.syncJournal().getFileRecord(QByteArray("A/a0"), &record));
                QCOMPARE(record._etag, fakeFolder.remoteModifier().find("A/a0")->etag);
                QCOMPARE(item->_etag, fakeFolder.remoteModifier().find("A/a0")->etag);
            }
        };
        auto connection1 = connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, checkIsUpdateMetaData);
        auto connection2 = connect(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted, checkEtagUpdated);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        disconnect(connection1);
        disconnect(connection2);
        QCOMPARE(nGET, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        // Test 2: modified file upload aborted
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"));
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // error: abort!

        // An EVAL/EVAL conflict is also UPDATE_METADATA when there's no checksums
        connection1 = connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, checkIsUpdateMetaData);
        connection2 = connect(&fakeFolder.syncEngine(), &SyncEngine::itemCompleted, checkEtagUpdated);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        disconnect(connection1);
        disconnect(connection2);
        QCOMPARE(nGET, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        // Test 3: modified file upload aborted, with good checksums
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"));
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // error: abort!

        // Set the remote checksum -- the test setup doesn't do it automatically
        QVERIFY(!moveChecksumHeader.isEmpty());
        fakeFolder.remoteModifier().find("A/a0")->checksums = moveChecksumHeader;

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(nGET, 0); // no new download, just a metadata update!
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());


        // Test 4: New file, that gets deleted locally before the next sync
        fakeFolder.localModifier().insert(QStringLiteral("A/a3"), size);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // error: abort!
        fakeFolder.localModifier().remove(QStringLiteral("A/a3"));

        // bug: in this case we must expect a re-download of A/A3
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(nGET, 1);
        QVERIFY(fakeFolder.currentLocalState().find("A/a3"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Check what happens when we abort during the final MOVE and the
    // the final MOVE is short enough for the abort-delay to help
    void testLateAbortRecoverable()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        const int size = 15 * 1000 * 1000;
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        // Make the MOVE never reply, but trigger a client-abort and apply the change remotely
        QObject parent;
        const auto responseDelay = 200ms; // smaller than abort-wait timeout
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (request.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE") {
                QTimer::singleShot(50ms, &parent, [&]() { fakeFolder.syncEngine().abort(); });
                return new DelayedReply<FakeChunkMoveReply>(responseDelay, fakeFolder.uploadState(), fakeFolder.remoteModifier(), op, request, &parent);
            }
            return nullptr;
        });

        // Test 1: NEW file aborted
        fakeFolder.localModifier().insert(QStringLiteral("A/a0"), size);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Test 2: modified file upload aborted
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // We modify the file locally after it has been partially uploaded
    void testRemoveStale1() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 10 * 1000 * 1000; // 10 MB
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;

        const auto a0size = fakeFolder.currentLocalState().find("A/a0")->contentSize;
        fakeFolder.localModifier().setContents(QStringLiteral("A/a0"), a0size, 'B');
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"), 'B');

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size + 1);
        // A different chunk id was used, and the previous one is removed
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QVERIFY(fakeFolder.uploadState().children.first().name != chunkingId);
    }

    // We remove the file locally after it has been partially uploaded
    void testRemoveStale2() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 10 * 1000 * 1000; // 10 MB
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);

        fakeFolder.localModifier().remove(QStringLiteral("A/a0"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.uploadState().children.count(), 0);
    }


    void testCreateConflictWhileSyncing() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 10 * 1000 * 1000; // 10 MB
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        // Put a file on the server and download it.
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a0"), size);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Modify the file localy and start the upload
        fakeFolder.localModifier().setContents(QStringLiteral("A/a0"), size, 'B');
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"), 'B');

        // But in the middle of the sync, modify the file on the server
        QMetaObject::Connection con = QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::transmissionProgress, [&](const ProgressInfo &progress) {
            qDebug() << progress.completedSize() << progress.totalSize();
            if (progress.completedSize() > (progress.totalSize() / 2)) {
                auto a0size = fakeFolder.currentRemoteState().find(QStringLiteral("A/a0"))->contentSize;
                fakeFolder.remoteModifier().setContents(QStringLiteral("A/a0"), a0size, 'C');
                QObject::disconnect(con);
            }
        });

        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        // There was a precondition failed error, this means wen need to sync again
        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), ImmediateFollowUp);

        QCOMPARE(fakeFolder.uploadState().children.count(), 1); // We did not clean the chunks at this point

        // Now we will download the server file and create a conflict
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        auto localState = fakeFolder.currentLocalState();

        // A0 is the one from the server
        QCOMPARE(localState.find("A/a0")->contentSize, size);
        QCOMPARE(localState.find("A/a0")->contentChar, 'C');

        // There is a conflict file with our version
        auto &stateAChildren = localState.find("A")->children;
        auto it = std::find_if(stateAChildren.cbegin(), stateAChildren.cend(), [&](const FileInfo &fi) {
            return fi.name.startsWith(QLatin1String("a0 (conflicted copy"));
        });
        QVERIFY(it != stateAChildren.cend());
        QCOMPARE(it->contentChar, 'B');
        QCOMPARE(it->contentSize, size + 1);

        // Remove the conflict file so the comparison works!
        fakeFolder.localModifier().remove("A/" + it->name);
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QCOMPARE(fakeFolder.uploadState().children.count(), 0); // The last sync cleaned the chunks
    }

    void testModifyLocalFileWhileUploading() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 10_mb;
        setChunkSize(fakeFolder.syncEngine(), 1_mb);

        fakeFolder.localModifier().insert(QStringLiteral("A/a0"), size, 'A');

        // middle of the sync, modify the file
        QMetaObject::Connection con = QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::transmissionProgress,
                                    [&](const ProgressInfo &progress) {
                if (progress.completedSize() > (progress.totalSize() / 2 )) {
                    fakeFolder.localModifier().setContents(QStringLiteral("A/a0"), size, 'B');
                    fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"), 'B');
                    QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
                    QObject::disconnect(con);
                }
        });

        // we will get  OCC::SyncFileItem::Message and error: "Local file changed during sync. It will be resumed."
        // so a message but we report a successful sync
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QThread::sleep(1);

        // There should be a followup sync
        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), ImmediateFollowUp);

        QCOMPARE(fakeFolder.uploadState().children.count(), 1); // We did not clean the chunks at this point
        auto chunkingId = fakeFolder.uploadState().children.first().name;

        // Now we make a new sync which should upload the file for good.
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size + 1);

        // A different chunk id was used, and the previous one is removed
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QVERIFY(fakeFolder.uploadState().children.first().name != chunkingId);
    }


    void testResumeServerDeletedChunks() {

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 30 * 1000 * 1000; // 30 MB
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);
        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;

        // Delete the chunks on the server
        fakeFolder.uploadState().children.clear();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size);

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
        const int size = chunking ? 1 * 1000 * 1000 : 300;
        setChunkSize(fakeFolder.syncEngine(), 300 * 1000);

        // Make the MOVE never reply, but trigger a client-abort and apply the change remotely
        QByteArray checksumHeader;
        int nGET = 0;
        QScopedValueRollback<std::chrono::seconds> setHttpTimeout(AbstractNetworkJob::httpTimeout, 1s);
        const auto responseDelay = 24h; // much bigger than http timeout (so a timeout will occur)
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
        fakeFolder.localModifier().insert(QStringLiteral("A/a0"), size);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // timeout!
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState()); // but the upload succeeded
        QVERIFY(!checksumHeader.isEmpty());
        fakeFolder.remoteModifier().find("A/a0")->checksums = checksumHeader; // The test system don't do that automatically
        // Should be resolved properly
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(nGET, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Test 2: Modify the file further
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"));
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // timeout!
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState()); // but the upload succeeded
        fakeFolder.remoteModifier().find("A/a0")->checksums = checksumHeader;
        // modify again, should not cause conflict
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"));
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync()); // now it's trying to upload the modified file
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        fakeFolder.remoteModifier().find("A/a0")->checksums = checksumHeader;
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(nGET, 0);
    }

    void testPercentEncoding() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const int size = 5 * 1000 * 1000;
        setChunkSize(fakeFolder.syncEngine(), 1 * 1000 * 1000);

        fakeFolder.localModifier().insert("A/file % \u20ac", size);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Only the second upload contains an "If" header
        fakeFolder.localModifier().appendByte("A/file % \u20ac");
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Test uploading large files (1 GiB)
    void testVeryBigFiles() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        const qint64 size = 1_gb; // stay under the INT_MAX (2_gb) limit: QByteArray takes a signed int, so bigger sizes will give weird results.

        // Partial upload of big files
        partialUpload(fakeFolder, QStringLiteral("A/a0"), size);
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        auto chunkingId = fakeFolder.uploadState().children.first().name;

        // Now resume
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size);

        // The same chunk id was re-used
        QCOMPARE(fakeFolder.uploadState().children.count(), 1);
        QCOMPARE(fakeFolder.uploadState().children.first().name, chunkingId);


        // Upload another file again, this time without interruption
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a0"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentRemoteState().find("A/a0")->contentSize, size + 1);
    }


};

QTEST_GUILESS_MAIN(TestChunkingNG)
#include "testchunkingng.moc"
