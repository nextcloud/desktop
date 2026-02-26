/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "application.h"

#include "account.h"
#include "accountmanager.h"
#include "accountsetupcommandlinemanager.h"
#include "accountstate.h"
#include "clientproxy.h"
#include "config.h"
#include "configfile.h"
#include "connectionvalidator.h"
#include "creds/abstractcredentials.h"
#include "e2efoldermanager.h"
#include "editlocallymanager.h"
#include "folder.h"
#include "folderman.h"
#include "logger.h"
#include "pushnotifications.h"
#include "socketapi/socketapi.h"
#include "theme.h"

#if defined(BUILD_UPDATER)
#include "updater/ocupdater.h"
#endif

#include "owncloudsetupwizard.h"
#include "version.h"
#include "csync_exclude.h"
#include "common/vfs.h"

#include "config.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#include "shellextensionsserver.h"
#elif defined(Q_OS_MACOS)
#include "macOS/fileprovider.h"
#endif

#include <QLocale>
#include <QTranslator>
#include <QMenu>
#include <QMessageBox>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QUrlQuery>
#include <QVersionNumber>
#include <QRandomGenerator>
#include <QHttp2Configuration>
#include <QPushButton>
#include <QAbstractButton>
#include <QFileOpenEvent>

#include <iostream>

class QSocket;

namespace OCC {

Q_LOGGING_CATEGORY(lcApplication, "nextcloud.gui.application", QtInfoMsg)

namespace {

    static const char optionsC[] =
        "Options:\n"
        "  --help, -h                 : show this help screen.\n"
        "  --version, -v              : show version information.\n"
        "  -q --quit                  : quit the running instance\n"
        "  --logwindow, -l            : open a window to show log output.\n"
        "  --logfile <filename>       : write log output to file <filename>.\n"
        "  --logdir <name>            : write each sync log output in a new file\n"
        "                               in folder <name>.\n"
        "  --logexpire <hours>        : removes logs older than <hours> hours.\n"
        "                               (to be used with --logdir)\n"
        "  --logflush                 : flush the log file after every write.\n"
        "  --logdebug                 : also output debug-level messages in the log.\n"
        "  --confdir <dirname>        : Use the given configuration folder.\n"
        "  --background               : launch the application in the background.\n"
        "  --overrideserverurl        : specify a server URL to use for the force override to be used in the account setup wizard.\n"
        "  --overridelocaldir         : specify a local dir to be used in the account setup wizard.\n"
        "  --userid                   : userId (username as on the server) to pass when creating an account via command-line.\n"
        "  --apppassword              : appPassword to pass when creating an account via command-line.\n"
        "  --set-language <lang>      : specify a language to use for the client, regardless of the OS language.\n"
        "  --localdirpath             : (optional) path where to create a local sync folder when creating an account via command-line.\n"
        "  --isvfsenabled             : whether to set a VFS or non-VFS folder (1 for 'yes' or 0 for 'no') when creating an account via command-line.\n"
        "  --remotedirpath            : (optional) path to a remote subfolder when creating an account via command-line.\n"
        "  --serverurl                : a server URL to use when creating an account via command-line.\n"
#if !DISABLE_ACCOUNT_MIGRATION
        "  --forcelegacyconfigimport  : forcefully import account configurations from legacy clients (if available).\n"
#endif
        "  --reverse            : use a reverse layout direction.\n";

