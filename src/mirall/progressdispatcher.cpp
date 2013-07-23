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
    default:
        re = QObject::tr("What do I know?");
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

void ProgressDispatcher::setFolderProgress( Progress::Kind kind, const QString& folder, const QString& file, long p1, long p2)
{
    emit itemProgress( kind, folder, file, p1, p2 );
}

void ProgressDispatcher::setOverallProgress( const QString& folder, const QString& file, int fileNo, int fileCnt,
                                             qlonglong o1, qlonglong o2 )
{
    emit overallProgress( folder, file, fileNo, fileCnt, o1, o2 );
}

}
