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

        const auto folderList = folderman->map();

        for (const auto &folder : folderList) {
            QVERIFY(!folder->isSyncRunning());
        }


        // those should be allowed
        // QString FolderMan::checkPathValidityForNewFolder(const QString& path, const QUrl &serverUrl, bool forNewDirectory).second

        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/free").second, QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/free2/").second, QString());
        // Not an existing directory -> Ok
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/bliblablu").second, QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/free/bliblablu").second, QString());
        // QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/bliblablu/some/more").second, QString());

        // A file -> Error
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/file.txt").second.isNull());

        // There are folders configured in those folders, url needs to be taken into account: -> ERROR
        QUrl url2(url);
        const QString user = account->credentials()->user();
        url2.setUserName(user);

        // The following both fail because they refer to the same account (user and url)
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1", url2).second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/", url2).second.isNull());

        // Now it will work because the account is different
        QUrl url3("http://anotherexample.org");
        url3.setUserName("dummy");
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1", url3).second, QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/", url3).second, QString());

        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath).second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/folder").second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/folder/f").second.isNull());

#ifndef Q_OS_WIN // no links on windows, no permissions
        // make a bunch of links
        QVERIFY(QFile::link(dirPath + "/sub/free", dirPath + "/link1"));
        QVERIFY(QFile::link(dirPath + "/sub", dirPath + "/link2"));
        QVERIFY(QFile::link(dirPath + "/sub/ownCloud1", dirPath + "/link3"));
        QVERIFY(QFile::link(dirPath + "/sub/ownCloud1/folder", dirPath + "/link4"));

        // Ok
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link1").second.isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link2/free").second.isNull());

        // Not Ok
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link2").second.isNull());

        // link 3 points to an existing sync folder. To make it fail, the account must be the same
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link3", url2).second.isNull());
        // while with a different account, this is fine
        QCOMPARE(folderman->checkPathValidityForNewFolder(dirPath + "/link3", url3).second, QString());

        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link4").second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link3/folder").second.isNull());

        // test some non existing sub path (error)
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/some/sub/path").second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/blublu").second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/sub/ownCloud1/folder/g/h").second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/link3/folder/neu_folder").second.isNull());

        // Subfolder of links
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link1/subfolder").second.isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dirPath + "/link2/free/subfolder").second.isNull());

        // Should not have the rights
        QVERIFY(!folderman->checkPathValidityForNewFolder("/").second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder("/usr/bin/somefolder").second.isNull());
#endif

#ifdef Q_OS_WIN // drive-letter tests
        if (!QFileInfo("v:/").exists()) {
            QVERIFY(!folderman->checkPathValidityForNewFolder("v:").second.isNull());
            QVERIFY(!folderman->checkPathValidityForNewFolder("v:/").second.isNull());
            QVERIFY(!folderman->checkPathValidityForNewFolder("v:/foo").second.isNull());
        }
        if (QFileInfo("c:/").isWritable()) {
            QVERIFY(folderman->checkPathValidityForNewFolder("c:").second.isNull());
            QVERIFY(folderman->checkPathValidityForNewFolder("c:/").second.isNull());
            QVERIFY(folderman->checkPathValidityForNewFolder("c:/foo").second.isNull());
        }
#endif

        // Invalid paths
        QVERIFY(!folderman->checkPathValidityForNewFolder("").second.isNull());


        // REMOVE ownCloud2 from the filesystem, but keep a folder sync'ed to it.
        QDir(dirPath + "/ownCloud2/").removeRecursively();
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/blublu").second.isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dirPath + "/ownCloud2/sub/subsub/sub").second.isNull());
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
