/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QTextCodec>

#include "syncenginetestutils.h"

#include "caseclashconflictsolver.h"
#include "configfile.h"
#include "propagatorjobs.h"
#include "syncengine.h"

#include <QFile>
#include <QtTest>

#include <filesystem>

#define DVSUFFIX APPLICATION_DOTVIRTUALFILE_SUFFIX

using namespace OCC;

namespace {

QStringList findCaseClashConflicts(const FileInfo &dir)
{
    QStringList conflicts;
    for (const auto &item : dir.children) {
        if (item.name.contains("(case clash from")) {
            conflicts.append(item.path());
        }
    }
    return conflicts;
}

bool expectConflict(FileInfo state, const QString path)
{
    PathComponents pathComponents(path);
    auto base = state.find(pathComponents.parentDirComponents());
    if (!base)
        return false;
    for (const auto &item : std::as_const(base->children)) {
        if (item.name.startsWith(pathComponents.fileName()) && item.name.contains("(case clash from")) {
            return true;
        }
    }
    return false;
}

bool itemDidComplete(const ItemCompletedSpy &spy, const QString &path)
{
    if (auto item = spy.findItem(path)) {
        return item->_instruction != CSYNC_INSTRUCTION_NONE && item->_instruction != CSYNC_INSTRUCTION_UPDATE_METADATA;
    }
    return false;
}

bool itemDidCompleteSuccessfully(const ItemCompletedSpy &spy, const QString &path)
{
    if (auto item = spy.findItem(path)) {
        return item->_status == SyncFileItem::Success;
    }
    return false;
}

bool itemDidCompleteSuccessfullyWithExpectedRank(const ItemCompletedSpy &spy, const QString &path, int rank)
{
    if (auto item = spy.findItemWithExpectedRank(path, rank)) {
        return item->_status == SyncFileItem::Success;
    }
    return false;
}

int itemSuccessfullyCompletedGetRank(const ItemCompletedSpy &spy, const QString &path)
{
    auto itItem = std::find_if(spy.begin(), spy.end(), [&path] (auto currentItem) {
        auto item = currentItem[0].template value<OCC::SyncFileItemPtr>();
        return item->destination() == path;
    });
    if (itItem != spy.end()) {
        return itItem - spy.begin();
    }
    return -1;
}

}

class TestSyncEngine : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Logger::instance()->setLogFlush(true);
        Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    }

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

    void testDirUploadWithDelayedAlgorithm() {
        QSKIP("bulk upload is disabled");

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"bulkupload", "1.0"} } } });

        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().mkdir("Y");
        fakeFolder.localModifier().insert("Y/d0");
        fakeFolder.localModifier().mkdir("Z");
        fakeFolder.localModifier().insert("Z/d0");
        fakeFolder.localModifier().insert("A/a0");
        fakeFolder.localModifier().insert("B/b0");
        fakeFolder.localModifier().insert("r0");
        fakeFolder.localModifier().insert("r1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfullyWithExpectedRank(completeSpy, "Y", 0));
        QVERIFY(itemDidCompleteSuccessfullyWithExpectedRank(completeSpy, "Z", 1));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Y/d0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "Y/d0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z/d0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "Z/d0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "A/a0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "B/b0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "B/b0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "r0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "r0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "r1"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "r1") > 1);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirUploadWithDelayedAlgorithmWithNewChecksum() {
        QSKIP("bulk upload is disabled");

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.setServerVersion(QStringLiteral("32.0.0"));
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"bulkupload", "1.0"} } } });

        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().mkdir("Y");
        fakeFolder.localModifier().insert("Y/d0");
        fakeFolder.localModifier().mkdir("Z");
        fakeFolder.localModifier().insert("Z/d0");
        fakeFolder.localModifier().insert("A/a0");
        fakeFolder.localModifier().insert("B/b0");
        fakeFolder.localModifier().insert("r0");
        fakeFolder.localModifier().insert("r1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfullyWithExpectedRank(completeSpy, "Y", 0));
        QVERIFY(itemDidCompleteSuccessfullyWithExpectedRank(completeSpy, "Z", 1));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Y/d0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "Y/d0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "Z/d0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "Z/d0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "A/a0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "B/b0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "B/b0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "r0"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "r0") > 1);
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "r1"));
        QVERIFY(itemSuccessfullyCompletedGetRank(completeSpy, "r1") > 1);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }


    void testLocalDelete_data()
    {
        QTest::addColumn<bool>("moveToTrashEnabled");
        QTest::newRow("move to trash") << true;
        QTest::newRow("delete") << false;
    }

    void testLocalDelete() {
        QFETCH(bool, moveToTrashEnabled);

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        auto syncOptions = fakeFolder.syncEngine().syncOptions();
        syncOptions._moveFilesToTrash = moveToTrashEnabled;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

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

    void testLocalDeleteWithReuploadForNewLocalFiles_data()
    {
        QTest::addColumn<bool>("moveToTrashEnabled");
        QTest::newRow("move to trash") << true;
        QTest::newRow("delete") << false;
    }

    void testEmlLocalChecksum() {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.localModifier().insert("a1.eml", 64, 'A');
        fakeFolder.localModifier().insert("a2.eml", 64, 'A');
        fakeFolder.localModifier().insert("a3.eml", 64, 'A');
        fakeFolder.localModifier().insert("b3.txt", 64, 'A');
        // Upload and calculate the checksums
        // fakeFolder.syncOnce();
        fakeFolder.syncOnce();

        auto getDbChecksum = [&](QString path) {
            SyncJournalFileRecord record;
            [[maybe_unused]] const auto result = fakeFolder.syncJournal().getFileRecord(path, &record);
            return record._checksumHeader;
        };

        // printf 'A%.0s' {1..64} | sha1sum -
        QByteArray referenceChecksum("SHA1:30b86e44e6001403827a62c58b08893e77cf121f");
        QCOMPARE(getDbChecksum("a1.eml"), referenceChecksum);
        QCOMPARE(getDbChecksum("a2.eml"), referenceChecksum);
        QCOMPARE(getDbChecksum("a3.eml"), referenceChecksum);
        QCOMPARE(getDbChecksum("b3.txt"), referenceChecksum);

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
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
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
            [[maybe_unused]] const auto result = fakeFolder.syncJournal().getFileRecord(file, &rec);
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
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QByteArrayLiteral("NewFolder"), &rec) && rec.isValid());
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
            << 1;

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
        auto &remoteInfo = fakeFolder.remoteModifier();

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
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QByteArray("A/a1"), &a1record));
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

        connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToPropagate, [&](SyncFileItemVector &items) {
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
        SyncOptions syncOptions;
        syncOptions._parallelNetworkJobs = 0;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        // Produce an error based on upload size
        int remoteQuota = 1000;
        int n507 = 0, nPUT = 0;
        QObject parent;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)

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
        QByteArray checksumValueRecalculated;
        QByteArray contentMd5Value;
        bool isChecksumRecalculateSupported = false;

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            if (op == QNetworkAccessManager::GetOperation) {
                auto reply = new FakeGetReply(fakeFolder.remoteModifier(), op, request, &parent);
                if (!checksumValue.isNull())
                    reply->setRawHeader(OCC::checkSumHeaderC, checksumValue);
                if (!contentMd5Value.isNull())
                    reply->setRawHeader(OCC::contentMd5HeaderC, contentMd5Value);
                return reply;
            } else if (op == QNetworkAccessManager::CustomOperation) {
                if (request.hasRawHeader(OCC::checksumRecalculateOnServerHeaderC)) {
                    if (!isChecksumRecalculateSupported) {
                        return new FakeErrorReply(op, request, &parent, 402);
                    }
                    auto reply = new FakeGetReply(fakeFolder.remoteModifier(), op, request, &parent);
                    reply->setRawHeader(OCC::checkSumHeaderC, checksumValueRecalculated);
                    return reply;
                }
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

        const QByteArray matchedSha1Checksum(QByteArrayLiteral("SHA1:19b1928d58a2030d08023f3d7054516dbc186f20"));
        const QByteArray mismatchedSha1Checksum(matchedSha1Checksum.chopped(1));

        // Good OC-Checksum
        checksumValue = matchedSha1Checksum; // printf 'A%.0s' {1..16} | sha1sum -
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        checksumValue = QByteArray();

        // Bad Content-MD5
        contentMd5Value = "bad";
        fakeFolder.remoteModifier().create("A/a5", 16, 'A');
        QVERIFY(!fakeFolder.syncOnce());

        // Good Content-MD5
        contentMd5Value = "d8a73157ce10cd94a91c2079fc9a92c8"; // printf 'A%.0s' {1..16} | md5sum -
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
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // OC-Checksum contains Unsupported checksums
        checksumValue = "Unsupported:XXXX SHA1:invalid Invalid:XxX";
        fakeFolder.remoteModifier().create("A/a8", 16, 'A');
        QVERIFY(!fakeFolder.syncOnce()); // Since the supported SHA1 checksum is invalid, no download
        checksumValue =  "Unsupported:XXXX SHA1:19b1928d58a2030d08023f3d7054516dbc186f20 Invalid:XxX";
        QVERIFY(fakeFolder.syncOnce()); // The supported SHA1 checksum is valid now, so the file are downloaded
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Begin Test mismatch recalculation---------------------------------------------------------------------------------

        const auto prevServerVersion = fakeFolder.account()->serverVersion();
        fakeFolder.account()->setServerVersion(QStringLiteral("%1.0.0").arg(fakeFolder.account()->checksumRecalculateServerVersionMinSupportedMajor()));

        // Mismatched OC-Checksum and X-Recalculate-Hash is not supported -> sync must fail
        isChecksumRecalculateSupported = false;
        checksumValue = mismatchedSha1Checksum;
        checksumValueRecalculated = matchedSha1Checksum;
        fakeFolder.remoteModifier().create("A/a9", 16, 'A');
        QVERIFY(!fakeFolder.syncOnce());

        // Mismatched OC-Checksum and X-Recalculate-Hash is supported, but, recalculated checksum is again mismatched -> sync must fail
        isChecksumRecalculateSupported = true;
        checksumValue = mismatchedSha1Checksum;
        checksumValueRecalculated = mismatchedSha1Checksum;
        QVERIFY(!fakeFolder.syncOnce());

        // Mismatched OC-Checksum and X-Recalculate-Hash is supported, and, recalculated checksum is a match -> sync must succeed
        isChecksumRecalculateSupported = true;
        checksumValue = mismatchedSha1Checksum;
        checksumValueRecalculated = matchedSha1Checksum;
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        checksumValue = QByteArray();

        fakeFolder.account()->setServerVersion(prevServerVersion);
        // End Test mismatch recalculation-----------------------------------------------------------------------------------
    }

    // Tests the behavior of invalid filename detection
    void testInvalidFilenameRegex()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

