/*
 * Copyright 2021 (c) Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#include "abstractnetworkjob.h"

#include "propagateupload.h"
#include "account.h"

#include <QLoggingCategory>
#include <QMap>
#include <QByteArray>
#include <QUrl>
#include <QString>
#include <QElapsedTimer>
#include <QHttpMultiPart>
#include <memory>

class QIODevice;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcPutMultiFileJob)

struct SingleUploadFileData
{
    std::unique_ptr<UploadDevice> _device;
    QMap<QByteArray, QByteArray> _headers;
};

/**
 * @brief The PutMultiFileJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT PutMultiFileJob : public AbstractNetworkJob
{
    Q_OBJECT

public:
    explicit PutMultiFileJob(AccountPtr account,
                             const QUrl &url,
                             std::vector<SingleUploadFileData> devices,
                             QObject *parent = nullptr);

    ~PutMultiFileJob() override;

    void start() override;

    bool finished() override;

    [[nodiscard]] QString errorString() const override;
    [[nodiscard]] std::chrono::milliseconds msSinceStart() const;

signals:
    void finishedSignal();
    void uploadProgress(qint64, qint64);

private:
    QHttpMultiPart _body;
    std::vector<SingleUploadFileData> _devices;
    QString _errorString;
    QUrl _url;
    QElapsedTimer _requestTimer;
};

}
