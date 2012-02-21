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

#include "mirall/constants.h"
#include "mirall/application.h"
#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/folderwizard.h"
#include "mirall/networklocation.h"
#include "mirall/unisonfolder.h"
#include "mirall/sitecopyfolder.h"
#include "mirall/sitecopyconfig.h"
#include "mirall/owncloudfolder.h"
#include "mirall/statusdialog.h"
#include "mirall/owncloudsetupwizard.h"
#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"

#include "mirall/miralltheme.h"
#include "mirall/owncloudtheme.h"

#ifdef WITH_CSYNC
#include "mirall/csyncfolder.h"
#endif
#include "mirall/inotify.h"

namespace Mirall {

Application::Application(int argc, char **argv) :
    QApplication(argc, argv),
    _networkMgr(new QNetworkConfigurationManager(this)),
    _contextMenu(0)
{
#ifdef OWNCLOUD_CLIENT
    _theme = new ownCloudTheme();
#else
    _theme = new mirallTheme();
#endif
    setApplicationName( _theme->appName() );
    INotify::initialize();

    _folderMan = new FolderMan();
    connect( _folderMan, SIGNAL(folderSyncStateChange(QString)),
             this,SLOT(slotSyncStateChange(QString)));

    /* use a signal mapper to map the open requests to the alias names */
    _folderOpenActionMapper = new QSignalMapper(this);
    connect(_folderOpenActionMapper, SIGNAL(mapped(const QString &)),
            this, SLOT(slotFolderOpenAction(const QString &)));

    setQuitOnLastWindowClosed(false);

    _folderWizard = new FolderWizard();
    _owncloudSetupWizard = new OwncloudSetupWizard();

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

    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (QNetworkConfiguration netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }

    /* setup the folder list */
    int cnt =  _folderMan->setupFolders();

    setupActions();
    setupSystemTray();

    if( cnt ) {
        _tray->setIcon(QIcon::fromTheme(MIRALL_ICON, QIcon( QString( ":/mirall/resources/%1").arg(MIRALL_ICON))));
    }

    qDebug() << "Network Location: " << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    qDebug() << "* Mirall shutdown";
    INotify::cleanup();

    delete _networkMgr;
    delete _folderMan;
    delete _tray;
}

void Application::setupActions()
{
    _actionAddFolder = new QAction(tr("Add folder..."), this);
    QObject::connect(_actionAddFolder, SIGNAL(triggered(bool)), SLOT(slotAddFolder()));
    _actionConfigure = new QAction(tr("Configure..."), this);
    QObject::connect(_actionConfigure, SIGNAL(triggered(bool)), SLOT(slotConfigure()));


    _actionQuit = new QAction(tr("Quit"), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), SLOT(quit()));
}

void Application::setupSystemTray()
{
    _tray = new QSystemTrayIcon(this);
    _tray->setIcon(QIcon::fromTheme(FOLDER_ICON_EMPTY, QIcon( QString( ":/mirall/resources/%1").arg(FOLDER_ICON_EMPTY))));

    connect(_tray,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            SLOT(slotTrayClicked(QSystemTrayIcon::ActivationReason)));

    setupContextMenu();

    _tray->show();
}

void Application::setupContextMenu()
{
    delete _contextMenu;
    _contextMenu = new QMenu();
    _contextMenu->setTitle(_theme->appName() );
    _contextMenu->addAction(_actionConfigure);
    _contextMenu->addAction(_actionAddFolder);
    _contextMenu->addSeparator();

    // here all folders should be added
    foreach (Folder *folder, _folderMan->map() ) {
        QAction *action = new QAction( tr("open %1").arg( folder->alias()), this );
        action->setIcon( _theme->folderIcon( folder->backend(), 22) );

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
    if( f ) {
        QDesktopServices::openUrl(QUrl( f->path() ));
    }
}

void Application::slotTrayClicked( QSystemTrayIcon::ActivationReason reason )
{
  if( reason == QSystemTrayIcon::Trigger ) {
    _folderMan->disableFoldersWithRestore();
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
      _statusDialog->setFolderList( _folderMan->map() );
      _statusDialog->show();
    }
    _folderMan->restoreEnabledFolders();
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
    }

  } else {
    qDebug() << "* Folder wizard cancelled";
  }
  _folderMan->restoreEnabledFolders();
}

void Application::slotRemoveFolder( const QString& alias )
{
  int ret = QMessageBox::question( 0, tr("Confirm Folder Remove"), tr("Do you really want to remove upload folder <i>%1</i>?").arg(alias),
                                    QMessageBox::Yes|QMessageBox::No );

  if( ret == QMessageBox::No ) {
    return;
  }

  _folderMan->slotRemoveFolder( alias );
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

  if( f->backend() == "sitecopy" ) {
    if( QMessageBox::question( 0, tr("Confirm Folder Fetch"), tr("Do you really want to fetch the folder with alias <i>%1</i> from your ownCloud?<br/>"
                                                                 "This overwrites your local data in directory <i>%2</i>!").arg(alias).arg(f->path()),
                                                                 QMessageBox::Yes|QMessageBox::No ) == QMessageBox::Yes ) {
      SiteCopyFolder *sf = static_cast<SiteCopyFolder*>( f );
      sf->fetchFromOC();
    } else {
      qDebug() << "!! Can only fetch backend type sitecopy, this one has " << f->backend();
    }
  }

}