    QString applicationTrPath()
    {
        QString devTrPath = qApp->applicationDirPath() + QString::fromLatin1("/../src/gui/");
        if (QDir(devTrPath).exists()) {
            // might miss Qt, QtKeyChain, etc.
            qCWarning(lcApplication) << "Running from build location! Translations may be incomplete!";
            return devTrPath;
        }
#if defined(Q_OS_WIN)
        return QApplication::applicationDirPath() + QLatin1String("/i18n/");
#elif defined(Q_OS_MACOS)
        return QApplication::applicationDirPath() + QLatin1String("/../Resources/Translations"); // path defaults to app dir.
#elif defined(Q_OS_UNIX)
        if (qEnvironmentVariableIsSet("APPIMAGE")) {
            return QApplication::applicationDirPath() + QLatin1String("/../share/" APPLICATION_EXECUTABLE "/i18n/");
        } else {
            return QString::fromLatin1(SHAREDIR "/" APPLICATION_EXECUTABLE "/i18n/");
        }
#endif
    }
}

// ----------------------------------------------------------------------------------

bool Application::configVersionMigration()
{
    ConfigFile configFile;
    const auto shouldTryToMigrate = configFile.shouldTryToMigrate();
    if (!shouldTryToMigrate) {
        qCInfo(lcApplication) << "This is not an upgrade/downgrade/migration. Proceed to read current application config file.";
        configFile.setMigrationPhase(ConfigFile::MigrationPhase::Done);
        return false;
    }

    configFile.setMigrationPhase(ConfigFile::MigrationPhase::SetupConfigFile);
    QStringList deleteKeys, ignoreKeys;
    AccountManager::backwardMigrationSettingsKeys(&deleteKeys, &ignoreKeys);
    FolderMan::backwardMigrationSettingsKeys(&deleteKeys, &ignoreKeys);
    configFile.setClientPreviousVersionString(configFile.clientVersionString());

    qCDebug(lcApplication) << "Migration is in progress:"  << configFile.isMigrationInProgress();
    const auto versionChanged = configFile.hasVersionChanged();
    if (versionChanged) {
        qCInfo(lcApplication) << "Version changed. Removing updater settings from config.";
        configFile.cleanUpdaterConfiguration();
    }

    if (!versionChanged && !(!deleteKeys.isEmpty() || (!ignoreKeys.isEmpty() && versionChanged))) {
        return true;
    }

    // 'Launch on system startup' defaults to true > 3.11.x
    const auto theme = Theme::instance();
    configFile.setLaunchOnSystemStartup(configFile.launchOnSystemStartup());
    Utility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), configFile.launchOnSystemStartup());

    // default is now off to displaying dialog warning user of too many files deletion
    configFile.setPromptDeleteFiles(false);

    // back up all old config files
    QStringList backupFilesList;
    QDir configDir(configFile.configPath());
    const auto anyConfigFileNameList = configDir.entryInfoList({"*.cfg"}, QDir::Files);
    for (const auto &oldConfig : anyConfigFileNameList) {
        const auto oldConfigFileName = oldConfig.fileName();
        const auto oldConfigFilePath = oldConfig.filePath();
        const auto newConfigFileName = configFile.configFile();
        backupFilesList.append(configFile.backup(oldConfigFileName));
        if (oldConfigFilePath != newConfigFileName) {
            if (!QFile::rename(oldConfigFilePath, newConfigFileName)) {
                qCWarning(lcApplication) << "Failed to rename configuration file from" << oldConfigFilePath << "to" << newConfigFileName;
            }
        }
    }

    // We want to message the user either for destructive changes,
    // or if we're ignoring something and the client version changed.
    if (configFile.showConfigBackupWarning() && backupFilesList.count() > 0) {
        QMessageBox box(
            QMessageBox::Warning,
            APPLICATION_SHORTNAME,
            tr("Some settings were configured in %1 versions of this client and "
               "use features that are not available in this version.<br>"
               "<br>"
               "Continuing will mean <b>%2 these settings</b>.<br>"
               "<br>"
               "The current configuration file was already backed up to <i>%3</i>.")
                .arg((configFile.isDowngrade() ? tr("newer", "newer software version") : tr("older", "older software version")),
                     deleteKeys.isEmpty()? tr("ignoring") : tr("deleting"),
                     backupFilesList.join("<br>")));
        box.addButton(tr("Quit"), QMessageBox::AcceptRole);
        auto continueBtn = box.addButton(tr("Continue"), QMessageBox::DestructiveRole);

        box.exec();
        if (box.clickedButton() != continueBtn) {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            return false;
        }
    }

    if (!deleteKeys.isEmpty()) {
        auto settings = ConfigFile::settingsWithGroup("foo");
        settings->endGroup();

        // Wipe confusing keys from the future, ignore the others
        for (const auto &badKey : std::as_const(deleteKeys)) {
            settings->remove(badKey);
            qCInfo(lcApplication) << "Migration: removed" << badKey << "key from settings.";
        }
    }

    configFile.setClientPreviousVersionString(configFile.clientVersionString());
    configFile.setClientVersionString(MIRALL_VERSION_STRING);
    return true;
}

ownCloudGui *Application::gui() const
{
    return _gui;
}

