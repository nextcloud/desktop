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
    QObject(parent)
{

}

ProgressDispatcher::~ProgressDispatcher()
{

}

void ProgressDispatcher::setProgressInfo(const QString& folder, Progress::Info newProgress)
{
    if( folder.isEmpty() ) {
        return;
    }

    if( newProgress.kind == Progress::EndSync ) {
        newProgress.overall_current_bytes = newProgress.overall_transmission_size;
        newProgress.current_file_no = newProgress.overall_file_count;
    }
    _lastProgressHash[folder] = newProgress;

    emit progressInfo( folder, newProgress );
}

Progress::Info ProgressDispatcher::lastProgressInfo(const QString& folder) {
    return _lastProgressHash[folder];
}

}
