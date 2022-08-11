/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "application.h"

#include <iostream>
#include <random>

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "clientproxy.h"
#include "common/asserts.h"
#include "common/version.h"
#include "common/vfs.h"
#include "configfile.h"
#include "connectionvalidator.h"
#include "creds/abstractcredentials.h"
#include "csync_exclude.h"
#include "folder.h"
#include "folderman.h"
#include "logbrowser.h"
#include "logger.h"
#include "settingsdialog.h"
#include "sharedialog.h"
#include "socketapi/socketapi.h"
#include "theme.h"
#include "translations.h"

#ifdef WITH_AUTO_UPDATER
#include "updater/ocupdater.h"
#endif

#include "config.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#if defined(WITH_CRASHREPORTER)
#include <libcrashreporter-handler/Handler.h>
#endif

#include <QCommandLineParser>
#include <QDir>
#include <QLibraryInfo>
#include <QMenu>
#include <QMessageBox>
#include <QScopeGuard>
#include <QTranslator>

#pragma push_macro("QT_DISABLE_DEPRECATED_BEFORE")
#undef QT_DISABLE_DEPRECATED_BEFORE
#define QT_DISABLE_DEPRECATED_BEFORE 0
#include <QDesktopServices>
#pragma pop_macro("QT_DISABLE_DEPRECATED_BEFORE")


class QSocket;

