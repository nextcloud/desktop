/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#include "mirall/statusdialog.h"
#include "mirall/folder.h"
#include "mirall/theme.h"
#include "mirall/owncloudinfo.h"

namespace Mirall {

FolderStatusModel::FolderStatusModel()
    :QStandardItemModel()
{

}

Qt::ItemFlags FolderStatusModel::flags ( const QModelIndex&  )
{
    return Qt::ItemIsSelectable;
}

QVariant FolderStatusModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::EditRole)
        return QVariant();
    else
        return QStandardItemModel::data(index,role);
}

// ====================================================================================

FolderViewDelegate::FolderViewDelegate()
    :QStyledItemDelegate()
{

}

FolderViewDelegate::~FolderViewDelegate()
{
  // TODO Auto-generated destructor stub
}

//alocate each item size in listview.
QSize FolderViewDelegate::sizeHint(const QStyleOptionViewItem & option ,
                                   const QModelIndex & index) const
{
  int w = 0;

  QString p = qvariant_cast<QString>(index.data(FolderPathRole));
  QFont aliasFont = QApplication::font();
  QFont font = QApplication::font();
  aliasFont.setPointSize( font.pointSize() +2 );

  QFontMetrics fm(font);
  QFontMetrics aliasFm(aliasFont);

  w = 8 + fm.boundingRect( p ).width();

  // calc height
  int h = aliasFm.height()/2;  // margin to top
  h += aliasFm.height();       // alias
  h += fm.height()/2;          // between alias and local path
  h += fm.height();            // local path
  h += fm.height()/2;          // between local and remote path
  h += fm.height();            // remote path
  h += aliasFm.height()/2;     // bottom margin

  int minHeight = 48 + fm.height()/2 + fm.height()/2; // icon + margins

  if( h < minHeight ) h = minHeight;
  return QSize( w, h );
}

void FolderViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
  QStyledItemDelegate::paint(painter,option,index);

  painter->save();

  QFont aliasFont    = QApplication::font();
  QFont subFont = QApplication::font();
  //font.setPixelSize(font.weight()+);
  aliasFont.setBold(true);
  aliasFont.setPointSize( subFont.pointSize()+2 );

  QFontMetrics subFm( subFont );
  QFontMetrics aliasFm( aliasFont );

  QIcon folderIcon = qvariant_cast<QIcon>(index.data(FolderIconRole));
  QIcon statusIcon = qvariant_cast<QIcon>(index.data(FolderStatusIcon));
  QString aliasText = qvariant_cast<QString>(index.data(FolderAliasRole));
  QString pathText = qvariant_cast<QString>(index.data(FolderPathRole));
  QString remotePath = qvariant_cast<QString>(index.data(FolderSecondPathRole));
  // QString statusText = qvariant_cast<QString>(index.data(FolderStatus));
  bool syncEnabled = index.data(FolderSyncEnabled).toBool();
  // QString syncStatus = syncEnabled? tr( "Enabled" ) : tr( "Disabled" );

  QSize iconsize(48, 48); //  = icon.actualSize(option.decorationSize);

  QRect aliasRect = option.rect;
  QRect iconRect = option.rect;

  iconRect.setRight( iconsize.width()+30 );
  iconRect.setTop( iconRect.top() + (iconRect.height()-iconsize.height())/2);
  aliasRect.setLeft(iconRect.right());

  aliasRect.setTop(aliasRect.top() + aliasFm.height()/2 );
  aliasRect.setBottom(aliasRect.top()+subFm.height());

  // local directory box
  QRect localPathRect = aliasRect;
  localPathRect.setTop(aliasRect.bottom() + subFm.height() / 2);
  localPathRect.setBottom(localPathRect.top()+subFm.height());

  // remote directory box
  QRect remotePathRect = localPathRect;
  remotePathRect.setTop( localPathRect.bottom() + subFm.height()/2 );
  remotePathRect.setBottom( remotePathRect.top() + subFm.height());

  //painter->drawPixmap(QPoint(iconRect.right()/2,iconRect.top()/2),icon.pixmap(iconsize.width(),iconsize.height()));
  if( syncEnabled ) {
      painter->drawPixmap(QPoint(iconRect.left()+15,iconRect.top()), folderIcon.pixmap(iconsize.width(),iconsize.height()));
  } else {
      painter->drawPixmap(QPoint(iconRect.left()+15,iconRect.top()), folderIcon.pixmap(iconsize.width(),iconsize.height(), QIcon::Disabled ));
  }

  painter->drawPixmap(QPoint(option.rect.right() - 4 - 48, option.rect.top() + (option.rect.height()-48)/2 ), statusIcon.pixmap(48,48));

  painter->setFont(aliasFont);
  painter->drawText(aliasRect, aliasText);

  painter->setFont(subFont);
  painter->drawText(localPathRect.left(),localPathRect.top()+17, pathText);
  painter->drawText(remotePathRect, tr("Remote path: %1").arg(remotePath));

  // painter->drawText(lastSyncRect, tr("Last Sync: %1").arg( statusText ));
  // painter->drawText(statusRect, tr("Sync Status: %1").arg( syncStatus ));
  painter->restore();

}


