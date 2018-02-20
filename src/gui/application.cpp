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

#include "config.h"
#include "account.h"
#include "accountstate.h"
#include "connectionvalidator.h"
#include "folder.h"
#include "folderman.h"
#include "logger.h"
#include "configfile.h"
#include "socketapi.h"
#include "sslerrordialog.h"
#include "theme.h"
#include "clientproxy.h"
#include "sharedialog.h"
#include "accountmanager.h"
#include "creds/abstractcredentials.h"
#include "updater/ocupdater.h"
#include "excludedfiles.h"
#include "owncloudsetupwizard.h"
#include "version.h"

#include "config.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#if defined(WITH_CRASHREPORTER)
#include <libcrashreporter-handler/Handler.h>
#endif

#include <QTranslator>
#include <QMenu>
#include <QMessageBox>

class QSocket;

namespace OCC {

Q_LOGGING_CATEGORY(lcApplication, "gui.application", QtInfoMsg)

namespace {

    static const char optionsC[] =
        "Options:\n"
        "  -h --help            : show this help screen.\n"
        "  --logwindow          : open a window to show log output.\n"
        "  --logfile <filename> : write log output to file <filename>.\n"
        "  --logdir <name>      : write each sync log output in a new file\n"
        "                         in folder <name>.\n"
        "  --logexpire <hours>  : removes logs older than <hours> hours.\n"
        "                         (to be used with --logdir)\n"
        "  --logflush           : flush the log file after every write.\n"
        "  --logdebug           : also output debug-level messages in the log (equivalent to setting the env var QT_LOGGING_RULES=\"qt.*=true;*.debug=true\").\n"
        "  --confdir <dirname>  : Use the given configuration folder.\n";

    QString applicationTrPath()
    {
        QString devTrPath = qApp->applicationDirPath() + QString::fromLatin1("/../src/gui/");
        if (QDir(devTrPath).exists()) {
            // might miss Qt, QtKeyChain, etc.
            qCWarning(lcApplication) << "Running from build location! Translations may be incomplete!";
            return devTrPath;
        }
#if defined(Q_OS_WIN)
        return QApplication::applicationDirPath();
#elif defined(Q_OS_MAC)
        return QApplication::applicationDirPath() + QLatin1String("/../Resources/Translations"); // path defaults to app dir.
#elif defined(Q_OS_UNIX)
        return QString::fromLatin1(SHAREDIR "/" APPLICATION_EXECUTABLE "/i18n/");
#endif
    }
}

// ----------------------------------------------------------------------------------

Application::Application(int &argc, char **argv)
    : SharedTools::QtSingleApplication(Theme::instance()->appName(), argc, argv)
    , _gui(0)
    , _theme(Theme::instance())
    , _helpOnly(false)
    , _versionOnly(false)
    , _showLogWindow(false)
    , _logExpire(0)
    , _logFlush(false)
    , _logDebug(false)
    , _userTriggeredConnect(false)
    , _debugMode(false)
{
    _startedAt.start();

    qsrand(std::random_device()());

#ifdef Q_OS_WIN
    // Ensure OpenSSL config file is only loaded from app directory
    QString opensslConf = QCoreApplication::applicationDirPath() + QString("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());
#endif

    // TODO: Can't set this without breaking current config paths
    //    setOrganizationName(QLatin1String(APPLICATION_VENDOR));
    setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));
    setApplicationName(_theme->appNameGUI());
    setWindowIcon(_theme->applicationIcon());
    setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    parseOptions(arguments());
    //no need to waste time;
    if (_helpOnly || _versionOnly)
        return;

    if (isRunning())
        return;

#if defined(WITH_CRASHREPORTER)
    if (ConfigFile().crashReporter())
        _crashHandler.reset(new CrashReporter::Handler(QDir::tempPath(), true, CRASHREPORTER_EXECUTABLE));
#endif

    setupLogging();
    setupTranslations();

