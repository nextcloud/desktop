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

#include "progressdispatcher.h"

#include <QObject>
#include <QMetaType>
#include <QDebug>
#include <QCoreApplication>

namespace OCC {

ProgressDispatcher* ProgressDispatcher::_instance = 0;

QString Progress::asResultString( const SyncFileItem& item)
{
    switch(item._instruction) {
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_NEW:
        if (item._direction != SyncFileItem::Up) {
            return QCoreApplication::translate( "progress", "Downloaded");
        } else {
            return QCoreApplication::translate( "progress", "Uploaded");
        }
    case CSYNC_INSTRUCTION_REMOVE:
        return QCoreApplication::translate( "progress", "Deleted");
    case CSYNC_INSTRUCTION_EVAL_RENAME:
    case CSYNC_INSTRUCTION_RENAME:
        return QCoreApplication::translate( "progress", "Moved to %1").arg(item._renameTarget);
    case CSYNC_INSTRUCTION_IGNORE:
        return QCoreApplication::translate( "progress", "Ignored");
    case CSYNC_INSTRUCTION_STAT_ERROR:
        return QCoreApplication::translate( "progress", "Filesystem access error");
    case CSYNC_INSTRUCTION_ERROR:
        return QCoreApplication::translate( "progress", "Error");
    case CSYNC_INSTRUCTION_NONE:
    case CSYNC_INSTRUCTION_EVAL:
        return QCoreApplication::translate( "progress", "Unknown");

    }
    return QCoreApplication::translate( "progress", "Unknown");
}

QString Progress::asActionString( const SyncFileItem &item )
{
    switch(item._instruction) {
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_NEW:
        if (item._direction != SyncFileItem::Up)
            return QCoreApplication::translate( "progress", "downloading");
        else
            return QCoreApplication::translate( "progress", "uploading");
    case CSYNC_INSTRUCTION_REMOVE:
        return QCoreApplication::translate( "progress", "deleting");
    case CSYNC_INSTRUCTION_EVAL_RENAME:
    case CSYNC_INSTRUCTION_RENAME:
        return QCoreApplication::translate( "progress", "moving");
    case CSYNC_INSTRUCTION_IGNORE:
        return QCoreApplication::translate( "progress", "ignoring");
    case CSYNC_INSTRUCTION_STAT_ERROR:
        return QCoreApplication::translate( "progress", "error");
    case CSYNC_INSTRUCTION_ERROR:
        return QCoreApplication::translate( "progress", "error");
    case CSYNC_INSTRUCTION_NONE:
    case CSYNC_INSTRUCTION_EVAL:
        break;
    }
    return QString();
}

bool Progress::isWarningKind( SyncFileItem::Status kind)
{
    return  kind == SyncFileItem::SoftError || kind == SyncFileItem::NormalError
         || kind == SyncFileItem::FatalError || kind == SyncFileItem::FileIgnored
         || kind == SyncFileItem::Conflict || kind == SyncFileItem::Restoration;

}

ProgressDispatcher* ProgressDispatcher::instance() {
    if (!_instance) {
        _instance = new ProgressDispatcher();
    }
    return _instance;
}

ProgressDispatcher::ProgressDispatcher(QObject *parent) :
    QObject(parent)
{

}

ProgressDispatcher::~ProgressDispatcher()
{

}

void ProgressDispatcher::setProgressInfo(const QString& folder, const Progress::Info& progress)
{
    if( folder.isEmpty())
// The update phase now also has progress
//            (progress._currentItems.size() == 0
//             && progress._totalFileCount == 0) )
    {
        return;
    }
    emit progressInfo( folder, progress );
}


}
