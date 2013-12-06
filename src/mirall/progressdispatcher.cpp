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
QString Progress::asResultString( const Progress::Info& progress)
{
    QString re;

    switch(progress.kind) {
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
    case StartRename:
        re = QCoreApplication::translate( "progress", "Moved to %1").arg(progress.rename_target);
        break;
    case EndRename:
        re = QCoreApplication::translate( "progress", "Moved to %1").arg(progress.rename_target);
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
    case StartRename:
        re = QCoreApplication::translate( "progress", "move");
        break;
    case EndRename:
        re = QCoreApplication::translate( "progress", "moved");
        break;
    default:
        Q_ASSERT(false);
    }
    return re;
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
    QObject(parent),
    _QueueSize(50)
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

void ProgressDispatcher::setProgressProblem(const QString& folder, const Progress::SyncProblem &problem)
{
    Q_ASSERT( Progress::isErrorKind(problem.kind));

    _recentProblems.prepend( problem );
    if( _recentProblems.size() > _QueueSize ) {
        _recentProblems.removeLast();
    }
    emit progressSyncProblem( folder, problem );
}

void ProgressDispatcher::setProgressInfo(const QString& folder, const Progress::Info& progress)
{
    if( folder.isEmpty() ) {
        return;
    }
    Progress::Info newProgress(progress);

    Q_ASSERT( !Progress::isErrorKind(progress.kind));

    if( newProgress.kind == Progress::StartSync ) {
        _recentProblems.clear();
        _timer.start();
    }
    if( newProgress.kind == Progress::EndSync ) {
        newProgress.overall_current_bytes = newProgress.overall_transmission_size;
        newProgress.current_file_no = newProgress.overall_file_count;
        _currentAction.remove(newProgress.folder);
        qint64 msecs = _timer.elapsed();

        qDebug()<< "[PROGRESS] progressed " << newProgress.overall_transmission_size
                << " bytes in " << newProgress.overall_file_count << " files"
                << " in msec " << msecs;
    }
    if( newProgress.kind == Progress::EndDownload ||
            newProgress.kind == Progress::EndUpload ||
            newProgress.kind == Progress::EndDelete ||
            newProgress.kind == Progress::EndRename ) {
        _recentChanges.prepend(newProgress);
        if( _recentChanges.size() > _QueueSize ) {
            _recentChanges.removeLast();
        }
    }
    // store the last real action to help clients that start during
    // the Context-phase of an upload or download.
    if( newProgress.kind != Progress::Context ) {
        _currentAction[folder] = newProgress.kind;
    }

    emit progressInfo( folder, newProgress );

}

Progress::Kind ProgressDispatcher::currentFolderContext( const QString& folder )
{
    if( _currentAction.contains(folder)) {
        return _currentAction[folder];
    }
    return Progress::Invalid;
}

}
