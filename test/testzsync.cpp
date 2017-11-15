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
        for (int i = 0; i < 10; i++) {
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
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            QUrlQuery query(request.url());
            if (op == QNetworkAccessManager::GetOperation) {
                if (query.hasQueryItem("zsync")) {
                    return new FakeGetWithDataReply{ fakeFolder.remoteModifier(), metadata, op, request, this };
                }

                return new FakeGetWithDataReply{ fakeFolder.remoteModifier(), data, op, request, this };
            }

            return nullptr;
        });
        QVERIFY(fakeFolder.syncOnce());
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
};

QTEST_GUILESS_MAIN(TestZsync)
#include "testzsync.moc"