Application::Application(int &argc, char **argv)
    : QApplication{argc, argv}
    , _gui(nullptr)
    , _theme(Theme::instance())
{
    _startedAt.start();

#ifdef Q_OS_WIN
    // Ensure OpenSSL config file is only loaded from app directory
    QString opensslConf = QCoreApplication::applicationDirPath() + QStringLiteral("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());

    const auto shouldDisableGraphicsAcceleration = [&]() {
        if (qEnvironmentVariableIsSet("VMWARE")) {
            return true;
        }

        if (qEnvironmentVariableIsSet("SESSIONNAME") && qEnvironmentVariable("SESSIONNAME").startsWith("RDP-")) {
            return true;
        }

        return false;
    };

    if (shouldDisableGraphicsAcceleration()) {
        qputenv("SVGA_ALLOW_LLVMPIPE", 0);
        qCInfo(lcApplication) << "Disabling graphics acceleration, application might be running in a virtual or in a remote desktop.";
    }
#endif

    // TODO: Can't set this without breaking current config paths
    //    setOrganizationName(QLatin1String(APPLICATION_VENDOR));
    setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));

    setDesktopFileName(QString(LINUX_APPLICATION_ID));

    setApplicationName(_theme->appName());
    setWindowIcon(_theme->applicationIcon());

    if (!ConfigFile().exists()) {
        setApplicationName(_theme->appNameGUI());
        QString legacyDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/" + APPLICATION_CONFIG_NAME;

        if (legacyDir.endsWith('/')) {
            legacyDir.chop(1); // macOS 10.11.x does not like trailing slash for rename/move.
        }
        setApplicationName(_theme->appName());
        if (QFileInfo(legacyDir).isDir()) {
            auto confDir = ConfigFile().configPath();

            // macOS 10.11.x does not like trailing slash for rename/move.
            if (confDir.endsWith('/')) {
                confDir.chop(1);
            }

            qCInfo(lcApplication) << "Migrating old config from" << legacyDir << "to" << confDir;

            if (!QFile::rename(legacyDir, confDir)) {
                qCWarning(lcApplication) << "Failed to move the old config directory to its new location (" << legacyDir << "to" << confDir << ")";

                // Try to move the files one by one
                if (QFileInfo(confDir).isDir() || QDir().mkdir(confDir)) {
                    const QStringList filesList = QDir(legacyDir).entryList(QDir::Files);
                    qCInfo(lcApplication) << "Will move the individual files" << filesList;
                    for (const auto &name : filesList) {
                        if (!QFile::rename(legacyDir + "/" + name,  confDir + "/" + name)) {
                            qCWarning(lcApplication) << "Fallback move of " << name << "also failed";
                        }
                    }
                }
            } else {
#ifndef Q_OS_WIN
                // Create a symbolic link so a downgrade of the client would still find the config.
                QFile::link(confDir, legacyDir);
#endif
            }
        }
    } else {
        setupConfigFile();
    }

    if (_theme->doNotUseProxy()) {
        ConfigFile().setProxyType(QNetworkProxy::NoProxy);
        const auto &allAccounts = AccountManager::instance()->accounts();
        for (const auto &accountState : allAccounts) {
            if (accountState && accountState->account()) {
                accountState->account()->setProxyType(QNetworkProxy::NoProxy);
            }
        }
    }

    parseOptions(arguments());
    //no need to waste time;
    if (_helpOnly || _versionOnly) {
        return;
    }

    if (_quitInstance) {
        QTimer::singleShot(0, qApp, &QApplication::quit);
        return;
    }

    if (!_singleApp.isPrimaryInstance()) {
        return;
    }

    setupLogging();
    setupTranslations();

    // try to migrate legacy accounts and folders from a previous client version
    // only copy the settings and check what should be skipped
    if (!configVersionMigration()) {
        qCWarning(lcApplication) << "Config version migration was not possible.";
    }

    ConfigFile cfg;
    {
        auto shouldExit = false;

        // these config values will always be empty after the first client run
        if (!_overrideServerUrl.isEmpty()) {
            cfg.setOverrideServerUrl(_overrideServerUrl);
            shouldExit = true;
        }

        if (!_overrideLocalDir.isEmpty()) {
            cfg.setOverrideLocalDir(_overrideLocalDir);
            shouldExit = true;
        }

        if (!_setLanguage.isNull()) {
            // checking for .isNull here as setting the value to an empty string will use the OS language again
            cfg.setLanguage(_setLanguage);
            shouldExit = true;
        }

        if (AccountSetupCommandLineManager::instance()) {
            cfg.setVfsEnabled(AccountSetupCommandLineManager::instance()->isVfsEnabled());
        }

        if (shouldExit) {
            std::exit(0);
        }
    }

    // The timeout is initialized with an environment variable, if not, override with the value from the config
    if (!AbstractNetworkJob::httpTimeout) {
        AbstractNetworkJob::httpTimeout = cfg.timeout();
    }

    // Check vfs plugins
    if (Theme::instance()->showVirtualFilesOption() && bestAvailableVfsMode() == Vfs::Off) {
        qCWarning(lcApplication) << "Theme wants to show vfs mode, but no vfs plugins are available";
    }

    if (isVfsPluginAvailable(Vfs::WindowsCfApi)) {
        qCInfo(lcApplication) << "VFS windows plugin is available";
    }

    if (isVfsPluginAvailable(Vfs::WithSuffix)) {
        qCInfo(lcApplication) << "VFS suffix plugin is available";
    }

    _theme->setSystrayUseMonoIcons(ConfigFile().monoIcons());
    connect(this, &Application::systemPaletteChanged,
            _theme, &Theme::systemPaletteHasChanged);

#if defined(Q_OS_WIN)
    _shellExtensionsServer.reset(new ShellExtensionsServer);
#endif

    connect(&_singleApp, &KDSingleApplication::messageReceived, this, &Application::slotParseMessage);

    // create accounts and folders from a legacy desktop client or from the current config file
    setupAccountsAndFolders();

    setQuitOnLastWindowClosed(false);

    // Setting up the gui class will allow tray notifications for the
    // setup that follows, like folder setup
    _gui = new ownCloudGui(this);
    connect(_theme, &Theme::systrayUseMonoIconsChanged, _gui, &ownCloudGui::slotComputeOverallSyncStatus);
    if (_showLogWindow) {
        _gui->slotToggleLogBrowser(); // _showLogWindow is set in parseOptions.
    }

#if WITH_LIBCLOUDPROVIDERS
    _gui->setupCloudProviders();
#endif

    if (_theme->doNotUseProxy()) {
        ConfigFile().setProxyType(QNetworkProxy::NoProxy);
        const auto &allAccounts = AccountManager::instance()->accounts();
        for (const auto &accountState : allAccounts) {
            if (accountState && accountState->account()) {
                accountState->account()->setProxyType(QNetworkProxy::NoProxy);
            }
        }
    }

    _proxy.setupQtProxyFromConfig(); // folders have to be defined first, than we set up the Qt proxy.

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &Application::slotAccountStateAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &Application::slotAccountStateRemoved);
    const auto accounts = AccountManager::instance()->accounts();
    for (const auto &ai : accounts) {
        slotAccountStateAdded(ai.data());
    }

    connect(FolderMan::instance()->socketApi(), &SocketApi::shareCommandReceived,
        _gui.data(), &ownCloudGui::slotShowShareDialog);

    connect(FolderMan::instance()->socketApi(), &SocketApi::fileActivityCommandReceived,
        _gui.data(), &ownCloudGui::slotShowFileActivityDialog);

    connect(FolderMan::instance()->socketApi(), &SocketApi::fileActionsCommandReceived,
            _gui.data(), &ownCloudGui::slotShowFileActionsDialog);

    // startup procedure.
    connect(&_checkConnectionTimer, &QTimer::timeout, this, &Application::slotCheckConnection);
    _checkConnectionTimer.setInterval(ConnectionValidator::DefaultCallingIntervalMsec); // check for connection every 32 seconds.
    _checkConnectionTimer.start();
    // Also check immediately
    QTimer::singleShot(0, this, &Application::slotCheckConnection);

    // Can't use onlineStateChanged because it is always true on modern systems because of many interfaces
    connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged,
        this, &Application::slotSystemOnlineConfigurationChanged);

