/*
 *    This software is in the public domain, furnished "as is", without technical
 *       support, and with no warranty, express or implied, as to its usefulness for
 *          any purpose.
 *          */

#ifndef MIRALL_TESTSYNCJOURNALDB_H
#define MIRALL_TESTSYNCJOURNALDB_H

#include <QtTest>

#include <sqlite3.h>

#include "libsync/syncjournaldb.h"
#include "libsync/syncjournalfilerecord.h"

using namespace OCC;

namespace {

const char testdbC[] = "/tmp";
}

class TestSyncJournalDB : public QObject
{
    Q_OBJECT

public:
    TestSyncJournalDB()
        : _db(testdbC)
    {
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
        record._mode = -17;
        record._fileSize = 213089055;
        QVERIFY(_db.setFileRecord(record));

        SyncJournalFileRecord storedRecord = _db.getFileRecord("foo");
        QVERIFY(storedRecord == record);

        QVERIFY(_db.deleteFileRecord("foo"));
        record = _db.getFileRecord("foo");
        QVERIFY(!record.isValid());
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

#endif
