/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
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

#include "folderstatusdelegate.h"
#include "folderstatusmodel.h"
#include "folderman.h"
#include "accountstate.h"
#include "utility.h"
#include <theme.h>
#include <account.h>

#include <QFileIconProvider>
#include <QPainter>
#include <QApplication>

namespace OCC {

QString FolderStatusDelegate::addFolderText()
{
    return tr("Add Folder to Synchronize");
}

//alocate each item size in listview.
QSize FolderStatusDelegate::sizeHint(const QStyleOptionViewItem & option ,
                                   const QModelIndex & index) const
{
    auto classif = static_cast<const FolderStatusModel *>(index.model())->classify(index);
    if (classif == FolderStatusModel::AddButton) {
        QFontMetrics fm(option.font);
        QStyleOptionButton opt;
        static_cast<QStyleOption&>(opt) = option;
        opt.text = addFolderText();
        return QApplication::style()->sizeFromContents(
                QStyle::CT_PushButton, &opt, fm.size(Qt::TextSingleLine, opt.text)).
            expandedTo(QApplication::globalStrut());
    }

    if (classif != FolderStatusModel::RootFolder) {
        return QStyledItemDelegate::sizeHint(option, index);
    }

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
    if( ! qvariant_cast<QStringList>(index.data(FolderErrorMsg)).isEmpty() ) {
        QStringList errMsgs = qvariant_cast<QStringList>(index.data(FolderErrorMsg));
        h += aliasMargin*2 + errMsgs.count()*fm.height();
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

    if (qvariant_cast<bool>(index.data(AddButton))) {
        QSize hint = sizeHint(option, index);
        QStyleOptionButton opt;
        static_cast<QStyleOption&>(opt) = option;
        // only keep the flags interesting for the button:
        opt.state = QStyle::State_Enabled;
        opt.text = addFolderText();
        opt.rect.setWidth(qMin(opt.rect.width(), hint.width()));
        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt, painter
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
                , option.widget
#endif
            );
        return;
    }

    if (static_cast<const FolderStatusModel *>(index.model())->classify(index) != FolderStatusModel::RootFolder) {
        return;
    }
    painter->save();

    QFont aliasFont = option.font;
    QFont subFont   = option.font;
    QFont errorFont = subFont;
    QFont progressFont = subFont;

    progressFont.setPointSize( subFont.pointSize()-2);
    //font.setPixelSize(font.weight()+);
    aliasFont.setBold(true);
    aliasFont.setPointSize( subFont.pointSize()+2 );

    QFontMetrics subFm( subFont );
    QFontMetrics aliasFm( aliasFont );
    QFontMetrics progressFm( progressFont );

    int aliasMargin = aliasFm.height()/2;
    int margin = subFm.height()/4;

    QIcon statusIcon      = qvariant_cast<QIcon>(index.data(FolderStatusIconRole));
    QString aliasText     = qvariant_cast<QString>(index.data(FolderAliasRole));
    QString pathText      = qvariant_cast<QString>(index.data(FolderPathRole));
    QString remotePath    = qvariant_cast<QString>(index.data(FolderSecondPathRole));
    QStringList errorTexts= qvariant_cast<QStringList>(index.data(FolderErrorMsg));

    int overallPercent    = qvariant_cast<int>(index.data(SyncProgressOverallPercent));
    QString overallString = qvariant_cast<QString>(index.data(SyncProgressOverallString));
    QString itemString    = qvariant_cast<QString>(index.data(SyncProgressItemString));
    int warningCount      = qvariant_cast<int>(index.data(WarningCount));
    bool syncOngoing      = qvariant_cast<bool>(index.data(SyncRunning));

    // QString statusText = qvariant_cast<QString>(index.data(FolderStatus));
    bool syncEnabled = index.data(FolderAccountConnected).toBool();
    // QString syncStatus = syncEnabled? tr( "Enabled" ) : tr( "Disabled" );

    QRect iconRect = option.rect;
    QRect aliasRect = option.rect;

    iconRect.setLeft( option.rect.left() + aliasMargin );
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

    // only show the warning icon if the sync is running. Otherwise its
    // encoded in the status icon.
    if( warningCount > 0 && syncOngoing) {
        QRect warnRect;
        warnRect.setLeft(iconRect.left());
        warnRect.setTop(iconRect.bottom()-17);
        warnRect.setWidth(16);
        warnRect.setHeight(16);

        QIcon warnIcon(":/client/resources/warning");
        QPixmap pm = warnIcon.pixmap(16,16, syncEnabled ? QIcon::Normal : QIcon::Disabled );
        painter->drawPixmap(QPoint(warnRect.left(), warnRect.top()),pm );
    }

    auto palette = option.palette;

    if (qApp->style()->inherits("QWindowsVistaStyle")) {
        // Hack: Windows Vista's light blue is not contrasting enough for white

        // (code from QWindowsVistaStyle::drawControl for CE_ItemViewItem)
        palette.setColor(QPalette::All, QPalette::HighlightedText, palette.color(QPalette::Active, QPalette::Text));
        palette.setColor(QPalette::All, QPalette::Highlight, palette.base().color().darker(108));
    }


    QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                            ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
        cg = QPalette::Inactive;

    if (option.state & QStyle::State_Selected) {
        painter->setPen(palette.color(cg, QPalette::HighlightedText));
    } else {
        painter->setPen(palette.color(cg, QPalette::Text));
    }
    QString elidedAlias = aliasFm.elidedText(aliasText, Qt::ElideRight, aliasRect.width());
    painter->setFont(aliasFont);
    painter->drawText(aliasRect, elidedAlias);

    painter->setFont(subFont);
    QString elidedRemotePathText;

    if (remotePath.isEmpty() || remotePath == QLatin1String("/")) {
        elidedRemotePathText = subFm.elidedText(tr("Syncing all files in your account with"),
                                                Qt::ElideRight, remotePathRect.width());
    } else {
        elidedRemotePathText = subFm.elidedText(tr("Remote path: %1").arg(remotePath),
                                                Qt::ElideMiddle, remotePathRect.width());
    }
    painter->drawText(remotePathRect, elidedRemotePathText);

    QString elidedPathText = subFm.elidedText(pathText, Qt::ElideMiddle, localPathRect.width());
    painter->drawText(localPathRect, elidedPathText);

    // paint an error overlay if there is an error string

    int h = iconRect.bottom();
    if( !errorTexts.isEmpty() ) {
        h += aliasMargin;
        QRect errorRect = localPathRect;
        errorRect.setLeft( iconRect.left());
        errorRect.setTop( h );
        errorRect.setHeight(errorTexts.count() * subFm.height()+aliasMargin);
        errorRect.setRight( option.rect.right()-aliasMargin );

        painter->setBrush( QColor(0xbb, 0x4d, 0x4d) );
        painter->setPen( QColor(0xaa, 0xaa, 0xaa));
        painter->drawRoundedRect( errorRect, 4, 4 );

        painter->setPen( Qt::white );
        painter->setFont(errorFont);
        QRect errorTextRect = errorRect;
        errorTextRect.setLeft( errorTextRect.left()+aliasMargin );
        errorTextRect.setTop( errorTextRect.top()+aliasMargin/2 );

        int x = errorTextRect.left();
        int y = errorTextRect.top()+aliasMargin/2 + subFm.height()/2;

        foreach( QString eText, errorTexts ) {
            painter->drawText(x, y, subFm.elidedText( eText, Qt::ElideLeft, errorTextRect.width()-2*aliasMargin));
            y += subFm.height();
        }

        h = errorRect.bottom();
    }
    h += aliasMargin;

    // Sync File Progress Bar: Show it if syncFile is not empty.
    if( !overallString.isEmpty() || !itemString.isEmpty()) {
        int fileNameTextHeight = subFm.boundingRect(tr("File")).height();
        int barHeight = qMax(fileNameTextHeight, aliasFm.height()+4); ;
        int overallWidth = option.rect.width()-2*aliasMargin;

        painter->save();

        // Sizes-Text
        QRect octetRect = progressFm.boundingRect(QRect(), 0, overallString );
        int progressTextWidth = octetRect.width() + 2;

        // Overall Progress Bar.
        QRect pBRect;
        pBRect.setTop( h );
        pBRect.setLeft( iconRect.left());
        pBRect.setHeight(barHeight);
        pBRect.setWidth( overallWidth - progressTextWidth - margin );

        QStyleOptionProgressBarV2 pBarOpt;

        pBarOpt.state    = option.state | QStyle::State_Horizontal;
        pBarOpt.minimum  = 0;
        pBarOpt.maximum  = 100;
        pBarOpt.progress = overallPercent;
        pBarOpt.orientation = Qt::Horizontal;
        pBarOpt.palette = palette;
        pBarOpt.rect = pBRect;

        QApplication::style()->drawControl( QStyle::CE_ProgressBar, &pBarOpt, painter );

        // Overall Progress Text
        QRect overallProgressRect;
        overallProgressRect.setTop( pBRect.top() );
        overallProgressRect.setHeight( pBRect.height() );
        overallProgressRect.setLeft( pBRect.right()+margin);
        overallProgressRect.setWidth( progressTextWidth );
        painter->setFont(progressFont);

        painter->drawText( overallProgressRect, Qt::AlignRight+Qt::AlignVCenter, overallString);
        // painter->drawRect(overallProgressRect);

        // Individual File Progress
        QRect fileRect;
        fileRect.setTop( pBRect.bottom() + margin);
        fileRect.setLeft( iconRect.left());
        fileRect.setWidth(overallWidth);
        fileRect.setHeight(fileNameTextHeight);
        QString elidedText = progressFm.elidedText(itemString, Qt::ElideLeft, fileRect.width());

        painter->drawText( fileRect, Qt::AlignLeft+Qt::AlignVCenter, elidedText);

        painter->restore();
    }

    painter->restore();
}

bool FolderStatusDelegate::editorEvent ( QEvent * event, QAbstractItemModel * model,
                                         const QStyleOptionViewItem & option, const QModelIndex & index )
{
    return QStyledItemDelegate::editorEvent(event, model, option, index);
    return false;
}

} // namespace OCC
