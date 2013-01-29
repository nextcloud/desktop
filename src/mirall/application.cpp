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
#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/folderwizard.h"
#include "mirall/networklocation.h"
#include "mirall/unisonfolder.h"
#include "mirall/owncloudfolder.h"
#include "mirall/statusdialog.h"
#include "mirall/owncloudsetupwizard.h"
#include "mirall/owncloudinfo.h"
#include "mirall/sslerrordialog.h"
#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/updatedetector.h"
#include "mirall/proxydialog.h"
#include "mirall/version.h"
#include "mirall/credentialstore.h"
#include "mirall/logger.h"

// for version information
#include "config.h"
#include <csync.h>

#ifdef WITH_CSYNC
#include "mirall/csyncfolder.h"
#endif
#include "mirall/inotify.h"

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
  Logger::instance()->mirallLog( QString::fromUtf8(msg) );
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
    _updateDetector(0),
    _logBrowser(0),
    _showLogWindow(false),
    _logFlush(false),
    _helpOnly(false),
    _fileItemDialog(0)
{
    setApplicationName( _theme->appName() );
    setWindowIcon( _theme->applicationIcon() );

    parseOptions(arguments());
    setupLogBrowser();
    //no need to waste time;
    if ( _helpOnly ) return;

    QString locale = Theme::instance()->enforcedLocale();
    if (locale.isEmpty()) locale = QLocale::system().name();

    QTranslator *qtTranslator = new QTranslator(this);
#if defined(Q_OS_MAC)
    qtTranslator->load(QLatin1String("qt_") + locale, applicationDirPath()+QLatin1String("/../translations") ); // path defaults to app dir.
#elif defined(Q_OS_WIN32)
    qtTranslator->load(QLatin1String("qt_") + locale, applicationDirPath());
#endif
    if (qtTranslator->isEmpty()) {
        qtTranslator->load(QLatin1String("qt_") + QLocale::system().name(),
                           QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    }
    installTranslator(qtTranslator);

    QTranslator *mirallTranslator = new QTranslator(this);

#ifdef Q_OS_LINUX
    // FIXME - proper path!
    mirallTranslator->load(QLatin1String("mirall_") + locale, QLatin1String("/usr/share/mirall/i18n/"));
#endif
#ifdef Q_OS_MAC
    mirallTranslator->load(QLatin1String("mirall_") + locale, applicationDirPath()+QLatin1String("/../translations") ); // path defaults to app dir.
#endif
#ifdef Q_OS_WIN32
    mirallTranslator->load(QLatin1String("mirall_") + locale, applicationDirPath());
#endif

    installTranslator(mirallTranslator);

    connect( this, SIGNAL(messageReceived(QString)), SLOT(slotParseOptions(QString)));
    connect( Logger::instance(), SIGNAL(guiLog(QString,QString)),
             this, SLOT(slotShowTrayMessage(QString,QString)));
    // create folder manager for sync folder management
    _folderMan = new FolderMan(this);
    connect( _folderMan, SIGNAL(folderSyncStateChange(QString)),
             this,SLOT(slotSyncStateChange(QString)));

    /* use a signal mapper to map the open requests to the alias names */
    _folderOpenActionMapper = new QSignalMapper(this);
    connect(_folderOpenActionMapper, SIGNAL(mapped(const QString &)),
            this, SLOT(slotFolderOpenAction(const QString &)));

    setQuitOnLastWindowClosed(false);

    _folderWizard = new FolderWizard;

    _owncloudSetupWizard = new OwncloudSetupWizard( _folderMan, _theme, this );
    connect( _owncloudSetupWizard, SIGNAL(ownCloudWizardDone(int)),
             this, SLOT(slotownCloudWizardDone(int)));

    _statusDialog = new StatusDialog( _theme );
    connect( _statusDialog, SIGNAL(addASync()), this, SLOT(slotAddFolder()) );

    connect( _statusDialog, SIGNAL(removeFolderAlias( const QString&)),
             SLOT(slotRemoveFolder(const QString&)));

    connect( _statusDialog, SIGNAL(openLogBrowser()), this, SLOT(slotOpenLogBrowser()));

    connect( _statusDialog, SIGNAL(enableFolderAlias(QString,bool)),
             SLOT(slotEnableFolder(QString,bool)));
    connect( _statusDialog, SIGNAL(infoFolderAlias(const QString&)),
             SLOT(slotInfoFolder( const QString&)));
    connect( _statusDialog, SIGNAL(openFolderAlias(const QString&)),
             SLOT(slotFolderOpenAction(QString)));

#if 0
#if QT_VERSION >= 0x040700
    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (const QNetworkConfiguration& netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }
#endif
#endif
    setupActions();
    setupSystemTray();
    setupProxy();

    QObject::connect( this, SIGNAL(messageReceived(QString)),
                         this, SLOT(slotOpenStatus()) );

    QTimer::singleShot( 0, this, SLOT( slotStartFolderSetup() ));

    MirallConfigFile cfg;
    if( !cfg.ownCloudSkipUpdateCheck() ) {
        QTimer::singleShot( 3000, this, SLOT( slotStartUpdateDetector() ));
    }

    connect( ownCloudInfo::instance(), SIGNAL(sslFailed(QNetworkReply*, QList<QSslError>)),
             this,SLOT(slotSSLFailed(QNetworkReply*, QList<QSslError>)));

    qDebug() << "Network Location: " << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    delete _tray; // needed, see ctor
    if( _fileItemDialog) delete _fileItemDialog;
    if( _statusDialog && ! _helpOnly)  delete _statusDialog;
    qDebug() << "* Mirall shutdown";
}

void Application::slotStartUpdateDetector()
{
    _updateDetector = new UpdateDetector(this);
    _updateDetector->versionCheck(_theme);

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
            _owncloudSetupWizard->startWizard(true); // with intro
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
        QMessageBox::warning(0, tr("%1 Server Mismatch").arg(_theme->appName()),
                             tr("<p>The configured server for this client is too old.</p>"
                                "<p>Please update to the latest %1 server and restart the client.</p>").arg(_theme->appName()));
        return;
    }

    QTimer::singleShot( 0, this, SLOT( slotFetchCredentials() ));
}

