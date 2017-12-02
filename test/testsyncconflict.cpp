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

SyncFileItemPtr findItem(const QSignalSpy &spy, const QString &path)
{
    for (const QList<QVariant> &args : spy) {
        auto item = args[0].value<SyncFileItemPtr>();
        if (item->destination() == path)
            return item;
    }
    return SyncFileItemPtr(new SyncFileItem);
}

bool itemSuccessful(const QSignalSpy &spy, const QString &path, const csync_instructions_e instr)
{
    auto item = findItem(spy, path);
    return item->_status == SyncFileItem::Success && item->_instruction == instr;
}

bool itemConflict(const QSignalSpy &spy, const QString &path)
{
    auto item = findItem(spy, path);
    return item->_status == SyncFileItem::Conflict && item->_instruction == CSYNC_INSTRUCTION_CONFLICT;
}

bool itemSuccessfulMove(const QSignalSpy &spy, const QString &path)
{
    return itemSuccessful(spy, path, CSYNC_INSTRUCTION_RENAME);
}

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

bool expectAndWipeConflict(FileModifier &local, FileInfo state, const QString path)
{
    PathComponents pathComponents(path);
    auto base = state.find(pathComponents.parentDirComponents());
    if (!base)
        return false;
    for (const auto &item : base->children) {
        if (item.name.startsWith(pathComponents.fileName()) && item.name.contains("_conflict")) {
            local.remove(item.path());
            return true;
        }
    }
    return false;
}

