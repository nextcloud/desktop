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

#include "mirall/folderstatusmodel.h"

#include <QtCore>
#include <QtGui>

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

FolderStatusDelegate::FolderStatusDelegate()
    :QStyledItemDelegate()
{

}

FolderStatusDelegate::~FolderStatusDelegate()
{
  // TODO Auto-generated destructor stub
}

//alocate each item size in listview.
QSize FolderStatusDelegate::sizeHint(const QStyleOptionViewItem & option ,
                                   const QModelIndex & index) const
{
  Q_UNUSED(option)
  QFont aliasFont = option.font;
  QFont font = option.font;
  aliasFont.setPointSize( font.pointSize() +2 );

  QFontMetrics fm(font);
  QFontMetrics aliasFm(aliasFont);

  int aliasMargin = aliasFm.height()/2;
  int margin = fm.height()/4;

  // calc height

  int h = aliasMargin;         // margin to top
  h += aliasFm.height();       // alias
  h += margin;                 // between alias and local path
  h += fm.height();            // local path
  h += margin;                 // between local and remote path
  h += fm.height();            // remote path
  h += aliasMargin;            // bottom margin

  // add some space to show an error condition.
  if( ! qvariant_cast<QString>(index.data(FolderErrorMsg)).isEmpty() ) {
      h += aliasMargin*2+fm.height();
  }

  if( qvariant_cast<bool>(index.data(AddProgressSpace)) ) {
      int margin = fm.height()/4;
      h += (5 * margin); // All the margins
      h += 2* fm.boundingRect(tr("File")).height();
  }

  return QSize( 0, h);
}

void FolderStatusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
  QStyledItemDelegate::paint(painter,option,index);

  painter->save();

  QFont aliasFont = option.font;
  QFont subFont   = option.font;
  QFont errorFont = subFont;
  QFont progressFont = subFont;

  progressFont.setPointSize( subFont.pointSize()-1);
  //font.setPixelSize(font.weight()+);
  aliasFont.setBold(true);
  aliasFont.setPointSize( subFont.pointSize()+2 );

  QFontMetrics subFm( subFont );
  QFontMetrics aliasFm( aliasFont );

  int aliasMargin = aliasFm.height()/2;
  int margin = subFm.height()/4;

  QIcon statusIcon = qvariant_cast<QIcon>(index.data(FolderStatusIconRole));
  QString aliasText = qvariant_cast<QString>(index.data(FolderAliasRole));
  QString pathText = qvariant_cast<QString>(index.data(FolderPathRole));
  QString remotePath = qvariant_cast<QString>(index.data(FolderSecondPathRole));
  QString errorText  = qvariant_cast<QString>(index.data(FolderErrorMsg));
  QString syncFile   = qvariant_cast<QString>(index.data(SyncFileName));
  long    progress1  = qvariant_cast<long>(index.data(SyncProgress1));
  long    progress2  = qvariant_cast<long>(index.data(SyncProgress2));

  // QString statusText = qvariant_cast<QString>(index.data(FolderStatus));
  bool syncEnabled = index.data(FolderSyncEnabled).toBool();
  // QString syncStatus = syncEnabled? tr( "Enabled" ) : tr( "Disabled" );

  QRect iconRect = option.rect;
  QRect aliasRect = option.rect;

  iconRect.setLeft( aliasMargin );
  iconRect.setTop( iconRect.top() + aliasMargin ); // (iconRect.height()-iconsize.height())/2);

  // alias box
  aliasRect.setTop(aliasRect.top() + aliasMargin );
  aliasRect.setBottom(aliasRect.top() + aliasFm.height());
  aliasRect.setRight(aliasRect.right() - aliasMargin );

  // remote directory box
  QRect remotePathRect = aliasRect;
  remotePathRect.setTop(aliasRect.bottom() + margin );
  remotePathRect.setBottom(remotePathRect.top() + subFm.height());

  // local directory box
  QRect localPathRect = remotePathRect;
  localPathRect.setTop( remotePathRect.bottom() + margin );
  localPathRect.setBottom( localPathRect.top() + subFm.height());

  iconRect.setBottom(localPathRect.bottom());
  iconRect.setWidth(iconRect.height());

  int nextToIcon = iconRect.right()+aliasMargin;
  aliasRect.setLeft(nextToIcon);
  localPathRect.setLeft(nextToIcon);
  remotePathRect.setLeft(nextToIcon);

  int iconSize = iconRect.width();

  QPixmap pm = statusIcon.pixmap(iconSize, iconSize, syncEnabled ? QIcon::Normal : QIcon::Disabled );
  painter->drawPixmap(QPoint(iconRect.left(), iconRect.top()), pm);

  if ((option.state & QStyle::State_Selected)
          && (option.state & QStyle::State_Active)
          // Hack: Windows Vista's light blue is not contrasting enough for white
          && !qApp->style()->inherits("QWindowsVistaStyle")) {
      painter->setPen(option.palette.color(QPalette::HighlightedText));
  } else {
      painter->setPen(option.palette.color(QPalette::Text));
  }
  QString elidedAlias = aliasFm.elidedText(aliasText, Qt::ElideRight, aliasRect.width());
  painter->setFont(aliasFont);
  painter->drawText(aliasRect, elidedAlias);

  painter->setFont(subFont);
  QString elidedRemotePathText;

  if (remotePath.isEmpty() || remotePath == QLatin1String("/")) {
      elidedRemotePathText = subFm.elidedText(tr("Syncing all files of your account with"),
                                              Qt::ElideRight, remotePathRect.width());
  } else {
      elidedRemotePathText = subFm.elidedText(tr("Remote path: %1").arg(remotePath),
                                              Qt::ElideMiddle, remotePathRect.width());
  }
  painter->drawText(remotePathRect, elidedRemotePathText);

  QString elidedPathText = subFm.elidedText(pathText, Qt::ElideMiddle, localPathRect.width());
  painter->drawText(localPathRect, elidedPathText);

  // paint an error overlay if there is an error string
  if( !errorText.isEmpty() ) {
      QRect errorRect = localPathRect;
      errorRect.setLeft( iconRect.left());
      errorRect.setTop( iconRect.bottom()+subFm.height()/2 );
      errorRect.setHeight(subFm.height()+aliasMargin);
      errorRect.setRight( option.rect.right()-aliasMargin );

      painter->setBrush( QColor(0xbb, 0x4d, 0x4d) );
      painter->setPen( QColor(0xaa, 0xaa, 0xaa));
      painter->drawRoundedRect( errorRect, 4, 4 );

      QIcon warnIcon(":/mirall/resources/warning-16");
      QPoint warnPos(errorRect.left()+aliasMargin/2, errorRect.top()+aliasMargin/2);
      painter->drawPixmap( warnPos, warnIcon.pixmap(QSize(16,16)));

      painter->setPen( Qt::white );
      painter->setFont(errorFont);
      QRect errorTextRect = errorRect;
      errorTextRect.setLeft( errorTextRect.left()+aliasMargin +16);
      errorTextRect.setTop( errorTextRect.top()+aliasMargin/2 );

      int linebreak = errorText.indexOf(QLatin1String("<br"));
      QString eText = errorText;
      if(linebreak) {
          eText = errorText.left(linebreak);
      }
      painter->drawText(errorTextRect, eText);
  }

  /* Display the sync progress. */
  if( !syncFile.isEmpty() ) {
      int fileNameTextHeight = subFm.boundingRect(tr("File")).height();
      int barHeight = fileNameTextHeight;

      QRect progressRect;
      progressRect.setLeft( iconRect.left());
      progressRect.setTop( localPathRect.bottom()+margin );
      progressRect.setHeight( 3 * margin + fileNameTextHeight + barHeight );
      progressRect.setRight( option.rect.right()-aliasMargin);

      painter->save();
      painter->setBrush( option.palette.brightText() );
      // painter->setPen( QColor(0x06, 0x46, 0x06));
      painter->drawRoundedRect( progressRect, 2, 2 );
      painter->setFont(progressFont);

      QRect fileNameRect;
      fileNameRect.setTop(progressRect.top() + margin);
      fileNameRect.setLeft(progressRect.left() + margin);
      fileNameRect.setWidth( progressRect.width() - 2*margin);
      fileNameRect.setHeight(fileNameTextHeight);

      QString pText = subFm.elidedText( tr("File %1: ").arg(syncFile), Qt::ElideLeft, fileNameRect.width());
      painter->drawText(fileNameRect, pText);
      painter->restore();

      QRect pBRect;
      pBRect.setTop( fileNameRect.bottom() + margin );
      pBRect.setLeft( fileNameRect.left());
      pBRect.setHeight(barHeight);
      pBRect.setWidth( fileNameRect.width());

      QStyleOptionProgressBarV2 pBarOpt;
      pBarOpt.state    = option.state | QStyle::State_Horizontal;
      pBarOpt.minimum  = 0;
      pBarOpt.maximum  = progress2;
      pBarOpt.progress = progress1;
      pBarOpt.orientation = Qt::Horizontal;
      pBarOpt.palette = option.palette;
      pBarOpt.rect = pBRect;

      QApplication::style()->drawControl( QStyle::CE_ProgressBar, &pBarOpt, painter );
  }
  // painter->drawText(lastSyncRect, tr("Last Sync: %1").arg( statusText ));
  // painter->drawText(statusRect, tr("Sync Status: %1").arg( syncStatus ));
  painter->restore();
}

bool FolderStatusDelegate::editorEvent ( QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index )
{
    return false;
}

} // namespace Mirall
