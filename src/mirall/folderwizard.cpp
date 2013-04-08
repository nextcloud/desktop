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
  :_folderMap(0)
{
    _ui.setupUi(this);
    registerField(QLatin1String("sourceFolder*"), _ui.localFolderLineEdit);
    _ui.localFolderLineEdit->setText( QString::fromLatin1( "%1/%2").arg( QDir::homePath() ).arg(Theme::instance()->appName() ) );
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
  QFileInfo selFile( _ui.localFolderLineEdit->text() );
  QString   userInput = selFile.canonicalFilePath();

  QString warnString;

  bool isOk = selFile.isDir();
  if( !isOk ) {
    warnString = tr("No local directory selected!");
  }
  // check if the local directory isn't used yet in another ownCloud sync
  Folder::Map *map = _folderMap;
  if( ! map ) return false;

  if( isOk ) {
    Folder::Map::const_iterator i = map->constBegin();
    while( isOk && i != map->constEnd() ) {
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
        warnString.append( tr("The local path %1 is already an upload folder.<br/>Please pick another one!").arg(userInput) );
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

  Folder::Map::const_iterator i = map->constBegin();
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

    registerField(QLatin1String("local?"),            _ui.localFolderRadioBtn);
    registerField(QLatin1String("remote?"),           _ui.urlFolderRadioBtn);
    registerField(QLatin1String("OC?"),               _ui.OCRadioBtn);
    registerField(QLatin1String("targetLocalFolder"), _ui.localFolder2LineEdit);
    registerField(QLatin1String("targetURLFolder"),   _ui.urlFolderLineEdit);
    registerField(QLatin1String("targetOCFolder"),    _ui.OCFolderLineEdit);

    connect( _ui.OCFolderLineEdit, SIGNAL(textChanged(QString)),
             SLOT(slotFolderTextChanged(QString)));

    _timer = new QTimer(this);
    _timer->setSingleShot( true );
    connect( _timer, SIGNAL(timeout()), SLOT(slotTimerFires()));

#if QT_Version >= 0x040700
    _ui.OCFolderLineEdit->setPlaceholderText(QApplication::translate("FolderWizardTargetPage", "root", 0, QApplication::UnicodeUTF8));
    _ui.localFolder2LineEdit->setPlaceholderText(QApplication::translate("FolderWizardTargetPage", "/home/local", 0, QApplication::UnicodeUTF8));
    _ui.urlFolderLineEdit->setPlaceholderText(QApplication::translate("FolderWizardTargetPage", "scp://john@host.com//myfolder", 0, QApplication::UnicodeUTF8));
#endif
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
    _ui.OCFolderLineEdit->setEnabled( false );

    const QString folder = _ui.OCFolderLineEdit->text();
    if( folder.isEmpty() ) return;

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
    if (_ui.localFolderRadioBtn->isChecked()) {
        return QFileInfo(_ui.localFolder2LineEdit->text()).isDir();
    } else if (_ui.urlFolderRadioBtn->isChecked()) {
        QUrl url(_ui.urlFolderLineEdit->text());
        return url.isValid() && (url.scheme() == QLatin1String("sftp")
                                 || url.scheme() == QLatin1String("smb"));
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
    showWarn();
}

void FolderWizardTargetPage::initializePage()
{
    slotToggleItems();
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

    } else {
      _ui.OCRadioBtn->setEnabled( false );
      _ui.OCFolderLineEdit->setEnabled( false );
    }

    QString dir = _ui.OCFolderLineEdit->text();
    if( !dir.isEmpty() ) {
        slotFolderTextChanged( dir );
    }
}

void FolderWizardTargetPage::slotOwnCloudFound( const QString& url, const QString& infoStr, const QString& version, const QString& edition)
{
    Q_UNUSED(version);
    Q_UNUSED(edition);

    if( infoStr.isEmpty() ) {
    } else {
        _ui.OCLabel->setText( tr("to your <a href=\"%1\">%2</a> (version %3)").arg(url)
                              .arg(Theme::instance()->appNameGUI()).arg(infoStr));
        _ui.OCFolderLineEdit->setEnabled( true );
        _ui.OCRadioBtn->setEnabled( true );
        qDebug() << "ownCloud found on " << url << " with version: " << infoStr;
    }
}

void FolderWizardTargetPage::slotNoOwnCloudFound( QNetworkReply* error )
{
  qDebug() << "No ownCloud configured: " << error->error();
  _ui.OCLabel->setText( tr("no configured %1 found!").arg(Theme::instance()->appNameGUI()) );
  showWarn( tr("%1 could not be reached:<br/><tt>%2</tt>")
            .arg(Theme::instance()->appNameGUI()).arg(error->errorString()));
  _ui.OCRadioBtn->setEnabled( false );
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
    registerField(QLatin1String("onlyNetwork*"), _ui.checkBoxOnlyOnline);
    registerField(QLatin1String("onlyLocalNetwork*"), _ui.checkBoxOnlyThisLAN );
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
    registerField(QLatin1String("OCUrl*"),       _ui.lineEditOCUrl);
    registerField(QLatin1String("OCUser*"),      _ui.lineEditOCUser );
    registerField(QLatin1String("OCPasswd"),     _ui.lineEditOCPasswd);
    registerField(QLatin1String("OCSiteAlias*"), _ui.lineEditOCAlias);
}

FolderWizardOwncloudPage::~FolderWizardOwncloudPage()
{
}

void FolderWizardOwncloudPage::initializePage()
{
    _ui.lineEditOCAlias->setText( QLatin1String("ownCloud") );
    _ui.lineEditOCUrl->setText( QLatin1String("http://localhost/owncloud") );
    QString user = QString::fromLocal8Bit(qgetenv("USER"));
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

FolderWizard::FolderWizard( QWidget *parent )
    : QWizard(parent),
    _folderWizardSourcePage(0),
    _folderWizardTargetPage(0)
{
    _folderWizardSourcePage = new FolderWizardSourcePage();
    setPage(Page_Source,   _folderWizardSourcePage );
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
  delete _folderWizardSourcePage;
  if( _folderWizardTargetPage )
    delete _folderWizardTargetPage;
}

void FolderWizard::setFolderMap( Folder::Map *fm)
{
  if( _folderWizardSourcePage ) {
    _folderWizardSourcePage->setFolderMap( fm );
  }
}

} // end namespace

