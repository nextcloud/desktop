/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>
#include <propagatecommonzsync.h>

using namespace OCC;

QStringList findConflicts(const FileInfo &dir)
{
    QStringList conflicts;
    for (const auto &item : dir.children) {
        if (item.name.contains("conflict")) {
            conflicts.append(item.path());
        }
    }
    return conflicts;
}

static quint64 blockstart_from_offset(quint64 offset)
{
    return offset & ~quint64(ZSYNC_BLOCKSIZE - 1);
}

class TestZsync : public QObject
{
    Q_OBJECT

private slots:

    void testFileDownloadSimple()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "chunking", "1.0" }, { "zsync", "1.0" } } } });

        SyncOptions opt;
        opt._deltaSyncEnabled = true;
        opt._deltaSyncMinFileSize = 0;
        fakeFolder.syncEngine().setSyncOptions(opt);

        const int size = 100 * 1000 * 1000;
        QByteArray metadata;

        // Test 1: NEW file upload with zsync metadata
        fakeFolder.localModifier().insert("A/a0", size);
        fakeFolder.localModifier().appendByte("A/a0", 'X');
        qsrand(QDateTime::currentDateTime().toTime_t());
        const int nModifications = 10;
        for (int i = 0; i < nModifications; i++) {
            quint64 offset = qrand() % size;
            fakeFolder.localModifier().modifyByte("A/a0", offset, 'Y');
        }
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *data) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation && request.url().toString().endsWith(".zsync")) {
                metadata = data->readAll();
                return new FakePutReply{ fakeFolder.uploadState(), op, request, metadata, this };
            }

            return nullptr;
        });
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(metadata.startsWith("zsync: "));

        // Keep hold of original file contents
        QFile f(fakeFolder.localPath() + "/A/a0");
        f.open(QIODevice::ReadOnly);
        QByteArray data = f.readAll();
        f.close();

        // Test 2: update local file to unchanged version and download changes
        fakeFolder.localModifier().remove("A/a0");
        fakeFolder.localModifier().insert("A/a0", size);
        auto currentMtime = QDateTime::currentDateTimeUtc();
        fakeFolder.remoteModifier().setModTime("A/a0", currentMtime);
        quint64 transferedData = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            QUrlQuery query(request.url());
            if (op == QNetworkAccessManager::GetOperation) {
                if (query.hasQueryItem("zsync")) {
                    return new FakeGetWithDataReply{ fakeFolder.remoteModifier(), metadata, op, request, this };
                }

                auto reply = new FakeGetWithDataReply{ fakeFolder.remoteModifier(), data, op, request, this };
                transferedData += reply->size;
                return reply;
            }

            return nullptr;
        });
        QVERIFY(fakeFolder.syncOnce());

        // We didn't transfer the whole file
        // (plus one because of the new trailing byte)
        QVERIFY(transferedData <= (nModifications + 1) * ZSYNC_BLOCKSIZE);

        // Verify that the newly propagated file was assembled to have the expected data
        f.open(QIODevice::ReadOnly);
        QVERIFY(data == f.readAll());
        f.close();

        auto conflicts = findConflicts(fakeFolder.currentLocalState().children["A"]);
        QCOMPARE(conflicts.size(), 1);
        for (auto c : conflicts) {
            fakeFolder.localModifier().remove(c);
        }
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testFileUploadSimple()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "chunking", "1.0" }, { "zsync", "1.0" } } } });

        SyncOptions opt;
        opt._deltaSyncEnabled = true;
        opt._deltaSyncMinFileSize = 0;
        fakeFolder.syncEngine().setSyncOptions(opt);

        const int size = 100 * 1000 * 1000;
        QByteArray metadata;

        // Test 1: NEW file upload with zsync metadata
        fakeFolder.localModifier().insert("A/a0", size);
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *data) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation && request.url().toString().endsWith(".zsync")) {
                metadata = data->readAll();
                return new FakePutReply{ fakeFolder.uploadState(), op, request, metadata, this };
            }

            return nullptr;
        });
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(metadata.startsWith("zsync: "));

        // Test 2: Modify local contents and ensure that modified chunks are sent
        QVector<quint64> mods;
        qsrand(QDateTime::currentDateTime().toTime_t());
        fakeFolder.localModifier().appendByte("A/a0", 'X');
        mods.append(blockstart_from_offset(size + 1));
        for (int i = 0; i < 10; i++) {
            quint64 offset = qrand() % size;
            fakeFolder.localModifier().modifyByte("A/a0", offset, 'Y');
            mods.append(blockstart_from_offset(offset));
        }
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            QUrlQuery query(request.url());
            if (op == QNetworkAccessManager::GetOperation && query.hasQueryItem("zsync")) {
                return new FakeGetWithDataReply{ fakeFolder.remoteModifier(), metadata, op, request, this };
            }

            if (request.attribute(QNetworkRequest::CustomVerbAttribute) == QLatin1String("MOVE")) {
                return new FakeChunkZsyncMoveReply{ fakeFolder.uploadState(), fakeFolder.remoteModifier(), op, request, 0, mods, this };
            }

            return nullptr;
        });
        QVERIFY(fakeFolder.syncOnce());
        fakeFolder.remoteModifier().appendByte("A/a0", 'X');
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testFileUploadGrowShrink()
    {
        FakeFolder fakeFolder{ FileInfo() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "chunking", "1.0" }, { "zsync", "1.0" } } } });

        SyncOptions opt;
        opt._deltaSyncEnabled = true;
        opt._deltaSyncMinFileSize = 0;
        int chunkSize = 2 * 1000 * 1000;
        opt._minChunkSize = opt._maxChunkSize = opt._initialChunkSize = chunkSize; // don't dynamically size chunks
        fakeFolder.syncEngine().setSyncOptions(opt);

        const int zsyncBlockSize = 1024 * 1024;

        QByteArray metadata;
        QList<QPair<int, int>> putChunks;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *data) -> QNetworkReply * {
            // Handle .zsync file storage and retrieval - note that preparing a PUT already updates the
            // zsync file data before the upload is finalized with a MOVE here.
            QUrlQuery query(request.url());
            if (request.url().toString().endsWith(".zsync") && op == QNetworkAccessManager::PutOperation) {
                metadata = data->readAll();
                return new FakePutReply{ fakeFolder.uploadState(), op, request, metadata, this };
            }
            if (op == QNetworkAccessManager::GetOperation && query.hasQueryItem("zsync")) {
                return new FakeGetWithDataReply{ fakeFolder.remoteModifier(), metadata, op, request, this };
            }
            // Grab chunk offset and size
            if (op == QNetworkAccessManager::PutOperation) {
                auto payload = data->readAll();
                putChunks.append({ request.rawHeader("OC-Chunk-Offset").toInt(), payload.size() });
                return new FakePutReply{ fakeFolder.uploadState(), op, request, payload, this };
            }
            return nullptr;
        });

        // Note: The remote side doesn't actually store the file contents

        // Test 1: NEW file upload with zsync metadata
        auto totalSize = 2 * zsyncBlockSize + 5;
        fakeFolder.localModifier().insert("a0", totalSize);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(putChunks.size() == 2);
        QVERIFY(putChunks[0] == qMakePair(0, 2000000));
        QVERIFY(putChunks[1] == qMakePair(2000000, totalSize - 2000000));

        // Test 2: Appending data to the file
        putChunks.clear();
        totalSize += 1;
        fakeFolder.localModifier().appendByte("a0", 'Q');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(putChunks.size() == 1);
        QVERIFY(putChunks[0] == qMakePair(2 * zsyncBlockSize, 6));

        // Test 3: reduce the file size
        putChunks.clear();
        totalSize -= 10;
        fakeFolder.localModifier().remove("a0");
        fakeFolder.localModifier().insert("a0", totalSize);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(putChunks.size() == 1);
        QVERIFY(putChunks[0] == qMakePair(zsyncBlockSize, totalSize - zsyncBlockSize)); // technically unnecessary

        // Test 4: add a large amount, such that the zsync block gets chunked
        putChunks.clear();
        totalSize += int(1.5 * chunkSize);
        fakeFolder.localModifier().remove("a0");
        fakeFolder.localModifier().insert("a0", totalSize);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(putChunks.size() == 3);
        // The first block stays, the rest is re-uploaded, total upload size is roughly 1MiB + 1.5 * 2MB = slightly more than 4MB
        QVERIFY(putChunks[0] == qMakePair(zsyncBlockSize, chunkSize));
        QVERIFY(putChunks[1] == qMakePair(zsyncBlockSize + chunkSize, chunkSize));
        QVERIFY(putChunks[2] == qMakePair(zsyncBlockSize + 2 * chunkSize, totalSize - (zsyncBlockSize + 2 * chunkSize)));

        // Test 5: append and change an early block at the same time
        putChunks.clear();
        totalSize += 1;
        fakeFolder.localModifier().appendByte("a0", 'Q');
        fakeFolder.localModifier().modifyByte("a0", 5, 'Q');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(putChunks.size() == 2);
        QVERIFY(putChunks[0] == qMakePair(0, zsyncBlockSize));
        QVERIFY(putChunks[1] == qMakePair(4 * zsyncBlockSize, totalSize - 4 * zsyncBlockSize));

        // Test 6: Shrink to an aligned size
        putChunks.clear();
        totalSize = 2 * zsyncBlockSize;
        fakeFolder.localModifier().remove("a0");
        fakeFolder.localModifier().insert("a0", totalSize);
        fakeFolder.localModifier().modifyByte("a0", 5, 'Q'); // same data as before
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(putChunks.size() == 0);

        // Test 7: Grow to an aligned size
        putChunks.clear();
        totalSize = 3 * zsyncBlockSize;
        fakeFolder.localModifier().remove("a0");
        fakeFolder.localModifier().insert("a0", totalSize);
        fakeFolder.localModifier().modifyByte("a0", 5, 'Q'); // same data as before
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(putChunks.size() == 1);
        QVERIFY(putChunks[0] == qMakePair(2 * zsyncBlockSize, zsyncBlockSize));
    }
};

QTEST_GUILESS_MAIN(TestZsync)
#include "testzsync.moc"
