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
#include "utility.h"
#include "clientproxy.h"
#include "sharedialog.h"

#include "updater/updater.h"
#include "creds/abstractcredentials.h"

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

namespace {

static const char optionsC[] =
        "Options:\n"
        "  -h --help            : show this help screen.\n"
        "  --logwindow          : open a window to show log output.\n"
        "  --logfile <filename> : write log output to file <filename>.\n"
        "  --logdir <name>      : write each sync log output in a new file\n"
        "                         in directory <name>.\n"
        "  --logexpire <hours>  : removes logs older than <hours> hours.\n"
        "                         (to be used with --logdir)\n"
        "  --logflush           : flush the log file after every write.\n"
        "  --confdir <dirname>  : Use the given configuration directory.\n"
        ;

QString applicationTrPath()
{
#if defined(Q_OS_WIN)
   return QApplication::applicationDirPath();
#elif defined(Q_OS_MAC)
    return QApplication::applicationDirPath()+QLatin1String("/../Resources/Translations"); // path defaults to app dir.
#elif defined(Q_OS_UNIX)
    return QString::fromLatin1(DATADIR "/" APPLICATION_EXECUTABLE "/i18n/");
#endif
}
}

// ----------------------------------------------------------------------------------

Application::Application(int &argc, char **argv) :
    SharedTools::QtSingleApplication(Theme::instance()->appName() ,argc, argv),
    _gui(0),
    _theme(Theme::instance()),
    _helpOnly(false),
    _startupNetworkError(false),
    _showLogWindow(false),
    _logExpire(0),
    _logFlush(false),
    _userTriggeredConnect(false),
    _debugMode(false)
{
// TODO: Can't set this without breaking current config pathes
//    setOrganizationName(QLatin1String(APPLICATION_VENDOR));
    setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));
    setApplicationName( _theme->appNameGUI() );
    setWindowIcon( _theme->applicationIcon() );
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif
    parseOptions(arguments());
    //no need to waste time;
    if ( _helpOnly ) return;

    if (isRunning())
        return;

#if defined(WITH_CRASHREPORTER)
    if (ConfigFile().crashReporter())
        _crashHandler.reset(new CrashReporter::Handler( QDir::tempPath(), true, CRASHREPORTER_EXECUTABLE ));
#endif

    setupLogging();
    setupTranslations();

    _folderManager.reset(new FolderMan);

    connect(this, SIGNAL(messageReceived(QString, QObject*)), SLOT(slotParseMessage(QString, QObject*)));

    // Create the account info manager to ensure it's listening to the
    // account manager.
    AccountStateManager::instance();

    AccountPtr account = Account::restore();
    if (account) {
        account->setSslErrorHandler(new SslDialogErrorHandler);
        AccountManager::instance()->setAccount(account);
    }

    FolderMan::instance()->setSyncEnabled(false);

    setQuitOnLastWindowClosed(false);

    qRegisterMetaType<Progress::Info>("Progress::Info");

    ConfigFile cfg;
    _theme->setSystrayUseMonoIcons(cfg.monoIcons());
    connect (_theme, SIGNAL(systrayUseMonoIconsChanged(bool)), SLOT(slotUseMonoIconsChanged(bool)));

    FolderMan::instance()->setupFolders();
    _proxy.setupQtProxyFromConfig(); // folders have to be defined first, than we set up the Qt proxy.

    _gui = new ownCloudGui(this);
    if( _showLogWindow ) {
        _gui->slotToggleLogBrowser(); // _showLogWindow is set in parseOptions.
    }

    connect(AccountStateManager::instance(), SIGNAL(accountStateAdded(AccountState*)),
            SLOT(slotAccountStateAdded(AccountState*)));
    connect(AccountStateManager::instance(), SIGNAL(accountStateRemoved(AccountState*)),
            SLOT(slotAccountStateRemoved(AccountState*)));
    if (AccountState *ai = AccountStateManager::instance()->accountState()) {
        slotAccountStateAdded(ai);
    }

    connect(FolderMan::instance()->socketApi(), SIGNAL(shareCommandReceived(QString, QString, bool)),
            _gui, SLOT(slotShowShareDialog(QString, QString, bool)));

    // startup procedure.
    connect(&_checkConnectionTimer, SIGNAL(timeout()), this, SLOT(slotCheckConnection()));
    _checkConnectionTimer.setInterval(32 * 1000); // check for connection every 32 seconds.
    _checkConnectionTimer.start();
    // Also check immediatly
    QTimer::singleShot( 0, this, SLOT( slotCheckConnection() ));

    if( cfg.skipUpdateCheck() ) {
        qDebug() << Q_FUNC_INFO << "Skipping update check";
    } else {
        QTimer::singleShot( 3000, this, SLOT( slotStartUpdateDetector() ));
    }

    connect (this, SIGNAL(aboutToQuit()), SLOT(slotCleanup()));

}

Application::~Application()
{
    // Remove the account from the account manager so it can be deleted.
    AccountManager::instance()->setAccount(AccountPtr());
}

