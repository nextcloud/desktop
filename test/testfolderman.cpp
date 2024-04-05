/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <qglobal.h>
#include <QTemporaryDir>
#include <QtTest>

#include "common/utility.h"
#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include "configfile.h"

#include "testutils/testutils.h"

#ifndef Q_OS_WIN
#include <unistd.h>
#else
#include "common/utility_win.h"
#endif

using namespace OCC;


class TestFolderMan: public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testCheckPathValidityForNewFolder()
    {
#ifdef Q_OS_WIN
        Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
        auto dir = TestUtils::createTempDir();
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath(QStringLiteral("sub/ownCloud1/folder/f")));
        QVERIFY(dir2.mkpath(QStringLiteral("ownCloud2")));
        QVERIFY(dir2.mkpath(QStringLiteral("sub/free")));
        QVERIFY(dir2.mkpath(QStringLiteral("free2/sub")));
        {
            QFile f(dir.path() + QStringLiteral("/sub/file.txt"));
            f.open(QFile::WriteOnly);
            f.write("hello");
        }
        QString dirPath = dir2.canonicalPath();

        auto newAccountState = TestUtils::createDummyAccount();
        FolderMan *folderman = TestUtils::folderMan();
        QCOMPARE(folderman, FolderMan::instance());
        QVERIFY(folderman->addFolder(
            newAccountState.get(), TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + QStringLiteral("/sub/ownCloud1"))));
        QVERIFY(folderman->addFolder(
            newAccountState.get(), TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + QStringLiteral("/ownCloud2"))));

        const auto type = FolderMan::NewFolderType::OC10SyncRoot;
        const QUuid uuid = {};

        // those should be allowed
        // QString FolderMan::checkPathValidityForNewFolder(const QString& path, const QUrl &serverUrl, bool forNewDirectory)

        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/free"), type, uuid), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/free2/"), type, uuid), QString());
        // Not an existing directory -> Ok
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/bliblablu"), type, uuid), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/free/bliblablu"), type, uuid), QString());
        // QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/bliblablu/some/more")), QString());

        // A file -> Error
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/file.txt"), type, uuid).isNull());

        // The following both fail because they refer to the same account
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/ownCloud1"), type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/ownCloud2/"), type, uuid).isNull());

        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath, type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/ownCloud1/folder"), type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/ownCloud1/folder/f"), type, uuid).isNull());

#ifndef Q_OS_WIN // no links on windows, no permissions
        // make a bunch of links
        QVERIFY(QFile::link(dirPath + QStringLiteral("/sub/free"), dirPath + QStringLiteral("/link1")));
        QVERIFY(QFile::link(dirPath + QStringLiteral("/sub"), dirPath + QStringLiteral("/link2")));
        QVERIFY(QFile::link(dirPath + QStringLiteral("/sub/ownCloud1"), dirPath + QStringLiteral("/link3")));
        QVERIFY(QFile::link(dirPath + QStringLiteral("/sub/ownCloud1/folder"), dirPath + QStringLiteral("/link4")));

        // Ok
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link1"), type, uuid).isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link2/free"), type, uuid).isNull());

        // Not Ok
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link2"), type, uuid).isNull());

        // link 3 points to an existing sync folder. To make it fail, the account must be the same
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link3"), type, uuid).isNull());

        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link4"), type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link3/folder"), type, uuid).isNull());

        // test some non existing sub path (error)
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/ownCloud1/some/sub/path"), type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/ownCloud2/blublu"), type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/sub/ownCloud1/folder/g/h"), type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link3/folder/neu_folder"), type, uuid).isNull());

        // Subfolder of links
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link1/subfolder"), type, uuid).isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/link2/free/subfolder"), type, uuid).isNull());

        if (getuid() != 0) {
            // Should not have the rights
            QVERIFY(!folderman->checkPathValidityForNewFolder(QStringLiteral("/"), type, uuid).isNull());
            QVERIFY(!folderman->checkPathValidityForNewFolder(QStringLiteral("/usr/bin/somefolder"), type, uuid).isNull());
        }
