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

    qint64 dropMsecs(QDateTime time)
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
        record._type = 5;
        record._etag = "789789";
        record._fileId = "abcd";
        record._remotePerm = RemotePermissions("RW");
        record._fileSize = 213089055;
        record._checksumHeader = "MD5:mychecksum";
        QVERIFY(_db.setFileRecord(record));

        SyncJournalFileRecord storedRecord;
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo"), &storedRecord));
        QVERIFY(storedRecord == record);

        // Update checksum
        record._checksumHeader = "Adler32:newchecksum";
        _db.updateFileRecordChecksum("foo", "newchecksum", "Adler32");
        QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo"), &storedRecord));
        QVERIFY(storedRecord == record);

        // Update metadata
        record._modtime = dropMsecs(QDateTime::currentDateTime().addDays(1));
        // try a value that only fits uint64, not int64
        record._inode = std::numeric_limits<quint64>::max() - std::numeric_limits<quint32>::max() - 1;
        record._type = 7;
        record._etag = "789FFF";
        record._fileId = "efg";
        record._remotePerm = RemotePermissions("NV");
        record._fileSize = 289055;
        _db.setFileRecordMetadata(record);
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
            record._remotePerm = RemotePermissions("RW");
            record._checksumHeader = "MD5:mychecksum";
            record._modtime = Utility::qDateTimeToTime_t(QDateTime::currentDateTimeUtc());
            QVERIFY(_db.setFileRecord(record));

            SyncJournalFileRecord storedRecord;
            QVERIFY(_db.getFileRecord(QByteArrayLiteral("foo-checksum"), &storedRecord));
            QVERIFY(storedRecord._path == record._path);
            QVERIFY(storedRecord._remotePerm == record._remotePerm);
            QVERIFY(storedRecord._checksumHeader == record._checksumHeader);

            // qDebug()<< "OOOOO " << storedRecord._modtime.toTime_t() << record._modtime.toTime_t();

            // Attention: compare time_t types here, as QDateTime seem to maintain
            // milliseconds internally, which disappear in sqlite. Go for full seconds here.
            QVERIFY(storedRecord._modtime == record._modtime);
            QVERIFY(storedRecord == record);
        }
        {
            SyncJournalFileRecord record;
            record._path = "foo-nochecksum";
            record._remotePerm = RemotePermissions("RWN");
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
        Info record = _db.getDownloadInfo("nonexistant");
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
        typedef SyncJournalDb::UploadInfo Info;
        Info record = _db.getUploadInfo("nonexistant");
        QVERIFY(!record._valid);

        record._errorCount = 5;
        record._chunk = 12;
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

    void testAvoidReadFromDbOnNextSync()
    {
        auto invalidEtag = QByteArray("_invalid_");
        auto initialEtag = QByteArray("etag");
        auto makeEntry = [&](const QByteArray &path, int type) {
            SyncJournalFileRecord record;
            record._path = path;
            record._type = type;
            record._etag = initialEtag;
            _db.setFileRecord(record);
        };
        auto getEtag = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            _db.getFileRecord(path, &record);
            return record._etag;
        };

        makeEntry("foodir", 2);
        makeEntry("otherdir", 2);
        makeEntry("foo%", 2); // wildcards don't apply
        makeEntry("foodi_", 2); // wildcards don't apply
        makeEntry("foodir/file", 0);
        makeEntry("foodir/subdir", 2);
        makeEntry("foodir/subdir/file", 0);
        makeEntry("foodir/otherdir", 2);
        makeEntry("fo", 2); // prefix, but does not match
        makeEntry("foodir/sub", 2); // prefix, but does not match
        makeEntry("foodir/subdir/subsubdir", 2);
        makeEntry("foodir/subdir/subsubdir/file", 0);
        makeEntry("foodir/subdir/otherdir", 2);

        _db.avoidReadFromDbOnNextSync(QByteArray("foodir/subdir"));

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

        makeEntry("foodir", 2);
        QCOMPARE(getEtag("foodir"), invalidEtag);
        makeEntry("foodir/subdir", 2);
        QCOMPARE(getEtag("foodir/subdir"), invalidEtag);
        makeEntry("foodir/subdir/subsubdir", 2);
        QCOMPARE(getEtag("foodir/subdir/subsubdir"), initialEtag);
        makeEntry("fo", 2);
        QCOMPARE(getEtag("fo"), initialEtag);
        makeEntry("foodir/sub", 2);
        QCOMPARE(getEtag("foodir/sub"), initialEtag);
    }

    void testRecursiveDelete()
    {
        auto makeEntry = [&](const QByteArray &path) {
            SyncJournalFileRecord record;
            record._path = path;
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
        for (auto elem : elements)
            makeEntry(elem);

        auto checkElements = [&]() {
            bool ok = true;
            for (auto elem : elements) {
                SyncJournalFileRecord record;
                _db.getFileRecord(elem, &record);
                if (!record.isValid()) {
                    qWarning() << "Missing record: " << elem;
                    ok = false;
                }
            }
            return ok;
        };

        _db.deleteFileRecord("moo", true);
        elements.removeAll("moo");
        elements.removeAll("moo/file");
        QVERIFY(checkElements());

        _db.deleteFileRecord("fo_", true);
        elements.removeAll("fo_");
        elements.removeAll("fo_/file");
        QVERIFY(checkElements());

        _db.deleteFileRecord("foo%bar", true);
        elements.removeAll("foo%bar");
        QVERIFY(checkElements());
    }

private:
    SyncJournalDb _db;
};

QTEST_APPLESS_MAIN(TestSyncJournalDB)
#include "testsyncjournaldb.moc"
