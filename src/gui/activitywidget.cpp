/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif

#include "activitywidget.h"
#include "configfile.h"
#include "syncresult.h"
#include "logger.h"
#include "utility.h"
#include "theme.h"
#include "folderman.h"
#include "syncfileitem.h"
#include "folder.h"
#include "openfilemanager.h"
#include "owncloudpropagator.h"

#include "ui_activitywidget.h"

#include <climits>

namespace OCC {

ActivityListModel::ActivityListModel(QWidget *parent)
    :QAbstractListModel(parent)
{

}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::EditRole)
        return QVariant();

    switch (role) {
    case Qt::ToolTipRole:
    case Qt::DisplayRole:
        return tr("%1 (%2)").arg("IM an item");
    case Qt::DecorationRole:
        return QFileIconProvider().icon(QFileIconProvider::Folder);
        break;
    }
    return QVariant();

}

int ActivityListModel::rowCount(const QModelIndex&) const
{
    return 4;
}

/* ==================================================================== */

ActivityWidget::ActivityWidget(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::ActivityWidget)
{
    _ui->setupUi(this);

    // Adjust copyToClipboard() when making changes here!
#if defined(Q_OS_MAC)
    _ui->_activityList->setMinimumWidth(400);
#endif

    _ui->_activityList->setModel(new ActivityListModel(this));

    connect(this, SIGNAL(guiLog(QString,QString)), Logger::instance(), SIGNAL(guiLog(QString,QString)));

    _copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip( tr("Copy the activity list to the clipboard."));
    _copyBtn->setEnabled(false);
    connect(_copyBtn, SIGNAL(clicked()), SLOT(copyToClipboard()));
}

ActivityWidget::~ActivityWidget()
{
    delete _ui;
}

void ActivityWidget::copyToClipboard()
{
    QString text;
    QTextStream ts(&text);
#if 0
    int topLevelItems = _ui->_activityList->topLevelItemCount();
    for (int i = 0; i < topLevelItems; i++) {
        QTreeWidgetItem *child = _ui->_activityList->topLevelItem(i);
        ts << left
                // time stamp
            << qSetFieldWidth(10)
            << child->data(0,Qt::DisplayRole).toString()
                // file name
            << qSetFieldWidth(64)
            << child->data(1,Qt::DisplayRole).toString()
                // folder
            << qSetFieldWidth(30)
            << child->data(2, Qt::DisplayRole).toString()
                // action
            << qSetFieldWidth(15)
            << child->data(3, Qt::DisplayRole).toString()
                // size
            << qSetFieldWidth(10)
            << child->data(4, Qt::DisplayRole).toString()
            << qSetFieldWidth(0)
            << endl;
    }
#endif
    QApplication::clipboard()->setText(text);
    emit guiLog(tr("Copied to clipboard"), tr("The sync status has been copied to the clipboard."));
}

// FIXME: Reused from protocol widget. Move over to utilities.
QString ActivityWidget::timeString(QDateTime dt, QLocale::FormatType format) const
{
    const QLocale loc = QLocale::system();
    QString dtFormat = loc.dateTimeFormat(format);
    static const QRegExp re("(HH|H|hh|h):mm(?!:s)");
    dtFormat.replace(re, "\\1:mm:ss");
    return loc.toString(dt, dtFormat);
}

void ActivityWidget::slotOpenFile( )
{
    // FIXME make work at all.
#if 0
    QString folderName = item->data(2, Qt::UserRole).toString();
    QString fileName = item->text(1);

    Folder *folder = FolderMan::instance()->folder(folderName);
    if (folder) {
        // folder->path() always comes back with trailing path
        QString fullPath = folder->path() + fileName;
        if (QFile(fullPath).exists()) {
            showInFileManager(fullPath);
        }
    }
#endif
}

}
