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

#include <QtCore>
#include <QtGui>
#include <QHash>
#include <QHashIterator>
#include <QUrl>
#include <QDesktopServices>
#include <QTranslator>

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
#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/updatedetector.h"

#include "mirall/miralltheme.h"
#include "mirall/owncloudtheme.h"

#ifdef WITH_CSYNC
#include "mirall/csyncfolder.h"
#endif
#include "mirall/inotify.h"

namespace Mirall {

Application::Application(int &argc, char **argv) :
    QApplication(argc, argv),
    _tray(0),
#if QT_VERSION >= 0x040700
    _networkMgr(new QNetworkConfigurationManager(this)),
#endif
    _contextMenu(0),
    _ocInfo(0),
    _updateDetector(0)
{

#ifdef OWNCLOUD_CLIENT
    _theme = new ownCloudTheme();
#else
    _theme = new mirallTheme();
#endif
    setApplicationName( _theme->appName() );
    setWindowIcon( _theme->applicationIcon() );

    processEvents();

    // Internationalization support.
    qDebug() << ""; // relaxing debug output in qtCreator
    qDebug() << QString( "################## %1 %2 %3 ").arg(_theme->appName())
                .arg( QLocale::system().name() )
                .arg(_theme->version());

    QTranslator *qtTranslator = new QTranslator;
    qtTranslator->load("qt_" + QLocale::system().name(),
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    installTranslator(qtTranslator);

    QTranslator *mirallTranslator = new QTranslator;
#ifdef Q_OS_LINUX
    // FIXME - proper path!
    mirallTranslator->load("mirall_" + QLocale::system().name(), QLatin1String("/usr/share/mirall/i18n/"));
#else
    mirallTranslator->load("mirall_" + QLocale::system().name()); // path defaults to app dir.
#endif
    installTranslator(mirallTranslator);

    // create folder manager for sync folder management
    _folderMan = new FolderMan();
    connect( _folderMan, SIGNAL(folderSyncStateChange(QString)),
             this,SLOT(slotSyncStateChange(QString)));

    /* use a signal mapper to map the open requests to the alias names */
    _folderOpenActionMapper = new QSignalMapper(this);
    connect(_folderOpenActionMapper, SIGNAL(mapped(const QString &)),
            this, SLOT(slotFolderOpenAction(const QString &)));

    setQuitOnLastWindowClosed(false);

    _folderWizard = new FolderWizard( 0, _theme );

    _ocInfo = new ownCloudInfo( QString(), this );
    connect( _ocInfo,SIGNAL(ownCloudInfoFound(QString,QString)),
             SLOT(slotOwnCloudFound(QString,QString)));

    connect( _ocInfo,SIGNAL(noOwncloudFound(QNetworkReply*)),
             SLOT(slotNoOwnCloudFound(QNetworkReply*)));

    connect( _ocInfo,SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheck(QString,QNetworkReply*)));

    _owncloudSetupWizard = new OwncloudSetupWizard( _folderMan, _theme );
    connect( _owncloudSetupWizard, SIGNAL(ownCloudWizardDone(int)), SLOT(slotStartFolderSetup()));

    _statusDialog = new StatusDialog( _theme );
    connect( _statusDialog, SIGNAL(addASync()), this, SLOT(slotAddFolder()) );

    connect( _statusDialog, SIGNAL(removeFolderAlias( const QString&)),
             SLOT(slotRemoveFolder(const QString&)));
#if 0
    connect( _statusDialog, SIGNAL(fetchFolderAlias(const QString&)),
             SLOT(slotFetchFolder( const QString&)));
    connect( _statusDialog, SIGNAL(pushFolderAlias(const QString&)),
             SLOT(slotPushFolder( const QString&)));
#endif
    connect( _statusDialog, SIGNAL(enableFolderAlias(QString,bool)),
             SLOT(slotEnableFolder(QString,bool)));
    connect( _statusDialog, SIGNAL(infoFolderAlias(const QString&)),
             SLOT(slotInfoFolder( const QString&)));
    connect( _statusDialog, SIGNAL(openFolderAlias(const QString&)),
             SLOT(slotFolderOpenAction(QString)));

#if QT_VERSION >= 0x040700
    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (QNetworkConfiguration netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }
#endif

    setupActions();
    setupSystemTray();
    processEvents();

    QTimer::singleShot( 0, this, SLOT( slotStartFolderSetup() ));

    MirallConfigFile cfg;
    if( !cfg.ownCloudSkipUpdateCheck() ) {
        QTimer::singleShot( 3000, this, SLOT( slotStartUpdateDetector() ));
    }

    qDebug() << "Network Location: " << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    qDebug() << "* Mirall shutdown";

#if QT_VERSION >= 0x040700
    delete _networkMgr;
#endif
    delete _folderMan;
    delete _ocInfo;
}

void Application::slotStartUpdateDetector()
{
    _updateDetector = new UpdateDetector(this);
    _updateDetector->versionCheck(_theme);

}

void Application::slotStartFolderSetup()
{
    if( _ocInfo->isConfigured() ) {
      _ocInfo->checkInstallation();
    } else {
        QMessageBox::warning(0, tr("No ownCloud Configuration"),
                             tr("<p>No ownCloud connection was configured yet.</p><p>Please configure one by clicking on the tray icon!</p>"));
        // It was evaluated to open the config dialog from here directly but decided
        // against because the user does not know why. The popup gives a better user
        // guidance, even if its a click more.
    }
}

void Application::slotOwnCloudFound( const QString& url , const QString& version )
{
    qDebug() << "** Application: ownCloud found: " << url << " with version " << version;
    // now check the authentication!
    QTimer::singleShot( 0, this, SLOT( slotCheckAuthentication() ));
}

void Application::slotNoOwnCloudFound( QNetworkReply* reply )
{
    qDebug() << "** Application: NO ownCloud found!";
    QString msg;
    if( reply ) {
        QString url( reply->url().toString() );
        url.remove( "/status.php" );
        msg = tr("<p>The ownCloud at %1 could not be reached.</p>").arg( url );
        msg += tr("<p>The detailed error message is<br/><tt>%1</tt></p>").arg( reply->errorString() );
    }
    msg += tr("<p>Please check your configuration by clicking on the tray icon.</p>");

    QMessageBox::warning(0, tr("ownCloud Connection Failed"), msg );
    _actionAddFolder->setEnabled( false );
    setupContextMenu();
}

void Application::slotCheckAuthentication()
{
    qDebug() << "# checking for authentication settings.";
    _ocInfo->getRequest("/", true ); // this call needs to be authenticated.
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
        }
        _actionAddFolder->setEnabled( true );
    }
    setupContextMenu();
}

