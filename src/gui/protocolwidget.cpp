/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QtGui>
#include <QtWidgets>

#include "protocolwidget.h"
#include "configfile.h"
#include "syncresult.h"
#include "logger.h"
#include "theme.h"
#include "folderman.h"
#include "syncfileitem.h"
#include "folder.h"
#include "openfilemanager.h"
#include "activityitemdelegate.h"

#include "ui_protocolwidget.h"

#include <climits>

namespace OCC {

ProtocolWidget::ProtocolWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ProtocolWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), SIGNAL(itemCompleted(QString, SyncFileItemPtr)),
        this, SLOT(slotItemCompleted(QString, SyncFileItemPtr)));

    connect(_ui->_treeWidget, SIGNAL(itemActivated(QTreeWidgetItem *, int)), SLOT(slotOpenFile(QTreeWidgetItem *, int)));

    // Adjust copyToClipboard() when making changes here!
    QStringList header;
    header << tr("Time");
    header << tr("File");
    header << tr("Folder");
    header << tr("Action");
    header << tr("Size");

    int timestampColumnExtra = 0;
#ifdef Q_OS_WIN
    timestampColumnExtra = 20; // font metrics are broken on Windows, see #4721
#endif

    _ui->_treeWidget->setHeaderLabels(header);
    int timestampColumnWidth =
        _ui->_treeWidget->fontMetrics().width(timeString(QDateTime::currentDateTime()))
        + timestampColumnExtra;
    _ui->_treeWidget->setColumnWidth(0, timestampColumnWidth);
    _ui->_treeWidget->setColumnWidth(1, 180);
    _ui->_treeWidget->setColumnCount(5);
    _ui->_treeWidget->setRootIsDecorated(false);
    _ui->_treeWidget->setTextElideMode(Qt::ElideMiddle);
    _ui->_treeWidget->header()->setObjectName("ActivityListHeader");
#if defined(Q_OS_MAC)
    _ui->_treeWidget->setMinimumWidth(400);
#endif
    _ui->_headerLabel->setText(tr("Local sync protocol"));

    QPushButton *copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    copyBtn->setToolTip(tr("Copy the activity list to the clipboard."));
    copyBtn->setEnabled(true);
    connect(copyBtn, SIGNAL(clicked()), SIGNAL(copyToClipboard()));
}

ProtocolWidget::~ProtocolWidget()
{
    delete _ui;
}

void ProtocolWidget::showEvent(QShowEvent *ev)
{
    ConfigFile cfg;
    cfg.restoreGeometryHeader(_ui->_treeWidget->header());
    QWidget::showEvent(ev);
}

void ProtocolWidget::hideEvent(QHideEvent *ev)
{
    ConfigFile cfg;
    cfg.saveGeometryHeader(_ui->_treeWidget->header());
    QWidget::hideEvent(ev);
}


QString ProtocolWidget::timeString(QDateTime dt, QLocale::FormatType format)
{
    const QLocale loc = QLocale::system();
    QString dtFormat = loc.dateTimeFormat(format);
    static const QRegExp re("(HH|H|hh|h):mm(?!:s)");
    dtFormat.replace(re, "\\1:mm:ss");
    return loc.toString(dt, dtFormat);
}

void ProtocolWidget::slotOpenFile(QTreeWidgetItem *item, int)
{
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
}

QTreeWidgetItem *ProtocolWidget::createCompletedTreewidgetItem(const QString &folder, const SyncFileItem &item)
{
    auto f = FolderMan::instance()->folder(folder);
    if (!f) {
        return 0;
    }

    QStringList columns;
    QDateTime timestamp = QDateTime::currentDateTime();
    const QString timeStr = timeString(timestamp);
    const QString longTimeStr = timeString(timestamp, QLocale::LongFormat);

    columns << timeStr;
    columns << Utility::fileNameForGuiUse(item._originalFile);
    columns << f->shortGuiLocalPath();

    // If the error string is set, it's prefered because it is a useful user message.
    QString message = item._errorString;
    if (message.isEmpty()) {
        message = Progress::asResultString(item);
    }
    columns << message;

    QIcon icon;
    if (item._status == SyncFileItem::NormalError
        || item._status == SyncFileItem::FatalError
        || item._status == SyncFileItem::BlacklistedError) {
        icon = Theme::instance()->syncStateIcon(SyncResult::Error);
    } else if (Progress::isWarningKind(item._status)) {
        icon = Theme::instance()->syncStateIcon(SyncResult::Problem);
    }

    if (ProgressInfo::isSizeDependent(item)) {
        columns << Utility::octetsToString(item._size);
    }

    QTreeWidgetItem *twitem = new QTreeWidgetItem(columns);
    twitem->setData(0, Qt::SizeHintRole, QSize(0, ActivityItemDelegate::rowHeight()));
    twitem->setIcon(0, icon);
    twitem->setToolTip(0, longTimeStr);
    twitem->setToolTip(1, item._file);
    twitem->setToolTip(3, message);
    twitem->setData(0, Qt::UserRole, item._status);
    twitem->setData(2, Qt::UserRole, folder);
    return twitem;
}

void ProtocolWidget::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    if (item->hasErrorStatus())
        return;
    QTreeWidgetItem *line = createCompletedTreewidgetItem(folder, *item);
    if (line) {
        // Limit the number of items
        int itemCnt = _ui->_treeWidget->topLevelItemCount();
        while (itemCnt > 2000) {
            delete _ui->_treeWidget->takeTopLevelItem(itemCnt - 1);
            itemCnt--;
        }
        _ui->_treeWidget->insertTopLevelItem(0, line);
    }
}

void ProtocolWidget::storeSyncActivity(QTextStream &ts)
{
    int topLevelItems = _ui->_treeWidget->topLevelItemCount();

    for (int i = 0; i < topLevelItems; i++) {
        QTreeWidgetItem *child = _ui->_treeWidget->topLevelItem(i);
        ts << right
           // time stamp
           << qSetFieldWidth(20)
           << child->data(0, Qt::DisplayRole).toString()
           // separator
           << qSetFieldWidth(0) << ","

           // file name
           << qSetFieldWidth(64)
           << child->data(1, Qt::DisplayRole).toString()
           // separator
           << qSetFieldWidth(0) << ","

           // folder
           << qSetFieldWidth(30)
           << child->data(2, Qt::DisplayRole).toString()
           // separator
           << qSetFieldWidth(0) << ","

           // action
           << qSetFieldWidth(15)
           << child->data(3, Qt::DisplayRole).toString()
           // separator
           << qSetFieldWidth(0) << ","

           // size
           << qSetFieldWidth(10)
           << child->data(4, Qt::DisplayRole).toString()
           << qSetFieldWidth(0)
           << endl;
    }
}

}
