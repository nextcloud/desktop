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
using namespace OCC;

bool itemDidComplete(const ItemCompletedSpy &spy, const QString &path)
{
    if (auto item = spy.findItem(path)) {
        return item->_instruction != CSYNC_INSTRUCTION_NONE && item->_instruction != CSYNC_INSTRUCTION_UPDATE_METADATA;
    }
    return false;
}

bool itemInstruction(const ItemCompletedSpy &spy, const QString &path, const SyncInstructions instr)
{
    auto item = spy.findItem(path);
    return item->_instruction == instr;
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
    void testFileDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().insert("A/a0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testFileUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().insert("A/a0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().mkdir("Y");
        fakeFolder.remoteModifier().mkdir("Z");
        fakeFolder.remoteModifier().insert("Z/d0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Y"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z/d0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().mkdir("Y");
        fakeFolder.localModifier().mkdir("Z");
        fakeFolder.localModifier().insert("Z/d0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Y"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z/d0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testLocalDelete() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().remove("A/a1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteDelete() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().remove("A/a1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testEmlLocalChecksum() {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.account()->setCapabilities(TestUtils::testCapabilities(CheckSums::Algorithm::SHA1));

        fakeFolder.localModifier().insert("a1.eml", 64, 'A');
        fakeFolder.localModifier().insert("a2.eml", 64, 'A');
        fakeFolder.localModifier().insert("a3.eml", 64, 'A');
        fakeFolder.localModifier().insert("b3.txt", 64, 'A');
        // Upload and calculate the checksums
        // fakeFolder.syncOnce();
        fakeFolder.syncOnce();

        auto getDbChecksum = [&](QString path) {
            SyncJournalFileRecord record;
            fakeFolder.syncJournal().getFileRecord(path, &record);
            return record._checksumHeader;
        };

        // printf 'A%.0s' {1..64} | sha1sum -
        QByteArray referenceChecksum("SHA1:30b86e44e6001403827a62c58b08893e77cf121f");
        QCOMPARE(getDbChecksum("a1.eml"), referenceChecksum);
        QCOMPARE(getDbChecksum("a2.eml"), referenceChecksum);
        QCOMPARE(getDbChecksum("a3.eml"), referenceChecksum);
        QCOMPARE(getDbChecksum("b3.txt"), referenceChecksum);

        // Make sure that the lastModified time caused by the setContent calls below is actually different:
        QThread::sleep(1);

        ItemCompletedSpy completeSpy(fakeFolder);
        // Touch the file without changing the content, shouldn't upload
        fakeFolder.localModifier().setContents("a1.eml", 'A');
        // Change the content/size
        fakeFolder.localModifier().setContents("a2.eml", 'B');
        fakeFolder.localModifier().appendByte("a3.eml");
        fakeFolder.localModifier().appendByte("b3.txt");
        fakeFolder.syncOnce();

        QCOMPARE(getDbChecksum("a1.eml"), referenceChecksum);
        QCOMPARE(getDbChecksum("a2.eml"), QByteArray("SHA1:84951fc23a4dafd10020ac349da1f5530fa65949"));
        QCOMPARE(getDbChecksum("a3.eml"), QByteArray("SHA1:826b7e7a7af8a529ae1c7443c23bf185c0ad440c"));
        QCOMPARE(getDbChecksum("b3.eml"), getDbChecksum("a3.txt"));

        QVERIFY(!itemDidComplete(completeSpy, "a1.eml"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "a2.eml"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "a3.eml"));

        // The local and remote state now differ: the local mtime for `a1.eml` is bigger (newer) than on the server, because
        // the upload was skipped (same checksum). So first verify that this is the case:
        QVERIFY(fakeFolder.currentLocalState().find("a1.eml")->lastModified() > fakeFolder.currentRemoteState().find("a1.eml")->lastModified());
        // And then check if everything else actually is the same:
        QVERIFY(fakeFolder.currentLocalState().equals(fakeFolder.currentRemoteState(), FileInfo::IgnoreLastModified));
    }

    void testSelectiveSyncBug() {
        // issue owncloud/enterprise#1965: files from selective-sync ignored
        // folders are uploaded anyway is some circumstances.
        FakeFolder fakeFolder{FileInfo{ QString(), {
            FileInfo { QStringLiteral("parentFolder"), {
                FileInfo{ QStringLiteral("subFolderA"), {
                    { QStringLiteral("fileA.txt"), 400 },
                    { QStringLiteral("fileB.txt"), 400, 'o' },
                    FileInfo { QStringLiteral("subsubFolder"), {
                        { QStringLiteral("fileC.txt"), 400 },
                        { QStringLiteral("fileD.txt"), 400, 'o' }
                    }},
                    FileInfo{ QStringLiteral("anotherFolder"), {
                        FileInfo { QStringLiteral("emptyFolder"), { } },
                        FileInfo { QStringLiteral("subsubFolder"), {
                            { QStringLiteral("fileE.txt"), 400 },
                            { QStringLiteral("fileF.txt"), 400, 'o' }
                        }}
                    }}
                }},
                FileInfo{ QStringLiteral("subFolderB"), {} }
            }}
        }}};

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto expectedServerState = fakeFolder.currentRemoteState();

        // Remove subFolderA with selectiveSync:
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                                                                {"parentFolder/subFolderA/"});
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
        fakeFolder.localModifier().setContents("parentFolder/subFolderA/fileB.txt", 'n');
        fakeFolder.localModifier().setContents("parentFolder/subFolderA/subsubFolder/fileD.txt", 'n');
        fakeFolder.localModifier().setContents("parentFolder/subFolderA/anotherFolder/subsubFolder/fileF.txt", 'n');

        // Several follow-up syncs don't change the state at all,
        // in particular the remote state doesn't change and fileB.txt
        // isn't uploaded.

        for (int i = 0; i < 3; ++i) {
            fakeFolder.syncOnce();

            {
                // Nothing changed on the server
                QCOMPARE(fakeFolder.currentRemoteState(), expectedServerState);
                // The local state should still have subFolderA
                auto local = fakeFolder.currentLocalState();
                QVERIFY(local.find("parentFolder/subFolderA"));
                QVERIFY(!local.find("parentFolder/subFolderA/fileA.txt"));
                QVERIFY(local.find("parentFolder/subFolderA/fileB.txt"));
                QVERIFY(!local.find("parentFolder/subFolderA/subsubFolder/fileC.txt"));
                QVERIFY(local.find("parentFolder/subFolderA/subsubFolder/fileD.txt"));
                QVERIFY(!local.find("parentFolder/subFolderA/anotherFolder/subsubFolder/fileE.txt"));
                QVERIFY(local.find("parentFolder/subFolderA/anotherFolder/subsubFolder/fileF.txt"));
                QVERIFY(!local.find("parentFolder/subFolderA/anotherFolder/emptyFolder"));
                QVERIFY(local.find("parentFolder/subFolderB"));
            }
        }
    }

    void abortAfterFailedMkdir() {
        FakeFolder fakeFolder{FileInfo{}};
        QSignalSpy finishedSpy(&fakeFolder.syncEngine(), &SyncEngine::finished);
        fakeFolder.serverErrorPaths().append("NewFolder");
        fakeFolder.localModifier().mkdir("NewFolder");
        // This should be aborted and would otherwise fail in FileInfo::create.
        fakeFolder.localModifier().insert("NewFolder/NewFile");
        fakeFolder.syncOnce();
        QCOMPARE(finishedSpy.size(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), false);
    }

    /** Verify that an incompletely propagated directory doesn't have the server's
     * etag stored in the database yet. */
    void testDirEtagAfterIncompleteSync() {
        FakeFolder fakeFolder{FileInfo{}};
        QSignalSpy finishedSpy(&fakeFolder.syncEngine(), &SyncEngine::finished);
        fakeFolder.serverErrorPaths().append("NewFolder/foo");
        fakeFolder.remoteModifier().mkdir("NewFolder");
        fakeFolder.remoteModifier().insert("NewFolder/foo");
        QVERIFY(!fakeFolder.syncOnce());

        SyncJournalFileRecord rec;
        fakeFolder.syncJournal().getFileRecord(QByteArrayLiteral("NewFolder"), &rec);
        QVERIFY(rec.isValid());
        QCOMPARE(rec._etag, QByteArrayLiteral("_invalid_"));
        QVERIFY(!rec._fileId.isEmpty());
    }

    void testDirDownloadWithError() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().mkdir("Y");
        fakeFolder.remoteModifier().mkdir("Y/Z");
        fakeFolder.remoteModifier().insert("Y/Z/d0");
        fakeFolder.remoteModifier().insert("Y/Z/d1");
        fakeFolder.remoteModifier().insert("Y/Z/d2");
        fakeFolder.remoteModifier().insert("Y/Z/d3");
        fakeFolder.remoteModifier().insert("Y/Z/d4");
        fakeFolder.remoteModifier().insert("Y/Z/d5");
        fakeFolder.remoteModifier().insert("Y/Z/d6");
        fakeFolder.remoteModifier().insert("Y/Z/d7");
        fakeFolder.remoteModifier().insert("Y/Z/d8");
        fakeFolder.remoteModifier().insert("Y/Z/d9");
        fakeFolder.serverErrorPaths().append("Y/Z/d2", 503);
        fakeFolder.serverErrorPaths().append("Y/Z/d3", 503);
        QVERIFY(!fakeFolder.syncOnce());
        QCoreApplication::processEvents(); // should not crash

        QSet<QString> seen;
        for(const QList<QVariant> &args : completeSpy) {
            auto item = args[0].value<SyncFileItemPtr>();
            qDebug() << item->_file << item->isDirectory() << item->_status;
            QVERIFY(!seen.contains(item->_file)); // signal only sent once per item
            seen.insert(item->_file);
            if (item->_file == "Y/Z/d2") {
                QVERIFY(item->_status == SyncFileItem::NormalError);
            } else if (item->_file == "Y/Z/d3") {
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

        QTest::addColumn<int>("expectedGET");

        QTest::newRow("Same mtime, but no server checksum -> ignored in reconcile")
            << true << QByteArray()
            << 0;

        QTest::newRow("Same mtime, weak server checksum differ -> downloaded")
            << true << QByteArray("Adler32:bad")
            << 1;

        QTest::newRow("Same mtime, matching weak checksum -> skipped")
            << true << QByteArray("Adler32:2a2010d")
            << 0;

        QTest::newRow("Same mtime, strong server checksum differ -> downloaded")
            << true << QByteArray("SHA1:bad")
            << 1;

        QTest::newRow("Same mtime, matching strong checksum -> skipped")
            << true << QByteArray("SHA1:56900fb1d337cf7237ff766276b9c1e8ce507427")
            << 0;


        QTest::newRow("mtime changed, but no server checksum -> download")
            << false << QByteArray()
            << 1;

        QTest::newRow("mtime changed, weak checksum match -> download anyway")
            << false << QByteArray("Adler32:2a2010d")
            << 1;

        QTest::newRow("mtime changed, strong checksum match -> skip")
            << false << QByteArray("SHA1:56900fb1d337cf7237ff766276b9c1e8ce507427")
            << 0;
    }

    void testFakeConflict()
    {
        QFETCH(bool, sameMtime);
        QFETCH(QByteArray, checksums);
        QFETCH(int, expectedGET);

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        int nGET = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &, QIODevice *) {
            if (op == QNetworkAccessManager::GetOperation)
                ++nGET;
            return nullptr;
        });

        // For directly editing the remote checksum
        FileInfo &remoteInfo = fakeFolder.remoteModifier();

        // Base mtime with no ms content (filesystem is seconds only)
        auto mtime = QDateTime::currentDateTimeUtc().addDays(-4);
        mtime.setMSecsSinceEpoch(mtime.toMSecsSinceEpoch() / 1000 * 1000);

        fakeFolder.localModifier().setContents("A/a1", 'C');
        fakeFolder.localModifier().setModTime("A/a1", mtime);
        fakeFolder.remoteModifier().setContents("A/a1", 'C');
        if (!sameMtime)
            mtime = mtime.addDays(1);
        fakeFolder.remoteModifier().setModTime("A/a1", mtime);
        remoteInfo.find("A/a1")->checksums = checksums;
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, expectedGET);

        // check that mtime in journal and filesystem agree
        QString a1path = fakeFolder.localPath() + "A/a1";
        SyncJournalFileRecord a1record;
        fakeFolder.syncJournal().getFileRecord(QByteArray("A/a1"), &a1record);
        QCOMPARE(a1record._modtime, (qint64)FileSystem::getModTime(a1path));

        // Extra sync reads from db, no difference
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, expectedGET);
    }

    /**
     * Checks whether SyncFileItems have the expected properties before start
     * of propagation.
     */
    void testSyncFileItemProperties()
    {
        auto initialMtime = QDateTime::currentDateTimeUtc().addDays(-7);
        auto changedMtime = QDateTime::currentDateTimeUtc().addDays(-4);
        auto changedMtime2 = QDateTime::currentDateTimeUtc().addDays(-3);

        // Base mtime with no ms content (filesystem is seconds only)
        initialMtime.setMSecsSinceEpoch(initialMtime.toMSecsSinceEpoch() / 1000 * 1000);
        changedMtime.setMSecsSinceEpoch(changedMtime.toMSecsSinceEpoch() / 1000 * 1000);
        changedMtime2.setMSecsSinceEpoch(changedMtime2.toMSecsSinceEpoch() / 1000 * 1000);

        // Ensure the initial mtimes are as expected
        auto initialFileInfo = FileInfo::A12_B12_C12_S12();
        initialFileInfo.setModTime("A/a1", initialMtime);
        initialFileInfo.setModTime("B/b1", initialMtime);
        initialFileInfo.setModTime("C/c1", initialMtime);

        FakeFolder fakeFolder{ initialFileInfo };


        // upload a
        fakeFolder.localModifier().appendByte("A/a1");
        fakeFolder.localModifier().setModTime("A/a1", changedMtime);
        // download b
        fakeFolder.remoteModifier().appendByte("B/b1");
        fakeFolder.remoteModifier().setModTime("B/b1", changedMtime);
        // conflict c
        fakeFolder.localModifier().appendByte("C/c1");
        fakeFolder.localModifier().appendByte("C/c1");
        fakeFolder.localModifier().setModTime("C/c1", changedMtime);
        fakeFolder.remoteModifier().appendByte("C/c1");
        fakeFolder.remoteModifier().setModTime("C/c1", changedMtime2);

        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, [&](const SyncFileItemSet &items) {
            SyncFileItemPtr a1, b1, c1;
            for (auto &item : items) {
                if (item->_file == "A/a1")
                    a1 = item;
                if (item->_file == "B/b1")
                    b1 = item;
                if (item->_file == "C/c1")
                    c1 = item;
            }

            // a1: should have local size and modtime
            QVERIFY(a1);
            QCOMPARE(a1->_instruction, CSYNC_INSTRUCTION_SYNC);
            QCOMPARE(a1->_direction, SyncFileItem::Up);
            QCOMPARE(a1->_size, qint64(5));

            QCOMPARE(Utility::qDateTimeFromTime_t(a1->_modtime), changedMtime);
            QCOMPARE(a1->_previousSize, qint64(4));
            QCOMPARE(Utility::qDateTimeFromTime_t(a1->_previousModtime), initialMtime);

            // b2: should have remote size and modtime
            QVERIFY(b1);
            QCOMPARE(b1->_instruction, CSYNC_INSTRUCTION_SYNC);
            QCOMPARE(b1->_direction, SyncFileItem::Down);
            QCOMPARE(b1->_size, qint64(17));
            QCOMPARE(Utility::qDateTimeFromTime_t(b1->_modtime), changedMtime);
            QCOMPARE(b1->_previousSize, qint64(16));
            QCOMPARE(Utility::qDateTimeFromTime_t(b1->_previousModtime), initialMtime);

            // c1: conflicts are downloads, so remote size and modtime
            QVERIFY(c1);
            QCOMPARE(c1->_instruction, CSYNC_INSTRUCTION_CONFLICT);
            QCOMPARE(c1->_direction, SyncFileItem::None);
            QCOMPARE(c1->_size, qint64(25));
            QCOMPARE(Utility::qDateTimeFromTime_t(c1->_modtime), changedMtime2);
            QCOMPARE(c1->_previousSize, qint64(26));
            QCOMPARE(Utility::qDateTimeFromTime_t(c1->_previousModtime), changedMtime);
        });

        QVERIFY(fakeFolder.syncOnce());
    }

    /**
     * Checks whether subsequent large uploads are skipped after a 507 error
     */
    void testInsufficientRemoteStorage()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

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

        fakeFolder.localModifier().insert("A/big", 800);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nPUT, 1);
        QCOMPARE(n507, 0);

        nPUT = 0;
        fakeFolder.localModifier().insert("A/big1", 500); // ok
        fakeFolder.localModifier().insert("A/big2", 1200); // 507 (quota guess now 1199)
        fakeFolder.localModifier().insert("A/big3", 1200); // skipped
        fakeFolder.localModifier().insert("A/big4", 1500); // skipped
        fakeFolder.localModifier().insert("A/big5", 1100); // 507 (quota guess now 1099)
        fakeFolder.localModifier().insert("A/big6", 900); // ok (quota guess now 199)
        fakeFolder.localModifier().insert("A/big7", 200); // skipped
        fakeFolder.localModifier().insert("A/big8", 199); // ok (quota guess now 0)

        fakeFolder.localModifier().insert("B/big8", 1150); // 507
        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nPUT, 6);
        QCOMPARE(n507, 3);
    }

    // Checks whether downloads with bad checksums are accepted
    void testChecksumValidation()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
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
        fakeFolder.remoteModifier().create("A/a3", 16, 'A');
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Bad OC-Checksum
        checksumValue = "SHA1:bad";
        fakeFolder.remoteModifier().create("A/a4", 16, 'A');
        QVERIFY(!fakeFolder.syncOnce());

        // Good OC-Checksum
        checksumValue = "SHA1:19b1928d58a2030d08023f3d7054516dbc186f20"; // printf 'A%.0s' {1..16} | sha1sum -
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        checksumValue = QByteArray();

        // Bad Content-MD5
        contentMd5Value = "bad";
        fakeFolder.remoteModifier().create("A/a5", 16, 'A');
        QVERIFY(!fakeFolder.syncOnce());

        // Good Content-MD5
        contentMd5Value = "d8a73157ce10cd94a91c2079fc9a92c8"; // printf 'A%.0s' {1..16} | md5sum -
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Invalid OC-Checksum is ignored
        checksumValue = "garbage";
        // contentMd5Value is still good
        fakeFolder.remoteModifier().create("A/a6", 16, 'A');
        QVERIFY(fakeFolder.syncOnce());
        contentMd5Value = "bad";
        fakeFolder.remoteModifier().create("A/a7", 16, 'A');
        QVERIFY(!fakeFolder.syncOnce());
        contentMd5Value.clear();
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // OC-Checksum contains Unsupported checksums
        checksumValue = "Unsupported:XXXX SHA1:invalid Invalid:XxX";
        fakeFolder.remoteModifier().create("A/a8", 16, 'A');
        QVERIFY(!fakeFolder.syncOnce()); // Since the supported SHA1 checksum is invalid, no download
        checksumValue =  "Unsupported:XXXX SHA1:19b1928d58a2030d08023f3d7054516dbc186f20 Invalid:XxX";
        fakeFolder.syncJournal().wipeErrorBlacklist();
        QVERIFY(fakeFolder.syncOnce()); // The supported SHA1 checksum is valid now, so the file are downloaded
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    // Tests the behavior of invalid filename detection
    void testInvalidFilenameRegex()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