void Application::slotNoOwnCloudFound( QNetworkReply* reply )
{
    qDebug() << "** Application: NO ownCloud found!";
    QString msg;
    if( reply ) {
        QString url( reply->url().toString() );
        url.remove( QLatin1String("/status.php") );
        msg = tr("<p>The %1 at %2 could not be reached.</p>").arg(_theme->appName()).arg( url );
        msg += tr("<p>The detailed error message is<br/><tt>%1</tt></p>").arg( reply->errorString() );
    }
    msg += tr("<p>Please check your configuration by clicking on the tray icon.</p>");

    QMessageBox::warning(0, tr("%1 Connection Failed").arg(_theme->appName()), msg );
    _actionAddFolder->setEnabled( false );

    // Disconnect.
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                this, SLOT(slotOwnCloudFound(QString,QString,QString,QString)));

    disconnect( ownCloudInfo::instance(),SIGNAL(noOwncloudFound(QNetworkReply*)),
                this, SLOT(slotNoOwnCloudFound(QNetworkReply*)));

    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
                this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    setupContextMenu();
}

void Application::slotFetchCredentials()
{
    QString trayMessage;

    if( CredentialStore::instance()->canTryAgain() ) {
        connect( CredentialStore::instance(), SIGNAL(fetchCredentialsFinished(bool)),
                 this, SLOT(slotCredentialsFetched(bool)) );
        CredentialStore::instance()->fetchCredentials();
        if( CredentialStore::instance()->state() == CredentialStore::TooManyAttempts ) {
            trayMessage = tr("Too many attempts to get a valid password.");
        }
    } else {
        qDebug() << "Can not try again to fetch Credentials.";
        trayMessage = tr("%1 user credentials are wrong. Please check configuration.")
                .arg(Theme::instance()->appName());
    }

    if( !trayMessage.isEmpty() ) {
        _tray->showMessage(tr("Credentials"), trayMessage);
        _actionOpenStatus->setEnabled( false );
        _actionAddFolder->setEnabled( false );
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
            _tray->showMessage(tr("Credentials"), trayMessage);
        }

        qDebug() << "Could not fetch credentials";
        _actionAddFolder->setEnabled( false );
        _actionOpenStatus->setEnabled( false );
    } else {
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
    ownCloudInfo::instance()->getRequest(QLatin1String("/"), true ); // this call needs to be authenticated.
    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
}

