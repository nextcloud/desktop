/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#include <QtTest>

#include <sqlite3.h>

#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"

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

    qint64 dropMsecs(const QDateTime &time)
    {
        return Utility::qDateTimeToTime_t(time);
    }

private slots:

    void initTestCase()
    {
    }

    void cleanupTestCase()
    {
        const QString file = _db.databaseFilePath();
        QFile::remove(file);
    }

    void testFileRecord()
    {
        SyncJournalFileRecord record;
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("nonexistant"), &record));
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
        record._checksumHeader = "ADLER32:newchecksum";
        _db.updateFileRecordChecksum(QStringLiteral("foo"), "newchecksum", CheckSums::fromByteArray("Adler32"));
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
        _db.setFileRecord(record);
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
            QCOMPARE(storedRecord._path, record._path);
            QCOMPARE(storedRecord._remotePerm, record._remotePerm);
            QCOMPARE(storedRecord._checksumHeader, record._checksumHeader);

            // qDebug()<< "OOOOO " << storedRecord._modtime.toTime_t() << record._modtime.toTime_t();

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
        typedef SyncJournalDb::DownloadInfo Info;
        Info record = _db.getDownloadInfo(QStringLiteral("nonexistant"));
        QVERIFY(!record._valid);

        record._errorCount = 5;
        record._etag = "ABCDEF";
        record._valid = true;
        record._tmpfile = QLatin1String("/tmp/foo");
        _db.setDownloadInfo(QStringLiteral("foo"), record);

        Info storedRecord = _db.getDownloadInfo(QStringLiteral("foo"));
        QVERIFY(storedRecord == record);

        _db.setDownloadInfo(QStringLiteral("foo"), Info());
        Info wipedRecord = _db.getDownloadInfo(QStringLiteral("foo"));
        QVERIFY(!wipedRecord._valid);
    }

    void testUploadInfo()
    {
        typedef SyncJournalDb::UploadInfo Info;
        Info record = _db.getUploadInfo(QStringLiteral("nonexistant"));
        QVERIFY(!record._valid);

        record._errorCount = 5;
        record._chunk = 12;
        record._transferid = 812974891;
        record._size = 12894789147;
        record._modtime = dropMsecs(QDateTime::currentDateTime());
        record._valid = true;
        _db.setUploadInfo(QStringLiteral("foo"), record);

        Info storedRecord = _db.getUploadInfo(QStringLiteral("foo"));
        QVERIFY(storedRecord == record);

        _db.setUploadInfo(QStringLiteral("foo"), Info());
        Info wipedRecord = _db.getUploadInfo(QStringLiteral("foo"));
        QVERIFY(!wipedRecord._valid);
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
            record._path = path;
            record._type = type;
            record._etag = initialEtag;
            record._remotePerm = RemotePermissions::fromDbValue("RW");
            _db.setFileRecord(record);
        };
        auto getEtag = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            _db.getFileRecord(path, &record);
            return record._etag;
        };

        const auto dirType = ItemTypeDirectory;
        const auto fileType = ItemTypeFile;

        makeEntry("foodir", dirType);
        makeEntry("otherdir", dirType);
        makeEntry("foo%", dirType); // wildcards don't apply
        makeEntry("foodi_", dirType); // wildcards don't apply
        makeEntry("foodir/file", fileType);
        makeEntry("foodir/subdir", dirType);
        makeEntry("foodir/subdir/file", fileType);
        makeEntry("foodir/otherdir", dirType);
        makeEntry("fo", dirType); // prefix, but does not match
        makeEntry("foodir/sub", dirType); // prefix, but does not match
        makeEntry("foodir/subdir/subsubdir", dirType);
        makeEntry("foodir/subdir/subsubdir/file", fileType);
        makeEntry("foodir/subdir/otherdir", dirType);

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

        makeEntry("foodir", dirType);
        QCOMPARE(getEtag("foodir"), invalidEtag);
        makeEntry("foodir/subdir", dirType);
        QCOMPARE(getEtag("foodir/subdir"), invalidEtag);
        makeEntry("foodir/subdir/subsubdir", dirType);
        QCOMPARE(getEtag("foodir/subdir/subsubdir"), initialEtag);
        makeEntry("fo", dirType);
        QCOMPARE(getEtag("fo"), initialEtag);
        makeEntry("foodir/sub", dirType);
        QCOMPARE(getEtag("foodir/sub"), initialEtag);
    }

    void testRecursiveDelete()
    {
        auto makeEntry = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            record._path = path;
            record._remotePerm = RemotePermissions::fromDbValue("RW");
            _db.setFileRecord(record);
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
        for (const auto &elem : qAsConst(elements))
            makeEntry(elem);

        auto checkElements = [&]() {
            bool ok = true;
            for (const auto &elem : qAsConst(elements)) {
                SyncJournalFileRecord record;
                _db.getFileRecord(elem, &record);
                if (!record.isValid()) {
                    qWarning() << "Missing record: " << elem;
                    ok = false;
                }
            }
            return ok;
        };

        _db.deleteFileRecord(QStringLiteral("moo"), true);
        elements.removeAll("moo");
        elements.removeAll("moo/file");
        QVERIFY(checkElements());

        _db.deleteFileRecord(QStringLiteral("fo_"), true);
        elements.removeAll("fo_");
        elements.removeAll("fo_/file");
        QVERIFY(checkElements());

        _db.deleteFileRecord(QStringLiteral("foo%bar"), true);
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
        QCOMPARE(get("nonexistant"), PinState::AlwaysLocal);
        QCOMPARE(get("online/local"), PinState::AlwaysLocal);
        QCOMPARE(get("local/online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit/local"), PinState::AlwaysLocal);
        QCOMPARE(get("inherit/online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("inherit/nonexistant"), PinState::AlwaysLocal);

        // Inheriting checks, level 1
        QCOMPARE(get("local/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("local/nonexistant"), PinState::AlwaysLocal);
        QCOMPARE(get("online/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("online/nonexistant"), PinState::OnlineOnly);

        // Inheriting checks, level 2
        QCOMPARE(get("local/inherit/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("local/local/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("local/local/nonexistant"), PinState::AlwaysLocal);
        QCOMPARE(get("local/online/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("local/online/nonexistant"), PinState::OnlineOnly);
        QCOMPARE(get("online/inherit/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("online/local/inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("online/local/nonexistant"), PinState::AlwaysLocal);
        QCOMPARE(get("online/online/inherit"), PinState::OnlineOnly);
        QCOMPARE(get("online/online/nonexistant"), PinState::OnlineOnly);

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
        QCOMPARE(get("nonexistant"), PinState::OnlineOnly);
        make("", PinState::AlwaysLocal);
        QCOMPARE(get("local"), PinState::AlwaysLocal);
        QCOMPARE(get("online"), PinState::OnlineOnly);
        QCOMPARE(get("inherit"), PinState::AlwaysLocal);
        QCOMPARE(get("nonexistant"), PinState::AlwaysLocal);

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

private:
    SyncJournalDb _db;
};

QTEST_APPLESS_MAIN(TestSyncJournalDB)
#include "testsyncjournaldb.moc"
