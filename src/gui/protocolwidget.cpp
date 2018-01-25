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
#include "guiutility.h"
#include "accountstate.h"

#include "ui_protocolwidget.h"

#include <climits>

namespace OCC {

QString ProtocolItem::timeString(QDateTime dt, QLocale::FormatType format)
{
    const QLocale loc = QLocale::system();
    QString dtFormat = loc.dateTimeFormat(format);
    static const QRegExp re("(HH|H|hh|h):mm(?!:s)");
    dtFormat.replace(re, "\\1:mm:ss");
    return loc.toString(dt, dtFormat);
}

QString ProtocolItem::folderName(const QTreeWidgetItem *item)
{
    return item->data(2, Qt::UserRole).toString();
}

void ProtocolItem::setFolderName(QTreeWidgetItem *item, const QString &folderName)
{
    item->setData(2, Qt::UserRole, folderName);
}

QString ProtocolItem::filePath(const QTreeWidgetItem *item)
{
    return item->toolTip(1);
}

void ProtocolItem::setFilePath(QTreeWidgetItem *item, const QString &filePath)
{
    item->setToolTip(1, filePath);
}

QDateTime ProtocolItem::timestamp(const QTreeWidgetItem *item)
{
    return item->data(0, Qt::UserRole).toDateTime();
}

void ProtocolItem::setTimestamp(QTreeWidgetItem *item, const QDateTime &timestamp)
{
    item->setData(0, Qt::UserRole, timestamp);
}

SyncFileItem::Status ProtocolItem::status(const QTreeWidgetItem *item)
{
    return static_cast<SyncFileItem::Status>(item->data(3, Qt::UserRole).toInt());
}

void ProtocolItem::setStatus(QTreeWidgetItem *item, SyncFileItem::Status status)
{
    item->setData(3, Qt::UserRole, status);
}

quint64 ProtocolItem::size(const QTreeWidgetItem *item)
{
    return item->data(4, Qt::UserRole).toULongLong();
}

void ProtocolItem::setSize(QTreeWidgetItem *item, quint64 size)
{
    item->setData(4, Qt::UserRole, size);
}

ProtocolItem *ProtocolItem::create(const QString &folder, const SyncFileItem &item)
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
        || item._status == SyncFileItem::DetailError
        || item._status == SyncFileItem::BlacklistedError) {
        icon = Theme::instance()->syncStateIcon(SyncResult::Error);
    } else if (Progress::isWarningKind(item._status)) {
        icon = Theme::instance()->syncStateIcon(SyncResult::Problem);
    }

    if (ProgressInfo::isSizeDependent(item)) {
        columns << Utility::octetsToString(item._size);
    }

    ProtocolItem *twitem = new ProtocolItem(columns);
    // Warning: The data and tooltips on the columns define an implicit
    // interface and can only be changed with care.
    twitem->setData(0, Qt::SizeHintRole, QSize(0, ActivityItemDelegate::rowHeight()));
    twitem->setIcon(0, icon);
    twitem->setToolTip(0, longTimeStr);
    twitem->setToolTip(3, message);
    setTimestamp(twitem, timestamp);
    setFilePath(twitem, item._file); // also sets toolTip(1)
    setFolderName(twitem, folder);
    setStatus(twitem, item._status);
    setSize(twitem, item._size);
    return twitem;
}

SyncJournalFileRecord ProtocolItem::syncJournalRecord(QTreeWidgetItem *item)
{
    SyncJournalFileRecord rec;
    auto f = folder(item);
    if (!f)
        return rec;
    f->journalDb()->getFileRecord(filePath(item), &rec);
    return rec;
}

Folder *ProtocolItem::folder(QTreeWidgetItem *item)
{
    return FolderMan::instance()->folder(folderName(item));
}

