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

#define LOG_TO_CALLBACK // FIXME: This should be in csync.
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

#include "mirall/miralltheme.h"
#include "mirall/owncloudtheme.h"

#include "config.h"

#ifdef WITH_CSYNC
#include "mirall/csyncfolder.h"
#endif
#include "mirall/inotify.h"

#include <csync.h>

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

void csyncLogCatcher(const char *msg)
{
  Logger::instance()->csyncLog( QString::fromUtf8(msg) );
}

// ----------------------------------------------------------------------------------

Application::Application(int &argc, char **argv) :
    SharedTools::QtSingleApplication(argc, argv),
    _tray(0),
    _sslErrorDialog(0),
#if QT_VERSION >= 0x040700
    _networkMgr(new QNetworkConfigurationManager(this)),
#endif
    _contextMenu(0),
    _updateDetector(0),
    _helpOnly(false)
{

#ifdef OWNCLOUD_CLIENT
    _theme = new ownCloudTheme();
#else
    _theme = new mirallTheme();
#endif
    setApplicationName( _theme->appName() );
    setWindowIcon( _theme->applicationIcon() );

    if( arguments().contains(QLatin1String("--help"))) {
        showHelp();
    }
    setupLogBrowser();
    processEvents();

    QTranslator *qtTranslator = new QTranslator(this);
    qtTranslator->load(QLatin1String("qt_") + QLocale::system().name(),
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    installTranslator(qtTranslator);

    QTranslator *mirallTranslator = new QTranslator(this);
#ifdef Q_OS_LINUX
    // FIXME - proper path!
    mirallTranslator->load(QLatin1String("mirall_") + QLocale::system().name(), QLatin1String("/usr/share/mirall/i18n/"));
#endif
#ifdef Q_OS_MAC
    mirallTranslator->load(QLatin1String("mirall_") + QLocale::system().name(), applicationDirPath()+QLatin1String("/../translations") ); // path defaults to app dir.
#endif
#ifdef Q_OS_WIN32
    mirallTranslator->load(QLatin1String("mirall_") + QLocale::system().name(), applicationDirPath());
#endif

    installTranslator(mirallTranslator);

    // create folder manager for sync folder management
    _folderMan = new FolderMan(this);
    connect( _folderMan, SIGNAL(folderSyncStateChange(QString)),
             this,SLOT(slotSyncStateChange(QString)));

    /* use a signal mapper to map the open requests to the alias names */
    _folderOpenActionMapper = new QSignalMapper(this);
    connect(_folderOpenActionMapper, SIGNAL(mapped(const QString &)),
            this, SLOT(slotFolderOpenAction(const QString &)));

    setQuitOnLastWindowClosed(false);

    _folderWizard = new FolderWizard( 0, _theme );

    _owncloudSetupWizard = new OwncloudSetupWizard( _folderMan, _theme, this );
    connect( _owncloudSetupWizard, SIGNAL(ownCloudWizardDone(int)), SLOT(slotStartFolderSetup(int)));

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

#if QT_VERSION >= 0x040700
    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (const QNetworkConfiguration& netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }
#endif

    setupActions();
    setupSystemTray();
    setupProxy();
    processEvents();

    QObject::connect( this, SIGNAL(messageReceived(QString)),
                         this, SLOT(slotOpenStatus()) );

    QTimer::singleShot( 0, this, SLOT( slotStartFolderSetup() ));

    MirallConfigFile cfg;
    if( !cfg.ownCloudSkipUpdateCheck() ) {
        QTimer::singleShot( 3000, this, SLOT( slotStartUpdateDetector() ));
    }

    qDebug() << "Network Location: " << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    delete _sslErrorDialog;
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

            connect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
                     this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

            connect( ownCloudInfo::instance(), SIGNAL(sslFailed(QNetworkReply*, QList<QSslError>)),
                     this,SLOT(slotSSLFailed(QNetworkReply*, QList<QSslError>)));


            ownCloudInfo::instance()->checkInstallation();
        } else {
            QMessageBox::warning(0, tr("No ownCloud Configuration"),
                                 tr("<p>No server connection has been configured for this ownCloud client.</p>"
                                    "<p>Please right click on the ownCloud system tray icon and select <i>Configure</i> "
                                    "to connect this client to an ownCloud server.</p>"));
            // It was evaluated to open the config dialog from here directly but decided
            // against because the user does not know why. The popup gives a better user
            // guidance, even if its a click more.
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
        QMessageBox::warning(0, tr("ownCloud Server Mismatch"),
                             tr("<p>The configured server for this client is too old.</p>"
                                "<p>Please update to the latest ownCloud server and restart the client.</p>"));
        return;
    }

    QTimer::singleShot( 0, this, SLOT( slotCheckAuthentication() ));
}