void Application::slotAuthCheck( const QString& ,QNetworkReply *reply )
{
    bool ok = true;

    if( reply->error() == QNetworkReply::AuthenticationRequiredError ) { // returned if the user is wrong.
        qDebug() << "******** Password is wrong!";
        QMessageBox::warning(0, tr("No %1 Connection").arg(_theme->appName()),
                             tr("<p>Your %1 credentials are not correct.</p>"
                                "<p>Please correct them by starting the configuration dialog from the tray!</p>")
                             .arg(_theme->appName()));
        _actionAddFolder->setEnabled( false );
        ok = false;
    } else if( reply->error() == QNetworkReply::OperationCanceledError ) {
        // the username was wrong and ownCloudInfo was closing the request after a couple of auth tries.
        qDebug() << "******** Username or password is wrong!";
        QMessageBox::warning(0, tr("No %1 Connection").arg(_theme->appName()),
                             tr("<p>Either your user name or your password are not correct.</p>"
                                "<p>Please correct it by starting the configuration dialog from the tray!</p>"));
        _actionAddFolder->setEnabled( false );
        ok = false;
    }

    // disconnect from ownCloud Info signals
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    if( ok ) {
        qDebug() << "######## Credentials are ok!";
        int cnt = _folderMan->setupFolders();
        if( cnt ) {
            _tray->setIcon( _theme->syncStateIcon( SyncResult::NotYetStarted, true ) );
            _tray->show();

            if( _tray )
                _tray->showMessage(tr("%1 Sync Started").arg(_theme->appName()),
                                   tr("Sync started for %1 configured sync folder(s).").arg(cnt));

            _statusDialog->setFolderList( _folderMan->map() );
            computeOverallSyncStatus();

        }
        _actionAddFolder->setEnabled( true );
        _actionOpenStatus->setEnabled( true );
        setupContextMenu();
    } else {
        slotFetchCredentials();
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

    }
    slotStartFolderSetup( res );
}

void Application::setupActions()
{
    _actionOpenoC = new QAction(tr("Open %1 in browser...").arg(_theme->appName()), this);
    QObject::connect(_actionOpenoC, SIGNAL(triggered(bool)), SLOT(slotOpenOwnCloud()));
    _actionOpenStatus = new QAction(tr("Open status..."), this);
    QObject::connect(_actionOpenStatus, SIGNAL(triggered(bool)), SLOT(slotOpenStatus()));
    _actionOpenStatus->setEnabled( false );
    _actionAddFolder = new QAction(tr("Add folder..."), this);
    QObject::connect(_actionAddFolder, SIGNAL(triggered(bool)), SLOT(slotAddFolder()));
    _actionConfigure = new QAction(tr("Configure..."), this);
    QObject::connect(_actionConfigure, SIGNAL(triggered(bool)), SLOT(slotConfigure()));
    _actionConfigureProxy = new QAction(tr("Configure proxy..."), this);
    QObject::connect(_actionConfigureProxy, SIGNAL(triggered(bool)), SLOT(slotConfigureProxy()));
    _actionAbout = new QAction(tr("About..."), this);
    QObject::connect(_actionAbout, SIGNAL(triggered(bool)), SLOT(slotAbout()));
    _actionQuit = new QAction(tr("Quit"), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), SLOT(quit()));
}