#endif

        if (Utility::isWindows()) { // drive-letter tests
            if (!QFileInfo(QStringLiteral("v:/")).exists()) {
                QVERIFY(!folderman->checkPathValidityForNewFolder(QStringLiteral("v:"), type, uuid).isNull());
                QVERIFY(!folderman->checkPathValidityForNewFolder(QStringLiteral("v:/"), type, uuid).isNull());
                QVERIFY(!folderman->checkPathValidityForNewFolder(QStringLiteral("v:/foo"), type, uuid).isNull());
            }
            if (QFileInfo(QStringLiteral("c:/")).isWritable()) {
                QVERIFY(folderman->checkPathValidityForNewFolder(QStringLiteral("c:"), type, uuid).isNull());
                QVERIFY(folderman->checkPathValidityForNewFolder(QStringLiteral("c:/"), type, uuid).isNull());
                QVERIFY(folderman->checkPathValidityForNewFolder(QStringLiteral("c:/foo"), type, uuid).isNull());
            }
        }

        // Invalid paths
        QVERIFY(!folderman->checkPathValidityForNewFolder({}, type, uuid).isNull());


        // REMOVE ownCloud2 from the filesystem, but keep a folder sync'ed to it.
        QDir(dirPath + QStringLiteral("/ownCloud2/")).removeRecursively();
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/ownCloud2/blublu"), type, uuid).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/ownCloud2/sub/subsub/sub"), type, uuid).isNull());

        { // check for rejection of a directory with `.sync_*.db`
            QVERIFY(dir2.mkpath(QStringLiteral("db-check1")));
            QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/db-check1"), type, uuid).isNull());
            QFile f(dirPath + QStringLiteral("/db-check1/.sync_something.db"));
            QVERIFY(f.open(QFile::Truncate | QFile::WriteOnly));
            f.close();
            QVERIFY(QFileInfo::exists(dirPath + QStringLiteral("/db-check1/.sync_something.db")));
            QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/db-check1"), type, uuid).isNull());
        }

        { // check for rejection of a directory with `._sync_*.db`
            QVERIFY(dir2.mkpath(QStringLiteral("db-check2")));
            QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/db-check2"), type, uuid).isNull());
            QFile f(dirPath + QStringLiteral("/db-check2/._sync_something.db"));
            QVERIFY(f.open(QFile::Truncate | QFile::WriteOnly));
            f.close();
            QVERIFY(QFileInfo::exists(dirPath + QStringLiteral("/db-check2/._sync_something.db")));
            QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/db-check2"), type, uuid).isNull());
        }
    }

    void testFindGoodPathForNewSyncFolder()
    {
        // SETUP

        auto dir = TestUtils::createTempDir();
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath(QStringLiteral("sub/ownCloud1/folder/f")));
        QVERIFY(dir2.mkpath(QStringLiteral("ownCloud")));
        QVERIFY(dir2.mkpath(QStringLiteral("ownCloud2")));
        QVERIFY(dir2.mkpath(QStringLiteral("ownCloud2/foo")));
        QVERIFY(dir2.mkpath(QStringLiteral("sub/free")));
        QVERIFY(dir2.mkpath(QStringLiteral("free2/sub")));
        QString dirPath = dir2.canonicalPath();

        auto newAccountState = TestUtils::createDummyAccount();

        FolderMan *folderman = TestUtils::folderMan();
        QVERIFY(folderman->addFolder(
            newAccountState.get(), TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + QStringLiteral("/sub/ownCloud/"))));
        QVERIFY(folderman->addFolder(
            newAccountState.get(), TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + QStringLiteral("/ownCloud (2)/"))));

        // TEST
        const auto folderType = FolderMan::NewFolderType::OC10SyncRoot;

        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("oc"), folderType), dirPath + QStringLiteral("/oc"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud"), folderType), dirPath + QStringLiteral("/ownCloud (3)"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud2"), folderType), dirPath + QStringLiteral("/ownCloud2 (2)"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud (2)"), folderType), dirPath + QStringLiteral("/ownCloud (2) (2)"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud2/foo"), folderType), dirPath + QStringLiteral("/ownCloud2_foo"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud2/bar"), folderType), dirPath + QStringLiteral("/ownCloud2_bar"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("sub"), folderType), dirPath + QStringLiteral("/sub (2)"));

        // REMOVE ownCloud2 from the filesystem, but keep a folder sync'ed to it.
        // We should still not suggest this folder as a new folder.
        QDir(dirPath + QStringLiteral("/ownCloud (2)/")).removeRecursively();
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud"), folderType), dirPath + QStringLiteral("/ownCloud (3)"));
        QCOMPARE(
            folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud2"), folderType), QString(dirPath + QStringLiteral("/ownCloud2 (2)")));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("ownCloud (2)"), folderType),
            QString(dirPath + QStringLiteral("/ownCloud (2) (2)")));

        // make sure people can't do evil stuff
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("../../../Bo/b"), folderType), QString(dirPath + QStringLiteral("/___Bo_b")));

        // normalise the name
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, QStringLiteral("            Bo:*<>!b          "), folderType),
            QString(dirPath + QStringLiteral("/Bo____!b")));
    }

    void testSpacesSyncRootAndFolderCreation()
    {
        auto dir = TestUtils::createTempDir();
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());

        // Create a sync root for another account
        QVERIFY(dir2.mkpath(QStringLiteral("AnotherSpacesSyncRoot")));
        const auto anotherUuid = QUuid::createUuid();
        Utility::markDirectoryAsSyncRoot(dir2.filePath(QStringLiteral("AnotherSpacesSyncRoot")), anotherUuid);

        FolderMan *folderman = TestUtils::folderMan();
        QCOMPARE(folderman, FolderMan::instance());

        QString dirPath = dir2.canonicalPath();
        const auto ourUuid = QUuid::createUuid();

        // Spaces Sync Root in another Spaces Sync Root should fail
        QVERIFY(!folderman
                     ->checkPathValidityForNewFolder(
                         dirPath + QStringLiteral("/AnotherSpacesSyncRoot/OurSpacesSyncRoot"), FolderMan::NewFolderType::SpacesSyncRoot, ourUuid)
                     .isNull());
        // Spaces Sync Root one level up should be fine
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + QStringLiteral("/OurSpacesSyncRoot"), FolderMan::NewFolderType::SpacesSyncRoot, ourUuid)
                    .isNull());

        // Create the sync root so we can test Spaces Folder creation below
        QVERIFY(dir2.mkpath(QStringLiteral("OurSpacesSyncRoot")));
        Utility::markDirectoryAsSyncRoot(dir2.filePath(QStringLiteral("OurSpacesSyncRoot")), ourUuid);

        // A folder for a Space in a sync root for another account should fail
        QVERIFY(!folderman
                     ->checkPathValidityForNewFolder(
                         dirPath + QStringLiteral("/AnotherSpacesSyncRoot/OurSpacesFolder"), FolderMan::NewFolderType::SpacesFolder, ourUuid)
                     .isNull());
        // But in our sync root that should just be fine
        QVERIFY(
            folderman
                ->checkPathValidityForNewFolder(dirPath + QStringLiteral("/OurSpacesSyncRoot/OurSpacesFolder"), FolderMan::NewFolderType::SpacesFolder, ourUuid)
                .isNull());
    }
};

QTEST_GUILESS_MAIN(TestFolderMan)
#include "testfolderman.moc"
