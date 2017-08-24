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

bool itemDidComplete(const QSignalSpy &spy, const QString &path)
{
    for(const QList<QVariant> &args : spy) {
        auto item = args[0].value<SyncFileItemPtr>();
        if (item->destination() == path)
            return true;
    }
    return false;
}

bool itemDidCompleteSuccessfully(const QSignalSpy &spy, const QString &path)
{
    for(const QList<QVariant> &args : spy) {
        auto item = args[0].value<SyncFileItemPtr>();
        if (item->destination() == path)
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
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        fakeFolder.remoteModifier().insert("A/a0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testFileUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        fakeFolder.localModifier().insert("A/a0");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a0"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testDirDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
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
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
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
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        fakeFolder.remoteModifier().remove("A/a1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteDelete() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        fakeFolder.localModifier().remove("A/a1");
        fakeFolder.syncOnce();
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "A/a1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testEmlLocalChecksum() {
        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.localModifier().insert("a1.eml", 64, 'A');
        fakeFolder.localModifier().insert("a2.eml", 64, 'A');
        fakeFolder.localModifier().insert("a3.eml", 64, 'A');
        // Upload and calculate the checksums
        // fakeFolder.syncOnce();
        fakeFolder.syncOnce();

        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
        // Touch the file without changing the content, shouldn't upload
        fakeFolder.localModifier().setContents("a1.eml", 'A');
        // Change the content/size
        fakeFolder.localModifier().setContents("a2.eml", 'B');
        fakeFolder.localModifier().appendByte("a3.eml");
        fakeFolder.syncOnce();

        QVERIFY(!itemDidComplete(completeSpy, "a1.eml"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "a2.eml"));
        QVERIFY(itemDidCompleteSuccessfully(completeSpy, "a3.eml"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRemoteChangeInMovedFolder() {
        // issue #5192
        FakeFolder fakeFolder{FileInfo{ QString(), {
            FileInfo { QStringLiteral("folder"), {
                FileInfo{ QStringLiteral("folderA"), { { QStringLiteral("file.txt"), 400 } } },
                QStringLiteral("folderB")
            }
        }}}};

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

    void testSelectiveSyncModevFolder() {
        // issue #5224
        FakeFolder fakeFolder{FileInfo{ QString(), {
            FileInfo { QStringLiteral("parentFolder"), {
                FileInfo{ QStringLiteral("subFolderA"), { { QStringLiteral("fileA.txt"), 400 } } },
                FileInfo{ QStringLiteral("subFolderB"), { { QStringLiteral("fileB.txt"), 400 } } }
            }
        }}}};

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto expectedServerState = fakeFolder.currentRemoteState();

        // Remove subFolderA with selectiveSync:
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                                                                {"parentFolder/subFolderA/"});
        fakeFolder.syncEngine().journal()->avoidReadFromDbOnNextSync("parentFolder/subFolderA/");

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

    void testSelectiveSyncBug() {
        // issue owncloud/enterprise#1965: files from selective-sync ignored
        // folders are uploaded anyway is some circumstances.
        FakeFolder fakeFolder{FileInfo{ QString(), {
            FileInfo { QStringLiteral("parentFolder"), {
                FileInfo{ QStringLiteral("subFolder"), {
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
                }}
            }}
        }}};

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        auto expectedServerState = fakeFolder.currentRemoteState();

        // Remove subFolder with selectiveSync:
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                                                                {"parentFolder/subFolder/"});
        fakeFolder.syncEngine().journal()->avoidReadFromDbOnNextSync("parentFolder/subFolder/");

        // But touch local file before the next sync, such that the local folder
        // can't be removed
        fakeFolder.localModifier().setContents("parentFolder/subFolder/fileB.txt", 'n');
        fakeFolder.localModifier().setContents("parentFolder/subFolder/subsubFolder/fileD.txt", 'n');
        fakeFolder.localModifier().setContents("parentFolder/subFolder/anotherFolder/subsubFolder/fileF.txt", 'n');

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
                QVERIFY(local.find("parentFolder/subFolder"));
                QVERIFY(!local.find("parentFolder/subFolder/fileA.txt"));
                QVERIFY(local.find("parentFolder/subFolder/fileB.txt"));
                QVERIFY(!local.find("parentFolder/subFolder/subsubFolder/fileC.txt"));
                QVERIFY(local.find("parentFolder/subFolder/subsubFolder/fileD.txt"));
                QVERIFY(!local.find("parentFolder/subFolder/anotherFolder/subsubFolder/fileE.txt"));
                QVERIFY(local.find("parentFolder/subFolder/anotherFolder/subsubFolder/fileF.txt"));
                QVERIFY(!local.find("parentFolder/subFolder/anotherFolder/emptyFolder"));
            }
        }
    }

    void abortAfterFailedMkdir() {
        FakeFolder fakeFolder{FileInfo{}};
        QSignalSpy finishedSpy(&fakeFolder.syncEngine(), SIGNAL(finished(bool)));
        fakeFolder.serverErrorPaths().append("NewFolder");
        fakeFolder.localModifier().mkdir("NewFolder");
        // This should be aborted and would otherwise fail in FileInfo::create.
        fakeFolder.localModifier().insert("NewFolder/NewFile");
        fakeFolder.syncOnce();
        QCOMPARE(finishedSpy.size(), 1);
        QCOMPARE(finishedSpy.first().first().toBool(), false);
    }

    void testDirDownloadWithError() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));
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
        fakeFolder.serverErrorPaths().append("Y/Z/d2", 503); // 503 is a fatal error
        fakeFolder.serverErrorPaths().append("Y/Z/d3", 503); // 503 is a fatal error
        QVERIFY(!fakeFolder.syncOnce());
        QCoreApplication::processEvents(); // should not crash

        QSet<QString> seen;
        for(const QList<QVariant> &args : completeSpy) {
            auto item = args[0].value<SyncFileItemPtr>();
            qDebug() << item->_file << item->_isDirectory << item->_status;
            QVERIFY(!seen.contains(item->_file)); // signal only sent once per item
            seen.insert(item->_file);
            if (item->_file == "Y/Z/d2") {
                QVERIFY(item->_status == SyncFileItem::FatalError);
            } else if(item->_file == "Y/Z/d3") {
                QVERIFY(item->_status != SyncFileItem::Success);
            }
            QVERIFY(item->_file != "Y/Z/d9"); // we should have aborted the sync before d9 starts
        }
    }

    void testFakeConflict()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        int nGET = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &) {
            if (op == QNetworkAccessManager::GetOperation)
                ++nGET;
            return nullptr;
        });

        // For directly editing the remote checksum
        FileInfo &remoteInfo = dynamic_cast<FileInfo &>(fakeFolder.remoteModifier());

        // Base mtime with no ms content (filesystem is seconds only)
        auto mtime = QDateTime::currentDateTime().addDays(-4);
        mtime.setMSecsSinceEpoch(mtime.toMSecsSinceEpoch() / 1000 * 1000);

        // Conflict: Same content, mtime, but no server checksum
        //           -> ignored in reconcile
        fakeFolder.localModifier().setContents("A/a1", 'C');
        fakeFolder.localModifier().setModTime("A/a1", mtime);
        fakeFolder.remoteModifier().setContents("A/a1", 'C');
        fakeFolder.remoteModifier().setModTime("A/a1", mtime);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);

        // Conflict: Same content, mtime, but weak server checksum
        //           -> ignored in reconcile
        mtime = mtime.addDays(1);
        fakeFolder.localModifier().setContents("A/a1", 'D');
        fakeFolder.localModifier().setModTime("A/a1", mtime);
        fakeFolder.remoteModifier().setContents("A/a1", 'D');
        fakeFolder.remoteModifier().setModTime("A/a1", mtime);
        remoteInfo.find("A/a1")->checksums = "Adler32:bad";
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 0);

        // Conflict: Same content, mtime, but server checksum differs
        //           -> downloaded
        mtime = mtime.addDays(1);
        fakeFolder.localModifier().setContents("A/a1", 'W');
        fakeFolder.localModifier().setModTime("A/a1", mtime);
        fakeFolder.remoteModifier().setContents("A/a1", 'W');
        fakeFolder.remoteModifier().setModTime("A/a1", mtime);
        remoteInfo.find("A/a1")->checksums = "SHA1:bad";
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 1);

        // Conflict: Same content, mtime, matching checksums
        //           -> PropagateDownload, but it skips the download
        mtime = mtime.addDays(1);
        fakeFolder.localModifier().setContents("A/a1", 'C');
        fakeFolder.localModifier().setModTime("A/a1", mtime);
        fakeFolder.remoteModifier().setContents("A/a1", 'C');
        fakeFolder.remoteModifier().setModTime("A/a1", mtime);
        remoteInfo.find("A/a1")->checksums = "SHA1:56900fb1d337cf7237ff766276b9c1e8ce507427";
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 1);

        // Extra sync reads from db, no difference
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(nGET, 1);
    }

    /**
     * Checks whether SyncFileItems have the expected properties before start
     * of propagation.
     */
    void testSyncFileItemProperties()
    {
        auto initialMtime = QDateTime::currentDateTime().addDays(-7);
        auto changedMtime = QDateTime::currentDateTime().addDays(-4);
        auto changedMtime2 = QDateTime::currentDateTime().addDays(-3);

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
            QCOMPARE(a1->_size, quint64(5));

            QCOMPARE(Utility::qDateTimeFromTime_t(a1->_modtime), changedMtime);
            QCOMPARE(a1->_previousSize, quint64(4));
            QCOMPARE(Utility::qDateTimeFromTime_t(a1->_previousModtime), initialMtime);

            // b2: should have remote size and modtime
            QVERIFY(b1);
            QCOMPARE(b1->_instruction, CSYNC_INSTRUCTION_SYNC);
            QCOMPARE(b1->_direction, SyncFileItem::Down);
            QCOMPARE(b1->_size, quint64(17));
            QCOMPARE(Utility::qDateTimeFromTime_t(b1->_modtime), changedMtime);
            QCOMPARE(b1->_previousSize, quint64(16));
            QCOMPARE(Utility::qDateTimeFromTime_t(b1->_previousModtime), initialMtime);

            // c1: conflicts are downloads, so remote size and modtime
            QVERIFY(c1);
            QCOMPARE(c1->_instruction, CSYNC_INSTRUCTION_CONFLICT);
            QCOMPARE(c1->_direction, SyncFileItem::None);
            QCOMPARE(c1->_size, quint64(25));
            QCOMPARE(Utility::qDateTimeFromTime_t(c1->_modtime), changedMtime2);
            QCOMPARE(c1->_previousSize, quint64(26));
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
        syncOptions._parallelNetworkJobs = false;
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        // Produce an error based on upload size
        int remoteQuota = 1000;
        int n507 = 0, nPUT = 0;
        auto parent = new QObject;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &request) -> QNetworkReply * {
            if (op == QNetworkAccessManager::PutOperation) {
                nPUT++;
                if (request.rawHeader("OC-Total-Length").toInt() > remoteQuota) {
                    n507++;
                    return new FakeErrorReply(op, request, parent, 507);
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
};

QTEST_GUILESS_MAIN(TestSyncEngine)
#include "testsyncengine.moc"