void Application::slotPushFolder( const QString& alias )
{
  qDebug() << "start to push folder with alias " << alias;

  if( ! _folderMap.contains( alias ) ) {
    qDebug() << "!! Can not push alias " << alias << ", can not be found in folderMap.";
    return;
  }

  Folder *f = _folderMap[alias];

  if( f->backend() == "sitecopy" ) {
    if( QMessageBox::question( 0, tr("Confirm Folder Push"), tr("Do you really want to push the folder with alias <i>%1</i> to your ownCloud?<br/>"
                                                                 "This overwrites your remote data with data in directory <i>%2</i>!").arg(alias).arg(f->path()),
                                                                 QMessageBox::Yes|QMessageBox::No ) == QMessageBox::Yes ) {
      SiteCopyFolder *sf = static_cast<SiteCopyFolder*>( f );
      sf->pushToOC();
    } else {
      qDebug() << "!! Can only fetch backend type sitecopy, this one has " << f->backend();
    }
  }
}
#endif

void Application::slotInfoFolder( const QString& alias )
{
    qDebug() << "details of folder with alias " << alias;

    SyncResult folderResult = _folderMan->syncResult( alias );

    QString folderMessage = tr( "Last sync was succesful" );

    SyncResult::Status syncStatus = folderResult.status();
    if ( syncStatus == SyncResult::Error ) {
      folderMessage = tr( "%1" ).arg( folderResult.errorString() );
    } else if ( syncStatus == SyncResult::SetupError ) {
      folderMessage = tr( "Setup error" );
    } else if ( syncStatus == SyncResult::Disabled ) {
      folderMessage = tr( "%1" ).arg( folderResult.errorString() );
    } else if ( syncStatus == SyncResult::Undefined ) {
      folderMessage = tr( "Undefined state" );
    }

    QMessageBox infoBox( QMessageBox::Information, tr( "Folder information" ), alias, QMessageBox::Ok );

    infoBox.setInformativeText(folderMessage);
    qDebug() << "informative text: " << infoBox.informativeText();

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
  qDebug() << "enable folder with alias " << alias;

  _folderMan->slotEnableFolder( alias, enable );

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

    _statusDialog->setFolderList( _folderMan->map() );
    computeOverallSyncStatus();

    qDebug() << "Sync state changed for folder " << alias << ": "  << result.errorString();
}

void Application::computeOverallSyncStatus()
{

  // display the info of the least succesful sync (eg. not just display the result of the latest sync
  SyncResult overallResult = SyncResult::Success;
  QString trayMessage;
  Folder::Map map = _folderMan->map();

  foreach ( Folder *syncedFolder, map ) {
    QString folderMessage;
    SyncResult folderResult = syncedFolder->syncResult();
    SyncResult::Status syncStatus = folderResult.status();
    if ( syncStatus == SyncResult::Success ) {
      folderMessage = tr( "Folder %1: Ok." ).arg( syncedFolder->alias() );
    } else if ( syncStatus == SyncResult::Error ) {
      overallResult = SyncResult::Error;
      folderMessage = tr( "Folder %1: %2" ).arg( syncedFolder->alias(), folderResult.errorString() );
    } else if ( syncStatus == SyncResult::SetupError ) {
      if ( overallResult.status() != SyncResult::Error ) {
        overallResult = SyncResult::SetupError;
      }
      folderMessage = tr( "Folder %1: setup error" ).arg( syncedFolder->alias() );
    } else if ( syncStatus == SyncResult::Disabled ) {
      if ( overallResult.status() != SyncResult::SetupError
           && overallResult.status() != SyncResult::Error ) {
        overallResult = SyncResult::Disabled;
      }
      folderMessage = tr( "Folder %1: %2" ).arg( syncedFolder->alias(), folderResult.errorString() );
    } else if ( syncStatus == SyncResult::Undefined ) {
      if ( overallResult.status() == SyncResult::Success ) {
        overallResult = SyncResult::Undefined;
      }
      folderMessage = tr( "Folder %1: undefined state" ).arg( syncedFolder->alias() );
    }
    if ( !trayMessage.isEmpty() ) {
      trayMessage += "\n";
    }
    trayMessage += folderMessage;
  }

  QString statusIcon = MIRALL_ICON;
  qDebug() << "overall result is " << overallResult.status();
  if( overallResult.status() == SyncResult::Error ) {
    statusIcon = "dialog-close";
  } else if( overallResult.status() == SyncResult::Success ) {
    statusIcon = MIRALL_ICON;
  } else if( overallResult.status() == SyncResult::Disabled ) {
    statusIcon = "dialog-cancel";
  } else if( overallResult.status() == SyncResult::SetupError ) {
    statusIcon = "dialog-cancel";
  } else if( overallResult.status() == SyncResult::Undefined ) {
    statusIcon = "view-refresh";
  }

  _tray->setIcon(QIcon::fromTheme(statusIcon, QIcon( QString( ":/mirall/resources/%1").arg(statusIcon))));
  _tray->setToolTip(trayMessage);

  // Only refresh the folder if it is being shown
  if( _statusDialog->isVisible() ) {
    _statusDialog->setFolderList( map );
  }
}


} // namespace Mirall

#include "application.moc"
