/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "common/filesystembase.h"
#include "logger.h"

#include "libsync/filesystem.h"

using namespace OCC;
using namespace Qt::StringLiterals;
namespace std_fs = std::filesystem;

class TestFileSystem : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir testDir;

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        QDir dir(testDir.path());
        dir.mkdir("existingDirectory");
    }

#ifndef Q_OS_WIN
    void testSetFolderPermissionsExistingDirectory_data()
    {
        constexpr auto perms_0555 =
            std_fs::perms::owner_read | std_fs::perms::owner_exec
            | std_fs::perms::group_read | std_fs::perms::group_exec
            | std_fs::perms::others_read | std_fs::perms::others_exec;
        constexpr auto perms_0755 = perms_0555 | std_fs::perms::owner_write;
        constexpr auto perms_0775 = perms_0755 | std_fs::perms::group_write;

        QTest::addColumn<std_fs::perms>("originalPermissions");
        QTest::addColumn<FileSystem::FolderPermissions>("folderPermissions");
        QTest::addColumn<bool>("expectedResult");
        QTest::addColumn<bool>("expectedPermissionsChanged");
        QTest::addColumn<std_fs::perms>("expectedPermissions");

        QTest::newRow("0777, readonly -> 0555, changed")
            << std_fs::perms::all
            << FileSystem::FolderPermissions::ReadOnly
            << true
            << true
            << perms_0555;

        QTest::newRow("0555, readonly -> 0555, not changed")
            << perms_0555
            << FileSystem::FolderPermissions::ReadOnly
            << true
            << false
            << perms_0555;

        QTest::newRow("0777, readwrite -> 0775, changed")
            << std_fs::perms::all
            << FileSystem::FolderPermissions::ReadWrite
            << true
            << true
            << perms_0775;

        QTest::newRow("0775, readwrite -> 0775, not changed")
            << perms_0775
            << FileSystem::FolderPermissions::ReadWrite
            << true
            << false
            << perms_0775;

        QTest::newRow("0755, readwrite -> 0755, not changed")
            << perms_0755
            << FileSystem::FolderPermissions::ReadWrite
            << true
            << false
            << perms_0755;

        QTest::newRow("0555, readwrite -> 0755, changed")
            << perms_0555
            << FileSystem::FolderPermissions::ReadWrite
            << true
            << true
            << perms_0755;
    }

    void testSetFolderPermissionsExistingDirectory()
    {
        QFETCH(std_fs::perms, originalPermissions);
        QFETCH(FileSystem::FolderPermissions, folderPermissions);
        QFETCH(bool, expectedResult);
        QFETCH(bool, expectedPermissionsChanged);
        QFETCH(std_fs::perms, expectedPermissions);

        bool permissionsDidChange = false;
        QString fullPath = testDir.filePath("existingDirectory");
        const auto stdStrPath = fullPath.toStdWString();

        std_fs::permissions(stdStrPath, originalPermissions);

        QCOMPARE(FileSystem::setFolderPermissions(fullPath, folderPermissions, &permissionsDidChange), expectedResult);

        const auto newPermissions = std_fs::status(stdStrPath).permissions();
        QCOMPARE(newPermissions, expectedPermissions);
        QCOMPARE(permissionsDidChange, expectedPermissionsChanged);
    }

    void testSetFolderPermissionsNonexistentDirectory()
    {
        bool permissionsDidChange = false;

        QString fullPath = testDir.filePath("nonexistentDirectory");

        QCOMPARE(FileSystem::setFolderPermissions("nonexistentDirectory", FileSystem::FolderPermissions::ReadOnly, &permissionsDidChange), false);
        QCOMPARE(permissionsDidChange, false);
    }
#endif

    void testSetFileReadOnlyLongPath()
    {
        // this should be enough to hit Windows' default MAX_PATH limitation (260 chars)
        QTemporaryFile longFile(u"test"_s.repeated(60));

        // QTemporaryFile::open actually creates the file
        QVERIFY(longFile.open());
        QVERIFY(!longFile.fileName().isEmpty());

        QFileInfo fileInfo(longFile);
        const auto path = fileInfo.absoluteFilePath();

        // ensure we start with a read/writeable directory
        QVERIFY(FileSystem::isWritable(path));

        FileSystem::setFileReadOnly(path, true);
        // path should now be readonly
        QVERIFY(!FileSystem::isWritable(path));

        FileSystem::setFileReadOnly(path, false);
        // path should now be writable aagain
        QVERIFY(FileSystem::isWritable(path));
    }
};

QTEST_GUILESS_MAIN(TestFileSystem)
#include "testfilesystem.moc"
