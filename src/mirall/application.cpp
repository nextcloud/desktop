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

#include <iostream>

#include "config.h"

#include "mirall/application.h"
#include "mirall/folder.h"
#include "mirall/folderman.h"
#include "mirall/folderwatcher.h"
#include "mirall/networklocation.h"
#include "mirall/folder.h"
#include "mirall/owncloudsetupwizard.h"
#include "mirall/owncloudinfo.h"
#include "mirall/sslerrordialog.h"
#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/updatedetector.h"
#include "mirall/logger.h"
#include "mirall/utility.h"
#include "mirall/connectionvalidator.h"

#include "creds/abstractcredentials.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#include <QHash>
#include <QHashIterator>
#include <QUrl>
#include <QDesktopServices>
#include <QTranslator>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QMenu>
#include <QMessageBox>

namespace Mirall {

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
#ifdef Q_OS_LINUX
    return QString::fromLatin1(DATADIR"/"APPLICATION_EXECUTABLE"/i18n/");
#endif
#ifdef Q_OS_MAC
    return QApplication::applicationDirPath()+QLatin1String("/../Resources/Translations"); // path defaults to app dir.
#endif
#ifdef Q_OS_WIN32
   return QApplication::applicationDirPath();
#endif
}
}

// ----------------------------------------------------------------------------------

Application::Application(int &argc, char **argv) :
    SharedTools::QtSingleApplication(argc, argv),
    _gui(0),
    // _networkMgr(new QNetworkConfigurationManager(this)), <- Not used yet.
    _sslErrorDialog(0),
    _theme(Theme::instance()),
    _helpOnly(false),
    _showLogWindow(false),
    _logExpire(0),
    _logFlush(false)
{
    setApplicationName( _theme->appNameGUI() );
    setWindowIcon( _theme->applicationIcon() );

    parseOptions(arguments());
    //no need to waste time;
    if ( _helpOnly ) return;

    _gui = new ownCloudGui(this);
    if( _showLogWindow ) {
        _gui->slotToggleLogBrowser(); // _showLogWindow is set in parseOptions.
    }
    connect( _gui, SIGNAL(setupProxy()), SLOT(slotSetupProxy()));

    setupLogging();
    setupTranslations();

    connect( this, SIGNAL(messageReceived(QString)), SLOT(slotParseOptions(QString)));
    connect( Logger::instance(), SIGNAL(guiLog(QString,QString)),
             _gui, SLOT(slotShowTrayMessage(QString,QString)));
    connect( Logger::instance(), SIGNAL(optionalGuiLog(QString,QString)),
             _gui, SLOT(slotShowOptionalTrayMessage(QString,QString)));
    connect( Logger::instance(), SIGNAL(guiMessage(QString,QString)),
             _gui, SLOT(slotShowGuiMessage(QString,QString)));

    FolderMan::instance()->setSyncEnabled(false);

    setQuitOnLastWindowClosed(false);

    qRegisterMetaType<Progress::Kind>("Progress::Kind");
    qRegisterMetaType<Progress::Info>("Progress::Info");
#if 0
    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (const QNetworkConfiguration& netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }
#endif

    MirallConfigFile cfg;
    _theme->setSystrayUseMonoIcons(cfg.monoIcons());
    connect (_theme, SIGNAL(systrayUseMonoIconsChanged(bool)), SLOT(slotUseMonoIconsChanged(bool)));

    slotSetupProxy();

    FolderMan::instance()->setupFolders();

    // startup procedure.
    QTimer::singleShot( 0, this, SLOT( slotCheckConnection() ));

    if( !cfg.ownCloudSkipUpdateCheck() ) {
        QTimer::singleShot( 3000, this, SLOT( slotStartUpdateDetector() ));
    }

    connect( ownCloudInfo::instance(), SIGNAL(sslFailed(QNetworkReply*, QList<QSslError>)),
             this,SLOT(slotSSLFailed(QNetworkReply*, QList<QSslError>)));

    connect (this, SIGNAL(aboutToQuit()), SLOT(slotCleanup()));

    qDebug() << "Network Location: " << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    // qDebug() << "* Mirall shutdown";
}

void Application::slotCleanup()
{
    // explicitly close windows. This is somewhat of a hack to ensure
    // that saving the geometries happens ASAP during a OS shutdown
    _gui->slotShutdown();
    _gui->deleteLater();

}

void Application::slotStartUpdateDetector()
{
    UpdateDetector *updateDetector = new UpdateDetector(this);
    updateDetector->versionCheck(_theme);
}

void Application::slotCheckConnection()
{
    if( _gui->checkConfigExists(false) ) {
        MirallConfigFile cfg;
        AbstractCredentials* credentials(cfg.getCredentials());

        if (! credentials->ready()) {
            connect( credentials, SIGNAL(fetched()),
                     this, SLOT(slotCredentialsFetched()));
            credentials->fetch();
        } else {
            runValidator();
        }
    } else {
        // the call to checkConfigExists opens the setup wizard
        // if the config does not exist. Nothing to do here.
    }
}

