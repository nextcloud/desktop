/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include "mirall/application.h"
#include "mirall/systray.h"
#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/folderwizard.h"
#include "mirall/networklocation.h"
#include "mirall/owncloudfolder.h"
#include "mirall/owncloudsetupwizard.h"
#include "mirall/owncloudinfo.h"
#include "mirall/sslerrordialog.h"
#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/updatedetector.h"
#include "mirall/credentialstore.h"
#include "mirall/logger.h"
#include "mirall/settingsdialog.h"
#include "mirall/utility.h"
#include "mirall/inotify.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#include <QtCore>
#include <QtGui>
#include <QHash>
#include <QHashIterator>
#include <QUrl>
#include <QDesktopServices>
#include <QTranslator>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>

namespace Mirall {

// application logging handler.
void mirallLogCatcher(QtMsgType type, const char *msg)
{
  Q_UNUSED(type)
  // qDebug() exports to local8Bit, which is not always UTF-8
  Logger::instance()->mirallLog( QString::fromLocal8Bit(msg) );
}

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
    // FIXME - proper path!
    return QLatin1String("/usr/share/mirall/i18n/");
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
    _tray(0),
#if QT_VERSION >= 0x040700
    _networkMgr(new QNetworkConfigurationManager(this)),
#endif
    _sslErrorDialog(0),
    _contextMenu(0),
    _theme(Theme::instance()),
    _logBrowser(0),
    _logExpire(0),
    _showLogWindow(false),
    _logFlush(false),
    _helpOnly(false)
{
    setApplicationName( _theme->appNameGUI() );
    setWindowIcon( _theme->applicationIcon() );

    parseOptions(arguments());
    setupTranslations();
    setupLogBrowser();
    //no need to waste time;
    if ( _helpOnly ) return;

    connect( this, SIGNAL(messageReceived(QString)), SLOT(slotParseOptions(QString)));
    connect( Logger::instance(), SIGNAL(guiLog(QString,QString)),
             this, SLOT(slotShowTrayMessage(QString,QString)));
    // create folder manager for sync folder management
    _folderMan = new FolderMan(this);
    connect( _folderMan, SIGNAL(folderSyncStateChange(QString)),
             this,SLOT(slotSyncStateChange(QString)));
    _folderMan->setSyncEnabled(false);

    /* use a signal mapper to map the open requests to the alias names */
    _folderOpenActionMapper = new QSignalMapper(this);
    connect(_folderOpenActionMapper, SIGNAL(mapped(const QString &)),
            this, SLOT(slotFolderOpenAction(const QString &)));

    setQuitOnLastWindowClosed(false);

#if 0
#if QT_VERSION >= 0x040700
    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (const QNetworkConfiguration& netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }
#endif
#endif

    MirallConfigFile cfg;
    _theme->setSystrayUseMonoIcons(cfg.monoIcons());
    connect (_theme, SIGNAL(systrayUseMonoIconsChanged(bool)), SLOT(slotUseMonoIconsChanged(bool)));

    setupActions();
    setupSystemTray();
    slotSetupProxy();

    int cnt = _folderMan->setupFolders();

    QTimer::singleShot( 0, this, SLOT( slotStartFolderSetup() ));

    if( !cfg.ownCloudSkipUpdateCheck() ) {
        QTimer::singleShot( 3000, this, SLOT( slotStartUpdateDetector() ));
    }

    connect( ownCloudInfo::instance(), SIGNAL(sslFailed(QNetworkReply*, QList<QSslError>)),
             this,SLOT(slotSSLFailed(QNetworkReply*, QList<QSslError>)));

    connect( ownCloudInfo::instance(), SIGNAL(quotaUpdated(qint64,qint64)),
             SLOT(slotRefreshQuotaDisplay(qint64, qint64)));

    qDebug() << "Network Location: " << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    delete _tray; // needed, see ctor

    qDebug() << "* Mirall shutdown";
}

void Application::slotStartUpdateDetector()
{
    UpdateDetector *updateDetector = new UpdateDetector(this);
    updateDetector->versionCheck(_theme);

}

void Application::slotStartFolderSetup( int result )
{
    if( result == QDialog::Accepted ) {
        if( ownCloudInfo::instance()->isConfigured() ) {
            connect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                     SLOT(slotOwnCloudFound(QString,QString,QString,QString)));

            connect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                     SLOT(slotNoOwnCloudFound(QNetworkReply*)));

            ownCloudInfo::instance()->checkInstallation();
        } else {
            slotCheckConfig();
        }
    } else {
        qDebug() << "Setup Wizard was canceled. No reparsing of config.";
    }
}

