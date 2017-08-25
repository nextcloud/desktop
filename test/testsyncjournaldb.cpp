/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#include <QtTest>

#include <sqlite3.h>

#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"

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

    QDateTime dropMsecs(QDateTime time)
    {
        return Utility::qDateTimeFromTime_t(Utility::qDateTimeToTime_t(time));
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
        SyncJournalFileRecord record = _db.getFileRecord("nonexistant");
        QVERIFY(!record.isValid());

        record._path = "foo";
        record._inode = 1234;
        record._modtime = dropMsecs(QDateTime::currentDateTime());
        record._type = 5;
        record._etag = "789789";
        record._fileId = "abcd";
        record._remotePerm = "744";
        record._fileSize = 213089055;
        record._checksumHeader = "MD5:mychecksum";
        QVERIFY(_db.setFileRecord(record));

        SyncJournalFileRecord storedRecord = _db.getFileRecord("foo");
        QVERIFY(storedRecord == record);

        // Update checksum
        record._checksumHeader = "Adler32:newchecksum";
        _db.updateFileRecordChecksum("foo", "newchecksum", "Adler32");
        storedRecord = _db.getFileRecord("foo");
        QVERIFY(storedRecord == record);

        // Update metadata
        record._inode = 12345;
        record._modtime = dropMsecs(QDateTime::currentDateTime().addDays(1));
        record._type = 7;
        record._etag = "789FFF";
        record._fileId = "efg";
        record._remotePerm = "777";
        record._fileSize = 289055;
        _db.setFileRecordMetadata(record);
        storedRecord = _db.getFileRecord("foo");
        QVERIFY(storedRecord == record);

        QVERIFY(_db.deleteFileRecord("foo"));
        record = _db.getFileRecord("foo");
        QVERIFY(!record.isValid());
    }

    void testFileRecordChecksum()
    {
        // Try with and without a checksum
        {
            SyncJournalFileRecord record;
            record._path = "foo-checksum";
            record._remotePerm = "744";
            record._checksumHeader = "MD5:mychecksum";
            record._modtime = QDateTime::currentDateTimeUtc();
            QVERIFY(_db.setFileRecord(record));

            SyncJournalFileRecord storedRecord = _db.getFileRecord("foo-checksum");
            QVERIFY(storedRecord._path == record._path);
            QVERIFY(storedRecord._remotePerm == record._remotePerm);
            QVERIFY(storedRecord._checksumHeader == record._checksumHeader);

            // qDebug()<< "OOOOO " << storedRecord._modtime.toTime_t() << record._modtime.toTime_t();

            // Attention: compare time_t types here, as QDateTime seem to maintain
            // milliseconds internally, which disappear in sqlite. Go for full seconds here.
            QVERIFY(storedRecord._modtime.toTime_t() == record._modtime.toTime_t());
            QVERIFY(storedRecord == record);
        }
        {
            SyncJournalFileRecord record;
            record._path = "foo-nochecksum";
            record._remotePerm = "744";
	    record._modtime = QDateTime::currentDateTimeUtc();

            QVERIFY(_db.setFileRecord(record));

            SyncJournalFileRecord storedRecord = _db.getFileRecord("foo-nochecksum");
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

private:
    SyncJournalDb _db;
};

QTEST_APPLESS_MAIN(TestSyncJournalDB)
#include "testsyncjournaldb.moc"
