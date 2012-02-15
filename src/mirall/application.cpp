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
#include "mirall/owncloudsetup.h"

#ifdef WITH_CSYNC
#include "mirall/csyncfolder.h"
#endif
#include "mirall/inotify.h"

namespace Mirall {

Application::Application(int argc, char **argv) :
    QApplication(argc, argv),
    _networkMgr(new QNetworkConfigurationManager(this)),
    _folderSyncCount(0),
    _contextMenu(0)
{
    INotify::initialize();

    setApplicationName("Mirall");
    setQuitOnLastWindowClosed(false);

    _folderWizard = new FolderWizard();
    _owncloudSetup = new OwncloudSetup();
    _statusDialog = new StatusDialog();
    _folderConfigPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/folders";

    connect( _statusDialog, SIGNAL(removeFolderAlias( const QString&)),
             SLOT(slotRemoveFolder(const QString&)));
    connect( _statusDialog, SIGNAL(fetchFolderAlias(const QString&)),
             SLOT(slotFetchFolder( const QString&)));
    connect( _statusDialog, SIGNAL(pushFolderAlias(const QString&)),
             SLOT(slotPushFolder( const QString&)));
    connect( _statusDialog, SIGNAL(enableFolderAlias(QString,bool)),
             SLOT(slotEnableFolder(QString,bool)));
    connect( _statusDialog, SIGNAL(infoFolderAlias(const QString&)),
             SLOT(slotInfoFolder( const QString&)));

    setupActions();
    setupSystemTray();

    qDebug() << "* Network is" << (_networkMgr->isOnline() ? "online" : "offline");
    foreach (QNetworkConfiguration netCfg, _networkMgr->allConfigurations(QNetworkConfiguration::Active)) {
        //qDebug() << "Network:" << netCfg.identifier();
    }

    // if QDir::mkpath would not be so stupid, I would not need to have this
    // duplication of folderConfigPath() here
    QDir storageDir(QDesktopServices::storageLocation(QDesktopServices::DataLocation));
    storageDir.mkpath("folders");

    // Look for configuration changes
    _configFolderWatcher = new FolderWatcher(storageDir.path());
    connect(_configFolderWatcher, SIGNAL(folderChanged(const QStringList &)),
            this, SLOT(slotReparseConfiguration()));

    setupKnownFolders();
    setupContextMenu();

    qDebug() << "Network Location: " << NetworkLocation::currentLocation().encoded();
}

Application::~Application()
{
    qDebug() << "* Mirall shutdown";
    INotify::cleanup();

    delete _networkMgr;
    delete _tray;

    foreach (Folder *folder, _folderMap) {
        delete folder;
    }
}

QString Application::folderConfigPath() const
{
    return _folderConfigPath;
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

    _tray->show();
}

void Application::slotTrayClicked( QSystemTrayIcon::ActivationReason reason )
{
  if( reason == QSystemTrayIcon::Trigger ) {
    disableFoldersWithRestore();
    // check if there is a mirall.cfg already.
    if( _owncloudSetup->wizard()->isVisible() ) {
      _owncloudSetup->wizard()->show();
    }
    QFile fi( QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/mirall.cfg" );
    if( !fi.exists() ) {
      qDebug() << "No configured folders yet, start the Owncloud integration dialog.";
      _owncloudSetup->startWizard();
    } else {
      _statusDialog->setOCUrl( QUrl( _owncloudSetup->ownCloudUrl()));

      _statusDialog->show();
    }
    restoreEnabledFolders();
    if ( !_folderMap.isEmpty() && _statusDialog->isVisible() ) {
      _statusDialog->setFolderList( _folderMap );
    }
  }
}

void Application::setupContextMenu()
{
    delete _contextMenu;
    _contextMenu = new QMenu();
    _contextMenu->setTitle(tr( "Mirall" ));
    _contextMenu->addAction(_actionConfigure);
    _contextMenu->addAction(_actionAddFolder);
    _contextMenu->addSeparator();

    // here all folders should be added
    foreach (Folder *folder, _folderMap) {
        _contextMenu->addAction(folder->openAction());
    }

    _contextMenu->addSeparator();

    _contextMenu->addAction(_actionQuit);
    _tray->setContextMenu(_contextMenu);
}

void Application::slotReparseConfiguration()
{
    setupKnownFolders();
    setupContextMenu();
}

void Application::slotAddFolder()
{
  disableFoldersWithRestore();

  _folderWizard->setFolderMap( &_folderMap );

  _folderWizard->restart();

  if (_folderWizard->exec() == QDialog::Accepted) {
    qDebug() << "* Folder wizard completed";

    QString alias = _folderWizard->field("alias").toString();

    QSettings settings(folderConfigPath() + "/" + alias, QSettings::IniFormat);
    settings.setValue("folder/backend", "csync");
    settings.setValue("folder/path", _folderWizard->field("sourceFolder"));

    if (_folderWizard->field("local?").toBool()) {
      settings.setValue("backend:csync/secondPath", _folderWizard->field("targetLocalFolder"));
    } else if (_folderWizard->field("remote?").toBool()) {
      settings.setValue("backend:csync/secondPath", _folderWizard->field("targetURLFolder"));
      bool onlyOnline = _folderWizard->field("onlyOnline?").toBool();
      settings.setValue("folder/onlyOnline", onlyOnline);

      if (onlyOnline) {
        bool onlyThisLAN = _folderWizard->field("onlyThisLAN?").toBool();
        settings.setValue("folder/onlyThisLAN", onlyThisLAN);
        if (onlyThisLAN) {
          settings.setValue("folder/onlyOnline", true);
        }
      }
    } else if( _folderWizard->field("OC?").toBool()) {
      settings.setValue("folder/backend", "owncloud");
      settings.setValue("backend:owncloud/targetPath", _folderWizard->field("targetOCFolder"));
      settings.setValue("backend:owncloud/alias",  _folderWizard->field("alias"));

      qDebug() << "Now writing owncloud config " << _folderWizard->field("alias").toString(); ;
    } else {
      qWarning() << "* Folder not local and note remote?";
      return;
    }

    settings.sync();
    setupFolderFromConfigFile(alias);
    setupContextMenu();
  } else {
    qDebug() << "* Folder wizard cancelled";
  }
  restoreEnabledFolders();
}

void Application::slotRemoveFolder( const QString& alias )
{
  QString configFile = folderConfigPath() + "/" + alias;
  QFile file( configFile );

  int ret = QMessageBox::question( 0, tr("Confirm Folder Remove"), tr("Do you really want to remove upload folder <i>%1</i>?").arg(alias),
                                    QMessageBox::Yes|QMessageBox::No );

  if( ret == QMessageBox::No ) {
    return;
  }

  if( _folderMap.contains( alias )) {
    qDebug() << "Removing " << alias;
    Folder *f = _folderMap.take( alias );
    delete f;
  } else {
    qDebug() << "!! Can not remove " << alias << ", not in folderMap.";
  }

  if( file.exists() ) {
    qDebug() << "Remove folder config file " << configFile;
    file.remove();
  }

  SitecopyConfig scConfig;
  if( ! scConfig.removeFolderConfig( alias ) ) {
    qDebug() << "Failed to remove folder config for " << alias;
  } else {
    setupKnownFolders();
    _statusDialog->setFolderList( _folderMap );
  }
}

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

void Application::slotInfoFolder( const QString& alias )
{
    qDebug() << "details of folder with alias " << alias;

    if( ! _folderMap.contains( alias ) ) {
      qDebug() << "!! Can not get details of alias " << alias << ", can not be found in folderMap.";
      return;
    }

    Folder *folder = _folderMap[alias];

    QString folderMessage = tr( "Last sync was succesful" );
    SyncResult folderResult = folder->lastSyncResult();
    SyncResult::Result syncResult = folderResult.result();
    if ( syncResult == SyncResult::Error ) {
      folderMessage = tr( "%1" ).arg( folderResult.errorString() );
    } else if ( syncResult == SyncResult::SetupError ) {
      folderMessage = tr( "Setup error" );
    } else if ( syncResult == SyncResult::Disabled ) {
      folderMessage = tr( "%1" ).arg( folderResult.errorString() );
    } else if ( syncResult == SyncResult::Undefined ) {
      folderMessage = tr( "Undefined state" );
    }

    QMessageBox infoBox( QMessageBox::Information, tr( "Folder information" ), folder->alias(), QMessageBox::Ok );

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

  if( ! _folderMap.contains( alias ) ) {
    qDebug() << "!! Can not enable alias " << alias << ", can not be found in folderMap.";
    return;
  }

  Folder *f = _folderMap[alias];
  f->setSyncEnabled(enable);
}

void Application::slotConfigure()
{
  disableFoldersWithRestore();
  _owncloudSetup->startWizard();
  restoreEnabledFolders();
}

void Application::setupKnownFolders()
{
  qDebug() << "* Setup folders from " << folderConfigPath();

  _folderMap.clear();
  QDir dir(folderConfigPath());
  dir.setFilter(QDir::Files);
  QStringList list = dir.entryList();
  foreach (QString file, list) {
    setupFolderFromConfigFile(file);
  }
  if( list.size() ) _tray->setIcon(QIcon::fromTheme(MIRALL_ICON, QIcon( QString( ":/mirall/resources/%1").arg(MIRALL_ICON))));
}

// filename is the name of the file only, it does not include
// the configuration directory path
void Application::setupFolderFromConfigFile(const QString &file) {
    Folder *folder = 0L;

    qDebug() << "  ` -> setting up:" << file;
    QSettings settings(folderConfigPath() + "/" + file, QSettings::IniFormat);
    qDebug() << "    -> file path: " + settings.fileName();

    if (!settings.contains("folder/path")) {
        qWarning() << "   `->" << file << "is not a valid folder configuration";
        return;
    }

    QVariant path = settings.value("folder/path").toString();
    if (path.isNull() || !QFileInfo(path.toString()).isDir()) {
        qWarning() << "    `->" << path.toString() << "does not exist. Skipping folder" << file;
        _tray->showMessage(tr("Unknown folder"),
                           tr("Folder %1 does not exist").arg(path.toString()),
                           QSystemTrayIcon::Critical);
        return;
    }

    QString backend = settings.value("folder/backend").toString();
    if (!backend.isEmpty()) {
        if( backend == "sitecopy") {
            qCritical() << "* sitecopy is not longer supported in this release." << endl;
        } else if (backend == "unison") {
            folder = new UnisonFolder(file,
                                      path.toString(),
                                      settings.value("backend:unison/secondPath").toString(),
                                      this);
        } else if (backend == "csync") {
#ifdef WITH_CSYNC
            folder = new CSyncFolder(file,
                                     path.toString(),
                                     settings.value("backend:csync/secondPath").toString(),
                                     this);
#else
            qCritical() << "* csync support not enabled!! ignoring:" << file;
#endif
        } else if( backend == "owncloud" ) {
#ifdef WITH_CSYNC
            QUrl url( _owncloudSetup->fullOwnCloudUrl() );
            QString existPath = url.path();
            qDebug() << "existing path: "  << existPath;
            QString newPath = settings.value("backend:owncloud/targetPath").toString();
            if( !existPath.isEmpty() ) {
                // cut off the trailing slash
                if( existPath.endsWith('/') ) {
                    existPath.truncate( existPath.length()-1 );
                }
                // cut off the leading slash
                if( newPath.startsWith('/') ) {
                    newPath.remove(0,1);
                }
            }

            url.setPath( QString("%1/files/webdav.php/%2").arg(existPath).arg(newPath) );

            folder = new ownCloudFolder( file, path.toString(),
                                         url.toString(),
                                         this );


#else
            qCritical() << "* owncloud support not enabled!! ignoring:" << file;
#endif
        }

        else {
            qWarning() << "unknown backend" << backend;
            return;
        }
    }
    folder->setBackend( backend );
    folder->setOnlyOnlineEnabled(settings.value("folder/onlyOnline", false).toBool());
    folder->setOnlyThisLANEnabled(settings.value("folder/onlyThisLAN", false).toBool());

    _folderMap[file] = folder;
    qDebug() << "Adding folder to Folder Map " << folder;
    QObject::connect(folder, SIGNAL(syncStarted()), SLOT(slotFolderSyncStarted()));
    QObject::connect(folder, SIGNAL(syncFinished(const SyncResult &)), SLOT(slotFolderSyncFinished(const SyncResult &)));
}

void Application::slotFolderSyncStarted()
{
    _folderSyncCount++;

    if (_folderSyncCount > 0) {
        _tray->setIcon(QIcon::fromTheme(FOLDER_SYNC_ICON, QIcon( QString( ":/mirall/resources/%1").arg(FOLDER_SYNC_ICON))));
    }
}

void Application::slotFolderSyncFinished(const SyncResult &result)
{
  _folderSyncCount--;

  // in case the sending folder is needed:
  // Folder *folder = dynamic_cast<Folder *>(sender());

  if( _folderSyncCount == 0 ) {
    computeOverallSyncStatus();
  }
}

void Application::computeOverallSyncStatus()
{

  // display the info of the least succesful sync (eg. not just display the result of the latest sync
  SyncResult overallResult = SyncResult::Success;
  QString trayMessage;
  foreach ( Folder *syncedFolder, _folderMap ) {
    QString folderMessage;
    SyncResult folderResult = syncedFolder->lastSyncResult();
    SyncResult::Result syncResult = folderResult.result();
    if ( syncResult == SyncResult::Success ) {
      folderMessage = tr( "Folder %1: Ok." ).arg( syncedFolder->alias() );
    } else if ( syncResult == SyncResult::Error ) {
      overallResult = SyncResult::Error;
      folderMessage = tr( "Folder %1: %2" ).arg( syncedFolder->alias(), folderResult.errorString() );
    } else if ( syncResult == SyncResult::SetupError ) {
      if ( overallResult.result() != SyncResult::Error ) {
        overallResult = SyncResult::SetupError;
      }
      folderMessage = tr( "Folder %1: setup error" ).arg( syncedFolder->alias() );
    } else if ( syncResult == SyncResult::Disabled ) {
      if ( overallResult.result() != SyncResult::SetupError
           && overallResult.result() != SyncResult::Error ) {
        overallResult = SyncResult::Disabled;
      }
      folderMessage = tr( "Folder %1: %2" ).arg( syncedFolder->alias(), folderResult.errorString() );
    } else if ( syncResult == SyncResult::Undefined ) {
      if ( overallResult.result() == SyncResult::Success ) {
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
  qDebug() << "overall result is " << overallResult.result();
  if( overallResult.result() == SyncResult::Error ) {
    statusIcon = "dialog-close";
  } else if( overallResult.result() == SyncResult::Success ) {
    statusIcon = MIRALL_ICON;
  } else if( overallResult.result() == SyncResult::Disabled ) {
    statusIcon = "dialog-cancel";
  } else if( overallResult.result() == SyncResult::SetupError ) {
    statusIcon = "dialog-cancel";
  } else if( overallResult.result() == SyncResult::Undefined ) {
    statusIcon = "view-refresh";
  }

  _tray->setIcon(QIcon::fromTheme(statusIcon, QIcon( QString( ":/mirall/resources/%1").arg(statusIcon))));
  _tray->setToolTip(trayMessage);

  // Only refresh the folder if it is being shown
  if( _statusDialog->isVisible() ) {
    _statusDialog->setFolderList( _folderMap );
  }
}

void Application::disableFoldersWithRestore()
{
  _folderEnabledMap.clear();
  foreach( Folder *f, _folderMap ) {
    // store the enabled state, then make sure it is disabled
    _folderEnabledMap.insert(f->alias(), f->syncEnabled());
    f->setSyncEnabled(false);
  }
}

void Application::restoreEnabledFolders()
{
  foreach( Folder *f, _folderMap ) {
    if (_folderEnabledMap.contains( f->alias() )) {
      f->setSyncEnabled( _folderEnabledMap.value( f->alias() ));
    }
  }
}

} // namespace Mirall

#include "application.moc"
