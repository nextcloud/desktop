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

#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>
#include <QDir>

#include <stdlib.h>

#include "mirall/folderwizard.h"
#include "mirall/owncloudinfo.h"
#include "mirall/ownclouddircheck.h"
#include "mirall/mirallwebdav.h"
#include "mirall/mirallconfigfile.h"

namespace Mirall
{

FolderWizardSourcePage::FolderWizardSourcePage()
  :_folderMap(0)
{
    _ui.setupUi(this);
    registerField("sourceFolder*", _ui.localFolderLineEdit);
    _ui.localFolderLineEdit->setText( QString( "%1/%2").arg( QDir::homePath() ).arg("ownCloud" ) );
    registerField("alias*", _ui.aliasLineEdit);
    _ui.aliasLineEdit->setText( QString::fromLatin1("ownCloud") );

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
  QFileInfo selFile( _ui.localFolderLineEdit->text() );
  QString warnString;

  bool isOk = selFile.isDir();
  if( !isOk ) {
    warnString = tr("No local directory selected!");
  }
  // check if the local directory isn't used yet in another ownCloud sync
  Folder::Map *map = _folderMap;
  if( ! map ) return false;

  if( isOk ) {
    Folder::Map::const_iterator i = map->begin();
    while( isOk && i != map->constEnd() ) {
      Folder *f = static_cast<Folder*>(i.value());
      QString folderDir = QDir( f->path() ).absolutePath();

      qDebug() << "Checking local path: " << folderDir << " <-> " << selFile.absoluteFilePath();
      if( QFileInfo( f->path() ) == selFile ) {
        isOk = false;
        warnString.append( tr("The local path %1 is already an upload folder.<br/>Please pick another one!").arg(selFile.absoluteFilePath()) );
      }
      if( isOk && folderDir.startsWith( selFile.absoluteFilePath() )) {
        qDebug() << "A already configured folder is child of the current selected";
        warnString.append( tr("An already configured folder is contained in the current entry."));
        isOk = false;
      }
      if( isOk && selFile.absoluteFilePath().startsWith( folderDir ) ) {
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

  Folder::Map::const_iterator i = map->begin();
  bool goon = true;
  while( goon && i != map->constEnd() ) {
    Folder *f = static_cast<Folder*>(i.value());
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
    _ui.warnLabel->setText( QString() );
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
        _ui.localFolderLineEdit->setText(dir);
    }
}

void FolderWizardSourcePage::on_localFolderLineEdit_textChanged()
{
    emit completeChanged();
}


// =================================================================================
FolderWizardTargetPage::FolderWizardTargetPage()
: _dirChecked( false ),
  _warnWasVisible(false)
{
    _ui.setupUi(this);
    _ui.warnFrame->hide();

    registerField("local?",            _ui.localFolderRadioBtn);
    registerField("remote?",           _ui.urlFolderRadioBtn);
    registerField("OC?",               _ui.OCRadioBtn);
    registerField("targetLocalFolder", _ui.localFolder2LineEdit);
    registerField("targetURLFolder",   _ui.urlFolderLineEdit);
    registerField("targetOCFolder",    _ui.OCFolderLineEdit);

    connect( _ui.OCFolderLineEdit, SIGNAL(textChanged(QString)),
             SLOT(slotFolderTextChanged(QString)));

    _timer = new QTimer(this);
    _timer->setSingleShot( true );
    connect( _timer, SIGNAL(timeout()), SLOT(slotTimerFires()));

    _ownCloudDirCheck = new ownCloudDirCheck( this );

    connect( _ownCloudDirCheck, SIGNAL(directoryExists(QString,bool)),
             SLOT(slotDirCheckReply(QString,bool)));
}

void FolderWizardTargetPage::slotFolderTextChanged( const QString& t)
{
  _dirChecked = false;
  emit completeChanged();

  if( t.isEmpty() ) {
    _timer->stop();
    _ui.warnLabel->hide();
    return;
  }

  _timer->start(500);
}

void FolderWizardTargetPage::slotTimerFires()
{
  const QString folder = _ui.OCFolderLineEdit->text();
  qDebug() << "Querying folder " << folder;
  _ownCloudDirCheck->checkDirectory( folder );
}

void FolderWizardTargetPage::slotDirCheckReply(const QString &url, bool exists )
{
  qDebug() << "Got reply from ownCloudInfo: " << url << " :" << exists;
  _dirChecked = exists;
  if( _dirChecked ) {
    _ui.warnLabel->hide();
  } else {
    showWarn( tr("The folder is not available on your ownCloud.<br/>Click to let mirall create it."), true );
  }

  emit completeChanged();
}

void FolderWizardTargetPage::slotCreateRemoteFolder()
{
  _ui.OCFolderLineEdit->setEnabled( false );

  const QString folder = _ui.OCFolderLineEdit->text();
  if( folder.isEmpty() ) return;

  MirallConfigFile cfgFile;

  QString url = cfgFile.ownCloudUrl();
  if( ! url.endsWith('/')) url.append('/');
  url.append( "files/webdav.php/");
  url.append( folder );
  qDebug() << "creating folder on ownCloud: " << url;

  MirallWebDAV *webdav = new MirallWebDAV(this);
  connect( webdav, SIGNAL(webdavFinished(QNetworkReply*)),
           SLOT(slotCreateRemoteFolderFinished(QNetworkReply*)));

  webdav->httpConnect( url, cfgFile.ownCloudUser(), cfgFile.ownCloudPasswd() );
  if( webdav->mkdir(  url  ) ) {
    qDebug() << "WebDAV mkdir request successfully started";
  } else {
    qDebug() << "WebDAV mkdir request failed";
  }
}

void FolderWizardTargetPage::slotCreateRemoteFolderFinished( QNetworkReply *reply )
{
  qDebug() << "** webdav mkdir request finished " << reply->error();

  _ui.OCFolderLineEdit->setEnabled( true );
  if( reply->error() == QNetworkReply::NoError ) {
    showWarn( tr("Folder on ownCloud was successfully created."), false );
    slotTimerFires();
  } else {
    showWarn( tr("Failed to create the folder on ownCloud.<br/>Please check manually."), false );
  }
}

FolderWizardTargetPage::~FolderWizardTargetPage()
{

}

bool FolderWizardTargetPage::isComplete() const
{
    if (_ui.localFolderRadioBtn->isChecked()) {
        return QFileInfo(_ui.localFolder2LineEdit->text()).isDir();
    } else if (_ui.urlFolderRadioBtn->isChecked()) {
        QUrl url(_ui.urlFolderLineEdit->text());
        return url.isValid() && (url.scheme() == "sftp" || url.scheme() == "smb");
    } else if( _ui.OCRadioBtn->isChecked()) {
      /* owncloud selected */
      QString dir = _ui.OCFolderLineEdit->text();
      if( dir.isEmpty() ) {
        showWarn( tr("Better do not use the remote root directory.<br/>If you do, you can <b>not</b> mirror another local folder."), false);
        return true;
      } else {
        if( _dirChecked ) {
          showWarn();
        }
        return _dirChecked;
      }
    }
    return false;
}

void FolderWizardTargetPage::cleanupPage()
{
  _ui.warnFrame->hide();
}

void FolderWizardTargetPage::initializePage()
{
    slotToggleItems();
    _ui.warnFrame->hide();

    ownCloudInfo *ocInfo = new ownCloudInfo( this );
    if( ocInfo->isConfigured() ) {
      connect(ocInfo, SIGNAL(ownCloudInfoFound(QString,QString)),SLOT(slotOwnCloudFound(QString,QString)));
      connect(ocInfo,SIGNAL(noOwncloudFound()),SLOT(slotNoOwnCloudFound()));
      connect(_ui._buttCreateFolder, SIGNAL(clicked()), SLOT(slotCreateRemoteFolder()));
      ocInfo->checkInstallation();

    } else {
      _ui.OCRadioBtn->setEnabled( false );
      _ui.OCFolderLineEdit->setEnabled( false );
    }
}

void FolderWizardTargetPage::slotOwnCloudFound( const QString& url, const QString& infoStr )
{
  if( infoStr.isEmpty() ) {
  } else {
    _ui.OCLabel->setText( tr("to your <a href=\"%1\">ownCloud</a> (version %2)").arg(url).arg(infoStr));
    _ui.OCFolderLineEdit->setEnabled( true );
    _ui.OCRadioBtn->setEnabled( true );
    qDebug() << "ownCloud found on " << url << " with version: " << infoStr;
  }
}

void FolderWizardTargetPage::slotNoOwnCloudFound()
{
  qDebug() << "No ownCloud configured!";
  _ui.OCLabel->setText( tr("no configured ownCloud found!") );
  _ui.OCRadioBtn->setEnabled( false );
  _ui.OCFolderLineEdit->setEnabled( false );
}

void FolderWizardTargetPage::showWarn( const QString& msg, bool showCreateButton ) const
{
  _ui._buttCreateFolder->setVisible( showCreateButton );

  if( msg.isEmpty() ) {
    _ui.warnFrame->hide();
  } else {
    _ui.warnFrame->show();
    _ui.warnLabel->setText( msg );
  }
}

void FolderWizardTargetPage::on_localFolderRadioBtn_toggled()
{
    slotToggleItems();
    emit completeChanged();
}

void FolderWizardTargetPage::on_urlFolderRadioBtn_toggled()
{
    slotToggleItems();
    emit completeChanged();

}

void FolderWizardTargetPage::on_checkBoxOnlyOnline_toggled()
{
    slotToggleItems();
}

void FolderWizardTargetPage::on_localFolder2LineEdit_textChanged()
{
    emit completeChanged();
}

void FolderWizardTargetPage::on_urlFolderLineEdit_textChanged()
{
    emit completeChanged();
}

void FolderWizardTargetPage::slotToggleItems()
{

  bool enabled = _ui.localFolderRadioBtn->isChecked();
  _ui.localFolder2LineEdit->setEnabled(enabled);
  _ui.localFolder2ChooseBtn->setEnabled(enabled);
  if( enabled ) {
    _warnWasVisible = _ui.warnFrame->isVisible();
    _ui.warnFrame->hide();
  }

  enabled = _ui.urlFolderRadioBtn->isChecked();
  _ui.urlFolderLineEdit->setEnabled(enabled);
  if( enabled ) {
    _warnWasVisible = _ui.warnFrame->isVisible();
    _ui.warnFrame->hide();
  }

  enabled = _ui.OCRadioBtn->isChecked();
  _ui.OCFolderLineEdit->setEnabled(enabled);
  if( enabled ) _ui.warnFrame->setVisible( _warnWasVisible );
}

void FolderWizardTargetPage::on_localFolder2ChooseBtn_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select the target folder"),
                                                    QDesktopServices::storageLocation(QDesktopServices::HomeLocation));
    if (!dir.isEmpty()) {
        _ui.localFolder2LineEdit->setText(dir);
    }
}


// ====================================================================================

FolderWizardNetworkPage::FolderWizardNetworkPage()
{
    _ui.setupUi(this);
    registerField("onlyNetwork*", _ui.checkBoxOnlyOnline);
    registerField("onlyLocalNetwork*", _ui.checkBoxOnlyThisLAN );
}

FolderWizardNetworkPage::~FolderWizardNetworkPage()
{
}

bool FolderWizardNetworkPage::isComplete() const
{
    return true;
}

FolderWizardOwncloudPage::FolderWizardOwncloudPage()
{
    _ui.setupUi(this);
    registerField("OCUrl*",       _ui.lineEditOCUrl);
    registerField("OCUser*",      _ui.lineEditOCUser );
    registerField("OCPasswd",     _ui.lineEditOCPasswd);
    registerField("OCSiteAlias*", _ui.lineEditOCAlias);
}

FolderWizardOwncloudPage::~FolderWizardOwncloudPage()
{
}

void FolderWizardOwncloudPage::initializePage()
{
    _ui.lineEditOCAlias->setText( "ownCloud" );
    _ui.lineEditOCUrl->setText( "http://localhost/owncloud" );
    QString user( getenv("USER"));
    _ui.lineEditOCUser->setText( user );
}

bool FolderWizardOwncloudPage::isComplete() const
{

    bool hasAlias = !(_ui.lineEditOCAlias->text().isEmpty());
    QUrl u( _ui.lineEditOCUrl->text() );
    bool hasUrl   = u.isValid();

    return hasAlias && hasUrl;
}

/**
 * Folder wizard itself
 */

FolderWizard::FolderWizard(QWidget *parent)
    : QWizard(parent),
    _folderWizardSourcePage(0)
{
  _folderWizardSourcePage = new FolderWizardSourcePage();
    setPage(Page_Source,   _folderWizardSourcePage );
    setPage(Page_Target,   new FolderWizardTargetPage());
    // setPage(Page_Network,  new FolderWizardNetworkPage());
    // setPage(Page_Owncloud, new FolderWizardOwncloudPage());
    setWindowTitle( tr( "Mirall Folder Wizard") );
}

void FolderWizard::setFolderMap( Folder::Map *fm)
{
  if( _folderWizardSourcePage ) {
    _folderWizardSourcePage->setFolderMap( fm );
  }
}

} // end namespace

#include "folderwizard.moc"