class TestSyncConflict : public QObject
{
    Q_OBJECT

private slots:
    void testNoUpload()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().setContents("A/a1", 'L');
        fakeFolder.remoteModifier().setContents("A/a1", 'R');
        fakeFolder.localModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(expectAndWipeConflict(fakeFolder.localModifier(), fakeFolder.currentLocalState(), "A/a1"));
        QVERIFY(expectAndWipeConflict(fakeFolder.localModifier(), fakeFolder.currentLocalState(), "A/a2"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testUploadAfterDownload()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "uploadConflictFiles", true } });
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QMap<QByteArray, QString> conflictMap;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                auto baseFileId = request.rawHeader("OC-ConflictBaseFileId");
                if (!baseFileId.isEmpty()) {
                    auto components = request.url().toString().split('/');
                    QString conflictFile = components.mid(components.size() - 2).join('/');
                    conflictMap[baseFileId] = conflictFile;
                }
            }
            return nullptr;
        });

        fakeFolder.localModifier().setContents("A/a1", 'L');
        fakeFolder.remoteModifier().setContents("A/a1", 'R');
        fakeFolder.localModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");
        QVERIFY(fakeFolder.syncOnce());
        auto local = fakeFolder.currentLocalState();
        auto remote = fakeFolder.currentRemoteState();
        QCOMPARE(local, remote);

        auto a1FileId = fakeFolder.remoteModifier().find("A/a1")->fileId;
        auto a2FileId = fakeFolder.remoteModifier().find("A/a2")->fileId;
        QVERIFY(conflictMap.contains(a1FileId));
        QVERIFY(conflictMap.contains(a2FileId));
        QCOMPARE(conflictMap.size(), 2);
        QCOMPARE(Utility::conflictFileBaseName(conflictMap[a1FileId].toUtf8()), QByteArray("A/a1"));

        QCOMPARE(remote.find(conflictMap[a1FileId])->contentChar, 'L');
        QCOMPARE(remote.find("A/a1")->contentChar, 'R');

        QCOMPARE(remote.find(conflictMap[a2FileId])->size, 5);
        QCOMPARE(remote.find("A/a2")->size, 6);
    }

    void testSeparateUpload()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "uploadConflictFiles", true } });
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QMap<QByteArray, QString> conflictMap;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                auto baseFileId = request.rawHeader("OC-ConflictBaseFileId");
                if (!baseFileId.isEmpty()) {
                    auto components = request.url().toString().split('/');
                    QString conflictFile = components.mid(components.size() - 2).join('/');
                    conflictMap[baseFileId] = conflictFile;
                }
            }
            return nullptr;
        });

        // Explicitly add a conflict file to simulate the case where the upload of the
        // file didn't finish in the same sync run that the conflict was created.
        // To do that we need to create a mock conflict record.
        auto a1FileId = fakeFolder.remoteModifier().find("A/a1")->fileId;
        QString conflictName = QLatin1String("A/a1_conflict-me-1234");
        fakeFolder.localModifier().insert(conflictName, 64, 'L');
        ConflictRecord conflictRecord;
        conflictRecord.path = conflictName.toUtf8();
        conflictRecord.baseFileId = a1FileId;
        fakeFolder.syncJournal().setConflictRecord(conflictRecord);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(conflictMap.size(), 1);
        QCOMPARE(conflictMap[a1FileId], conflictName);
        QCOMPARE(fakeFolder.currentRemoteState().find(conflictMap[a1FileId])->contentChar, 'L');
        conflictMap.clear();

        // Now the user can locally alter the conflict file and it will be uploaded
        // as usual.
        fakeFolder.localModifier().setContents(conflictName, 'P');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(conflictMap.size(), 1);
        QCOMPARE(conflictMap[a1FileId], conflictName);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        conflictMap.clear();

        // Similarly, remote modifications of conflict files get propagated downwards
        fakeFolder.remoteModifier().setContents(conflictName, 'Q');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(conflictMap.isEmpty());

        // Conflict files for conflict files!
        auto a1ConflictFileId = fakeFolder.remoteModifier().find(conflictName)->fileId;
        fakeFolder.remoteModifier().appendByte(conflictName);
        fakeFolder.remoteModifier().appendByte(conflictName);
        fakeFolder.localModifier().appendByte(conflictName);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(conflictMap.size(), 1);
        QVERIFY(conflictMap.contains(a1ConflictFileId));
        QCOMPARE(fakeFolder.currentRemoteState().find(conflictName)->size, 66);
        QCOMPARE(fakeFolder.currentRemoteState().find(conflictMap[a1ConflictFileId])->size, 65);
        conflictMap.clear();
    }

    // What happens if we download a conflict file? Is the metadata set up correctly?
    void testDownloadingConflictFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "uploadConflictFiles", true } });
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // With no headers from the server
        fakeFolder.remoteModifier().insert("A/a1_conflict-1234");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto conflictRecord = fakeFolder.syncJournal().conflictRecord("A/a1_conflict-1234");
        QVERIFY(conflictRecord.isValid());
        QCOMPARE(conflictRecord.baseFileId, fakeFolder.remoteModifier().find("A/a1")->fileId);

        // Now with server headers
        QObject parent;
        auto a2FileId = fakeFolder.remoteModifier().find("A/a2")->fileId;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation) {
                auto reply = new FakeGetReply(fakeFolder.remoteModifier(), op, request, &parent);
                reply->setRawHeader("OC-Conflict", "1");
                reply->setRawHeader("OC-ConflictBaseFileId", a2FileId);
                reply->setRawHeader("OC-ConflictBaseMtime", "1234");
                reply->setRawHeader("OC-ConflictBaseEtag", "etag");
                return reply;
            }
            return nullptr;
        });
        fakeFolder.remoteModifier().insert("A/really-a-conflict"); // doesn't look like a conflict, but headers say it is
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        conflictRecord = fakeFolder.syncJournal().conflictRecord("A/really-a-conflict");
        QVERIFY(conflictRecord.isValid());
        QCOMPARE(conflictRecord.baseFileId, a2FileId);
        QCOMPARE(conflictRecord.baseModtime, 1234);
        QCOMPARE(conflictRecord.baseEtag, QByteArray("etag"));
    }

    // Check that conflict records are removed when the file is gone
    void testConflictRecordRemoval1()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "uploadConflictFiles", true } });
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Make conflict records
        ConflictRecord conflictRecord;
        conflictRecord.path = "A/a1";
        fakeFolder.syncJournal().setConflictRecord(conflictRecord);
        conflictRecord.path = "A/a2";
        fakeFolder.syncJournal().setConflictRecord(conflictRecord);

        // A nothing-to-sync keeps them alive
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(fakeFolder.syncJournal().conflictRecord("A/a1").isValid());
        QVERIFY(fakeFolder.syncJournal().conflictRecord("A/a2").isValid());

        // When the file is removed, the record is removed too
        fakeFolder.localModifier().remove("A/a2");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(fakeFolder.syncJournal().conflictRecord("A/a1").isValid());
        QVERIFY(!fakeFolder.syncJournal().conflictRecord("A/a2").isValid());
    }

    // Same test, but with uploadConflictFiles == false
    void testConflictRecordRemoval2()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "uploadConflictFiles", false } });
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Create two conflicts
        fakeFolder.localModifier().appendByte("A/a1");
        fakeFolder.localModifier().appendByte("A/a1");
        fakeFolder.remoteModifier().appendByte("A/a1");
        fakeFolder.localModifier().appendByte("A/a2");
        fakeFolder.localModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findConflicts(fakeFolder.currentLocalState().children["A"]);
        QByteArray a1conflict;
        QByteArray a2conflict;
        for (const auto & conflict : conflicts) {
            if (conflict.contains("a1"))
                a1conflict = conflict.toUtf8();
            if (conflict.contains("a2"))
                a2conflict = conflict.toUtf8();
        }

        // A nothing-to-sync keeps them alive
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.syncJournal().conflictRecord(a1conflict).isValid());
        QVERIFY(fakeFolder.syncJournal().conflictRecord(a2conflict).isValid());

        // When the file is removed, the record is removed too
        fakeFolder.localModifier().remove(a2conflict);
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.syncJournal().conflictRecord(a1conflict).isValid());
        QVERIFY(!fakeFolder.syncJournal().conflictRecord(a2conflict).isValid());
    }

    void testConflictFileBaseName_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("output");

        QTest::newRow("")
            << "a/b/foo"
            << "";
        QTest::newRow("")
            << "a/b/foo.txt"
            << "";
        QTest::newRow("")
            << "a/b/foo_conflict"
            << "";
        QTest::newRow("")
            << "a/b/foo_conflict.txt"
            << "";

        QTest::newRow("")
            << "a/b/foo_conflict-123.txt"
            << "a/b/foo.txt";
        QTest::newRow("")
            << "a/b/foo_conflict-foo-123.txt"
            << "a/b/foo.txt";

        QTest::newRow("")
            << "a/b/foo_conflict-123"
            << "a/b/foo";
        QTest::newRow("")
            << "a/b/foo_conflict-foo-123"
            << "a/b/foo";

        // double conflict files
        QTest::newRow("")
            << "a/b/foo_conflict-123_conflict-456.txt"
            << "a/b/foo_conflict-123.txt";
        QTest::newRow("")
            << "a/b/foo_conflict-foo-123_conflict-bar-456.txt"
            << "a/b/foo_conflict-foo-123.txt";
    }

    void testConflictFileBaseName()
    {
        QFETCH(QString, input);
        QFETCH(QString, output);
        QCOMPARE(Utility::conflictFileBaseName(input.toUtf8()), output.toUtf8());
    }
};

QTEST_GUILESS_MAIN(TestSyncConflict)
#include "testsyncconflict.moc"