void Application::setupActions()
{
    _actionOpenStatus = new QAction(tr("Open status..."), this);
    QObject::connect(_actionOpenStatus, SIGNAL(triggered(bool)), SLOT(slotOpenStatus()));
    _actionAddFolder = new QAction(tr("Add folder..."), this);
    QObject::connect(_actionAddFolder, SIGNAL(triggered(bool)), SLOT(slotAddFolder()));
    _actionConfigure = new QAction(tr("Configure..."), this);
    QObject::connect(_actionConfigure, SIGNAL(triggered(bool)), SLOT(slotConfigure()));


    _actionQuit = new QAction(tr("Quit"), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), SLOT(quit()));
}

void Application::setupSystemTray()
{
    _tray = new QSystemTrayIcon();
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
    _contextMenu->addAction(_actionConfigure);
    _contextMenu->addAction(_actionAddFolder);
    _contextMenu->addSeparator();

    // here all folders should be added
    foreach (Folder *folder, _folderMan->map() ) {
        QAction *action = new QAction( tr("open %1").arg( folder->alias()), this );
        action->setIcon( _theme->trayFolderIcon( folder->backend()) );

        connect( action, SIGNAL(triggered()),_folderOpenActionMapper,SLOT(map()));
        _folderOpenActionMapper->setMapping( action, folder->alias() );

        _contextMenu->addAction(action);
    }

    _contextMenu->addSeparator();

    _contextMenu->addAction(_actionQuit);
    _tray->setContextMenu(_contextMenu);
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
        url.setScheme( "file" );
        QDesktopServices::openUrl(url);
    }
}