void Application::slotOwnCloudFound( const QString& url, const QString& versionStr, const QString& version, const QString& edition)
{
    qDebug() << "** Application: ownCloud found: " << url << " with version " << versionStr << "(" << version << ")";
    // now check the authentication
    MirallConfigFile cfgFile;
    cfgFile.setOwnCloudVersion( version );
    // disconnect from ownCloudInfo
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                this, SLOT(slotOwnCloudFound(QString,QString,QString,QString)));

    disconnect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                this, SLOT(slotNoOwnCloudFound(QNetworkReply*)));

    if( version.startsWith("4.0") ) {
        QMessageBox::warning(0, tr("%1 Server Mismatch").arg(_theme->appNameGUI()),
                             tr("<p>The configured server for this client is too old.</p>"
                                "<p>Please update to the latest %1 server and restart the client.</p>").arg(_theme->appNameGUI()));
        return;
    }

    QTimer::singleShot( 0, this, SLOT( slotFetchCredentials() ));
}

void Application::slotNoOwnCloudFound( QNetworkReply* reply )
{
    Q_UNUSED(reply)

    qDebug() << "** Application: NO ownCloud found! Going offline";

    // Disconnect.
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                this, SLOT(slotOwnCloudFound(QString,QString,QString,QString)));

    disconnect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                this, SLOT(slotNoOwnCloudFound(QNetworkReply*)));

    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
                this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    setupContextMenu();
    QTimer::singleShot( 30*1000, this, SLOT( slotStartFolderSetup() ));
}

void Application::slotFetchCredentials()
{
    QString trayMessage;

    if( CredentialStore::instance()->state() == CredentialStore::Ok ) {
        // the credentials are still valid and ok.
        slotCredentialsFetched( true );
    } else {
        if( CredentialStore::instance()->canTryAgain() ) {
            connect( CredentialStore::instance(), SIGNAL(fetchCredentialsFinished(bool)),
                     this, SLOT(slotCredentialsFetched(bool)) );
            CredentialStore::instance()->fetchCredentials();
            if( CredentialStore::instance()->state() == CredentialStore::TooManyAttempts ) {
                trayMessage = tr("Too many incorrect password attempts.");
            }
        } else {
            qDebug() << "Can not try again to fetch Credentials.";
            trayMessage = tr("%1 user credentials are wrong. Please check configuration.")
                    .arg(Theme::instance()->appNameGUI());
        }

        if( !trayMessage.isEmpty() ) {
            slotShowTrayMessage(tr("Credentials"), trayMessage);
        }
    }
}

void Application::slotCredentialsFetched(bool ok)
{
    qDebug() << "Credentials successfully fetched: " << ok;
    if( ! ok ) {
        QString trayMessage;
        trayMessage = tr("Error: Could not retrieve the password!");
        if( CredentialStore::instance()->state() == CredentialStore::UserCanceled ) {
            trayMessage = tr("Password dialog was canceled!");
        } else {
            trayMessage = CredentialStore::instance()->errorMessage();
        }

        if( !trayMessage.isEmpty() ) {
            slotShowTrayMessage(tr("Credentials"), trayMessage);
        }

        qDebug() << "Could not fetch credentials";
    } else {
        ownCloudInfo::instance()->setCredentials( CredentialStore::instance()->user(),
                                                  CredentialStore::instance()->password() );
        // Credential fetched ok.
        QTimer::singleShot( 0, this, SLOT( slotCheckAuthentication() ));
    }
    disconnect( CredentialStore::instance(), SIGNAL(fetchCredentialsFinished(bool)) );
}

