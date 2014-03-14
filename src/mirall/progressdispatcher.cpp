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

namespace Mirall {

ProgressDispatcher* ProgressDispatcher::_instance = 0;

QString Progress::asResultString( const SyncFileItem& item)
{
    switch(item._instruction) {
        case CSYNC_INSTRUCTION_CONFLICT:
        case CSYNC_INSTRUCTION_SYNC:
        case CSYNC_INSTRUCTION_NEW:
            if (item._dir == SyncFileItem::Down)
                return QCoreApplication::translate( "progress", "Downloaded");
            else
                return QCoreApplication::translate( "progress", "Uploaded");
        case CSYNC_INSTRUCTION_REMOVE:
            return QCoreApplication::translate( "progress", "Deleted");
        case CSYNC_INSTRUCTION_EVAL_RENAME:
            return QCoreApplication::translate( "progress", "Moved to %1").arg(item._renameTarget);
        default:
            // Should normaly not happen
            return QCoreApplication::translate( "progress", "Unknown");
    }
}

QString Progress::asActionString( const SyncFileItem &item )
{
    switch(item._instruction) {
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_NEW:
        if (item._dir == SyncFileItem::Down)
            return QCoreApplication::translate( "progress", "downloading");
        else
            return QCoreApplication::translate( "progress", "uploading");
    case CSYNC_INSTRUCTION_REMOVE:
        return QCoreApplication::translate( "progress", "deleting");
    case CSYNC_INSTRUCTION_EVAL_RENAME:
        return QCoreApplication::translate( "progress", "moving");
    default:
        // Should normaly not happen
        return QCoreApplication::translate( "progress", "processing");
    }
}

bool Progress::isErrorKind( Kind kind )
{
    bool re = false;
    if( kind == SoftError || kind == NormalError || kind == FatalError ) {
        re = true;
    }
    return re;
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


void ProgressDispatcher::setProgressProblem(const QString& folder, const Progress::SyncProblem &problem)
{
    Q_ASSERT( Progress::isErrorKind(problem.kind));
    emit progressSyncProblem( folder, problem );
}

void ProgressDispatcher::setProgressInfo(const QString& folder, const Progress::Info& progress)
{
    if( folder.isEmpty() ) {
        return;
    }
    emit progressInfo( folder, progress );
}


}
