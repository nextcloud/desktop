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


namespace Mirall {

ProgressDispatcher* ProgressDispatcher::_instance = 0;

QString Progress::asString( Kind kind )
{
    QString re;

    switch(kind) {
    case Download:
        re = QObject::tr("downloading");
        break;
    case Upload:
        re = QObject::tr("uploading");
        break;
    case Context:
        re = QObject::tr("Context");
        break;
    case Inactive:
        re = QObject::tr("inactive");
        break;
    case StartDownload:
        re = QObject::tr("downloading");
        break;
    case StartUpload:
        re = QObject::tr("uploading");
        break;
    case EndDownload:
        re = QObject::tr("downloading");
        break;
    case EndUpload:
        re = QObject::tr("uploading");
        break;
    case StartSync:
        re = QObject::tr("starting");
        break;
    case EndSync:
        re = QObject::tr("finished");
        break;
    default:
        Q_ASSERT(false);
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
    QObject(parent),
    _problemQueueSize(50)
{

}

ProgressDispatcher::~ProgressDispatcher()
{

}

QList<Progress::Info> ProgressDispatcher::recentChangedItems(int count)
{
    if( count > 0 ) {
        return _recentChanges.mid(0, count);
    }
    return _recentChanges;
}

QList<Progress::SyncProblem> ProgressDispatcher::recentProblems(int count)
{
    if( count > 0 ) {
        return _recentProblems.mid(0, count);
    }
    return _recentProblems;
}

void ProgressDispatcher::setProgressInfo(const QString& folder, const Progress::Info& progress)
{
    if( folder.isEmpty() ) {
        return;
    }
    Progress::Info newProgress = progress;

    if( newProgress.kind == Progress::Error ) {
        Progress::SyncProblem err;
        err.folder        = folder;
        err.current_file  = newProgress.current_file;
        err.error_message = QString::fromLocal8Bit( (const char*)newProgress.file_size );
        err.error_code    = newProgress.file_size;
        err.timestamp     = QTime::currentTime();

        _recentProblems.enqueue( err );
        if( _recentProblems.size() > _problemQueueSize ) {
            _recentProblems.dequeue();
        }
        emit progressSyncProblem( folder, err );
    } else {
        if( newProgress.kind == Progress::StartSync ) {
            _recentProblems.clear();
        }
        if( newProgress.kind == Progress::EndSync ) {
            newProgress.overall_current_bytes = newProgress.overall_transmission_size;
            newProgress.current_file_no = newProgress.overall_file_count;
        }
        if( newProgress.kind == Progress::EndDownload || newProgress.kind == Progress::EndUpload ) {
            _recentChanges.enqueue(newProgress);
        }
        emit progressInfo( folder, newProgress );
    }
}

}