#if defined(BUILD_UPDATER)
    // Update checks
    auto *updaterScheduler = new UpdaterScheduler(this);
    connect(updaterScheduler, &UpdaterScheduler::updaterAnnouncement,
        _gui.data(), &ownCloudGui::slotShowTrayUpdateMessage);
    connect(updaterScheduler, &UpdaterScheduler::requestRestart,
        _folderManager.data(), &FolderMan::slotScheduleAppRestart);
#endif

    // Cleanup at Quit.
    connect(this, &QCoreApplication::aboutToQuit, this, &Application::slotCleanup);

    // Allow other classes to hook into isShowingSettingsDialog() signals (re-auth widgets, for example)
    connect(_gui.data(), &ownCloudGui::isShowingSettingsDialog, this, &Application::slotGuiIsShowingSettings);

    _gui->createTray();

    handleEditLocallyFromOptions();

    if (AccountSetupCommandLineManager::instance()->isCommandLineParsed()) {
        AccountSetupCommandLineManager::instance()->setupAccountFromCommandLine();
    }
    AccountSetupCommandLineManager::destroy();

#if defined(BUILD_FILE_PROVIDER_MODULE)
    Mac::FileProvider::instance();
#endif
}

Application::~Application()
{
    // Make sure all folders are gone, otherwise removing the
    // accounts will remove the associated folders from the settings.
    if (_folderManager) {
        _folderManager->unloadAndDeleteAllFolders();
    }

    // Remove the account from the account manager so it can be deleted.
    disconnect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &Application::slotAccountStateRemoved);
    AccountManager::instance()->shutdown();
}

void Application::setupAccountsAndFolders()
{
    _folderManager.reset(new FolderMan);
    ConfigFile configFile;
    configFile.setMigrationPhase(ConfigFile::MigrationPhase::SetupUsers);
    const auto accountsRestoreResult = restoreLegacyAccount();
    if (accountsRestoreResult == AccountManager::AccountsNotFound || accountsRestoreResult == AccountManager::AccountsRestoreFailure) {
        qCWarning(lcApplication) << "Migration result: " << accountsRestoreResult;
        qCDebug(lcApplication) << "is migration disabled?" << DISABLE_ACCOUNT_MIGRATION;
        qCWarning(lcApplication) << "No accounts were migrated, prompting user to set up accounts and folders from scratch.";
        configFile.setMigrationPhase(ConfigFile::MigrationPhase::Done);

        return;
    }

    configFile.setMigrationPhase(ConfigFile::MigrationPhase::SetupFolders);
    const auto foldersListSize = FolderMan::instance()->setupFolders();
    FolderMan::instance()->setSyncEnabled(true);

    // Initialize E2E folder restoration manager
    E2EFolderManager::instance()->initialize();

    const auto prettyNamesList = [](const QList<AccountStatePtr> &accounts) {
        QStringList list;
        for (const auto &account : accounts) {
            list << account->account()->prettyName().prepend("- ");
        }
        return list.join("\n");
    };

    const auto accounts = AccountManager::instance()->accounts();
    const auto accountsListSize = accounts.size();
    if (accountsRestoreResult == AccountManager::AccountsRestoreSuccessFromLegacyVersion
        && Theme::instance()->displayLegacyImportDialog()
        && !AccountManager::instance()->forceLegacyImport()
        && accountsListSize > 0) {
        const auto accountsRestoreMessage = accountsListSize > 1
            ? tr("%1 accounts", "number of accounts imported").arg(QString::number(accountsListSize))
            : tr("1 account");
        const auto foldersRestoreMessage = foldersListSize > 1
            ? tr("%1 folders", "number of folders imported").arg(QString::number(foldersListSize))
            : tr("1 folder");
        const auto messageBox = new QMessageBox(QMessageBox::Information,
                                                tr("Legacy import"),
                                                tr("Imported %1 and %2 from a legacy desktop client.\n%3",
                                                   "number of accounts and folders imported. list of users.")
                                                    .arg(accountsRestoreMessage,
                                                         foldersRestoreMessage,
                                                         prettyNamesList(accounts))
                                                );
        messageBox->setWindowModality(Qt::NonModal);
        messageBox->open();
    }

    qCWarning(lcApplication) << "Account(s) setup result:" << accountsRestoreResult;
    qCWarning(lcApplication) << foldersListSize << "folder(s) migrated";
    qCWarning(lcApplication) << accountsListSize << "account(s) migrated:" << prettyNamesList(accounts);
}