void Application::slotNoOwnCloudFound( QNetworkReply* reply )
{
    qDebug() << "** Application: NO ownCloud found!";
    QString msg;
    if( reply ) {
        QString url( reply->url().toString() );
        url.remove( QLatin1String("/status.php") );
        msg = tr("<p>The ownCloud at %1 could not be reached.</p>").arg( url );
        msg += tr("<p>The detailed error message is<br/><tt>%1</tt></p>").arg( reply->errorString() );
    }
    msg += tr("<p>Please check your configuration by clicking on the tray icon.</p>");

    QMessageBox::warning(0, tr("ownCloud Connection Failed"), msg );
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

void Application::slotCheckAuthentication()
{
    qDebug() << "# checking for authentication settings.";
    ownCloudInfo::instance()->getRequest(QLatin1String("/"), true ); // this call needs to be authenticated.
    // simply GET the webdav root, will fail if credentials are wrong.
    // continue in slotAuthCheck here :-)
}

void Application::slotAuthCheck( const QString& ,QNetworkReply *reply )
{
    if( reply->error() == QNetworkReply::AuthenticationRequiredError ) { // returned if the user is wrong.
        qDebug() << "******** Password is wrong!";
        QMessageBox::warning(0, tr("No ownCloud Connection"),
                             tr("<p>Your ownCloud credentials are not correct.</p>"
                                "<p>Please correct them by starting the configuration dialog from the tray!</p>"));
        _actionAddFolder->setEnabled( false );
    } else if( reply->error() == QNetworkReply::OperationCanceledError ) {
        // the username was wrong and ownCloudInfo was closing the request after a couple of auth tries.
        qDebug() << "******** Username or password is wrong!";
        QMessageBox::warning(0, tr("No ownCloud Connection"),
                             tr("<p>Your ownCloud user name or password is not correct.</p>"
                                "<p>Please correct it by starting the configuration dialog from the tray!</p>"));
        _actionAddFolder->setEnabled( false );
    } else {
        qDebug() << "######## Credentials are ok!";
        int cnt = _folderMan->setupFolders();
        if( cnt ) {
            _tray->setIcon(_theme->applicationIcon());
            _tray->show();
            processEvents();

            if( _tray )
                _tray->showMessage(tr("ownCloud Sync Started"), tr("Sync started for %1 configured sync folder(s).").arg(cnt));

            _statusDialog->setFolderList( _folderMan->map() );
        }
        _actionAddFolder->setEnabled( true );
    }

    // disconnect from ownCloud Info signals
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheck(QString,QNetworkReply*)));
    setupContextMenu();
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

void Application::setupActions()
{
    _actionOpenoC = new QAction(tr("Open ownCloud..."), this);
    QObject::connect(_actionOpenoC, SIGNAL(triggered(bool)), SLOT(slotOpenOwnCloud()));
    _actionOpenStatus = new QAction(tr("Open status..."), this);
    QObject::connect(_actionOpenStatus, SIGNAL(triggered(bool)), SLOT(slotOpenStatus()));
    _actionAddFolder = new QAction(tr("Add folder..."), this);
    QObject::connect(_actionAddFolder, SIGNAL(triggered(bool)), SLOT(slotAddFolder()));
    _actionConfigure = new QAction(tr("Configure..."), this);
    QObject::connect(_actionConfigure, SIGNAL(triggered(bool)), SLOT(slotConfigure()));
    _actionConfigureProxy = new QAction(tr("Configure proxy..."), this);
    QObject::connect(_actionConfigureProxy, SIGNAL(triggered(bool)), SLOT(slotConfigureProxy()));
    _actionQuit = new QAction(tr("Quit"), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), SLOT(quit()));
}

void Application::setupSystemTray()
{
    _tray = new QSystemTrayIcon(this);
    _tray->setIcon( _theme->applicationIcon() ); // load the grey icon

    connect(_tray,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            SLOT(slotTrayClicked(QSystemTrayIcon::ActivationReason)));

    setupContextMenu();

    _tray->show();
}