void ProtocolItem::openContextMenu(QPoint globalPos, QTreeWidgetItem *item, QWidget *parent)
{
    auto f = folder(item);
    if (!f)
        return;
    AccountPtr account = f->accountState()->account();
    auto rec = syncJournalRecord(item);
    // rec might not be valid

    auto menu = new QMenu(parent);

    if (rec.isValid()) {
        // "Open in Browser" action
        auto openInBrowser = menu->addAction(ProtocolWidget::tr("Open in browser"));
        QObject::connect(openInBrowser, &QAction::triggered, parent, [parent, account, rec]() {
            fetchPrivateLinkUrl(account, rec._path, rec.numericFileId(), parent,
                [parent](const QString &url) {
                    Utility::openBrowser(url, parent);
                });
        });
    }

    // More actions will be conditionally added to the context menu here later

    if (menu->actions().isEmpty()) {
        delete menu;
        return;
    }

    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(globalPos);
}

bool ProtocolItem::operator<(const QTreeWidgetItem &other) const
{
    int column = treeWidget()->sortColumn();
    if (column == 0) {
        // Items with empty "File" column are larger than others,
        // otherwise sort by time (this uses lexicographic ordering)
        return std::forward_as_tuple(text(1).isEmpty(), timestamp(this))
            < std::forward_as_tuple(other.text(1).isEmpty(), timestamp(&other));
    } else if (column == 4) {
        return size(this) < size(&other);
    }

    return QTreeWidgetItem::operator<(other);
}

ProtocolWidget::ProtocolWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ProtocolWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), &ProgressDispatcher::itemCompleted,
        this, &ProtocolWidget::slotItemCompleted);

    connect(_ui->_treeWidget, &QTreeWidget::itemActivated, this, &ProtocolWidget::slotOpenFile);

    _ui->_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_ui->_treeWidget, &QTreeWidget::customContextMenuRequested, this, &ProtocolWidget::slotItemContextMenu);

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
        _ui->_treeWidget->fontMetrics().width(ProtocolItem::timeString(QDateTime::currentDateTime()))
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
    connect(copyBtn, &QAbstractButton::clicked, this, &ProtocolWidget::copyToClipboard);
}

ProtocolWidget::~ProtocolWidget()
{
    delete _ui;
}

void ProtocolWidget::showEvent(QShowEvent *ev)
{
    ConfigFile cfg;
    cfg.restoreGeometryHeader(_ui->_treeWidget->header());

    // Sorting by section was newly enabled. But if we restore the header
    // from a state where sorting was disabled, both of these flags will be
    // false and sorting will be impossible!
    _ui->_treeWidget->header()->setSectionsClickable(true);
    _ui->_treeWidget->header()->setSortIndicatorShown(true);

    // Switch back to "by time" ordering
    _ui->_treeWidget->sortByColumn(0, Qt::DescendingOrder);

    QWidget::showEvent(ev);
}

void ProtocolWidget::hideEvent(QHideEvent *ev)
{
    ConfigFile cfg;
    cfg.saveGeometryHeader(_ui->_treeWidget->header());
    QWidget::hideEvent(ev);
}

void ProtocolWidget::slotItemContextMenu(const QPoint &pos)
{
    auto item = _ui->_treeWidget->itemAt(pos);
    if (!item)
        return;
    auto globalPos = _ui->_treeWidget->viewport()->mapToGlobal(pos);
    ProtocolItem::openContextMenu(globalPos, item, this);
}

void ProtocolWidget::slotOpenFile(QTreeWidgetItem *item, int)
{
    QString fileName = item->text(1);
    if (Folder *folder = ProtocolItem::folder(item)) {
        // folder->path() always comes back with trailing path
        QString fullPath = folder->path() + fileName;
        if (QFile(fullPath).exists()) {
            showInFileManager(fullPath);
        }
    }
}

void ProtocolWidget::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    if (!item->showInProtocolTab())
        return;
    QTreeWidgetItem *line = ProtocolItem::create(folder, *item);
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
