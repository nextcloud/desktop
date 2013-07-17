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


namespace Mirall {

ProgressDispatcher* ProgressDispatcher::_instance = 0;

QString Progress::asString( Kind kind )
{
    QString re;

    switch(kind) {
    case Download:
        re = QObject::tr("Download");
        break;
    case Upload:
        re = QObject::tr("Upload");
        break;
    case Context:
        re = QObject::tr("Context");
        break;
    case Inactive:
        re = QObject::tr("Inactive");
        break;
    case StartDownload:
        re = QObject::tr("Start download");
        break;
    case StartUpload:
        re = QObject::tr("Start upload");
        break;
    case EndDownload:
        re = QObject::tr("End download");
        break;
    case EndUpload:
        re = QObject::tr("End upload");
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

void ProgressDispatcher::setFolderProgress( Progress::Kind kind, const QString& alias, const QString& file, long p1, long p2)
{

    emit folderProgress( kind, alias, file, p1, p2 );
}

}
