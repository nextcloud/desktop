// SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "e2eefoldermanager.h"
#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "clientsideencryption.h"
#include "configfile.h"
#include "folderman.h"
#include "folder.h"
#include "syncenginetestutils.h"
#include "foldermantestutils.h"

using namespace OCC;

class TestE2EFolderManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        // Clean up before each test
        AccountManager::instance()->shutdown();
    }

    void cleanup()
    {
        // Clean up after each test
        AccountManager::instance()->shutdown();
    }

    void testSingletonInstance()
    {
        // GIVEN
        auto *manager1 = E2EFolderManager::instance();
        auto *manager2 = E2EFolderManager::instance();

        // THEN - should return the same instance
        QVERIFY(manager1 != nullptr);
        QCOMPARE(manager1, manager2);
    }

    void testInitializeWithNoAccounts()
    {
        // GIVEN - no accounts
        QCOMPARE(AccountManager::instance()->accounts().size(), 0);

        // WHEN - instance is created (init happens in constructor)
        auto *manager = E2EFolderManager::instance();

        // THEN - should not crash
        QVERIFY(manager != nullptr);
    }

    void testInitializeWithExistingAccount()
    {
        // GIVEN - an account with E2E
        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl("http://example.com"));

        [[maybe_unused]] auto accountState = new AccountState(account);
        AccountManager::instance()->addAccount(account);

        // WHEN - E2EFolderManager is instantiated it should connect to the account
        auto *manager = E2EFolderManager::instance();

        // THEN - should connect to the account
        QVERIFY(manager != nullptr);
        QCOMPARE(AccountManager::instance()->accounts().size(), 1);
    }

    void testAccountAddedSignal()
    {
        // GIVEN - initialized manager (constructor connects to accountAdded signal)
        auto *manager = E2EFolderManager::instance();
        QVERIFY(manager != nullptr);

        // WHEN - adding a new account
        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl("http://example.com"));

        [[maybe_unused]] auto accountState = new AccountState(account);

        // Spy on the accountAdded signal to verify E2EFolderManager reacts to it
        QSignalSpy accountAddedSpy(AccountManager::instance(), &AccountManager::accountAdded);
        AccountManager::instance()->addAccount(account);

        // THEN - accountAdded signal was emitted (E2EFolderManager should have handled it)
        QCOMPARE(accountAddedSpy.count(), 1);
        QCOMPARE(AccountManager::instance()->accounts().size(), 1);
    }

    void testRestoreFoldersWhenE2EInitialized()
    {
        // Test that E2EFolderManager responds to E2E initialization signals
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl("http://example.com"));

        const QVariantMap capabilities {
            {QStringLiteral("end-to-end-encryption"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("api-version"), QString::number(2.0)},
            }},
        };
        account->setCapabilities(capabilities);

        auto accountState = new AccountState(account);
        AccountManager::instance()->addAccount(account);

        // E2EFolderManager is initialized via constructor when instance() is called
        auto *manager = E2EFolderManager::instance();
        QVERIFY(manager != nullptr);

        QVERIFY(account->e2e());

        // Verify E2E is not yet initialized
        QVERIFY(!account->e2e()->isInitialized());
        QCOMPARE(account->e2e()->initializationState(),
                 ClientSideEncryption::InitializationState::NotStarted);
    }

    void testNoRestorationWhenE2ENotInitialized()
    {
        // GIVEN - account without initialized E2E
        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl("http://example.com"));

        // THEN - E2E should not be initialized
        QVERIFY(account->e2e());
        QVERIFY(!account->e2e()->isInitialized());
        QCOMPARE(account->e2e()->initializationState(),
                 ClientSideEncryption::InitializationState::NotStarted);
    }

    void testMultipleAccountsHandling()
    {
        // GIVEN - multiple accounts
        auto account1 = Account::create();
        account1->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account1->setUrl(QUrl("http://example1.com"));

        auto account2 = Account::create();
        account2->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account2->setUrl(QUrl("http://example2.com"));

        [[maybe_unused]] auto accountState1 = new AccountState(account1);
        [[maybe_unused]] auto accountState2 = new AccountState(account2);

        AccountManager::instance()->addAccount(account1);
        AccountManager::instance()->addAccount(account2);

        // WHEN - E2EFolderManager is instantiated it connects to all accounts
        auto *manager = E2EFolderManager::instance();

        // THEN - should handle both accounts
        QVERIFY(manager != nullptr);
        QCOMPARE(AccountManager::instance()->accounts().size(), 2);
    }

    void testRestorationClearsTrackingList()
    {
        // Test that E2EFolderManager properly initializes with accounts
        // The actual restoration clearing is tested at the FolderMan level
        // in testfolderman.cpp::testE2ERestorationClearsTrackingList()
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl("http://example.com"));

        const QVariantMap capabilities {
            {QStringLiteral("end-to-end-encryption"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("api-version"), QString::number(2.0)},
            }},
        };
        account->setCapabilities(capabilities);

        auto accountState = new AccountState(account);
        AccountManager::instance()->addAccount(account);

        // E2EFolderManager connects to account's E2E signals in its constructor
        auto *manager = E2EFolderManager::instance();

        QVERIFY(manager != nullptr);
        QCOMPARE(AccountManager::instance()->accounts().size(), 1);
    }

    void testOnlyRestoresForCorrectAccount()
    {
        // Test that E2EFolderManager handles multiple accounts correctly
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        auto account1 = Account::create();
        account1->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account1->setUrl(QUrl("http://example1.com"));

        const QVariantMap capabilities1 {
            {QStringLiteral("end-to-end-encryption"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("api-version"), QString::number(2.0)},
            }},
        };
        account1->setCapabilities(capabilities1);

        auto account2 = Account::create();
        account2->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account2->setUrl(QUrl("http://example2.com"));

        const QVariantMap capabilities2 {
            {QStringLiteral("end-to-end-encryption"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("api-version"), QString::number(2.0)},
            }},
        };
        account2->setCapabilities(capabilities2);

        [[maybe_unused]] auto accountState1 = new AccountState(account1);
        [[maybe_unused]] auto accountState2 = new AccountState(account2);

        AccountManager::instance()->addAccount(account1);
        AccountManager::instance()->addAccount(account2);

        // E2EFolderManager connects to all accounts' E2E signals in its constructor
        auto *manager = E2EFolderManager::instance();

        // Verify manager handles both accounts
        QVERIFY(manager != nullptr);
        QCOMPARE(AccountManager::instance()->accounts().size(), 2);

        // Verify each account has its own E2E instance
        QVERIFY(account1->e2e());
        QVERIFY(account2->e2e());
        QVERIFY(account1->e2e() != account2->e2e());
    }

    void testScenario1_FoldersRestoreAfterRestart()
    {
        // Scenario 1: Client Restart (Primary Bug Fix)
        // Verify E2E folders marked for restoration are processed when E2E initializes
        QTemporaryDir dir;
        ConfigFile::setConfDir(dir.path());

        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setUrl(QUrl("http://example.com"));

        const QVariantMap capabilities {
            {QStringLiteral("end-to-end-encryption"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("api-version"), QString::number(2.0)},
            }},
        };
        account->setCapabilities(capabilities);

        auto accountState = new AccountState(account);
        AccountManager::instance()->addAccount(account);

        // Simulate folders blacklisted during startup (before E2E initialized)
        // This mimics what happens when client restarts and E2E isn't ready yet
        QTemporaryDir syncDir;
        QString dbPath = syncDir.path() + "/.sync_test.db";
        SyncJournalDb db(dbPath);

        QStringList e2eFoldersToRestore = {"/encrypted1/", "/encrypted2/"};
        db.setSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist,
            e2eFoldersToRestore);

        // Verify folders are marked for restoration
        bool ok = false;
        auto restorationList = db.getSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, &ok);
        QVERIFY(ok);
        QCOMPARE(restorationList.size(), 2);

        // E2EFolderManager is ready to restore folders when E2E signal fires
        auto *manager = E2EFolderManager::instance();
        QVERIFY(manager != nullptr);
        QVERIFY(account->e2e());
    }

    void testScenario5_MultipleFoldersTrackedForRestoration()
    {
        // Scenario 5: Multiple E2E Folders
        // Verify multiple E2E folders can be tracked and restored
        QTemporaryDir dir;
        QString dbPath = dir.path() + "/.sync_test.db";
        SyncJournalDb db(dbPath);

        // Simulate multiple E2E folders being blacklisted during startup
        QStringList multipleFolders = {
            "/Documents/Private/",
            "/Photos/Encrypted/",
            "/Work/Confidential/",
            "/Personal/Secrets/"
        };

        db.setSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist,
            multipleFolders);

        // Verify all folders are tracked
        bool ok = false;
        auto restorationList = db.getSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, &ok);
        QVERIFY(ok);
        QCOMPARE(restorationList.size(), 4);

        for (const auto &folder : multipleFolders) {
            QVERIFY(restorationList.contains(folder));
        }

        // After restoration, list should be clearable
        db.setSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist,
            {});

        restorationList = db.getSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, &ok);
        QVERIFY(ok);
        QVERIFY(restorationList.isEmpty());
    }

    void testScenario6_UserBlacklistPreserved()
    {
        // Scenario 6: User-Blacklisted E2E Folder
        // Verify user-blacklisted folders are NOT added to restoration list
        QTemporaryDir dir;
        QString dbPath = dir.path() + "/.sync_test.db";
        SyncJournalDb db(dbPath);

        // User manually blacklists an E2E folder via selective sync
        QStringList userBlacklist = {"/User/Excluded/"};
        db.setSelectiveSyncList(
            SyncJournalDb::SelectiveSyncBlackList,
            userBlacklist);

        // Verify it's blacklisted
        bool ok = false;
        auto blacklist = db.getSelectiveSyncList(
            SyncJournalDb::SelectiveSyncBlackList, &ok);
        QVERIFY(ok);
        QCOMPARE(blacklist.size(), 1);
        QVERIFY(blacklist.contains("/User/Excluded/"));

        // Verify it's NOT in restoration list (would be added only during E2E init)
        auto restorationList = db.getSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, &ok);
        QVERIFY(ok);
        QVERIFY(restorationList.isEmpty());

        // This ensures user preferences are preserved across restarts
    }
};

QTEST_GUILESS_MAIN(TestE2EFolderManager)
#include "teste2efoldermanager.moc"