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


static void changeAllFileId(FileInfo &info) {
    info.fileId = generateFileId();
    if (!info.isDir)
        return;
    info.etag = generateEtag();
    for (auto it = info.children.begin(); it != info.children.end(); ++it) {
        changeAllFileId(*it);
    }
}

/*
 * This test ensure that the SyncEngine::aboutToRemoveAllFiles is correctly called and that when
 * we the user choose to remove all files SyncJournalDb::clearFileTable makes works as expected
 */
class TestAllFilesDeleted : public QObject
{
    Q_OBJECT

private slots:

    void testAllFilesDeletedKeep_data()
    {
        QTest::addColumn<bool>("deleteOnRemote");
        QTest::newRow("local") << false;
        QTest::newRow("remote") << true;

    }

    /*
     * In this test, all files are deleted in the client, or the server, and we simulate
     * that the users press "keep"
     */
    void testAllFilesDeletedKeep()
    {
        QFETCH(bool, deleteOnRemote);
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        //Just set a blacklist so we can check it is still there. This directory does not exists but
        // that does not matter for our purposes.
        QStringList selectiveSyncBlackList = { "Q/" };
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                                                                selectiveSyncBlackList);

        auto initialState = fakeFolder.currentLocalState();
        int aboutToRemoveAllFilesCalled = 0;
        QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToRemoveAllFiles,
            [&](SyncFileItem::Direction dir, bool *cancel) {
                QCOMPARE(aboutToRemoveAllFilesCalled, 0);
                aboutToRemoveAllFilesCalled++;
                QCOMPARE(dir, deleteOnRemote ? SyncFileItem::Down : SyncFileItem::Up);
                *cancel = true;
                fakeFolder.syncEngine().journal()->clearFileTable(); // That's what Folder is doing
            });

        auto &modifier = deleteOnRemote ? fakeFolder.remoteModifier() : fakeFolder.localModifier();
        for (const auto &s : fakeFolder.currentRemoteState().children.keys())
            modifier.remove(s);

        QVERIFY(!fakeFolder.syncOnce()); // Should fail because we cancel the sync
        QCOMPARE(aboutToRemoveAllFilesCalled, 1);

        // Next sync should recover all files
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), initialState);
        QCOMPARE(fakeFolder.currentRemoteState(), initialState);

        // The selective sync blacklist should be not have been deleted.
        bool ok = true;
        QCOMPARE(fakeFolder.syncEngine().journal()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok),
                 selectiveSyncBlackList);
    }

    void testAllFilesDeletedDelete_data()
    {
        testAllFilesDeletedKeep_data();
    }

    /*
     * This test is like the previous one but we simulate that the user presses "delete"
     */
    void testAllFilesDeletedDelete()
    {
        QFETCH(bool, deleteOnRemote);
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        int aboutToRemoveAllFilesCalled = 0;
        QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToRemoveAllFiles,
            [&](SyncFileItem::Direction dir, bool *cancel) {
                QCOMPARE(aboutToRemoveAllFilesCalled, 0);
                aboutToRemoveAllFilesCalled++;
                QCOMPARE(dir, deleteOnRemote ? SyncFileItem::Down : SyncFileItem::Up);
                *cancel = false;
            });

        auto &modifier = deleteOnRemote ? fakeFolder.remoteModifier() : fakeFolder.localModifier();
        for (const auto &s : fakeFolder.currentRemoteState().children.keys())
            modifier.remove(s);

        QVERIFY(fakeFolder.syncOnce()); // Should succeed, and all files must then be deleted

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentLocalState().children.count(), 0);

        // Try another sync to be sure.

        QVERIFY(fakeFolder.syncOnce()); // Should succeed (doing nothing)
        QCOMPARE(aboutToRemoveAllFilesCalled, 1); // should not have been called.

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QCOMPARE(fakeFolder.currentLocalState().children.count(), 0);
    }

    void testNotDeleteMetaDataChange() {
        /**
         * This test make sure that we don't popup a file deleted message if all the metadata have
         * been updated (for example when the server is upgraded or something)
         **/

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        // We never remove all files.
        QObject::connect(&fakeFolder.syncEngine(), &SyncEngine::aboutToRemoveAllFiles,
            [&] { QVERIFY(false); });
        QVERIFY(fakeFolder.syncOnce());

        for (const auto &s : fakeFolder.currentRemoteState().children.keys())
            fakeFolder.syncJournal().avoidRenamesOnNextSync(s); // clears all the fileid and inodes.
        fakeFolder.localModifier().remove("A/a1");
        auto expectedState = fakeFolder.currentLocalState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);

        fakeFolder.remoteModifier().remove("B/b1");
        changeAllFileId(fakeFolder.remoteModifier());
        expectedState = fakeFolder.currentRemoteState();
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), expectedState);
        QCOMPARE(fakeFolder.currentRemoteState(), expectedState);
    }
};

QTEST_GUILESS_MAIN(TestAllFilesDeleted)
#include "testallfilesdeleted.moc"
