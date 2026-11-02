/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <sqlite3.h>

#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "logger.h"

using namespace OCC;
using namespace Qt::StringLiterals;

class TestSyncJournalDB : public QObject
{
    Q_OBJECT

    QTemporaryDir _tempDir;

public:
    TestSyncJournalDB()
        : _db((_tempDir.path() + "/sync.db"))
    {
        QVERIFY(_tempDir.isValid());
    }

    qint64 dropMsecs(QDateTime time)
    {
        return Utility::qDateTimeToTime_t(time);
    }

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void cleanupTestCase()
    {
        const QString file = _db.databaseFilePath();
        QFile::remove(file);
    }

    void testFileRecord()
    {
        SyncJournalFileRecord record;
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("nonexistent"), &record));
        QVERIFY(!record.isValid());

        record._path = "foo";
        // Use a value that exceeds uint32 and isn't representable by the
        // signed int being cast to uint64 either (like uint64::max would be)
        record._inode = std::numeric_limits<quint32>::max() + 12ull;
        record._modtime = dropMsecs(QDateTime::currentDateTime());
        record._type = ItemTypeDirectory;
        record._etag = "789789";
        record._fileId = "abcd";
        record._remotePerm = RemotePermissions::fromDbValue("RW");
        record._fileSize = 213089055;
        record._checksumHeader = "MD5:mychecksum";
        QVERIFY(_db.setFileRecord(record));

        SyncJournalFileRecord storedRecord;
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo"), &storedRecord));
        QVERIFY(storedRecord == record);

        // Update checksum
        record._checksumHeader = "Adler32:newchecksum";
        QVERIFY(_db.updateFileRecordChecksum("foo", "newchecksum", "Adler32"));
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo"), &storedRecord));
        QVERIFY(storedRecord == record);

        // Update metadata
        record._modtime = dropMsecs(QDateTime::currentDateTime().addDays(1));
        // try a value that only fits uint64, not int64
        record._inode = std::numeric_limits<quint64>::max() - std::numeric_limits<quint32>::max() - 1;
        record._type = ItemTypeFile;
        record._etag = "789FFF";
        record._fileId = "efg";
        record._remotePerm = RemotePermissions::fromDbValue("NV");
        record._fileSize = 289055;
        QVERIFY(_db.setFileRecord(record));
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo"), &storedRecord));
        QVERIFY(storedRecord == record);

        QVERIFY(_db.deleteFileRecord("foo"));
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo"), &record));
        QVERIFY(!record.isValid());
    }

    void testFolderQuota()
    {
        const auto bigFolderRecord = QByteArray("bigfolder");
        SyncJournalFileRecord record;
        record._path = bigFolderRecord;
        record._inode = std::numeric_limits<quint32>::max() + 12ull;
        record._modtime = dropMsecs(QDateTime::currentDateTime());
        record._type = ItemTypeDirectory;
        record._etag = "123123";
        record._fileId = "abcd";
        record._fileSize = 213089999;
        QVERIFY(_db.setFileRecord(record));

        SyncJournalFileRecord storedRecord;
        QVERIFY(_db.getFileRecord(bigFolderRecord, &storedRecord));
        QVERIFY(storedRecord == record);
        // default values
        QCOMPARE(storedRecord._folderQuota.bytesAvailable, -1);
        QCOMPARE(storedRecord._folderQuota.bytesUsed, -1);

        record._folderQuota.bytesUsed = 100;
        record._folderQuota.bytesAvailable = 5000;
        QVERIFY(_db.setFileRecord(record));
        QVERIFY(_db.getFileRecord(bigFolderRecord, &storedRecord));
        QVERIFY(storedRecord == record);
        QCOMPARE(storedRecord._folderQuota.bytesAvailable, 5000);
        QCOMPARE(storedRecord._folderQuota.bytesUsed, 100);
    }

    void testFolderMigration() {
        const auto quotaBytesUsed =  QStringLiteral("quotaBytesUsed");
        const auto quotaBytesAvailable =  QStringLiteral("quotaBytesAvailable");
        const auto metadata = QStringLiteral("metadata");
        const auto columnsBeforeRemoval = _db.tableColumns(metadata.toLatin1());
        QCOMPARE_GT(columnsBeforeRemoval.indexOf(quotaBytesUsed.toLatin1()), -1);
        QCOMPARE_GT(columnsBeforeRemoval.indexOf(quotaBytesAvailable.toLatin1()), -1);
        QVERIFY(_db.removeColumn(quotaBytesAvailable));
        QVERIFY(_db.removeColumn(quotaBytesUsed));
        QVERIFY(_db.updateMetadataTableStructure());
        const auto columnsAfterConnect = _db.tableColumns(metadata.toLatin1());
        QVERIFY(columnsAfterConnect.indexOf(quotaBytesUsed.toLatin1()) > -1);
        QVERIFY(columnsAfterConnect.indexOf(quotaBytesAvailable.toLatin1()) > -1);
        QVERIFY(_db.hasDefaultValue(quotaBytesUsed));
        QVERIFY(_db.hasDefaultValue(quotaBytesAvailable));
    }

    void testUniqueInodeIndexRemoval_data()
    {
        QTest::addColumn<bool>("hasUniqueInodeIndex");

        QTest::newRow("existing-index") << true;
        QTest::newRow("already-removed") << false;
    }

    void testUniqueInodeIndexRemoval()
    {
        QFETCH(bool, hasUniqueInodeIndex);

        SyncJournalDb journal(_tempDir.filePath(u"inode-index-%1.db"_s.arg(hasUniqueInodeIndex)));
        auto records = QList<SyncJournalFileRecord>{};
        for (const auto &path : QByteArrayList{"existing", "unknown", "folder"}) {
            SyncJournalFileRecord record;
            record._path = path;
            record._inode = path == "unknown" ? 0 : 100 + records.size();
            record._modtime = 123456789;
            record._type = path == "folder" ? ItemTypeDirectory : ItemTypeFile;
            record._etag = "etag-" + path;
            record._fileId = "id-" + path;
            record._fileSize = 123;
            record._remotePerm = RemotePermissions::fromDbValue("RW");
            record._checksumHeader = "MD5:checksum-" + path;
            QVERIFY(journal.setFileRecord(record));
            records.append(record);
        }
        journal.close();

        {
            SqlDatabase legacyDb;
            QVERIFY(legacyDb.openOrCreateReadWrite(journal.databaseFilePath()));
            SqlQuery query(legacyDb);
            query.prepare("UPDATE metadata SET inode=NULL WHERE path='unknown';");
            QVERIFY(query.exec());
            if (hasUniqueInodeIndex) {
                query.prepare("CREATE UNIQUE INDEX metadata_unique_inode ON metadata(inode);");
                QVERIFY(query.exec());
            }
        }

        for (auto openCount = 0; openCount < 2; ++openCount) {
            QVERIFY(journal.checkConnect());
            {
                SqlQuery query(journal._db);
                query.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name='metadata_unique_inode';");
                QVERIFY(query.exec());
                QVERIFY(query.next().hasData);
                QCOMPARE(query.intValue(0), 0);
            }

            if (openCount == 0) {
                for (const auto &path : QByteArrayList{"duplicate1", "duplicate2", "zero1", "zero2"}) {
                    auto record = records.first();
                    record._path = path;
                    record._inode = path.startsWith("zero") ? 0 : records.first()._inode;
                    QVERIFY(journal.setFileRecord(record));
                    if (path == "duplicate1") {
                        record._inode = 0;
                    }
                    QVERIFY(journal.updateLocalMetadata(record.path(), record._modtime, record._fileSize, record._inode, {}));
                    records.append(record);
                }
            }

            QCOMPARE(journal.getFileRecordCount(), records.size());
            for (const auto &expected : records) {
                SyncJournalFileRecord actual;
                QVERIFY(journal.getFileRecord(expected._path, &actual));
                QVERIFY2(actual == expected, expected._path.constData());
            }
            journal.close();
        }
    }

    void testOpenStopsAfterMigrationFailure()
    {
        SyncJournalDb journal(_tempDir.filePath(u"open-migration-failure.db"_s));
        QVERIFY(journal.open());
        journal.close();

        journal.autotestFailCounter = 1;
        QVERIFY(!journal.open());
        QVERIFY(!journal.isOpen());

        journal.autotestFailCounter = -1;
        QVERIFY(journal.open());
    }

    void testOpenStopsAfterDiscoverySqlFailure()
    {
#if !defined(QT_NO_DEBUG) || defined(QT_FORCE_ASSERTS)
        QSKIP("sqlFail intentionally aborts when assertions are enabled.");
#endif
        SyncJournalDb journal(_tempDir.filePath(u"open-discovery-failure.db"_s));
        QVERIFY(journal.open());
        QCOMPARE(sqlite3_exec(journal._db.sqliteDb(),
                              "DELETE FROM version;"
                              "INSERT INTO metadata(phash,path,type) VALUES(1,'folder',2);"
                              "CREATE TRIGGER fail_discovery BEFORE UPDATE OF md5 ON metadata "
                              "BEGIN SELECT RAISE(ABORT,'injected discovery failure'); END;",
                              nullptr,
                              nullptr,
                              nullptr),
                 SQLITE_OK);
        journal.close();

        QVERIFY(!journal.open());
        QVERIFY(!journal.isOpen());
    }

    void testMigrationSqlFailure_data()
    {
        QTest::addColumn<bool>("metadata");
        QTest::addColumn<QByteArray>("setupSql");
        QTest::addColumn<QByteArray>("failedStatement");

        QTest::newRow("metadata-up-to-date") << true << QByteArray{} << QByteArray{};
        QTest::newRow("metadata-column") << true << QByteArray("ALTER TABLE metadata DROP COLUMN remotePerm;")
                                         << QByteArray("ALTER TABLE metadata ADD COLUMN remotePerm");
        QTest::newRow("metadata-column-index") << true << QByteArray("DROP INDEX metadata_fileid; ALTER TABLE metadata DROP COLUMN fileid;")
                                               << QByteArray("CREATE INDEX metadata_fileid");
        QTest::newRow("metadata-inode-index") << true << QByteArray("DROP INDEX metadata_inode;") << QByteArray("CREATE INDEX IF NOT EXISTS metadata_inode");
        QTest::newRow("metadata-path-index") << true << QByteArray("DROP INDEX metadata_path;") << QByteArray("CREATE INDEX IF NOT EXISTS metadata_path");
        QTest::newRow("metadata-parent-index") << true << QByteArray("DROP INDEX metadata_parent;") << QByteArray("CREATE INDEX IF NOT EXISTS metadata_parent");
        QTest::newRow("upload-checksum-column") << true << QByteArray("ALTER TABLE uploadinfo DROP COLUMN contentChecksum;")
                                                << QByteArray("ALTER TABLE uploadinfo ADD COLUMN contentChecksum");
        QTest::newRow("conflict-base-path-column") << true << QByteArray("ALTER TABLE conflicts DROP COLUMN basePath;")
                                                   << QByteArray("ALTER TABLE conflicts ADD COLUMN basePath");
        QTest::newRow("metadata-encryption-id-index") << true << QByteArray("DROP INDEX metadata_e2e_id;")
                                                      << QByteArray("CREATE INDEX IF NOT EXISTS metadata_e2e_id");
        QTest::newRow("metadata-encryption-status-index")
            << true << QByteArray("DROP INDEX metadata_e2e_status;") << QByteArray("CREATE INDEX IF NOT EXISTS metadata_e2e_status");
        QTest::newRow("case-conflict-base-path-index")
            << true << QByteArray("DROP INDEX caseconflicts_basePath;") << QByteArray("CREATE INDEX IF NOT EXISTS caseconflicts_basePath");
        QTest::newRow("quota-used-removal") << true
                                            << QByteArray(
                                                   "ALTER TABLE metadata DROP COLUMN quotaBytesUsed; ALTER TABLE metadata ADD COLUMN quotaBytesUsed BIGINT;")
                                            << QByteArray("ALTER TABLE metadata DROP COLUMN quotaBytesUsed");
        QTest::newRow("quota-available-removal")
            << true << QByteArray("ALTER TABLE metadata DROP COLUMN quotaBytesAvailable; ALTER TABLE metadata ADD COLUMN quotaBytesAvailable BIGINT;")
            << QByteArray("ALTER TABLE metadata DROP COLUMN quotaBytesAvailable");
        QTest::newRow("blacklist-up-to-date") << false << QByteArray{} << QByteArray{};
        QTest::newRow("blacklist-last-try-column") << false
                                                   << QByteArray(
                                                          "ALTER TABLE blacklist DROP COLUMN lastTryTime; ALTER TABLE blacklist DROP COLUMN ignoreDuration;")
                                                   << QByteArray("ALTER TABLE blacklist ADD COLUMN lastTryTime");
        QTest::newRow("blacklist-ignore-duration-column")
            << false << QByteArray("ALTER TABLE blacklist DROP COLUMN lastTryTime; ALTER TABLE blacklist DROP COLUMN ignoreDuration;")
            << QByteArray("ALTER TABLE blacklist ADD COLUMN ignoreDuration");
        QTest::newRow("blacklist-rename-column") << false << QByteArray("ALTER TABLE blacklist DROP COLUMN renameTarget;")
                                                 << QByteArray("ALTER TABLE blacklist ADD COLUMN renameTarget");
        QTest::newRow("blacklist-category-column") << false << QByteArray("ALTER TABLE blacklist DROP COLUMN errorCategory;")
                                                   << QByteArray("ALTER TABLE blacklist ADD COLUMN errorCategory");
        QTest::newRow("blacklist-request-column") << false << QByteArray("ALTER TABLE blacklist DROP COLUMN requestId;")
                                                  << QByteArray("ALTER TABLE blacklist ADD COLUMN requestId");
        QTest::newRow("blacklist-index") << false << QByteArray("DROP INDEX blacklist_index;") << QByteArray("CREATE INDEX IF NOT EXISTS blacklist_index");
    }

    void testMigrationSqlFailure()
    {
#if !defined(QT_NO_DEBUG) || defined(QT_FORCE_ASSERTS)
        QSKIP("sqlFail intentionally aborts when assertions are enabled.");
#endif
        QFETCH(bool, metadata);
        QFETCH(QByteArray, setupSql);
        QFETCH(QByteArray, failedStatement);

        SyncJournalDb journal(_tempDir.filePath(u"migration-failure-%1.db"_s.arg(QString::fromLatin1(QTest::currentDataTag()))));
        QVERIFY(journal.open());
        QCOMPARE(sqlite3_exec(journal._db.sqliteDb(), setupSql.constData(), nullptr, nullptr, nullptr), SQLITE_OK);
        auto failure = qMakePair(failedStatement, false);
        QCOMPARE(interruptStatement(journal, failure), SQLITE_OK);

        const auto result = metadata ? journal.updateMetadataTableStructure() : journal.updateErrorBlacklistTableStructure();
        const auto isOpen = journal.isOpen();
        if (isOpen) {
            sqlite3_trace_v2(journal._db.sqliteDb(), 0, nullptr, nullptr);
        }
        QCOMPARE(failure.second, !failedStatement.isEmpty());
        QCOMPARE(result, failedStatement.isEmpty());
        QCOMPARE(isOpen, failedStatement.isEmpty());
    }

    void testSqlFailureStopsUpdates_data()
    {
        QTest::addColumn<QByteArray>("failedStatement");
        QTest::newRow("virtual-file-update") << QByteArray("UPDATE metadata SET type=5");
        QTest::newRow("avoid-renames") << QByteArray("UPDATE metadata SET fileid = ''");
        QTest::newRow("schedule-discovery") << QByteArray("UPDATE metadata SET md5='_invalid_'");
    }

    void testSqlFailureStopsUpdates()
    {
#if !defined(QT_NO_DEBUG) || defined(QT_FORCE_ASSERTS)
        QSKIP("sqlFail intentionally aborts when assertions are enabled.");
#endif
        QFETCH(QByteArray, failedStatement);
        SyncJournalDb journal(_tempDir.filePath(u"update-failure-%1.db"_s.arg(QString::fromLatin1(QTest::currentDataTag()))));
        QVERIFY(journal.open());
        auto failure = qMakePair(failedStatement, false);
        QCOMPARE(interruptStatement(journal, failure), SQLITE_OK);

        const auto row = QByteArray(QTest::currentDataTag());
        if (row == "virtual-file-update") {
            journal.markVirtualFileForDownloadRecursively("");
        } else if (row == "avoid-renames") {
            journal.avoidRenamesOnNextSync(QByteArray("folder"));
        } else {
            journal.schedulePathForRemoteDiscovery(QByteArray("folder"));
        }
        const auto isOpen = journal.isOpen();
        if (isOpen) {
            sqlite3_trace_v2(journal._db.sqliteDb(), 0, nullptr, nullptr);
        }
        QVERIFY(failure.second);
        QVERIFY(!isOpen);
        QVERIFY(journal._etagStorageFilter.isEmpty());
    }

    void testFileRecordChecksum()
    {
        // Try with and without a checksum
        {
            SyncJournalFileRecord record;
            record._path = "foo-checksum";
            record._remotePerm = RemotePermissions::fromDbValue(" ");
            record._checksumHeader = "MD5:mychecksum";
            record._modtime = Utility::qDateTimeToTime_t(QDateTime::currentDateTimeUtc());
            QVERIFY(_db.setFileRecord(record));

            SyncJournalFileRecord storedRecord;
            QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo-checksum"), &storedRecord));
            QVERIFY(storedRecord._path == record._path);
            QVERIFY(storedRecord._remotePerm == record._remotePerm);
            QVERIFY(storedRecord._checksumHeader == record._checksumHeader);

            // Attention: compare time_t types here, as QDateTime seem to maintain
            // milliseconds internally, which disappear in sqlite. Go for full seconds here.
            QVERIFY(storedRecord._modtime == record._modtime);
            QVERIFY(storedRecord == record);
        }
        {
            SyncJournalFileRecord record;
            record._path = "foo-nochecksum";
            record._remotePerm = RemotePermissions::fromDbValue("RW");
            record._modtime = Utility::qDateTimeToTime_t(QDateTime::currentDateTimeUtc());

            QVERIFY(_db.setFileRecord(record));

            SyncJournalFileRecord storedRecord;
            QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo-nochecksum"), &storedRecord));
            QVERIFY(storedRecord == record);
        }
    }

    void testDownloadInfo()
    {
        using Info = SyncJournalDb::DownloadInfo;
        Info record = _db.getDownloadInfo("nonexistent");
        QVERIFY(!record._valid);

        record._errorCount = 5;
        record._etag = "ABCDEF";
        record._valid = true;
        record._tmpfile = "/tmp/foo";
        _db.setDownloadInfo("foo", record);

        Info storedRecord = _db.getDownloadInfo("foo");
        QVERIFY(storedRecord == record);

        _db.setDownloadInfo("foo", Info());
        Info wipedRecord = _db.getDownloadInfo("foo");
        QVERIFY(!wipedRecord._valid);
    }

    void testUploadInfo()
    {
        using Info = SyncJournalDb::UploadInfo;
        Info record = _db.getUploadInfo("nonexistent");
        QVERIFY(!record._valid);

        record._errorCount = 5;
        record._chunkUploadV1 = 12;
        record._transferid = 812974891;
        record._size = 12894789147;
        record._modtime = dropMsecs(QDateTime::currentDateTime());
        record._valid = true;
        _db.setUploadInfo("foo", record);

        Info storedRecord = _db.getUploadInfo("foo");
        QVERIFY(storedRecord == record);

        _db.setUploadInfo("foo", Info());
        Info wipedRecord = _db.getUploadInfo("foo");
        QVERIFY(!wipedRecord._valid);
    }

    void testNumericId()
    {
        SyncJournalFileRecord record;

        // Typical 8-digit padded id
        record._fileId = "00000001abcd";
        QCOMPARE(record.numericFileId(), QByteArray("00000001"));

        // When the numeric id overflows the 8-digit boundary
        record._fileId = "123456789ocidblaabcd";
        QCOMPARE(record.numericFileId(), QByteArray("123456789"));
    }

    void testConflictRecord()
    {
        ConflictRecord record;
        record.path = "abc";
        record.baseFileId = "def";
        record.baseModtime = 1234;
        record.baseEtag = "ghi";

        QVERIFY(!_db.conflictRecord(record.path).isValid());

        _db.setConflictRecord(record);
        auto newRecord = _db.conflictRecord(record.path);
        QVERIFY(newRecord.isValid());
        QCOMPARE(newRecord.path, record.path);
        QCOMPARE(newRecord.baseFileId, record.baseFileId);
        QCOMPARE(newRecord.baseModtime, record.baseModtime);
        QCOMPARE(newRecord.baseEtag, record.baseEtag);

        _db.deleteConflictRecord(record.path);
        QVERIFY(!_db.conflictRecord(record.path).isValid());
    }

    void testAvoidReadFromDbOnNextSync()
    {
        auto invalidEtag = QByteArray("_invalid_");
        auto initialEtag = QByteArray("etag");
        auto makeEntry = [&](const QByteArray &path, ItemType type) {
            SyncJournalFileRecord record;
            record._modtime = QDateTime::currentSecsSinceEpoch();
            record._path = path;
            record._type = type;
            record._etag = initialEtag;
            record._remotePerm = RemotePermissions::fromDbValue("RW");
            QVERIFY(_db.setFileRecord(record));
        };
        auto getEtag = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            [[maybe_unused]] const auto result = _db.getFileRecord(path, &record);
            return record._etag;
        };

        makeEntry("foodir", ItemTypeDirectory);
        makeEntry("otherdir", ItemTypeDirectory);
        makeEntry("foo%", ItemTypeDirectory); // wildcards don't apply
        makeEntry("foodi_", ItemTypeDirectory); // wildcards don't apply
        makeEntry("foodir/file", ItemTypeFile);
        makeEntry("foodir/subdir", ItemTypeDirectory);
        makeEntry("foodir/subdir/file", ItemTypeFile);
        makeEntry("foodir/otherdir", ItemTypeDirectory);
        makeEntry("fo", ItemTypeDirectory); // prefix, but does not match
        makeEntry("foodir/sub", ItemTypeDirectory); // prefix, but does not match
        makeEntry("foodir/subdir/subsubdir", ItemTypeDirectory);
        makeEntry("foodir/subdir/subsubdir/file", ItemTypeFile);
        makeEntry("foodir/subdir/otherdir", ItemTypeDirectory);

        _db.schedulePathForRemoteDiscovery(QByteArray("foodir/subdir"));

        // Direct effects of parent directories being set to _invalid_
        QCOMPARE(getEtag("foodir"), invalidEtag);
        QCOMPARE(getEtag("foodir/subdir"), invalidEtag);
        QCOMPARE(getEtag("foodir/subdir/subsubdir"), initialEtag);

        QCOMPARE(getEtag("foodir/file"), initialEtag);
        QCOMPARE(getEtag("foodir/subdir/file"), initialEtag);
        QCOMPARE(getEtag("foodir/subdir/subsubdir/file"), initialEtag);

        QCOMPARE(getEtag("fo"), initialEtag);
        QCOMPARE(getEtag("foo%"), initialEtag);
        QCOMPARE(getEtag("foodi_"), initialEtag);
        QCOMPARE(getEtag("otherdir"), initialEtag);
        QCOMPARE(getEtag("foodir/otherdir"), initialEtag);
        QCOMPARE(getEtag("foodir/sub"), initialEtag);
        QCOMPARE(getEtag("foodir/subdir/otherdir"), initialEtag);

        // Indirect effects: setFileRecord() calls filter etags
        initialEtag = "etag2";

        makeEntry("foodir", ItemTypeDirectory);
        QCOMPARE(getEtag("foodir"), invalidEtag);
        makeEntry("foodir/subdir", ItemTypeDirectory);
        QCOMPARE(getEtag("foodir/subdir"), invalidEtag);
        makeEntry("foodir/subdir/subsubdir", ItemTypeDirectory);
        QCOMPARE(getEtag("foodir/subdir/subsubdir"), initialEtag);
        makeEntry("fo", ItemTypeDirectory);
        QCOMPARE(getEtag("fo"), initialEtag);
        makeEntry("foodir/sub", ItemTypeDirectory);
        QCOMPARE(getEtag("foodir/sub"), initialEtag);
    }

    void testRecursiveDelete()
    {
        auto makeEntry = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            record._path = path;
            record._remotePerm = RemotePermissions::fromDbValue("RW");
            record._modtime = QDateTime::currentSecsSinceEpoch();
            QVERIFY(_db.setFileRecord(record));
        };

        QByteArrayList elements;
        elements
            << "foo"
            << "foo/file"
            << "bar"
            << "moo"
            << "moo/file"
            << "foo%bar"
            << "foo bla bar/file"
            << "fo_"
            << "fo_/file";
        for (const auto& elem : std::as_const(elements)) {
            makeEntry(elem);
        }

        auto checkElements = [&]() {
            bool ok = true;
            for (const auto& elem : std::as_const(elements)) {
                SyncJournalFileRecord record;
                if (!_db.getFileRecord(elem, &record) || !record.isValid()) {
                    qWarning() << "Missing record: " << elem;
                    ok = false;
                }
            }
            return ok;
        };

        QVERIFY(_db.deleteFileRecord("moo", true));
        elements.removeAll("moo");
        elements.removeAll("moo/file");
        QVERIFY(checkElements());

        QVERIFY(_db.deleteFileRecord("fo_", true));
        elements.removeAll("fo_");
        elements.removeAll("fo_/file");
        QVERIFY(checkElements());

        QVERIFY(_db.deleteFileRecord("foo%bar", true));
        elements.removeAll("foo%bar");
        QVERIFY(checkElements());
    }

    void testPinState()
    {
        auto make = [&](const QByteArray &path, PinState state) {
            _db.internalPinStates().setForPath(path, state);
            auto pinState = _db.internalPinStates().rawForPath(path);
            QVERIFY(pinState);
            QCOMPARE(*pinState, state);
        };
        auto get = [&](const QByteArray &path) -> PinState {
            auto state = _db.internalPinStates().effectiveForPath(path);
            if (!state) {
                QTest::qFail("couldn't read pin state", __FILE__, __LINE__);
                return PinState::Inherited;
            }
            return *state;
        };
        auto getRecursive = [&](const QByteArray &path) -> PinState {
            auto state = _db.internalPinStates().effectiveForPathRecursive(path);
            if (!state) {
                QTest::qFail("couldn't read pin state", __FILE__, __LINE__);
                return PinState::Inherited;
            }
            return *state;
        };
        auto getRaw = [&](const QByteArray &path) -> PinState {
            auto state = _db.internalPinStates().rawForPath(path);
            if (!state) {
                QTest::qFail("couldn't read pin state", __FILE__, __LINE__);
                return PinState::Inherited;
            }
            return *state;
        };

        _db.internalPinStates().wipeForPathAndBelow("");
        auto list = _db.internalPinStates().rawList();
        QCOMPARE(list->size(), 0);

        // Make a thrice-nested setup
        make("", PinState::AlwaysLocal);
        make("local", PinState::AlwaysLocal);
        make("online", PinState::OnlineOnly);
        make("inherit", PinState::Inherited);
        for (auto base : {"local/", "online/", "inherit/"}) {
            make(QByteArray(base) + "inherit", PinState::Inherited);
            make(QByteArray(base) + "local", PinState::AlwaysLocal);
            make(QByteArray(base) + "online", PinState::OnlineOnly);

            for (auto base2 : {"local/", "online/", "inherit/"}) {
                make(QByteArray(base) + base2 + "inherit", PinState::Inherited);
                make(QByteArray(base) + base2 + "local", PinState::AlwaysLocal);
                make(QByteArray(base) + base2 + "online", PinState::OnlineOnly);
            }
        }

        list = _db.internalPinStates().rawList();
        QCOMPARE(list->size(), 4 + 9 + 27);

        // Baseline direct checks (the fallback for unset root pinstate is AlwaysLocal)
        QCOMPARE(get(""), PinState::AlwaysLocal);
        QCOMPARE(get("local"), PinState::AlwaysLocal);
        QCOMPARE(get("online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("nonexistent"), PinState::AlwaysLocal);
        QCOMPARE(get("online/local"), PinState::AlwaysLocal);
        QCOMPARE(get("local/online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit/local"), PinState::AlwaysLocal);
        QCOMPARE(get("inherit/online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("inherit/nonexistent"), PinState::AlwaysLocal);

        // Inheriting checks, level 1
        QCOMPARE(get("local/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("local/nonexistent"), PinState::AlwaysLocal);
        QCOMPARE(get("online/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("online/nonexistent"), PinState::OnlineOnly);

        // Inheriting checks, level 2
        QCOMPARE(get("local/inherit/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("local/local/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("local/local/nonexistent"), PinState::AlwaysLocal);
        QCOMPARE(get("local/online/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("local/online/nonexistent"), PinState::OnlineOnly);
        QCOMPARE(get("online/inherit/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("online/local/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("online/local/nonexistent"), PinState::AlwaysLocal);
        QCOMPARE(get("online/online/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("online/online/nonexistent"), PinState::OnlineOnly);

        // Spot check the recursive variant
        QCOMPARE(getRecursive(""), PinState::Inherited);
        QCOMPARE(getRecursive("local"), PinState::Inherited);
        QCOMPARE(getRecursive("online"), PinState::Inherited);
        QCOMPARE(getRecursive("inherit"), PinState::Inherited);
        QCOMPARE(getRecursive("online/local"), PinState::Inherited);
        QCOMPARE(getRecursive("online/local/inherit"), PinState::AlwaysLocal);
        QCOMPARE(getRecursive("inherit/inherit/inherit"), PinState::AlwaysLocal);
        QCOMPARE(getRecursive("inherit/online/inherit"), PinState::OnlineOnly);
        QCOMPARE(getRecursive("inherit/online/local"), PinState::AlwaysLocal);
        make("local/local/local/local", PinState::AlwaysLocal);
        QCOMPARE(getRecursive("local/local/local"), PinState::AlwaysLocal);
        QCOMPARE(getRecursive("local/local/local/local"), PinState::AlwaysLocal);

        // Check changing the root pin state
        make("", PinState::OnlineOnly);
        QCOMPARE(get("local"), PinState::AlwaysLocal);
        QCOMPARE(get("online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit"), PinState::OnlineOnly);
        QCOMPARE(get("nonexistent"), PinState::OnlineOnly);
        make("", PinState::AlwaysLocal);
        QCOMPARE(get("local"), PinState::AlwaysLocal);
        QCOMPARE(get("online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("nonexistent"), PinState::AlwaysLocal);

        // Wiping
        QCOMPARE(getRaw("local/local"), PinState::AlwaysLocal);
        _db.internalPinStates().wipeForPathAndBelow("local/local");
        QCOMPARE(getRaw("local"), PinState::AlwaysLocal);
        QCOMPARE(getRaw("local/local"), PinState::Inherited);
        QCOMPARE(getRaw("local/local/local"), PinState::Inherited);
        QCOMPARE(getRaw("local/local/online"), PinState::Inherited);
        list = _db.internalPinStates().rawList();
        QCOMPARE(list->size(), 4 + 9 + 27 - 4);

        // Wiping everything
        _db.internalPinStates().wipeForPathAndBelow("");
        QCOMPARE(getRaw(""), PinState::Inherited);
        QCOMPARE(getRaw("local"), PinState::Inherited);
        QCOMPARE(getRaw("online"), PinState::Inherited);
        list = _db.internalPinStates().rawList();
        QCOMPARE(list->size(), 0);
    }

    void testHasFileIds()
    {
        QList<qint64> allFileIds = {};
        const auto makeEntry = [this, &allFileIds](const qint64 &fileId) -> void {
            SyncJournalFileRecord record;
            record._fileId = u"%1oc123xyz987e"_s.arg(fileId, 8, 10, '0'_L1).toLocal8Bit();
            record._modtime = QDateTime::currentSecsSinceEpoch();
            record._path = u"item%1"_s.arg(fileId).toLocal8Bit();
            record._type = ItemTypeFile;
            record._etag = "etag"_ba;
            QVERIFY(_db.setFileRecord(record));

            allFileIds.append(fileId);
        };

        // generate some test data: -9, 0..32, int32_max..(int32_max + 32), (int64_max - 32)..int64_max
        makeEntry(-9);

        for (qint64 fileId = 0; fileId <= 32; fileId++) {
            makeEntry(fileId);
        }

        constexpr qint64 maxInt32 = std::numeric_limits<qint32>::max();
        for (qint64 fileId = maxInt32; fileId <= maxInt32 + 32; fileId++) {
            makeEntry(fileId);
        }

        constexpr qint64 maxInt64 = std::numeric_limits<qint64>::max();
        for (qint64 fileId = maxInt64 - 32; fileId < maxInt64; fileId++) {
            makeEntry(fileId);
        }
        makeEntry(maxInt64); // have an entry for the maximum int64 value

        // these exist:
        QVERIFY(_db.hasFileIds({4}));
        QVERIFY(_db.hasFileIds({-9}));
        QVERIFY(_db.hasFileIds({8, 25, 31}));
        QVERIFY(_db.hasFileIds({maxInt32 + 4, maxInt64 - 17}));
        QVERIFY(_db.hasFileIds({maxInt64 - 17}));
        QVERIFY(_db.hasFileIds({maxInt64}));

        // these don't:
        QVERIFY(!_db.hasFileIds({-8}));
        QVERIFY(!_db.hasFileIds({-16}));
        QVERIFY(!_db.hasFileIds({-(maxInt32 + 4)}));
        QVERIFY(!_db.hasFileIds({maxInt64 - 33}));
        QVERIFY(!_db.hasFileIds({maxInt32 + 33}));
        QVERIFY(!_db.hasFileIds({std::numeric_limits<qint32>::min()}));
        QVERIFY(!_db.hasFileIds({std::numeric_limits<qint64>::min()}));

        // as fileids are padded with zeroes, ensure that there is no accidental base-8 conversion done
        // 0o37 (octal) == 31 (decimal) --> 37(dec) does not exist, but 31(dec) does.
        QVERIFY(_db.hasFileIds({037})); // octal
        QVERIFY(!_db.hasFileIds({37})); // decimal

        QVERIFY(!_db.hasFileIds({33}));    // 33 does not exist...
        QVERIFY(_db.hasFileIds({33, 25})); // ...but 25 does

        // nothing doesn't exist
        QVERIFY(!_db.hasFileIds({}));

        // checking for a large amount of file ids should also work just fine, and be reasonably fast.
        QBENCHMARK {
            QVERIFY(_db.hasFileIds(allFileIds));
        }
    }

private:
    static int interruptStatement(SyncJournalDb &journal, QPair<QByteArray, bool> &failure)
    {
        return sqlite3_trace_v2(
            journal._db.sqliteDb(),
            SQLITE_TRACE_STMT,
            [](unsigned, void *context, void *statement, void *) {
                auto &failure = *static_cast<QPair<QByteArray, bool> *>(context);
                const auto query = static_cast<sqlite3_stmt *>(statement);
                if (!failure.first.isEmpty() && QByteArray(sqlite3_sql(query)).startsWith(failure.first)) {
                    failure.second = true;
                    sqlite3_interrupt(sqlite3_db_handle(query));
                }
                return 0;
            },
            &failure);
    }

    SyncJournalDb _db;
};

QTEST_APPLESS_MAIN(TestSyncJournalDB)
#include "testsyncjournaldb.moc"