void Application::setupContextMenu()
{
    if( _contextMenu ) {
        _contextMenu->clear();
    } else {
        _contextMenu = new QMenu();
    }
    _contextMenu->setTitle(_theme->appName() );
    _contextMenu->addAction(_actionOpenStatus);
    _contextMenu->addAction(_actionOpenoC);

    _contextMenu->addSeparator();

    if (!_folderMan->map().isEmpty())
        _contextMenu->addAction(tr("Managed Folders:"))->setDisabled(true);

    // here all folders should be added
    foreach (Folder *folder, _folderMan->map() ) {
        QAction *action = new QAction( folder->alias(), this );
        action->setIcon( _theme->trayFolderIcon( folder->backend()) );

        connect( action, SIGNAL(triggered()),_folderOpenActionMapper,SLOT(map()));
        _folderOpenActionMapper->setMapping( action, folder->alias() );

        _contextMenu->addAction(action);
    }
    _contextMenu->addAction(_actionAddFolder);

    _contextMenu->addSeparator();
    _contextMenu->addAction(_actionConfigure);
    _contextMenu->addAction(_actionConfigureProxy);
    _contextMenu->addSeparator();

    _contextMenu->addAction(_actionQuit);
    _tray->setContextMenu(_contextMenu);
}

void Application::setupLogBrowser()
{
    // init the log browser.
    _logBrowser = new LogBrowser;
    qInstallMsgHandler( mirallLogCatcher );
    csync_set_log_callback( csyncLogCatcher );

    if( arguments().contains(QLatin1String("--logwindow"))
            || arguments().contains(QLatin1String("-l"))) {
        slotOpenLogBrowser();
    }

    // check for command line option for a log file.
    int lf = arguments().indexOf(QLatin1String("--logfile"));

    if( lf > -1 && lf+1 < arguments().count() ) {
        QString logfile = arguments().at( lf+1 );

        bool flush = false;
        if( arguments().contains(QLatin1String("--logflush"))) flush = true;

        qDebug() << "Logging into logfile: " << logfile << " with flush " << flush;
        _logBrowser->setLogFile( logfile, flush );
    }

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
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::NoProxy);
        QNetworkProxy::setApplicationProxy(proxy);
        break;
    }
    case QNetworkProxy::DefaultProxy: {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        break;
    }
    case QNetworkProxy::Socks5Proxy: {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::Socks5Proxy);
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
#if defined Q_WS_WIN || defined Q_WS_X11
  if( reason == QSystemTrayIcon::Trigger ) {
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
    } else if( _folderWizard->field(QLatin1String("OC?")).toBool()) {
        // setup a ownCloud folder
        backend    = QLatin1String("owncloud");
        targetPath = _folderWizard->field(QLatin1String("targetOCFolder")).toString();
    } else {
      qWarning() << "* Folder not local and note remote?";
      goodData = false;
    }

    if( goodData ) {
        _folderMan->addFolderDefinition( backend, alias, sourceFolder, targetPath, onlyThisLAN );
        Folder *f = _folderMan->setupFolderFromConfigFile( alias );
        if( f ) {
            _statusDialog->slotAddFolder( f );
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
      _owncloudSetupWizard->startWizard();
    } else {
      qDebug() << "#============# Status dialog starting #=============#";
      raiseWidget = _statusDialog;
      _statusDialog->setFolderList( _folderMan->map() );
    }
  }

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

