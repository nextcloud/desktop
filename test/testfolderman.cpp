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
#include "testhelper.h"

using namespace OCC;

class TestFolderMan: public QObject
{
    Q_OBJECT

    FolderMan _fm;

private slots:
    void testCheckPathValidityForNewFolder()
    {
#ifdef Q_OS_WIN
        Utility::NtfsPermissionLookupRAII ntfs_perm;
#endif
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file
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

        AccountPtr account = Account::create();
        QUrl url("http://example.de");
        auto *cred = new HttpCredentialsTest("testuser", "secret");
        account->setCredentials(cred);
        account->setUrl( url );

        AccountStatePtr newAccountState(new AccountState(account));
        FolderMan *folderman = FolderMan::instance();
        QCOMPARE(folderman, &_fm);
        QVERIFY(folderman->addFolder(newAccountState.data(), folderDefinition(dirPath + "/sub/ownCloud1")));
        QVERIFY(folderman->addFolder(newAccountState.data(), folderDefinition(dirPath + "/ownCloud2")));


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

        // There are folders configured in those folders, url needs to be taken into account: -> ERROR
        QUrl url2(url);
        const QString user = account->credentials()->user();
        url2.setUserName(user);

        // The following both fail because they refer to the same account (user and url)
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1", url2).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/", url2).isNull());

        // Now it will work because the account is different
        QUrl url3("http://anotherexample.org");
        url3.setUserName("dummy");
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1", url3), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/", url3), QString());

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
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link3", url2).isNull());
        // while with a different account, this is fine
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/link3", url3), QString());

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

        // Should not have the rights
        QVERIFY(!folderman->checkPathValidityForNewFolder("/").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder("/usr/bin/somefolder").isNull());
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
    }

    void testFindGoodPathForNewSyncFolder()
    {
        // SETUP

        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file
        QVERIFY(dir.isValid());
        QDir dir2(dir.path());
        QVERIFY(dir2.mkpath("sub/ownCloud1/folder/f"));
        QVERIFY(dir2.mkpath("ownCloud"));
        QVERIFY(dir2.mkpath("ownCloud2"));
        QVERIFY(dir2.mkpath("ownCloud2/foo"));
        QVERIFY(dir2.mkpath("sub/free"));
        QVERIFY(dir2.mkpath("free2/sub"));
        QString dirPath = dir2.canonicalPath();

        AccountPtr account = Account::create();
        QUrl url("http://example.de");
        auto *cred = new HttpCredentialsTest("testuser", "secret");
        account->setCredentials(cred);
        account->setUrl( url );
        url.setUserName(cred->user());

        AccountStatePtr newAccountState(new AccountState(account));
        FolderMan *folderman = FolderMan::instance();
        QCOMPARE(folderman, &_fm);
        QVERIFY(folderman->addFolder(newAccountState.data(), folderDefinition(dirPath + "/sub/ownCloud/")));
        QVERIFY(folderman->addFolder(newAccountState.data(), folderDefinition(dirPath + "/ownCloud2/")));

        // TEST

        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/oc", url),
                 QString(dirPath + "/oc"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud", url),
                 QString(dirPath + "/ownCloud3"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2", url),
                 QString(dirPath + "/ownCloud22"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2/foo", url),
                 QString(dirPath + "/ownCloud2/foo"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2/bar", url),
                 QString(dirPath + "/ownCloud2/bar"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/sub", url),
                 QString(dirPath + "/sub2"));

        // REMOVE ownCloud2 from the filesystem, but keep a folder sync'ed to it.
        // We should still not suggest this folder as a new folder.
        QDir(dirPath + "/ownCloud2/").removeRecursively();
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud", url),
            QString(dirPath + "/ownCloud3"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2", url),
            QString(dirPath + "/ownCloud22"));
    }
};

QTEST_APPLESS_MAIN(TestFolderMan)
#include "testfolderman.moc"
