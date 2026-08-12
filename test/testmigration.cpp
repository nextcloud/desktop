/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <qglobal.h>
#include <QTemporaryDir>
#include <QtTest>
#include <QtTest/qtestcase.h>

#include "common/utility.h"
#include "folderman.h"
#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "configfile.h"
#include "syncenginetestutils.h"
#include "testhelper.h"
#include "version.h"
#include "settings/migration.h"

using namespace OCC;

class TestMigration: public QObject
{
    Q_OBJECT

    ConfigFile _configFile;
    QTemporaryDir _temporaryDir;
    std::unique_ptr<FolderMan> _folderMan;

private:
    static constexpr char standardAppName[] = "Nextcloud";
    static constexpr char legacyAppConfigContent[] = "[General]\n"
        "clientVersion=5.3.2.15463\n"
        "issuesWidgetFilter=FatalError, BlacklistedError, Excluded, Message, FilenameReserved\n"
        "logHttp=false\n"
        "optionalDesktopNotifications=true\n"
        "\n"
        "[Accounts]\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\davUrl=@Variant(http://oc.de/remote.php/dav/files/admin/)\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\deployed=false\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\displayString=ownCloud\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\ignoreHiddenFiles=true\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\journalPath=.sync_journal.db\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\localPath=/ownCloud/\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\paused=false\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\priority=0\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\targetPath=/\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\version=13\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\virtualFilesMode=off\n"
        "0\\capabilities=@QVariant()\n"
        "0\\dav_user=admin\n"
        "0\\default_sync_root=/ownCloud\n"
        "0\\display-name=admin\n"
        "0\\http_CredentialVersion=1\n"
        "0\\http_oauth=false\n"
        "0\\http_user=admin\n"
        "0\\supportsSpaces=true\n"
        "0\\url=http://oc.de/\n"
        "0\\user=admin\n"
        "0\\userExplicitlySignedOut=false\n"
        "0\\uuid=@Variant()\n"
        "0\\version=13\n"
        "version=13\n"
        "\n"
        "[Credentials]\n"
        "ownCloud_credentials%oc.de%2ba4b09a-1223-aaaa-abcd-c2df238816d8\\http\\password=true";

private slots:
    void setupStandardConfigFolder()
    {
        QVERIFY(QDir(_temporaryDir.path()).mkpath(standardAppName));
        const auto standardConfigFolder = QString(_temporaryDir.path() + "/" + standardAppName);
        _configFile.setConfDir(standardConfigFolder);
    }

