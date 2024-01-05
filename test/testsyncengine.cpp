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

using namespace std::chrono_literals;
using namespace OCC::FileSystem::SizeLiterals;
using namespace OCC;

bool itemDidComplete(const ItemCompletedSpy &spy, const QString &path)
{
    if (auto item = spy.findItem(path)) {
        return item->instruction() & ~(CSYNC_INSTRUCTION_NONE | CSYNC_INSTRUCTION_UPDATE_METADATA);
    }
    return false;
}

bool itemInstruction(const ItemCompletedSpy &spy, const QString &path, const SyncInstructions instr)
{
    auto item = spy.findItem(path);
    return item->instruction() == instr;
}

bool itemDidCompleteSuccessfully(const ItemCompletedSpy &spy, const QString &path)
{
    if (auto item = spy.findItem(path)) {
        return item->_status == SyncFileItem::Success;
    }
    return false;
}

class TestSyncEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase_data()
    {
        QTest::addColumn<Vfs::Mode>("vfsMode");
        QTest::addColumn<bool>("filesAreDehydrated");

        QTest::newRow("Vfs::Off") << Vfs::Off << false;

        if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi)) {
            QTest::newRow("Vfs::WindowsCfApi dehydrated") << Vfs::WindowsCfApi << true;

            // TODO: the hydrated version will fail due to an issue in the winvfs plugin, so leave it disabled for now.
            // QTest::newRow("Vfs::WindowsCfApi hydrated") << Vfs::WindowsCfApi << false;
        } else if (Utility::isWindows()) {
            qWarning("Skipping Vfs::WindowsCfApi");
        }
    }

    void testFileDownload() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/a0"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("A/a0")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testFileUpload() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().insert(QStringLiteral("A/a0"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("A/a0")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirDownload() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("Y"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("Z"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Z/d0"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("Y")));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("Z")));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("Z/d0")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirUpload() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().mkdir(QStringLiteral("Y"));
        fakeFolder.localModifier().mkdir(QStringLiteral("Z"));
        fakeFolder.localModifier().insert(QStringLiteral("Z/d0"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("Y")));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("Z")));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("Z/d0")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testLocalDelete() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().remove(QStringLiteral("A/a1"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("A/a1")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteDelete() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().remove(QStringLiteral("A/a1"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("A/a1")));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testEmlLocalChecksum() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo {}, vfsMode, filesAreDehydrated);
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), TestUtils::testCapabilities(CheckSums::Algorithm::SHA1)});
        fakeFolder.localModifier().insert(QStringLiteral("a1.eml"), 64_B, 'A');
        fakeFolder.localModifier().insert(QStringLiteral("a2.eml"), 64_B, 'A');
        fakeFolder.localModifier().insert(QStringLiteral("a3.eml"), 64_B, 'A');
        fakeFolder.localModifier().insert(QStringLiteral("b3.txt"), 64_B, 'A');
        // Upload and calculate the checksums
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        auto getDbChecksum = [&](const QString &path) {
            SyncJournalFileRecord record;
            fakeFolder.syncJournal().getFileRecord(path, &record);
            return record._checksumHeader;
        };

        // printf 'A%.0s' {1..64} | sha1sum -
        QByteArray referenceChecksum("SHA1:30b86e44e6001403827a62c58b08893e77cf121f");
        QCOMPARE(getDbChecksum(QStringLiteral("a1.eml")), referenceChecksum);
        QCOMPARE(getDbChecksum(QStringLiteral("a2.eml")), referenceChecksum);
        QCOMPARE(getDbChecksum(QStringLiteral("a3.eml")), referenceChecksum);
        QCOMPARE(getDbChecksum(QStringLiteral("b3.txt")), referenceChecksum);

        // Make sure that the lastModified time caused by the setContent calls below is actually different:
        QThread::sleep(1);

        ItemCompletedSpy completeSpy(fakeFolder);
        // Touch the file without changing the content, shouldn't upload
        fakeFolder.localModifier().setContents(QStringLiteral("a1.eml"), 64_B, 'A');
        // Change the content/size
        fakeFolder.localModifier().setContents(QStringLiteral("a2.eml"), 64_B, 'B');
        fakeFolder.localModifier().appendByte(QStringLiteral("a3.eml"), 'X');
        fakeFolder.localModifier().appendByte(QStringLiteral("b3.txt"), 'X');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(getDbChecksum(QStringLiteral("a1.eml")), referenceChecksum);
        QCOMPARE(getDbChecksum(QStringLiteral("a2.eml")), QByteArray("SHA1:84951fc23a4dafd10020ac349da1f5530fa65949"));
        QCOMPARE(getDbChecksum(QStringLiteral("a3.eml")), QByteArray("SHA1:c119308d57884896cd86a7050e449aaba24b1fee"));
        QCOMPARE(getDbChecksum(QStringLiteral("b3.eml")), getDbChecksum(QStringLiteral("a3.txt")));

        QVERIFY(!itemDidComplete(completeSpy, QStringLiteral("a1.eml")));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("a2.eml")));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, QStringLiteral("a3.eml")));

        // The local and remote state now differ: the local mtime for `a1.eml` is bigger (newer) than on the server, because
        // the upload was skipped (same checksum). So first verify that this is the case:
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("a1.eml"))->lastModified()
            > fakeFolder.currentRemoteState().find(QStringLiteral("a1.eml"))->lastModified());
        // And then check if everything else actually is the same:
        QVERIFY(fakeFolder.currentLocalState().equals(fakeFolder.currentRemoteState(), FileInfo::IgnoreLastModified));
    }

    void testSelectiveSyncBug() {
        // issue owncloud/enterprise#1965: files from selective-sync ignored
        // folders are uploaded anyway is some circumstances.
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        if (vfsMode == Vfs::WindowsCfApi) {
            QSKIP("selective sync is not supported with winvfs");
        }

        FakeFolder fakeFolder(FileInfo { QString(), { FileInfo { QStringLiteral("parentFolder"), { FileInfo { QStringLiteral("subFolderA"), { { QStringLiteral("fileA.txt"), 400 }, { QStringLiteral("fileB.txt"), 400, 'o' }, FileInfo { QStringLiteral("subsubFolder"), { { QStringLiteral("fileC.txt"), 400 }, { QStringLiteral("fileD.txt"), 400, 'o' } } }, FileInfo { QStringLiteral("anotherFolder"), { FileInfo { QStringLiteral("emptyFolder"), {} }, FileInfo { QStringLiteral("subsubFolder"), { { QStringLiteral("fileE.txt"), 400 }, { QStringLiteral("fileF.txt"), 400, 'o' } } } } } } }, FileInfo { QStringLiteral("subFolderB"), {} } } } } }, vfsMode, filesAreDehydrated);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto expectedServerState = fakeFolder.currentRemoteState();

        // Remove subFolderA with selectiveSync:
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, {QStringLiteral("parentFolder/subFolderA/")});
        fakeFolder.syncEngine().journal()->schedulePathForRemoteDiscovery(QByteArrayLiteral("parentFolder/subFolderA/"));
        auto getEtag = [&](const QByteArray &file) {
            SyncJournalFileRecord rec;
            fakeFolder.syncJournal().getFileRecord(file, &rec);
            return rec._etag;
        };
        QVERIFY(getEtag("parentFolder") == "_invalid_");
        QVERIFY(getEtag("parentFolder/subFolderA") == "_invalid_");
        QVERIFY(getEtag("parentFolder/subFolderA/subsubFolder") != "_invalid_");

        // But touch local file before the next sync, such that the local folder
        // can't be removed
        fakeFolder.localModifier().setContents(QStringLiteral("parentFolder/subFolderA/fileB.txt"), FileModifier::DefaultFileSize, 'n');
        fakeFolder.localModifier().setContents(QStringLiteral("parentFolder/subFolderA/subsubFolder/fileD.txt"), FileModifier::DefaultFileSize, 'n');
        fakeFolder.localModifier().setContents(QStringLiteral("parentFolder/subFolderA/anotherFolder/subsubFolder/fileF.txt"), FileModifier::DefaultFileSize, 'n');

        // Several follow-up syncs don't change the state at all,
        // in particular the remote state doesn't change and fileB.txt
        // isn't uploaded.

        for (int i = 0; i < 3; ++i) {
            QVERIFY(fakeFolder.applyLocalModificationsAndSync());

            {
                // Nothing changed on the server
                QCOMPARE(fakeFolder.currentRemoteState(), expectedServerState);
                // The local state should still have subFolderA
                auto local = fakeFolder.currentLocalState();
                QVERIFY(local.find(QStringLiteral("parentFolder/subFolderA")));
                QVERIFY(!local.find(QStringLiteral("parentFolder/subFolderA/fileA.txt")));
                QVERIFY(local.find(QStringLiteral("parentFolder/subFolderA/fileB.txt")));
                QVERIFY(!local.find(QStringLiteral("parentFolder/subFolderA/subsubFolder/fileC.txt")));
                QVERIFY(local.find(QStringLiteral("parentFolder/subFolderA/subsubFolder/fileD.txt")));
                QVERIFY(!local.find(QStringLiteral("parentFolder/subFolderA/anotherFolder/subsubFolder/fileE.txt")));
                QVERIFY(local.find(QStringLiteral("parentFolder/subFolderA/anotherFolder/subsubFolder/fileF.txt")));
                QVERIFY(!local.find(QStringLiteral("parentFolder/subFolderA/anotherFolder/emptyFolder")));
                QVERIFY(local.find(QStringLiteral("parentFolder/subFolderB")));
            }
        }
    }

    void abortAfterFailedMkdir() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo {}, vfsMode, filesAreDehydrated);
        QSignalSpy finishedSpy(&fakeFolder.syncEngine(), &SyncEngine::finished);
        fakeFolder.serverErrorPaths().append(QStringLiteral("NewFolder"));
        fakeFolder.localModifier().mkdir(QStringLiteral("NewFolder"));
        // This should be aborted and would otherwise fail in FileInfo::create.
        fakeFolder.localModifier().insert(QStringLiteral("NewFolder/NewFile"), FileModifier::DefaultFileSize, 'n');

        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(finishedSpy.size(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), false);
    }

    /**
     * Verify that an incompletely propagated directory doesn't have the server's
     * etag stored in the database yet.
     */
    void testDirEtagAfterIncompleteSync()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo {}, vfsMode, filesAreDehydrated);
        QSignalSpy finishedSpy(&fakeFolder.syncEngine(), &SyncEngine::finished);
        fakeFolder.serverErrorPaths().append(QStringLiteral("NewFolder/foo"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("NewFolder"));
        fakeFolder.remoteModifier().insert(QStringLiteral("NewFolder/foo"));
        bool syncSuccess = fakeFolder.applyLocalModificationsAndSync();
        if (filesAreDehydrated) {
            QVERIFY(syncSuccess);
        } else {
            QVERIFY(!syncSuccess);
        }

        SyncJournalFileRecord rec;
        fakeFolder.syncJournal().getFileRecord(QByteArrayLiteral("NewFolder"), &rec);
        QVERIFY(rec.isValid());
        if (filesAreDehydrated) {
            // No error, failure occurs only with a GET, so etag should be valid (i.e. NOT invalid):
            QVERIFY(rec._etag != QByteArrayLiteral("_invalid_"));
        } else {
            // Download failed, check for invalid etag:
            QCOMPARE(rec._etag, QByteArrayLiteral("_invalid_"));
        }
        QVERIFY(!rec._fileId.isEmpty());
    }

    void testDirDownloadWithError() {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        if (filesAreDehydrated) {
            QSKIP("Nothing gets downloaded, so no error, so skip this test");
        }

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("Y"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("Y/Z"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d0"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d2"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d3"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d4"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d5"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d6"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d7"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d8"));
        fakeFolder.remoteModifier().insert(QStringLiteral("Y/Z/d9"));
        fakeFolder.serverErrorPaths().append(QStringLiteral("Y/Z/d2"), 503);
        fakeFolder.serverErrorPaths().append(QStringLiteral("Y/Z/d3"), 503);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        QCoreApplication::processEvents(); // should not crash

        QSet<QString> seen;
        for(const QList<QVariant> &args : completeSpy) {
            auto item = args[0].value<SyncFileItemPtr>();
            qDebug() << item->_file << item->isDirectory() << item->_status;
            QVERIFY(!seen.contains(item->_file)); // signal only sent once per item
            seen.insert(item->_file);
            if (item->_file == QLatin1String("Y/Z/d2")) {
                QVERIFY(item->_status == SyncFileItem::NormalError);
            } else if (item->_file == QLatin1String("Y/Z/d3")) {
                QVERIFY(item->_status != SyncFileItem::Success);
            } else if (!item->isDirectory()) {
                QVERIFY(item->_status == SyncFileItem::Success);
            }
        }
    }

    void testFakeConflict_data()
    {
        QTest::addColumn<bool>("sameMtime");
        QTest::addColumn<QByteArray>("checksums");
        QTest::addColumn<int>("expectedGEThydrated");
        QTest::addColumn<int>("expectedGETdehydrated");

        QTest::newRow("Same mtime, but no server checksum -> ignored in reconcile")
            << true << QByteArray()
            << 0 // hydrated
            << 1; // dehydrated

        QTest::newRow("Same mtime, weak server checksum differ -> downloaded")
            << true << QByteArray("Adler32:bad")
            << 1 // hydrated
            << 1; // dehydrated;

        QTest::newRow("Same mtime, matching weak checksum -> skipped")
            << true << QByteArray("Adler32:2a2010d")
            << 0 // hydrated
            << 1; // dehydrated;

        QTest::newRow("Same mtime, strong server checksum differ -> downloaded")
            << true << QByteArray("SHA1:bad")
            << 1 // hydrated
            << 1; // dehydrated;

        QTest::newRow("Same mtime, matching strong checksum -> skipped")
            << true << QByteArray("SHA1:56900fb1d337cf7237ff766276b9c1e8ce507427")
            << 0 // hydrated
            << 1; // dehydrated;

        QTest::newRow("mtime changed, but no server checksum -> download")
            << false << QByteArray()
            << 1 // hydrated
            << 1; // dehydrated;

        QTest::newRow("mtime changed, weak checksum match -> download anyway")
            << false << QByteArray("Adler32:2a2010d")
            << 1 // hydrated
            << 1; // dehydrated;

        QTest::newRow("mtime changed, strong checksum match -> skip")
            << false << QByteArray("SHA1:56900fb1d337cf7237ff766276b9c1e8ce507427")
            << 0 // hydrated
            << 1; // dehydrated;
    }

    void testFakeConflict()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);
        QFETCH(bool, sameMtime);
        QFETCH(QByteArray, checksums);
        QFETCH(int, expectedGEThydrated);
        QFETCH(int, expectedGETdehydrated);

        int expectedGET = filesAreDehydrated ? expectedGETdehydrated : expectedGEThydrated;

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        OperationCounter counter(fakeFolder);

        // For directly editing the remote checksum
        FileInfo &remoteInfo = fakeFolder.remoteModifier();

        // Base mtime with no ms content (filesystem is seconds only)
        auto mtime = QDateTime::currentDateTimeUtc().addDays(-4);
        mtime.setMSecsSinceEpoch(mtime.toMSecsSinceEpoch() / 1000 * 1000);

        const auto a1size = fakeFolder.currentRemoteState().find(QStringLiteral("A/a1"))->contentSize;

        // In the dehydrated case, executing this `setContents` will cause a hydration of the file, so there will always be 1 GET request.
        fakeFolder.localModifier().setContents(QStringLiteral("A/a1"), a1size, 'C');

        fakeFolder.localModifier().setModTime(QStringLiteral("A/a1"), mtime);
        fakeFolder.remoteModifier().setContents(QStringLiteral("A/a1"), a1size, 'C');
        if (!sameMtime) {
            mtime = mtime.addDays(1);
        }
        fakeFolder.remoteModifier().setModTime(QStringLiteral("A/a1"), mtime);
        remoteInfo.find(QStringLiteral("A/a1"))->checksums = checksums;
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(counter.nGET, expectedGET);

        // check that mtime in journal and filesystem agree
        QString a1path = fakeFolder.localPath() + QStringLiteral("A/a1");
        SyncJournalFileRecord a1record;
        fakeFolder.syncJournal().getFileRecord(QByteArray("A/a1"), &a1record);
        QCOMPARE(a1record._modtime, (qint64)FileSystem::getModTime(a1path));

        // Extra sync reads from db, no difference
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(counter.nGET, expectedGET);
    }

    /**
     * Checks whether SyncFileItems have the expected properties before start
     * of propagation.
     */
    void testSyncFileItemProperties()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        if (vfsMode == Vfs::WindowsCfApi && filesAreDehydrated) {
            QSKIP("This test expects files to exist in the sync folder before a sync.");
        }

        auto initialMtime = QDateTime::currentDateTimeUtc().addDays(-7);
        auto changedMtime = QDateTime::currentDateTimeUtc().addDays(-4);
        auto changedMtime2 = QDateTime::currentDateTimeUtc().addDays(-3);

        // Base mtime with no ms content (filesystem is seconds only)
        initialMtime.setMSecsSinceEpoch(initialMtime.toMSecsSinceEpoch() / 1000 * 1000);
        changedMtime.setMSecsSinceEpoch(changedMtime.toMSecsSinceEpoch() / 1000 * 1000);
        changedMtime2.setMSecsSinceEpoch(changedMtime2.toMSecsSinceEpoch() / 1000 * 1000);

        // Ensure the initial mtimes are as expected
        auto initialFileInfo = FileInfo::A12_B12_C12_S12();
        initialFileInfo.setModTime(QStringLiteral("A/a1"), initialMtime);
        initialFileInfo.setModTime(QStringLiteral("B/b1"), initialMtime);
        initialFileInfo.setModTime(QStringLiteral("C/c1"), initialMtime);

        FakeFolder fakeFolder(initialFileInfo, vfsMode, filesAreDehydrated);

        // upload a
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.localModifier().setModTime(QStringLiteral("A/a1"), changedMtime);
        // download b
        fakeFolder.remoteModifier().appendByte(QStringLiteral("B/b1"));
        fakeFolder.remoteModifier().setModTime(QStringLiteral("B/b1"), changedMtime);
        // conflict c
        fakeFolder.localModifier().appendByte(QStringLiteral("C/c1"));
        fakeFolder.localModifier().appendByte(QStringLiteral("C/c1"));
        fakeFolder.localModifier().setModTime(QStringLiteral("C/c1"), changedMtime);
        fakeFolder.remoteModifier().appendByte(QStringLiteral("C/c1"));
        fakeFolder.remoteModifier().setModTime(QStringLiteral("C/c1"), changedMtime2);

        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, [&](const SyncFileItemSet &items) {
            SyncFileItemPtr a1, b1, c1;
            for (auto &item : items) {
                if (item->_file == QLatin1String("A/a1"))
                    a1 = item;
                if (item->_file == QLatin1String("B/b1"))
                    b1 = item;
                if (item->_file == QLatin1String("C/c1"))
                    c1 = item;
            }

            // a1: should have local size and modtime
            QVERIFY(a1);
            QCOMPARE(a1->instruction(), CSYNC_INSTRUCTION_SYNC);
            QCOMPARE(a1->_direction, SyncFileItem::Up);
            QCOMPARE(a1->_size, qint64(5));

            QCOMPARE(Utility::qDateTimeFromTime_t(a1->_modtime), changedMtime);
            QCOMPARE(a1->_previousSize, qint64(4));
            QCOMPARE(Utility::qDateTimeFromTime_t(a1->_previousModtime), initialMtime);

            // b2: should have remote size and modtime
            QVERIFY(b1);
            QCOMPARE(b1->instruction(), CSYNC_INSTRUCTION_SYNC);
            QCOMPARE(b1->_direction, SyncFileItem::Down);
            QCOMPARE(b1->_size, qint64(17));
            QCOMPARE(Utility::qDateTimeFromTime_t(b1->_modtime), changedMtime);
            QCOMPARE(b1->_previousSize, qint64(16));
            QCOMPARE(Utility::qDateTimeFromTime_t(b1->_previousModtime), initialMtime);

            // c1: conflicts are downloads, so remote size and modtime
            QVERIFY(c1);
            QCOMPARE(c1->instruction(), CSYNC_INSTRUCTION_CONFLICT);
            QCOMPARE(c1->_direction, SyncFileItem::None);
            QCOMPARE(c1->_size, qint64(25));
            QCOMPARE(Utility::qDateTimeFromTime_t(c1->_modtime), changedMtime2);
            QCOMPARE(c1->_previousSize, qint64(26));
            QCOMPARE(Utility::qDateTimeFromTime_t(c1->_previousModtime), changedMtime);
        });

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
    }

    /**
     * Checks whether subsequent large uploads are skipped after a 507 error
     */
    void testInsufficientRemoteStorage()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);

        // Disable parallel uploads
        SyncOptions syncOptions = fakeFolder.syncEngine().syncOptions();
        syncOptions._parallelNetworkJobs = 0;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        // Produce an error based on upload size
        int remoteQuota = 1000;
        int n507 = 0, nPUT = 0;
        QObject parent;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                nPUT++;
                if (request.rawHeader("OC-Total-Length").toInt() > remoteQuota) {
                    n507++;
                    return new FakeErrorReply(op, request, &parent, 507);
                }
            }
            return nullptr;
        });

        fakeFolder.localModifier().insert(QStringLiteral("A/big"), 800_B);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(nPUT, 1);
        QCOMPARE(n507, 0);

        nPUT = 0;
        fakeFolder.localModifier().insert(QStringLiteral("A/big1"), 500_B); // ok
        fakeFolder.localModifier().insert(QStringLiteral("A/big2"), 1200_B); // 507 (quota guess now 1199)
        fakeFolder.localModifier().insert(QStringLiteral("A/big3"), 1200_B); // skipped
        fakeFolder.localModifier().insert(QStringLiteral("A/big4"), 1500_B); // skipped
        fakeFolder.localModifier().insert(QStringLiteral("A/big5"), 1100_B); // 507 (quota guess now 1099)
        fakeFolder.localModifier().insert(QStringLiteral("A/big6"), 900_B); // ok (quota guess now 199)
        fakeFolder.localModifier().insert(QStringLiteral("A/big7"), 200_B); // skipped
        fakeFolder.localModifier().insert(QStringLiteral("A/big8"), 199_B); // ok (quota guess now 0)

        fakeFolder.localModifier().insert(QStringLiteral("B/big8"), 1150_B); // 507
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(nPUT, 6);
        QCOMPARE(n507, 3);
    }

    // Checks whether downloads with bad checksums are accepted
    void testChecksumValidation()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        QObject parent;

        QByteArray checksumValue;
        QByteArray contentMd5Value;

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation) {
                auto reply = new FakeGetReply(fakeFolder.remoteModifier(), op, request, &parent);
                if (!checksumValue.isNull())
                    reply->setRawHeader("OC-Checksum", checksumValue);
                if (!contentMd5Value.isNull())
                    reply->setRawHeader("Content-MD5", contentMd5Value);
                return reply;
            }
            return nullptr;
        });

        // Basic case
        fakeFolder.remoteModifier().create(QStringLiteral("A/a3"), 16, 'A');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Bad OC-Checksum
        checksumValue = "SHA1:bad";
        fakeFolder.remoteModifier().create(QStringLiteral("A/a4"), 16, 'A');
        bool syncSucceeded = fakeFolder.applyLocalModificationsAndSync();
        if (filesAreDehydrated) {
            // In the dehydrated case, files are never downloaded, so checksums are not computed. Only placeholders are created.
            QVERIFY(syncSucceeded);
        } else {
            // Any case where files are actually downloaded (no vfs, local files are hydrated, etc), the checksum calculation should fail.
            QVERIFY(!syncSucceeded);
        }

        // Good OC-Checksum
        checksumValue = "SHA1:19b1928d58a2030d08023f3d7054516dbc186f20"; // printf 'A%.0s' {1..16} | sha1sum -
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        checksumValue = QByteArray();

        // Bad Content-MD5
        contentMd5Value = "bad";
        fakeFolder.remoteModifier().create(QStringLiteral("A/a5"), 16, 'A');
        syncSucceeded = fakeFolder.applyLocalModificationsAndSync();
        if (filesAreDehydrated) {
            // In the dehydrated case, files are never downloaded, so checksums are not computed. Only placeholders are created.
            QVERIFY(syncSucceeded);
        } else {
            // Any case where files are actually downloaded (no vfs, local files are hydrated, etc), the checksum calculation should fail.
            QVERIFY(!syncSucceeded);
        }

        // Good Content-MD5
        contentMd5Value = "d8a73157ce10cd94a91c2079fc9a92c8"; // printf 'A%.0s' {1..16} | md5sum -
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Invalid OC-Checksum is ignored
        checksumValue = "garbage";
        // contentMd5Value is still good
        fakeFolder.remoteModifier().create(QStringLiteral("A/a6"), 16, 'A');
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        contentMd5Value = "bad";
        fakeFolder.remoteModifier().create(QStringLiteral("A/a7"), 16, 'A');
        syncSucceeded = fakeFolder.applyLocalModificationsAndSync();
        if (filesAreDehydrated) {
            // In the dehydrated case, files are never downloaded, so checksums are not computed. Only placeholders are created.
            QVERIFY(syncSucceeded);
        } else {
            // Any case where files are actually downloaded (no vfs, local files are hydrated, etc), the checksum calculation should fail.
            QVERIFY(!syncSucceeded);
        }
        contentMd5Value.clear();
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // OC-Checksum contains Unsupported checksums
        checksumValue = "Unsupported:XXXX SHA1:invalid Invalid:XxX";
        fakeFolder.remoteModifier().create(QStringLiteral("A/a8"), 16, 'A');
        syncSucceeded = fakeFolder.applyLocalModificationsAndSync();
        if (filesAreDehydrated) {
            // In the dehydrated case, files are never downloaded, so checksums are not computed. Only placeholders are created.
            QVERIFY(syncSucceeded);
        } else {
            // Since the supported SHA1 checksum is invalid, no download
            QVERIFY(!syncSucceeded);
        }
        checksumValue =  "Unsupported:XXXX SHA1:19b1928d58a2030d08023f3d7054516dbc186f20 Invalid:XxX";
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync()); // The supported SHA1 checksum is valid now, so the file are downloaded
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Tests the behavior of invalid filename detection
    void testInvalidFilenameRegex()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);

        if (!Utility::isWindows()) { // We can't have local file with these character
            fakeFolder.localModifier().insert(QStringLiteral("A/\\:?*\"<>|.txt"));
            QVERIFY(fakeFolder.applyLocalModificationsAndSync());
            QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        }

        // We can override that by setting the capability
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Check that new servers also accept the capability
        auto invalidFilenameRegexCapabilities = [](const QString &regex) {
            auto cap = TestUtils::testCapabilities();
            auto dav = cap[QStringLiteral("dav")].toMap();
            dav.insert({{QStringLiteral("invalidFilenameRegex"), regex}});
            cap[QStringLiteral("dav")] = dav;
            return cap;
        };
        fakeFolder.syncEngine().account()->setCapabilities({fakeFolder.account()->url(), invalidFilenameRegexCapabilities(QStringLiteral("my[fgh]ile"))});
        fakeFolder.localModifier().insert(QStringLiteral("C/myfile.txt"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("C/myfile.txt")));
    }

    void testDiscoveryHiddenFile()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // We can't depend on currentLocalState for hidden files since
        // it should rightfully skip things like download temporaries
        auto localFileExists = [&](const QString &name) {
            return QFileInfo::exists(fakeFolder.localPath() + name);
        };

        fakeFolder.syncEngine().setIgnoreHiddenFiles(true);
        fakeFolder.remoteModifier().insert(QStringLiteral("A/.hidden"));
        fakeFolder.localModifier().insert(QStringLiteral("B/.hidden"));
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(!localFileExists(QStringLiteral("A/.hidden")));
        QVERIFY(!fakeFolder.currentRemoteState().find(QStringLiteral("B/.hidden")));

        fakeFolder.syncEngine().setIgnoreHiddenFiles(false);
        fakeFolder.syncJournal().forceRemoteDiscoveryNextSync();
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QVERIFY(localFileExists(QStringLiteral("A/.hidden")));
        QVERIFY(fakeFolder.currentRemoteState().find(QStringLiteral("B/.hidden")));
    }

    // Aborting has had bugs when there are parallel upload jobs
    void testUploadV1Multiabort()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo {}, vfsMode, filesAreDehydrated);
        SyncOptions options = fakeFolder.syncEngine().syncOptions();
        options._initialChunkSize = 10;
        options._maxChunkSize = 10;
        options._minChunkSize = 10;
        fakeFolder.syncEngine().setSyncOptions(options);
        auto cap = TestUtils::testCapabilities();
        // unset chunking v1
        cap.remove(QStringLiteral("dav"));
        fakeFolder.account()->setCapabilities({fakeFolder.account()->url(), cap});

        auto counter = std::make_unique<OperationCounter>();
        fakeFolder.setServerOverride([counter = counter.get(), fakeFolder = &fakeFolder](
                                         QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *device) -> QNetworkReply * {
            counter->serverOverride(op, request, device);
            if (op == QNetworkAccessManager::PutOperation) {
                return new FakeHangingReply(op, request, fakeFolder);
            }
            return nullptr;
        });

        fakeFolder.localModifier().insert(QStringLiteral("file"), 1_MiB, 'W');
        // wait until the sync engine is ready
        // wait a second and abort
        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, &fakeFolder.syncEngine(),
            [&]() { QTimer::singleShot(1s, &fakeFolder.syncEngine(), [&]() { fakeFolder.syncEngine().abort({}); }); });
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());

        QCOMPARE(counter->nPUT, 3);
    }

