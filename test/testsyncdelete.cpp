/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include "syncenginetestutils.h"

#ifdef Q_OS_LINUX
#include "common/vfs.h"
#include <QScopeGuard>
#endif

#include <QtTest>

using namespace OCC;

#ifdef Q_OS_LINUX
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
#endif

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

#ifdef Q_OS_LINUX
    void testRemoteDeleteWithTrash_data()
    {
        QTest::addColumn<QStringList>("journalPaths");
        QTest::addColumn<bool>("trashAvailable");

        QTest::newRow("file-to-trash") << QStringList{u"B/b1"_s} << true;
        QTest::newRow("file-fallback") << QStringList{u"B/b1"_s} << false;
        QTest::newRow("directory-fallback") << QStringList{u"A"_s, u"A/a1"_s, u"A/a2"_s} << false;
    }

    void testRemoteDeleteWithTrash()
    {
        QFETCH(QStringList, journalPaths);
        QFETCH(bool, trashAvailable);

        auto fakeFolder = FakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.switchToVfs(QSharedPointer<MockCfApiVfs>::create());
        const auto &relativePath = journalPaths.constFirst();
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
        fakeFolder.syncEngine().setSyncOptions(syncOptions);

        auto dataHome = QTemporaryDir{};
        QVERIFY(dataHome.isValid());
        const auto previousDataHome = qgetenv("XDG_DATA_HOME");
        const auto previousTestMode = QStandardPaths::isTestModeEnabled();
        const auto restoreDataHome = qScopeGuard([previousDataHome, previousTestMode] {
            if (previousDataHome.isNull()) {
                qunsetenv("XDG_DATA_HOME");
            } else {
                qputenv("XDG_DATA_HOME", previousDataHome);
            }
            QStandardPaths::setTestModeEnabled(previousTestMode);
        });
        // Test mode makes QStandardPaths ignore XDG_DATA_HOME.
        QStandardPaths::setTestModeEnabled(false);
        QVERIFY(qputenv("XDG_DATA_HOME", QFile::encodeName(dataHome.path())));
        if (!trashAvailable) {
            // A regular file at Trash stops Qt from opening the trash directory.
            auto blocker = QFile{dataHome.filePath(u"Trash"_s)};
            QVERIFY(blocker.open(QIODevice::WriteOnly));
            QVERIFY(!QFile::moveToTrash(localPath));
            QVERIFY(QFileInfo::exists(localPath));
        }

        auto completed = ItemCompletedSpy{fakeFolder};
        fakeFolder.remoteModifier().remove(relativePath);
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(completed.findItem(relativePath)->_status, SyncFileItem::Success);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        for (const auto &path : journalPaths) {
            QVERIFY(journal.getFileRecord(path, &record));
            QVERIFY(!record.isValid());
        }

        const auto trashedPath = dataHome.filePath(u"Trash/files/"_s + QFileInfo{relativePath}.fileName());
        QCOMPARE(QFileInfo::exists(trashedPath), trashAvailable);
    }
#endif
};

QTEST_GUILESS_MAIN(TestSyncDelete)
#include "testsyncdelete.moc"
