/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#include <QtTest>

#include <sqlite3.h>

#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "logger.h"

using namespace OCC;

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

private:
    bool makeEntry(const QByteArray &path, ItemType type, const QByteArray &etag)
    {
        SyncJournalFileRecord record;
        record._path = path;
        record._type = type;
        record._etag = etag;
        record._remotePerm = RemotePermissions::fromDbValue("RW");
        return _db.setFileRecord(record).isValid();
    }

private slots:
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
        auto getEtag = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            [[maybe_unused]] const auto result = _db.getFileRecord(path, &record);
            return record._etag;
        };

        QVERIFY(makeEntry("foodir", ItemTypeDirectory, initialEtag));
        QVERIFY(makeEntry("otherdir", ItemTypeDirectory, initialEtag));
        QVERIFY(makeEntry("foo%", ItemTypeDirectory, initialEtag)); // wildcards don't apply
        QVERIFY(makeEntry("foodi_", ItemTypeDirectory, initialEtag)); // wildcards don't apply
        QVERIFY(makeEntry("foodir/file", ItemTypeFile, initialEtag));
        QVERIFY(makeEntry("foodir/subdir", ItemTypeDirectory, initialEtag));
        QVERIFY(makeEntry("foodir/subdir/file", ItemTypeFile, initialEtag));
        QVERIFY(makeEntry("foodir/otherdir", ItemTypeDirectory, initialEtag));
        QVERIFY(makeEntry("fo", ItemTypeDirectory, initialEtag)); // prefix, but does not match
        QVERIFY(makeEntry("foodir/sub", ItemTypeDirectory, initialEtag)); // prefix, but does not match
        QVERIFY(makeEntry("foodir/subdir/subsubdir", ItemTypeDirectory, initialEtag));
        QVERIFY(makeEntry("foodir/subdir/subsubdir/file", ItemTypeFile, initialEtag));
        QVERIFY(makeEntry("foodir/subdir/otherdir", ItemTypeDirectory, initialEtag));

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

        QVERIFY(makeEntry("foodir", ItemTypeDirectory, initialEtag));
        QCOMPARE(getEtag("foodir"), invalidEtag);
        QVERIFY(makeEntry("foodir/subdir", ItemTypeDirectory, initialEtag));
        QCOMPARE(getEtag("foodir/subdir"), invalidEtag);
        QVERIFY(makeEntry("foodir/subdir/subsubdir", ItemTypeDirectory, initialEtag));
        QCOMPARE(getEtag("foodir/subdir/subsubdir"), initialEtag);
        QVERIFY(makeEntry("fo", ItemTypeDirectory, initialEtag));
        QCOMPARE(getEtag("fo"), initialEtag);
        QVERIFY(makeEntry("foodir/sub", ItemTypeDirectory, initialEtag));
        QCOMPARE(getEtag("foodir/sub"), initialEtag);
    }

    void testRecursiveDelete()
    {
        auto makeDummyEntry = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            record._path = path;
            record._remotePerm = RemotePermissions::fromDbValue("RW");
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
            makeDummyEntry(elem);
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

    void testUpdateParentForAllChildren()
    {
        const auto initialEtag = QByteArray("etag");

        QVector<QPair<QByteArray, CSyncEnums::ItemType>> folder1Contents = {
            {QByteArrayLiteral("common_parent"), ItemTypeDirectory},
            {QByteArrayLiteral("common_parent/file"), ItemTypeFile},
            {QByteArrayLiteral("common_parent/subdir"), ItemTypeDirectory},
            {QByteArrayLiteral("common_parent/subdir/file"), ItemTypeFile},
            {QByteArrayLiteral("common_parent/otherdir"), ItemTypeDirectory},
        };
        const auto folderContents1MovedParent = QByteArrayLiteral("common_parent_moved");
        QVector<QPair<QByteArray, CSyncEnums::ItemType>> folder1ContentsMoved;
        for (int i = 0; i < folder1Contents.size(); ++i) {
            folder1ContentsMoved.push_back({folderContents1MovedParent + "/" + folder1Contents[i].first, folder1Contents[i].second});
        }
        for (const auto &folderItem : folder1Contents) {
            QVERIFY(makeEntry(folderItem.first, folderItem.second, initialEtag));
        }
        QVERIFY(makeEntry(folder1ContentsMoved.first().first, folder1ContentsMoved.first().second, initialEtag));

        QVector<QPair<QByteArray, CSyncEnums::ItemType>> folder2Contents = {
            {QByteArrayLiteral("another_common_parent"), ItemTypeDirectory},
            {QByteArrayLiteral("another_common_parent/sub"), ItemTypeDirectory},
            {QByteArrayLiteral("another_common_parent/subdir/subsubdir"), ItemTypeDirectory},
            {QByteArrayLiteral("another_common_parent/subdir/subsubdir/file"), ItemTypeFile},
            {QByteArrayLiteral("another_common_parent/subdir/otherdir"), ItemTypeDirectory},
        };
        const auto folderContents2MovedParent = QByteArrayLiteral("another_common_parent_moved");
        QVector<QPair<QByteArray, CSyncEnums::ItemType>> folder2ContentsMoved;
        for (int i = 0; i < folder2Contents.size(); ++i) {
            folder2ContentsMoved.push_back({folderContents2MovedParent + "/" + folder2Contents[i].first, folder2Contents[i].second});
        }
        for (const auto &folderItem : folder2Contents) {
            QVERIFY(makeEntry(folderItem.first, folderItem.second, initialEtag));
        }
        QVERIFY(makeEntry(folder2ContentsMoved.first().first, folder2ContentsMoved.first().second, initialEtag));

        // move a folder under new location, all children paths must get updated with one query
        QVERIFY(_db.updateParentForAllChildren(folder1Contents.first().first, folder1ContentsMoved.first().first));
        QVERIFY(_db.updateParentForAllChildren(folder2Contents.first().first, folder2ContentsMoved.first().first));

        // verify all moved records exist under new paths
        for (const auto &folderItemMoved : folder1ContentsMoved) {
            SyncJournalFileRecord movedRecord;
            QVERIFY(_db.getFileRecord(folderItemMoved.first, &movedRecord));
            QVERIFY(movedRecord.isValid());
        }
        for (const auto &folderItem2Moved : folder2ContentsMoved) {
            SyncJournalFileRecord movedRecord;
            QVERIFY(_db.getFileRecord(folderItem2Moved.first, &movedRecord));
            QVERIFY(movedRecord.isValid());
        }
    }

private:
    SyncJournalDb _db;
};

QTEST_APPLESS_MAIN(TestSyncJournalDB)
#include "testsyncjournaldb.moc"
