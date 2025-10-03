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

bool itemSuccessful(const ItemCompletedSpy &spy, const QString &path, const SyncInstructions instr)
{
    auto item = spy.findItem(path);
    return item->_status == SyncFileItem::Success && item->_instruction == instr;
}

bool itemConflict(const ItemCompletedSpy &spy, const QString &path)
{
    auto item = spy.findItem(path);
    return item->_status == SyncFileItem::Conflict && item->_instruction == CSYNC_INSTRUCTION_CONFLICT;
}

bool itemSuccessfulMove(const ItemCompletedSpy &spy, const QString &path)
{
    return itemSuccessful(spy, path, CSYNC_INSTRUCTION_RENAME);
}

QStringList findConflicts(const FileInfo &dir)
{
    QStringList conflicts;
    for (const auto &item : dir.children) {
        if (item.name.contains("(conflicted copy")) {
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
        if (item.name.startsWith(pathComponents.fileName()) && item.name.contains("(conflicted copy")) {
            local.remove(item.path());
            return true;
        }
    }
    return false;
}

SyncJournalFileRecord dbRecord(FakeFolder &folder, const QString &path)
{
    SyncJournalFileRecord record;
    folder.syncJournal().getFileRecord(path, &record);
    return record;
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

        // Verify that the conflict names don't have the user name
        for (const auto &name : findConflicts(fakeFolder.currentLocalState().children["A"])) {
            QVERIFY(!name.contains(fakeFolder.syncEngine().account()->davDisplayName()));
        }

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
                if (request.rawHeader("OC-Conflict") == "1") {
                    auto baseFileId = request.rawHeader("OC-ConflictBaseFileId");
                    auto components = request.url().toString().split('/');
                    QString conflictFile = components.mid(components.size() - 2).join('/');
                    conflictMap[baseFileId] = conflictFile;
                    [&] {
                        QVERIFY(!baseFileId.isEmpty());
                        QCOMPARE(request.rawHeader("OC-ConflictInitialBasePath"), Utility::conflictFileBaseNameFromPattern(conflictFile.toUtf8()));
                    }();
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
        QCOMPARE(Utility::conflictFileBaseNameFromPattern(conflictMap[a1FileId].toUtf8()), QByteArray("A/a1"));

        // Check that the conflict file contains the username
        QVERIFY(conflictMap[a1FileId].contains(QString("(conflicted copy %1 ").arg(fakeFolder.syncEngine().account()->davDisplayName())));

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
                if (request.rawHeader("OC-Conflict") == "1") {
                    auto baseFileId = request.rawHeader("OC-ConflictBaseFileId");
                    auto components = request.url().toString().split('/');
                    QString conflictFile = components.mid(components.size() - 2).join('/');
                    conflictMap[baseFileId] = conflictFile;
                    [&] {
                        QVERIFY(!baseFileId.isEmpty());
                        QCOMPARE(request.rawHeader("OC-ConflictInitialBasePath"), Utility::conflictFileBaseNameFromPattern(conflictFile.toUtf8()));
                    }();
                }
            }
            return nullptr;
        });

        // Explicitly add a conflict file to simulate the case where the upload of the
        // file didn't finish in the same sync run that the conflict was created.
        // To do that we need to create a mock conflict record.
        auto a1FileId = fakeFolder.remoteModifier().find("A/a1")->fileId;
        QString conflictName = QLatin1String("A/a1 (conflicted copy me 1234)");
        fakeFolder.localModifier().insert(conflictName, 64, 'L');
        ConflictRecord conflictRecord;
        conflictRecord.path = conflictName.toUtf8();
        conflictRecord.baseFileId = a1FileId;
        conflictRecord.initialBasePath = "A/a1";
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
        fakeFolder.remoteModifier().insert("A/a1 (conflicted copy 1234)");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto conflictRecord = fakeFolder.syncJournal().conflictRecord("A/a1 (conflicted copy 1234)");
        QVERIFY(conflictRecord.isValid());
        QCOMPARE(conflictRecord.baseFileId, fakeFolder.remoteModifier().find("A/a1")->fileId);
        QCOMPARE(conflictRecord.initialBasePath, QByteArray("A/a1"));

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
                reply->setRawHeader("OC-ConflictInitialBasePath", "A/original");
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
        QCOMPARE(conflictRecord.initialBasePath, QByteArray("A/original"));
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

        QTest::newRow("nomatch1")
            << "a/b/foo"
            << "";
        QTest::newRow("nomatch2")
            << "a/b/foo.txt"
            << "";
        QTest::newRow("nomatch3")
            << "a/b/foo_conflict"
            << "";
        QTest::newRow("nomatch4")
            << "a/b/foo_conflict.txt"
            << "";

        QTest::newRow("match1")
            << "a/b/foo_conflict-123.txt"
            << "a/b/foo.txt";
        QTest::newRow("match2")
            << "a/b/foo_conflict-foo-123.txt"
            << "a/b/foo.txt";

        QTest::newRow("match3")
            << "a/b/foo_conflict-123"
            << "a/b/foo";
        QTest::newRow("match4")
            << "a/b/foo_conflict-foo-123"
            << "a/b/foo";

        // new style
        QTest::newRow("newmatch1")
            << "a/b/foo (conflicted copy 123).txt"
            << "a/b/foo.txt";
        QTest::newRow("newmatch2")
            << "a/b/foo (conflicted copy foo 123).txt"
            << "a/b/foo.txt";

        QTest::newRow("newmatch3")
            << "a/b/foo (conflicted copy 123)"
            << "a/b/foo";
        QTest::newRow("newmatch4")
            << "a/b/foo (conflicted copy foo 123)"
            << "a/b/foo";

        QTest::newRow("newmatch5")
            << "a/b/foo (conflicted copy foo 123) bla"
            << "a/b/foo bla";

        QTest::newRow("newmatch6")
            << "a/b/foo (conflicted copy foo.bar 123)"
            << "a/b/foo";

        // double conflict files
        QTest::newRow("double1")
            << "a/b/foo_conflict-123_conflict-456.txt"
            << "a/b/foo_conflict-123.txt";
        QTest::newRow("double2")
            << "a/b/foo_conflict-foo-123_conflict-bar-456.txt"
            << "a/b/foo_conflict-foo-123.txt";
        QTest::newRow("double3")
            << "a/b/foo (conflicted copy 123) (conflicted copy 456).txt"
            << "a/b/foo (conflicted copy 123).txt";
        QTest::newRow("double4")
            << "a/b/foo (conflicted copy 123)_conflict-456.txt"
            << "a/b/foo (conflicted copy 123).txt";
        QTest::newRow("double5")
            << "a/b/foo_conflict-123 (conflicted copy 456).txt"
            << "a/b/foo_conflict-123.txt";
    }

    void testConflictFileBaseName()
    {
        QFETCH(QString, input);
        QFETCH(QString, output);
        QCOMPARE(Utility::conflictFileBaseNameFromPattern(input.toUtf8()), output.toUtf8());
    }

    void testLocalDirRemoteFileConflict()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "uploadConflictFiles", true } });
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // 1) a NEW/NEW conflict
        fakeFolder.localModifier().mkdir("Z");
        fakeFolder.localModifier().mkdir("Z/subdir");
        fakeFolder.localModifier().insert("Z/foo");
        fakeFolder.remoteModifier().insert("Z", 63);

        // 2) local file becomes a dir; remote file changes
        fakeFolder.localModifier().remove("A/a1");
        fakeFolder.localModifier().mkdir("A/a1");
        fakeFolder.localModifier().insert("A/a1/bar");
        fakeFolder.remoteModifier().appendByte("A/a1");

        // 3) local dir gets a new file; remote dir becomes a file
        fakeFolder.localModifier().insert("B/zzz");
        fakeFolder.remoteModifier().remove("B");
        fakeFolder.remoteModifier().insert("B", 31);

        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findConflicts(fakeFolder.currentLocalState());
        conflicts += findConflicts(fakeFolder.currentLocalState().children["A"]);
        QCOMPARE(conflicts.size(), 3);
        std::sort(conflicts.begin(), conflicts.end());

        auto conflictRecords = fakeFolder.syncJournal().conflictRecordPaths();
        QCOMPARE(conflictRecords.size(), 3);
        std::sort(conflictRecords.begin(), conflictRecords.end());

        // 1)
        QVERIFY(itemConflict(completeSpy, "Z"));
        QCOMPARE(fakeFolder.currentLocalState().find("Z")->size, 63);
        QVERIFY(conflicts[2].contains("Z"));
        QCOMPARE(conflicts[2].toUtf8(), conflictRecords[2]);
        QVERIFY(QFileInfo(fakeFolder.localPath() + conflicts[2]).isDir());
        QVERIFY(QFile::exists(fakeFolder.localPath() + conflicts[2] + "/foo"));

        // 2)
        QVERIFY(itemConflict(completeSpy, "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState().find("A/a1")->size, 5);
        QVERIFY(conflicts[0].contains("A/a1"));
        QCOMPARE(conflicts[0].toUtf8(), conflictRecords[0]);
        QVERIFY(QFileInfo(fakeFolder.localPath() + conflicts[0]).isDir());
        QVERIFY(QFile::exists(fakeFolder.localPath() + conflicts[0] + "/bar"));

        // 3)
        QVERIFY(itemConflict(completeSpy, "B"));
        QCOMPARE(fakeFolder.currentLocalState().find("B")->size, 31);
        QVERIFY(conflicts[1].contains("B"));
        QCOMPARE(conflicts[1].toUtf8(), conflictRecords[1]);
        QVERIFY(QFileInfo(fakeFolder.localPath() + conflicts[1]).isDir());
        QVERIFY(QFile::exists(fakeFolder.localPath() + conflicts[1] + "/zzz"));

        // The contents of the conflict directories will only be uploaded after
        // another sync.
        QVERIFY(fakeFolder.syncEngine().isAnotherSyncNeeded() == ImmediateFollowUp);
        cleanup();
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(itemSuccessful(completeSpy, conflicts[0], CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[0] + "/bar", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[1], CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[1] + "/zzz", CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[2], CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[2] + "/foo", CSYNC_INSTRUCTION_NEW));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testLocalFileRemoteDirConflict()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "uploadConflictFiles", true } });
        ItemCompletedSpy completeSpy(fakeFolder);

        // 1) a NEW/NEW conflict
        fakeFolder.remoteModifier().mkdir("Z");
        fakeFolder.remoteModifier().mkdir("Z/subdir");
        fakeFolder.remoteModifier().insert("Z/foo");
        fakeFolder.localModifier().insert("Z");

        // 2) local dir becomes file: remote dir adds file
        fakeFolder.localModifier().remove("A");
        fakeFolder.localModifier().insert("A", 63);
        fakeFolder.remoteModifier().insert("A/bar");

        // 3) local file changes; remote file becomes dir
        fakeFolder.localModifier().appendByte("B/b1");
        fakeFolder.remoteModifier().remove("B/b1");
        fakeFolder.remoteModifier().mkdir("B/b1");
        fakeFolder.remoteModifier().insert("B/b1/zzz");

        QVERIFY(fakeFolder.syncOnce());
        auto conflicts = findConflicts(fakeFolder.currentLocalState());
        conflicts += findConflicts(fakeFolder.currentLocalState().children["B"]);
        QCOMPARE(conflicts.size(), 3);
        std::sort(conflicts.begin(), conflicts.end());

        auto conflictRecords = fakeFolder.syncJournal().conflictRecordPaths();
        QCOMPARE(conflictRecords.size(), 3);
        std::sort(conflictRecords.begin(), conflictRecords.end());

        // 1)
        QVERIFY(itemConflict(completeSpy, "Z"));
        QVERIFY(conflicts[2].contains("Z"));
        QCOMPARE(conflicts[2].toUtf8(), conflictRecords[2]);

        // 2)
        QVERIFY(itemConflict(completeSpy, "A"));
        QVERIFY(conflicts[0].contains("A"));
        QCOMPARE(conflicts[0].toUtf8(), conflictRecords[0]);

        // 3)
        QVERIFY(itemConflict(completeSpy, "B/b1"));
        QVERIFY(conflicts[1].contains("B/b1"));
        QCOMPARE(conflicts[1].toUtf8(), conflictRecords[1]);

        // Also verifies that conflicts were uploaded
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testTypeConflictWithMove()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        ItemCompletedSpy completeSpy(fakeFolder);

        // the remote becomes a file, but a file inside the dir has moved away!
        fakeFolder.remoteModifier().remove("A");
        fakeFolder.remoteModifier().insert("A");
        fakeFolder.localModifier().rename("A/a1", "a1");

        // same, but with a new file inside the dir locally
        fakeFolder.remoteModifier().remove("B");
        fakeFolder.remoteModifier().insert("B");
        fakeFolder.localModifier().rename("B/b1", "b1");
        fakeFolder.localModifier().insert("B/new");

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(itemSuccessful(completeSpy, "A", CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemConflict(completeSpy, "B"));

        auto conflicts = findConflicts(fakeFolder.currentLocalState());
        std::sort(conflicts.begin(), conflicts.end());
        QVERIFY(conflicts.size() == 2);
        QVERIFY(conflicts[0].contains("A (conflicted copy"));
        QVERIFY(conflicts[1].contains("B (conflicted copy"));
        for (const auto& conflict : conflicts)
            QDir(fakeFolder.localPath() + conflict).removeRecursively();
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Currently a1 and b1 don't get moved, but redownloaded
    }

    void testTypeChange()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        ItemCompletedSpy completeSpy(fakeFolder);

        // dir becomes file
        fakeFolder.remoteModifier().remove("A");
        fakeFolder.remoteModifier().insert("A");
        fakeFolder.localModifier().remove("B");
        fakeFolder.localModifier().insert("B");

        // file becomes dir
        fakeFolder.remoteModifier().remove("C/c1");
        fakeFolder.remoteModifier().mkdir("C/c1");
        fakeFolder.remoteModifier().insert("C/c1/foo");
        fakeFolder.localModifier().remove("C/c2");
        fakeFolder.localModifier().mkdir("C/c2");
        fakeFolder.localModifier().insert("C/c2/bar");

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(itemSuccessful(completeSpy, "A", CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemSuccessful(completeSpy, "B", CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemSuccessful(completeSpy, "C/c1", CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemSuccessful(completeSpy, "C/c2", CSYNC_INSTRUCTION_TYPE_CHANGE));

        // A becomes a conflict because we don't delete folders with files
        // inside of them!
        auto conflicts = findConflicts(fakeFolder.currentLocalState());
        QVERIFY(conflicts.size() == 1);
        QVERIFY(conflicts[0].contains("A (conflicted copy"));
        for (const auto& conflict : conflicts)
            QDir(fakeFolder.localPath() + conflict).removeRecursively();

        QVERIFY(fakeFolder.syncEngine().isAnotherSyncNeeded() == ImmediateFollowUp);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Test what happens if we remove entries both on the server, and locally
    void testRemoveRemove()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.remoteModifier().remove("A");
        fakeFolder.localModifier().remove("A");
        fakeFolder.remoteModifier().remove("B/b1");
        fakeFolder.localModifier().remove("B/b1");

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto expectedState = fakeFolder.currentLocalState();

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);

        QVERIFY(dbRecord(fakeFolder, "B/b2").isValid());

        QVERIFY(!dbRecord(fakeFolder, "B/b1").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A/a1").isValid());
        QVERIFY(!dbRecord(fakeFolder, "A").isValid());
    }
};

QTEST_GUILESS_MAIN(TestSyncConflict)
#include "testsyncconflict.moc"