void Application::slotCheckAuthentication()
{
    connect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    qDebug() << "# checking for authentication settings.";
    ownCloudInfo::instance()->getWebDAVPath(QLatin1String("/") ); // this call needs to be authenticated.
    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
}

void Application::slotAuthCheck( const QString& ,QNetworkReply *reply )
{
    bool ok = true;

    if( reply->error() == QNetworkReply::AuthenticationRequiredError ) { // returned if the user is wrong.
        qDebug() << "******** Password is wrong!";
        QMessageBox::warning(0, tr("No %1 Connection").arg(_theme->appNameGUI()),
                             tr("<p>Your %1 credentials are not correct.</p>"
                                "<p>Please correct them by starting the configuration dialog from the tray!</p>")
                             .arg(_theme->appNameGUI()));
        ok = false;
    } else if( reply->error() == QNetworkReply::OperationCanceledError ) {
        // the username was wrong and ownCloudInfo was closing the request after a couple of auth tries.
        qDebug() << "******** Username or password is wrong!";
        QMessageBox::warning(0, tr("No %1 Connection").arg(_theme->appNameGUI()),
                             tr("<p>Either your user name or your password are not correct.</p>"
                                "<p>Please correct it by starting the configuration dialog from the tray!</p>"));
        ok = false;
    }

    // disconnect from o`   wnCloud Info signals
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    if( ok ) {
        qDebug() << "######## Credentials are ok!";
        _folderMan->setSyncEnabled(true);
        QMetaObject::invokeMethod(_folderMan, "slotScheduleFolderSync");

        _tray->setIcon( _theme->syncStateIcon( SyncResult::NotYetStarted, true ) );
        _tray->show();

        int cnt = _folderMan->map().size();
        if( _tray )
            _tray->showMessage(tr("%1 Sync Started").arg(_theme->appNameGUI()),
                               tr("Sync started for %1 configured sync folder(s).").arg(cnt));

        // queue up the sync for all folders.
        _folderMan->slotScheduleAllFolders();

        computeOverallSyncStatus();

        setupContextMenu();
    }
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
    if( res == QDialog::Accepted ) {
        int cnt = _folderMan->setupFolders();
        qDebug() << "Set up " << cnt << " folders.";
// FIXME!
//        _statusDialog->setFolderList( _folderMan->map() );
    }
    _folderMan->setSyncEnabled( true );
    slotStartFolderSetup( res );
}

void Application::setupActions()
{
    _actionOpenoC = new QAction(tr("Open %1 in browser").arg(_theme->appNameGUI()), this);
    QObject::connect(_actionOpenoC, SIGNAL(triggered(bool)), SLOT(slotOpenOwnCloud()));
    _actionQuota = new QAction(tr("Calculating quota..."), this);
    _actionQuota->setEnabled( false );
    _actionSettings = new QAction(tr("Settings..."), this);
    QObject::connect(_actionSettings, SIGNAL(triggered(bool)), SLOT(slotSettings()));
    _actionHelp = new QAction(tr("Help"), this);
    QObject::connect(_actionHelp, SIGNAL(triggered(bool)), SLOT(slotHelp()));
    _actionQuit = new QAction(tr("Quit"), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), SLOT(quit()));
}

void Application::setupSystemTray()
{
    // Setting a parent heres will crash on X11 since by the time qapp runs
    // its childrens dtors, the X11->screen variable queried for is gone -> crash
    _tray = new Systray();
    _tray->setIcon( _theme->syncStateIcon( SyncResult::NotYetStarted, true ) );

    connect(_tray,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            SLOT(slotTrayClicked(QSystemTrayIcon::ActivationReason)));

    setupContextMenu();

    _tray->show();
}