void Application::setupSystemTray()
{
    // Setting a parent heres will crash on X11 since by the time qapp runs
    // its childrens dtors, the X11->screen variable queried for is gone -> crash
    _tray = new QSystemTrayIcon;
    _tray->setIcon( _theme->syncStateIcon( SyncResult::NotYetStarted, true ) );

    connect(_tray,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            SLOT(slotTrayClicked(QSystemTrayIcon::ActivationReason)));

    setupContextMenu();

    _tray->show();
}

void Application::setupContextMenu()
{
    bool isConfigured = ownCloudInfo::instance()->isConfigured();

    _actionOpenStatus->setEnabled(isConfigured);
    _actionOpenoC->setEnabled(isConfigured);
    _actionAddFolder->setEnabled(isConfigured);

    if( _contextMenu ) {
        _contextMenu->clear();
    } else {
        _contextMenu = new QMenu();
        // this must be called only once after creating the context menu, or
        // it will trigger a bug in Ubuntu's SNI bridge patch (11.10, 12.04).
        _tray->setContextMenu(_contextMenu);
    }
    _contextMenu->setTitle(_theme->appName() );
    _contextMenu->addAction(_actionOpenStatus);
    _contextMenu->addAction(_actionOpenoC);

    _contextMenu->addSeparator();

    int folderCnt = _folderMan->map().size();
    // add open actions for all sync folders to the tray menu
    if( _theme->singleSyncFolder() ) {
        if( folderCnt == 0 ) {
            // if there is no folder configured yet, show the add action.
            _contextMenu->addAction(_actionAddFolder);
        } else {
            // there should be exactly one folder. No sync-folder add action will be shown.
            QStringList li = _folderMan->map().keys();
            if( li.size() == 1 ) {
                Folder *folder = _folderMan->map().value(li.first());
                if( folder ) {
                    // if there is singleFolder mode, a generic open action is displayed.
                    QAction *action = new QAction( tr("Open %1 folder").arg(_theme->appName()), this);
                    action->setIcon( _theme->trayFolderIcon( folder->backend()) );

                    connect( action, SIGNAL(triggered()),_folderOpenActionMapper,SLOT(map()));
                    _folderOpenActionMapper->setMapping( action, folder->alias() );

                    _contextMenu->addAction(action);
                }
            }
        }
    } else {
        // show a grouping with more than one folder.
        if ( folderCnt ) {
            _contextMenu->addAction(tr("Managed Folders:"))->setDisabled(true);
        }
        foreach (Folder *folder, _folderMan->map() ) {
            QAction *action = new QAction( folder->alias(), this );
            action->setIcon( _theme->trayFolderIcon( folder->backend()) );

            connect( action, SIGNAL(triggered()),_folderOpenActionMapper,SLOT(map()));
            _folderOpenActionMapper->setMapping( action, folder->alias() );

            _contextMenu->addAction(action);
        }
        _contextMenu->addAction(_actionAddFolder);
    }

    _contextMenu->addSeparator();
    _contextMenu->addAction(_actionConfigure);
    _contextMenu->addAction(_actionConfigureProxy);
    _contextMenu->addSeparator();

    if (!Theme::instance()->about().isEmpty()) {
        _contextMenu->addAction(_actionAbout);
        _contextMenu->addSeparator();
    }

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
        if (!_logFile.isEmpty()) {
            qDebug() << "Logging into logfile: " << _logFile << " with flush " << _logFlush;
            _logBrowser->setLogFile( _logFile, _logFlush );
        }
    }

    if (_showLogWindow)
        slotOpenLogBrowser();

    qDebug() << QString::fromLatin1( "################## %1 %2 %3 ").arg(_theme->appName())
                .arg( QLocale::system().name() )
                .arg(_theme->version());

}

