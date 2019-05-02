/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QPointer>

// Manual forward declares to avoid pulling in C headers that don't
// work well with moc (like due to use of "signals" as an identifier).
typedef struct _CloudProvidersProviderExporter CloudProvidersProviderExporter;
typedef struct _CloudProvidersAccountExporter CloudProvidersAccountExporter;

// There's a naming mixup.
// What libcloudproviders calls "Provider" is not our Account, but "the ownCloud client"
// What libcloudproviders calls "Account" we call Folder

namespace OCC {

class LibCloudProviders;
class Folder;

class LibCloudProvidersPrivate : public QObject
{
    Q_OBJECT
public:
    ~LibCloudProvidersPrivate();

    void start();

    void exportFolder(Folder *folder);
    void unexportFolder(Folder *folder);

    struct FolderExport
    {
        /** Can become zero when parent folder is removed, check before use */
        QPointer<Folder> _folder;

        /** For updating exported folder information */
        CloudProvidersAccountExporter *_exporter;
    };

    /** DBus id so exporting can stop on error or destruction */
    uint _busOwnerId = 0;

    /** Exporter for the whole client. */
    CloudProvidersProviderExporter *_exporter = nullptr;

    /** Each folder's exporter for later updating. */
    QMap<Folder *, FolderExport> _folderExports;

    LibCloudProviders *_q = nullptr;

public slots:
    void updateExportedFolderList();
    void updateFolderExport();
};

} // namespace OCC
