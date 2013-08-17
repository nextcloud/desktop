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
QString Progress::asResultString( Kind kind )
{
    QString re;

    switch(kind) {
    case Download:
    case EndDownload:
        re = QCoreApplication::translate( "progress", "Download");
        break;
    case Upload:
        re = QCoreApplication::translate( "progress", "Upload");
        break;
    case Context:
        re = QCoreApplication::translate( "progress", "Context" );
        break;
    case Inactive:
        re = QCoreApplication::translate( "progress", "Inactive");
        break;
    case StartDownload:
        re = QCoreApplication::translate( "progress", "Download");
        break;
    case StartUpload:
    case EndUpload:
        re = QCoreApplication::translate( "progress", "Upload");
        break;
    case StartSync:
        re = QCoreApplication::translate( "progress", "Start");
        break;
    case EndSync:
        re = QCoreApplication::translate( "progress", "Finished");
        break;
    case StartDelete:
        re = QCoreApplication::translate( "progress", "For deletion");
        break;
    case EndDelete:
        re = QCoreApplication::translate( "progress", "deleted");
        break;
    default:
        Q_ASSERT(false);
    }
    return re;

}

QString Progress::asActionString( Kind kind )
{
    QString re;

    switch(kind) {
    case Download:
        re = QCoreApplication::translate( "progress", "downloading");
        break;
    case Upload:
        re = QCoreApplication::translate( "progress", "uploading");
        break;
    case Context:
        re = QCoreApplication::translate( "progress", "Context");
        break;
    case Inactive:
        re = QCoreApplication::translate( "progress", "inactive");
        break;
    case StartDownload:
        re = QCoreApplication::translate( "progress", "downloading");
        break;
    case StartUpload:
        re = QCoreApplication::translate( "progress", "uploading");
        break;
    case EndDownload:
        re = QCoreApplication::translate( "progress", "downloading");
        break;
    case EndUpload:
        re = QCoreApplication::translate( "progress", "uploading");
        break;
    case StartSync:
        re = QCoreApplication::translate( "progress", "starting");
        break;
    case EndSync:
        re = QCoreApplication::translate( "progress", "finished");
        break;
    case StartDelete:
        re = QCoreApplication::translate( "progress", "delete");
        break;
    case EndDelete:
        re = QCoreApplication::translate( "progress", "deleted");
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
        // its really
        err.error_message = QString::fromLocal8Bit( (const char*)newProgress.file_size );
        err.error_code    = newProgress.current_file_bytes;
        err.timestamp     = QDateTime::currentDateTime();

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
            _currentAction.remove(newProgress.folder);
        }
        if( newProgress.kind == Progress::EndDownload ||
                newProgress.kind == Progress::EndUpload ||
                newProgress.kind == Progress::EndDelete ) {
            _recentChanges.enqueue(newProgress);
        }
        // store the last real action to help clients that start during
        // the Context-phase of an upload or download.
        if( newProgress.kind != Progress::Context ) {
            _currentAction[folder] = newProgress.kind;
        }

        emit progressInfo( folder, newProgress );
    }
}

Progress::Kind ProgressDispatcher::currentFolderContext( const QString& folder )
{
    if( _currentAction.contains(folder)) {
        return _currentAction[folder];
    }
    return Progress::Invalid;
}

}