    void setupStandardConfig(const QString &version)
    {
        setupStandardConfigFolder();
        QSettings settings(_configFile.configFile(), QSettings::IniFormat);
        _configFile.setClientVersionString(version);
        _configFile.setOptionalServerNotifications(true);
        _configFile.setShowChatNotifications(true);
        _configFile.setShowCallNotifications(true);
        _configFile.setShowInExplorerNavigationPane(true);
        _configFile.setRemotePollInterval(std::chrono::milliseconds(1000));
        _configFile.setAutoUpdateCheck(true, QString());
        _configFile.setUpdateChannel("beta");
        _configFile.setOverrideServerUrl("http://example.de");
        _configFile.setOverrideLocalDir("A");
        _configFile.setVfsEnabled(true);
        _configFile.setProxyType(0);
        _configFile.setUseUploadLimit(0);
        _configFile.setUploadLimit(1);
        _configFile.setUseDownloadLimit(0);
        _configFile.setUseDownloadLimit(1);
        _configFile.setNewBigFolderSizeLimit(true, 500);
        _configFile.setNotifyExistingFoldersOverLimit(true);
        _configFile.setStopSyncingExistingFoldersOverLimit(true);
        _configFile.setConfirmExternalStorage(true);
        _configFile.setMoveToTrash(true);
        _configFile.setPromptDeleteFiles(true);
        _configFile.setDeleteFilesThreshold(1);
        _configFile.setMonoIcons(true);
        _configFile.setAutomaticLogDir(true);
        _configFile.setLogDir(_temporaryDir.path());
        _configFile.setLogDebug(true);
        _configFile.setLogExpire(72);
        _configFile.setLogFlush(true);
        _configFile.setCertificatePath(_temporaryDir.path());
        _configFile.setCertificatePasswd("123456");
        _configFile.setLaunchOnSystemStartup(true);
        _configFile.setServerHasValidSubscription(true);
        _configFile.setDesktopEnterpriseChannel("stable");
        _configFile.setLanguage("pt");
        settings.sync();
        QVERIFY(_configFile.exists());
        QScopedPointer<FakeQNAM> fakeQnam(new FakeQNAM({}));
        OCC::AccountPtr account = OCC::Account::create();
        account->setDavUser("user");
        account->setDavDisplayName("Nextcloud user");
        // TODO: detangle UI from logic
        //account->setProxyType(QNetworkProxy::ProxyType::HttpProxy);
        //account->setProxyUser("proxyuser");
        account->setDownloadLimit(120);
        account->setUploadLimit(120);
        account->setDownloadLimitSetting(OCC::Account::AccountNetworkTransferLimitSetting::ManualLimit);
        account->setServerVersion("30");
        account->setCredentials(new FakeCredentials{fakeQnam.data()});
        account->setUrl(QUrl(("http://example.de")));
        const auto accountState = OCC::AccountManager::instance()->addAccount(account);
        OCC::AccountManager::instance()->saveAccount(accountState->account());
        OCC::FolderDefinition folderDefinition;
        const auto localFolder = QString(_temporaryDir.path() + "/syncfolder");
        QVERIFY(QDir().mkpath(localFolder));
        folderDefinition.localPath = localFolder;
        folderDefinition.targetPath = "/";
        folderDefinition.alias = standardAppName;
        _folderMan.reset(new FolderMan{});
        QVERIFY(_folderMan->addFolder(accountState, folderDefinition));
    }

    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    // Reset all Migration static state and QSettings config before every test
    // so tests are fully independent of each other.
    void init()
    {
        Migration::resetForTesting();
        setupStandardConfigFolder();
        QSettings settings(_configFile.configFile(), QSettings::IniFormat);
        settings.clear();
        settings.sync();
    }

    // Release the folders and accounts created by setupStandardConfig so they
    // do not leak into the AccountManager singleton across tests.
    void cleanup()
    {
        _folderMan.reset();
        if (const auto accountManager = AccountManager::instance()) {
            accountManager->shutdown();
        }
    }

    void testSetPhase()
    {
        QCOMPARE(Migration::phase(), OCC::Migration::Phase::NotStarted);
        Migration::setPhase(OCC::Migration::Phase::SetupConfigFile);
        QCOMPARE(Migration::phase(), OCC::Migration::Phase::SetupConfigFile);
        Migration::setPhase(OCC::Migration::Phase::SetupUsers);
        QCOMPARE(Migration::phase(), OCC::Migration::Phase::SetupUsers);
        Migration::setPhase(OCC::Migration::Phase::SetupFolders);
        QCOMPARE(Migration::phase(), OCC::Migration::Phase::SetupFolders);
        Migration::setPhase(OCC::Migration::Phase::Done);
        QCOMPARE(Migration::phase(), OCC::Migration::Phase::Done);
    }

    void testSetUpgradeType()
    {
        QCOMPARE(Migration::upgradeType(), OCC::Migration::UpgradeType::NoChange);
        Migration::setUpgradeType(OCC::Migration::UpgradeType::Upgrade);
        QCOMPARE(Migration::upgradeType(), OCC::Migration::UpgradeType::Upgrade);
        Migration::setUpgradeType(OCC::Migration::UpgradeType::Downgrade);
        QCOMPARE(Migration::upgradeType(), OCC::Migration::UpgradeType::Downgrade);
    }

