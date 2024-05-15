/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <qglobal.h>
#include <QTemporaryDir>
#include <QtTest>

#include "remotewipe.h"

#include "common/utility.h"
#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include "configfile.h"
#include "logger.h"

#include "testhelper.h"

using namespace OCC;

class TestRemoteWipe: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    // TODO
    void testWipe(){
//        QTemporaryDir dir;
//        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file
//        QVERIFY(dir.isValid());

//        QDir dirToRemove(dir.path());
//        QVERIFY(dirToRemove.mkpath("nextcloud"));

//        QString dirPath = dirToRemove.canonicalPath();

//        AccountPtr account = Account::create();
//        QVERIFY(account);

//        auto manager = AccountManager::instance();
//        QVERIFY(manager);

//        AccountState *newAccountState = manager->addAccount(account);
//        manager->save();
//        QVERIFY(newAccountState);

//        QUrl url("http://example.de");
//        HttpCredentialsTest *cred = new HttpCredentialsTest("testuser", "secret");
//        account->setCredentials(cred);
//        account->setUrl( url );

//        FolderMan *folderman = FolderMan::instance();
//        folderman->addFolder(newAccountState, folderDefinition(dirPath + "/sub/nextcloud/"));

//        // check if account exists
//        qDebug() << "Does account exists?!";
//        QVERIFY(!account->id().isEmpty());

//        manager->deleteAccount(newAccountState);
//        manager->save();

//        // check if account exists
//        qDebug() << "Does account exists yet?!";
//        QVERIFY(account);

//        // check if folder exists
//        QVERIFY(dirToRemove.exists());

//        // remote folders
//        qDebug() <<  "Removing folder for account " << newAccountState->account()->url();

//        folderman->slotWipeFolderForAccount(newAccountState);

//        // check if folders dont exist anymore
//        QCOMPARE(dirToRemove.exists(), false);
    }
};

QTEST_APPLESS_MAIN(TestRemoteWipe)
#include "testremotewipe.moc"
