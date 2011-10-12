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

  QFont font = QApplication::font();
  QFont SubFont = QApplication::font();
  //font.setPixelSize(font.weight()+);
  font.setBold(true);
  SubFont.setWeight(SubFont.weight()-2);
  QFontMetrics fm(font);

  QIcon icon = qvariant_cast<QIcon>(index.data(FolderIconRole));
  QString aliasText = qvariant_cast<QString>(index.data(FolderNameRole));
  QString pathText = qvariant_cast<QString>(index.data(FolderPathRole));

  QSize iconsize = icon.actualSize(option.decorationSize);

  QRect headerRect = option.rect;
  QRect subheaderRect = option.rect;
  QRect iconRect = subheaderRect;

  iconRect.setRight(iconsize.width()+30);
  iconRect.setTop(iconRect.top()+5);
  headerRect.setLeft(iconRect.right());
  subheaderRect.setLeft(iconRect.right());
  headerRect.setTop(headerRect.top()+5);
  headerRect.setBottom(headerRect.top()+fm.height());

  subheaderRect.setTop(headerRect.bottom()+2);

  //painter->drawPixmap(QPoint(iconRect.right()/2,iconRect.top()/2),icon.pixmap(iconsize.width(),iconsize.height()));
  painter->drawPixmap(QPoint(iconRect.left()+iconsize.width()/2+2,iconRect.top()+iconsize.height()/2+3),icon.pixmap(iconsize.width(),iconsize.height()));

  painter->setFont(font);
  painter->drawText(headerRect, aliasText);


  painter->setFont(SubFont);
  painter->drawText(subheaderRect.left(),subheaderRect.top()+17, pathText);

  painter->restore();

}

 // ====================================================================================

StatusDialog::StatusDialog(QWidget *parent) :
    QDialog(parent)
{
  setupUi( this  );

  _model = new QStandardItemModel();
  FolderViewDelegate *delegate = new FolderViewDelegate();

  _folderList->setItemDelegate( delegate );
  _folderList->setModel( _model );

  connect(_ButtonClose, SIGNAL(clicked()), this, SLOT(accept()));
  connect(_ButtonRemove, SIGNAL(clicked()), this, SLOT(slotRemoveFolder()));
}

void StatusDialog::setFolderList( Folder::Map folders )
{
  _model->clear();

  foreach( Folder *f, folders ) {
    qDebug() << "Folder: " << f;
    QStandardItem *item = new QStandardItem();
    item->setData( QIcon::fromTheme( "folder-sync" ), FolderViewDelegate::FolderIconRole );
    item->setData( f->path(),  FolderViewDelegate::FolderPathRole );
    item->setData( f->alias(),  FolderViewDelegate::FolderNameRole );
    _model->appendRow( item );
  }
}

void StatusDialog::slotRemoveFolder()
{
    qDebug() << "Remove Folder!";
}

}

#include "statusdialog.moc"