void Application::setupConfigFile()
{
    // Migrate from version <= 2.4
    setApplicationName(_theme->appNameGUI());
#ifndef QT_WARNING_DISABLE_DEPRECATED // Was added in Qt 5.9
    #define QT_WARNING_DISABLE_DEPRECATED QT_WARNING_DISABLE_GCC("-Wdeprecated-declarations")
#endif
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QT_WARNING_POP
    setApplicationName(_theme->appName());

    auto oldDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // macOS 10.11.x does not like trailing slash for rename/move.
    if (oldDir.endsWith('/')) {
        oldDir.chop(1);
    }

    if (!QFileInfo(oldDir).isDir()) {
        return;
    }

    auto confDir = ConfigFile().configPath();

    // macOS 10.11.x does not like trailing slash for rename/move.
    if (confDir.endsWith('/')) {
        confDir.chop(1);
    }

    qCInfo(lcApplication) << "Migrating old config from" << oldDir << "to" << confDir;
    if (!QFile::rename(oldDir, confDir)) {
        qCWarning(lcApplication) << "Failed to move the old config directory to its new location (" << oldDir << "to" << confDir << ")";

        // Try to move the files one by one
        if (QFileInfo(confDir).isDir() || QDir().mkdir(confDir)) {
            const QStringList filesList = QDir(oldDir).entryList(QDir::Files);
            qCInfo(lcApplication) << "Will move the individual files" << filesList;
            for (const auto &name : filesList) {
                if (!QFile::rename(oldDir + "/" + name,  confDir + "/" + name)) {
                    qCWarning(lcApplication) << "Fallback move of " << name << "also failed";
                }
            }
        }
    } else {
#ifndef Q_OS_WIN
        // Create a symbolic link so a downgrade of the client would still find the config.
        QFile::link(confDir, oldDir);
#endif
    }
}

AccountManager::AccountsRestoreResult Application::restoreLegacyAccount()
{
    ConfigFile cfg;
    const auto tryMigrate = cfg.overrideServerUrl().isEmpty();
    auto accountsRestoreResult = AccountManager::AccountsRestoreFailure;
    if (accountsRestoreResult = AccountManager::instance()->restore(tryMigrate);
        accountsRestoreResult == AccountManager::AccountsRestoreFailure) {
        // If there is an error reading the account settings, try again
        // after a couple of seconds, if that fails, give up.
        // (non-existence is not an error)
        Utility::sleep(5);
        if (accountsRestoreResult = AccountManager::instance()->restore(tryMigrate);
            accountsRestoreResult == AccountManager::AccountsRestoreFailure) {
            qCCritical(lcApplication) << "Could not read the account settings, quitting";
            QMessageBox::critical(
                nullptr,
                tr("Error accessing the configuration file"),
                tr("There was an error while accessing the configuration "
                   "file at %1. Please make sure the file can be accessed by your system account.")
                    .arg(ConfigFile().configFile()),
                QMessageBox::Ok
            );
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }
    }
    return accountsRestoreResult;
}

void Application::slotAccountStateRemoved(AccountState *accountState)
{
    if (_gui) {
        disconnect(accountState, &AccountState::stateChanged,
            _gui.data(), &ownCloudGui::slotComputeOverallSyncStatus);
        disconnect(accountState->account().data(), &Account::serverVersionChanged,
            _gui.data(), &ownCloudGui::slotTrayMessageIfServerUnsupported);
    }
    if (_folderManager) {
        disconnect(accountState, &AccountState::stateChanged,
            _folderManager.data(), &FolderMan::slotAccountStateChanged);
        disconnect(accountState->account().data(), &Account::serverVersionChanged,
            _folderManager.data(), &FolderMan::slotServerVersionChanged);
    }

    // if there is no more account, show the wizard.
    if (_gui && AccountManager::instance()->accounts().isEmpty()) {
        // allow to add a new account if there is non any more. Always think
        // about single account theming!
        OwncloudSetupWizard::runWizard(this, SLOT(slotownCloudWizardDone(int)));
    }
}

void Application::slotAccountStateAdded(AccountState *accountState)
{
    connect(accountState, &AccountState::stateChanged,
        _gui.data(), &ownCloudGui::slotComputeOverallSyncStatus);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _gui.data(), &ownCloudGui::slotTrayMessageIfServerUnsupported);
    connect(accountState, &AccountState::termsOfServiceChanged,
            _gui.data(), &ownCloudGui::slotNeedToAcceptTermsOfService);
    connect(accountState, &AccountState::stateChanged,
        _folderManager.data(), &FolderMan::slotAccountStateChanged);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _folderManager.data(), &FolderMan::slotServerVersionChanged);

    _gui->slotTrayMessageIfServerUnsupported(accountState->account());
}

void Application::slotCleanup()
{
    AccountManager::instance()->save();
    FolderMan::instance()->unloadAndDeleteAllFolders();

    _gui->slotShutdown();
    _gui->deleteLater();
}

// FIXME: This is not ideal yet since a ConnectionValidator might already be running and is in
// progress of timing out in some seconds.
// Maybe we need 2 validators, one triggered by timer, one by network configuration changes?
void Application::slotSystemOnlineConfigurationChanged()
{
    if (QNetworkInformation::instance()->reachability() == QNetworkInformation::Reachability::Site ||
            QNetworkInformation::instance()->reachability() == QNetworkInformation::Reachability::Online) {
        const auto list = AccountManager::instance()->accounts();
        for (const auto &accountState : list) {
            accountState->systemOnlineConfigurationChanged();
        }
    }
}

void Application::slotCheckConnection()
{
    if (AccountManager::instance()->accounts().isEmpty()) {
        // let gui open the setup wizard
        _gui->slotOpenSettingsDialog();

        _checkConnectionTimer.stop(); // don't popup the wizard on interval;
    }
}

void Application::slotCrash()
{
    Utility::crash();
}

void Application::slotownCloudWizardDone(int res)
{
    FolderMan *folderMan = FolderMan::instance();

    // During the wizard, scheduling of new syncs is disabled
    folderMan->setSyncEnabled(true);

    if (res == QDialog::Accepted) {
        // Check connectivity of the newly created account
        _checkConnectionTimer.start();
        slotCheckConnection();

        Utility::setLaunchOnStartup(_theme->appName(), _theme->appNameGUI(), true);

        Systray::instance()->showWindow();
    }
}