namespace {

void migrateConfigFile(const QCoreApplication *app)
{
    using namespace OCC;
    if (!ConfigFile::exists()) {
        // check whether an old config location must be migrated
        // we support multiple locations from old versions
        // these are worked on in-order to upgrade from version to version
        // the algorithm is the same for all these locations, thus we can use a loop
        // note that we try to migrate in descending order, i.e., we try to migrate from the last release, then from the release before, ...
        // this is done in order to avoid porting old configu
        const auto configLocationsToMigrate = [&app] {
            QStringList out;
            // note: this change is temporary to allow using QDesktopServices etc. to determine the paths
            // the application name was changed to
            auto scopeGuard = qScopeGuard([&app, oldApplicationName = app->applicationName()] {
                // reset to original value
                app->setApplicationName(oldApplicationName);
            });

            auto addLegacyLocation = [&out](const QString &path) {
                if (QFileInfo(path).isDir()) {
                    // macOS 10.11.x does not like trailing slash for rename/move.
                    out.append(Utility::stripTrailingSlash(path));
                }
            };

            QCoreApplication::setApplicationName(Theme::instance()->appNameGUI());

            // location used in versions from 2.5 to 2.8
            addLegacyLocation(QStandardPaths::writableLocation(Utility::isWindows() ? QStandardPaths::AppDataLocation : QStandardPaths::AppConfigLocation));

            // location used in versions <= 2.4
            // We need to use the deprecated QDesktopServices::storageLocation because of its Qt4 behavior of adding "data" to the path
            addLegacyLocation(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
            return out;
        }();

        // macOS 10.11.x does not like trailing slash for rename/move.
        const auto confDir = Utility::stripTrailingSlash(ConfigFile::configPath());
        for (auto &oldDir : configLocationsToMigrate) {
            qCInfo(lcApplication) << Q_FUNC_INFO << "Migrating old config from" << oldDir << "to" << confDir;

            if (!QFile::rename(oldDir, confDir)) {
                qCWarning(lcApplication) << Q_FUNC_INFO << "Failed to move the old config directory to its new location (" << oldDir << "to" << confDir << ")";

                // Try to move the files one by one
                if (QFileInfo(confDir).isDir() || QDir().mkpath(confDir)) {
                    const auto filesList = QDir(oldDir).entryInfoList(QDir::Files);
                    qCInfo(lcApplication) << Q_FUNC_INFO << "Will move the individual files" << filesList;
                    for (const auto &fileInfo : filesList) {
                        if (!QFile::rename(fileInfo.canonicalFilePath(), confDir + QLatin1Char('/') + fileInfo.fileName())) {
                            qCWarning(lcApplication) << Q_FUNC_INFO << "Fallback move of " << fileInfo.fileName() << "also failed";
                        } else {
                            // we found a suitable config directory to migrate, hence we can stop here
                            // if we continued to run, we would try to overwrite the working migration
                            break;
                        }
                    }
                }
            } else {
#ifndef Q_OS_WIN
                // Create a symbolic link so a downgrade of the client would still find the config.
                QFile::link(confDir, oldDir);
#endif
                // we found a suitable config directory to migrate, hence we can stop here
                // if we continued to run, we would try to overwrite the working migration
                break;
            }
        }
    }
}


} // namespace

namespace OCC {

Q_LOGGING_CATEGORY(lcApplication, "gui.application", QtInfoMsg)

bool Application::configVersionMigration()
{
    QStringList deleteKeys, ignoreKeys;
    AccountManager::backwardMigrationSettingsKeys(&deleteKeys, &ignoreKeys);
    FolderMan::backwardMigrationSettingsKeys(&deleteKeys, &ignoreKeys);

    ConfigFile configFile;

    // Did the client version change?
    // (The client version is adjusted further down)
    const bool versionChanged = QVersionNumber::fromString(configFile.clientVersionString()) != OCC::Version::version();

    // We want to message the user either for destructive changes,
    // or if we're ignoring something and the client version changed.
    bool warningMessage = !deleteKeys.isEmpty() || (!ignoreKeys.isEmpty() && versionChanged);

    if (!versionChanged && !warningMessage)
        return true;

    const auto backupFile = configFile.backup();

    if (warningMessage) {
        QString boldMessage;
        if (!deleteKeys.isEmpty()) {
            boldMessage = tr("Continuing will mean <b>deleting these settings</b>.");
        } else {
            boldMessage = tr("Continuing will mean <b>ignoring these settings</b>.");
        }

        QMessageBox box(
            QMessageBox::Warning,
            Theme::instance()->appNameGUI(),
            tr("Some settings were configured in newer versions of this client and "
               "use features that are not available in this version.<br>"
               "<br>"
               "%1<br>"
               "<br>"
               "The current configuration file was already backed up to <i>%2</i>.")
                .arg(boldMessage, backupFile));
        box.addButton(tr("Quit"), QMessageBox::AcceptRole);
        auto continueBtn = box.addButton(tr("Continue"), QMessageBox::DestructiveRole);

        box.exec();
        if (box.clickedButton() != continueBtn) {
            QTimer::singleShot(0, qApp, &QApplication::quit);
            return false;
        }

        auto settings = ConfigFile::settingsWithGroup(QStringLiteral("foo"));
        settings->endGroup();

        // Wipe confusing keys from the future, ignore the others
        for (const auto &badKey : qAsConst(deleteKeys))
            settings->remove(badKey);
    }

    configFile.setClientVersionString(OCC::Version::version().toString());
    return true;
}

QString Application::displayLanguage() const
{
    return _displayLanguage;
}

ownCloudGui *Application::gui() const
{
    return _gui;
}

Application::Application(int &argc, char **argv)
    : SharedTools::QtSingleApplication(Theme::instance()->appName(), argc, argv)
    , _gui(nullptr)
    , _theme(Theme::instance())
    , _logFlush(false)
    , _logDebug(false)
    , _userTriggeredConnect(false)
    , _debugMode(false)
{
#ifdef Q_OS_WIN
    // Ensure OpenSSL config file is only loaded from app directory
    const QString opensslConf = QCoreApplication::applicationDirPath() + QStringLiteral("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());
#elif defined(Q_OS_LINUX)
#if defined(OC_PLUGIN_DIR)
    addLibraryPath(QDir(QApplication::applicationDirPath()).filePath(QStringLiteral(OC_PLUGIN_DIR)));
#endif
#endif
    // TODO: Can't set this without breaking current config paths
    //    setOrganizationName(QLatin1String(APPLICATION_VENDOR));
    setOrganizationDomain(QStringLiteral(APPLICATION_REV_DOMAIN));
    setApplicationName(_theme->appName());
    setWindowIcon(_theme->applicationIcon());

    // migrate old configuration files if necessary
    migrateConfigFile(this);

    // needed during commandline options parsing
    setApplicationVersion(_theme->versionSwitchOutput());

    parseOptions(arguments());

    if (isRunning())
        return;

#if defined(WITH_CRASHREPORTER)
    if (ConfigFile().crashReporter()) {
        auto reporter = QStringLiteral(CRASHREPORTER_EXECUTABLE);
#ifdef Q_OS_WIN
        if (!reporter.endsWith(QLatin1String(".exe"))) {
            reporter.append(QLatin1String(".exe"));
        }
#endif
        _crashHandler.reset(new CrashReporter::Handler(QDir::tempPath(), true, reporter));
    }
#endif

    setupLogging();
    setupTranslations();

    qCInfo(lcApplication) << "Plugin search paths:" << libraryPaths();

    // Check vfs plugins
    if (Theme::instance()->showVirtualFilesOption() && bestAvailableVfsMode() == Vfs::Off) {
        qCWarning(lcApplication) << "Theme wants to show vfs mode, but no vfs plugins are available";
    }
    if (isVfsPluginAvailable(Vfs::WindowsCfApi))
        qCInfo(lcApplication) << "VFS windows plugin is available";
    if (isVfsPluginAvailable(Vfs::WithSuffix))
        qCInfo(lcApplication) << "VFS suffix plugin is available";

    if (!configVersionMigration()) {
        return;
    }

    ConfigFile cfg;
    // The timeout is initialized with an environment variable, if not, override with the value from the config
    if (AbstractNetworkJob::httpTimeout == AbstractNetworkJob::DefaultHttpTimeout) {
        AbstractNetworkJob::httpTimeout = cfg.timeout();
    }

    // Check vfs plugins
    if (Theme::instance()->showVirtualFilesOption() && bestAvailableVfsMode() == Vfs::Off) {
        qCWarning(lcApplication) << "Theme wants to show vfs mode, but no vfs plugins are available";
    }
    if (isVfsPluginAvailable(Vfs::WindowsCfApi))
        qCInfo(lcApplication) << "VFS windows plugin is available";
    if (isVfsPluginAvailable(Vfs::WithSuffix))
        qCInfo(lcApplication) << "VFS suffix plugin is available";

    if (_quitInstance) {
        QTimer::singleShot(0, qApp, &QApplication::quit);
        return;
    }

    _folderManager.reset(new FolderMan);

    connect(this, &SharedTools::QtSingleApplication::messageReceived, this, &Application::slotParseMessage);

    if (!AccountManager::instance()->restore()) {
        // If there is an error reading the account settings, try again
        // after a couple of seconds, if that fails, give up.
        // (non-existence is not an error)
        Utility::sleep(5);
        if (!AccountManager::instance()->restore()) {
            qCCritical(lcApplication) << "Could not read the account settings, quitting";
            QMessageBox::critical(
                nullptr,
                tr("Error accessing the configuration file"),
                tr("There was an error while accessing the configuration "
                   "file at %1.")
                    .arg(ConfigFile::configFile()),
                tr("Quit %1").arg(Theme::instance()->appNameGUI()));
            QTimer::singleShot(0, qApp, &QApplication::quit);
            return;
        }
    }

    FolderMan::instance()->setSyncEnabled(true);

    setQuitOnLastWindowClosed(false);

    _theme->setSystrayUseMonoIcons(cfg.monoIcons());
    connect(_theme, &Theme::systrayUseMonoIconsChanged, this, &Application::slotUseMonoIconsChanged);

    // Setting up the gui class will allow tray notifications for the
    // setup that follows, like folder setup
    _gui = new ownCloudGui(this);
    if (_showSettings) {
        _gui->slotShowSettings();
    }

    FolderMan::instance()->setupFolders();
    _proxy.setupQtProxyFromConfig(); // folders have to be defined first, than we set up the Qt proxy.

    // Enable word wrapping of QInputDialog (#4197)
    setStyleSheet(QStringLiteral("QInputDialog QLabel { qproperty-wordWrap:1; }"));

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &Application::slotAccountStateAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &Application::slotAccountStateRemoved);
    for (const auto &ai : AccountManager::instance()->accounts()) {
        slotAccountStateAdded(ai);
    }

    connect(FolderMan::instance()->socketApi(), &SocketApi::shareCommandReceived,
        _gui.data(), &ownCloudGui::slotShowShareDialog);

    // startup procedure.
    connect(&_checkConnectionTimer, &QTimer::timeout, this, &Application::slotCheckConnection);
    _checkConnectionTimer.setInterval(ConnectionValidator::DefaultCallingInterval);
    _checkConnectionTimer.start();
    // Also check immediately
    QTimer::singleShot(0, this, &Application::slotCheckConnection);

    // Can't use onlineStateChanged because it is always true on modern systems because of many interfaces
    connect(&_networkConfigurationManager, &QNetworkConfigurationManager::configurationChanged,
        this, &Application::slotSystemOnlineConfigurationChanged);

#ifdef WITH_AUTO_UPDATER
    // Update checks
    UpdaterScheduler *updaterScheduler = new UpdaterScheduler(this);
    connect(updaterScheduler, &UpdaterScheduler::updaterAnnouncement,
        _gui.data(), [this](const QString &title, const QString &msg) {
            _gui->slotShowTrayMessage(title, msg);
        });
    connect(updaterScheduler, &UpdaterScheduler::requestRestart,
        _folderManager.data(), &FolderMan::slotScheduleAppRestart);
#endif

    // Cleanup at Quit.
    connect(this, &QCoreApplication::aboutToQuit, this, &Application::slotCleanup);
}

Application::~Application()
{
    // Make sure all folders are gone, otherwise removing the
    // accounts will remove the associated folders from the settings.
    if (_folderManager) {
        _folderManager->unloadAndDeleteAllFolders();
    }

    // Remove the account from the account manager so it can be deleted.
    AccountManager::instance()->shutdown();
}

void Application::slotAccountStateRemoved() const
{
    // if there is no more account, show the wizard.
    if (_gui && AccountManager::instance()->accounts().isEmpty()) {
        // allow to add a new account if there is non any more. Always think
        // about single account theming!
        gui()->runNewAccountWizard();
    }
}

void Application::slotAccountStateAdded(AccountStatePtr accountState) const
{
    // Hook up the GUI slots to the account state's signals:
    connect(accountState.data(), &AccountState::stateChanged,
        _gui.data(), &ownCloudGui::slotAccountStateChanged);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _gui.data(), [account = accountState->account().data(), this] {
            _gui->slotTrayMessageIfServerUnsupported(account);
        });