void Application::setupContextMenu()
{
    bool isConfigured = ownCloudInfo::instance()->isConfigured();

    _actionOpenoC->setEnabled(isConfigured);

    if( _contextMenu ) {
        _contextMenu->clear();
    } else {
        _contextMenu = new QMenu();
        // this must be called only once after creating the context menu, or
        // it will trigger a bug in Ubuntu's SNI bridge patch (11.10, 12.04).
        _tray->setContextMenu(_contextMenu);
    }
    _contextMenu->setTitle(_theme->appNameGUI() );
    _contextMenu->addAction(_actionOpenoC);

    int folderCnt = _folderMan->map().size();
    // add open actions for all sync folders to the tray menu
    if( _theme->singleSyncFolder() ) {
        // there should be exactly one folder. No sync-folder add action will be shown.
        QStringList li = _folderMan->map().keys();
        if( li.size() == 1 ) {
            Folder *folder = _folderMan->map().value(li.first());
            if( folder ) {
                // if there is singleFolder mode, a generic open action is displayed.
                QAction *action = new QAction( tr("Open local folder '%1'").arg(_theme->appNameGUI()), this);
                action->setIcon( _theme->trayFolderIcon( folder->backend()) );

                connect( action, SIGNAL(triggered()),_folderOpenActionMapper,SLOT(map()));
                _folderOpenActionMapper->setMapping( action, folder->alias() );

                _contextMenu->addAction(action);
            }
        }
    } else {
        // show a grouping with more than one folder.
        if ( folderCnt > 1) {
            _contextMenu->addAction(tr("Managed Folders:"))->setDisabled(true);
        }
        foreach (Folder *folder, _folderMan->map() ) {
            QAction *action = new QAction( tr("Open folder '%1'").arg(folder->alias()), this );
            connect( action, SIGNAL(triggered()),_folderOpenActionMapper,SLOT(map()));
            _folderOpenActionMapper->setMapping( action, folder->alias() );

            _contextMenu->addAction(action);
        }
    }

    _contextMenu->addSeparator();
    _contextMenu->addAction(_actionQuota);
    _contextMenu->addSeparator();
    _contextMenu->addAction(_actionSettings);
    _contextMenu->addAction(_actionHelp);
    _contextMenu->addSeparator();

    _contextMenu->addAction(_actionQuit);
}

void Application::setupLogBrowser()
{
    // might be called from second instance
    if (!_logBrowser) {
        // init the log browser.
        _logBrowser = new LogBrowser;
        qInstallMsgHandler( mirallLogCatcher );
        // ## TODO: allow new log name maybe?
        if (!_logDirectory.isEmpty()) {
            enterNextLogFile();
        } else if (!_logFile.isEmpty()) {
            qDebug() << "Logging into logfile: " << _logFile << " with flush " << _logFlush;
            _logBrowser->setLogFile( _logFile, _logFlush );
        }
    }

    if (_showLogWindow)
        slotOpenLogBrowser();

    qDebug() << QString::fromLatin1( "################## %1 %2 (%3) %4").arg(_theme->appName())
                .arg( QLocale::system().name() )
                .arg(property("ui_lang").toString())
                .arg(_theme->version());

}

void Application::enterNextLogFile()
{
    if (_logBrowser && !_logDirectory.isEmpty()) {
        QDir dir(_logDirectory);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // Find out what is the file with the highest nymber if any
        QStringList files = dir.entryList(QStringList("owncloud.log.*"),
                                    QDir::Files);
        QRegExp rx("owncloud.log.(\\d+)");
        uint maxNumber = 0;
        QDateTime now = QDateTime::currentDateTime();
        foreach(const QString &s, files) {
            if (rx.exactMatch(s)) {
                maxNumber = qMax(maxNumber, rx.cap(1).toUInt());
                if (_logExpire > 0) {
                    QFileInfo fileInfo = dir.absoluteFilePath(s);
                    if (fileInfo.lastModified().addSecs(60*60 * _logExpire) < now) {
                        dir.remove(s);
                    }
                }
            }
        }

        QString filename = _logDirectory + "/owncloud.log." + QString::number(maxNumber+1);
        _logBrowser->setLogFile(filename  , _logFlush);
    }
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
    _folderMan->setProxy();
}