#ifndef Q_OS_WIN  // We can't have local file with these character
        fakeFolder.localModifier().insert("A/\\:?*\"<>|.txt");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
#endif

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Check that new servers also accept the capability
        auto invalidFilenameRegexCapabilities = [](const QString &regex) {
            auto cap = TestUtils::testCapabilities();
            auto dav = cap["dav"].toMap();
            dav.insert({ { "invalidFilenameRegex", regex } });
            cap["dav"] = dav;
            return cap;
        };
        fakeFolder.syncEngine().account()->setCapabilities(invalidFilenameRegexCapabilities("my[fgh]ile"));
        fakeFolder.localModifier().insert("C/myfile.txt");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentRemoteState().find("C/myfile.txt"));
    }

    void testDiscoveryHiddenFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // We can't depend on currentLocalState for hidden files since
        // it should rightfully skip things like download temporaries
        auto localFileExists = [&](QString name) {
            return QFileInfo::exists(fakeFolder.localPath() + name);
        };

        fakeFolder.syncEngine().setIgnoreHiddenFiles(true);
        fakeFolder.remoteModifier().insert("A/.hidden");
        fakeFolder.localModifier().insert("B/.hidden");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!localFileExists("A/.hidden"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/.hidden"));

        fakeFolder.syncEngine().setIgnoreHiddenFiles(false);
        fakeFolder.syncJournal().forceRemoteDiscoveryNextSync();
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(localFileExists("A/.hidden"));
        QVERIFY(fakeFolder.currentRemoteState().find("B/.hidden"));
    }

    void testNoLocalEncoding()
    {
#ifndef Q_OS_WIN
        const auto utf8Locale = QTextCodec::codecForLocale();
        if (utf8Locale->mibEnum() != 106) {
            QSKIP(qUtf8Printable(QStringLiteral("Test only works for UTF8 locale, but current locale is %1").arg(QString::fromUtf8(utf8Locale->name()))));
        }
#endif

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Utf8 locale can sync both
        fakeFolder.remoteModifier().insert("A/tößt");
        fakeFolder.remoteModifier().insert("A/t𠜎t");
        fakeFolder.remoteModifier().insert("A/💩");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/tößt"));
        QVERIFY(fakeFolder.currentLocalState().find("A/t𠜎t"));
        QVERIFY(fakeFolder.currentLocalState().find("A/💩"));

#if !defined(Q_OS_MAC) && !defined(Q_OS_WIN)
        // Try again with a locale that can represent ö but not 𠜎 (4-byte utf8).
        auto codec = QTextCodec::codecForName("ISO-8859-15");
        QVERIFY(codec);
        QTextCodec::setCodecForLocale(codec);
        QCOMPARE(QTextCodec::codecForLocale()->mibEnum(), 111);

        fakeFolder.remoteModifier().insert("B/tößt");
        fakeFolder.remoteModifier().insert("B/t𠜎t");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("B/tößt"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/t𠜎t"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/t?t"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/t??t"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/t???t"));
        QVERIFY(!fakeFolder.currentLocalState().find("B/t????t"));
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("B/tößt"));
        QVERIFY(fakeFolder.currentRemoteState().find("B/t𠜎t"));

        // Try again with plain ascii
        codec = QTextCodec::codecForName("ASCII");
        if (codec) {
            QTextCodec::setCodecForLocale(codec);
            QCOMPARE(QTextCodec::codecForLocale()->mibEnum(), 3);

            fakeFolder.remoteModifier().insert("C/tößt");
            QVERIFY(fakeFolder.syncOnce());
            QVERIFY(!fakeFolder.currentLocalState().find("C/tößt"));
            QVERIFY(!fakeFolder.currentLocalState().find("C/t??t"));
            QVERIFY(!fakeFolder.currentLocalState().find("C/t????t"));
            QVERIFY(fakeFolder.syncOnce());
            QVERIFY(fakeFolder.currentRemoteState().find("C/tößt"));
        } else {
            qDebug() << "Skipping test for ASCII, ASCII is not available, available encodings are:" << QTextCodec::availableCodecs();
        }

        QTextCodec::setCodecForLocale(utf8Locale);