void Application::setupProxy()
{
    //
    Mirall::MirallConfigFile cfg;
    int proxy = cfg.proxyType();

    switch(proxy) {
    case QNetworkProxy::NoProxy: {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        break;
    }
    case QNetworkProxy::DefaultProxy: {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        break;
    }

    case QNetworkProxy::Socks5Proxy: {
        proxy = QNetworkProxy::HttpProxy;
        cfg.setProxyType(proxy);
        // fall through
    }
    case QNetworkProxy::HttpProxy:{
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::HttpProxy);
        proxy.setHostName(cfg.proxyHostName());
        proxy.setPort(cfg.proxyPort());
        proxy.setUser(cfg.proxyUser());
        proxy.setPassword(cfg.proxyPassword());
        QNetworkProxy::setApplicationProxy(proxy);
        break;
    }
    }
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
    if( reason == QSystemTrayIcon::Trigger && _actionOpenStatus->isEnabled() ) {
        slotOpenStatus();
    }
#endif
}

void Application::slotAddFolder()
{
  _folderMan->disableFoldersWithRestore();

  Folder::Map folderMap = _folderMan->map();

  _folderWizard->setFolderMap( &folderMap );

  _folderWizard->restart();

  if (_folderWizard->exec() == QDialog::Accepted) {
    qDebug() << "* Folder wizard completed";

    bool goodData = true;

    QString alias        = _folderWizard->field(QLatin1String("alias")).toString();
    QString sourceFolder = _folderWizard->field(QLatin1String("sourceFolder")).toString();
    QString backend      = QLatin1String("csync");
    QString targetPath;
    bool onlyThisLAN = false;
    bool onlyOnline  = false;

    if (_folderWizard->field(QLatin1String("local?")).toBool()) {
        // setup a local csync folder
        targetPath = _folderWizard->field(QLatin1String("targetLocalFolder")).toString();
    } else if (_folderWizard->field(QLatin1String("remote?")).toBool()) {
        // setup a remote csync folder
        targetPath  = _folderWizard->field(QLatin1String("targetURLFolder")).toString();
        onlyOnline  = _folderWizard->field(QLatin1String("onlyOnline?")).toBool();
        onlyThisLAN = _folderWizard->field(QLatin1String("onlyThisLAN?")).toBool();
        (void) onlyOnline;
        (void) onlyThisLAN;
    } else if( _folderWizard->field(QLatin1String("OC?")).toBool() ||
               Theme::instance()->singleSyncFolder()) {
        // setup a ownCloud folder
        backend    = QLatin1String("owncloud");
        targetPath = _folderWizard->field(QLatin1String("targetOCFolder")).toString(); //empty in single folder mode
    } else {
      qWarning() << "* Folder not local and note remote?";
      goodData = false;
    }

    if( goodData ) {
        _folderMan->addFolderDefinition( backend, alias, sourceFolder, targetPath, onlyThisLAN );
        Folder *f = _folderMan->setupFolderFromConfigFile( alias );
        if( f ) {
            _statusDialog->slotAddFolder( f );
            _statusDialog->buttonsSetEnabled();
            setupContextMenu();
        }
    }

  } else {
    qDebug() << "* Folder wizard cancelled";
  }
  _folderMan->restoreEnabledFolders();
}

void Application::slotOpenStatus()
{
  if( ! _statusDialog ) return;

  QWidget *raiseWidget = 0;

  // check if there is a mirall.cfg already.
  if( _owncloudSetupWizard->wizard()->isVisible() ) {
    raiseWidget = _owncloudSetupWizard->wizard();
  }

  // if no config file is there, start the configuration wizard.
  if( ! raiseWidget ) {
    MirallConfigFile cfgFile;

    if( !cfgFile.exists() ) {
      qDebug() << "No configured folders yet, start the Owncloud integration dialog.";
      _owncloudSetupWizard->startWizard(true); // with intro
    } else {
      qDebug() << "#============# Status dialog starting #=============#";
      raiseWidget = _statusDialog;
      _statusDialog->setFolderList( _folderMan->map() );
    }
  }
  raiseDialog( raiseWidget );
}

