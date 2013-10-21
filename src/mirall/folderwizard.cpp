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
#include "mirall/folderman.h"
#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QInputDialog>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>
#include <QTreeWidget>

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
    connect(_ui.refreshButton, SIGNAL(clicked()), SLOT(slotRefreshFolders()));
    connect(_ui.folderTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem*,int)), SIGNAL(completeChanged()));
    connect(_ui.folderTreeWidget, SIGNAL(itemActivated(QTreeWidgetItem*,int)), SIGNAL(completeChanged()));
}

void FolderWizardTargetPage::slotAddRemoteFolder()
{
    QTreeWidgetItem *current = _ui.folderTreeWidget->currentItem();

    QString parent('/');
    if (current) {
        parent = current->data(0, Qt::UserRole).toString();
    }

    QInputDialog *dlg = new QInputDialog(this);

    dlg->setWindowTitle(tr("Add Remote Folder"));
    dlg->setLabelText(tr("Enter the name of the new folder:"));
    dlg->setTextValue(parent);
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

static QTreeWidgetItem* findFirstChild(QTreeWidgetItem *parent, const QString& text)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->text(0) == text) {
            return child;
        }
    }
    return 0;
}

static void recursiveInsert(QTreeWidgetItem *parent, QStringList pathTrail, QString path)
{
    QFileIconProvider prov;
    QIcon folderIcon = prov.icon(QFileIconProvider::Folder);
    if (pathTrail.size() == 0) {
        if (path.endsWith('/')) {
            path.chop(1);
        }
        parent->setToolTip(0, path);
        parent->setData(0, Qt::UserRole, path);
    } else {
        QTreeWidgetItem *item = findFirstChild(parent, pathTrail.first());
        if (!item) {
            item = new QTreeWidgetItem(parent);
            item->setIcon(0, folderIcon);
            item->setText(0, pathTrail.first());
            item->setData(0, Qt::UserRole, pathTrail.first());
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        pathTrail.removeFirst();
        recursiveInsert(item, pathTrail, path);
    }
}

void FolderWizardTargetPage::slotUpdateDirectories(QStringList list)
{
    QString webdavFolder = QUrl(ownCloudInfo::instance()->webdavUrl()).path();

    QTreeWidgetItem *root = _ui.folderTreeWidget->topLevelItem(0);
    if (!root) {
        root = new QTreeWidgetItem(_ui.folderTreeWidget);
        root->setText(0, Theme::instance()->appNameGUI());
        root->setIcon(0, Theme::instance()->applicationIcon());
        root->setToolTip(0, tr("Choose this to sync the entire account"));
        root->setData(0, Qt::UserRole, "/");
    }
    foreach (QString path, list) {
        path.remove(webdavFolder);
        QStringList paths = path.split('/');
        if (paths.last().isEmpty()) paths.removeLast();
        recursiveInsert(root, paths, path);
    }
    root->setExpanded(true);
}

void FolderWizardTargetPage::slotRefreshFolders()
{
    ownCloudInfo::instance()->getDirectoryListing("/");
    _ui.folderTreeWidget->clear();
}

void FolderWizardTargetPage::slotItemExpanded(QTreeWidgetItem *item)
{
    ownCloudInfo::instance()->getDirectoryListing(item->data(0, Qt::UserRole).toString());
}

FolderWizardTargetPage::~FolderWizardTargetPage()
{
}

bool FolderWizardTargetPage::isComplete() const
{
    if (!_ui.folderTreeWidget->currentItem())
        return false;

    QString dir = _ui.folderTreeWidget->currentItem()->data(0, Qt::UserRole).toString();
    wizard()->setProperty("targetPath", dir);

    Folder::Map map = _folderMap;
    Folder::Map::const_iterator i = map.constBegin();
    for(i = map.constBegin();i != map.constEnd(); i++ ) {
        Folder *f = static_cast<Folder*>(i.value());
        QString curDir = f->remotePath();
        if (dir == curDir) {
            showWarn( tr("This directory is already being synced.") );
            return false;
        } else if (dir.startsWith(curDir)) {
            if (dir.isEmpty()) dir = QLatin1Char('/');
            if (curDir.isEmpty()) curDir = QLatin1Char('/');
            showWarn( tr("You are already syncing <i>%1</i>, which is a parent folder of <i>%2</i>.").arg(curDir).arg(dir) );
            return false;
        }
    }

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
        connect(_ui.folderTreeWidget, SIGNAL(itemExpanded(QTreeWidgetItem*)),
                SLOT(slotItemExpanded(QTreeWidgetItem*)));

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

    setWindowTitle( tr("Add Folder") );
    setOptions(QWizard::CancelButtonOnLeft);
    setButtonText(QWizard::FinishButton, tr("Add Folder"));
}

FolderWizard::~FolderWizard()
{
}

void FolderWizard::setFolderMap( const Folder::Map& fm)
{
    _folderWizardSourcePage->setFolderMap( fm );
    _folderWizardTargetPage->setFolderMap( fm );
}

} // end namespace

