/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <syncengine.h>

#include "testutils/syncenginetestutils.h"
#include "testutils/testutils.h"

#include <QtTest>

using namespace OCC;

namespace {
auto uploadConflictFilesCapabilities(bool b)
{
    auto cap = TestUtils::testCapabilities();
    cap.insert({{QStringLiteral("uploadConflictFiles"), b}});
    return cap;
}

bool itemSuccessful(const ItemCompletedSpy &spy, const QString &path, const SyncInstructions instr)
{
    auto item = spy.findItem(path);
    Q_ASSERT(item);
    return item->_status == SyncFileItem::Success && item->instruction() == instr;
}

bool itemConflict(const ItemCompletedSpy &spy, const QString &path)
{
    auto item = spy.findItem(path);
    Q_ASSERT(item);
    return item->_status == SyncFileItem::Conflict && item->instruction() == CSYNC_INSTRUCTION_CONFLICT;
}

QStringList findConflicts(const FileInfo &dir)
{
    QStringList conflicts;
    for (const auto &item : dir.children) {
        if (item.name.contains(QLatin1String("(conflicted copy"))) {
            conflicts.append(item.path());
        }
    }
    return conflicts;
}

bool expectAndWipeConflict(FakeFolder &fFolder, const QString &path)
{
    PathComponents pathComponents(path);
    auto localState = fFolder.currentLocalState();
    auto base = localState.find(pathComponents.parentDirComponents());
    if (!base) {
        return false;
    }

    for (const auto &item : qAsConst(base->children)) {
        if (item.name.startsWith(pathComponents.fileName()) && item.name.contains(QLatin1String("(conflicted copy"))) {
            fFolder.localModifier().remove(item.path());
            OC_ASSERT(fFolder.applyLocalModificationsWithoutSync());
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

}

class TestSyncConflict : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase_data()
    {
        QTest::addColumn<Vfs::Mode>("vfsMode");
        QTest::addColumn<bool>("filesAreDehydrated");

        QTest::newRow("Vfs::Off") << Vfs::Off << false;

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi dehydrated") << Vfs::WindowsCfApi << true;
            QTest::newRow("Vfs::WindowsCfApi hydrated") << Vfs::WindowsCfApi << false;
        } else if (Utility::isWindows()) {
            qWarning("Skipping Vfs::WindowsCfApi");
        }
    }

    void testNoUpload()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().setContents(QStringLiteral("A/a1"), FileModifier::DefaultFileSize, 'L');
        fakeFolder.remoteModifier().setContents(QStringLiteral("A/a1"), FileModifier::DefaultFileSize, 'R');
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        const auto &conflicts = findConflicts(fakeFolder.currentLocalState().children[QStringLiteral("A")]);
        if (filesAreDehydrated) {
            // There should be no conflicts: before a modification is done to a local file,
            // it will be downloaded from the remote first.
            QCOMPARE(conflicts.size(), 0);
        } else {
            // Verify that the conflict names don't have the user name
            for (const auto &name : conflicts) {
                QVERIFY(!name.contains(fakeFolder.syncEngine().account()->davDisplayName()));
            }

            QVERIFY(expectAndWipeConflict(fakeFolder, QStringLiteral("A/a1")));
            QVERIFY(expectAndWipeConflict(fakeFolder, QStringLiteral("A/a2")));
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        }
    }