void Application::slotCredentialsFetched()
{
    MirallConfigFile cfg;
    AbstractCredentials* credentials(cfg.getCredentials());

    disconnect(credentials, SIGNAL(fetched()),
               this, SLOT(slotCredentialsFetched()));
    runValidator();
}

void Application::runValidator()
{
    _conValidator = new ConnectionValidator();
    connect( _conValidator, SIGNAL(connectionResult(ConnectionValidator::Status)),
             this, SLOT(slotConnectionValidatorResult(ConnectionValidator::Status)) );
    _conValidator->checkConnection();
}

void Application::slotConnectionValidatorResult(ConnectionValidator::Status status)
{
    qDebug() << "Connection Validator Result: " << _conValidator->statusString(status);
    QStringList startupFails;

    if( status == ConnectionValidator::Connected ) {
        FolderMan *folderMan = FolderMan::instance();
        qDebug() << "######## Connection and Credentials are ok!";
        folderMan->setSyncEnabled(true);
        // queue up the sync for all folders.
        folderMan->slotScheduleAllFolders();
    } else {
        // if we have problems here, it's unlikely that syncing will work.
        FolderMan::instance()->setSyncEnabled(false);

        startupFails = _conValidator->errors();
    }
    _gui->startupConnected( (status == ConnectionValidator::Connected), startupFails);

    _conValidator->deleteLater();
}

void Application::slotSSLFailed( QNetworkReply *reply, QList<QSslError> errors )
{
    qDebug() << "SSL-Warnings happened for url " << reply->url().toString();

    if( ownCloudInfo::instance()->certsUntrusted() ) {
        // User decided once to untrust. Honor this decision.
        qDebug() << "Untrusted by user decision, returning.";
        return;
    }

    QString configHandle = ownCloudInfo::instance()->configHandle(reply);

    // make the ssl dialog aware of the custom config. It loads known certs.
    if( ! _sslErrorDialog ) {
        _sslErrorDialog = new SslErrorDialog;
    }
    _sslErrorDialog->setCustomConfigHandle( configHandle );

    if( _sslErrorDialog->setErrorList( errors ) ) {
        // all ssl certs are known and accepted. We can ignore the problems right away.
        qDebug() << "Certs are already known and trusted, Warnings are not valid.";
        reply->ignoreSslErrors();
    } else {
        if( _sslErrorDialog->exec() == QDialog::Accepted ) {
            if( _sslErrorDialog->trustConnection() ) {
                reply->ignoreSslErrors();
            } else {
                // User does not want to trust.
                ownCloudInfo::instance()->setCertsUntrusted(true);
            }
        } else {
            ownCloudInfo::instance()->setCertsUntrusted(true);
        }
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
// FIXME!
//        _statusDialog->setFolderList( folderMan->map() );
    }
    folderMan->setSyncEnabled( true );
    if( res == QDialog::Accepted ) {
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

QNetworkProxy proxyFromConfig(const MirallConfigFile& cfg)
{
    QNetworkProxy proxy;

    if (cfg.proxyHostName().isEmpty())
        return QNetworkProxy();

    proxy.setHostName(cfg.proxyHostName());
    proxy.setPort(cfg.proxyPort());
    if (cfg.proxyNeedsAuth()) {
        proxy.setUser(cfg.proxyUser());
        proxy.setPassword(cfg.proxyPassword());
    }
    return proxy;
}

void Application::slotSetupProxy()
{
    Mirall::MirallConfigFile cfg;
    int proxyType = cfg.proxyType();
    QNetworkProxy proxy = proxyFromConfig(cfg);

    switch(proxyType) {
    case QNetworkProxy::NoProxy:
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        break;
    case QNetworkProxy::DefaultProxy:
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        break;
    case QNetworkProxy::Socks5Proxy:
        proxy.setType(QNetworkProxy::Socks5Proxy);
        QNetworkProxy::setApplicationProxy(proxy);
        break;
    case QNetworkProxy::HttpProxy:
        proxy.setType(QNetworkProxy::HttpProxy);
        QNetworkProxy::setApplicationProxy(proxy);
        break;
    default:
        break;
    }

    FolderMan::instance()->setDirtyProxy(true);
    FolderMan::instance()->slotScheduleAllFolders();
}

void Application::slotUseMonoIconsChanged(bool)
{
    _gui->slotComputeOverallSyncStatus();
}

void Application::slotParseOptions(const QString &opts)
{
    QStringList options = opts.split(QLatin1Char('|'));
    parseOptions(options);
    setupLogging();
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
                MirallConfigFile::setConfDir( confDir );
            } else {
                showHelp();
            }
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

void Application::setHelp()
{
    _helpOnly = true;
}

#if defined(Q_OS_WIN)
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
        const QString trFile = QLatin1String("mirall_") + lang;
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
            if (qtTranslator->load(qtTrFile, qtTrPath)) {
                qtTranslator->load(qtTrFile, trPath);
            }
            const QString qtkeychainFile = QLatin1String("qt_") + lang;
            if (!qtkeychainTranslator->load(qtkeychainFile, qtTrPath)) {
               qtkeychainTranslator->load(qtkeychainFile, trPath);
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
} // namespace Mirall

