/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <qglobal.h>
#include <QtTest>

#include "remotewipe.h"
#include "accountmanager.h"

#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include "configfile.h"
#include "logger.h"

#include "testhelper.h"

#include "syncenginetestutils.h"

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

    void testRemoteWipe()
    {
        auto dir = QTemporaryDir {};
        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file

        // RemoteWipe needs FolderMan for actually wiping local data
        FolderMan fm;
        auto folderMan = FolderMan::instance();
        QVERIFY(folderMan);

        // RemoteWipe also needs an account present in the AccountManager
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        auto account = fakeFolder.account();
        auto accountState = AccountManager::instance()->addAccount(account);

        // retrieve the RemoteWipe instance created from the real AccountState,
        // and replace its QNetworkAccessManager with our own one for testing
        auto remoteWipe = FakeAccountState::remoteWipe(accountState);
        auto fakeQnam = new FakeQNAM({});
        remoteWipe->_networkManager->deleteLater();
        remoteWipe->_networkManager = fakeQnam;

        // let FolderMan know about our sync folder
        FolderMan::instance()->addFolder(accountState, folderDefinition(fakeFolder.localPath()));

        bool revokeAppPassword = false; // whether respond with 401 to requests
        bool doWipe = false;            // whether a remote wipe should be done

        const auto fakeQnamOverride = [&](const QNetworkAccessManager::Operation op, const QNetworkRequest &request, QIODevice *device) -> QNetworkReply * {
            Q_UNUSED(device)
            if (!revokeAppPassword) {
                qDebug() << "App password not revoked";
                return nullptr;
            }

            const auto requestUrl = request.url();
            const auto requestPath = requestUrl.path();

            if (op == QNetworkAccessManager::Operation::DeleteOperation && requestPath.endsWith("/ocs/v2.php/core/apppassword")) {
                qDebug() << "Responding success for app password deletion";
                // allow deletion of appPassword to succeed
                return new FakeJsonReply(op, request, this, 200);
            }

            if (doWipe) {
                qDebug() << "Wipe enabled";
                if (requestPath.endsWith("/index.php/core/wipe/check")) {
                    qDebug() << "Responding with wipe=true";
                    return new FakeJsonReply(op, request, this, 200, QJsonDocument::fromJson(R"({"wipe": true})"));
                } else if (requestPath.endsWith("/index.php/core/wipe/success")) {
                    qDebug() << "Responding with successful wipe";
                    return new FakeJsonReply(op, request, this, 200, QJsonDocument::fromJson(R"({})"));
                }
            }

            qDebug() << "Responding with unauthorised";
            auto errorReply = new FakeErrorReply(op, request, this, 401);
            errorReply->setError(QNetworkReply::AuthenticationRequiredError, QLatin1String("Unauthorised"));

            return errorReply;
        };
        fakeFolder.setServerOverride(fakeQnamOverride);
        fakeQnam->setOverride(fakeQnamOverride);

        const auto localFolderExists = [&fakeFolder]() -> bool {
            return QDir(fakeFolder.localPath()).exists();
        };

        // initial sync to ensure we've had a working connection
        qDebug() << "Test: Initial sync works";
        QVERIFY(fakeFolder.syncOnce());

        // just revoking the app password -> no remote wipe should be done
        qDebug() << "Test: App password revoked, no remote wipe triggered";
        revokeAppPassword = true;
        QVERIFY(!fakeFolder.syncOnce());
        // `Account` will try to retrieve the password from the keychain,
        // however during testing the password received from it will be empty.
        // An empty password will not perform the wipe check at all.
        // Therefore: call the slot which `Account` connects its
        // `appPasswordRetrieved` signal directly on the remoteWipe instance
        remoteWipe->startCheckJobWithAppPassword("password");
        QTest::qWait(500); // wait a bit to process events
        // ensure the account was not removed and the sync folder is still present
        QCOMPARE(AccountManager::instance()->accounts().size(), 1);
        QVERIFY2(localFolderExists(), "Local sync folder should exist as no wipe was requested");

        // hack for test: close the journal db of FakeFolder, as FolderMan::addFolder creates its own
        // as long as the test DB is open, removing files will break on e.g. Windows
        fakeFolder.syncJournal().close();

        // server tells us to wipe the local data
        qDebug() << "Test: Server tells us to remote wipe";
        doWipe = true;
        // ensure folder exists before performing the wipe
        QVERIFY2(localFolderExists(), "Local sync folder should exist before wiping");
        remoteWipe->startCheckJobWithAppPassword("password");
        QTest::qWait(500); // wait a bit to process events
        // account should now be gone
        QCOMPARE(AccountManager::instance()->accounts().size(), 0);
        // local folder should now be gone
        QVERIFY2(!localFolderExists(), "Local sync folder should be removed after wiping");
    }
};

QTEST_GUILESS_MAIN(TestRemoteWipe)
#include "testremotewipe.moc"