void Application::setupLogging()
{
    // might be called from second instance
    auto logger = Logger::instance();
    logger->setLogFile(_logFile);
    if (_logFile.isEmpty()) {
        logger->setLogDir(_logDir.isEmpty() ? ConfigFile().logDir() : _logDir);
    }
    logger->setLogExpire(_logExpire > 0 ? _logExpire : ConfigFile().logExpire());
#if defined NEXTCLOUD_DEV && NEXTCLOUD_DEV
    logger->setLogFlush(true);
    logger->setLogDebug(true);
#else
    logger->setLogFlush(_logFlush || ConfigFile().logFlush());
    logger->setLogDebug(_logDebug || ConfigFile().logDebug());
#endif
    if (!logger->isLoggingToFile() && ConfigFile().automaticLogDir()) {
        logger->setupTemporaryFolderLogDir();
    }

#if defined QT_DEBUG
    logger->setLogFlush(true);
    logger->setLogDebug(true);
#endif

    logger->enterNextLogFile(QStringLiteral("nextcloud.log"), OCC::Logger::LogType::Log);
    logger->enterNextLogFile(QStringLiteral("permanent_delete.log"), OCC::Logger::LogType::DeleteLog);

    qCInfo(lcApplication) << "##################" << _theme->appName()
                          << "locale:" << QLocale::system().name()
                          << "ui_lang:" << property("ui_lang")
                          << "version:" << _theme->version()
                          << "os:" << Utility::platformName();
    qCInfo(lcApplication) << "Arguments:" << qApp->arguments();
}

void Application::slotParseMessage(const QByteArray &message)
{
    const auto msg = QString::fromLatin1(message);
    if (msg.startsWith(QLatin1String("MSG_PARSEOPTIONS:"))) {
        const int lengthOfMsgPrefix = 17;
        const auto options = msg.mid(lengthOfMsgPrefix).split(QLatin1Char{'|'});
        _showLogWindow = false;
        parseOptions(options);
        setupLogging();
        if (_showLogWindow) {
            _gui->slotToggleLogBrowser(); // _showLogWindow is set in parseOptions.
        }
        if (_quitInstance) {
            qApp->quit();
        }

        handleEditLocallyFromOptions();

        if (AccountSetupCommandLineManager::instance()->isCommandLineParsed()) {
            AccountSetupCommandLineManager::instance()->setupAccountFromCommandLine();
        }
        AccountSetupCommandLineManager::destroy();

    } else if (msg.startsWith(QLatin1String("MSG_SHOWMAINDIALOG"))) {
        qCInfo(lcApplication) << "Running for" << _startedAt.elapsed() / 1000.0 << "sec";
        if (_startedAt.elapsed() < 10 * 1000) {
            // This call is mirrored with the one in int main()
            qCWarning(lcApplication) << "Ignoring MSG_SHOWMAINDIALOG, possibly double-invocation of client via session restore and auto start";
            return;
        }

        // Show the main dialog only if there is at least one account configured
        if (!AccountManager::instance()->accounts().isEmpty()) {
            showMainDialog();
        } else {
            _gui->slotNewAccountWizard();
        }
    }
}

void Application::parseOptions(const QStringList &options)
{
    QStringListIterator it(options);
    // skip file name;
    if (it.hasNext()) {
        it.next();
    }

    bool shouldExit = false;

    //parse options; if help or bad option exit
    while (it.hasNext()) {
        QString option = it.next();
        if (option == QLatin1String("--help") || option == QLatin1String("-h")) {
            setHelp();
            break;
        } else if (option == QLatin1String("--quit") || option == QLatin1String("-q")) {
            _quitInstance = true;
        } else if (option == QLatin1String("--logwindow") || option == QLatin1String("-l")) {
            _showLogWindow = true;
        } else if (option == QLatin1String("--logfile")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                _logFile = it.next();
            } else {
                showHint("Log file not specified");
            }
        } else if (option == QLatin1String("--logdir")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                _logDir = it.next();
            } else {
                showHint("Log dir not specified");
            }
        } else if (option == QLatin1String("--logexpire")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                _logExpire = it.next().toInt();
            } else {
                showHint("Log expiration not specified");
            }
        } else if (option == QLatin1String("--logflush")) {
            _logFlush = true;
        } else if (option == QLatin1String("--logdebug")) {
            _logDebug = true;
        } else if (option == QLatin1String("--confdir")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                QString confDir = it.next();
                if (!ConfigFile::setConfDir(confDir)) {
                    showHint("Invalid path passed to --confdir");
                }
            } else {
                showHint("Path for confdir not specified");
            }
        } else if (option == QLatin1String("--debug")) {
            _logDebug = true;
            _debugMode = true;
        } else if (option == QLatin1String("--background")) {
            _backgroundMode = true;
        } else if (option == QLatin1String("--version") || option == QLatin1String("-v")) {
            _versionOnly = true;
        } else if (option == QLatin1String("--reverse")) {
            setLayoutDirection(layoutDirection() == Qt::LeftToRight ? Qt::RightToLeft : Qt::LeftToRight);
        } else if (option.endsWith(QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX))) {
            // virtual file, open it after the Folder were created (if the app is not terminated)
            QTimer::singleShot(0, this, [this, option] { openVirtualFile(option); });
        } else if (option.startsWith(QStringLiteral(APPLICATION_URI_HANDLER_SCHEME "://open"))) {
            // see the section Local file editing of the Architecture page of the user documentation
            _editFileLocallyUrl = QUrl::fromUserInput(option);
            if (!_editFileLocallyUrl.isValid()) {
                _editFileLocallyUrl.clear();
                const auto errorParsingLocalFileEditingUrl = QStringLiteral("The supplied url for local file editing '%1' is invalid!").arg(option);
                qCInfo(lcApplication) << errorParsingLocalFileEditingUrl;
                showHint(errorParsingLocalFileEditingUrl.toStdString());
            }
        } else if (option == QStringLiteral("--overrideserverurl")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                const auto overrideUrl = it.next();
                const auto isUrlValid = (overrideUrl.startsWith(QStringLiteral("http://")) || overrideUrl.startsWith(QStringLiteral("https://")))
                    && QUrl::fromUserInput(overrideUrl).isValid();
                if (!isUrlValid) {
                    showHint("Invalid URL passed to --overrideserverurl");
                    shouldExit = true;
                } else {
                    _overrideServerUrl = overrideUrl;
                }
            } else {
                showHint("Invalid URL passed to --overrideserverurl");
            }
        } else if (option == QStringLiteral("--overridelocaldir")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                _overrideLocalDir = it.next();
            } else {
                showHint("Invalid URL passed to --overridelocaldir");
            }
        } else if (option == QStringLiteral("--set-language")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                _setLanguage = it.next();
            }
        }