void Application::slotLogin()
{
    AccountState *a = AccountStateManager::instance()->accountState();
    if (a) {
        FolderMan::instance()->setupFolders();
        a->setSignedOut(false);
    }
}

void Application::slotLogout()
{
    AccountState* ai = AccountStateManager::instance()->accountState();
    if (ai) {
        AccountPtr a = ai->account();
        // invalidate & forget token/password
        a->credentials()->invalidateToken();
        // terminate all syncs and unload folders
        FolderMan *folderMan = FolderMan::instance();
        folderMan->setSyncEnabled(false);
        folderMan->terminateSyncProcess();
        ai->setSignedOut(true);
        // show result
        _gui->slotComputeOverallSyncStatus();
    }
}

void Application::slotAccountStateRemoved(AccountState *accountState)
{
    disconnect(accountState, SIGNAL(stateChanged(int)), _gui, SLOT(slotAccountStateChanged()));
    disconnect(accountState, SIGNAL(stateChanged(int)), this, SLOT(slotAccountStateChanged(int)));
    connect(accountState->quotaInfo(), SIGNAL(quotaUpdated(qint64,qint64)),
            _gui, SLOT(slotRefreshQuotaDisplay(qint64,qint64)));
}

void Application::slotAccountStateAdded(AccountState *accountState)
{
    connect(accountState, SIGNAL(stateChanged(int)), _gui, SLOT(slotAccountStateChanged()));
    connect(accountState, SIGNAL(stateChanged(int)), this, SLOT(slotAccountStateChanged(int)));
    connect(accountState->quotaInfo(), SIGNAL(quotaUpdated(qint64,qint64)),
            _gui, SLOT(slotRefreshQuotaDisplay(qint64,qint64)));
}

void Application::slotCleanup()
{
    // explicitly close windows. This is somewhat of a hack to ensure
    // that saving the geometries happens ASAP during a OS shutdown
    AccountPtr account = AccountManager::instance()->account();
    if (account) {
        account->save();
    }
    FolderMan::instance()->unloadAndDeleteAllFolders();

    _gui->slotShutdown();
    _gui->deleteLater();
}

void Application::slotStartUpdateDetector()
{
    Updater *updater = Updater::instance();
    updater->backgroundCheckForUpdate();
}

void Application::slotCheckConnection()
{
    AccountState *accountState = AccountStateManager::instance()->accountState();

    if( accountState ) {
        accountState->checkConnectivity();

    } else {
        // let gui open the setup wizard
        _gui->slotOpenSettingsDialog( true );

        _checkConnectionTimer.stop(); // don't popup the wizard on interval;
    }
}

void Application::slotAccountStateChanged(int state)
{
    FolderMan* folderMan = FolderMan::instance();
    switch (state) {
    case AccountState::Connected:
        qDebug() << "Enabling sync scheduler, scheduling all folders";
        folderMan->setSyncEnabled(true);
        folderMan->slotScheduleAllFolders();
        break;
    case AccountState::ServerMaintenance:
    case AccountState::SignedOut:
    case AccountState::ConfigurationError:
    case AccountState::NetworkError:
    case AccountState::Disconnected:
        qDebug() << "Disabling sync scheduler, terminating sync";
        folderMan->setSyncEnabled(false);
        folderMan->terminateSyncProcess();
        break;
    }

    // Stop checking the connection if we're manually signed out or
    // when the error is permanent.
    if (state == AccountState::SignedOut
            || state == AccountState::ConfigurationError) {
        _checkConnectionTimer.stop();
    } else if (! _checkConnectionTimer.isActive()) {
        _checkConnectionTimer.start();
    }

    slotUpdateConnectionErrors(state);
}

void Application::slotCrash()
{
    Utility::crash();
}

void Application::slotUpdateConnectionErrors(int accountState)
{
    bool isConnected = accountState == AccountState::Connected;
    if( !isConnected ) {
        _startupNetworkError = accountState == AccountState::NetworkError;
    }

    AccountState *as = AccountStateManager::instance()->accountState();
    if (as) {
        _gui->setConnectionErrors( isConnected, as->connectionErrors() );
    }
}

void Application::slotownCloudWizardDone( int res )
{
    FolderMan *folderMan = FolderMan::instance();
    if( res == QDialog::Accepted ) {
        int cnt = folderMan->setupFolders();
        qDebug() << "Set up " << cnt << " folders.";
        // We have some sort of configuration. Enable autostart
        Utility::setLaunchOnStartup(_theme->appName(), _theme->appNameGUI(), true);
        if (cnt == 0) {
            // The folder configuration was skipped
            _gui->slotShowSettings();
        }
    }
    folderMan->setSyncEnabled( true );
    if( res == QDialog::Accepted ) {
        _checkConnectionTimer.start();
        slotCheckConnection();
    }

}

void Application::setupLogging()
{
    // might be called from second instance
    Logger::instance()->setLogFile(_logFile);
    Logger::instance()->setLogDir(_logDir);
    Logger::instance()->setLogExpire(_logExpire);
    Logger::instance()->setLogFlush(_logFlush);

    Logger::instance()->enterNextLogFile();

    qDebug() << QString::fromLatin1( "################## %1 %2 (%3) %4").arg(_theme->appName())
                .arg( QLocale::system().name() )
                .arg(property("ui_lang").toString())
                .arg(_theme->version());

}

