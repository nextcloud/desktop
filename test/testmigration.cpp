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
        account->setProxyType(QNetworkProxy::ProxyType::HttpProxy);
        account->setProxyUser("proxyuser");
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

    // Upgrade - TODO: test running app with --confdir
    void testUpgrade()
    {
        // create Nextcloud config with older version
        setupStandarConfig("1.0.0");
        const auto oldAppVersionNumber = QVersionNumber::fromString(_configFile.clientVersionString());
        QVERIFY(_configFile.isUpgrade());

        // backup old config
        const auto backupFilesList = _configFile.backupConfigFiles();
        QCOMPARE_GE(backupFilesList.size(), 1);

        // successfully upgrade to new config
        const auto afterUpgradeVersionNumber = MIRALL_VERSION_STRING;
        _configFile.setClientVersionString(afterUpgradeVersionNumber);
        QVERIFY(MIRALL_VERSION_STRING == _configFile.clientVersionString());

        QCOMPARE_GE(AccountManager::instance()->accounts().size(), 1);
        auto accounts = AccountManager::instance()->accounts().first()->settings();
        QCOMPARE_GE(accounts->childGroups().size(), 1);
        accounts->beginGroup(QLatin1String("Folders"));
        QCOMPARE_GE(accounts->childGroups().size(), 1);
        accounts->endGroup();
    }

    // From oC client to Nextcloud
    void testMigrationFromOctoNextcloud()
    {
        QTemporaryDir tempDir;
        QVERIFY(QDir(tempDir.path()).mkpath(legacyAppName));
        const auto ocConfigFolder = QString(tempDir.path() + "/" + legacyAppName);
        const auto ocConfig = QString(ocConfigFolder + "/" + QString(legacyAppName).toLower() + ".cfg");
        QFile ocConfigFile(ocConfig);
        QVERIFY(ocConfigFile.open(QFile::WriteOnly));
        QCOMPARE_GE(ocConfigFile.write(legacyAppConfigContent, qstrlen(legacyAppConfigContent)), 0);
        ocConfigFile.close();

        ConfigFile configFile;

        QVERIFY(QDir(tempDir.path()).mkpath(standardAppName));
        const auto standardConfigFolder = QString(tempDir.path() + "/" + standardAppName);
        configFile.setConfDir(standardConfigFolder);

        // Nextcloud config file does not exist
        QVERIFY(!configFile.exists());

        // owncloud config files exists
        configFile.findLegacyClientConfigFile();
        const auto legacyConfigFile = configFile.discoveredLegacyConfigFile();
        QVERIFY(!legacyConfigFile.isEmpty());
        QCOMPARE(legacyConfigFile, ocConfig);

        // TODO: add accounts and folders to AccountManager and FolderMan without UI interference
        //_folderMan.reset({});
        // _folderMan.reset(new FolderMan{});
        // create accounts and folders from a legacy desktop client or for a new config file
        // QVERIFY(AccountManager::instance()->restore(configFile.configFileToRestore()) != AccountManager::AccountsRestoreFailure);
        // QCOMPARE_GE(FolderMan::instance()->setupFoldersMigration(), 1);
        // QVERIFY(configFile.configFile().contains("nextcloud"));
        // QCOMPARE_GE(AccountManager::instance()->accounts().size(), 1);
        // auto accounts = AccountManager::instance()->accounts().first()->settings();
        // QCOMPARE_GE(accounts->childGroups().size(), 1);
        // accounts->beginGroup(QLatin1String("Folders"));
        // QCOMPARE_GE(accounts->childGroups().size(), 1);
        // accounts->endGroup();
    }

    // From branded oC client to branded Nextcloud
    void testMigrationFromBrandedOctoBrandedNextcloud()
    {
        QCoreApplication::setApplicationName(brandedAppName);
        setupStandardConfigFolder();

        // branded legacy have directory name in lower case
        QTemporaryDir tempDir;
        QVERIFY(QDir(tempDir.path()).mkpath(ocBrandedAppName));
        const auto ocBrandedConfigFolder = QString(tempDir.path() + "/" + ocBrandedAppName);
        const auto ocBrandedConfig = QString(ocBrandedConfigFolder + "/" + QString(ocBrandedAppName) + ".cfg");
        QFile::copy(_configFile.configFile(), QFileInfo(ocBrandedConfig).filePath());

        QFile ocBrandedConfigFile(ocBrandedConfig);
        QVERIFY(ocBrandedConfigFile.open(QFile::WriteOnly));
        QCOMPARE_GE(ocBrandedConfigFile.write(legacyAppConfigContent, qstrlen(legacyAppConfigContent)), 0);
        ocBrandedConfigFile.close();

        ConfigFile configFile;
        QVERIFY(QDir(tempDir.path()).mkpath(brandedAppName));
        const auto brandedConfigFolder = QString(tempDir.path() + "/" + brandedAppName);
        configFile.setConfDir(brandedConfigFolder);

        const auto path3 = _configFile.configFile();
        const auto path4 = configFile.configFile();

        // our branded config file does not exist
        QVERIFY(!configFile.exists());

        // branded owncloud config files exists
        configFile.findLegacyClientConfigFile();
        const auto legacyConfigFile = configFile.discoveredLegacyConfigFile();
        QVERIFY(!legacyConfigFile.isEmpty());
        QCOMPARE(legacyConfigFile, ocBrandedConfig);
    }


    // From the standard Nextcloud client to a branded version
    void testMigrationFromNextcloudToBranded()
    {
        setupStandardConfigFolder();

        QTemporaryDir tempDir;
        QVERIFY(QDir(tempDir.path()).mkpath(brandedAppName));
        const auto brandedConfigFolder = QString(tempDir.path() + "/" + brandedAppName);
        const auto brandedConfig = QString(brandedConfigFolder + "/" + QString(brandedAppName).toLower() + ".cfg");
        QFile::copy(_configFile.configFile(), QFileInfo(brandedConfig).filePath());

        ConfigFile configFile;
        QVERIFY(QDir(tempDir.path()).mkpath(standardAppName));
        const auto standardConfigFolder = QString(tempDir.path() + "/" + standardAppName);
        configFile.setConfDir(standardConfigFolder);

        // Nextcloud config file does not exist
        QVERIFY(!configFile.exists());

        // owncloud config files exists
        configFile.findLegacyClientConfigFile();
        const auto legacyConfigFile = configFile.discoveredLegacyConfigFile();
        QVERIFY(!legacyConfigFile.isEmpty());
        QCOMPARE(legacyConfigFile, brandedConfig);
    }

    // TODO: Downgrade
};

QTEST_GUILESS_MAIN(TestMigration)
#include "testmigration.moc"