#if !DISABLE_ACCOUNT_MIGRATION
        else if (option == QStringLiteral("--forcelegacyconfigimport")) {
            AccountManager::instance()->setForceLegacyImport(true);
        }
#endif
        else {
            QString errorMessage;
            if (!AccountSetupCommandLineManager::instance()->parseCommandlineOption(option, it, errorMessage)) {
                if (!errorMessage.isEmpty()) {
                    showHint(errorMessage.toStdString());
                    return;
                }
                showHint("Unrecognized option '" + option.toStdString() + "'");
            }
        }
    }
    if (shouldExit) {
        std::exit(0);
    }
}

// Helpers for displaying messages. Note that there is no console on Windows.
#ifdef Q_OS_WIN
// Format as <pre> HTML
static inline void toHtml(QString &t)
{
    t.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    t.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    t.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    t.insert(0, QLatin1String("<html><pre>"));
    t.append(QLatin1String("</pre></html>"));
}

static void displayHelpText(QString t) // No console on Windows.
{
    toHtml(t);
    QMessageBox::information(nullptr, Theme::instance()->appNameGUI(), t);
}

#else

static void displayHelpText(const QString &t)
{
    std::cout << qUtf8Printable(t);
}
#endif

void Application::showHelp()
{
    setHelp();
    QString helpText;
    QTextStream stream(&helpText);
    stream << _theme->appName()
           << QLatin1String(" version ")
           << _theme->version() << Qt::endl;

    stream << QLatin1String("File synchronisation desktop utility.") << Qt::endl
           << Qt::endl
           << QLatin1String(optionsC);

    if (_theme->appName() == QLatin1String("ownCloud"))
        stream << Qt::endl
               << "For more information, see http://www.owncloud.org" << Qt::endl
               << Qt::endl;

    displayHelpText(helpText);
}

void Application::showVersion()
{
    displayHelpText(Theme::instance()->versionSwitchOutput());
}

bool Application::isRunning() const
{
    return !_singleApp.isPrimaryInstance();
}

bool Application::sendMessage(const QString &message)
{
    return _singleApp.sendMessage(message.toLatin1());
}

void Application::showHint(std::string errorHint)
{
    static QString binName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    std::cerr << errorHint << std::endl;
    std::cerr << "Try '" << binName.toStdString() << " --help' for more information" << std::endl;
    std::exit(1);
}

bool Application::debugMode()
{
    return _debugMode;
}

bool Application::backgroundMode() const
{
    return _backgroundMode;
}

void Application::setHelp()
{
    _helpOnly = true;
}

void Application::handleEditLocallyFromOptions()
{
    if (!_editFileLocallyUrl.isValid()) {
        return;
    }

    EditLocallyManager::instance()->handleRequest(_editFileLocallyUrl);
    _editFileLocallyUrl.clear();
}

QString enforcedLanguage()
{
    const ConfigFile cfg;
    const auto configLanguage = cfg.language();
    if (!configLanguage.isEmpty()) {
        // always prefer value from configuration
        return configLanguage;
    }

    return Theme::instance()->enforcedLocale();
}

