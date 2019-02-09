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


struct OperationCounter {
    int nGET = 0;
    int nPUT = 0;
    int nMOVE = 0;
    int nDELETE = 0;

    void reset() { *this = {}; }

    auto functor() {
        return [&](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *) {
            if (op == QNetworkAccessManager::GetOperation)
                ++nGET;
            if (op == QNetworkAccessManager::PutOperation)
                ++nPUT;
            if (op == QNetworkAccessManager::DeleteOperation)
                ++nDELETE;
            if (req.attribute(QNetworkRequest::CustomVerbAttribute) == "MOVE")
                ++nMOVE;
            return nullptr;
        };
    }
};

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

class TestSyncMove : public QObject
{
    Q_OBJECT

private slots:
    void testRemoteChangeInMovedFolder()
    {
        // issue #5192
        FakeFolder fakeFolder{ FileInfo{ QString(), { FileInfo{ QStringLiteral("folder"), { FileInfo{ QStringLiteral("folderA"), { { QStringLiteral("file.txt"), 400 } } }, QStringLiteral("folderB") } } } } };

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Edit a file in a moved directory.
        fakeFolder.remoteModifier().setContents("folder/folderA/file.txt", 'a');
        fakeFolder.remoteModifier().rename("folder/folderA", "folder/folderB/folderA");
        fakeFolder.syncOnce();
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto oldState = fakeFolder.currentLocalState();
        QVERIFY(oldState.find("folder/folderB/folderA/file.txt"));
        QVERIFY(!oldState.find("folder/folderA/file.txt"));

        // This sync should not remove the file
        fakeFolder.syncOnce();
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentLocalState(), oldState);
    }

    void testSelectiveSyncMovedFolder()
    {
        // issue #5224
        FakeFolder fakeFolder{ FileInfo{ QString(), { FileInfo{ QStringLiteral("parentFolder"), { FileInfo{ QStringLiteral("subFolderA"), { { QStringLiteral("fileA.txt"), 400 } } }, FileInfo{ QStringLiteral("subFolderB"), { { QStringLiteral("fileB.txt"), 400 } } } } } } } };

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto expectedServerState = fakeFolder.currentRemoteState();

        // Remove subFolderA with selectiveSync:
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
            { "parentFolder/subFolderA/" });
        fakeFolder.syncEngine().journal()->avoidReadFromDbOnNextSync(QByteArrayLiteral("parentFolder/subFolderA/"));

        fakeFolder.syncOnce();

        {
            // Nothing changed on the server
            QCOMPARE(fakeFolder.currentRemoteState(), expectedServerState);
            // The local state should not have subFolderA
            auto remoteState = fakeFolder.currentRemoteState();
            remoteState.remove("parentFolder/subFolderA");
            QCOMPARE(fakeFolder.currentLocalState(), remoteState);
        }

        // Rename parentFolder on the server
        fakeFolder.remoteModifier().rename("parentFolder", "parentFolderRenamed");
        expectedServerState = fakeFolder.currentRemoteState();
        fakeFolder.syncOnce();

        {
            QCOMPARE(fakeFolder.currentRemoteState(), expectedServerState);
            auto remoteState = fakeFolder.currentRemoteState();
            // The subFolderA should still be there on the server.
            QVERIFY(remoteState.find("parentFolderRenamed/subFolderA/fileA.txt"));
            // But not on the client because of the selective sync
            remoteState.remove("parentFolderRenamed/subFolderA");
            QCOMPARE(fakeFolder.currentLocalState(), remoteState);
        }

        // Rename it again, locally this time.
        fakeFolder.localModifier().rename("parentFolderRenamed", "parentThirdName");
        fakeFolder.syncOnce();

        {
            auto remoteState = fakeFolder.currentRemoteState();
            // The subFolderA should still be there on the server.
            QVERIFY(remoteState.find("parentThirdName/subFolderA/fileA.txt"));
            // But not on the client because of the selective sync
            remoteState.remove("parentThirdName/subFolderA");
            QCOMPARE(fakeFolder.currentLocalState(), remoteState);

            expectedServerState = fakeFolder.currentRemoteState();
            QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
            fakeFolder.syncOnce(); // This sync should do nothing
            QCOMPARE(completeSpy.count(), 0);

            QCOMPARE(fakeFolder.currentRemoteState(), expectedServerState);
            QCOMPARE(fakeFolder.currentLocalState(), remoteState);
        }
    }

    void testLocalMoveDetection()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        int nPUT = 0;
        int nDELETE = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &, QIODevice *) {
            if (op == QNetworkAccessManager::PutOperation)
                ++nPUT;
            if (op == QNetworkAccessManager::DeleteOperation)
                ++nDELETE;
            return nullptr;
        });

        // For directly editing the remote checksum
        FileInfo &remoteInfo = fakeFolder.remoteModifier();

        // Simple move causing a remote rename
        fakeFolder.localModifier().rename("A/a1", "A/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), remoteInfo);
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(remoteInfo));
        QCOMPARE(nPUT, 0);

        // Move-and-change, causing a upload and delete
        fakeFolder.localModifier().rename("A/a2", "A/a2m");
        fakeFolder.localModifier().appendByte("A/a2m");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), remoteInfo);
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(remoteInfo));
        QCOMPARE(nPUT, 1);
        QCOMPARE(nDELETE, 1);

        // Move-and-change, mtime+content only
        fakeFolder.localModifier().rename("B/b1", "B/b1m");
        fakeFolder.localModifier().setContents("B/b1m", 'C');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), remoteInfo);
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(remoteInfo));
        QCOMPARE(nPUT, 2);
        QCOMPARE(nDELETE, 2);

        // Move-and-change, size+content only
        auto mtime = fakeFolder.remoteModifier().find("B/b2")->lastModified;
        fakeFolder.localModifier().rename("B/b2", "B/b2m");
        fakeFolder.localModifier().appendByte("B/b2m");
        fakeFolder.localModifier().setModTime("B/b2m", mtime);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), remoteInfo);
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(remoteInfo));
        QCOMPARE(nPUT, 3);
        QCOMPARE(nDELETE, 3);

        // Move-and-change, content only -- c1 has no checksum, so we fail to detect this!
        // NOTE: This is an expected failure.
        mtime = fakeFolder.remoteModifier().find("C/c1")->lastModified;
        fakeFolder.localModifier().rename("C/c1", "C/c1m");
        fakeFolder.localModifier().setContents("C/c1m", 'C');
        fakeFolder.localModifier().setModTime("C/c1m", mtime);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nPUT, 3);
        QCOMPARE(nDELETE, 3);
        QVERIFY(!(fakeFolder.currentLocalState() == remoteInfo));

        // cleanup, and upload a file that will have a checksum in the db
        fakeFolder.localModifier().remove("C/c1m");
        fakeFolder.localModifier().insert("C/c3");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), remoteInfo);
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(remoteInfo));
        QCOMPARE(nPUT, 4);
        QCOMPARE(nDELETE, 4);

        // Move-and-change, content only, this time while having a checksum
        mtime = fakeFolder.remoteModifier().find("C/c3")->lastModified;
        fakeFolder.localModifier().rename("C/c3", "C/c3m");
        fakeFolder.localModifier().setContents("C/c3m", 'C');
        fakeFolder.localModifier().setModTime("C/c3m", mtime);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nPUT, 5);
        QCOMPARE(nDELETE, 5);
        QCOMPARE(fakeFolder.currentLocalState(), remoteInfo);
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(remoteInfo));
    }

    void testDuplicateFileId_data()
    {
        QTest::addColumn<QString>("prefix");

        // There have been bugs related to how the original
        // folder and the folder with the duplicate tree are
        // ordered. Test both cases here.
        QTest::newRow("first ordering") << "O"; // "O" > "A"
        QTest::newRow("second ordering") << "0"; // "0" < "A"
    }

    // If the same folder is shared in two different ways with the same
    // user, the target user will see duplicate file ids. We need to make
    // sure the move detection and sync still do the right thing in that
    // case.
    void testDuplicateFileId()
    {
        QFETCH(QString, prefix);

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto &remote = fakeFolder.remoteModifier();

        remote.mkdir("A/W");
        remote.insert("A/W/w1");
        remote.mkdir("A/Q");

        // Duplicate every entry in A under O/A
        remote.mkdir(prefix);
        remote.children[prefix].addChild(remote.children["A"]);

        // This already checks that the rename detection doesn't get
        // horribly confused if we add new files that have the same
        // fileid as existing ones
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        int nGET = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &, QIODevice *) {
            if (op == QNetworkAccessManager::GetOperation)
                ++nGET;
            return nullptr;
        });

        // Try a remote file move
        remote.rename("A/a1", "A/W/a1m");
        remote.rename(prefix + "/A/a1", prefix + "/A/W/a1m");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(nGET, 0);

        // And a remote directory move
        remote.rename("A/W", "A/Q/W");
        remote.rename(prefix + "/A/W", prefix + "/A/Q/W");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(nGET, 0);

        // Partial file removal (in practice, A/a2 may be moved to O/a2, but we don't care)
        remote.rename(prefix + "/A/a2", prefix + "/a2");
        remote.remove("A/a2");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(nGET, 0);

        // Local change plus remote move at the same time
        fakeFolder.localModifier().appendByte(prefix + "/a2");
        remote.rename(prefix + "/a2", prefix + "/a3");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(nGET, 1);
    }

    void testMovePropagation()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto &local = fakeFolder.localModifier();
        auto &remote = fakeFolder.remoteModifier();

        OperationCounter counter;
        fakeFolder.setServerOverride(counter.functor());

        // Move
        {
            counter.reset();
            local.rename("A/a1", "A/a1m");
            remote.rename("B/b1", "B/b1m");
            QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
            QVERIFY(fakeFolder.syncOnce());
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
            QCOMPARE(counter.nGET, 0);
            QCOMPARE(counter.nPUT, 0);
            QCOMPARE(counter.nMOVE, 1);
            QCOMPARE(counter.nDELETE, 0);
            QVERIFY(itemSuccessfulMove(completeSpy, "A/a1m"));
            QVERIFY(itemSuccessfulMove(completeSpy, "B/b1m"));
        }

        // Touch+Move on same side
        counter.reset();
        local.rename("A/a2", "A/a2m");
        local.setContents("A/a2m", 'A');
        remote.rename("B/b2", "B/b2m");
        remote.setContents("B/b2m", 'A');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        QCOMPARE(counter.nGET, 1);
        QCOMPARE(counter.nPUT, 1);
        QCOMPARE(counter.nMOVE, 0);
        QCOMPARE(counter.nDELETE, 1);
        QCOMPARE(remote.find("A/a2m")->contentChar, 'A');
        QCOMPARE(remote.find("B/b2m")->contentChar, 'A');

        // Touch+Move on opposite sides
        counter.reset();
        local.rename("A/a1m", "A/a1m2");
        remote.setContents("A/a1m", 'B');
        remote.rename("B/b1m", "B/b1m2");
        local.setContents("B/b1m", 'B');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        QCOMPARE(counter.nGET, 2);
        QCOMPARE(counter.nPUT, 2);
        QCOMPARE(counter.nMOVE, 0);
        QCOMPARE(counter.nDELETE, 0);
        // All these files existing afterwards is debatable. Should we propagate
        // the rename in one direction and grab the new contents in the other?
        // Currently there's no propagation job that would do that, and this does
        // at least not lose data.
        QCOMPARE(remote.find("A/a1m")->contentChar, 'B');
        QCOMPARE(remote.find("B/b1m")->contentChar, 'B');
        QCOMPARE(remote.find("A/a1m2")->contentChar, 'W');
        QCOMPARE(remote.find("B/b1m2")->contentChar, 'W');

        // Touch+create on one side, move on the other
        {
            counter.reset();
            local.appendByte("A/a1m");
            local.insert("A/a1mt");
            remote.rename("A/a1m", "A/a1mt");
            remote.appendByte("B/b1m");
            remote.insert("B/b1mt");
            local.rename("B/b1m", "B/b1mt");
            QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
            QVERIFY(fakeFolder.syncOnce());
            QVERIFY(expectAndWipeConflict(local, fakeFolder.currentLocalState(), "A/a1mt"));
            QVERIFY(expectAndWipeConflict(local, fakeFolder.currentLocalState(), "B/b1mt"));
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
            QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
            QCOMPARE(counter.nGET, 3);
            QCOMPARE(counter.nPUT, 1);
            QCOMPARE(counter.nMOVE, 0);
            QCOMPARE(counter.nDELETE, 0);
            QVERIFY(itemSuccessful(completeSpy, "A/a1m", CSYNC_INSTRUCTION_NEW));
            QVERIFY(itemSuccessful(completeSpy, "B/b1m", CSYNC_INSTRUCTION_NEW));
            QVERIFY(itemConflict(completeSpy, "A/a1mt"));
            QVERIFY(itemConflict(completeSpy, "B/b1mt"));
        }

        // Create new on one side, move to new on the other
        {
            counter.reset();
            local.insert("A/a1N", 13);
            remote.rename("A/a1mt", "A/a1N");
            remote.insert("B/b1N", 13);
            local.rename("B/b1mt", "B/b1N");
            QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
            QVERIFY(fakeFolder.syncOnce());
            QVERIFY(expectAndWipeConflict(local, fakeFolder.currentLocalState(), "A/a1N"));
            QVERIFY(expectAndWipeConflict(local, fakeFolder.currentLocalState(), "B/b1N"));
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
            QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
            QCOMPARE(counter.nGET, 2);
            QCOMPARE(counter.nPUT, 0);
            QCOMPARE(counter.nMOVE, 0);
            QCOMPARE(counter.nDELETE, 1);
            QVERIFY(itemSuccessful(completeSpy, "A/a1mt", CSYNC_INSTRUCTION_REMOVE));
            QVERIFY(itemSuccessful(completeSpy, "B/b1mt", CSYNC_INSTRUCTION_REMOVE));
            QVERIFY(itemConflict(completeSpy, "A/a1N"));
            QVERIFY(itemConflict(completeSpy, "B/b1N"));
        }

        // Local move, remote move
        counter.reset();
        local.rename("C/c1", "C/c1mL");
        remote.rename("C/c1", "C/c1mR");
        QVERIFY(fakeFolder.syncOnce());
        // end up with both files
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        QCOMPARE(counter.nGET, 1);
        QCOMPARE(counter.nPUT, 1);
        QCOMPARE(counter.nMOVE, 0);
        QCOMPARE(counter.nDELETE, 0);

        // Rename/rename conflict on a folder
        counter.reset();
        remote.rename("C", "CMR");
        local.rename("C", "CML");
        QVERIFY(fakeFolder.syncOnce());
        // End up with both folders
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        QCOMPARE(counter.nGET, 3); // 3 files in C
        QCOMPARE(counter.nPUT, 3);
        QCOMPARE(counter.nMOVE, 0);
        QCOMPARE(counter.nDELETE, 0);

        // Folder move
        {
            counter.reset();
            local.rename("A", "AM");
            remote.rename("B", "BM");
            QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
            QVERIFY(fakeFolder.syncOnce());
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
            QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
            QCOMPARE(counter.nGET, 0);
            QCOMPARE(counter.nPUT, 0);
            QCOMPARE(counter.nMOVE, 1);
            QCOMPARE(counter.nDELETE, 0);
            QVERIFY(itemSuccessfulMove(completeSpy, "AM"));
            QVERIFY(itemSuccessfulMove(completeSpy, "BM"));
        }

        // Folder move with contents touched on the same side
        {
            counter.reset();
            local.setContents("AM/a2m", 'C');
            // We must change the modtime for it is likely that it did not change between sync.
            // (Previous version of the client (<=2.5) would not need this because it was always doing
            // checksum comparison for all renames. But newer version no longer does it if the file is
            // renamed because the parent folder is renamed)
            local.setModTime("AM/a2m", QDateTime::currentDateTimeUtc().addDays(3));
            local.rename("AM", "A2");
            remote.setContents("BM/b2m", 'C');
            remote.rename("BM", "B2");
            QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
            QVERIFY(fakeFolder.syncOnce());
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
            QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
            QCOMPARE(counter.nGET, 1);
            QCOMPARE(counter.nPUT, 1);
            QCOMPARE(counter.nMOVE, 1);
            QCOMPARE(counter.nDELETE, 0);
            QCOMPARE(remote.find("A2/a2m")->contentChar, 'C');
            QCOMPARE(remote.find("B2/b2m")->contentChar, 'C');
            QVERIFY(itemSuccessfulMove(completeSpy, "A2"));
            QVERIFY(itemSuccessfulMove(completeSpy, "B2"));
        }

        // Folder rename with contents touched on the other tree
        counter.reset();
        remote.setContents("A2/a2m", 'D');
        // setContents alone may not produce updated mtime if the test is fast
        // and since we don't use checksums here, that matters.
        remote.appendByte("A2/a2m");
        local.rename("A2", "A3");
        local.setContents("B2/b2m", 'D');
        local.appendByte("B2/b2m");
        remote.rename("B2", "B3");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        QCOMPARE(counter.nGET, 1);
        QCOMPARE(counter.nPUT, 1);
        QCOMPARE(counter.nMOVE, 1);
        QCOMPARE(counter.nDELETE, 0);
        QCOMPARE(remote.find("A3/a2m")->contentChar, 'D');
        QCOMPARE(remote.find("B3/b2m")->contentChar, 'D');

        // Folder rename with contents touched on both ends
        counter.reset();
        remote.setContents("A3/a2m", 'R');
        remote.appendByte("A3/a2m");
        local.setContents("A3/a2m", 'L');
        local.appendByte("A3/a2m");
        local.appendByte("A3/a2m");
        local.rename("A3", "A4");
        remote.setContents("B3/b2m", 'R');
        remote.appendByte("B3/b2m");
        local.setContents("B3/b2m", 'L');
        local.appendByte("B3/b2m");
        local.appendByte("B3/b2m");
        remote.rename("B3", "B4");
        QVERIFY(fakeFolder.syncOnce());
        auto currentLocal = fakeFolder.currentLocalState();
        auto conflicts = findConflicts(currentLocal.children["A4"]);
        QCOMPARE(conflicts.size(), 1);
        for (auto c : conflicts) {
            QCOMPARE(currentLocal.find(c)->contentChar, 'L');
            local.remove(c);
        }
        conflicts = findConflicts(currentLocal.children["B4"]);
        QCOMPARE(conflicts.size(), 1);
        for (auto c : conflicts) {
            QCOMPARE(currentLocal.find(c)->contentChar, 'L');
            local.remove(c);
        }
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        QCOMPARE(counter.nGET, 2);
        QCOMPARE(counter.nPUT, 0);
        QCOMPARE(counter.nMOVE, 1);
        QCOMPARE(counter.nDELETE, 0);
        QCOMPARE(remote.find("A4/a2m")->contentChar, 'R');
        QCOMPARE(remote.find("B4/b2m")->contentChar, 'R');

        // Rename a folder and rename the contents at the same time
        counter.reset();
        local.rename("A4/a2m", "A4/a2m2");
        local.rename("A4", "A5");
        remote.rename("B4/b2m", "B4/b2m2");
        remote.rename("B4", "B5");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);
        QCOMPARE(counter.nMOVE, 2);
        QCOMPARE(counter.nDELETE, 0);
    }

    // Check interaction of moves with file type changes
    void testMoveAndTypeChange()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        auto &local = fakeFolder.localModifier();
        auto &remote = fakeFolder.remoteModifier();

        // Touch on one side, rename and mkdir on the other
        {
            local.appendByte("A/a1");
            remote.rename("A/a1", "A/a1mq");
            remote.mkdir("A/a1");
            remote.appendByte("B/b1");
            local.rename("B/b1", "B/b1mq");
            local.mkdir("B/b1");
            QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
            QVERIFY(fakeFolder.syncOnce());
            // BUG: This doesn't behave right
            //QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        }
    }

    // https://github.com/owncloud/client/issues/6629#issuecomment-402450691
    // When a file is moved and the server mtime was not in sync, the local mtime should be kept
    void testMoveAndMTimeChange()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        OperationCounter counter;
        fakeFolder.setServerOverride(counter.functor());

        // Changing the mtime on the server (without invalidating the etag)
        fakeFolder.remoteModifier().find("A/a1")->lastModified = QDateTime::currentDateTimeUtc().addSecs(-50000);
        fakeFolder.remoteModifier().find("A/a2")->lastModified = QDateTime::currentDateTimeUtc().addSecs(-40000);

        // Move a few files
        fakeFolder.remoteModifier().rename("A/a1", "A/a1_server_renamed");
        fakeFolder.localModifier().rename("A/a2", "A/a2_local_renamed");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);
        QCOMPARE(counter.nMOVE, 1);
        QCOMPARE(counter.nDELETE, 0);

        // Another sync should do nothing
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);
        QCOMPARE(counter.nMOVE, 1);
        QCOMPARE(counter.nDELETE, 0);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Test for https://github.com/owncloud/client/issues/6694
    void testInvertFolderHierarchy()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.remoteModifier().mkdir("A/Empty");
        fakeFolder.remoteModifier().mkdir("A/Empty/Foo");
        fakeFolder.remoteModifier().mkdir("C/AllEmpty");
        fakeFolder.remoteModifier().mkdir("C/AllEmpty/Bar");
        fakeFolder.remoteModifier().insert("A/Empty/f1");
        fakeFolder.remoteModifier().insert("A/Empty/Foo/f2");
        fakeFolder.remoteModifier().mkdir("C/AllEmpty/f3");
        fakeFolder.remoteModifier().mkdir("C/AllEmpty/Bar/f4");
        QVERIFY(fakeFolder.syncOnce());

        OperationCounter counter;
        fakeFolder.setServerOverride(counter.functor());

        // "Empty" is after "A", alphabetically
        fakeFolder.localModifier().rename("A/Empty", "Empty");
        fakeFolder.localModifier().rename("A", "Empty/A");

        // "AllEmpty" is before "C", alphabetically
        fakeFolder.localModifier().rename("C/AllEmpty", "AllEmpty");
        fakeFolder.localModifier().rename("C", "AllEmpty/C");

        auto expectedState = fakeFolder.currentLocalState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
        QCOMPARE(counter.nDELETE, 0);
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);

        // Now, the revert, but "crossed"
        fakeFolder.localModifier().rename("Empty/A", "A");
        fakeFolder.localModifier().rename("AllEmpty/C", "C");
        fakeFolder.localModifier().rename("Empty", "C/Empty");
        fakeFolder.localModifier().rename("AllEmpty", "A/AllEmpty");
        expectedState = fakeFolder.currentLocalState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
        QCOMPARE(counter.nDELETE, 0);
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);

        // Reverse on remote
        fakeFolder.remoteModifier().rename("A/AllEmpty", "AllEmpty");
        fakeFolder.remoteModifier().rename("C/Empty", "Empty");
        fakeFolder.remoteModifier().rename("C", "AllEmpty/C");
        fakeFolder.remoteModifier().rename("A", "Empty/A");
        expectedState = fakeFolder.currentRemoteState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
        QCOMPARE(counter.nDELETE, 0);
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);
    }

    void testDeepHierarchy_data()
    {
        QTest::addColumn<bool>("local");
        QTest::newRow("remote") << false;
        QTest::newRow("local") << true;
    }

    void testDeepHierarchy()
    {
        QFETCH(bool, local);
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        auto &modifier = local ? fakeFolder.localModifier() : fakeFolder.remoteModifier();

        modifier.mkdir("FolA");
        modifier.mkdir("FolA/FolB");
        modifier.mkdir("FolA/FolB/FolC");
        modifier.mkdir("FolA/FolB/FolC/FolD");
        modifier.mkdir("FolA/FolB/FolC/FolD/FolE");
        modifier.insert("FolA/FileA.txt");
        modifier.insert("FolA/FolB/FileB.txt");
        modifier.insert("FolA/FolB/FolC/FileC.txt");
        modifier.insert("FolA/FolB/FolC/FolD/FileD.txt");
        modifier.insert("FolA/FolB/FolC/FolD/FolE/FileE.txt");
        QVERIFY(fakeFolder.syncOnce());

        OperationCounter counter;
        fakeFolder.setServerOverride(counter.functor());

        modifier.insert("FolA/FileA2.txt");
        modifier.insert("FolA/FolB/FileB2.txt");
        modifier.insert("FolA/FolB/FolC/FileC2.txt");
        modifier.insert("FolA/FolB/FolC/FolD/FileD2.txt");
        modifier.insert("FolA/FolB/FolC/FolD/FolE/FileE2.txt");
        modifier.rename("FolA", "FolA_Renamed");
        modifier.rename("FolA_Renamed/FolB", "FolB_Renamed");
        modifier.rename("FolB_Renamed/FolC", "FolA");
        modifier.rename("FolA/FolD", "FolA/FolD_Renamed");
        modifier.mkdir("FolB_Renamed/New");
        modifier.rename("FolA/FolD_Renamed/FolE", "FolB_Renamed/New/FolE");
        auto expected = local ? fakeFolder.currentLocalState() : fakeFolder.currentRemoteState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expected);
        QCOMPARE(fakeFolder.currentRemoteState(), expected);
        QCOMPARE(counter.nDELETE, local ? 1 : 0); // FolC was is renamed to an existing name, so it is not considered as renamed
        // There was 5 inserts
        QCOMPARE(counter.nGET, local ? 0 : 5);
        QCOMPARE(counter.nPUT, local ? 5 : 0);
    }

    void renameOnBothSides()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        OperationCounter counter;
        fakeFolder.setServerOverride(counter.functor());

        // Test that renaming a file within a directory that was renamed on the other side actually do a rename.

        // 1) move the folder alphabeticaly before
        fakeFolder.remoteModifier().rename("A/a1", "A/a1m");
        fakeFolder.localModifier().rename("A", "_A");
        fakeFolder.localModifier().rename("B/b1", "B/b1m");
        fakeFolder.remoteModifier().rename("B", "_B");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentRemoteState(), fakeFolder.currentRemoteState());
        QVERIFY(fakeFolder.currentRemoteState().find("_A/a1m"));
        QVERIFY(fakeFolder.currentRemoteState().find("_B/b1m"));
        QCOMPARE(counter.nDELETE, 0);
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);
        QCOMPARE(counter.nMOVE, 2);
        counter.reset();

        // 2) move alphabetically after
        fakeFolder.remoteModifier().rename("_A/a2", "_A/a2m");
        fakeFolder.localModifier().rename("_B/b2", "_B/b2m");
        fakeFolder.localModifier().rename("_A", "S/A");
        fakeFolder.remoteModifier().rename("_B", "S/B");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentRemoteState(), fakeFolder.currentRemoteState());
        QVERIFY(fakeFolder.currentRemoteState().find("S/A/a2m"));
        QVERIFY(fakeFolder.currentRemoteState().find("S/B/b2m"));
        QCOMPARE(counter.nDELETE, 0);
        QCOMPARE(counter.nGET, 0);
        QCOMPARE(counter.nPUT, 0);
        QCOMPARE(counter.nMOVE, 2);
    }
};

QTEST_GUILESS_MAIN(TestSyncMove)
#include "testsyncmove.moc"
