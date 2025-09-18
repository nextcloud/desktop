/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>
#include <common/syncjournaldb.h>

using namespace OCC;

class TestUploadReset : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    // Verify that the chunked transfer eventually gets reset with the new chunking
    void testFileUploadNg() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        fakeFolder.syncEngine().account()->setCapabilities({ { "dav", QVariantMap{
                {"chunking", "1.0"},
                {"httpErrorCodesThatResetFailingChunkedUploads", QVariantList{500} } } } });

        const auto size = 200LL * 1024LL * 1024LL; // 200 MiB
        fakeFolder.localModifier().insert("A/a0", size);
        QDateTime modTime = QDateTime::currentDateTime();
        fakeFolder.localModifier().setModTime("A/a0", modTime);

        // Create a transfer id, so we can make the final MOVE fail
        SyncJournalDb::UploadInfo uploadInfo;
        uploadInfo._transferid = 1;
        uploadInfo._valid = true;
        uploadInfo._modtime = Utility::qDateTimeToTime_t(modTime);
        uploadInfo._size = size;
        fakeFolder.syncEngine().journal()->setUploadInfo("A/a0", uploadInfo);

        fakeFolder.uploadState().mkdir("1");
        fakeFolder.serverErrorPaths().append("1/.file");

        QVERIFY(!fakeFolder.syncOnce());

        uploadInfo = fakeFolder.syncEngine().journal()->getUploadInfo("A/a0");
        QCOMPARE(uploadInfo._errorCount, 1);
        QCOMPARE(uploadInfo._transferid, 1U);

        QVERIFY(fakeFolder.syncEngine().journal()->wipeErrorBlacklist());
        QVERIFY(!fakeFolder.syncOnce());

        uploadInfo = fakeFolder.syncEngine().journal()->getUploadInfo("A/a0");
        QCOMPARE(uploadInfo._errorCount, 2);
        QCOMPARE(uploadInfo._transferid, 1U);

        QVERIFY(fakeFolder.syncEngine().journal()->wipeErrorBlacklist());
        QVERIFY(!fakeFolder.syncOnce());

        uploadInfo = fakeFolder.syncEngine().journal()->getUploadInfo("A/a0");
        QCOMPARE(uploadInfo._errorCount, 3);
        QCOMPARE(uploadInfo._transferid, 1U);

        QVERIFY(fakeFolder.syncEngine().journal()->wipeErrorBlacklist());
        QVERIFY(!fakeFolder.syncOnce());

        uploadInfo = fakeFolder.syncEngine().journal()->getUploadInfo("A/a0");
        QCOMPARE(uploadInfo._errorCount, 0);
        QCOMPARE(uploadInfo._transferid, 0U);
        QVERIFY(!uploadInfo._valid);
    }
};

QTEST_GUILESS_MAIN(TestUploadReset)
#include "testuploadreset.moc"