    void testUploadAfterDownload()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), uploadConflictFilesCapabilities(true)});
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QMap<QByteArray, QString> conflictMap;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                if (request.rawHeader("OC-Conflict") == "1") {
                    auto baseFileId = request.rawHeader("OC-ConflictBaseFileId");
                    auto components = request.url().toString().split(QLatin1Char('/'));
                    QString conflictFile = components.mid(components.size() - 2).join(QLatin1Char('/'));
                    conflictMap[baseFileId] = conflictFile;
                    [&] {
                        QVERIFY(!baseFileId.isEmpty());
                        QCOMPARE(request.rawHeader("OC-ConflictInitialBasePath"),
                                 Utility::conflictFileBaseNameFromPattern(conflictFile.toUtf8()));
                    }();
                }
            }
            return nullptr;
        });

        fakeFolder.localModifier().setContents(QStringLiteral("A/a1"), FileModifier::DefaultFileSize, 'L');
        fakeFolder.remoteModifier().setContents(QStringLiteral("A/a1"), FileModifier::DefaultFileSize, 'R');
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        if (filesAreDehydrated) {
            // There should be no conflicts: before a modification is done to a local file,
            // it will be downloaded from the remote first.
            QCOMPARE(conflictMap.size(), 0);
        } else {
            auto local = fakeFolder.currentLocalState();
            auto remote = fakeFolder.currentRemoteState();
            QCOMPARE(local, remote);

            auto a1FileId = fakeFolder.remoteModifier().find(QStringLiteral("A/a1"))->fileId;
            auto a2FileId = fakeFolder.remoteModifier().find(QStringLiteral("A/a2"))->fileId;
            QVERIFY(conflictMap.contains(a1FileId));
            QVERIFY(conflictMap.contains(a2FileId));
            QCOMPARE(conflictMap.size(), 2);
            QCOMPARE(Utility::conflictFileBaseNameFromPattern(conflictMap[a1FileId].toUtf8()), QByteArray("A/a1"));

            // Check that the conflict file contains the username
            QVERIFY(conflictMap[a1FileId].contains(QString::fromLatin1("(conflicted copy %1 ").arg(fakeFolder.syncEngine().account()->davDisplayName())));

            QCOMPARE(remote.find(conflictMap[a1FileId])->contentChar, 'L');
            QCOMPARE(remote.find(QStringLiteral("A/a1"))->contentChar, 'R');

            QCOMPARE(remote.find(conflictMap[a2FileId])->contentSize, 5);
            QCOMPARE(remote.find(QStringLiteral("A/a2"))->contentSize, 6);
        }
    }

    void testSeparateUpload()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), uploadConflictFilesCapabilities(true)});
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QMap<QByteArray, QString> conflictMap;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                if (request.rawHeader("OC-Conflict") == "1") {
                    auto baseFileId = request.rawHeader("OC-ConflictBaseFileId");
                    auto components = request.url().toString().split(QLatin1Char('/'));
                    QString conflictFile = components.mid(components.size() - 2).join(QLatin1Char('/'));
                    conflictMap[baseFileId] = conflictFile;
                    [&] {
                        QVERIFY(!baseFileId.isEmpty());
                        QCOMPARE(request.rawHeader("OC-ConflictInitialBasePath"),
                                 Utility::conflictFileBaseNameFromPattern(conflictFile.toUtf8()));
                    }();
                }
            }
            return nullptr;
        });

        // Explicitly add a conflict file to simulate the case where the upload of the
        // file didn't finish in the same sync run that the conflict was created.
        // To do that we need to create a mock conflict record.
        auto a1FileId = fakeFolder.remoteModifier().find(QStringLiteral("A/a1"))->fileId;
        QString conflictName = QStringLiteral("A/a1 (conflicted copy me 1234)");
        fakeFolder.localModifier().insert(conflictName, FileModifier::DefaultFileSize, 'L');
        ConflictRecord conflictRecord;
        conflictRecord.path = conflictName.toUtf8();
        conflictRecord.baseFileId = a1FileId;
        conflictRecord.initialBasePath = "A/a1";
        fakeFolder.syncJournal().setConflictRecord(conflictRecord);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(conflictMap.size(), 1);
        QCOMPARE(conflictMap[a1FileId], conflictName);
        QCOMPARE(fakeFolder.currentRemoteState().find(conflictMap[a1FileId])->contentChar, 'L');
        conflictMap.clear();

        // Now the user can locally alter the conflict file and it will be uploaded
        // as usual.
        fakeFolder.localModifier().setContents(conflictName, FileModifier::DefaultFileSize + 1, 'P'); // make sure the file sizes are different
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(conflictMap.size(), 1);
        QCOMPARE(conflictMap[a1FileId], conflictName);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        conflictMap.clear();

        // Similarly, remote modifications of conflict files get propagated downwards
        fakeFolder.remoteModifier().setContents(conflictName, FileModifier::DefaultFileSize + 1, 'Q'); // make sure the file sizes are different
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(conflictMap.isEmpty());

        // Conflict files for conflict files!
        auto a1ConflictFileId = fakeFolder.remoteModifier().find(conflictName)->fileId;
        fakeFolder.remoteModifier().appendByte(conflictName);
        fakeFolder.remoteModifier().appendByte(conflictName);
        fakeFolder.localModifier().appendByte(conflictName);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(conflictMap.size(), 1);
        QVERIFY(conflictMap.contains(a1ConflictFileId));
        QCOMPARE(fakeFolder.currentRemoteState().find(conflictName)->contentSize, FileModifier::DefaultFileSize + 3);
        QCOMPARE(fakeFolder.currentRemoteState().find(conflictMap[a1ConflictFileId])->contentSize, FileModifier::DefaultFileSize + 2);
        conflictMap.clear();
    }

    // What happens if we download a conflict file? Is the metadata set up correctly?
    void testDownloadingConflictFile()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), uploadConflictFilesCapabilities(true)});
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // With no headers from the server
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a1 (conflicted copy 1234)"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto conflictRecord = fakeFolder.syncJournal().conflictRecord("A/a1 (conflicted copy 1234)");
        QVERIFY(conflictRecord.isValid());
        QCOMPARE(conflictRecord.baseFileId, fakeFolder.remoteModifier().find(QStringLiteral("A/a1"))->fileId);
        QCOMPARE(conflictRecord.initialBasePath, QByteArray("A/a1"));

        // Now with server headers
        QObject parent;
        auto a2FileId = fakeFolder.remoteModifier().find(QStringLiteral("A/a2"))->fileId;
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
        fakeFolder.remoteModifier().insert(QStringLiteral("A/really-a-conflict")); // doesn't look like a conflict, but headers say it is
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        conflictRecord = fakeFolder.syncJournal().conflictRecord("A/really-a-conflict");

        if (filesAreDehydrated) {
            // A placeholder for the conflicting file is created, but no actual GET request is made, so there should be no conflict record
            QVERIFY(!conflictRecord.isValid());
        } else {
            QVERIFY(conflictRecord.isValid());
            QCOMPARE(conflictRecord.baseFileId, a2FileId);
            QCOMPARE(conflictRecord.baseModtime, 1234);
            QCOMPARE(conflictRecord.baseEtag, QByteArray("etag"));
            QCOMPARE(conflictRecord.initialBasePath, QByteArray("A/original"));
        }
    }

    // Check that conflict records are removed when the file is gone
    void testConflictRecordRemoval1()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), uploadConflictFilesCapabilities(true)});
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Make conflict records
        ConflictRecord conflictRecord;
        conflictRecord.path = "A/a1";
        fakeFolder.syncJournal().setConflictRecord(conflictRecord);
        conflictRecord.path = "A/a2";
        fakeFolder.syncJournal().setConflictRecord(conflictRecord);

        // A nothing-to-sync keeps them alive
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(fakeFolder.syncJournal().conflictRecord("A/a1").isValid());
        QVERIFY(fakeFolder.syncJournal().conflictRecord("A/a2").isValid());

        // When the file is removed, the record is removed too
        fakeFolder.localModifier().remove(QStringLiteral("A/a2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QVERIFY(fakeFolder.syncJournal().conflictRecord("A/a1").isValid());
        QVERIFY(!fakeFolder.syncJournal().conflictRecord("A/a2").isValid());
    }

    // Same test, but with uploadConflictFiles == false
    void testConflictRecordRemoval2()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), uploadConflictFilesCapabilities(false)});
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Create two conflicts
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        const auto &conflicts = findConflicts(fakeFolder.currentLocalState().children[QStringLiteral("A")]);
        if (filesAreDehydrated) {
            // There should be no conflicts: before a modification is done to a local file,
            // it will be downloaded from the remote first.
            QCOMPARE(conflicts.size(), 0);
        } else {
            QString a1conflict;
            QString a2conflict;
            for (const auto &conflict : conflicts) {
                if (conflict.contains(QLatin1String("a1")))
                    a1conflict = conflict;
                if (conflict.contains(QLatin1String("a2")))
                    a2conflict = conflict;
            }

            // A nothing-to-sync keeps them alive
            QVERIFY(fakeFolder.applyLocalModificationsAndSync());
            QVERIFY(fakeFolder.syncJournal().conflictRecord(a1conflict.toUtf8()).isValid());
            QVERIFY(fakeFolder.syncJournal().conflictRecord(a2conflict.toUtf8()).isValid());

            // When the file is removed, the record is removed too
            fakeFolder.localModifier().remove(a2conflict);
            QVERIFY(fakeFolder.applyLocalModificationsAndSync());
            QVERIFY(fakeFolder.syncJournal().conflictRecord(a1conflict.toUtf8()).isValid());
            QVERIFY(!fakeFolder.syncJournal().conflictRecord(a2conflict.toUtf8()).isValid());
        }
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
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), uploadConflictFilesCapabilities(true)});
        ItemCompletedSpy completeSpy(fakeFolder);

        auto cleanup = [&]() {
            completeSpy.clear();
        };
        cleanup();

        // 1) a NEW/NEW conflict
        fakeFolder.localModifier().mkdir(QStringLiteral("Z"));
        fakeFolder.localModifier().mkdir(QStringLiteral("Z/subdir"));
        fakeFolder.localModifier().insert(QStringLiteral("Z/foo"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Z"), 63_B);

        // 2) local file becomes a dir; remote file changes
        fakeFolder.localModifier().remove(QStringLiteral("A/a1"));
        fakeFolder.localModifier().mkdir(QStringLiteral("A/a1"));
        fakeFolder.localModifier().insert(QStringLiteral("A/a1/bar"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));

        // 3) local dir gets a new file; remote dir becomes a file
        fakeFolder.localModifier().insert(QStringLiteral("B/zzz"));
        fakeFolder.remoteModifier().remove(QStringLiteral("B"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B"), 31_B);

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        auto conflicts = findConflicts(fakeFolder.currentLocalState());
        conflicts += findConflicts(fakeFolder.currentLocalState().children[QStringLiteral("A")]);
        QCOMPARE(conflicts.size(), 3);
        std::sort(conflicts.begin(), conflicts.end());

        auto conflictRecords = fakeFolder.syncJournal().conflictRecordPaths();
        QCOMPARE(conflictRecords.size(), 3);
        std::sort(conflictRecords.begin(), conflictRecords.end());

        // 1)
        QVERIFY(itemConflict(completeSpy, QStringLiteral("Z")));
        QCOMPARE(fakeFolder.currentLocalState().find(QStringLiteral("Z"))->contentSize, 63);
        QVERIFY(conflicts[2].contains(QStringLiteral("Z")));
        QCOMPARE(conflicts[2].toUtf8(), conflictRecords[2]);
        QVERIFY(QFileInfo(fakeFolder.localPath() + conflicts[2]).isDir());
        QVERIFY(QFile::exists(fakeFolder.localPath() + conflicts[2] + QStringLiteral("/foo")));

        // 2)
        QVERIFY(itemConflict(completeSpy, QStringLiteral("A/a1")));
        QCOMPARE(fakeFolder.currentLocalState().find(QStringLiteral("A/a1"))->contentSize, 5);
        QVERIFY(conflicts[0].contains(QStringLiteral("A/a1")));
        QCOMPARE(conflicts[0].toUtf8(), conflictRecords[0]);
        QVERIFY(QFileInfo(fakeFolder.localPath() + conflicts[0]).isDir());
        QVERIFY(QFile::exists(fakeFolder.localPath() + conflicts[0] + QStringLiteral("/bar")));

        // 3)
        QVERIFY(itemConflict(completeSpy, QStringLiteral("B")));
        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.vfs()->isDehydratedPlaceholder(fakeFolder.localPath() + QStringLiteral("B")));
        } else {
            QCOMPARE(fakeFolder.currentLocalState().find(QStringLiteral("B"))->contentSize, 31);
        }
        QVERIFY(conflicts[1].contains(QStringLiteral("B")));
        QCOMPARE(conflicts[1].toUtf8(), conflictRecords[1]);
        QVERIFY(QFileInfo(fakeFolder.localPath() + conflicts[1]).isDir());
        QVERIFY(QFile::exists(fakeFolder.localPath() + conflicts[1] + QStringLiteral("/zzz")));

        // The contents of the conflict directories will only be uploaded after
        // another sync.
        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), true);
        cleanup();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(itemSuccessful(completeSpy, conflicts[0], CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[0] + QStringLiteral("/bar"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[1], CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[1] + QStringLiteral("/zzz"), CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[2], CSYNC_INSTRUCTION_NEW));
        QVERIFY(itemSuccessful(completeSpy, conflicts[2] + QStringLiteral("/foo"), CSYNC_INSTRUCTION_NEW));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testLocalFileRemoteDirConflict()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), uploadConflictFilesCapabilities(true)});
        ItemCompletedSpy completeSpy(fakeFolder);

        // 1) a NEW/NEW conflict
        fakeFolder.remoteModifier().mkdir(QStringLiteral("Z"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("Z/subdir"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Z/foo"));
        fakeFolder.localModifier().insert(QStringLiteral("Z"));

        // 2) local dir becomes file: remote dir adds file
        fakeFolder.localModifier().remove(QStringLiteral("A"));
        fakeFolder.localModifier().insert(QStringLiteral("A"), 63_B);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/bar"));

        // 3) local file changes; remote file becomes dir
        fakeFolder.localModifier().appendByte(QStringLiteral("B/b1"));
        fakeFolder.remoteModifier().remove(QStringLiteral("B/b1"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("B/b1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B/b1/zzz"));

        if (filesAreDehydrated) {
            // the dehydrating the placeholder failed as the metadata is out of sync
            QSignalSpy spy(fakeFolder.vfs().get(), &Vfs::needSync);
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
            QVERIFY(spy.count() == 1);
            QVERIFY(fakeFolder.syncOnce());
            // reapply change and try again
            fakeFolder.localModifier().appendByte(QStringLiteral("B/b1"));
            // writing to a file fails!
            QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
            // TODO: check error type
            return;
        }
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        auto conflicts = findConflicts(fakeFolder.currentLocalState());
        conflicts += findConflicts(fakeFolder.currentLocalState().children[QStringLiteral("B")]);
        QCOMPARE(conflicts.size(), 3);
        std::sort(conflicts.begin(), conflicts.end());

        auto conflictRecords = fakeFolder.syncJournal().conflictRecordPaths();
        QCOMPARE(conflictRecords.size(), 3);
        std::sort(conflictRecords.begin(), conflictRecords.end());

        // 1)
        QVERIFY(itemConflict(completeSpy, QStringLiteral("Z")));
        QVERIFY(conflicts[2].contains(QStringLiteral("Z")));
        QCOMPARE(conflicts[2].toUtf8(), conflictRecords[2]);

        // 2)
        QVERIFY(itemConflict(completeSpy, QStringLiteral("A")));
        QVERIFY(conflicts[0].contains(QStringLiteral("A")));
        QCOMPARE(conflicts[0].toUtf8(), conflictRecords[0]);

        // 3)
        QVERIFY(itemConflict(completeSpy, QStringLiteral("B/b1")));
        QVERIFY(conflicts[1].contains(QStringLiteral("B/b1")));
        QCOMPARE(conflicts[1].toUtf8(), conflictRecords[1]);

        // Also verifies that conflicts were uploaded
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testTypeConflictWithMove()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);

        // the remote becomes a file, but a file inside the dir has moved away!
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A"));
        fakeFolder.localModifier().rename(QStringLiteral("A/a1"), QStringLiteral("a1"));

        // same, but with a new file inside the dir locally
        fakeFolder.remoteModifier().remove(QStringLiteral("B"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B"));
        fakeFolder.localModifier().rename(QStringLiteral("B/b1"), QStringLiteral("b1"));
        fakeFolder.localModifier().insert(QStringLiteral("B/new"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(itemSuccessful(completeSpy, QStringLiteral("A"), CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemConflict(completeSpy, QStringLiteral("B")));

        auto conflicts = findConflicts(fakeFolder.currentLocalState());
        std::sort(conflicts.begin(), conflicts.end());
        QVERIFY(conflicts.size() == 2);
        QVERIFY(conflicts[0].contains(QStringLiteral("A (conflicted copy")));
        QVERIFY(conflicts[1].contains(QStringLiteral("B (conflicted copy")));
        for (const auto &conflict : qAsConst(conflicts))
            QDir(fakeFolder.localPath() + conflict).removeRecursively();
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Currently a1 and b1 don't get moved, but redownloaded
    }

    void testTypeChange()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);

        // dir becomes file
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.remoteModifier().insert(QStringLiteral("A"));
        fakeFolder.localModifier().remove(QStringLiteral("B"));
        fakeFolder.localModifier().insert(QStringLiteral("B"));

        // file becomes dir
        fakeFolder.remoteModifier().remove(QStringLiteral("C/c1"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("C/c1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("C/c1/foo"));
        fakeFolder.localModifier().remove(QStringLiteral("C/c2"));
        fakeFolder.localModifier().mkdir(QStringLiteral("C/c2"));
        fakeFolder.localModifier().insert(QStringLiteral("C/c2/bar"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QVERIFY(itemSuccessful(completeSpy, QStringLiteral("A"), CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemSuccessful(completeSpy, QStringLiteral("B"), CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemSuccessful(completeSpy, QStringLiteral("C/c1"), CSYNC_INSTRUCTION_TYPE_CHANGE));
        QVERIFY(itemSuccessful(completeSpy, QStringLiteral("C/c2"), CSYNC_INSTRUCTION_TYPE_CHANGE));

        // A becomes a conflict because we don't delete folders with files
        // inside of them!
        const auto &conflicts = findConflicts(fakeFolder.currentLocalState());
        QVERIFY(conflicts.size() == 1);
        QVERIFY(conflicts[0].contains(QStringLiteral("A (conflicted copy")));
        for (const auto &conflict : conflicts) {
            QDir(fakeFolder.localPath() + conflict).removeRecursively();
        }

        QCOMPARE(fakeFolder.syncEngine().isAnotherSyncNeeded(), true);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }


    void testTypeChangeEmptyFolderToFile()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder({}, vfsMode, filesAreDehydrated);

        fakeFolder.remoteModifier().mkdir(QStringLiteral("TestFolder"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().remove(QStringLiteral("TestFolder"));
        fakeFolder.remoteModifier().insert(QStringLiteral("TestFolder"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        // TestFolder should now be a file, if we are using placeholders, this file must not be hydrated
        if (filesAreDehydrated) {
            QVERIFY(fakeFolder.vfs()->isDehydratedPlaceholder(fakeFolder.localPath() + QStringLiteral("TestFolder")));
        }
        QVERIFY(itemSuccessful(completeSpy, QStringLiteral("TestFolder"), CSYNC_INSTRUCTION_TYPE_CHANGE));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Test what happens if we remove entries both on the server, and locally
    void testRemoveRemove()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.localModifier().remove(QStringLiteral("A"));
        fakeFolder.remoteModifier().remove(QStringLiteral("B/b1"));
        fakeFolder.localModifier().remove(QStringLiteral("B/b1"));

        auto expectedState = fakeFolder.currentRemoteState();

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);

        QVERIFY(dbRecord(fakeFolder, QStringLiteral("B/b2")).isValid());

        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("B/b1")).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A/a1")).isValid());
        QVERIFY(!dbRecord(fakeFolder, QStringLiteral("A")).isValid());
    }
};

QTEST_GUILESS_MAIN(TestSyncConflict)
#include "testsyncconflict.moc"