#endif
    }

    // Aborting has had bugs when there are parallel upload jobs
    void testUploadV1Multiabort()
    {
        FakeFolder fakeFolder{ FileInfo{} };
        SyncOptions options = fakeFolder.syncEngine().syncOptions();
        options._initialChunkSize = 10;
        options._maxChunkSize = 10;
        options._minChunkSize = 10;
        fakeFolder.syncEngine().setSyncOptions(options);
        auto cap = TestUtils::testCapabilities();
        // unset chunking v1
        cap.remove("dav");
        fakeFolder.account()->setCapabilities(cap);

        QObject parent;
        int nPUT = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                ++nPUT;
                return new FakeHangingReply(op, request, &parent);
            }
            return nullptr;
        });

        fakeFolder.localModifier().insert("file", 100, 'W');
        QTimer::singleShot(100ms, &fakeFolder.syncEngine(), [&]() { fakeFolder.syncEngine().abort(); });
        QVERIFY(!fakeFolder.syncOnce());

        QCOMPARE(nPUT, 3);
    }

#ifndef Q_OS_WIN
    void testPropagatePermissions()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        auto perm = QFileDevice::Permission(0x7704); // user/owner: rwx, group: r, other: -
        QFile::setPermissions(fakeFolder.localPath() + "A/a1", perm);
        QFile::setPermissions(fakeFolder.localPath() + "A/a2", perm);
        fakeFolder.syncOnce(); // get the metadata-only change out of the way
        fakeFolder.remoteModifier().appendByte("A/a1");
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.localModifier().appendByte("A/a2");
        fakeFolder.localModifier().appendByte("A/a2");
        fakeFolder.syncOnce(); // perms should be preserved
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a1").permissions(), perm);
        QCOMPARE(QFileInfo(fakeFolder.localPath() + "A/a2").permissions(), perm);

        auto conflictName = fakeFolder.syncJournal().conflictRecord(fakeFolder.syncJournal().conflictRecordPaths().first()).path;
        QVERIFY(conflictName.contains("A/a2"));
        QCOMPARE(QFileInfo(fakeFolder.localPath() + conflictName).permissions(), perm);
    }
#endif

    void testEmptyLocalButHasRemote()
    {
        FakeFolder fakeFolder{ FileInfo{} };
        fakeFolder.remoteModifier().mkdir("foo");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QVERIFY(fakeFolder.currentLocalState().find("foo"));

    }

    // Check that server mtime is set on directories on initial propagation
    void testDirectoryInitialMtime()
    {
        FakeFolder fakeFolder{ FileInfo{} };
        fakeFolder.remoteModifier().mkdir("foo");
        fakeFolder.remoteModifier().insert("foo/bar");
        auto datetime = QDateTime::currentDateTime();
        datetime.setSecsSinceEpoch(datetime.toSecsSinceEpoch()); // wipe ms
        fakeFolder.remoteModifier().find("foo")->setLastModified(datetime);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QCOMPARE(QFileInfo(fakeFolder.localPath() + "foo").lastModified(), datetime);
    }
};

QTEST_GUILESS_MAIN(TestSyncEngine)
#include "testsyncengine.moc"
