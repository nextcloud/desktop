/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#pragma once

#include "common/chronoelapsedtimer.h"

#include <QObject>

#include <unordered_map>

namespace OCC {

class FolderMan;
class Folder;

class ETagWatcher : public QObject
{
    Q_OBJECT
public:
    ETagWatcher(FolderMan *folderMan, QObject *parent);

private:
    void updateEtag(Folder *f, const QString &etag);

    // oc10 relies on etag polling, with ocis we use the spaces endpoint
    void startOC10EtagJob(Folder *f);


    FolderMan *_folderMan;

    struct ETagInfo
    {
        QString etag;
        // only used with oc10 in order to decide whether we need to query the etag
        Utility::ChronoElapsedTimer lastUpdate;
    };

    std::unordered_map<Folder *, ETagInfo> _lastEtagJob;
};

}