    // Setup global excludes
    qCInfo(lcApplication) << "Loading global exclude list";
    ConfigFile cfg;
    ExcludedFiles &excludes = ExcludedFiles::instance();
    excludes.addExcludeFilePath(cfg.excludeFile(ConfigFile::SystemScope));
    excludes.addExcludeFilePath(cfg.excludeFile(ConfigFile::UserScope));
    excludes.reloadExcludes();

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
                0,
                tr("Error accessing the configuration file"),
                tr("There was an error while accessing the configuration "
                   "file at %1.")
                    .arg(ConfigFile().configFile()),
                tr("Quit ownCloud"));
            QTimer::singleShot(0, qApp, SLOT(quit()));
            return;
        }
    }

    FolderMan::instance()->setSyncEnabled(true);

    setQuitOnLastWindowClosed(false);

    _theme->setSystrayUseMonoIcons(cfg.monoIcons());
    connect(_theme, &Theme::systrayUseMonoIconsChanged, this, &Application::slotUseMonoIconsChanged);

    FolderMan::instance()->setupFolders();
    _proxy.setupQtProxyFromConfig(); // folders have to be defined first, than we set up the Qt proxy.

    _gui = new ownCloudGui(this);
    if (_showLogWindow) {
        _gui->slotToggleLogBrowser(); // _showLogWindow is set in parseOptions.
    }

    // Enable word wrapping of QInputDialog (#4197)
    setStyleSheet("QInputDialog QLabel { qproperty-wordWrap:1; }");

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &Application::slotAccountStateAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &Application::slotAccountStateRemoved);
    foreach (auto ai, AccountManager::instance()->accounts()) {
        slotAccountStateAdded(ai.data());
    }

    connect(FolderMan::instance()->socketApi(), &SocketApi::shareCommandReceived,
        _gui.data(), &ownCloudGui::slotShowShareDialog);

    // startup procedure.
    connect(&_checkConnectionTimer, &QTimer::timeout, this, &Application::slotCheckConnection);
    _checkConnectionTimer.setInterval(ConnectionValidator::DefaultCallingIntervalMsec); // check for connection every 32 seconds.
    _checkConnectionTimer.start();
    // Also check immediately
    QTimer::singleShot(0, this, &Application::slotCheckConnection);

    // Can't use onlineStateChanged because it is always true on modern systems because of many interfaces
    connect(&_networkConfigurationManager, &QNetworkConfigurationManager::configurationChanged,
        this, &Application::slotSystemOnlineConfigurationChanged);

    // Update checks
    UpdaterScheduler *updaterScheduler = new UpdaterScheduler(this);
    connect(updaterScheduler, &UpdaterScheduler::updaterAnnouncement,
        _gui.data(), &ownCloudGui::slotShowTrayMessage);
    connect(updaterScheduler, &UpdaterScheduler::requestRestart,
        _folderManager.data(), &FolderMan::slotScheduleAppRestart);

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

void Application::slotAccountStateRemoved(AccountState *accountState)
{
    if (_gui) {
        disconnect(accountState, &AccountState::stateChanged,
            _gui.data(), &ownCloudGui::slotAccountStateChanged);
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
    if (AccountManager::instance()->accounts().isEmpty()) {
        // allow to add a new account if there is non any more. Always think
        // about single account theming!
        OwncloudSetupWizard::runWizard(this, SLOT(slotownCloudWizardDone(int)));
    }
}

void Application::slotAccountStateAdded(AccountState *accountState)
{
    connect(accountState, &AccountState::stateChanged,
        _gui.data(), &ownCloudGui::slotAccountStateChanged);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _gui.data(), &ownCloudGui::slotTrayMessageIfServerUnsupported);
    connect(accountState, &AccountState::stateChanged,
        _folderManager.data(), &FolderMan::slotAccountStateChanged);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _folderManager.data(), &FolderMan::slotServerVersionChanged);

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
    auto list = AccountManager::instance()->accounts();
    foreach (const auto &accountState, list) {
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

void Application::slotownCloudWizardDone(int res)
{
    AccountManager *accountMan = AccountManager::instance();
    FolderMan *folderMan = FolderMan::instance();

    // During the wizard, scheduling of new syncs is disabled
    folderMan->setSyncEnabled(true);

    if (res == QDialog::Accepted) {
        // Check connectivity of the newly created account
        _checkConnectionTimer.start();
        slotCheckConnection();

        // If one account is configured: enable autostart
        bool shouldSetAutoStart = (accountMan->accounts().size() == 1);
#ifdef Q_OS_MAC
        // Don't auto start when not being 'installed'
        shouldSetAutoStart = shouldSetAutoStart
            && QCoreApplication::applicationDirPath().startsWith("/Applications/");
#endif
        if (shouldSetAutoStart) {
            Utility::setLaunchOnStartup(_theme->appName(), _theme->appNameGUI(), true);
        }

        _gui->slotShowSettings();
    }
}

void Application::setupLogging()
{
    // might be called from second instance
    Logger::instance()->setLogFile(_logFile);
    Logger::instance()->setLogDir(_logDir);
    Logger::instance()->setLogExpire(_logExpire);
    Logger::instance()->setLogFlush(_logFlush);
    Logger::instance()->setLogDebug(_logDebug);

    Logger::instance()->enterNextLogFile();

    qCInfo(lcApplication) << QString::fromLatin1("################## %1 locale:[%2] ui_lang:[%3] version:[%4] os:[%5]").arg(_theme->appName()).arg(QLocale::system().name()).arg(property("ui_lang").toString()).arg(_theme->version()).arg(Utility::platformName());
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
        parseOptions(options);
        setupLogging();
    } else if (msg.startsWith(QLatin1String("MSG_SHOWSETTINGS"))) {
        qCInfo(lcApplication) << "Running for" << _startedAt.elapsed() / 1000.0 << "sec";
        if (_startedAt.elapsed() < 10 * 1000) {
            // This call is mirrored with the one in int main()
            qCWarning(lcApplication) << "Ignoring MSG_SHOWSETTINGS, possibly double-invocation of client via session restore and auto start";
            return;
        }
        showSettingsDialog();
    }
}

