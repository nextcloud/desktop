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
#endif

using namespace OCC;


class TestFolderMan: public QObject
{
    Q_OBJECT
private slots:
    void testCheckPathValidityForNewFolder()
    {
#ifdef Q_OS_WIN
        Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
        auto dir = TestUtils::createTempDir();
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath("sub/ownCloud1/folder/f"));
        QVERIFY(dir2.mkpath("ownCloud2"));
        QVERIFY(dir2.mkpath("sub/free"));
        QVERIFY(dir2.mkpath("free2/sub"));
        {
            QFile f(dir.path() + "/sub/file.txt");
            f.open(QFile::WriteOnly);
            f.write("hello");
        }
        QString dirPath = dir2.canonicalPath();

        AccountPtr account = TestUtils::createDummyAccount();
        AccountStatePtr newAccountState = AccountState::fromNewAccount(account);
        FolderMan *folderman = TestUtils::folderMan();
        QCOMPARE(folderman, FolderMan::instance());
        QVERIFY(folderman->addFolder(newAccountState, TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + "/sub/ownCloud1")));
        QVERIFY(folderman->addFolder(newAccountState, TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + "/ownCloud2")));


        // those should be allowed
        // QString FolderMan::checkPathValidityForNewFolder(const QString& path, const QUrl &serverUrl, bool forNewDirectory)

        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/free"), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/free2/"), QString());
        // Not an existing directory -> Ok
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/bliblablu"), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/free/bliblablu"), QString());
        // QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/bliblablu/some/more"), QString());

        // A file -> Error
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/file.txt").isNull());

        // The following both fail because they refer to the same account
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/").isNull());

        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/folder").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/folder/f").isNull());

#ifndef Q_OS_WIN // no links on windows, no permissions
        // make a bunch of links
        QVERIFY(QFile::link(dirPath + "/sub/free", dirPath + "/link1"));
        QVERIFY(QFile::link(dirPath + "/sub", dirPath + "/link2"));
        QVERIFY(QFile::link(dirPath + "/sub/ownCloud1", dirPath + "/link3"));
        QVERIFY(QFile::link(dirPath + "/sub/ownCloud1/folder", dirPath + "/link4"));

        // Ok
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link1").isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link2/free").isNull());

        // Not Ok
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link2").isNull());

        // link 3 points to an existing sync folder. To make it fail, the account must be the same
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link3").isNull());

        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link4").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link3/folder").isNull());

        // test some non existing sub path (error)
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/some/sub/path").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/blublu").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/folder/g/h").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link3/folder/neu_folder").isNull());

        // Subfolder of links
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link1/subfolder").isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link2/free/subfolder").isNull());

        if (getuid() != 0) {
            // Should not have the rights
            QVERIFY(!folderman->checkPathValidityForNewFolder("/").isNull());
            QVERIFY(!folderman->checkPathValidityForNewFolder("/usr/bin/somefolder").isNull());
        }
#endif

#ifdef Q_OS_WIN // drive-letter tests
        if (!QFileInfo("v:/").exists()) {
            QVERIFY(!folderman->checkPathValidityForNewFolder("v:").isNull());
            QVERIFY(!folderman->checkPathValidityForNewFolder("v:/").isNull());
            QVERIFY(!folderman->checkPathValidityForNewFolder("v:/foo").isNull());
        }
        if (QFileInfo("c:/").isWritable()) {
            QVERIFY(folderman->checkPathValidityForNewFolder("c:").isNull());
            QVERIFY(folderman->checkPathValidityForNewFolder("c:/").isNull());
            QVERIFY(folderman->checkPathValidityForNewFolder("c:/foo").isNull());
        }
#endif

        // Invalid paths
        QVERIFY(!folderman->checkPathValidityForNewFolder("").isNull());


        // REMOVE ownCloud2 from the filesystem, but keep a folder sync'ed to it.
        QDir(dirPath + "/ownCloud2/").removeRecursively();
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/blublu").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/sub/subsub/sub").isNull());

        { // check for rejection of a directory with `.sync_*.db`
            QVERIFY(dir2.mkpath("db-check1"));
            QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/db-check1").isNull());
            QFile f(dirPath + "/db-check1/.sync_something.db");
            QVERIFY(f.open(QFile::Truncate | QFile::WriteOnly));
            f.close();
            QVERIFY(QFileInfo::exists(dirPath + "/db-check1/.sync_something.db"));
            QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/db-check1").isNull());
        }

        { // check for rejection of a directory with `._sync_*.db`
            QVERIFY(dir2.mkpath("db-check2"));
            QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/db-check2").isNull());
            QFile f(dirPath + "/db-check2/._sync_something.db");
            QVERIFY(f.open(QFile::Truncate | QFile::WriteOnly));
            f.close();
            QVERIFY(QFileInfo::exists(dirPath + "/db-check2/._sync_something.db"));
            QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/db-check2").isNull());
        }
    }

    void testFindGoodPathForNewSyncFolder()
    {
        // SETUP

        auto dir = TestUtils::createTempDir();
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath("sub/ownCloud1/folder/f"));
        QVERIFY(dir2.mkpath("ownCloud"));
        QVERIFY(dir2.mkpath("ownCloud2"));
        QVERIFY(dir2.mkpath("ownCloud2/foo"));
        QVERIFY(dir2.mkpath("sub/free"));
        QVERIFY(dir2.mkpath("free2/sub"));
        QString dirPath = dir2.canonicalPath();

        AccountPtr account = TestUtils::createDummyAccount();

        AccountStatePtr newAccountState = AccountState::fromNewAccount(account);
        FolderMan *folderman = TestUtils::folderMan();
        QVERIFY(folderman->addFolder(newAccountState, TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + "/sub/ownCloud/")));
        QVERIFY(folderman->addFolder(newAccountState, TestUtils::createDummyFolderDefinition(newAccountState->account(), dirPath + "/ownCloud2/")));

        // TEST

        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "oc"), QString(dirPath + "/oc"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "ownCloud"), QString(dirPath + "/ownCloud3"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "ownCloud2"), QString(dirPath + "/ownCloud22"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "ownCloud2/foo"), QString(dirPath + "/ownCloud2_foo"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "ownCloud2/bar"), QString(dirPath + "/ownCloud2_bar"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "sub"), QString(dirPath + "/sub2"));

        // REMOVE ownCloud2 from the filesystem, but keep a folder sync'ed to it.
        // We should still not suggest this folder as a new folder.
        QDir(dirPath + "/ownCloud2/").removeRecursively();
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "ownCloud"), QString(dirPath + "/ownCloud3"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "ownCloud2"), QString(dirPath + "/ownCloud22"));

        // make sure people can't do evil stuff
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "../../../Bo/b"), QString(dirPath + "/___Bo_b"));

        // normalise the name
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath, "            Bo:*<>!b          "), QString(dirPath + "/Bo____!b"));
    }
};

QTEST_GUILESS_MAIN(TestFolderMan)
#include "testfolderman.moc"