    // Hook up the folder manager slots to the account state's signals:
    connect(accountState.data(), &AccountState::stateChanged,
        _folderManager.data(), &FolderMan::slotAccountStateChanged);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _folderManager.data(), [account = accountState->account().data()] {
            FolderMan::instance()->slotServerVersionChanged(account);
        });

    _gui->slotTrayMessageIfServerUnsupported(accountState->account().data());
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
void Application::slotSystemOnlineConfigurationChanged(QNetworkConfiguration cnf)
{
    if (cnf.state() & QNetworkConfiguration::Active) {
        QMetaObject::invokeMethod(this, "slotCheckConnection", Qt::QueuedConnection);
    }
}

void Application::slotCheckConnection()
{
    const auto &list = AccountManager::instance()->accounts();
    for (const auto &accountState : list) {
        AccountState::State state = accountState->state();

        // Don't check if we're manually signed out or
        // when the error is permanent.
        if (state != AccountState::SignedOut
            && state != AccountState::ConfigurationError
            && state != AccountState::AskingCredentials) {
            accountState->checkConnectivity();
        }
    }

    if (list.isEmpty()) {
        // let gui open the setup wizard
        _gui->slotOpenSettingsDialog();

        _checkConnectionTimer.stop(); // don't popup the wizard on interval;
    }
}

void Application::slotCrash()
{
    Utility::crash();
}

void Application::slotCrashEnforce()
{
    OC_ENFORCE(1 == 0);
}


void Application::slotCrashFatal()
{
    qFatal("la Qt fatale");
}

void Application::slotShowGuiMessage(const QString &title, const QString &message)
{
    gui()->slotShowGuiMessage(title, message);
}


AccountStatePtr Application::addNewAccount(AccountPtr newAccount)
{
    auto *accountMan = AccountManager::instance();

    // first things first: we need to add the new account
    auto accountStatePtr = accountMan->addAccount(newAccount);

    // check connectivity of the newly created account
    _checkConnectionTimer.start();
    slotCheckConnection();

    // if one account is configured: enable autostart
    bool shouldSetAutoStart = (accountMan->accounts().size() == 1);
#ifdef Q_OS_MAC
    // Don't auto start when not being 'installed'
    shouldSetAutoStart = shouldSetAutoStart
        && QCoreApplication::applicationDirPath().startsWith(QLatin1String("/Applications/"));
#endif
    if (shouldSetAutoStart) {
        Utility::setLaunchOnStartup(_theme->appName(), _theme->appNameGUI(), true);
    }

    // showing the UI to show the user that the account has been added successfully
    _gui->slotShowSettings();

    return accountStatePtr;
}

void Application::setupLogging()
{
    // might be called from second instance
    auto logger = Logger::instance();
    // call setLogFlush first, other log settings might already imply flushing
    // so setting it false in the end will have undesired results.
    logger->setLogFlush(_logFlush);

    if (!_logDir.isEmpty()) {
        logger->setLogDir(_logDir);
    }
    if (!_logFile.isEmpty()) {
        Q_ASSERT(_logDir.isEmpty());
        logger->setLogFile(_logFile);
    }
    logger->setLogDebug(_logDebug);

    // Possibly configure logging from config file
    LogBrowser::setupLoggingFromConfig();

    qCInfo(lcApplication) << "##################" << _theme->appName()
                          << "locale:" << QLocale::system().name()
                          << "version:" << _theme->aboutVersions(Theme::VersionFormat::OneLiner);
    qCInfo(lcApplication) << "Arguments:" << qApp->arguments();
}

void Application::slotUseMonoIconsChanged(bool)
{
    _gui->slotComputeOverallSyncStatus();
}

void Application::slotParseMessage(const QString &msg, QObject *)
{
    if (msg.startsWith(QLatin1String("MSG_PARSEOPTIONS:"))) {
        const int lengthOfMsgPrefix = 17;
        QStringList options = msg.mid(lengthOfMsgPrefix).split(QLatin1Char('|'));
        _showSettings = false;
        parseOptions(options);
        setupLogging();
        if (_showSettings) {
            _gui->slotShowSettings();
        }
        if (_quitInstance) {
            qApp->quit();
        }
    }
}

// Helpers for displaying messages. Note that there is probably no console on Windows.
static void displayHelpText(const QString &t, std::ostream &stream = std::cout)
{
    Logger::instance()->attacheToConsole();
    stream << qUtf8Printable(t) << std::endl;
#ifdef Q_OS_WIN
    // No console on Windows.
    QString spaces(80, QLatin1Char(' ')); // Add a line of non-wrapped space to make the messagebox wide enough.
    QString text = QStringLiteral("<qt><pre style='white-space:pre-wrap'>")
        + t.toHtmlEscaped() + QStringLiteral("</pre><pre>") + spaces + QStringLiteral("</pre></qt>");
    QMessageBox::information(0, Theme::instance()->appNameGUI(), text);
#endif
}

void Application::parseOptions(const QStringList &arguments)
{
    QCommandLineParser parser;

    QString descriptionText;
    QTextStream descriptionTextStream(&descriptionText);

    descriptionTextStream << tr("%1 version %2\r\nFile synchronization desktop utility.").arg(_theme->appName(), OCC::Version::displayString()) << endl;

    if (_theme->appName() == QLatin1String("ownCloud")) {
        descriptionTextStream << endl
                              << endl
                              << tr("For more information, see %1", "link to homepage").arg(QStringLiteral("https://www.owncloud.com"));
    }

    parser.setApplicationDescription(descriptionText);

    auto helpOption = parser.addHelpOption();
    auto versionOption = parser.addVersionOption();

    // this little snippet saves a few lines below
    auto addOption = [&parser](const QCommandLineOption &option) {
        parser.addOption(option);
        return option;
    };

    auto showSettingsOption = addOption({ { QStringLiteral("s"), QStringLiteral("showsettings") }, tr("Show the settings dialog while starting.") });
    auto quitInstanceOption = addOption({ { QStringLiteral("q"), QStringLiteral("quit") }, tr("Quit the running instance.") });
    auto logFileOption = addOption({ QStringLiteral("logfile"), tr("Write log to file (use - to write to stdout)."), QStringLiteral("filename") });
    auto logDirOption = addOption({ QStringLiteral("logdir"), tr("Write each sync log output in a new file in folder."), QStringLiteral("name") });
    auto logFlushOption = addOption({ QStringLiteral("logflush"), tr("Flush the log file after every write.") });
    auto logDebugOption = addOption({ QStringLiteral("logdebug"), tr("Output debug-level messages in the log.") });
    auto languageOption = addOption({ QStringLiteral("language"), tr("Override UI language."), QStringLiteral("language") });
    auto listLanguagesOption = addOption({ QStringLiteral("list-languages"), tr("Override UI language.") });
    auto confDirOption = addOption({ QStringLiteral("confdir"), tr("Use the given configuration folder."), QStringLiteral("dirname") });
    auto debugOption = addOption({ QStringLiteral("debug"), tr("Enable debug mode.") });

    // virtual file system parameters (optional)
    parser.addPositionalArgument(QStringLiteral("vfs file"), tr("Virtual file system file to be opened (optional)."), { tr("[<vfs file>]") });

    parser.process(arguments);

    // TODO: rename this option (see #8234 for more information)
    if (parser.isSet(showSettingsOption)) {
        _showSettings = true;
    }
    if (parser.isSet(quitInstanceOption)) {
        _quitInstance = true;
    }
    if (parser.isSet(logFileOption)) {
        _logFile = parser.value(logFileOption);
    }
    if (parser.isSet(logDirOption)) {
        if (parser.isSet(logFileOption)) {
            displayHelpText(tr("--logfile and --logdir are mutually exclusive"));
            std::exit(1);
        }
        _logDir = parser.value(logDirOption);
    }
    if (parser.isSet(logFlushOption)) {
        _logFlush = true;
    }
    if (parser.isSet(logDebugOption)) {
        _logDebug = true;
    }
    if (parser.isSet(confDirOption)) {
        const auto confDir = parser.value(confDirOption);
        if (!ConfigFile::setConfDir(confDir)) {
            displayHelpText(tr("Invalid path passed to --confdir"));
            std::exit(1);
        }
    }
    if (parser.isSet(debugOption)) {
        _logDebug = true;
        _debugMode = true;
    }
    if (parser.isSet(languageOption)) {
        const auto languageValue = parser.value(languageOption);

        // fail if the language is unknown
        if (!Translations::listAvailableTranslations().contains(languageValue)) {
            displayHelpText(tr("Error: unknown language \"%1\" (use --list-languages to get a complete list of supported translations)").arg(languageValue));
            std::exit(1);
        } else {
            _userEnforcedLanguage = languageValue;
        }
    }
    if (parser.isSet(listLanguagesOption)) {
        auto availableTranslations = Translations::listAvailableTranslations().toList();
        availableTranslations.sort(Qt::CaseInsensitive);
        displayHelpText(tr("Available translations: %1").arg(availableTranslations.join(QStringLiteral(", "))));
        std::exit(1);
    }

    auto positionalArguments = parser.positionalArguments();

    // ignore any positional arguments beyond the first one
    if (!positionalArguments.empty()) {
        QTimer::singleShot(0, this, [this, positionalArguments] { openVirtualFile(positionalArguments.front()); });
    }
}

bool Application::debugMode()
{
    return _debugMode;
}

QString substLang(const QString &lang)
{
    // Map the more appropriate script codes
    // to country codes as used by Qt and
    // transifex translation conventions.

    // Simplified Chinese
    if (lang == QLatin1String("zh_Hans"))
        return QStringLiteral("zh_CN");
    // Traditional Chinese
    if (lang == QLatin1String("zh_Hant"))
        return QStringLiteral("zh_TW");
    return lang;
}

void Application::setupTranslations()
{
    const auto trPath = Translations::translationsDirectoryPath();
    qCDebug(lcApplication) << "Translations directory path:" << trPath;

    QStringList uiLanguages = QLocale::system().uiLanguages();
    qCDebug(lcApplication) << "UI languages:" << uiLanguages;

    // the user can also set a locale in the settings, so we need to load the config file
    ConfigFile cfg;

    // allow user and theme to enforce a language via a commandline parameter
    const auto themeEnforcedLocale = Theme::instance()->enforcedLocale();
    qCDebug(lcApplication) << "Theme-enforced locale:" << themeEnforcedLocale;

    // we need to track the enforced languages separately, since we need to distinguish between locale-provided
    // and user-enforced ones below
    QSet<QString> enforcedLanguages;

    // note that user-enforced languages are prioritized over the theme enforced one
    // to make testing easier, --language overrides the setting from the config file
    // as we are prepending to the list of languages, the list passed to the loop must be sorted with ascending priority
    for (const auto &enforcedLocale : { themeEnforcedLocale, cfg.uiLanguage(), _userEnforcedLanguage }) {
        if (!enforcedLocale.isEmpty()) {
            enforcedLanguages.insert(enforcedLocale);
            uiLanguages.prepend(enforcedLocale);
        }
    }

    qCDebug(lcApplication) << "Enforced languages:" << enforcedLanguages;

    QTranslator *translator = new QTranslator(this);
    QTranslator *qtTranslator = new QTranslator(this);
    QTranslator *qtkeychainTranslator = new QTranslator(this);

    for (QString lang : qAsConst(uiLanguages)) {
        lang.replace(QLatin1Char('-'), QLatin1Char('_')); // work around QTBUG-25973
        lang = substLang(lang);
        const QString trFile = Translations::translationsFilePrefix() + lang;
        if (translator->load(trFile, trPath) || lang.startsWith(QLatin1String("en"))) {
            // Permissive approach: Qt and keychain translations
            // may be missing, but Qt translations must be there in order
            // for us to accept the language. Otherwise, we try with the next.
            // "en" is an exception as it is the default language and may not
            // have a translation file provided.
            qCInfo(lcApplication) << "Using" << lang << "translation";
            _displayLanguage = lang;

            const QString qtTrPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
            qCDebug(lcApplication) << "qtTrPath:" << qtTrPath;
            const QString qtTrFile = QLatin1String("qt_") + lang;
            qCDebug(lcApplication) << "qtTrFile:" << qtTrFile;
            const QString qtBaseTrFile = QLatin1String("qtbase_") + lang;
            qCDebug(lcApplication) << "qtBaseTrFile:" << qtBaseTrFile;

            if (!qtTranslator->load(qtTrFile, qtTrPath)) {
                if (!qtTranslator->load(qtTrFile, trPath)) {
                    if (!qtTranslator->load(qtBaseTrFile, qtTrPath)) {
                        if (!qtTranslator->load(qtBaseTrFile, trPath)) {
                            qCCritical(lcApplication) << "Could not load Qt translations";
                        }
                    }
                }
            }

            const QString qtkeychainTrFile = QLatin1String("qtkeychain_") + lang;
            if (!qtkeychainTranslator->load(qtkeychainTrFile, qtTrPath)) {
                if (!qtkeychainTranslator->load(qtkeychainTrFile, trPath)) {
                    qCCritical(lcApplication) << "Could not load qtkeychain translations";
                }
            }

            if (!translator->isEmpty() && !installTranslator(translator)) {
                qCCritical(lcApplication) << "Failed to install translator";
            }
            if (!qtTranslator->isEmpty() && !installTranslator(qtTranslator)) {
                qCCritical(lcApplication) << "Failed to install Qt translator";
            }
            if (!qtkeychainTranslator->isEmpty() && !installTranslator(qtkeychainTranslator)) {
                qCCritical(lcApplication) << "Failed to install qtkeychain translator";
            }

            // makes sure widgets with locale-dependent formatting, e.g., QDateEdit, display the correct formatting
            // if the language is provided by the system locale anyway (i.e., coming from QLocale::system().uiLanguages()), we should
            // not mess with the system locale, though
            // if we did, we would enforce a locale for no apparent reason
            // see https://github.com/owncloud/client/issues/8608 for more information
            if (enforcedLanguages.contains(lang)) {
                QLocale newLocale(lang);
                qCDebug(lcApplication) << "language" << lang << "was enforced, changing default locale to" << newLocale;
                QLocale::setDefault(newLocale);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                // setting the layout direction directly only appears to be needed on mac
                setLayoutDirection(newLocale.textDirection());
#endif
            }

            break;
        }
    }
}

void Application::showSettingsDialog()
{
    _gui->slotShowSettings();
}

void Application::openVirtualFile(const QString &filename)
{
    QString virtualFileExt = QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX);
    if (!filename.endsWith(virtualFileExt)) {
        qWarning(lcApplication) << "Can only handle file ending in .owncloud. Unable to open" << filename;
        return;
    }
    QString relativePath;
    auto folder = FolderMan::instance()->folderForPath(filename, &relativePath);
    if (!folder) {
        qWarning(lcApplication) << "Can't find sync folder for" << filename;
        // TODO: show a QMessageBox for errors
        return;
    }
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
    if (!_gui->contextMenuVisible())
        _gui->hideAndShowTray();
}

bool Application::event(QEvent *event)
{
#ifdef Q_OS_MAC
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        qCDebug(lcApplication) << "QFileOpenEvent" << openEvent->file();
        // virtual file, open it after the Folder were created (if the app is not terminated)
        QString fn = openEvent->file();
        QTimer::singleShot(0, this, [this, fn] { openVirtualFile(fn); });
    }
#endif
    return SharedTools::QtSingleApplication::event(event);
}

} // namespace OCC