void Application::parseOptions(const QStringList &options)
{
    QStringListIterator it(options);
    // skip file name;
    if (it.hasNext())
        it.next();

    //parse options; if help or bad option exit
    while (it.hasNext()) {
        QString option = it.next();
        if (option == QLatin1String("--help") || option == QLatin1String("-h")) {
            setHelp();
            break;
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
        } else if (option == QLatin1String("--version")) {
            _versionOnly = true;
        } else {
            showHint("Unrecognized option '" + option.toStdString() + "'");
        }
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
    QMessageBox::information(0, Theme::instance()->appNameGUI(), t);
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
           << _theme->version() << endl;

    stream << QLatin1String("File synchronisation desktop utility.") << endl
           << endl
           << QLatin1String(optionsC);

    if (_theme->appName() == QLatin1String("ownCloud"))
        stream << endl
               << "For more information, see http://www.owncloud.org" << endl
               << endl;

    displayHelpText(helpText);
}

void Application::showVersion()
{
    displayHelpText(Theme::instance()->versionSwitchOutput());
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

void Application::setHelp()
{
    _helpOnly = true;
}

QString substLang(const QString &lang)
{
    // Map the more appropriate script codes
    // to country codes as used by Qt and
    // transifex translation conventions.

    // Simplified Chinese
    if (lang == QLatin1String("zh_Hans"))
        return QLatin1String("zh_CN");
    // Traditional Chinese
    if (lang == QLatin1String("zh_Hant"))
        return QLatin1String("zh_TW");
    return lang;
}

void Application::setupTranslations()
{
    QStringList uiLanguages;
// uiLanguages crashes on Windows with 4.8.0 release builds
#if (QT_VERSION >= 0x040801) || (QT_VERSION >= 0x040800 && !defined(Q_OS_WIN))
    uiLanguages = QLocale::system().uiLanguages();
#else
    // older versions need to fall back to the systems locale
    uiLanguages << QLocale::system().name();
#endif

    QString enforcedLocale = Theme::instance()->enforcedLocale();
    if (!enforcedLocale.isEmpty())
        uiLanguages.prepend(enforcedLocale);

    QTranslator *translator = new QTranslator(this);
    QTranslator *qtTranslator = new QTranslator(this);
    QTranslator *qtkeychainTranslator = new QTranslator(this);

    foreach (QString lang, uiLanguages) {
        lang.replace(QLatin1Char('-'), QLatin1Char('_')); // work around QTBUG-25973
        lang = substLang(lang);
        const QString trPath = applicationTrPath();
        const QString trFile = QLatin1String("client_") + lang;
        if (translator->load(trFile, trPath) || lang.startsWith(QLatin1String("en"))) {
            // Permissive approach: Qt and keychain translations
            // may be missing, but Qt translations must be there in order
            // for us to accept the language. Otherwise, we try with the next.
            // "en" is an exception as it is the default language and may not
            // have a translation file provided.
            qCInfo(lcApplication) << "Using" << lang << "translation";
            setProperty("ui_lang", lang);
            const QString qtTrPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
            const QString qtTrFile = QLatin1String("qt_") + lang;
            const QString qtBaseTrFile = QLatin1String("qtbase_") + lang;
            if (!qtTranslator->load(qtTrFile, qtTrPath)) {
                if (!qtTranslator->load(qtTrFile, trPath)) {
                    if (!qtTranslator->load(qtBaseTrFile, qtTrPath)) {
                        qtTranslator->load(qtBaseTrFile, trPath);
                    }
                }
            }
            const QString qtkeychainTrFile = QLatin1String("qtkeychain_") + lang;
            if (!qtkeychainTranslator->load(qtkeychainTrFile, qtTrPath)) {
                qtkeychainTranslator->load(qtkeychainTrFile, trPath);
            }
            if (!translator->isEmpty())
                installTranslator(translator);
            if (!qtTranslator->isEmpty())
                installTranslator(qtTranslator);
            if (!qtkeychainTranslator->isEmpty())
                installTranslator(qtkeychainTranslator);
            break;
        }
        if (property("ui_lang").isNull())
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

void Application::showSettingsDialog()
{
    _gui->slotShowSettings();
}


} // namespace OCC
