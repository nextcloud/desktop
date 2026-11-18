/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "failingtrashlocalremovejob.h"
#include "syncenginetestutils.h"

#include "common/vfs.h"

#include <QtTest>

using namespace OCC;
using namespace Qt::StringLiterals;

namespace
{

class MockCfApiVfs : public VfsOff
{
public:
    using VfsOff::VfsOff;
    [[nodiscard]] Mode mode() const override
    {
        return Vfs::WindowsCfApi;
    }
};

} // namespace

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

    void testMoveToTrashFailureFallback_data()
    {
        QTest::addColumn<QString>("relativePath");
        QTest::addColumn<bool>("isDirectory");
        QTest::addColumn<QStringList>("journalPaths");

        QTest::newRow("directory") << u"A"_s << true << QStringList{u"A"_s, u"A/a1"_s, u"A/a2"_s};
        QTest::newRow("file") << u"B/b1"_s << false << QStringList{u"B/b1"_s};
    }

    void testMoveToTrashFailureFallback()
    {
        QFETCH(QString, relativePath);
        QFETCH(bool, isDirectory);
        QFETCH(QStringList, journalPaths);

        auto fakeFolder = FakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.switchToVfs(QSharedPointer<MockCfApiVfs>::create());
        const auto localPath = fakeFolder.localPath() + relativePath;
        QVERIFY(QFileInfo::exists(localPath));

        auto &journal = fakeFolder.syncJournal();
        auto record = SyncJournalFileRecord{};
        for (const auto &path : journalPaths) {
            QVERIFY(journal.getFileRecord(path, &record));
            QVERIFY(record.isValid());
        }

        auto syncOptions = fakeFolder.syncEngine().syncOptions();
        syncOptions._moveFilesToTrash = true;

        auto bulkUploadBlackList = QSet<QString>{};
        auto propagator = OwncloudPropagator(fakeFolder.account(), fakeFolder.localPath(), u"/"_s, &journal, bulkUploadBlackList);
        propagator.setSyncOptions(syncOptions);

        auto item = SyncFileItemPtr::create();
        item->_file = relativePath;
        item->_originalFile = relativePath;
        item->_type = isDirectory ? ItemTypeDirectory : ItemTypeFile;
        item->_instruction = CSYNC_INSTRUCTION_REMOVE;
        item->_direction = SyncFileItem::Down;

        auto job = FailingTrashLocalRemoveJob(&propagator, item);
        auto finishedSpy = QSignalSpy(&job, &PropagatorJob::finished);
        job.start();

        QCOMPARE(job._trashAttempts, QStringList{localPath});
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.at(0).at(0).value<SyncFileItem::Status>(), SyncFileItem::Success);
        QVERIFY(!QFileInfo::exists(localPath));

        for (const auto &path : journalPaths) {
            QVERIFY(journal.getFileRecord(path, &record));
            QVERIFY(!record.isValid());
        }
    }
};

QTEST_GUILESS_MAIN(TestSyncDelete)
#include "testsyncdelete.moc"
