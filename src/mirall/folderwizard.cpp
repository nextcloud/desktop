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
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>
#include <QDir>

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
#if QT_VERSION >= 0x040700
    _ui.localFolderLineEdit->setPlaceholderText(QApplication::translate("FolderWizardSourcePage", "/home/local1", 0, QApplication::UnicodeUTF8));
    _ui.aliasLineEdit->setPlaceholderText(QApplication::translate("FolderWizardSourcePage", "Music", 0, QApplication::UnicodeUTF8));
#endif
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
    warnString = tr("No local directory selected!");
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
: _dirChecked( false ),
  _warnWasVisible(false)
{
    _ui.setupUi(this);
    _ui.warnFrame->hide();

    registerField(QLatin1String("OCFolderLineEdit"),    _ui.OCFolderLineEdit);

    connect( _ui.OCFolderLineEdit, SIGNAL(textChanged(QString)),
             SLOT(slotFolderTextChanged(QString)));

    _timer = new QTimer(this);
    _timer->setSingleShot( true );
    connect( _timer, SIGNAL(timeout()), SLOT(slotTimerFires()));
}

void FolderWizardTargetPage::slotFolderTextChanged( const QString& t)
{
  _dirChecked = false;
  emit completeChanged();

  if( t.isEmpty() ) {
    _timer->stop();
    return;
  }

  if( _timer->isActive() ) _timer->stop();
  _timer->start(500);
}

void FolderWizardTargetPage::slotTimerFires()
{
    const QString folder = _ui.OCFolderLineEdit->text();
    qDebug() << "Querying folder " << folder;
    ownCloudInfo::instance()->getWebDAVPath( folder );
}

void FolderWizardTargetPage::slotDirCheckReply(const QString &url, QNetworkReply *reply)
{
    qDebug() << "Got reply from owncloud dir check: " << url << " :" << reply->error();
    _dirChecked = (reply->error() == QNetworkReply::NoError);
    if( _dirChecked ) {
        showWarn();
    } else {
        showWarn( tr("The folder is not available on your %1.<br/>Click to create it." )
                  .arg( Theme::instance()->appNameGUI() ), true );
    }

    emit completeChanged();
}

void FolderWizardTargetPage::slotCreateRemoteFolder()
{
    const QString folder = _ui.OCFolderLineEdit->text();
    if( folder.isEmpty() ) return;

    _ui.OCFolderLineEdit->setEnabled( false );
    qDebug() << "creating folder on ownCloud: " << folder;
    ownCloudInfo::instance()->mkdirRequest( folder );
}

void FolderWizardTargetPage::slotCreateRemoteFolderFinished( QNetworkReply::NetworkError error )
{
  qDebug() << "** webdav mkdir request finished " << error;

  _ui.OCFolderLineEdit->setEnabled( true );
  // the webDAV server seems to return a 202 even if mkdir was successful.
  if( error == QNetworkReply::NoError ||
          error == QNetworkReply::ContentOperationNotPermittedError) {
    showWarn( tr("Folder was successfully created on %1.").arg( Theme::instance()->appNameGUI() ), false );
    slotTimerFires();
  } else {
    showWarn( tr("Failed to create the folder on %1.<br/>Please check manually.").arg( Theme::instance()->appNameGUI() ), false );
  }
}

FolderWizardTargetPage::~FolderWizardTargetPage()
{
}

bool FolderWizardTargetPage::isComplete() const
{
    QString dir = _ui.OCFolderLineEdit->text();
    if( dir.isEmpty() || dir == QLatin1String("/") ) {
        showWarn( tr("If you sync the root folder, you can <b>not</b> configure another sync directory."), false);
        return true;
    } else {
        if( _dirChecked ) {
            showWarn();
        }
        return _dirChecked;
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
        connect( ocInfo, SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                 SLOT(slotOwnCloudFound(QString,QString,QString,QString)));
        connect( ocInfo, SIGNAL(noOwncloudFound(QNetworkReply*)),
                 SLOT(slotNoOwnCloudFound(QNetworkReply*)));
        connect( ocInfo, SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
                 SLOT(slotDirCheckReply(QString,QNetworkReply*)));
        connect( ocInfo, SIGNAL(webdavColCreated(QNetworkReply::NetworkError)),
                 SLOT(slotCreateRemoteFolderFinished( QNetworkReply::NetworkError )));

        connect(_ui._buttCreateFolder, SIGNAL(clicked()), SLOT(slotCreateRemoteFolder()));
        ocInfo->checkInstallation();

        _ui.OCFolderLineEdit->setEnabled( false );

        QString dir = _ui.OCFolderLineEdit->text();
        if( !dir.isEmpty() ) {
            slotFolderTextChanged( dir );
        }
    }
}
void FolderWizardTargetPage::slotOwnCloudFound( const QString& url, const QString& infoStr, const QString& version, const QString& edition)
{
    Q_UNUSED(version);
    Q_UNUSED(edition);

    if( infoStr.isEmpty() ) {
    } else {
//        _ui.OCLabel->setText( tr("to your <a href=\"%1\">%2</a> (version %3)").arg(url)
//                              .arg(Theme::instance()->appNameGUI()).arg(infoStr));
        _ui.OCFolderLineEdit->setEnabled( true );
        qDebug() << "ownCloud found on " << url << " with version: " << infoStr;
    }
}

void FolderWizardTargetPage::slotNoOwnCloudFound( QNetworkReply* error )
{
  qDebug() << "No ownCloud configured: " << error->error();
//  _ui.OCLabel->setText( tr("no configured %1 found!").arg(Theme::instance()->appNameGUI()) );
  showWarn( tr("%1 could not be reached:<br/><tt>%2</tt>")
            .arg(Theme::instance()->appNameGUI()).arg(error->errorString()));
  _ui.OCFolderLineEdit->setEnabled( false );
}

void FolderWizardTargetPage::showWarn( const QString& msg, bool showCreateButton ) const
{
    _ui._buttCreateFolder->setVisible( showCreateButton && !msg.isEmpty() );

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

