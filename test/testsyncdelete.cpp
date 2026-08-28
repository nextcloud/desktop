/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace OCC;

class TestSyncDelete : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testDeleteDirectoryWithNewFile()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        // Remove a directory on the server with new files on the client
        fakeFolder.remoteModifier().remove("A");
        fakeFolder.localModifier().insert("A/hello.txt");

        // Symmetry
        fakeFolder.localModifier().remove("B");
        fakeFolder.remoteModifier().insert("B/hello.txt");

        QVERIFY(fakeFolder.syncOnce());

        // A/a1 must be gone because the directory was removed on the server, but hello.txt must be there
        QVERIFY(!fakeFolder.currentRemoteState().find("A/a1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/hello.txt"));

        // Symmetry
        QVERIFY(!fakeFolder.currentRemoteState().find("B/b1"));
        QVERIFY(!fakeFolder.currentRemoteState().find("B/hello.txt"));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void issue1329()
    {
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };

        fakeFolder.localModifier().remove("B");
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // Add a directory that was just removed in the previous sync:
        fakeFolder.localModifier().mkdir("B");
        fakeFolder.localModifier().insert("B/b1");
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(fakeFolder.currentRemoteState().find("B/b1"));
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void partiallyFailedRecursiveRemovalCleansJournal()
    {
#ifndef Q_OS_WIN
        QSKIP("Requires Windows file-sharing semantics");
#else
        FakeFolder fakeFolder{ FileInfo::A12_B12_C12_S12() };
        fakeFolder.remoteModifier().remove(QStringLiteral("A"));
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();

        const auto lockedFilePath = FileSystem::longWinPath(
            QDir::toNativeSeparators(fakeFolder.localPath() + QStringLiteral("A/a1")));
        const auto lockedFile = CreateFileW(reinterpret_cast<const wchar_t *>(lockedFilePath.utf16()),
            GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(lockedFile != INVALID_HANDLE_VALUE);

        const auto syncResult = fakeFolder.execUntilFinished();
        CloseHandle(lockedFile);

        QVERIFY(!syncResult);

        SyncJournalFileRecord lockedRecord;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a1"), &lockedRecord));
        QVERIFY(lockedRecord.isValid());

        SyncJournalFileRecord deletedRecord;
        QVERIFY(fakeFolder.syncJournal().getFileRecord(QStringLiteral("A/a2"), &deletedRecord));
        QVERIFY(!deletedRecord.isValid());
#endif
    }
};

QTEST_GUILESS_MAIN(TestSyncDelete)
#include "testsyncdelete.moc"