#ifndef Q_OS_WIN  // We can't have local file with these character
        // For current servers, no characters are forbidden
        fakeFolder.syncEngine().account()->setServerVersion("10.0.0");
        fakeFolder.localModifier().insert("A/\\:?*\"<>|.txt");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // For legacy servers, some characters were forbidden by the client
        fakeFolder.syncEngine().account()->setServerVersion("8.0.0");
        fakeFolder.localModifier().insert("B/\\:?*\"<>|.txt");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(!fakeFolder.currentRemoteState().find("B/\\:?*\"<>|.txt"));
#endif

        // We can override that by setting the capability
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "invalidFilenameRegex", "" } } } });
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Check that new servers also accept the capability
        fakeFolder.syncEngine().account()->setServerVersion("10.0.0");
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ { "invalidFilenameRegex", "my[fgh]ile" } } } });
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
            return QFileInfo(fakeFolder.localPath() + name).exists();
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
        auto utf8Locale = QTextCodec::codecForLocale();
        if (utf8Locale->mibEnum() != 106) {
            QSKIP("Test only works for UTF8 locale");
        }

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Utf8 locale can sync both
        fakeFolder.remoteModifier().insert("A/tößt");
        fakeFolder.remoteModifier().insert("A/t𠜎t");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("A/tößt"));
        QVERIFY(fakeFolder.currentLocalState().find("A/t𠜎t"));

#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN)
        try {
            // Try again with a locale that can represent ö but not 𠜎 (4-byte utf8).
            QTextCodec::setCodecForLocale(QTextCodec::codecForName("ISO-8859-15"));
            QVERIFY(QTextCodec::codecForLocale()->mibEnum() == 111);

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
            QTextCodec::setCodecForLocale(QTextCodec::codecForName("ASCII"));
            QVERIFY(QTextCodec::codecForLocale()->mibEnum() == 3);

            fakeFolder.remoteModifier().insert("C/tößt");
            QVERIFY(fakeFolder.syncOnce());
            QVERIFY(!fakeFolder.currentLocalState().find("C/tößt"));
            QVERIFY(!fakeFolder.currentLocalState().find("C/t??t"));
            QVERIFY(!fakeFolder.currentLocalState().find("C/t????t"));
            QVERIFY(fakeFolder.syncOnce());
            QVERIFY(fakeFolder.currentRemoteState().find("C/tößt"));

        }
        catch (const std::filesystem::filesystem_error &e)
        {
            qCritical() << e.what() << e.path1().c_str() << e.path2().c_str() << e.code().message().c_str();
        }
        catch (const std::system_error &e)
        {
            qCritical() << e.what() << e.code().message().c_str();
        }
        catch (...)
        {
            qCritical() << "exception unknown";
        }
        QTextCodec::setCodecForLocale(utf8Locale);