void Application::slotRefreshQuotaDisplay( qint64 total, qint64 used )
{
    if (total == 0) {
        _actionQuota->setText(tr("Quota n/a"));
        return;
    }

    double percent = used/(double)total * 100;
    QString percentFormatted;
    // Don't display floating point numbers. Nobody cares.
    if ((int)percent == 0) {
        if (percent == .0) {
            percentFormatted = QLatin1String("0");
        } else {
            percentFormatted = QLatin1String("<1");
        }
    } else {
        percentFormatted = QString::number((int)percent);
    }
    QString totalFormatted = Utility::octetsToString(total);
    _actionQuota->setText(tr("%1% of %2 used").arg(percentFormatted).arg(totalFormatted));
}

void Application::slotUseMonoIconsChanged(bool)
{
    computeOverallSyncStatus();
}

void Application::slotHelp()
{
    QDesktopServices::openUrl(QUrl(_theme->helpUrl()));
}

/*
 * open the folder with the given Alais
 */
void Application::slotFolderOpenAction( const QString& alias )
{
    Folder *f = _folderMan->folder(alias);
    qDebug() << "opening local url " << f->path();
    if( f ) {
        QUrl url(f->path(), QUrl::TolerantMode);
        url.setScheme( QLatin1String("file") );

#ifdef Q_OS_WIN32
        // work around a bug in QDesktopServices on Win32, see i-net
        QString filePath = f->path();

        if (filePath.startsWith(QLatin1String("\\\\")) || filePath.startsWith(QLatin1String("//")))
            url.setUrl(QDir::toNativeSeparators(filePath));
        else
            url = QUrl::fromLocalFile(filePath);
#endif
        QDesktopServices::openUrl(url);
    }
}

void Application::slotOpenOwnCloud()
{
  MirallConfigFile cfgFile;

  QString url = cfgFile.ownCloudUrl();
  QDesktopServices::openUrl( url );
}

void Application::slotTrayClicked( QSystemTrayIcon::ActivationReason reason )
{
    // A click on the tray icon should only open the status window on Win and
    // Linux, not on Mac. They want a menu entry.
    // If the user canceled login, rather open the login window.
    if( CredentialStore::instance()->state() == CredentialStore::UserCanceled ||
            CredentialStore::instance()->state() == CredentialStore::Error ) {
        slotFetchCredentials();
    }
#if defined Q_WS_WIN || defined Q_WS_X11
    if( reason == QSystemTrayIcon::Trigger ) {
        slotCheckConfig();
    }
#endif
}

void Application::slotCheckConfig()
{
    // if no config file is there, start the configuration wizard.
    MirallConfigFile cfgFile;

    if( cfgFile.exists() ) {
        slotSettings();
    } else {
        qDebug() << "No configured folders yet, starting setup wizard";
        OwncloudSetupWizard::runWizard(_folderMan, this, SLOT(slotownCloudWizardDone(int)));
    }
}

void Application::slotOpenLogBrowser()
{
    _logBrowser->show();
    _logBrowser->raise();
}

void Application::slotFoldersChanged()
{
    computeOverallSyncStatus();
    setupContextMenu();
}

void Application::slotSettings()
{
    if (_settingsDialog.isNull()) {
        _settingsDialog = new SettingsDialog(this);
        connect(_settingsDialog, SIGNAL(accepted()), _settingsDialog, SLOT(deleteLater()));
        _settingsDialog->open();
    } else {
        Utility::raiseDialog(_settingsDialog);
    }
}