void Application::slotUseMonoIconsChanged(bool)
{
    _gui->slotComputeOverallSyncStatus();
}

void Application::slotParseMessage(const QString &msg, QObject*)
{
    if (msg.startsWith(QLatin1String("MSG_PARSEOPTIONS:"))) {
        const int lengthOfMsgPrefix = 17;
        QStringList options = msg.mid(lengthOfMsgPrefix).split(QLatin1Char('|'));
        parseOptions(options);
        setupLogging();
    } else if (msg.startsWith(QLatin1String("MSG_SHOWSETTINGS"))) {
        showSettingsDialog();
    }
}

void Application::parseOptions(const QStringList &options)
{
    QStringListIterator it(options);
    // skip file name;
    if (it.hasNext()) it.next();

    //parse options; if help or bad option exit
    while (it.hasNext()) {
        QString option = it.next();
        if (option == QLatin1String("--help") || option == QLatin1String("-h")) {
            setHelp();
            break;
        } else if (option == QLatin1String("--logwindow") ||
                   option == QLatin1String("-l")) {
            _showLogWindow = true;
        } else if (option == QLatin1String("--logfile")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
               _logFile = it.next();
            } else {
                setHelp();
            }
        } else if (option == QLatin1String("--logdir")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
               _logDir = it.next();
            } else {
                setHelp();
            }
        } else if (option == QLatin1String("--logexpire")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                _logExpire = it.next().toInt();
            } else {
                setHelp();
            }
        } else if (option == QLatin1String("--logflush")) {
            _logFlush = true;
        } else if (option == QLatin1String("--confdir")) {
            if (it.hasNext() && !it.peekNext().startsWith(QLatin1String("--"))) {
                QString confDir = it.next();
                if (!ConfigFile::setConfDir( confDir )) {
                    std::cerr << "Invalid path passed to --confdir" << std::endl;
                    std::exit(1);
                }
            } else {
                showHelp();
            }
        } else if (option == QLatin1String("--debug")) {
            _debugMode = true;
        } else {
            setHelp();
            break;
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
    std::cout << qPrintable(t);
}
#endif

void Application::showHelp()
{
    setHelp();
    QString helpText;
    QTextStream stream(&helpText);
    stream << _theme->appName().toLatin1().constData()
           << QLatin1String(" version ")
           << _theme->version().toLatin1().constData() << endl;

    stream << QLatin1String("File synchronisation desktop utility.") << endl << endl
           << QLatin1String(optionsC);

    if (_theme->appName() == QLatin1String("ownCloud"))
        stream << endl << "For more information, see http://www.owncloud.org" << endl << endl;

    displayHelpText(helpText);
}

bool Application::debugMode()
{
    return _debugMode;
}

void Application::setHelp()
{
    _helpOnly = true;
}

#if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(5,0,0)
bool Application::winEventFilter(MSG *pMsg, long *result)
{
    if (pMsg->message == WM_POWERBROADCAST) {
        switch(pMsg->wParam) {
        case PBT_APMPOWERSTATUSCHANGE:
            qDebug() << "WM_POWERBROADCAST: Power state changed";
            break;
        case PBT_APMSUSPEND:
            qDebug() << "WM_POWERBROADCAST: Entering low power state";
            break;
        case PBT_APMRESUMEAUTOMATIC:
            qDebug() << "WM_POWERBROADCAST: Resuming from low power state";
            break;
        default:
            break;
        }
        return true;
    }

    return SharedTools::QtSingleApplication::winEventFilter(pMsg, result);
}
#endif

QString substLang(const QString &lang)
{
    // Map the more apropriate script codes
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

    foreach(QString lang, uiLanguages) {
        lang.replace(QLatin1Char('-'), QLatin1Char('_')); // work around QTBUG-25973
        lang = substLang(lang);
        const QString trPath = applicationTrPath();
        const QString trFile = QLatin1String("client_") + lang;
        if (translator->load(trFile, trPath) ||
            lang.startsWith(QLatin1String("en"))) {
            // Permissive approach: Qt and keychain translations
            // may be missing, but Qt translations must be there in order
            // for us to accept the language. Otherwise, we try with the next.
            // "en" is an exeption as it is the default language and may not
            // have a translation file provided.
            qDebug() << Q_FUNC_INFO << "Using" << lang << "translation";
            setProperty("ui_lang", lang);
            const QString qtTrPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
            const QString qtTrFile = QLatin1String("qt_") + lang;
            const QString qtBaseTrFile = QLatin1String("qtbase_") + lang;
            if (!qtTranslator->load(qtTrFile, qtTrPath)) {
                if (!qtTranslator->load(qtTrFile, trPath)) {
                    qtTranslator->load(qtBaseTrFile, trPath);
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

void Application::showSettingsDialog()
{
    _gui->slotShowSettings();
}


} // namespace OCC

