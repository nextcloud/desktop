/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "testutils/syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;

SyncJournalFileRecord journalRecord(FakeFolder &folder, const QByteArray &path)
{
    SyncJournalFileRecord rec;
    folder.syncJournal().getFileRecord(path, &rec);
    return rec;
}

class TestBlacklist : public QObject
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

    void testBlacklistBasic_data()
    {
        QTest::addColumn<bool>("remote");
        QTest::newRow("remote") << true;
        QTest::newRow("local") << false;
    }

    void testBlacklistBasic()
    {
        QFETCH_GLOBAL(Vfs::Mode, vfsMode);
        QFETCH_GLOBAL(bool, filesAreDehydrated);
        QFETCH(bool, remote);

        FakeFolder fakeFolder(FileInfo::A12_B12_C12_S12(), vfsMode, filesAreDehydrated);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(fakeFolder);

        auto &modifier = remote ? fakeFolder.remoteModifier() : fakeFolder.localModifier();

        int counter = 0;
        const QString testFileName = QStringLiteral("A/new");
        QByteArray reqId;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *) -> QNetworkReply * {
            if (req.url().path().endsWith(testFileName)) {
                reqId = req.rawHeader("X-Request-ID");
            }
            if (!remote && op == QNetworkAccessManager::PutOperation)
                ++counter;
            if (remote && op == QNetworkAccessManager::GetOperation)
                ++counter;
            return nullptr;
        });

        auto cleanup = [&]() {
            completeSpy.clear();
        };

        auto initialEtag = journalRecord(fakeFolder, "A")._etag;
        QVERIFY(!initialEtag.isEmpty());

        // The first sync and the download will fail - the item will be blacklisted
        modifier.insert(testFileName);
        fakeFolder.serverErrorPaths().append(testFileName, 500); // will be blacklisted
        const bool syncResult = fakeFolder.applyLocalModificationsAndSync();
        if (vfsMode == Vfs::WindowsCfApi && filesAreDehydrated && remote) {
            // With dehydrated files, only a PROPFIND is done, but not a GET request.
            // And it is the GET request that fails, and causes a blacklist entry, all "syncs" will succeed.
            QVERIFY(syncResult);
            QSKIP("The remainder of this test is not applicable for dehydrated files");
        }

        QVERIFY(!syncResult);
        {
            auto it = completeSpy.findItem(testFileName);
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::NormalError); // initial error visible
            QCOMPARE(it->instruction(), CSYNC_INSTRUCTION_NEW);

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(testFileName);
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Category::Normal);
            QCOMPARE(entry._retryCount, 1);
            QCOMPARE(counter, 1);
            QVERIFY(entry._ignoreDuration > 0);
            QCOMPARE(entry._requestId, reqId);

            if (remote) {
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
            }
        }
        cleanup();

        // Ignored during the second run - but soft errors are also errors
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        {
            auto it = completeSpy.findItem(testFileName);
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::BlacklistedError);
            QCOMPARE(it->instruction(), CSYNC_INSTRUCTION_IGNORE); // no retry happened!

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(testFileName);
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Category::Normal);
            QCOMPARE(entry._retryCount, 1);
            QCOMPARE(counter, 1);
            QVERIFY(entry._ignoreDuration > 0);
            QCOMPARE(entry._requestId, reqId);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
        }
        cleanup();

        // Let's expire the blacklist entry to verify it gets retried
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(testFileName);
            entry._ignoreDuration = 1;
            entry._lastTryTime -= 1;
            fakeFolder.syncJournal().setErrorBlacklistEntry(entry);
        }
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        {
            auto it = completeSpy.findItem(testFileName);
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::BlacklistedError); // blacklisted as it's just a retry
            QCOMPARE(it->instruction(), CSYNC_INSTRUCTION_NEW); // retry!

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(testFileName);
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Category::Normal);
            QCOMPARE(entry._retryCount, 2);
            QCOMPARE(counter, 2);
            QVERIFY(entry._ignoreDuration > 0);
            QCOMPARE(entry._requestId, reqId);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
        }
        cleanup();

        // Try to make sure that the modifications below do are not within the same second as the
        // previous sync attempt was done, otherwise the changes go undetected.
        QThread::sleep(1);

        // When the file changes a retry happens immediately
        modifier.appendByte(testFileName);
        QVERIFY(!fakeFolder.applyLocalModificationsAndSync());
        {
            auto it = completeSpy.findItem(testFileName);
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::BlacklistedError);
            QCOMPARE(it->instruction(), CSYNC_INSTRUCTION_NEW); // retry!

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(testFileName);
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Category::Normal);
            QCOMPARE(entry._retryCount, 3);
            QCOMPARE(counter, 3);
            QVERIFY(entry._ignoreDuration > 0);
            QCOMPARE(entry._requestId, reqId);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
        }
        cleanup();

        // When the error goes away and the item is retried, the sync succeeds
        fakeFolder.serverErrorPaths().clear();
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(testFileName);
            entry._ignoreDuration = 1;
            entry._lastTryTime -= 1;
            fakeFolder.syncJournal().setErrorBlacklistEntry(entry);
        }
        QVERIFY(fakeFolder.applyLocalModificationsAndSync());
        {
            auto it = completeSpy.findItem(testFileName);
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::Success);
            QCOMPARE(it->instruction(), CSYNC_INSTRUCTION_NEW);

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry(testFileName);
            QVERIFY(!entry.isValid());
            QCOMPARE(counter, 4);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, fakeFolder.currentRemoteState().find(QStringLiteral("A"))->etag);
        }
        cleanup();

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestBlacklist)
#include "testblacklist.moc"
