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

    void testDeleteDirectoryKeepsNestedIgnoredFile()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().excludedFiles().addManualExclude("*.ign");
        fakeFolder.remoteModifier().mkdir("A/sub");
        fakeFolder.remoteModifier().mkdir("A/sub/deeper");
        fakeFolder.remoteModifier().insert("A/sub/deeper/synced.txt");
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.localModifier().insert("A/sub/deeper/local.ign");
        fakeFolder.remoteModifier().remove("A");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!fakeFolder.currentRemoteState().find("A"));
        QVERIFY(QFileInfo::exists(fakeFolder.localPath() + "A/sub/deeper/local.ign"));
        QVERIFY(!QFileInfo::exists(fakeFolder.localPath() + "A/a1"));
        QVERIFY(!QFileInfo::exists(fakeFolder.localPath() + "A/sub/deeper/synced.txt"));

        // The kept folders stay stable on the next sync
        QVERIFY(fakeFolder.syncOnce());
        QVERIFY(QFileInfo::exists(fakeFolder.localPath() + "A/sub/deeper/local.ign"));
    }

    void testDeleteSubdirectoryKeepsIgnoredFileAndSiblings()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().excludedFiles().addManualExclude("*.ign");
        fakeFolder.remoteModifier().mkdir("A/sub");
        fakeFolder.remoteModifier().insert("A/sub/synced.txt");
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.localModifier().insert("A/sub/local.ign");
        fakeFolder.remoteModifier().remove("A/sub");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(QFileInfo::exists(fakeFolder.localPath() + "A/sub/local.ign"));
        QVERIFY(QFileInfo::exists(fakeFolder.localPath() + "A/a1"));
        QVERIFY(!QFileInfo::exists(fakeFolder.localPath() + "A/sub/synced.txt"));
    }

    void testDeleteNestedDirectoryWithoutIgnoredFile()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().excludedFiles().addManualExclude("*.ign");
        fakeFolder.remoteModifier().mkdir("A/sub");
        fakeFolder.remoteModifier().insert("A/sub/synced.txt");
        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.remoteModifier().remove("A");
        QVERIFY(fakeFolder.syncOnce());

        QVERIFY(!QFileInfo::exists(fakeFolder.localPath() + "A"));
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
};

QTEST_GUILESS_MAIN(TestSyncDelete)
#include "testsyncdelete.moc"
