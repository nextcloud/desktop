/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <qglobal.h>
#include <QTemporaryDir>
#include <QtTest>

#include "QtTest/qtestcase.h"
#include "common/utility.h"
#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include <accountmanager.h>
#include "configfile.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

using namespace OCC;

static QByteArray fake400Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}}
)";

bool itemDidCompleteSuccessfully(const ItemCompletedSpy &spy, const QString &path)
{
    if (auto item = spy.findItem(path)) {
        return item->_status == SyncFileItem::Success;
    }
    return false;
}

class TestFolderMan: public QObject
{
    Q_OBJECT

    FolderMan _fm;

signals:
    void incomingShareDeleted();

private slots:
    void testDeleteEncryptedFiles()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QCOMPARE(fakeFolder.currentLocalState().children.count(), 4);

        ItemCompletedSpy completeSpy(fakeFolder);
        fakeFolder.localModifier().mkdir("encrypted");
        fakeFolder.localModifier().setE2EE("encrypted", true);
        fakeFolder.remoteModifier().mkdir("encrypted");
        fakeFolder.remoteModifier().setE2EE("encrypted", true);

        const auto fakeFileInfo = fakeFolder.remoteModifier().find("encrypted");
        QVERIFY(fakeFileInfo);
        QVERIFY(fakeFileInfo->isEncrypted);
        QCOMPARE(fakeFolder.currentLocalState().children.count(), 5);

        const auto fakeFileId = fakeFileInfo->fileId;
        const auto fakeQnam = new FakeQNAM({});
        // Let's avoid the null filename assert in the default FakeQNAM request creation
        const auto fakeQnamOverride = [this, fakeFileId](const QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device)
            QNetworkReply *reply = nullptr;

            const auto reqUrl = req.url();
            const auto reqRawPath = reqUrl.path();
            const auto reqPath = reqRawPath.startsWith("/owncloud/") ? reqRawPath.mid(10) : reqRawPath;

            if (reqPath.startsWith(QStringLiteral("ocs/v2.php/apps/end_to_end_encryption/api/v1/meta-data/"))) {
                const auto splitUrlPath = reqPath.split('/');
                const auto fileId = splitUrlPath.last();

                const QUrlQuery urlQuery(req.url());
                const auto formatParam = urlQuery.queryItemValue(QStringLiteral("format"));

                if(fileId == fakeFileId && formatParam == QStringLiteral("json")) {
                    reply = new FakePayloadReply(op, req, QJsonDocument().toJson(), this);
                } else {
                    reply = new FakeErrorReply(op, req, this, 400, fake400Response);
                }
            }