void Application::raiseDialog( QWidget *raiseWidget )
{
  // viel hilft viel ;-)
  if( raiseWidget ) {
#if defined(Q_WS_WIN) || defined (Q_OS_MAC)
    Qt::WindowFlags eFlags = raiseWidget->windowFlags();
    eFlags |= Qt::WindowStaysOnTopHint;
    raiseWidget->setWindowFlags(eFlags);
    raiseWidget->show();
    eFlags &= ~Qt::WindowStaysOnTopHint;
    raiseWidget->setWindowFlags(eFlags);
#endif
    raiseWidget->show();
    raiseWidget->raise();
    raiseWidget->activateWindow();
  }
}

void Application::slotOpenLogBrowser()
{
    _logBrowser->show();
    _logBrowser->raise();
}

void Application::slotAbout()
{
    QString devString;
#ifdef GIT_SHA1
    const QString githubPrefix(QLatin1String(
                               "https://github.com/owncloud/mirall/commit/"));
    const QString gitSha1(QLatin1String(GIT_SHA1));
    devString = tr("<p><small>Built from Git revision <a href=\"%1\">%2</a>"
                   " on %3, %4<br>using OCsync %5 and Qt %6.</small><p>")
                       .arg(githubPrefix+gitSha1).arg(gitSha1.left(6))
                       .arg(__DATE__).arg(__TIME__)
                       .arg(MIRALL_STRINGIFY(LIBCSYNC_VERSION))
                       .arg(QT_VERSION_STR);
#endif
    QMessageBox::about(0, tr("About %1").arg(_theme->appName()),
                       Theme::instance()->about());
}

/*
  * the folder is to be removed. The slot is called from a signal emitted by
  * the status dialog, which removes the folder from its list by itself.
  */
void Application::slotRemoveFolder( const QString& alias )
{
    int ret = QMessageBox::question( 0, tr("Confirm Folder Remove"),
                                     tr("Do you really want to remove upload folder <i>%1</i>?").arg(alias),
                                     QMessageBox::Yes|QMessageBox::No );

    if( ret == QMessageBox::No ) {
        return;
    }
    Folder *f = _folderMan->folder(alias);
    if( f && _overallStatusStrings.contains( f->alias() )) {
        _overallStatusStrings.remove( f->alias() );
    }

    _folderMan->slotRemoveFolder( alias );
    _statusDialog->slotRemoveSelectedFolder( );
    computeOverallSyncStatus();
    setupContextMenu();
}

// Open the File list info dialog.
void Application::slotInfoFolder( const QString& alias )
{
    qDebug() << "details of folder with alias " << alias;

    if( !_fileItemDialog ) {
        _fileItemDialog = new FileItemDialog(_theme);
    }

    SyncResult folderResult = _folderMan->syncResult( alias );

    _fileItemDialog->setSyncResult( folderResult );
    raiseDialog( _fileItemDialog );
}

