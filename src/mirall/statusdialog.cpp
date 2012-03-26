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
  int h = 70; // height 64 + 4px margin top and down
  int w = 0;

  QString p = qvariant_cast<QString>(index.data(FolderPathRole));
  QFont font = QApplication::font();
  QFontMetrics fm(font);
  w = 8 + fm.boundingRect( p ).width();

  return QSize( w, h );
}

void FolderViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
  QStyledItemDelegate::paint(painter,option,index);

  painter->save();

  QFont font    = QApplication::font();
  QFont subFont = QApplication::font();
  //font.setPixelSize(font.weight()+);
  font.setBold(true);
  subFont.setWeight(subFont.weight() - 4);
  QFontMetrics fm(font);

  QIcon icon = qvariant_cast<QIcon>(index.data(FolderIconRole));
  QIcon statusIcon = qvariant_cast<QIcon>(index.data(FolderStatusIcon));
  QString aliasText = qvariant_cast<QString>(index.data(FolderAliasRole));
  QString pathText = qvariant_cast<QString>(index.data(FolderPathRole));
  QString statusText = qvariant_cast<QString>(index.data(FolderStatus));
  bool syncEnabled = index.data(FolderSyncEnabled).toBool();
  QString syncStatus = syncEnabled? tr( "Enabled" ) : tr( "Disabled" );

  QSize iconsize(48,48); //  = icon.actualSize(option.decorationSize);

  QRect headerRect = option.rect;
  QRect iconRect = option.rect;

  iconRect.setRight(iconsize.width()+30);
  iconRect.setTop(iconRect.top()+5);
  headerRect.setLeft(iconRect.right());
  headerRect.setTop(headerRect.top()+5);
  headerRect.setBottom(headerRect.top()+fm.height());

  QRect subheaderRect = headerRect;
  subheaderRect.setTop(headerRect.bottom());
  QFontMetrics fmSub( subFont );
  subheaderRect.setBottom(subheaderRect.top()+fmSub.height());
  QRect statusRect = subheaderRect;
  statusRect.setTop( subheaderRect.bottom() + 5);
  statusRect.setBottom( statusRect.top() + fmSub.height());

  QRect lastSyncRect = statusRect;
  lastSyncRect.setTop( statusRect.bottom());
  lastSyncRect.setBottom( lastSyncRect.top() + fmSub.height());

  //painter->drawPixmap(QPoint(iconRect.right()/2,iconRect.top()/2),icon.pixmap(iconsize.width(),iconsize.height()));
  painter->drawPixmap(QPoint(iconRect.left()+15,iconRect.top()),icon.pixmap(iconsize.width(),iconsize.height()));

  painter->drawPixmap(QPoint(option.rect.right() - 4 - 48, option.rect.top() + 8 ), statusIcon.pixmap( 48,48));

  painter->setFont(font);
  painter->drawText(headerRect, aliasText);

  painter->setFont(subFont);
  painter->drawText(subheaderRect.left(),subheaderRect.top()+17, pathText);
  painter->drawText(lastSyncRect, tr("Last Sync: %1").arg( statusText ));
  painter->drawText(statusRect, tr("Sync Status: %1").arg( syncStatus ));
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

  connect( _folderList,SIGNAL(doubleClicked(QModelIndex)),SLOT(slotDoubleClicked(QModelIndex)));

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

  connect(_folderList, SIGNAL(activated(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
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
      _ButtonEnable->setText( tr( "disable" ) );
    } else {
      _ButtonEnable->setText( tr( "enable" ) );
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
    }
}

void StatusDialog::folderToModelItem( QStandardItem *item, Folder *f )
{
    QIcon icon = _theme->folderIcon( f->backend(), 48 );
    item->setData( icon,        FolderViewDelegate::FolderIconRole );
    item->setData( f->path(),   FolderViewDelegate::FolderPathRole );
    item->setData( f->alias(),  FolderViewDelegate::FolderAliasRole );
    item->setData( f->syncEnabled(), FolderViewDelegate::FolderSyncEnabled );
    qDebug() << "***** Folder is SyncEnabled: " << f->syncEnabled();

    SyncResult res = f->syncResult();
    SyncResult::Status status = res.status();

    QString errors = res.errorStrings().join("<br/>");

    item->setData( _theme->syncStateIcon( status, 64 ), FolderViewDelegate::FolderStatusIcon );
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
            _model->removeRow( selected.row() );
        }
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
    _ownCloudInfo = new ownCloudInfo();
    _ownCloudInfo->checkInstallation();
    connect(_ownCloudInfo, SIGNAL(ownCloudInfoFound( const QString&,  const QString& )),
            SLOT(slotOCInfo( const QString&, const QString& )));
    connect(_ownCloudInfo, SIGNAL(noOwncloudFound(QNetworkReply::NetworkError)),
            SLOT(slotOCInfoFail()));
}

void StatusDialog::slotOCInfo( const QString& url, const QString& version )
{
    _OCUrl = url;
    /* enable the open button */
    _ocUrlLabel->setOpenExternalLinks(true);
    _ocUrlLabel->setText( tr("Connected to <a href=\"%1\">%2</a>, ownCloud %3").arg(url).arg(url).arg(version) );
    _ButtonAdd->setEnabled(true);
    _ownCloudInfo->deleteLater();
}

void StatusDialog::slotOCInfoFail()
{
    _ocUrlLabel->setText( tr("Failed to connect to ownCloud. Please check configuration!") );
    _ButtonAdd->setEnabled( false);
    _ownCloudInfo->deleteLater();
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
    slotCheckConnection();
    QDialog::showEvent( event );
}

}