    void testSetBrandingType()
    {
        QCOMPARE(Migration::brandingType(), OCC::Migration::BrandingType::UnbrandedToUnbranded);
        Migration::setBrandingType(OCC::Migration::BrandingType::LegacyToUnbranded);
        QCOMPARE(Migration::brandingType(), OCC::Migration::BrandingType::LegacyToUnbranded);
        Migration::setBrandingType(OCC::Migration::BrandingType::LegacyToBranded);
        QCOMPARE(Migration::brandingType(), OCC::Migration::BrandingType::LegacyToBranded);
        Migration::setBrandingType(OCC::Migration::BrandingType::UnbrandedToBranded);
        QCOMPARE(Migration::brandingType(), OCC::Migration::BrandingType::UnbrandedToBranded);
    }

    void testSetDiscoveredLegacyConfigPath()
    {
        QCOMPARE(Migration::discoveredLegacyConfigPath(), QString());
        const auto legacyConfigPath = QString("/path/to/legacy/config");
        Migration::setDiscoveredLegacyConfigPath(legacyConfigPath);
        QCOMPARE(Migration::discoveredLegacyConfigPath(), legacyConfigPath);
    }

    void testUpgrade()
    {
        // create Nextcloud config with older version
        setupStandardConfig("1.0.0");
        QCOMPARE(Migration::isUpgrade(), true);

        // backup old config
        const auto backupFilesList = _configFile.backupConfigFiles();
        QCOMPARE_GE(backupFilesList.size(), 1);

        // successfully upgrade to new config
        const auto afterUpgradeVersionNumber = MIRALL_VERSION_STRING;
        _configFile.setClientVersionString(afterUpgradeVersionNumber);
        QCOMPARE(_configFile.clientVersionString(), MIRALL_VERSION_STRING);
    }

    void testIsInProgress_notStarted()
    {
        QCOMPARE(Migration::phase(), Migration::Phase::NotStarted);
        QCOMPARE(Migration::isInProgress(), false);
    }

    void testIsInProgress_trueForMidPhases()
    {
        Migration::setPhase(Migration::Phase::SetupConfigFile);
        QCOMPARE(Migration::isInProgress(), true);

        Migration::resetForTesting();
        Migration::setPhase(Migration::Phase::SetupUsers);
        QCOMPARE(Migration::isInProgress(), true);

        Migration::resetForTesting();
        Migration::setPhase(Migration::Phase::SetupFolders);
        QCOMPARE(Migration::isInProgress(), true);
    }

    void testIsInProgress_falseWhenDone()
    {
        Migration::setPhase(Migration::Phase::Done);
        QCOMPARE(Migration::isInProgress(), false);
    }

    void testPhaseRollbackPrevented()
    {
        Migration::setPhase(Migration::Phase::Done);
        Migration::setPhase(Migration::Phase::SetupUsers);  // attempt rollback
        QCOMPARE(Migration::phase(), Migration::Phase::Done);
    }

    void testIsUpgrade_noSideEffects()
    {
        setupStandardConfig("1.0.0");

        // upgrading: current > previous
        QCOMPARE(Migration::isUpgrade(), true);
        QCOMPARE(Migration::isDowngrade(), false);
        QCOMPARE(Migration::versionChanged(), true);

        // calling isDowngrade after isUpgrade must not corrupt the result
        QCOMPARE(Migration::isUpgrade(), true);

        // upgradeType is not touched by isUpgrade/isDowngrade
        QCOMPARE(Migration::upgradeType(), Migration::UpgradeType::NoChange);
    }

    void testIsDowngrade_noSideEffects()
    {
        // simulate a downgrade: the config was written by a newer client than the current binary
        _configFile.setClientVersionString("99.0.0");

        QCOMPARE(Migration::isDowngrade(), true);
        QCOMPARE(Migration::isUpgrade(), false);
        QCOMPARE(Migration::versionChanged(), true);

        // upgradeType is not touched by isDowngrade
        QCOMPARE(Migration::upgradeType(), Migration::UpgradeType::NoChange);
    }