void Application::slotInfoFolder( const QString& alias )
{
    qDebug() << "details of folder with alias " << alias;

    SyncResult folderResult = _folderMan->syncResult( alias );

    bool enabled = true;
    Folder *f = _folderMan->folder( alias );
    if( f && ! f->syncEnabled() ) {
        enabled = false;
    }

    QString folderMessage;

    SyncResult::Status syncStatus = folderResult.status();
    switch( syncStatus ) {
    case SyncResult::Undefined:
        folderMessage = tr( "Undefined Folder State" );
        break;
    case SyncResult::NotYetStarted:
        folderMessage = tr( "The folder waits to start syncing." );
        break;
    case SyncResult::SyncRunning:
        folderMessage = tr("Sync is running.");
        break;
    case SyncResult::Success:
        folderMessage = tr("Last Sync was successful.");
        break;
    case SyncResult::Error:
        folderMessage = tr( "Syncing Error." );
        break;
    case SyncResult::SetupError:
        folderMessage = tr( "Setup Error." );
        break;
    default:
        folderMessage = tr( "Undefined Error State." );
    }
    folderMessage = QLatin1String("<b>") + folderMessage + QLatin1String("</b><br/>");

    QMessageBox infoBox( QMessageBox::Information, tr( "Folder information" ), alias, QMessageBox::Ok );
    QStringList li = folderResult.errorStrings();
    foreach( const QString& l, li ) {
        folderMessage += QString::fromLatin1("<p>%1</p>").arg( l );
    }

    infoBox.setText( folderMessage );

    //    qDebug() << "informative text: " << infoBox.informativeText();

    if ( !folderResult.syncChanges().isEmpty() ) {
        QString details;
        QHash < QString, QStringList > changes = folderResult.syncChanges();
        QHash< QString, QStringList >::const_iterator change_it = changes.constBegin();
        for(; change_it != changes.constEnd(); ++change_it ) {
            QString changeType = tr( "Unknown" );
            if ( change_it.key() == QLatin1String("changed") ) {
            changeType = tr( "Changed files:\n" );
            } else if ( change_it.key() == QLatin1String("added") ) {
                changeType = tr( "Added files:\n" );
            } else if ( change_it.key() == QLatin1String("deleted") ) {
            changeType = tr( "New files in the server, or files deleted locally:\n");
            }

            QStringList files = change_it.value();
            QString fileList;
                foreach( const QString& file, files) {
                    fileList += file + QLatin1Char('\n');
            }
            details += changeType + fileList;
        }
        infoBox.setDetailedText(details);
        qDebug() << "detailed text: " << infoBox.detailedText();
    }
    infoBox.exec();
}

void Application::slotEnableFolder(const QString& alias, const bool enable)
{
  qDebug() << "Application: enable folder with alias " << alias;

  _folderMan->slotEnableFolder( alias, enable );

  // this sets the folder status to disabled but does not interrupt it.
  Folder *f = _folderMan->folder( alias );
  if( f && !enable ) {
      // check if a sync is still running and if so, ask if we should terminate.
      if( f->isBusy() ) { // its still running
          QMessageBox::StandardButton b = QMessageBox::question( 0, tr("Sync Running"),
                                                                 tr("The syncing operation is running.<br/>Do you want to terminate it?"),
                                                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
          if( b == QMessageBox::Yes ) {
              _folderMan->terminateSyncProcess( alias );
          }
      }
  }

  _statusDialog->slotUpdateFolderState( f );
}

void Application::slotConfigure()
{
  _folderMan->disableFoldersWithRestore();
  _owncloudSetupWizard->startWizard();
  _folderMan->restoreEnabledFolders();
}

void Application::slotConfigureProxy()
{
    ProxyDialog* dlg = new ProxyDialog();
    if (dlg->exec() == QDialog::Accepted)
    {
        setupProxy();
    }
    dlg->deleteLater();
}

void Application::slotSyncStateChange( const QString& alias )
{
    SyncResult result = _folderMan->syncResult( alias );

    // do not promote LocalSyncState to the status dialog.
    if( !result.localRunOnly() ) {
        _statusDialog->slotUpdateFolderState( _folderMan->folder(alias) );
    }
    computeOverallSyncStatus();

    qDebug() << "Sync state changed for folder " << alias << ": "  << result.statusString();
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
            if( syncedFolder->syncEnabled() ) {
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
            } else {
                // sync is disabled.
                folderMessage = tr( "Sync is paused." );
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

        QIcon statusIcon = _theme->syncStateIcon( overallResult.status()); // size 48 before

        _tray->setIcon( statusIcon );
        _tray->setToolTip(trayMessage);
    }
}

void Application::showHelp()
{
    std::cout << _theme->appName().toLatin1().constData() << " version " <<
                 _theme->version().toLatin1().constData() << std::endl << std::endl;
    std::cout << "File synchronisation desktop utility." << std::endl << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --logwindow          : open a window to show log output." << std::endl;
    std::cout << "  --logfile <filename> : write log output to file <filename>." << std::endl;
    std::cout << "  --flushlog           : flush the log file after every write." << std::endl;
    std::cout << std::endl;
    std::cout << "For more information, see http://www.owncloud.org" << std::endl;
    _helpOnly = true;
}

bool Application::giveHelp()
{
    return _helpOnly;
}
} // namespace Mirall