#endif
    }

    // Aborting has had bugs when there are parallel upload jobs
    void testUploadV1Multiabort()
    {
        FakeFolder fakeFolder{ FileInfo{} };
        SyncOptions options;
        options._initialChunkSize = 10;
        options.setMaxChunkSize(10);
        options.setMinChunkSize(10);
        fakeFolder.syncEngine().setSyncOptions(options);

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
        QTimer::singleShot(100, &fakeFolder.syncEngine(), [&]() { fakeFolder.syncEngine().abort(); });
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
        fakeFolder.remoteModifier().find("foo")->lastModified = datetime;

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        QCOMPARE(QFileInfo(fakeFolder.localPath() + "foo").lastModified(), datetime);
    }

    // A local file should not be modified after upload to server if nothing has changed.
    void testLocalFileInitialMtime()
    {
        constexpr auto fooFolder = "foo/";
        constexpr auto barFile = "foo/bar";

        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.localModifier().mkdir(fooFolder);
        fakeFolder.localModifier().insert(barFile);

        const auto localDiskFileModifier = dynamic_cast<DiskFileModifier &>(fakeFolder.localModifier());

        const auto localFile = localDiskFileModifier.find(barFile);
        const auto localFileInfo = QFileInfo(localFile);
        const auto expectedMtime = localFileInfo.metadataChangeTime();

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const auto currentMtime = localFileInfo.metadataChangeTime();
        QCOMPARE(currentMtime, expectedMtime);
    }

    /**
     * Checks whether subsequent large uploads are skipped after a 507 error
     */
    void testErrorsWithBulkUpload()
    {
        QSKIP("bulk upload is disabled");

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"bulkupload", "1.0"} } } });

        // Disable parallel uploads
        SyncOptions syncOptions;
        syncOptions._parallelNetworkJobs = 0;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        int nPUT = 0;
        int nPOST = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            auto contentType = request.header(QNetworkRequest::ContentTypeHeader).toString();
            if (op == QNetworkAccessManager::PostOperation) {
                ++nPOST;
                if (contentType.startsWith(QStringLiteral("multipart/related; boundary="))) {
                    auto hasAnError = false;
                    auto jsonReplyObject = fakeFolder.forEachReplyPart(outgoingData, contentType, [&hasAnError] (const QMap<QString, QByteArray> &allHeaders) -> QJsonObject {
                        auto reply = QJsonObject{};
                        const auto fileName = allHeaders[QStringLiteral("x-file-path")];
                        if (fileName.endsWith("A/big2") ||
                                fileName.endsWith("A/big3") ||
                                fileName.endsWith("A/big4") ||
                                fileName.endsWith("A/big5") ||
                                fileName.endsWith("A/big7") ||
                                fileName.endsWith("B/big8")) {
                            hasAnError = true;
                            reply.insert(QStringLiteral("error"), true);
                            reply.insert(QStringLiteral("etag"), {});
                            return reply;
                        } else {
                            reply.insert(QStringLiteral("error"), false);
                            reply.insert(QStringLiteral("etag"), {});
                        }
                        return reply;
                    });
                    if (jsonReplyObject.size()) {
                        auto jsonReply = QJsonDocument{};
                        jsonReply.setObject(jsonReplyObject);
                        if (hasAnError) {
                            return new FakeJsonErrorReply{op, request, this, 200, jsonReply};
                        } else {
                            return new FakeJsonReply{op, request, this, 200, jsonReply};
                        }
                    }
                    return  nullptr;
                }
            } else if (op == QNetworkAccessManager::PutOperation) {
                ++nPUT;
                const auto fileName = getFilePathFromUrl(request.url());
                if (fileName.endsWith("A/big2") ||
                        fileName.endsWith("A/big3") ||
                        fileName.endsWith("A/big4") ||
                        fileName.endsWith("A/big5") ||
                        fileName.endsWith("A/big7") ||
                        fileName.endsWith("B/big8")) {
                    return new FakeErrorReply(op, request, this, 412);
                }
                return  nullptr;
            }
            return  nullptr;
        });

        fakeFolder.localModifier().insert("A/big", 1);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nPUT, 0);
        QCOMPARE(nPOST, 1);
        nPUT = 0;
        nPOST = 0;

        fakeFolder.localModifier().insert("A/big1", 1); // ok
        fakeFolder.localModifier().insert("A/big2", 1); // ko
        fakeFolder.localModifier().insert("A/big3", 1); // ko
        fakeFolder.localModifier().insert("A/big4", 1); // ko
        fakeFolder.localModifier().insert("A/big5", 1); // ko
        fakeFolder.localModifier().insert("A/big6", 1); // ok
        fakeFolder.localModifier().insert("A/big7", 1); // ko
        fakeFolder.localModifier().insert("A/big8", 1); // ok
        fakeFolder.localModifier().insert("B/big8", 1); // ko

        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nPUT, 0);
        QCOMPARE(nPOST, 1);
        nPUT = 0;
        nPOST = 0;

        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nPUT, 6);
        QCOMPARE(nPOST, 0);
    }

    /**
     * Checks whether subsequent large uploads are skipped after a 507 error
     */
    void testNetworkErrorsWithBulkUpload()
    {
        QSKIP("bulk upload is disabled");

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"bulkupload", "1.0"} } } });

        // Disable parallel uploads
        SyncOptions syncOptions;
        syncOptions._parallelNetworkJobs = 0;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        int nPUT = 0;
        int nPOST = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *) -> QNetworkReply * {
            auto contentType = request.header(QNetworkRequest::ContentTypeHeader).toString();
            if (op == QNetworkAccessManager::PostOperation) {
                ++nPOST;
                if (contentType.startsWith(QStringLiteral("multipart/related; boundary="))) {
                    return new FakeErrorReply(op, request, this, 400);
                }
                return  nullptr;
            } else if (op == QNetworkAccessManager::PutOperation) {
                ++nPUT;
            }
            return  nullptr;
        });

        fakeFolder.localModifier().insert("A/big1", 1);
        fakeFolder.localModifier().insert("A/big2", 1);
        fakeFolder.localModifier().insert("A/big3", 1);
        fakeFolder.localModifier().insert("A/big4", 1);
        fakeFolder.localModifier().insert("A/big5", 1);
        fakeFolder.localModifier().insert("A/big6", 1);
        fakeFolder.localModifier().insert("A/big7", 1);
        fakeFolder.localModifier().insert("A/big8", 1);
        fakeFolder.localModifier().insert("B/big8", 1);

        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nPUT, 0);
        QCOMPARE(nPOST, 1);
        nPUT = 0;
        nPOST = 0;

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nPUT, 9);
        QCOMPARE(nPOST, 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testNetworkErrorsWithSmallerBatchSizes()
    {
        QSKIP("bulk upload is disabled");

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"bulkupload", "1.0"} } } });

        int nPUT = 0;
        int nPOST = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            auto contentType = request.header(QNetworkRequest::ContentTypeHeader).toString();
            if (op == QNetworkAccessManager::PostOperation) {
                ++nPOST;
                if (contentType.startsWith(QStringLiteral("multipart/related; boundary="))) {
                    auto jsonReplyObject = fakeFolder.forEachReplyPart(outgoingData, contentType, [] (const QMap<QString, QByteArray> &allHeaders) -> QJsonObject {
                        auto reply = QJsonObject{};
                        const auto fileName = allHeaders[QStringLiteral("X-File-Path")];
                        if(fileName.endsWith("B/small30") ||
                            fileName.endsWith("B/small60") ||
                            fileName.endsWith("B/big30") ||
                            fileName.endsWith("B/big60")) {
                            reply.insert(QStringLiteral("error"), true);
                            reply.insert(QStringLiteral("etag"), {});
                            return reply;
                        } else {
                            reply.insert(QStringLiteral("error"), false);
                            reply.insert(QStringLiteral("etag"), {});
                        }
                        return reply;
                    });
                    if (jsonReplyObject.size()) {
                        auto jsonReply = QJsonDocument{};
                        jsonReply.setObject(jsonReplyObject);
                        return new FakeJsonErrorReply{op, request, this, 200, jsonReply};
                    }
                    return  nullptr;
                }
            } else if (op == QNetworkAccessManager::PutOperation) {
                ++nPUT;
                const auto fileName = getFilePathFromUrl(request.url());
                if (fileName.endsWith("B/small30") ||
                    fileName.endsWith("B/small60") ||
                    fileName.endsWith("B/big30") ||
                    fileName.endsWith("B/big60")) {
                    return new FakeErrorReply(op, request, this, 504);
                }
                return  nullptr;
            }
            return  nullptr;
        });

        const auto smallSize = 0.5 * 1000 * 1000;
        const auto bigSize = 10 * 1000 * 1000;

        for(auto i = 0 ; i < 120; ++i) {
            fakeFolder.localModifier().insert(QString("A/small%1").arg(i), smallSize);
        }

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nPUT, 0);
        QCOMPARE(nPOST, 1);
        nPUT = 0;
        nPOST = 0;

        for(auto i = 0 ; i < 120; ++i) {
            fakeFolder.localModifier().insert(QString("B/small%1").arg(i), smallSize);
            fakeFolder.localModifier().insert(QString("B/big%1").arg(i), bigSize);
        }

        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nPUT, 120);
        QCOMPARE(nPOST, 1);
        nPUT = 0;
        nPOST = 0;

        QVERIFY(!fakeFolder.syncOnce());
        QCOMPARE(nPUT, 0);
        QCOMPARE(nPOST, 0);
    }

    void testRemoteMoveFailedInsufficientStorageLocalMoveRolledBack()
    {
        FakeFolder fakeFolder{FileInfo{}};

        // create a big shared folder with some files
        fakeFolder.remoteModifier().mkdir("big_shared_folder");
        fakeFolder.remoteModifier().mkdir("big_shared_folder/shared_files");
        fakeFolder.remoteModifier().insert("big_shared_folder/shared_files/big_shared_file_A.data", 1000);
        fakeFolder.remoteModifier().insert("big_shared_folder/shared_files/big_shared_file_B.data", 1000);

        // make sure big shared folder is synced
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_B.data"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // try to move from a big shared folder to your own folder
        fakeFolder.localModifier().mkdir("own_folder");
        fakeFolder.localModifier().rename(
            "big_shared_folder/shared_files/big_shared_file_A.data", "own_folder/big_shared_file_A.data");
        fakeFolder.localModifier().rename(
            "big_shared_folder/shared_files/big_shared_file_B.data", "own_folder/big_shared_file_B.data");

        // emulate server MOVE 507 error
        QObject parent;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request,
                                         QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)

            if (op == QNetworkAccessManager::CustomOperation
                && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("MOVE")) {
                return new FakeErrorReply(op, request, &parent, 507);
            }
            return nullptr;
        });

        // make sure the first sync fails and files get restored to original folder
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_B.data"));
        QVERIFY(!fakeFolder.currentLocalState().find("own_folder/big_shared_file_A.data"));
        QVERIFY(!fakeFolder.currentLocalState().find("own_folder/big_shared_file_B.data"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteMoveFailedForbiddenLocalMoveRolledBack()
    {
        FakeFolder fakeFolder{FileInfo{}};

        // create a big shared folder with some files
        fakeFolder.remoteModifier().mkdir("big_shared_folder");
        fakeFolder.remoteModifier().mkdir("big_shared_folder/shared_files");
        fakeFolder.remoteModifier().insert("big_shared_folder/shared_files/big_shared_file_A.data", 1000);
        fakeFolder.remoteModifier().insert("big_shared_folder/shared_files/big_shared_file_B.data", 1000);

        // make sure big shared folder is synced
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_B.data"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // try to move from a big shared folder to your own folder
        fakeFolder.localModifier().mkdir("own_folder");
        fakeFolder.localModifier().rename(
            "big_shared_folder/shared_files/big_shared_file_A.data", "own_folder/big_shared_file_A.data");
        fakeFolder.localModifier().rename(
            "big_shared_folder/shared_files/big_shared_file_B.data", "own_folder/big_shared_file_B.data");

        // emulate server MOVE 507 error
        QObject parent;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request,
                                         QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)

            auto attributeCustomVerb = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();

            if (op == QNetworkAccessManager::CustomOperation
                && request.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("MOVE")) {
                return new FakeErrorReply(op, request, &parent, 403);
            }
            return nullptr;
        });

        // make sure the first sync fails and files get restored to original folder
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_A.data"));
        QVERIFY(fakeFolder.currentLocalState().find("big_shared_folder/shared_files/big_shared_file_B.data"));
        QVERIFY(!fakeFolder.currentLocalState().find("own_folder/big_shared_file_A.data"));
        QVERIFY(!fakeFolder.currentLocalState().find("own_folder/big_shared_file_B.data"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testFolderWithFilesInError()
    {
        FakeFolder fakeFolder{FileInfo{}};

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *outgoingData) -> QNetworkReply * {
            Q_UNUSED(outgoingData)

            if (op == QNetworkAccessManager::GetOperation) {
                const auto fileName = getFilePathFromUrl(request.url());
                if (fileName == QStringLiteral("aaa/subfolder/foo")) {
                    return new FakeErrorReply(op, request, &fakeFolder.syncEngine(), 403);
                }
            }
            return nullptr;
        });

        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa/subfolder"));
        fakeFolder.remoteModifier().insert(QStringLiteral("aaa/subfolder/bar"));

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().insert(QStringLiteral("aaa/subfolder/foo"));
        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.syncOnce());
    }

    void testInvalidMtimeRecoveryAtStart()
    {
        constexpr auto INVALID_MTIME = 0;
        constexpr auto CURRENT_MTIME = 1646057277;

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const QString fooFileRootFolder("foo");
        const QString barFileRootFolder("bar");
        const QString fooFileSubFolder("subfolder/foo");
        const QString barFileSubFolder("subfolder/bar");
        const QString fooFileAaaSubFolder("aaa/subfolder/foo");
        const QString barFileAaaSubFolder("aaa/subfolder/bar");

        fakeFolder.remoteModifier().insert(fooFileRootFolder);
        fakeFolder.remoteModifier().insert(barFileRootFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("subfolder"));
        fakeFolder.remoteModifier().insert(fooFileSubFolder);
        fakeFolder.remoteModifier().insert(barFileSubFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa/subfolder"));
        fakeFolder.remoteModifier().insert(fooFileAaaSubFolder);
        fakeFolder.remoteModifier().setModTime(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MTIME));
        fakeFolder.remoteModifier().insert(barFileAaaSubFolder);
        fakeFolder.remoteModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MTIME));

        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.syncOnce());

        fakeFolder.remoteModifier().setModTimeKeepEtag(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.remoteModifier().setModTimeKeepEtag(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());

        auto expectedState = fakeFolder.currentLocalState();
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }

    void testInvalidMtimeRecovery()
    {
        constexpr auto INVALID_MTIME = 0;
        constexpr auto CURRENT_MTIME = 1646057277;

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const QString fooFileRootFolder("foo");
        const QString barFileRootFolder("bar");
        const QString fooFileSubFolder("subfolder/foo");
        const QString barFileSubFolder("subfolder/bar");
        const QString fooFileAaaSubFolder("aaa/subfolder/foo");
        const QString barFileAaaSubFolder("aaa/subfolder/bar");

        fakeFolder.remoteModifier().insert(fooFileRootFolder);
        fakeFolder.remoteModifier().insert(barFileRootFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("subfolder"));
        fakeFolder.remoteModifier().insert(fooFileSubFolder);
        fakeFolder.remoteModifier().insert(barFileSubFolder);
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa"));
        fakeFolder.remoteModifier().mkdir(QStringLiteral("aaa/subfolder"));
        fakeFolder.remoteModifier().insert(fooFileAaaSubFolder);
        fakeFolder.remoteModifier().insert(barFileAaaSubFolder);

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().setModTime(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MTIME));
        fakeFolder.remoteModifier().setModTime(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(INVALID_MTIME));

        QVERIFY(!fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.syncOnce());

        fakeFolder.remoteModifier().setModTimeKeepEtag(fooFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));
        fakeFolder.remoteModifier().setModTimeKeepEtag(barFileAaaSubFolder, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(fakeFolder.syncOnce());

        auto expectedState = fakeFolder.currentLocalState();
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }

    void testLocalInvalidMtimeCorrection()
    {
        const auto INVALID_MTIME = QDateTime::fromSecsSinceEpoch(0);
        const auto RECENT_MTIME = QDateTime::fromSecsSinceEpoch(1743004783); // 2025-03-26T16:59:43+0100

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.localModifier().insert(QStringLiteral("invalid"));
        fakeFolder.localModifier().setModTime("invalid", INVALID_MTIME);
        fakeFolder.localModifier().insert(QStringLiteral("recent"));
        fakeFolder.localModifier().setModTime("recent", RECENT_MTIME);

        QVERIFY(fakeFolder.syncOnce());

        // "invalid" file had a mtime of 0, so it's been updated to the current time during testing
        const auto currentMtime = fakeFolder.currentLocalState().find("invalid")->lastModified;
        QCOMPARE_GT(currentMtime, RECENT_MTIME);
        QCOMPARE_GT(fakeFolder.currentRemoteState().find("invalid")->lastModified, RECENT_MTIME);

        // "recent" file had a mtime of RECENT_MTIME, so it shouldn't have been changed
        QCOMPARE(fakeFolder.currentLocalState().find("recent")->lastModified, RECENT_MTIME);
        QCOMPARE(fakeFolder.currentRemoteState().find("recent")->lastModified, RECENT_MTIME);

        QVERIFY(fakeFolder.syncOnce());

        // verify that the mtime of "invalid" hasn't changed since the last sync that fixed it
        QCOMPARE(fakeFolder.currentLocalState().find("invalid")->lastModified, currentMtime);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testLocalInvalidMtimeCorrectionBulkUpload()
    {
        QSKIP("bulk upload is disabled");

        const auto INVALID_MTIME = QDateTime::fromSecsSinceEpoch(0);
        const auto RECENT_MTIME = QDateTime::fromSecsSinceEpoch(1743004783); // 2025-03-26T16:59:43+0100

        FakeFolder fakeFolder{FileInfo{}};
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"bulkupload", "1.0"} } } });

        fakeFolder.localModifier().insert(QStringLiteral("invalid"));
        fakeFolder.localModifier().setModTime("invalid", INVALID_MTIME);
        fakeFolder.localModifier().insert(QStringLiteral("recent"));
        fakeFolder.localModifier().setModTime("recent", RECENT_MTIME);

        QVERIFY(fakeFolder.syncOnce()); // this will use the BulkPropagatorJob

        // "invalid" file had a mtime of 0, so it's been updated to the current time during testing
        const auto currentMtime = fakeFolder.currentLocalState().find("invalid")->lastModified;
        QCOMPARE_GT(currentMtime, RECENT_MTIME);
        QCOMPARE_GT(fakeFolder.currentRemoteState().find("invalid")->lastModified, RECENT_MTIME);

        // "recent" file had a mtime of RECENT_MTIME, so it shouldn't have been changed
        QCOMPARE(fakeFolder.currentLocalState().find("recent")->lastModified, RECENT_MTIME);
        QCOMPARE(fakeFolder.currentRemoteState().find("recent")->lastModified, RECENT_MTIME);

        QVERIFY(fakeFolder.syncOnce()); // this will not propagate anything

        // verify that the mtime of "invalid" hasn't changed since the last sync that fixed it
        QCOMPARE(fakeFolder.currentLocalState().find("invalid")->lastModified, currentMtime);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testServerUpdatingMTimeShouldNotCreateConflicts()
    {
        constexpr auto testFile = "test.txt";
        constexpr auto CURRENT_MTIME = 1646057277;

        FakeFolder fakeFolder{ FileInfo{} };

        fakeFolder.remoteModifier().insert(testFile);
        fakeFolder.remoteModifier().setModTimeKeepEtag(testFile, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME - 2));

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());
        auto localState = fakeFolder.currentLocalState();
        QCOMPARE(localState, fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        const auto fileFirstSync = localState.find(testFile);

        QVERIFY(fileFirstSync);
        QCOMPARE(fileFirstSync->lastModified.toSecsSinceEpoch(), CURRENT_MTIME - 2);

        fakeFolder.remoteModifier().setModTimeKeepEtag(testFile, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME - 1));

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::FilesystemOnly);
        QVERIFY(fakeFolder.syncOnce());
        localState = fakeFolder.currentLocalState();
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        const auto fileSecondSync = localState.find(testFile);
        QVERIFY(fileSecondSync);
        QCOMPARE(fileSecondSync->lastModified.toSecsSinceEpoch(), CURRENT_MTIME - 1);

        fakeFolder.remoteModifier().setModTime(testFile, QDateTime::fromSecsSinceEpoch(CURRENT_MTIME));

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::FilesystemOnly);
        QVERIFY(fakeFolder.syncOnce());
        localState = fakeFolder.currentLocalState();
        QCOMPARE(localState, fakeFolder.currentRemoteState());
        QCOMPARE(printDbData(fakeFolder.dbState()), printDbData(fakeFolder.currentRemoteState()));
        const auto fileThirdSync = localState.find(testFile);
        QVERIFY(fileThirdSync);
        QCOMPARE(fileThirdSync->lastModified.toSecsSinceEpoch(), CURRENT_MTIME);
    }

    void testFolderRemovalWithCaseClash_data()
    {
        QTest::addColumn<bool>("moveToTrashEnabled");
        QTest::newRow("move to trash") << true;
        QTest::newRow("delete") << false;
    }

    void testFolderRemovalWithCaseClash()
    {
        QFETCH(bool, moveToTrashEnabled);

        FakeFolder fakeFolder{FileInfo{}};

        auto syncOptions = fakeFolder.syncEngine().syncOptions();
        syncOptions._moveFilesToTrash = moveToTrashEnabled;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        fakeFolder.remoteModifier().mkdir("A");
        fakeFolder.remoteModifier().mkdir("toDelete");
        fakeFolder.remoteModifier().insert("A/file");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().insert("A/FILE");
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().remove("toDelete");

        QVERIFY(fakeFolder.syncOnce());
        auto folderA = fakeFolder.currentLocalState().find("toDelete");
        QCOMPARE(folderA, nullptr);
    }

    void testServer_caseClash_createConflict()
    {
        constexpr auto testLowerCaseFile = "test";
        constexpr auto testUpperCaseFile = "TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };

        fakeFolder.remoteModifier().insert("otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testServer_subFolderCaseClash_createConflict()
    {
        constexpr auto testLowerCaseFile = "a/b/test";
        constexpr auto testUpperCaseFile = "a/b/TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };

        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().mkdir("a/b");
        fakeFolder.remoteModifier().insert("a/b/otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testServer_caseClash_createConflictOnMove()
    {
        constexpr auto testLowerCaseFile = "test";
        constexpr auto testUpperCaseFile = "TEST2";
        constexpr auto testUpperCaseFileAfterMove = "TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };

        fakeFolder.remoteModifier().insert("otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, false);

        fakeFolder.remoteModifier().rename(testUpperCaseFile, testUpperCaseFileAfterMove);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflictAfterMove = expectConflict(fakeFolder.currentLocalState(), testUpperCaseFileAfterMove);
        QCOMPARE(hasConflictAfterMove, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testServer_subFolderCaseClash_createConflictOnMove()
    {
        constexpr auto testLowerCaseFile = "a/b/test";
        constexpr auto testUpperCaseFile = "a/b/TEST2";
        constexpr auto testUpperCaseFileAfterMove = "a/b/TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };

        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().mkdir("a/b");
        fakeFolder.remoteModifier().insert("a/b/otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, false);

        fakeFolder.remoteModifier().rename(testUpperCaseFile, testUpperCaseFileAfterMove);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflictAfterMove = expectConflict(fakeFolder.currentLocalState(), testUpperCaseFileAfterMove);
        QCOMPARE(hasConflictAfterMove, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
    }

    void testServer_caseClash_createConflictAndSolveIt()
    {
        constexpr auto testLowerCaseFile = "test";
        constexpr auto testUpperCaseFile = "TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };

        fakeFolder.remoteModifier().insert("otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);

        if (shouldHaveCaseClashConflict) {
            const auto conflictFileName = QString{conflicts.constFirst()};
            qDebug() << conflictFileName;
            CaseClashConflictSolver conflictSolver(fakeFolder.localPath() + testLowerCaseFile,
                                                   fakeFolder.localPath() + conflictFileName,
                                                   QStringLiteral("/"),
                                                   fakeFolder.localPath(),
                                                   fakeFolder.account(),
                                                   &fakeFolder.syncJournal());

            QSignalSpy conflictSolverDone(&conflictSolver, &CaseClashConflictSolver::done);
            QSignalSpy conflictSolverFailed(&conflictSolver, &CaseClashConflictSolver::failed);

            conflictSolver.solveConflict("test2");

            QVERIFY(conflictSolverDone.wait());

            QVERIFY(fakeFolder.syncOnce());

            conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
            QCOMPARE(conflicts.size(), 0);
        }
    }

    void testServer_subFolderCaseClash_createConflictAndSolveIt()
    {
        constexpr auto testLowerCaseFile = "a/b/test";
        constexpr auto testUpperCaseFile = "a/b/TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{ FileInfo{} };

        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().mkdir("a/b");
        fakeFolder.remoteModifier().insert("a/b/otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);

        if (shouldHaveCaseClashConflict) {
            CaseClashConflictSolver conflictSolver(fakeFolder.localPath() + testLowerCaseFile,
                                                   fakeFolder.localPath() + conflicts.constFirst(),
                                                   QStringLiteral("/"),
                                                   fakeFolder.localPath(),
                                                   fakeFolder.account(),
                                                   &fakeFolder.syncJournal());

            QSignalSpy conflictSolverDone(&conflictSolver, &CaseClashConflictSolver::done);
            QSignalSpy conflictSolverFailed(&conflictSolver, &CaseClashConflictSolver::failed);

            conflictSolver.solveConflict("a/b/test2");

            QVERIFY(conflictSolverDone.wait());

            QVERIFY(fakeFolder.syncOnce());

            conflicts = findCaseClashConflicts(*fakeFolder.currentLocalState().find("a/b"));
            QCOMPARE(conflicts.size(), 0);
        }
    }

    void testServer_caseClash_createConflict_thenRemoveOneRemoteFile_data()
    {
        QTest::addColumn<bool>("moveToTrashEnabled");
        QTest::newRow("move to trash") << true;
        QTest::newRow("delete") << false;
    }

    void testServer_caseClash_createConflict_thenRemoveOneRemoteFile()
    {
        QFETCH(bool, moveToTrashEnabled);

        constexpr auto testLowerCaseFile = "test";
        constexpr auto testUpperCaseFile = "TEST";

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif

        FakeFolder fakeFolder{FileInfo{}};

        auto syncOptions = fakeFolder.syncEngine().syncOptions();
        syncOptions._moveFilesToTrash = moveToTrashEnabled;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        fakeFolder.remoteModifier().insert("otherFile.txt");
        fakeFolder.remoteModifier().insert(testLowerCaseFile);
        fakeFolder.remoteModifier().insert(testUpperCaseFile);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        auto conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);
        const auto hasConflict = expectConflict(fakeFolder.currentLocalState(), testLowerCaseFile);
        QCOMPARE(hasConflict, shouldHaveCaseClashConflict ? true : false);

        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());

        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);

        // remove (UPPERCASE) file
        fakeFolder.remoteModifier().remove(testUpperCaseFile);
        QVERIFY(fakeFolder.syncOnce());

        // make sure we got no conflicts now (conflicted copy gets removed)
        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), 0);

        // insert (UPPERCASE) file back
        fakeFolder.remoteModifier().insert(testUpperCaseFile);
        QVERIFY(fakeFolder.syncOnce());

        // we must get conflicts
        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), shouldHaveCaseClashConflict ? 1 : 0);

        // now remove (lowercase) file
        fakeFolder.remoteModifier().remove(testLowerCaseFile);
        QVERIFY(fakeFolder.syncOnce());

        // make sure we got no conflicts now (conflicted copy gets removed)
        conflicts = findCaseClashConflicts(fakeFolder.currentLocalState());
        QCOMPARE(conflicts.size(), 0);

        // remove the other file
        fakeFolder.remoteModifier().remove(testUpperCaseFile);
        QVERIFY(fakeFolder.syncOnce());
    }

    void testServer_caseClash_createDiverseConflictsInsideOneFolderAndSolveThem()
    {
        FakeFolder fakeFolder{FileInfo{}};

        const QStringList conflictsFolderPathComponents = {"Documents", "DiverseConflicts"};

        QString diverseConflictsFolderPath;
        for (const auto &conflictsFolderPathComponent : conflictsFolderPathComponents) {
            if (diverseConflictsFolderPath.isEmpty()) {
                diverseConflictsFolderPath += conflictsFolderPathComponent;
            } else {
                diverseConflictsFolderPath += "/" + conflictsFolderPathComponent;
            }
            fakeFolder.remoteModifier().mkdir(diverseConflictsFolderPath);
        }

        constexpr auto testLowerCaseFile = "testfile";
        constexpr auto testUpperCaseFile = "TESTFILE";

        constexpr auto testLowerCaseFolder = "testfolder";
        constexpr auto testUpperCaseFolder = "TESTFOLDER";

        constexpr auto testInvalidCharFolder = "Really?";

        fakeFolder.remoteModifier().insert(diverseConflictsFolderPath + "/" + testLowerCaseFile);
        fakeFolder.remoteModifier().insert(diverseConflictsFolderPath + "/" + testUpperCaseFile);

        fakeFolder.remoteModifier().mkdir(diverseConflictsFolderPath + "/" + testLowerCaseFolder);
        fakeFolder.remoteModifier().mkdir(diverseConflictsFolderPath + "/" + testUpperCaseFolder);

        fakeFolder.remoteModifier().mkdir(diverseConflictsFolderPath + "/" + testInvalidCharFolder);

#if defined Q_OS_LINUX
        constexpr auto shouldHaveCaseClashConflict = false;
#else
        constexpr auto shouldHaveCaseClashConflict = true;
#endif
        if (shouldHaveCaseClashConflict) {
            ItemCompletedSpy completeSpy(fakeFolder);

            fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
            QVERIFY(fakeFolder.syncOnce());

            // verify the parent of a folder where caseclash and invalidchar conflicts were found, has corresponding flags set (conflict info must get
            // propagated to the very top)
            const auto diverseConflictsFolderParent = completeSpy.findItem(conflictsFolderPathComponents.first());
            QVERIFY(diverseConflictsFolderParent);
            QVERIFY(diverseConflictsFolderParent->_isAnyCaseClashChild);
            QVERIFY(diverseConflictsFolderParent->_isAnyInvalidCharChild);
            completeSpy.clear();

            auto diverseConflictsFolderInfo = fakeFolder.currentLocalState().findRecursive(diverseConflictsFolderPath);
            QVERIFY(!diverseConflictsFolderInfo.name.isEmpty());

            auto conflictsFile = findCaseClashConflicts(diverseConflictsFolderInfo);
            QCOMPARE(conflictsFile.size(), shouldHaveCaseClashConflict ? 1 : 0);
            const auto hasFileCaseClashConflict = expectConflict(diverseConflictsFolderInfo, testLowerCaseFile);
            QCOMPARE(hasFileCaseClashConflict, shouldHaveCaseClashConflict ? true : false);

            fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
            QVERIFY(fakeFolder.syncOnce());

            diverseConflictsFolderInfo = fakeFolder.currentLocalState().findRecursive(diverseConflictsFolderPath);
            QVERIFY(!diverseConflictsFolderInfo.name.isEmpty());

            conflictsFile = findCaseClashConflicts(diverseConflictsFolderInfo);
            QCOMPARE(conflictsFile.size(), shouldHaveCaseClashConflict ? 1 : 0);

            const auto conflictFileName = QString{conflictsFile.constFirst()};
            qDebug() << conflictFileName;
            CaseClashConflictSolver conflictSolver(fakeFolder.localPath() + diverseConflictsFolderPath + "/" + testLowerCaseFile,
                                                   fakeFolder.localPath() + conflictFileName,
                                                   QStringLiteral("/"),
                                                   fakeFolder.localPath(),
                                                   fakeFolder.account(),
                                                   &fakeFolder.syncJournal());

            QSignalSpy conflictSolverDone(&conflictSolver, &CaseClashConflictSolver::done);

            conflictSolver.solveConflict("testfile2");

            QVERIFY(conflictSolverDone.wait());

            QVERIFY(fakeFolder.syncOnce());

            diverseConflictsFolderInfo = fakeFolder.currentLocalState().findRecursive(diverseConflictsFolderPath);
            QVERIFY(!diverseConflictsFolderInfo.name.isEmpty());

            conflictsFile = findCaseClashConflicts(diverseConflictsFolderInfo);
            QCOMPARE(conflictsFile.size(), 0);

            fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
            QVERIFY(fakeFolder.syncOnce());

            // After solving file conflict, verify that we did not lose conflict detection of caseclash and invalidchar for folders
            for (auto it = completeSpy.begin(); it != completeSpy.end(); ++it) {
                auto item = (*it).first().value<OCC::SyncFileItemPtr>();
                item = nullptr;
            }
            auto invalidFilenameConflictFolderItem = completeSpy.findItem(diverseConflictsFolderPath + "/" + testInvalidCharFolder);
            QVERIFY(invalidFilenameConflictFolderItem);
            QVERIFY(invalidFilenameConflictFolderItem->_status == SyncFileItem::FileNameInvalid);

            auto caseClashConflictFolderItemLower = completeSpy.findItem(diverseConflictsFolderPath + "/" + testLowerCaseFolder);
            auto caseClashConflictFolderItemUpper = completeSpy.findItem(diverseConflictsFolderPath + "/" + testUpperCaseFolder);
            completeSpy.clear();

            // we always create UPPERCASE folder in current syncengine logic for now and then create a conflict for a lowercase folder, but this may change, so
            // keep this check more future proof
            const auto upperOrLowerCaseFolderCaseClashFound =
                (caseClashConflictFolderItemLower && caseClashConflictFolderItemLower->_status == SyncFileItem::FileNameClash)
                || (caseClashConflictFolderItemUpper && caseClashConflictFolderItemUpper->_status == SyncFileItem::FileNameClash);

            QVERIFY(upperOrLowerCaseFolderCaseClashFound);

            // solve case clash folders conflict
            CaseClashConflictSolver conflictSolverCaseClashForFolder(fakeFolder.localPath() + diverseConflictsFolderPath + "/" + testLowerCaseFolder,
                                                                     fakeFolder.localPath() + diverseConflictsFolderPath + "/" + testUpperCaseFolder,
                                                                     QStringLiteral("/"),
                                                                     fakeFolder.localPath(),
                                                                     fakeFolder.account(),
                                                                     &fakeFolder.syncJournal());

            QSignalSpy conflictSolverCaseClashForFolderDone(&conflictSolverCaseClashForFolder, &CaseClashConflictSolver::done);
            conflictSolverCaseClashForFolder.solveConflict("testfolder1");
            QVERIFY(conflictSolverCaseClashForFolderDone.wait());
            QVERIFY(fakeFolder.syncOnce());

            // veriy invalid filename conflict folder item is still present
            invalidFilenameConflictFolderItem = completeSpy.findItem(diverseConflictsFolderPath + "/" + testInvalidCharFolder);
            completeSpy.clear();
            QVERIFY(invalidFilenameConflictFolderItem);
            QVERIFY(invalidFilenameConflictFolderItem->_status == SyncFileItem::FileNameInvalid);
        }
    }

    void testExistingFolderBecameBig()
    {
        constexpr auto testFolder = "folder";
        constexpr auto testSmallFile = "folder/small_file.txt";
        constexpr auto testLargeFile = "folder/large_file.txt";

        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file
        auto config = ConfigFile();
        config.setNotifyExistingFoldersOverLimit(true);

        FakeFolder fakeFolder{FileInfo{}};
        QSignalSpy spy(&fakeFolder.syncEngine(), &SyncEngine::existingFolderNowBig);

        auto syncOptions = fakeFolder.syncEngine().syncOptions();
        syncOptions._newBigFolderSizeLimit = 128; // 128 bytes
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        fakeFolder.remoteModifier().mkdir(testFolder);
        fakeFolder.remoteModifier().insert(testSmallFile, 64);
        fakeFolder.syncEngine().setLocalDiscoveryOptions(OCC::LocalDiscoveryStyle::DatabaseAndFilesystem);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(spy.count(), 0);

        fakeFolder.remoteModifier().insert(testLargeFile, 256);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(spy.count(), 1);
    }

    void testFileDownloadWithUnicodeCharacterInName() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.remoteModifier().insert("A/abcdęfg.txt");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/abcdęfg.txt"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteTypeChangeExistingLocalMustGetRemoved()
    {
        FakeFolder fakeFolder{FileInfo{}};

        // test file change to directory on remote
        fakeFolder.remoteModifier().mkdir("a");
        fakeFolder.remoteModifier().insert("a/TESTFILE");
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("a/TESTFILE");
        fakeFolder.remoteModifier().mkdir("a/TESTFILE");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // test directory change to file on remote
        fakeFolder.remoteModifier().mkdir("a/TESTDIR");
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("a/TESTDIR");
        fakeFolder.remoteModifier().insert("a/TESTDIR");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoveAllFilesWithNextcloudCmd()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto nextcloudCmdSyncOptions = fakeFolder.syncEngine().syncOptions();
        nextcloudCmdSyncOptions.setIsCmd(true);
        fakeFolder.syncEngine().setSyncOptions(nextcloudCmdSyncOptions);
        ConfigFile().setPromptDeleteFiles(true);
        QSignalSpy displayDialogSignal(&fakeFolder.syncEngine(), &SyncEngine::aboutToRemoveAllFiles);

        fakeFolder.remoteModifier().mkdir("folder");
        fakeFolder.remoteModifier().insert("folder/file1");
        fakeFolder.remoteModifier().insert("folder/file2");
        fakeFolder.remoteModifier().insert("folder/file3");
        fakeFolder.remoteModifier().mkdir("folder2");
        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("file2");
        fakeFolder.remoteModifier().insert("file3");

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("folder");
        fakeFolder.remoteModifier().remove("folder2");
        fakeFolder.remoteModifier().remove("file1");
        fakeFolder.remoteModifier().remove("file2");
        fakeFolder.remoteModifier().remove("file3");

        QVERIFY(fakeFolder.syncOnce());
        // the signal to display the dialog should not be emitted
        QCOMPARE(displayDialogSignal.count(), 0);
        QCOMPARE(fakeFolder.remoteModifier().find("folder"), nullptr);
        QCOMPARE(fakeFolder.remoteModifier().find("folder2"), nullptr);
        QCOMPARE(fakeFolder.remoteModifier().find("file1"), nullptr);
    }

    void testRemoveAllFilesWithoutNextcloudCmd()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto nextcloudCmdSyncOptions = fakeFolder.syncEngine().syncOptions();
        nextcloudCmdSyncOptions.setIsCmd(false);
        fakeFolder.syncEngine().setSyncOptions(nextcloudCmdSyncOptions);
        ConfigFile().setPromptDeleteFiles(true);
        QSignalSpy displayDialogSignal(&fakeFolder.syncEngine(), &SyncEngine::aboutToRemoveAllFiles);

        fakeFolder.remoteModifier().mkdir("folder");
        fakeFolder.remoteModifier().insert("folder/file1");
        fakeFolder.remoteModifier().insert("folder/file2");
        fakeFolder.remoteModifier().insert("folder/file3");
        fakeFolder.remoteModifier().mkdir("folder2");
        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("file2");
        fakeFolder.remoteModifier().insert("file3");

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().remove("folder");
        fakeFolder.remoteModifier().remove("folder2");
        fakeFolder.remoteModifier().remove("file1");
        fakeFolder.remoteModifier().remove("file2");
        fakeFolder.remoteModifier().remove("file3");

        QVERIFY(fakeFolder.syncOnce());
        // the signal to show the dialog should be emitted
        QCOMPARE(displayDialogSignal.count(), 1);
        QCOMPARE(fakeFolder.remoteModifier().find("folder"), nullptr);
        QCOMPARE(fakeFolder.remoteModifier().find("folder2"), nullptr);
        QCOMPARE(fakeFolder.remoteModifier().find("file1"), nullptr);
    }

    void testSyncReadOnlyLnkWindowsShortcuts()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto nextcloudCmdSyncOptions = fakeFolder.syncEngine().syncOptions();

        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd");
        fakeFolder.remoteModifier().insert("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.lnk");

        fakeFolder.remoteModifier().mkdir("folder");
        fakeFolder.remoteModifier().insert("folder/file1.lnk");
        fakeFolder.remoteModifier().insert("folder/file2.lnk");
        fakeFolder.remoteModifier().insert("folder/file3.lnk");
        fakeFolder.remoteModifier().mkdir("folder2");
        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("file2");
        fakeFolder.remoteModifier().insert("file3");

        fakeFolder.remoteModifier().find("folder")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("folder/file1.lnk")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("folder/file2.lnk")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("folder/file3.lnk")->permissions = RemotePermissions::fromServerString("SG");

        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.lnk")->permissions = RemotePermissions::fromServerString("SG");

        QVERIFY(fakeFolder.syncOnce());
    }

    void testSyncLongPaths()
    {
        FakeFolder fakeFolder{FileInfo{}};
        auto nextcloudCmdSyncOptions = fakeFolder.syncEngine().syncOptions();

        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc");
        fakeFolder.remoteModifier().mkdir("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd");
        fakeFolder.remoteModifier().insert("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.md");

        fakeFolder.remoteModifier().mkdir("folder");
        fakeFolder.remoteModifier().insert("folder/file1.lnk");
        fakeFolder.remoteModifier().insert("folder/file2.lnk");
        fakeFolder.remoteModifier().insert("folder/file3.lnk");
        fakeFolder.remoteModifier().mkdir("folder2");
        fakeFolder.remoteModifier().insert("file1");
        fakeFolder.remoteModifier().insert("file2");
        fakeFolder.remoteModifier().insert("file3");

        fakeFolder.remoteModifier().find("folder")->permissions = RemotePermissions::fromServerString("DNVSG");
        fakeFolder.remoteModifier().find("folder/file1.lnk")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("folder/file2.lnk")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("folder/file3.lnk")->permissions = RemotePermissions::fromServerString("SG");

        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd")->permissions = RemotePermissions::fromServerString("DNVSG");

        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd")->permissions = RemotePermissions::fromServerString("SG");
        fakeFolder.remoteModifier().find("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.md")->permissions = RemotePermissions::fromServerString("GS");

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.md");

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().insert("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.md");

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().rename("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.md",
                                           "abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long hello.docx - Sh.md");

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().rename("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long hello.docx - Sh.md",
                                           "abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/this is a long long long long long long long long long long hello.docx - Sh.md");

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().appendByte("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/this is a long long long long long long long long long long hello.docx - Sh.md");

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("abcdefabcdefabcdefabcdefabcdefabcd/abcdef abcdef abcdef a/abcdef abcdef/abcdef acbdef abcd/123abcdefabcdef1/123123abcdef123 abcdef1/12abcabc/12abcabd/this is a long long long long long long long long long long long long long long long long l.docx - Sh.md");

        QVERIFY(fakeFolder.syncOnce());
    }

    void testCreateFileWithTrailingLeadingSpaces_local_automatedRenameBeforeUpload()
    {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.enableEnforceWindowsFileNameCompatibility();

        fakeFolder.syncEngine().setLocalDiscoveryEnforceWindowsFileNameCompatibility(true);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        const QString fileWithSpaces1(" foo");
        const QString fileWithSpaces2(" bar ");
        const QString fileWithSpaces3("bla ");
        const QString fileWithSpaces4("A/ foo");
        const QString fileWithSpaces5("A/ bar ");
        const QString fileWithSpaces6("A/bla ");
        const auto extraFileNameWithSpaces = QStringLiteral(" with spaces ");
        const QString fileWithoutSpaces1(" foo");
        const QString fileWithoutSpaces2(" bar");
        const QString fileWithoutSpaces3("bla");
        const QString fileWithoutSpaces4("A/ foo");
        const QString fileWithoutSpaces5("A/ bar");
        const QString fileWithoutSpaces6("A/bla");
        const auto extraFileNameWithoutSpaces = QStringLiteral(" with spaces");

        fakeFolder.localModifier().insert(fileWithSpaces1);
        fakeFolder.localModifier().insert(fileWithSpaces2);
        fakeFolder.localModifier().insert(fileWithSpaces3);
        fakeFolder.localModifier().mkdir("A");
        fakeFolder.localModifier().insert(fileWithSpaces4);
        fakeFolder.localModifier().insert(fileWithSpaces5);
        fakeFolder.localModifier().insert(fileWithSpaces6);
        fakeFolder.localModifier().mkdir(extraFileNameWithSpaces);

        ItemCompletedSpy completeSpy(fakeFolder);
        completeSpy.clear();

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(fileWithSpaces1)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces2)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces3)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(extraFileNameWithSpaces)->_status, SyncFileItem::Status::FileNameInvalid);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces1)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces2)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces3)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces4)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces5)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces6)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(extraFileNameWithoutSpaces)->_status, SyncFileItem::Status::NoStatus);

        completeSpy.clear();

        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {QStringLiteral("foo"), QStringLiteral("bar"), QStringLiteral("bla"), QStringLiteral("A/foo"), QStringLiteral("A/bar"), QStringLiteral("A/bla")});
        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(completeSpy.findItem(fileWithSpaces1)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithSpaces2)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithSpaces3)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithSpaces4)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithSpaces5)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithSpaces6)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(extraFileNameWithSpaces)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces1)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces2)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces3)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces4)->_status, SyncFileItem::Status::NoStatus);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces5)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(fileWithoutSpaces6)->_status, SyncFileItem::Status::Success);
        QCOMPARE(completeSpy.findItem(extraFileNameWithoutSpaces)->_status, SyncFileItem::Status::Success);
    }

    void testTouchedFilesWhenChangingFolderPermissionsDuringSync()
    {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.localModifier().mkdir("directory");
        fakeFolder.localModifier().mkdir("directory/subdir");
        fakeFolder.remoteModifier().mkdir("directory");
        fakeFolder.remoteModifier().mkdir("directory/subdir");

        // perform an initial sync to ensure local and remote have the same state
        QVERIFY(fakeFolder.syncOnce());

        QStringList touchedFiles;

        // syncEngine->_propagator is only set during a sync, which doesn't work with QSignalSpy :(
        connect(&fakeFolder.syncEngine(), &SyncEngine::started, this, [&]() {
            // at this point we have a propagator to connect signals to
            connect(fakeFolder.syncEngine().getPropagator().get(), &OwncloudPropagator::touchedFile, this, [&touchedFiles](const QString& fileName) {
                touchedFiles.append(fileName);
            });
        });

        const auto syncAndExpectNoTouchedFiles = [&]() {
            touchedFiles.clear();
            QVERIFY(fakeFolder.syncOnce());
            QCOMPARE(touchedFiles.size(), 0);
        };

        // when nothing changed expect no files to be touched
        syncAndExpectNoTouchedFiles();

        // when the remote etag of a subsubdir changes expect the parent+subdirs to be touched
        fakeFolder.remoteModifier().findInvalidatingEtags("directory/subdir");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(touchedFiles.size(), 2);
        QVERIFY(touchedFiles.contains(fakeFolder.localModifier().find("directory/subdir").fileName()));
        QVERIFY(touchedFiles.contains(fakeFolder.localModifier().find("directory").fileName()));

        // nothing changed again, expect no files to be touched
        syncAndExpectNoTouchedFiles();

        // when subdir folder permissions change, expect the parent to be touched
        touchedFiles.clear();
        fakeFolder.remoteModifier().find("directory")->permissions = RemotePermissions::fromServerString("SG");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(touchedFiles.size(), 1);
        QVERIFY(touchedFiles.contains(fakeFolder.localModifier().find("directory").fileName()));

        // another sync without changes, expect no files to be touched
        syncAndExpectNoTouchedFiles();

        // remote etag of the subdir changed, expect the parent to be touched
        touchedFiles.clear();
        fakeFolder.remoteModifier().findInvalidatingEtags("directory");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(touchedFiles.size(), 1);
        QVERIFY(touchedFiles.contains(fakeFolder.localModifier().find("directory").fileName()));

        // same as usual, expect no files to be touched
        syncAndExpectNoTouchedFiles();

        // remote rename of the subdir folder, expect the new name to be touched
        touchedFiles.clear();
        fakeFolder.remoteModifier().rename("directory", "renamedDirectory");
        QVERIFY(fakeFolder.syncOnce());
        qDebug() << touchedFiles;
        QCOMPARE_GT(touchedFiles.size(), 1);
        QVERIFY(touchedFiles.contains(fakeFolder.localModifier().find("renamedDirectory").fileName()));

        // last sync without changes, expect no files to be touched
        syncAndExpectNoTouchedFiles();
    }

    void testSyncFolderNewDeleteConflictExpectDeletion()
    {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.remoteModifier().mkdir("directory");
        fakeFolder.remoteModifier().mkdir("directory/subdir");
        fakeFolder.remoteModifier().insert("directory/file1");
        fakeFolder.remoteModifier().insert("directory/file2");
        fakeFolder.remoteModifier().insert("directory/file3");
        fakeFolder.remoteModifier().insert("directory/subdir/fileTxt1.txt");
        fakeFolder.remoteModifier().insert("directory/subdir/fileTxt2.txt");
        fakeFolder.remoteModifier().insert("directory/subdir/fileTxt3.txt");

        // perform an initial sync to ensure local and remote have the same state
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("directory");
        fakeFolder.localModifier().mkdir("directory/subFolder");
        fakeFolder.localModifier().insert("directory/file4");
        fakeFolder.localModifier().insert("directory/subdir/fileTxt4.txt");

        QVERIFY(fakeFolder.syncOnce());
        const auto directoryItem = fakeFolder.remoteModifier().find("directory");
        QCOMPARE(directoryItem, nullptr);
    }
    // Test that when the server sets request-upload property, the client re-uploads the file
    void testRequestUploadProperty() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        
        // Create a file on both local and remote
        fakeFolder.localModifier().insert("A/file.txt", 100, 'A');
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/file.txt"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        
        completeSpy.clear();
        
        // Now simulate the server requesting a re-upload by setting the request-upload property
        auto remoteFile = fakeFolder.remoteModifier().find("A/file.txt");
        QVERIFY(remoteFile);
        remoteFile->extraDavProperties = "<nc:request-upload xmlns:nc=\"http://nextcloud.org/ns\">1</nc:request-upload>";
        
        // Sync again - the file should be uploaded even though nothing changed locally
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/file.txt"));
        
        // Verify the file was uploaded (check that it appeared in completed items)
        auto item = completeSpy.findItem("A/file.txt");
        QVERIFY(item);
        QCOMPARE(item->_direction, SyncFileItem::Up);
        QCOMPARE(item->_instruction, CSYNC_INSTRUCTION_SYNC);
    }
    
    // Test that request-upload is ignored for directories
    void testRequestUploadPropertyIgnoredForDirectories() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        ItemCompletedSpy completeSpy(fakeFolder);
        
        // Create a directory
        fakeFolder.localModifier().mkdir("TestDir");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "TestDir"));
        
        completeSpy.clear();
        
        // Set request-upload on the directory
        auto remoteDir = fakeFolder.remoteModifier().find("TestDir");
        QVERIFY(remoteDir);
        remoteDir->extraDavProperties = "<nc:request-upload xmlns:nc=\"http://nextcloud.org/ns\">1</nc:request-upload>";
        
        // Sync again - nothing should happen for the directory
        fakeFolder.syncOnce();
        
        // The directory should not be in the completed items (or should only have metadata update)
        auto item = completeSpy.findItem("TestDir");
        if (item) {
            QVERIFY(item->_direction != SyncFileItem::Up || item->_instruction != CSYNC_INSTRUCTION_SYNC);
        }
    }
    
    // Test that request-upload works for virtual files
    void testRequestUploadPropertyWithVirtualFiles() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{ {"chunking", "1.0"} } } });
        
        auto vfs = QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::WithSuffix).release());
        QVERIFY(vfs);
        fakeFolder.switchToVfs(vfs);
        
        ItemCompletedSpy completeSpy(fakeFolder);
        
        // Create a file on remote that becomes a virtual file locally
        fakeFolder.remoteModifier().insert("A/virtual.txt", 100, 'V');
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/virtual.txt"));
        
        // Verify it's a virtual file
        QVERIFY(fakeFolder.currentLocalState().find("A/virtual.txt" DVSUFFIX));
        
        completeSpy.clear();
        
        // Set request-upload property
        auto remoteFile = fakeFolder.remoteModifier().find("A/virtual.txt");
        QVERIFY(remoteFile);
        remoteFile->extraDavProperties = "<nc:request-upload xmlns:nc=\"http://nextcloud.org/ns\">1</nc:request-upload>";
        
        // Sync - should not upload since we don't have the actual file data locally
        fakeFolder.syncOnce();
        
        // Virtual file should not be uploaded
        auto item = completeSpy.findItem("A/virtual.txt");
        if (item) {
            // If the item is processed, it should not be an upload
            QVERIFY(item->_direction != SyncFileItem::Up || item->_instruction != CSYNC_INSTRUCTION_SYNC);
        }
    }
};

QTEST_GUILESS_MAIN(TestSyncEngine)
#include "testsyncengine.moc"
