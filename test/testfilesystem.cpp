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

#ifdef Q_OS_WIN
#include <securitybaseapi.h>
#include <aclapi.h>
#include <sddl.h>
#endif

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

#ifdef Q_OS_WIN
    void testAclWithManyDeniedAces()
    {
        // Regression from client versions < 4.0.2; see GH issue nextcloud/desktop#8860
        //
        // When a file/folder was set to read-only, the access denied ACE was always
        // added, eventually leading to failures when modifying the ACL.

        QTemporaryDir tempDir;
        QDir tempQDir{tempDir.path()};
        tempQDir.mkpath("readonlyTest");

        // Test case setup: retrieve the ACL and size info from the temporary folder using a win32 HANDLE
        // implementation adapted from `FileSystem::setAclPermission`
        const auto testPath = tempDir.filePath("readonlyTest");
        const auto testPathLong = FileSystem::longWinPath(testPath);
        const auto testPathRaw = reinterpret_cast<const wchar_t *>(testPathLong.utf16());

        Utility::UniqueHandle fileHandle;

        constexpr SECURITY_INFORMATION securityInfo = DACL_SECURITY_INFORMATION | READ_CONTROL | WRITE_DAC;

        PACL resultDacl = nullptr; // this is a part of the `securityDescriptor` and won't need to be free
        Utility::UniqueLocalFree<PSECURITY_DESCRIPTOR> securityDescriptor;
        Utility::UniqueLocalFree<PSID> sid;

        constexpr DWORD desiredAccess = READ_CONTROL | WRITE_DAC | MAXIMUM_ALLOWED;
        constexpr DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        constexpr DWORD creationDisposition = OPEN_EXISTING;
        constexpr DWORD flagsAndAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
        fileHandle.reset(CreateFileW(testPathRaw, desiredAccess, shareMode, nullptr, creationDisposition, flagsAndAttributes, nullptr));

        if (fileHandle.get() == INVALID_HANDLE_VALUE) {
            const auto lastError = GetLastError();
            qCWarning(lcFileSystem).nospace() << "CreateFileW failed, path=" << testPath << " errorMessage=" << Utility::formatWinError(lastError);
            QVERIFY(false);
        }

        {
            PSID sidUnmanaged = nullptr;
            if (!ConvertStringSidToSidW(L"S-1-5-32-545", &sidUnmanaged)) {
                const auto lastError = GetLastError();
                qCWarning(lcFileSystem).nospace() << "ConvertStringSidToSidW failed, path=" << testPath
                << " errorMessage=" << Utility::formatWinError(lastError);
                QVERIFY(false);
            }
            sid.reset(sidUnmanaged);
        }

        // Test helper to retrieve the most recent ACL data returning the ACL size.
        // This will modify resultDacl as side effect, which is fine for this specific test.
        const auto getAclSizeInformation = [&fileHandle, &resultDacl, &testPath, &securityDescriptor]() -> ACL_SIZE_INFORMATION {
            PSECURITY_DESCRIPTOR securityDescriptorUnmanaged = nullptr;
            if (const auto lastError = GetSecurityInfo(fileHandle.get(), SE_FILE_OBJECT, securityInfo, nullptr, nullptr, &resultDacl, nullptr, &securityDescriptorUnmanaged); lastError != ERROR_SUCCESS) {
                qCWarning(lcFileSystem).nospace() << "GetSecurityInfo failed, path=" << testPath << " errorMessage=" << Utility::formatWinError(lastError);
                Q_ASSERT(false);
            }
            securityDescriptor.reset(securityDescriptorUnmanaged);

            if (!resultDacl) {
                qCWarning(lcFileSystem).nospace() << "failed to retrieve DACL needed to set a folder read-only or read-write, path=" << testPath;
                Q_ASSERT(false);
            }

            ACL_SIZE_INFORMATION aclSize;
            if (!GetAclInformation(resultDacl, &aclSize, sizeof(aclSize), AclSizeInformation)) {
                const auto lastError = GetLastError();
                qCWarning(lcFileSystem).nospace() << "GetAclInformation failed, path=" << testPath << " errorMessage=" << Utility::formatWinError(lastError);
                Q_ASSERT(false);
            }

            qCDebug(lcFileSystem).nospace() << "retrieved ACL size information"
                << " aclSize.AceCount=" << aclSize.AceCount
                << " aclSize.AclBytesInUse=" << aclSize.AclBytesInUse
                << " aclSize.AclBytesFree=" << aclSize.AclBytesFree;
            return aclSize;
        };

        // ACLs can be up to 65532 bytes in size, so let's add as many ACEs as possible to reproduce the original issue
        const auto aclSizeInitial = getAclSizeInformation();
        const auto accessDeniedAceSize = (sizeof(ACCESS_DENIED_ACE) - sizeof(DWORD) + GetLengthSid(sid.get()));
        const auto accessDeniedAceTotalCount = (65532 - aclSizeInitial.AclBytesInUse) / accessDeniedAceSize;

        auto newAclSize = aclSizeInitial.AclBytesInUse + accessDeniedAceSize * accessDeniedAceTotalCount;
        std::unique_ptr<ACL> newDacl{reinterpret_cast<PACL>(new char[newAclSize])};
        int newAceIndex = 0;
        qCDebug(lcFileSystem) << "allocated a new DACL object of size" << newAclSize;

        if (!InitializeAcl(newDacl.get(), newAclSize, ACL_REVISION)) {
            const auto lastError = GetLastError();
            qCWarning(lcFileSystem).nospace() << "InitializeAcl failed,"
                << " path=" << testPath
                << " aclSizeInitial.AclBytesInUse=" << aclSizeInitial.AclBytesInUse
                << " newAclSize=" << newAclSize
                << " errorMessage=" << Utility::formatWinError(lastError);
            QVERIFY(false);
        }

        qCDebug(lcFileSystem) << "adding" << accessDeniedAceTotalCount << "access denied ACEs";
        for (auto i = 0; i < accessDeniedAceTotalCount; i++) {
            if (!AddAccessDeniedAceEx(newDacl.get(), ACL_REVISION, NO_PROPAGATE_INHERIT_ACE, FILE_DELETE_CHILD | DELETE | FILE_WRITE_DATA | FILE_WRITE_EA | FILE_APPEND_DATA, sid.get())) {
                const auto lastError = GetLastError();
                qCWarning(lcFileSystem).nospace() << "AddAccessDeniedAceEx failed, path=" << testPath << " errorMessage=" << Utility::formatWinError(lastError);
                QVERIFY(false);
            }
            newAceIndex++;
        }

        // Finally, append the ACEs from the original ACL
        for (int currentAceIndex = 0; currentAceIndex < aclSizeInitial.AceCount; ++currentAceIndex) {
            void *currentAce = nullptr;
            if (!GetAce(resultDacl, currentAceIndex, &currentAce)) {
                const auto lastError = GetLastError();
                qCWarning(lcFileSystem).nospace() << "GetAce failed, path=" << testPath << " errorMessage=" << Utility::formatWinError(lastError);
                QVERIFY(false);
            }

            const auto currentAceHeader = reinterpret_cast<PACE_HEADER>(currentAce);

            if (!AddAce(newDacl.get(), ACL_REVISION, newAceIndex, currentAce, currentAceHeader->AceSize)) {
                const auto lastError = GetLastError();
                qCWarning(lcFileSystem).nospace() << "AddAce failed,"
                    << " path=" << testPath
                    << " errorMessage=" << Utility::formatWinError(lastError)
                    << " newAclSize=" << newAclSize
                    << " newAceIndex=" << newAceIndex;
                QVERIFY(false);
            }
            newAceIndex++;
        }

        if (const auto lastError = SetSecurityInfo(fileHandle.get(), SE_FILE_OBJECT, PROTECTED_DACL_SECURITY_INFORMATION | securityInfo, nullptr, nullptr, newDacl.get(), nullptr); lastError != ERROR_SUCCESS) {
            qCWarning(lcFileSystem).nospace() << "SetSecurityInfo failed, path=" << testPath << " errorMessage=" << Utility::formatWinError(lastError);
            QVERIFY(false);
        }

        // Test setup done, so let's verify the behaviour.
        // ensure we have more ACEs than before and that the used bytes are close to the limit
        const auto aclSizeDuplicateACEs = getAclSizeInformation();
        QCOMPARE_GT(aclSizeDuplicateACEs.AceCount, aclSizeInitial.AceCount);
        QCOMPARE_GT(aclSizeDuplicateACEs.AclBytesInUse, 65511);

        // Try to mark the folder as readonly.  This used to fail because the ACL size was always extended,
        // even if it wouldn't have been necessary or even possible.
        QVERIFY(FileSystem::setAclPermission(testPath, FileSystem::FolderPermissions::ReadOnly));

        const auto aclSizeAfterReadOnly = getAclSizeInformation();
        QCOMPARE_GT(aclSizeAfterReadOnly.AceCount, aclSizeInitial.AceCount);
        QCOMPARE_LT(aclSizeAfterReadOnly.AceCount, aclSizeDuplicateACEs.AceCount);
        // The ACLs containing the required ACEs are rather small (usually around 100-200 bytes) -- but it's definitely way less than before:
        QCOMPARE_LT(aclSizeAfterReadOnly.AclBytesInUse, 512);

        // Finally, remove the read-only ACL :)
        // This should end up with the same ACEs as before the test.
        QVERIFY(FileSystem::setAclPermission(testPath, FileSystem::FolderPermissions::ReadWrite));
        const auto aclSizeAfterReadWrite = getAclSizeInformation();
        QCOMPARE_EQ(aclSizeAfterReadWrite.AceCount, aclSizeInitial.AceCount);
        QCOMPARE_EQ(aclSizeAfterReadWrite.AclBytesInUse, aclSizeInitial.AclBytesInUse);
    }
#endif
};

QTEST_GUILESS_MAIN(TestFileSystem)
#include "testfilesystem.moc"
