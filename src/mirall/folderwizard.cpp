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

#include "mirall/folderwizard.h"
#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>

#include <stdlib.h>

namespace Mirall
{

FolderWizardSourcePage::FolderWizardSourcePage()
    : QWizardPage()
{
    _ui.setupUi(this);
    registerField(QLatin1String("sourceFolder*"), _ui.localFolderLineEdit);
    QString defaultPath = QString::fromLatin1( "%1/%2").arg( QDir::homePath() ).arg(Theme::instance()->appName() );
    _ui.localFolderLineEdit->setText( QDir::toNativeSeparators( defaultPath ) );
    registerField(QLatin1String("alias*"), _ui.aliasLineEdit);
    _ui.aliasLineEdit->setText( Theme::instance()->appNameGUI() );

    _ui.warnLabel->hide();
}

FolderWizardSourcePage::~FolderWizardSourcePage()
{

}

void FolderWizardSourcePage::initializePage()
{
  _ui.warnLabel->hide();
}

void FolderWizardSourcePage::cleanupPage()
{
  _ui.warnLabel->hide();
}

bool FolderWizardSourcePage::isComplete() const
{
  QFileInfo selFile( QDir::fromNativeSeparators(_ui.localFolderLineEdit->text()) );
  QString   userInput = selFile.canonicalFilePath();

  QString warnString;

  bool isOk = selFile.isDir();
  if( !isOk ) {
    warnString = tr("No local folder selected!");
  }

  if (isOk && !selFile.isWritable()) {
      isOk = false;
      warnString += tr("You have no permission to write to the selected folder!");
  }

  // check if the local directory isn't used yet in another ownCloud sync
  Folder::Map map = _folderMap;

  if( isOk ) {
    Folder::Map::const_iterator i = map.constBegin();
    while( isOk && i != map.constEnd() ) {
      Folder *f = static_cast<Folder*>(i.value());
      QString folderDir = QDir( f->path() ).canonicalPath();
      if( folderDir.isEmpty() )
      {
        isOk = true;
        qDebug() << "Absolute path for folder: " << f->path() << " doesn't exist. Skipping.";
        i++;
        continue;
      }
      if( ! folderDir.endsWith(QLatin1Char('/')) ) folderDir.append(QLatin1Char('/'));

      qDebug() << "Checking local path: " << folderDir << " <-> " << userInput;
      if( QFileInfo( f->path() ) == userInput ) {
        isOk = false;
        warnString.append( tr("The local path %1 is already an upload folder.<br/>Please pick another one!")
                           .arg(QDir::toNativeSeparators(userInput)) );
      }
      if( isOk && folderDir.startsWith( userInput )) {
        qDebug() << "A already configured folder is child of the current selected";
        warnString.append( tr("An already configured folder is contained in the current entry."));
        isOk = false;
      }
      if( isOk && userInput.startsWith( folderDir ) ) {
        qDebug() << "An already configured folder is parent of the current selected";
        warnString.append( tr("An already configured folder contains the currently entered directory."));
        isOk = false;
      }
      i++;
    }
  }

  // check if the alias is unique.
  QString alias = _ui.aliasLineEdit->text();
  if( alias.isEmpty() ) {
    warnString.append( tr("The alias can not be empty. Please provide a descriptive alias word.") );
    isOk = false;
  }

  Folder::Map::const_iterator i = map.constBegin();
  bool goon = true;
  while( goon && i != map.constEnd() ) {
    Folder *f = i.value();
    qDebug() << "Checking local alias: " << f->alias();
    if( f ) {
      if( f->alias() == alias ) {
        warnString.append( tr("<br/>The alias <i>%1</i> is already in use. Please pick another alias.").arg(alias) );
        isOk = false;
        goon = false;
      }
    }
    i++;
  }

  if( isOk ) {
    _ui.warnLabel->hide();
    _ui.warnLabel->setText( QString::null );
  } else {
    _ui.warnLabel->show();
    _ui.warnLabel->setText( warnString );
  }
  return isOk;
}

void FolderWizardSourcePage::on_localFolderChooseBtn_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select the source folder"),
                                                    QDesktopServices::storageLocation(QDesktopServices::HomeLocation));
    if (!dir.isEmpty()) {
        _ui.localFolderLineEdit->setText(QDir::toNativeSeparators(dir));
    }
}

void FolderWizardSourcePage::on_localFolderLineEdit_textChanged()
{
    emit completeChanged();
}