#ifndef Q_OS_WIN
    void testPropagatePermissions()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        auto perm = QFileDevice::Permission(0x7704); // user/owner: rwx, group: r, other: -
        QFile::setPermissions(fakeFolder.localPath() + QStringLiteral("A/a1"), perm);
        QFile::setPermissions(fakeFolder.localPath() + QStringLiteral("A/a2"), perm);
        fakeFolder.applyLocalModificationsAndSync(); // get the metadata-only change out of the way
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a2"));
        fakeFolder.applyLocalModificationsAndSync(); // perms should be preserved
        QCOMPARE(QFileInfo(fakeFolder.localPath() + QStringLiteral("A/a1")).permissions(), perm);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + QStringLiteral("A/a2")).permissions(), perm);

        const QString conflictName = QString::fromUtf8(fakeFolder.syncJournal().conflictRecord(fakeFolder.syncJournal().conflictRecordPaths().first()).path);
        QVERIFY(conflictName.contains(QStringLiteral("A/a2")));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + conflictName).permissions(), perm);
    }
#endif

    void testEmptyLocalButHasRemote()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo {}, vfsMode, filesAreDehydrated);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("foo"));

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("foo")));
    }

    // Check that server mtime is set on directories on initial propagation
    void testDirectoryInitialMtime()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo {}, vfsMode, filesAreDehydrated);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("foo"));
        fakeFolder.remoteModifier().insert(QStringLiteral("foo/bar"));
        auto datetime = QDateTime::currentDateTime();
        datetime.setSecsSinceEpoch(datetime.toSecsSinceEpoch()); // wipe ms
        fakeFolder.remoteModifier().find(QStringLiteral("foo"))->setLastModified(datetime);

        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QCOMPARE(QFileInfo(fakeFolder.localPath() + QStringLiteral("foo")).lastModified(), datetime);
    }


    void testProceedWithIndependentDelets()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // during an upload of B/newFile an error occurs that aborts all delete jobs
        fakeFolder.localModifier().insert(QStringLiteral("B/newFile"));
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                return new FakeErrorReply(op, request, this, 507);
            }
            return nullptr;
        });

        // this will go through
        fakeFolder.remoteModifier().insert(QStringLiteral("C/newRemoteFile"));

        // those will fail https://github.com/owncloud/client/issues/9311
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.remoteModifier().remove(QStringLiteral("S"));

        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());

        // the download succeeded
        QVERIFY(fakeFolder.currentLocalState().find(QStringLiteral("C/newRemoteFile")));

        // the deletes happened as they are independent of the error in B
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("A")));
        QVERIFY(!fakeFolder.currentLocalState().find(QStringLiteral("S")));
    }
};

QTEST_GUILESS_MAIN(TestSyncEngine)
#include "testsyncengine.moc"