void Application::slotEnableFolder(const QString& alias, const bool enable)
{
    qDebug() << "Application: enable folder with alias " << alias;
    bool terminate = false;

    // this sets the folder status to disabled but does not interrupt it.
    Folder *f = _folderMan->folder( alias );
    if( f && !enable ) {
        // check if a sync is still running and if so, ask if we should terminate.
        if( f->isBusy() ) { // its still running
            int reply = QMessageBox::question( 0, tr("Sync Running"),
                                               tr("The syncing operation is running.<br/>Do you want to terminate it?"),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
            if ( reply == QMessageBox::Yes )
                terminate = true;
            else
                return; // do nothing
        }
    }

    // message box can return at any time while the thread keeps running,
    // so better check again after the user has responded.
    if ( f->isBusy() && terminate )
        _folderMan->terminateSyncProcess( alias );

    _folderMan->slotEnableFolder( alias, enable );
    _statusDialog->slotUpdateFolderState( f );
}

void Application::slotConfigure()
{
  _folderMan->disableFoldersWithRestore();
  _owncloudSetupWizard->startWizard(false);
  _folderMan->restoreEnabledFolders();
}

void Application::slotConfigureProxy()
{
    ProxyDialog dlg;
    if (dlg.exec() == QDialog::Accepted)
    {
        setupProxy();
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
    _tray->showMessage(title, msg);
}

void Application::slotSyncStateChange( const QString& alias )
{
    SyncResult result = _folderMan->syncResult( alias );

    // do not promote LocalSyncState to the status dialog.
    if( !result.localRunOnly() ) {
        _statusDialog->slotUpdateFolderState( _folderMan->folder(alias) );

        if( _fileItemDialog && _fileItemDialog->isVisible() ) {
            _fileItemDialog->setSyncResult( _folderMan->syncResult(alias) );
        }
    }
    computeOverallSyncStatus();

    qDebug() << "Sync state changed for folder " << alias << ": "  << result.statusString();
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
        } else if (option == QLatin1String("--logflush")) {
            _logFlush = true;
        } else if (option == QLatin1String("--monoicons")) {
            _theme->setSystrayUseMonoIcons(true); 
	} else {
	    setHelp();
	    std::cout << "Option not recognized:  " << option.toStdString() << std::endl;
	    break;
	}
    }
}

void Application::computeOverallSyncStatus()
{

    // display the info of the least successful sync (eg. not just display the result of the latest sync
    SyncResult overallResult(SyncResult::Undefined );
    QString trayMessage;
    Folder::Map map = _folderMan->map();

    foreach ( Folder *syncedFolder, map.values() ) {
        QString folderMessage = _overallStatusStrings[syncedFolder->alias()];

        SyncResult folderResult = syncedFolder->syncResult();
        SyncResult::Status syncStatus = folderResult.status();

        if( ! folderResult.localRunOnly() ) { // skip local runs, use the last message.
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
                    folderMessage = tr( "Last Sync was successful." );
                    overallResult.setStatus( SyncResult::Success );
                }
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
        }

        qDebug() << "Folder in overallStatus Message: " << syncedFolder << " with name " << syncedFolder->alias();
        QString msg = QString::fromLatin1("Folder %1: %2").arg(syncedFolder->alias()).arg(folderMessage);
        if( msg != _overallStatusStrings[syncedFolder->alias()] ) {
            _overallStatusStrings[syncedFolder->alias()] = msg;
        }
    }

    // create the tray blob message, check if we have an defined state
    if( overallResult.status() != SyncResult::Undefined ) {
        QStringList allStatusStrings = _overallStatusStrings.values();
        if( ! allStatusStrings.isEmpty() )
            trayMessage = allStatusStrings.join(QLatin1String("\n"));
        else
            trayMessage = tr("No sync folders configured.");

        QIcon statusIcon = _theme->syncStateIcon( overallResult.status(), true); // size 48 before

        _tray->setIcon( statusIcon );
        _tray->setToolTip(trayMessage);
    }
}

void Application::showHelp()
{
setHelp();
    std::cout << _theme->appName().toLatin1().constData() << " version " <<
                 _theme->version().toLatin1().constData() << std::endl << std::endl;
    std::cout << "File synchronisation desktop utility." << std::endl << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h --help            : show this help screen." << std::endl;
    std::cout << "  --logwindow          : open a window to show log output." << std::endl;
    std::cout << "  --logfile <filename> : write log output to file <filename>." << std::endl;
    std::cout << "  --logflush           : flush the log file after every write." << std::endl;
    std::cout << "  --monoicons          : Use black/white pictograms for systray." << std::endl;
    std::cout << std::endl;
    if (_theme->appName() == QLatin1String("ownCloud"))
        std::cout << "For more information, see http://www.owncloud.org" << std::endl;
}

void Application::setHelp()
{
    _helpOnly = true;
}

bool Application::giveHelp()
{
    return _helpOnly;
}
} // namespace Mirall