    void testVersionUnchanged()
    {
        setupStandardConfigFolder();
        _configFile.setClientVersionString(MIRALL_VERSION_STRING);
        _configFile.setClientPreviousVersionString(MIRALL_VERSION_STRING);

        QCOMPARE(Migration::isUpgrade(), false);
        QCOMPARE(Migration::isDowngrade(), false);
        QCOMPARE(Migration::versionChanged(), false);
    }

    void testShouldTryToMigrate_trueOnUpgrade()
    {
        setupStandardConfig("1.0.0");
        QCOMPARE(Migration::shouldTryToMigrate(), true);
    }

    void testShouldTryToMigrate_falseWhenVersionsMatch()
    {
        setupStandardConfigFolder();
        _configFile.setClientVersionString(MIRALL_VERSION_STRING);
        _configFile.setClientPreviousVersionString(MIRALL_VERSION_STRING);

        QCOMPARE(Migration::shouldTryToMigrate(), false);
    }

    void testShouldTryToMigrate_falseWhenConfigMatchesRunningVersion()
    {
        // clientVersion already equals the running binary: nothing to migrate,
        // even though an older previous version is recorded.
        setupStandardConfigFolder();
        _configFile.setClientVersionString(MIRALL_VERSION_STRING);
        _configFile.setClientPreviousVersionString(QStringLiteral("1.0.0"));

        QCOMPARE(Migration::shouldTryToMigrate(), false);
    }

    void testShouldTryToMigrate_trueWhenUpgradingFromMatchingVersions()
    {
        // A genuine upgrade in which clientVersion equals clientPreviousVersion
        // (the previous run settled), so migration must still run.
        setupStandardConfigFolder();
        _configFile.setClientVersionString(QStringLiteral("1.0.0"));
        _configFile.setClientPreviousVersionString(QStringLiteral("1.0.0"));

        QCOMPARE(Migration::shouldTryToMigrate(), true);
    }

    void testLegacyData_discoversAndParsesConfig()
    {
        // No current config keys, so legacyData() searches the legacy locations.
        setupStandardConfigFolder();

        // Place a legacy owncloud.cfg next to the themed config file.
        const auto configDir = QFileInfo(_configFile.configFile()).absolutePath();
        const auto legacyConfigPath = configDir + QStringLiteral("/owncloud.cfg");
        QFile legacyFile(legacyConfigPath);
        QVERIFY(legacyFile.open(QIODevice::WriteOnly | QIODevice::Text));
        legacyFile.write(legacyAppConfigContent);
        legacyFile.close();

        const auto legacy = Migration::legacyData();

        // Sole ownership is transferred to the caller and the file is parsed.
        QVERIFY(legacy != nullptr);
        QCOMPARE(legacy->value(QStringLiteral("clientVersion")).toString(), QStringLiteral("5.3.2.15463"));
        legacy->beginGroup(QStringLiteral("Accounts"));
        QVERIFY(legacy->childGroups().contains(QStringLiteral("0")));
        legacy->endGroup();
        QVERIFY(Migration::discoveredLegacyConfigPath().isEmpty());
    }

    void testLegacyData_returnsNullWhenNoLegacyConfig()
    {
        setupStandardConfigFolder();
        // The config dir is shared between tests, so drop any legacy file a
        // previous test may have written.
        const auto configDir = QFileInfo(_configFile.configFile()).absolutePath();
        QFile::remove(configDir + QStringLiteral("/owncloud.cfg"));

        const auto legacy = Migration::legacyData();

        QVERIFY(!legacy);
        QCOMPARE(Migration::discoveredLegacyConfigPath(), QString());
    }
};

QTEST_GUILESS_MAIN(TestMigration)
#include "testmigration.moc"
