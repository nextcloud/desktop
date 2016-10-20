/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <qglobal.h>
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
#include <QTemporaryDir>
#endif
#include <QtTest>

#include "utility.h"
#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include "configfile.h"

using namespace OCC;


static FolderDefinition folderDefinition(const QString &path) {
    FolderDefinition d;
    d.localPath = path;
    d.targetPath = path;
    d.alias = path;
    return d;
}


class TestFolderMan: public QObject
{
    Q_OBJECT

    FolderMan _fm;

private slots:
    void testCheckPathValidityForNewFolder()
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
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

        AccountStatePtr newAccountState(new AccountState(Account::create()));
        FolderMan *folderman = FolderMan::instance();
        QCOMPARE(folderman, &_fm);
        QVERIFY(folderman->addFolder(newAccountState.data(), folderDefinition(dir.path() + "/sub/ownCloud1")));
        QVERIFY(folderman->addFolder(newAccountState.data(), folderDefinition(dir.path() + "/ownCloud2")));


        // those should be allowed
        QCOMPARE(folderman->checkPathValidityForNewFolder(dir.path() + "/sub/free"), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dir.path() + "/free2/"), QString());
        // Not an existing directory -> Ok
        QCOMPARE(folderman->checkPathValidityForNewFolder(dir.path() + "/sub/bliblablu"), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dir.path() + "/sub/free/bliblablu"), QString());
        QCOMPARE(folderman->checkPathValidityForNewFolder(dir.path() + "/sub/bliblablu/some/more"), QString());

        // A file -> Error
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub/file.txt").isNull());

        // There are folders configured in those folders: -> ERROR
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub/ownCloud1").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/ownCloud2/").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub/").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path()).isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub/ownCloud1/folder").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub/ownCloud1/folder/f").isNull());


        // make a bunch of links
        QVERIFY(QFile::link(dir.path() + "/sub/free", dir.path() + "/link1"));
        QVERIFY(QFile::link(dir.path() + "/sub", dir.path() + "/link2"));
        QVERIFY(QFile::link(dir.path() + "/sub/ownCloud1", dir.path() + "/link3"));
        QVERIFY(QFile::link(dir.path() + "/sub/ownCloud1/folder", dir.path() + "/link4"));

        // Ok
        QVERIFY(folderman->checkPathValidityForNewFolder(dir.path() + "/link1").isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dir.path() + "/link2/free").isNull());

        // Not Ok
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/link2").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/link3").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/link4").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/link3/folder").isNull());


        // test some non existing sub path (error)
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub/ownCloud1/some/sub/path").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/ownCloud2/blublu").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/sub/ownCloud1/folder/g/h").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder(dir.path() + "/link3/folder/neu_folder").isNull());

        // Subfolder of links
        QVERIFY(folderman->checkPathValidityForNewFolder(dir.path() + "/link1/subfolder").isNull());
        QVERIFY(folderman->checkPathValidityForNewFolder(dir.path() + "/link2/free/subfolder").isNull());

        // Invalid paths
        QVERIFY(!folderman->checkPathValidityForNewFolder("").isNull());

        // Should not have the rights
        QVERIFY(!folderman->checkPathValidityForNewFolder("/").isNull());
        QVERIFY(!folderman->checkPathValidityForNewFolder("/usr/bin/somefolder").isNull());
#else
        QSKIP("Test not supported with Qt4", SkipSingle);
#endif
    }
};

QTEST_APPLESS_MAIN(TestFolderMan)
#include "testfolderman.moc"