            return reply;
        };
        fakeFolder.setServerOverride(fakeQnamOverride);
        fakeQnam->setOverride(fakeQnamOverride);

        const auto account = Account::create();
        const auto capabilities = QVariantMap {
            {QStringLiteral("end-to-end-encryption"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("api-version"), QString::number(2.0)},
            }},
        };
        account->setCapabilities(capabilities);
        account->setCredentials(new FakeCredentials{fakeQnam});
        account->setUrl(QUrl(("owncloud://somehost/owncloud")));
        const auto accountState = new FakeAccountState(account);
        QVERIFY(accountState->isConnected());

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        auto folderDef = folderDefinition(fakeFolder.localPath());
        folderDef.targetPath = "";
        const auto folder = FolderMan::instance()->addFolder(accountState, folderDef);
        QVERIFY(folder);

        qRegisterMetaType<OCC::SyncResult>("SyncResult");
        QSignalSpy folderSyncDone(folder, &Folder::syncFinished);

        QDir dir(folder->path() + QStringLiteral("encrypted"));
        QVERIFY(dir.exists());
        QVERIFY(fakeFolder.remoteModifier().find("encrypted"));
        QVERIFY(fakeFolder.currentLocalState().find("encrypted"));
        QCOMPARE(fakeFolder.currentLocalState().children.count(), 5);

        // Rather than go through the pain of trying to replicate the E2EE response from
        // the server, let's just manually set the encryption bool in the folder journal
        SyncJournalFileRecord rec;
        QVERIFY(folder->journalDb()->getFileRecord(QStringLiteral("encrypted"), &rec));
        rec._e2eEncryptionStatus = SyncJournalFileRecord::EncryptionStatus::EncryptedMigratedV1_2;
        rec._path = QStringLiteral("encrypted").toUtf8();
        rec._type = CSyncEnums::ItemTypeDirectory;
        QVERIFY(folder->journalDb()->setFileRecord(rec));

        SyncJournalFileRecord updatedRec;
        QVERIFY(folder->journalDb()->getFileRecord(QStringLiteral("encrypted"), &updatedRec));
        QVERIFY(updatedRec.isE2eEncrypted());
        QVERIFY(updatedRec.isDirectory());

        FolderMan::instance()->removeE2eFiles(account);

        if (folderSyncDone.isEmpty()) {
            QVERIFY(folderSyncDone.wait());
        }

        QVERIFY(fakeFolder.currentRemoteState().find("encrypted"));
        QVERIFY(!fakeFolder.currentLocalState().find("encrypted"));
        QCOMPARE(fakeFolder.currentLocalState().children.count(), 4);
    }

    void testLeaveShare()
    {
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file

        constexpr auto firstSharePath = "A/sharedwithme_A.txt";
        constexpr auto secondSharePath = "A/B/sharedwithme_B.data";

        QScopedPointer<FakeQNAM> fakeQnam(new FakeQNAM({}));
        OCC::AccountPtr account = OCC::Account::create();
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));
        OCC::AccountManager::instance()->addAccount(account);

        FakeFolder fakeFolder{FileInfo{}};
        fakeFolder.remoteModifier().mkdir("A");

        fakeFolder.remoteModifier().insert(firstSharePath, 100);
        const auto firstShare = fakeFolder.remoteModifier().find(firstSharePath);
        QVERIFY(firstShare);
        firstShare->permissions.setPermission(OCC::RemotePermissions::IsShared);

        fakeFolder.remoteModifier().mkdir("A/B");

        fakeFolder.remoteModifier().insert(secondSharePath, 100);
        const auto secondShare = fakeFolder.remoteModifier().find(secondSharePath);
        QVERIFY(secondShare);
        secondShare->permissions.setPermission(OCC::RemotePermissions::IsShared);

        FolderMan *folderman = FolderMan::instance();
        QCOMPARE(folderman, &_fm);
        OCC::AccountState *accountState = OCC::AccountManager::instance()->accounts().first().data();
        const auto folder = folderman->addFolder(accountState, folderDefinition(fakeFolder.localPath()));
        QVERIFY(folder);

        auto realFolder = FolderMan::instance()->folderForPath(fakeFolder.localPath());
        QVERIFY(realFolder);

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeQnam->setOverride([this, accountState, &fakeFolder](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            Q_UNUSED(device);
            QNetworkReply *reply = nullptr;

            if (op != QNetworkAccessManager::DeleteOperation) {
                reply = new FakeErrorReply(op, req, this, 405);
                return reply;
            }

            if (req.url().path().isEmpty()) {
                reply = new FakeErrorReply(op, req, this, 404);
                return reply;
            }

            const auto filePathRelative = req.url().path().remove(accountState->account()->davPath());

            const auto foundFileInRemoteFolder = fakeFolder.remoteModifier().find(filePathRelative);

            if (filePathRelative.isEmpty() || !foundFileInRemoteFolder) {
                reply = new FakeErrorReply(op, req, this, 404);
                return reply;
            }

           fakeFolder.remoteModifier().remove(filePathRelative);
           reply = new FakePayloadReply(op, req, {}, nullptr);

           emit incomingShareDeleted();

           return reply;
        });

        QSignalSpy incomingShareDeletedSignal(this, &TestFolderMan::incomingShareDeleted);

        // verify first share gets deleted
        folderman->leaveShare(fakeFolder.localPath() + firstSharePath);
        QCOMPARE(incomingShareDeletedSignal.count(), 1);
        QVERIFY(!fakeFolder.remoteModifier().find(firstSharePath));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // verify no share gets deleted
        folderman->leaveShare(fakeFolder.localPath() + "A/B/notsharedwithme_B.data");
        QCOMPARE(incomingShareDeletedSignal.count(), 1);
        QVERIFY(fakeFolder.remoteModifier().find("A/B/sharedwithme_B.data"));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        // verify second share gets deleted
        folderman->leaveShare(fakeFolder.localPath() + secondSharePath);
        QCOMPARE(incomingShareDeletedSignal.count(), 2);
        QVERIFY(!fakeFolder.remoteModifier().find(secondSharePath));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        OCC::AccountManager::instance()->deleteAccount(accountState);
    }

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

        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/oc", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
                 QString(dirPath + "/oc"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
                 QString(dirPath + "/ownCloud3"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
                 QString(dirPath + "/ownCloud22"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2/foo", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
                 QString(dirPath + "/ownCloud2/foo"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2/bar", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
                 QString(dirPath + "/ownCloud2/bar"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/sub", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
                 QString(dirPath + "/sub2"));

        // REMOVE ownCloud2 from the filesystem, but keep a folder sync'ed to it.
        // We should still not suggest this folder as a new folder.
        QDir(dirPath + "/ownCloud2/").removeRecursively();
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
            QString(dirPath + "/ownCloud3"));
        QCOMPARE(folderman->findGoodPathForNewSyncFolder(dirPath + "/ownCloud2", url, FolderMan::GoodPathStrategy::AllowOnlyNewPath),
            QString(dirPath + "/ownCloud22"));
    }
};

QTEST_GUILESS_MAIN(TestFolderMan)
#include "testfolderman.moc"