void Application::slotParseOptions(const QString &opts)
{
    QStringList options = opts.split(QLatin1Char('|'));
    parseOptions(options);
    setupLogBrowser();
}

void Application::slotShowTrayMessage(const QString &title, const QString &msg)
{
    if( _tray )
        _tray->showMessage(title, msg);
    else
        qDebug() << "Tray not ready: " << msg;
}

void Application::slotSyncStateChange( const QString& alias )
{
    SyncResult result = _folderMan->syncResult( alias );

    emit folderStateChanged( _folderMan->folder(alias) );

    computeOverallSyncStatus();

    qDebug() << "Sync state changed for folder " << alias << ": "  << result.statusString();

    if (result.status() == SyncResult::Success || result.status() == SyncResult::Error) {
        enterNextLogFile();
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
                _logDirectory = it.next();
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

void Application::computeOverallSyncStatus()
{

    // display the info of the least successful sync (eg. not just display the result of the latest sync
    SyncResult overallResult(SyncResult::Undefined );
    QMap<QString, QString> overallStatusStrings;
    QString trayMessage;
    Folder::Map map = _folderMan->map();

    foreach ( Folder *syncedFolder, map.values() ) {
        QString folderMessage;

        SyncResult folderResult = syncedFolder->syncResult();
        SyncResult::Status syncStatus = folderResult.status();

        switch( syncStatus ) {
        case SyncResult::Undefined:
            if ( overallResult.status() != SyncResult::Error ) {
                overallResult.setStatus(SyncResult::Error);
            }
            folderMessage = tr( "Undefined State." );
            break;
        case SyncResult::NotYetStarted:
            folderMessage = tr( "Waits to start syncing." );
            overallResult.setStatus( SyncResult::NotYetStarted );
            break;
        case SyncResult::SyncPrepare:
            folderMessage = tr( "Preparing for sync." );
            overallResult.setStatus( SyncResult::SyncPrepare );
            break;
        case SyncResult::SyncRunning:
            folderMessage = tr( "Sync is running." );
            overallResult.setStatus( SyncResult::SyncRunning );
            break;
        case SyncResult::Unavailable:
            folderMessage = tr( "Server is currently not available." );
            overallResult.setStatus( SyncResult::Unavailable );
            break;
        case SyncResult::Success:
            if( overallResult.status() == SyncResult::Undefined ) {
                overallResult.setStatus( SyncResult::Success );
            }
            folderMessage = tr( "Last Sync was successful." );
            break;
        case SyncResult::Error:
            overallResult.setStatus( SyncResult::Error );
            folderMessage = tr( "Syncing Error." );
            break;
        case SyncResult::SetupError:
            if ( overallResult.status() != SyncResult::Error ) {
                overallResult.setStatus( SyncResult::SetupError );
            }
            folderMessage = tr( "Setup Error." );
            break;
        default:
            folderMessage = tr( "Undefined Error State." );
            overallResult.setStatus( SyncResult::Error );
        }
        if( !syncedFolder->syncEnabled() ) {
            // sync is disabled.
            folderMessage += tr( " (Sync is paused)" );
        }

        qDebug() << "Folder in overallStatus Message: " << syncedFolder << " with name " << syncedFolder->alias();
        QString msg = tr("Folder %1: %2").arg(syncedFolder->alias(), folderMessage);
        overallStatusStrings[syncedFolder->alias()] = msg;
    }

    // create the tray blob message, check if we have an defined state
    if( overallResult.status() != SyncResult::Undefined ) {
        QStringList allStatusStrings = overallStatusStrings.values();
        if( ! allStatusStrings.isEmpty() )
            trayMessage = allStatusStrings.join(QLatin1String("\n"));
        else
            trayMessage = tr("No sync folders configured.");

        QIcon statusIcon = _theme->syncStateIcon( overallResult.status(), true); // size 48 before

        _tray->setIcon( statusIcon );
        _tray->setToolTip(trayMessage);
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
        stream << endl << "For more information, see http://www.owncloud.org" << endl;

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

