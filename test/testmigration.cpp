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
    static constexpr char legacyAppName[] = "ownCloud";
    static constexpr char standardAppName[] = "Nextcloud";
    static constexpr char brandedAppName[] = "Branded";
    static constexpr char ocBrandedAppName[] = "branded";
    static constexpr char legacyAppConfigContent[] = "[General]\n"
        "clientVersion=5.3.2.15463\n"
        "issuesWidgetFilter=FatalError, BlacklistedError, Excluded, Message, FilenameReserved\n"
        "logHttp=false\n"
        "optionalDesktopNotifications=true\n"
        "\n"
        "[Accounts]e\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\davUrl=@Variant(http://oc.de/remote.php/dav/files/admin/)\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\deployed=false\n"
        "0\\Folders\\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\displayString=ownCloud\n"
        "0\\Folders\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\ignoreHiddenFiles=true\n"
        "0\\Folders\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\journalPath=.sync_journal.db\n"
        "0\\Folders\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\localPath=/ownCloud/\n"
        "0\\Folders\2ba4b09a-1223-aaaa-abcd-c2df238816d8\\paused=false\n"
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

    void setupStandarConfig(const QString &version)
    {
        setupStandardConfigFolder();
        QSettings settings(_configFile.configFile(), QSettings::IniFormat);
        _configFile.setClientVersionString(version);
        _configFile.setOptionalServerNotifications(true);
        _configFile.setShowChatNotifications(true);
        _configFile.setShowCallNotifications(true);
        _configFile.setShowInExplorerNavigationPane(true);
        _configFile.setShowInExplorerNavigationPane(true);
        _configFile.setRemotePollInterval(std::chrono::milliseconds(1000));
        _configFile.setAutoUpdateCheck(true, QString());
        _configFile.setUpdateChannel("beta");
        _configFile.setOverrideServerUrl("http://example.de");
        _configFile.setOverrideLocalDir("A");
        _configFile.setVfsEnabled(true);
        _configFile.setProxyType(0);
        _configFile.setVfsEnabled(true);
        _configFile.setUseUploadLimit(0);
        _configFile.setUploadLimit(1);
        _configFile.setUseDownloadLimit(0);
        _configFile.setUseDownloadLimit(1);
        _configFile.setNewBigFolderSizeLimit(true, 500);
        _configFile.setNotifyExistingFoldersOverLimit(true);
        _configFile.setStopSyncingExistingFoldersOverLimit(true);
        _configFile.setConfirmExternalStorage(true);
        _configFile.setMoveToTrash(true);
        _configFile.setForceLoginV2(true);
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
        folderDefinition.localPath = "/standardAppName";
        folderDefinition.targetPath = "/";
        folderDefinition.alias = standardAppName;
        _folderMan.reset({});
        _folderMan.reset(new FolderMan{});
        QVERIFY(_folderMan->addFolder(accountState, folderDefinition));
    }

    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testSetPhase()
    {
        Migration migration;
        QCOMPARE(migration.phase(), OCC::Migration::Phase::NotStarted);
        migration.setPhase(OCC::Migration::Phase::SetupConfigFile);
        QCOMPARE(migration.phase(), OCC::Migration::Phase::SetupConfigFile);
        migration.setPhase(OCC::Migration::Phase::SetupUsers);
        QCOMPARE(migration.phase(), OCC::Migration::Phase::SetupUsers);
        migration.setPhase(OCC::Migration::Phase::SetupFolders);
        QCOMPARE(migration.phase(), OCC::Migration::Phase::SetupFolders);
        migration.setPhase(OCC::Migration::Phase::Done);
        QCOMPARE(migration.phase(), OCC::Migration::Phase::Done);
    }

    void testSetUpgradeType()
    {
        Migration migration;
        QCOMPARE(migration.upgradeType(), OCC::Migration::UpgradeType::NoChange);
        migration.setUpgradeType(OCC::Migration::UpgradeType::Upgrade);
        QCOMPARE(migration.upgradeType(), OCC::Migration::UpgradeType::Upgrade);
        migration.setUpgradeType(OCC::Migration::UpgradeType::Downgrade);
        QCOMPARE(migration.upgradeType(), OCC::Migration::UpgradeType::Downgrade);
    }

    void testSetBrandingType()
    {
        Migration migration;
        QCOMPARE(migration.brandingType(), OCC::Migration::BrandingType::UnbrandedToUnbranded);
        migration.setBrandingType(OCC::Migration::BrandingType::LegacyToUnbranded);
        QCOMPARE(migration.brandingType(), OCC::Migration::BrandingType::LegacyToUnbranded);
        migration.setBrandingType(OCC::Migration::BrandingType::LegacyToBranded);
        QCOMPARE(migration.brandingType(), OCC::Migration::BrandingType::LegacyToBranded);
        migration.setBrandingType(OCC::Migration::BrandingType::UnbrandedToBranded);
        QCOMPARE(migration.brandingType(), OCC::Migration::BrandingType::UnbrandedToBranded);
    }

    void testSetDiscoveredLegacyConfigPath()
    {
        Migration migration;
        QCOMPARE(migration.discoveredLegacyConfigPath(), QString());
        const auto legacyConfigPath = QString("/path/to/legacy/config");
        migration.setDiscoveredLegacyConfigPath(legacyConfigPath);
        QCOMPARE(migration.discoveredLegacyConfigPath(), legacyConfigPath);
    }

    void testUpgrade()
    {
        // create Nextcloud config with older version
        setupStandarConfig("1.0.0");
        Migration migration;
        QCOMPARE(migration.isUpgrade(), true);

        // backup old config
        const auto backupFilesList = _configFile.backupConfigFiles();
        QCOMPARE_GE(backupFilesList.size(), 1);

        // successfully upgrade to new config
        const auto afterUpgradeVersionNumber = MIRALL_VERSION_STRING;
        _configFile.setClientVersionString(afterUpgradeVersionNumber);
        QCOMPARE(_configFile.clientVersionString(), MIRALL_VERSION_STRING);
    }
};

QTEST_GUILESS_MAIN(TestMigration)
#include "testmigration.moc"
