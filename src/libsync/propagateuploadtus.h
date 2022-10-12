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

#include "propagateupload.h"

namespace OCC {
Q_DECLARE_LOGGING_CATEGORY(lcPropagateUploadTUS)

class PropagateUploadFileTUS : public PropagateUploadFileCommon
{
    Q_OBJECT

private:
    SimpleNetworkJob *makeCreationWithUploadJob(QNetworkRequest *request, UploadDevice *device);
    QNetworkRequest prepareRequest(const quint64 &chunkSize);
    UploadDevice *prepareDevice(const quint64 &chunkSize);

    void startNextChunk();
    void slotChunkFinished();
    void finalize(const QString &etag, const QByteArray &fileId);

    quint64 _currentOffset = 0;
    QUrl _location;

public:
    PropagateUploadFileTUS(OwncloudPropagator *propagator, const SyncFileItemPtr &item);

    void doStartUpload() override;
public Q_SLOTS:
    void abort(PropagatorJob::AbortType abortType) override;
};

}