bool FolderViewDelegate::editorEvent ( QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index )
{
    return false;
}

 // ====================================================================================

StatusDialog::StatusDialog( Theme *theme, QWidget *parent) :
    QDialog(parent),
    _theme( theme ),
    _ownCloudInfo(0)
{
  setupUi( this  );
  setWindowTitle( _theme->appName() + QString (" %1" ).arg( _theme->version() ) );

  _model = new FolderStatusModel();
  FolderViewDelegate *delegate = new FolderViewDelegate();

  _folderList->setItemDelegate( delegate );
  _folderList->setModel( _model );
  _folderList->setMinimumWidth( 300 );
  _folderList->setEditTriggers( QAbstractItemView::NoEditTriggers );

  connect(_ButtonClose,  SIGNAL(clicked()), this, SLOT(accept()));
  connect(_ButtonRemove, SIGNAL(clicked()), this, SLOT(slotRemoveFolder()));
#ifdef HAVE_FETCH_AND_PUSH
  connect(_ButtonFetch,  SIGNAL(clicked()), this, SLOT(slotFetchFolder()));
  connect(_ButtonPush,   SIGNAL(clicked()), this, SLOT(slotPushFolder()));
#else
  _ButtonFetch->setVisible( false );
  _ButtonPush->setVisible( false );
#endif
  connect(_ButtonEnable, SIGNAL(clicked()), this, SLOT(slotEnableFolder()));
  connect(_ButtonInfo,   SIGNAL(clicked()), this, SLOT(slotInfoFolder()));
  connect(_ButtonAdd,    SIGNAL(clicked()), this, SLOT(slotAddSync()));

  _ButtonRemove->setEnabled(false);
  _ButtonFetch->setEnabled(false);
  _ButtonPush->setEnabled(false);
  _ButtonEnable->setEnabled(false);
  _ButtonInfo->setEnabled(false);
  _ButtonAdd->setEnabled(true);

  _ownCloudInfo = new ownCloudInfo();

  connect(_ownCloudInfo, SIGNAL(ownCloudInfoFound(const QString&, const QString&, const QString&, const QString&)),
          this, SLOT(slotOCInfo( const QString&, const QString&, const QString&, const QString& )));
  connect(_ownCloudInfo, SIGNAL(noOwncloudFound(QNetworkReply*)),
          this, SLOT(slotOCInfoFail(QNetworkReply*)));

#if defined Q_WS_X11 
  connect(_folderList, SIGNAL(activated(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
  connect( _folderList,SIGNAL(doubleClicked(QModelIndex)),SLOT(slotDoubleClicked(QModelIndex)));
#endif
#if defined Q_WS_WIN || defined Q_WS_MAC
  connect(_folderList, SIGNAL(clicked(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
  connect( _folderList,SIGNAL(doubleClicked(QModelIndex)),SLOT(slotDoubleClicked(QModelIndex)));
#endif

  _ocUrlLabel->setWordWrap( true );
}

StatusDialog::~StatusDialog()
{
    delete _ownCloudInfo;
}

void StatusDialog::slotFolderActivated( const QModelIndex& indx )
{
  bool state = indx.isValid();

  _ButtonRemove->setEnabled( state );
  _ButtonFetch->setEnabled( state );
  _ButtonPush->setEnabled( state );
  _ButtonEnable->setEnabled( state );
  _ButtonInfo->setEnabled( state );

  if ( state ) {
    bool folderEnabled = _model->data( indx, FolderViewDelegate::FolderSyncEnabled).toBool();
    qDebug() << "folder is sync enabled: " << folderEnabled;
    if ( folderEnabled ) {
      _ButtonEnable->setText( tr( "Pause" ) );
    } else {
      _ButtonEnable->setText( tr( "Resume" ) );
    }
  }
}

void StatusDialog::slotDoubleClicked( const QModelIndex& indx )
{
    if( ! indx.isValid() ) return;
    QString alias = _model->data( indx, FolderViewDelegate::FolderAliasRole ).toString();

    emit openFolderAlias( alias );
}

void StatusDialog::setFolderList( Folder::Map folders )
{
    _model->clear();
    foreach( Folder *f, folders ) {
        qDebug() << "Folder: " << f;
        slotAddFolder( f );
    }

}

void StatusDialog::slotAddFolder( Folder *folder )
{
    if( ! folder || folder->alias().isEmpty() ) return;

    QStandardItem *item = new QStandardItem();
    folderToModelItem( item, folder );
    _model->appendRow( item );
}

void StatusDialog::slotUpdateFolderState( Folder *folder )
{
    QStandardItem *item = 0;
    int row = 0;

    if( ! folder ) return;

    item = _model->item( row );

    while( item ) {
        if( item->data( FolderViewDelegate::FolderAliasRole ) == folder->alias() ) {
            // its the item to update!
            break;
        }
        item = _model->item( ++row );
    }
    if( item ) {
        folderToModelItem( item, folder );
    } else {
        // the dialog is not visible.
    }
}

void StatusDialog::folderToModelItem( QStandardItem *item, Folder *f )
{
    if( ! item || !f ) return;

    QIcon icon = _theme->folderIcon( f->backend() );
    item->setData( icon,             FolderViewDelegate::FolderIconRole );
    item->setData( f->path(),        FolderViewDelegate::FolderPathRole );
    item->setData( f->secondPath(),  FolderViewDelegate::FolderSecondPathRole );
    item->setData( f->alias(),       FolderViewDelegate::FolderAliasRole );
    item->setData( f->syncEnabled(), FolderViewDelegate::FolderSyncEnabled );

    SyncResult res = f->syncResult();
    SyncResult::Status status = res.status();

    QString errors = res.errorStrings().join("<br/>");

    item->setData( _theme->statusHeaderText( status ),  Qt::ToolTipRole );
    if( f->syncEnabled() ) {
        item->setData( _theme->syncStateIcon( status, 48 ), FolderViewDelegate::FolderStatusIcon );
    } else {
        item->setData( _theme->folderDisabledIcon( 48 ), FolderViewDelegate::FolderStatusIcon );
    }
    item->setData( _theme->statusHeaderText( status ),  FolderViewDelegate::FolderStatus );
    item->setData( errors,                              FolderViewDelegate::FolderErrorMsg );
}

void StatusDialog::slotRemoveFolder()
{
    QModelIndex selected = _folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderViewDelegate::FolderAliasRole ).toString();
        qDebug() << "Remove Folder alias " << alias;
        if( !alias.isEmpty() ) {
            // remove from file system through folder man
            emit(removeFolderAlias( alias ));
            // _model->removeRow( selected.row() );
        }
    }
}

void StatusDialog::slotRemoveSelectedFolder()
{
    QModelIndex selected = _folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        _model->removeRow( selected.row() );
    }
}

void StatusDialog::slotFetchFolder()
{
  QModelIndex selected = _folderList->selectionModel()->currentIndex();
  if( selected.isValid() ) {
    QString alias = _model->data( selected, FolderViewDelegate::FolderAliasRole ).toString();
    qDebug() << "Fetch Folder alias " << alias;
    if( !alias.isEmpty() ) {
      emit(fetchFolderAlias( alias ));
    }
  }
}

void StatusDialog::slotPushFolder()
{
  QModelIndex selected = _folderList->selectionModel()->currentIndex();
  if( selected.isValid() ) {
    QString alias = _model->data( selected, FolderViewDelegate::FolderAliasRole ).toString();
    qDebug() << "Push Folder alias " << alias;
    if( !alias.isEmpty() ) {
      emit(pushFolderAlias( alias ));
    }
  }
}

void StatusDialog::slotEnableFolder()
{
  QModelIndex selected = _folderList->selectionModel()->currentIndex();
  if( selected.isValid() ) {
    QString alias = _model->data( selected, FolderViewDelegate::FolderAliasRole ).toString();
    bool folderEnabled = _model->data( selected, FolderViewDelegate::FolderSyncEnabled).toBool();
    qDebug() << "Toggle enabled/disabled Folder alias " << alias << " - current state: " << folderEnabled;
    if( !alias.isEmpty() ) {
      emit(enableFolderAlias( alias, !folderEnabled ));

      // set the button text accordingly.
      slotFolderActivated( selected );
    }
  }
}

void StatusDialog::slotInfoFolder()
{
  QModelIndex selected = _folderList->selectionModel()->currentIndex();
  if( selected.isValid() ) {
    QString alias = _model->data( selected, FolderViewDelegate::FolderAliasRole ).toString();
    qDebug() << "Info Folder alias " << alias;
    if( !alias.isEmpty() ) {
      emit(infoFolderAlias( alias ));
    }
  }
}

void StatusDialog::slotAddSync()
{
    qDebug() << "Add a sync requested.";
    emit addASync();
}

void StatusDialog::slotCheckConnection()
{
    if( _ownCloudInfo->isConfigured() ) {
        _ocUrlLabel->setText( tr("Checking ownCloud connection..."));
        qDebug() << "Check status.php from statusdialog.";
        _ownCloudInfo->checkInstallation();
    } else {
        // ownCloud is not yet configured.
        _ocUrlLabel->setText( tr("No ownCloud connection configured."));
        _ButtonAdd->setEnabled( false);
    }
}

void StatusDialog::slotOCInfo( const QString& url, const QString& versionStr, const QString& version, const QString& )
{
    _OCUrl = url;
    qDebug() << "#-------# oC found on " << url;
    /* enable the open button */
    _ocUrlLabel->setOpenExternalLinks(true);
    _ocUrlLabel->setText( tr("Connected to <a href=\"%1\">%2</a>, ownCloud %3").arg(url).arg(url).arg(versionStr) );
    _ocUrlLabel->setToolTip( tr("Version: %1").arg(version));
    _ButtonAdd->setEnabled(true);

}

void StatusDialog::slotOCInfoFail( QNetworkReply *reply)
{
    QString errStr = tr("unknown problem.");
    if( reply ) errStr = reply->errorString();

    _ocUrlLabel->setText( tr("<p>Failed to connect to ownCloud: <tt>%1</tt></p>").arg(errStr) );
    _ButtonAdd->setEnabled( false);
}

void StatusDialog::slotOpenOC()
{
  if( _OCUrl.isValid() )
    QDesktopServices::openUrl( _OCUrl );
}

/*
  * in the show event, start a connection check to the ownCloud.
  */
void StatusDialog::showEvent ( QShowEvent *event  )
{
    QTimer::singleShot(0, this, SLOT(slotCheckConnection()));
    QDialog::showEvent( event );
}

}