// =================================================================================
FolderWizardTargetPage::FolderWizardTargetPage()
: _warnWasVisible(false)
{
    _ui.setupUi(this);
    _ui.warnFrame->hide();

    connect(_ui.addFolderButton, SIGNAL(clicked()), SLOT(slotAddRemoteFolder()));
    connect(_ui.refreshButton, SIGNAL(clicked()), SLOT(slotRefreshFolders())),
    connect(_ui.folderListWidget, SIGNAL(currentTextChanged(QString)),
            SIGNAL(completeChanged()));
}

void FolderWizardTargetPage::slotAddRemoteFolder()
{
    QInputDialog *dlg = new QInputDialog(this);
    dlg->open(this, SLOT(slotCreateRemoteFolder(QString)));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
}

void FolderWizardTargetPage::slotCreateRemoteFolder(QString folder)
{
    if( folder.isEmpty() ) return;
    ownCloudInfo::instance()->mkdirRequest( folder );
}

void FolderWizardTargetPage::slotCreateRemoteFolderFinished( QNetworkReply::NetworkError error )
{
  qDebug() << "** webdav mkdir request finished " << error;

  // the webDAV server seems to return a 202 even if mkdir was successful.
  if( error == QNetworkReply::NoError ||
          error == QNetworkReply::ContentOperationNotPermittedError) {
    showWarn( tr("Folder was successfully created on %1.").arg( Theme::instance()->appNameGUI() ) );
    slotRefreshFolders();
  } else {
    showWarn( tr("Failed to create the folder on %1.<br/>Please check manually.").arg( Theme::instance()->appNameGUI() ) );
  }
}

void FolderWizardTargetPage::slotUpdateDirectories(QStringList list)
{
    _ui.folderListWidget->clear();
    foreach (QString item, list) {
        item.remove(QLatin1String("/remote.php/webdav"));
        _ui.folderListWidget->addItem(item);
    }
}

void FolderWizardTargetPage::slotRefreshFolders()
{
    ownCloudInfo::instance()->getDirectoryListing("/");
}

FolderWizardTargetPage::~FolderWizardTargetPage()
{
}

bool FolderWizardTargetPage::isComplete() const
{
    if (!_ui.folderListWidget->currentItem())
        return false;

    QString dir = _ui.folderListWidget->currentItem()->text();
    wizard()->setProperty("targetPath", dir);

    if( dir == QLatin1String("/") ) {
        showWarn( tr("If you sync the root folder, you can <b>not</b> configure another sync directory."));
        return true;
    } else {
        showWarn();
        return true;
    }
}

void FolderWizardTargetPage::cleanupPage()
{
    showWarn();
}

void FolderWizardTargetPage::initializePage()
{
    showWarn();

    /* check the owncloud configuration file and query the ownCloud */
    ownCloudInfo *ocInfo = ownCloudInfo::instance();
    if( ocInfo->isConfigured() ) {
        connect( ocInfo, SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
                 SLOT(slotDirCheckReply(QString,QNetworkReply*)));
        connect( ocInfo, SIGNAL(webdavColCreated(QNetworkReply::NetworkError)),
                 SLOT(slotCreateRemoteFolderFinished( QNetworkReply::NetworkError )));
        connect( ocInfo, SIGNAL(directoryListingUpdated(QStringList)),
                 SLOT(slotUpdateDirectories(QStringList)));

        slotRefreshFolders();
    }
}

void FolderWizardTargetPage::showWarn( const QString& msg ) const
{
  if( msg.isEmpty() ) {
    _ui.warnFrame->hide();

  } else {
    _ui.warnFrame->show();
    _ui.warnLabel->setText( msg );
  }
}

// ====================================================================================

/**
 * Folder wizard itself
 */

FolderWizard::FolderWizard( QWidget *parent )
    : QWizard(parent),
    _folderWizardSourcePage(new FolderWizardSourcePage),
    _folderWizardTargetPage(0)
{
    setPage(Page_Source, _folderWizardSourcePage );
    if (!Theme::instance()->singleSyncFolder()) {
        _folderWizardTargetPage = new FolderWizardTargetPage();
        setPage(Page_Target, _folderWizardTargetPage );
    }

    setWindowTitle( tr( "%1 Folder Wizard" ).arg( Theme::instance()->appNameGUI() ) );
#ifdef Q_WS_MAC
    setWizardStyle( QWizard::ModernStyle );
#endif
}

FolderWizard::~FolderWizard()
{
}

void FolderWizard::setFolderMap( const Folder::Map& fm)
{
    _folderWizardSourcePage->setFolderMap( fm );
}

} // end namespace

