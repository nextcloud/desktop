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

#include "statusdialog.h"
#include "folder.h"
#include "version.h"

namespace Mirall {
FolderViewDelegate::FolderViewDelegate()
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
  int h = 64; // height 64 + 4px margin top and down
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
  subFont.setWeight(subFont.weight()-2);
  QFontMetrics fm(font);

  QIcon icon = qvariant_cast<QIcon>(index.data(FolderIconRole));
  QIcon statusIcon = qvariant_cast<QIcon>(index.data(FolderStatusIcon));
  QString aliasText = qvariant_cast<QString>(index.data(FolderNameRole));
  QString pathText = qvariant_cast<QString>(index.data(FolderPathRole));
  QString statusText = qvariant_cast<QString>(index.data(FolderStatus));

  QSize iconsize(48,48); //  = icon.actualSize(option.decorationSize);

  QRect headerRect = option.rect;
  QRect subheaderRect = option.rect;
  QRect iconRect = subheaderRect;

  iconRect.setRight(iconsize.width()+30);
  iconRect.setTop(iconRect.top()+5);
  headerRect.setLeft(iconRect.right());
  subheaderRect.setLeft(iconRect.right());
  headerRect.setTop(headerRect.top()+5);
  headerRect.setBottom(headerRect.top()+fm.height());

  subheaderRect.setTop(headerRect.bottom());
  QFontMetrics fmSub( subFont );
  subheaderRect.setBottom(subheaderRect.top()+fmSub.height());
  QRect statusRect = subheaderRect;
  statusRect.setTop( subheaderRect.bottom() + 2 );
  statusRect.setBottom( statusRect.top() + fmSub.height());

  //painter->drawPixmap(QPoint(iconRect.right()/2,iconRect.top()/2),icon.pixmap(iconsize.width(),iconsize.height()));
  painter->drawPixmap(QPoint(iconRect.left()+15,iconRect.top()),icon.pixmap(iconsize.width(),iconsize.height()));

  painter->drawPixmap(QPoint(option.rect.right() - 4 - 48, option.rect.top() + 8 ), statusIcon.pixmap( 48,48));


  painter->setFont(font);
  painter->drawText(headerRect, aliasText);

  painter->setFont(subFont);
  painter->drawText(subheaderRect.left(),subheaderRect.top()+17, pathText);
  painter->drawText(statusRect, tr("Status: %1").arg( statusText ));
  painter->restore();

}

 // ====================================================================================

StatusDialog::StatusDialog(QWidget *parent) :
    QDialog(parent)
{
  setupUi( this  );
  setWindowTitle( tr("Mirall %1").arg( VERSION_STRING ) );

  _model = new QStandardItemModel();
  FolderViewDelegate *delegate = new FolderViewDelegate();

  _folderList->setItemDelegate( delegate );
  _folderList->setModel( _model );
  _folderList->setMinimumWidth( 300 );

  connect(_ButtonClose,  SIGNAL(clicked()), this, SLOT(accept()));
  connect(_ButtonRemove, SIGNAL(clicked()), this, SLOT(slotRemoveFolder()));
  connect(_ButtonFetch,  SIGNAL(clicked()), this, SLOT(slotFetchFolder()));
  connect(_ButtonOpenOC, SIGNAL(clicked()), this, SLOT(slotOpenOC()));

  _ButtonOpenOC->setEnabled(false);
  _ButtonRemove->setEnabled(false);
  _ButtonFetch->setEnabled(false);

  connect(_folderList, SIGNAL(activated(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
}

void StatusDialog::slotFolderActivated( const QModelIndex& indx )
{
  bool state = indx.isValid();

  _ButtonRemove->setEnabled( state );
  _ButtonFetch->setEnabled( state );

}

void StatusDialog::setFolderList( Folder::Map folders )
{
  _model->clear();

  foreach( Folder *f, folders ) {
    qDebug() << "Folder: " << f;
    QStandardItem *item = new QStandardItem();
    QIcon icon = f->icon( 48 );
    item->setData( icon, FolderViewDelegate::FolderIconRole );
    item->setData( f->path(),  FolderViewDelegate::FolderPathRole );
    item->setData( f->alias(),  FolderViewDelegate::FolderNameRole );

    SyncResult res = f->lastSyncResult();
    QString resultStr = tr("Undefined");
    QString statusIcon = "view-refresh";
    qDebug() << "Status: " << res.result();
    if( res.result() == SyncResult::Error ) {
      resultStr = tr("Error");
      statusIcon = "dialog-close";
    } else if( res.result() == SyncResult::Success ) {
      resultStr = tr("Success");
      statusIcon = "dialog-ok";
    } else if( res.result() == SyncResult::Disabled ) {
      resultStr = tr("Disabled");
      statusIcon = "dialog-cancel";
    } else if( res.result() == SyncResult::SetupError ) {
      resultStr = tr( "Setup Error" );
      statusIcon = "dialog-cancel";
    }
    item->setData( QIcon::fromTheme( statusIcon, QIcon( QString( ":/mirall/resources/%1").arg(statusIcon))), FolderViewDelegate::FolderStatusIcon );
    item->setData( resultStr, FolderViewDelegate::FolderStatus );
    item->setData( res.errorString(), FolderViewDelegate::FolderErrorMsg );

    _model->appendRow( item );
  }
}

void StatusDialog::slotRemoveFolder()
{
  QModelIndex selected = _folderList->selectionModel()->currentIndex();
  if( selected.isValid() ) {
    QString alias = _model->data( selected, FolderViewDelegate::FolderNameRole ).toString();
    qDebug() << "Remove Folder alias " << alias;
    if( !alias.isEmpty() ) {
      emit(removeFolderAlias( alias ));
    }
  }
}

void StatusDialog::slotFetchFolder()
{
  QModelIndex selected = _folderList->selectionModel()->currentIndex();
  if( selected.isValid() ) {
    QString alias = _model->data( selected, FolderViewDelegate::FolderNameRole ).toString();
    qDebug() << "Fetch Folder alias " << alias;
    if( !alias.isEmpty() ) {
      emit(fetchFolderAlias( alias ));
    }
  }
}

void StatusDialog::setOCUrl( const QUrl& url )
{
  _OCUrl = url;
  if( url.isValid() )
    _ButtonOpenOC->setEnabled( true );
}

void StatusDialog::slotOpenOC()
{
  if( _OCUrl.isValid() )
    QDesktopServices::openUrl( _OCUrl );
}

}

#include "statusdialog.moc"