void Application::setupTranslations()
{
    auto translator = std::make_unique<QTranslator>();
    auto qtTranslator = std::make_unique<QTranslator>();
    auto qtkeychainTranslator = std::make_unique<QTranslator>();

    const auto trPath = applicationTrPath();
    const auto trFolder = QDir{trPath};
    if (!trFolder.exists()) {
        qCWarning(lcApplication()) << trPath << "folder containing translations is missing. Impossible to load translations";
        return;
    }

    qCInfo(lcApplication) << "System UI languages are:" << QLocale::system().uiLanguages();
    auto choosenLanguage = enforcedLanguage();
    if (choosenLanguage.isEmpty()) {
        for(const auto &localeToTest : QLocale::system().uiLanguages(QLocale::TagSeparator::Underscore)){
            const auto trFile = QString{QLatin1String{"client_"} + localeToTest};
            qCDebug(lcApplication()) << "trying to load" << localeToTest << "in" << trFile << "from" << trPath;
            if (translator->load(trFile, trPath)) {
                choosenLanguage = localeToTest;
                break;
            }
        }
    } else {
        const QString trFile = QLatin1String("client_") + choosenLanguage;
        qCDebug(lcApplication()) << "trying to load" << choosenLanguage << "in" << trFile << "from" << trPath;
        static_cast<void>(translator->load(trFile, trPath));
    }

    qCInfo(lcApplication) << "selected application language:" << choosenLanguage;
    if (!translator->isEmpty() || choosenLanguage.startsWith(QLatin1String("en"))) {
        // Permissive approach: Qt and keychain translations
        // may be missing, but Qt translations must be there in order
        // for us to accept the language. Otherwise, we try with the next.
        // "en" is an exception as it is the default language and may not
        // have a translation file provided.
        qCInfo(lcApplication) << "Using" << choosenLanguage << "translation";
        setProperty("ui_lang", choosenLanguage);
        const QString qtTrPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        const QString qtTrFile = QLatin1String("qt_") + choosenLanguage;
        const QString qtBaseTrFile = QLatin1String("qtbase_") + choosenLanguage;
        if (!qtTranslator->load(qtTrFile, qtTrPath)) {
            if (!qtTranslator->load(qtTrFile, trPath)) {
                if (!qtTranslator->load(qtBaseTrFile, qtTrPath)) {
                    if (!qtTranslator->load(qtBaseTrFile, trPath)) {
                        qCDebug(lcApplication()) << "impossible to load Qt translation catalog" << qtBaseTrFile;
                    }
                }
            }
        }
        const QString qtkeychainTrFile = QLatin1String("qtkeychain_") + choosenLanguage;
        if (!qtkeychainTranslator->load(qtkeychainTrFile, qtTrPath)) {
            if (!qtkeychainTranslator->load(qtkeychainTrFile, trPath)) {
                qCDebug(lcApplication()) << "impossible to load QtKeychain translation catalog" << qtkeychainTrFile;
            }
        }
        if (!translator->isEmpty()) {
            translator->setParent(this);
            installTranslator(translator.release());
        }

        if (!qtTranslator->isEmpty()) {
            qtTranslator->setParent(this);
            installTranslator(qtTranslator.release());
        }

        if (!qtkeychainTranslator->isEmpty()) {
            qtkeychainTranslator->setParent(this);
            installTranslator(qtkeychainTranslator.release());
        }
    } else {
        qCWarning(lcApplication()) << "translation catalog failed to load";
        const auto folderContent = trFolder.entryList();
        qCDebug(lcApplication()) << "folder content" << folderContent.join(QStringLiteral(", "));
    }
    if (property("ui_lang").isNull()) {
        setProperty("ui_lang", "C");
    }
}

bool Application::giveHelp()
{
    return _helpOnly;
}

bool Application::versionOnly()
{
    return _versionOnly;
}

void Application::showMainDialog()
{
    _gui->slotOpenMainDialog();
}

void Application::slotGuiIsShowingSettings()
{
    emit isShowingSettingsDialog();
}

void Application::openVirtualFile(const QString &filename)
{
    QString virtualFileExt = QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX);
    if (!filename.endsWith(virtualFileExt)) {
        qWarning(lcApplication) << "Can only handle file ending in .owncloud. Unable to open" << filename;
        return;
    }
    auto folder = FolderMan::instance()->folderForPath(filename);
    if (!folder) {
        qWarning(lcApplication) << "Can't find sync folder for" << filename;
        // TODO: show a QMessageBox for errors
        return;
    }
    QString relativePath = QDir::cleanPath(filename).mid(folder->cleanPath().length() + 1);
    folder->implicitlyHydrateFile(relativePath);
    QString normalName = filename.left(filename.size() - virtualFileExt.size());
    auto con = QSharedPointer<QMetaObject::Connection>::create();
    *con = connect(folder, &Folder::syncFinished, folder, [folder, con, normalName] {
        folder->disconnect(*con);
        if (QFile::exists(normalName)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(normalName));
        }
    });
}

void Application::tryTrayAgain()
{
    qCInfo(lcApplication) << "Trying tray icon, tray available:" << QSystemTrayIcon::isSystemTrayAvailable();
    _gui->hideAndShowTray();
}

bool Application::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        const auto openEvent = dynamic_cast<QFileOpenEvent *>(event);
        qCDebug(lcApplication) << "macOS: Received a QFileOpenEvent";

        if(!openEvent->file().isEmpty()) {
            qCDebug(lcApplication) << "QFileOpenEvent" << openEvent->file();
            // virtual file, open it after the Folder were created (if the app is not terminated)
            const auto fn = openEvent->file();
            QTimer::singleShot(0, this, [this, fn] { openVirtualFile(fn); });
        } else if (!openEvent->url().isEmpty() && openEvent->url().isValid()) {
            // On macOS, Qt does not handle receiving a custom URI as it does on other systems (as an application argument).
            // Instead, it sends out a QFileOpenEvent. We therefore need custom handling for our URI handling on macOS.
            qCInfo(lcApplication) << "macOS: Opening local file for editing: " << openEvent->url();
            EditLocallyManager::instance()->handleRequest(openEvent->url());
        } else {
            const auto errorParsingLocalFileEditingUrl = QStringLiteral("The supplied url for local file editing '%1' is invalid!").arg(openEvent->url().toString());
            qCInfo(lcApplication) << errorParsingLocalFileEditingUrl;
            showHint(errorParsingLocalFileEditingUrl.toStdString());
        }
    } else if (event->type() == QEvent::ApplicationPaletteChange) {
        qCInfo(lcApplication) << "application palette changed";
        emit systemPaletteChanged();
    }
    return QGuiApplication::event(event);
}

} // namespace OCC