void Application::slotTrayClicked( QSystemTrayIcon::ActivationReason reason )
{
  if( reason == QSystemTrayIcon::Trigger ) {
    // check if there is a mirall.cfg already.
    if( _owncloudSetupWizard->wizard()->isVisible() ) {
      _owncloudSetupWizard->wizard()->show();
    }

    // if no config file is there, start the configuration wizard.
    MirallConfigFile cfgFile;

    if( !cfgFile.exists() ) {
      qDebug() << "No configured folders yet, start the Owncloud integration dialog.";
      _owncloudSetupWizard->startWizard();
    } else {
        qDebug() << "#============# Status dialog starting #=============#";
#if defined Q_WS_WIN || defined Q_WS_X11
      _statusDialog->setFolderList( _folderMan->map() );
      _statusDialog->show();
#endif
    }
  }
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

    QString alias        = _folderWizard->field("alias").toString();
    QString sourceFolder = _folderWizard->field("sourceFolder").toString();
    QString backend      = QString::fromLocal8Bit("csync");
    QString targetPath;
    bool onlyThisLAN = false;
    bool onlyOnline  = false;

    if (_folderWizard->field("local?").toBool()) {
        // setup a local csync folder
        targetPath = _folderWizard->field("targetLocalFolder").toString();
    } else if (_folderWizard->field("remote?").toBool()) {
        // setup a remote csync folder
        targetPath  = _folderWizard->field("targetURLFolder").toString();
        onlyOnline  = _folderWizard->field("onlyOnline?").toBool();
        onlyThisLAN = _folderWizard->field("onlyThisLAN?").toBool();
    } else if( _folderWizard->field("OC?").toBool()) {
        // setup a ownCloud folder
        backend    = QString::fromLocal8Bit("owncloud");
        targetPath = _folderWizard->field("targetOCFolder").toString();
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
  _statusDialog->setFolderList( _folderMan->map() );
  _statusDialog->show();
}

/*
  * the folder is to be removed. The slot is called from a signal emitted by
  * the status dialog, which removes the folder from its list by itself.
  */
void Application::slotRemoveFolder( const QString& alias )
{
    int ret = QMessageBox::question( 0, tr("Confirm Folder Remove"), tr("Do you really want to remove upload folder <i>%1</i>?").arg(alias),
                                     QMessageBox::Yes|QMessageBox::No );

    if( ret == QMessageBox::No ) {
        return;
    }

    _folderMan->slotRemoveFolder( alias );
    _statusDialog->slotRemoveSelectedFolder( );
    setupContextMenu();
}

#ifdef HAVE_FETCH_AND_PUSH
void Application::slotFetchFolder( const QString& alias )
{
  qDebug() << "start to fetch folder with alias " << alias;

  if( ! _folderMap.contains( alias ) ) {
    qDebug() << "!! Can not fetch alias " << alias << ", can not be found in folderMap.";
    return;
  }

  Folder *f = _folderMap[alias];

}

void Application::slotPushFolder( const QString& alias )
{
  qDebug() << "start to push folder with alias " << alias;

  if( ! _folderMap.contains( alias ) ) {
    qDebug() << "!! Can not push alias " << alias << ", can not be found in folderMap.";
    return;
  }

  Folder *f = _folderMap[alias];

}
#endif

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
    foreach( const QString l, li ) {
        folderMessage += QString("<p>%1</p>").arg( l );
    }

    infoBox.setText( folderMessage );

    //    qDebug() << "informative text: " << infoBox.informativeText();

    if ( !folderResult.syncChanges().isEmpty() ) {
	QString details;
	QHash < QString, QStringList > changes = folderResult.syncChanges();
	QHash< QString, QStringList >::const_iterator change_it = changes.constBegin();
	for(; change_it != changes.constEnd(); ++change_it ) {
	    QString changeType = tr( "Unknown" );
	    if ( change_it.key() == "changed" ) {
		changeType = tr( "Changed files:\n" );
	    } else if ( change_it.key() == "added" ) {
	        changeType = tr( "Added files:\n" );
	    } else if ( change_it.key() == "deleted" ) {
		changeType = tr( "New files in the server, or files deleted locally:\n");
	    }

	    QStringList files = change_it.value();
	    QString fileList;
	    foreach( QString file, files) {
		fileList += file + "\n";
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

void Application::slotSyncStateChange( const QString& alias )
{
    SyncResult result = _folderMan->syncResult( alias );

    // do not promote LocalSyncState to the status dialog.
    if( !result.localRunOnly() ) {
        _statusDialog->slotUpdateFolderState( _folderMan->folder(alias) );
    }
    computeOverallSyncStatus();

    qDebug() << "Sync state changed for folder " << alias << ": "  << result.localRunOnly();
}

void Application::computeOverallSyncStatus()
{

    // display the info of the least succesful sync (eg. not just display the result of the latest sync
    SyncResult overallResult = SyncResult::Success;
    QString trayMessage;
    Folder::Map map = _folderMan->map();

    foreach ( Folder *syncedFolder, map ) {
        QString folderMessage = _overallStatusStrings[syncedFolder];

        SyncResult folderResult = syncedFolder->syncResult();
        SyncResult::Status syncStatus = folderResult.status();

        if( ! folderResult.localRunOnly() ) { // skip local runs, use the last message.
            if( syncedFolder->syncEnabled() ) {
                switch( syncStatus ) {
                case SyncResult::Undefined:
                    if ( overallResult.status() != SyncResult::Error ) {
                        overallResult = SyncResult::Error;
                    }
                    folderMessage = tr( "Undefined State." );
                    break;
                case SyncResult::NotYetStarted:
                    folderMessage = tr( "Waits to start syncing." );
                    break;
                case SyncResult::SyncRunning:
                    folderMessage = tr( "Sync is running." );
                    break;
                case SyncResult::Success:
                    folderMessage = tr( "Last Sync was successful." );
                    break;
                case SyncResult::Error:
                    overallResult = SyncResult::Error;
                    folderMessage = tr( "Syncing Error." );
                    break;
                case SyncResult::SetupError:
                    if ( overallResult.status() != SyncResult::Error ) {
                        overallResult = SyncResult::SetupError;
                    }
                    folderMessage = tr( "Setup Error." );
                    break;
                default:
                    folderMessage = tr( "Undefined Error State." );
                }
            } else {
                    // sync is disabled.
                folderMessage = tr( "Sync is disabled." );
            }
        }
        if( folderMessage != _overallStatusStrings[syncedFolder] ) {
            _overallStatusStrings[syncedFolder] = QString("Folder %1: %2").arg(syncedFolder->alias()).arg(folderMessage);
        }
    }

    // create the tray blob message
    QStringList allStatusStrings = _overallStatusStrings.values();
    trayMessage = allStatusStrings.join("\n");

    QIcon statusIcon = _theme->syncStateIcon( overallResult.status(), 48 );

    if( overallResult.status() == SyncResult::Success ) {
        // Rather display the mirall icon instead of the ok icon.
        statusIcon = _theme->applicationIcon();
    }

    _tray->setIcon( statusIcon );
    _tray->setToolTip(trayMessage);
}

} // namespace Mirall

