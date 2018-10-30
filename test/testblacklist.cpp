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
    void testBlacklistBasic_data()
    {
        QTest::addColumn<bool>("remote");
        QTest::newRow("remote") << true;
        QTest::newRow("local") << false;
    }

    void testBlacklistBasic()
    {
        QFETCH(bool, remote);

        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
        QSignalSpy completeSpy(&fakeFolder.syncEngine(), SIGNAL(itemCompleted(const SyncFileItemPtr &)));

        auto &modifier = remote ? fakeFolder.remoteModifier() : fakeFolder.localModifier();

        int counter = 0;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &, QIODevice *) -> QNetworkReply * {
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
        modifier.insert("A/new");
        fakeFolder.serverErrorPaths().append("A/new", 500); // will be blacklisted
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto it = findItem(completeSpy, "A/new");
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::NormalError); // initial error visible
            QCOMPARE(it->_instruction, CSYNC_INSTRUCTION_NEW);

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry("A/new");
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Normal);
            QCOMPARE(entry._retryCount, 1);
            QCOMPARE(counter, 1);
            QVERIFY(entry._ignoreDuration > 0);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
        }
        cleanup();

        // Ignored during the second run - but soft errors are also errors
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto it = findItem(completeSpy, "A/new");
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::BlacklistedError);
            QCOMPARE(it->_instruction, CSYNC_INSTRUCTION_IGNORE); // no retry happened!

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry("A/new");
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Normal);
            QCOMPARE(entry._retryCount, 1);
            QCOMPARE(counter, 1);
            QVERIFY(entry._ignoreDuration > 0);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
        }
        cleanup();

        // Let's expire the blacklist entry to verify it gets retried
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry("A/new");
            entry._ignoreDuration = 1;
            entry._lastTryTime -= 1;
            fakeFolder.syncJournal().setErrorBlacklistEntry(entry);
        }
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto it = findItem(completeSpy, "A/new");
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::BlacklistedError); // blacklisted as it's just a retry
            QCOMPARE(it->_instruction, CSYNC_INSTRUCTION_NEW); // retry!

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry("A/new");
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Normal);
            QCOMPARE(entry._retryCount, 2);
            QCOMPARE(counter, 2);
            QVERIFY(entry._ignoreDuration > 0);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
        }
        cleanup();

        // When the file changes a retry happens immediately
        modifier.appendByte("A/new");
        QVERIFY(!fakeFolder.syncOnce());
        {
            auto it = findItem(completeSpy, "A/new");
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::BlacklistedError);
            QCOMPARE(it->_instruction, CSYNC_INSTRUCTION_NEW); // retry!

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry("A/new");
            QVERIFY(entry.isValid());
            QCOMPARE(entry._errorCategory, SyncJournalErrorBlacklistRecord::Normal);
            QCOMPARE(entry._retryCount, 3);
            QCOMPARE(counter, 3);
            QVERIFY(entry._ignoreDuration > 0);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, initialEtag);
        }
        cleanup();

        // When the error goes away and the item is retried, the sync succeeds
        fakeFolder.serverErrorPaths().clear();
        {
            auto entry = fakeFolder.syncJournal().errorBlacklistEntry("A/new");
            entry._ignoreDuration = 1;
            entry._lastTryTime -= 1;
            fakeFolder.syncJournal().setErrorBlacklistEntry(entry);
        }
        QVERIFY(fakeFolder.syncOnce());
        {
            auto it = findItem(completeSpy, "A/new");
            QVERIFY(it);
            QCOMPARE(it->_status, SyncFileItem::Success);
            QCOMPARE(it->_instruction, CSYNC_INSTRUCTION_NEW);

            auto entry = fakeFolder.syncJournal().errorBlacklistEntry("A/new");
            QVERIFY(!entry.isValid());
            QCOMPARE(counter, 4);

            if (remote)
                QCOMPARE(journalRecord(fakeFolder, "A")._etag, fakeFolder.currentRemoteState().find("A")->etag.toUtf8());
        }
        cleanup();

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }
};

QTEST_GUILESS_MAIN(TestBlacklist)
#include "testblacklist.moc"
