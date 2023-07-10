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

#include "putmultifilejob.h"

#include <QHttpPart>

namespace OCC {

Q_LOGGING_CATEGORY(lcPutMultiFileJob, "nextcloud.sync.networkjob.put.multi", QtInfoMsg)

PutMultiFileJob::PutMultiFileJob(AccountPtr account,
                                 const QUrl &url,
                                 std::vector<SingleUploadFileData> devices,
                                 QObject *parent)
    : AbstractNetworkJob(account, {}, parent)
    , _devices(std::move(devices))
    , _url(url)
{
    _body.setContentType(QHttpMultiPart::RelatedType);

    for(const auto &singleDevice : _devices) {
        singleDevice._device->setParent(this);
        connect(this, &PutMultiFileJob::uploadProgress,
                singleDevice._device.get(), &UploadDevice::slotJobUploadProgress);
    }
}

PutMultiFileJob::~PutMultiFileJob() = default;

void PutMultiFileJob::start()
{
    QNetworkRequest req;

    for(const auto &oneDevice : _devices) {
        // Our rate limits in UploadDevice::readData will cause an application freeze if used here.
        // QHttpMultiPart's internal QHttpMultiPartIODevice::readData will loop over and over trying
        // to read data from our UploadDevice while there is data left to be read; this will cause
        // a deadlock as we will never have a chance to progress the data read
        oneDevice._device->setChoked(false);
        oneDevice._device->setBandwidthLimited(false);

        auto onePart = QHttpPart{};

        if (oneDevice._device->size() == 0) {
            onePart.setBody({});
        } else {
            onePart.setBodyDevice(oneDevice._device.get());
        }

        for (auto it = oneDevice._headers.begin(); it != oneDevice._headers.end(); ++it) {
            onePart.setRawHeader(it.key(), it.value());
        }

        req.setPriority(QNetworkRequest::LowPriority); // Long uploads must not block non-propagation jobs.

        _body.append(onePart);
    }

    sendRequest("POST", _url, req, &_body);

    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcPutMultiFileJob) << " Network error: " << reply()->errorString();
    }

    connect(reply(), &QNetworkReply::uploadProgress, this, &PutMultiFileJob::uploadProgress);
    connect(this, &AbstractNetworkJob::networkActivity, account().data(), &Account::propagatorNetworkActivity);
    _requestTimer.start();
    AbstractNetworkJob::start();
}

bool PutMultiFileJob::finished()
{
    qCInfo(lcPutMultiFileJob) << "POST of" << reply()->request().url().toString() << path() << "FINISHED WITH STATUS"
                              << replyStatusString()
                              << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                              << reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute);

    for(const auto &oneDevice : _devices) {
        Q_ASSERT(oneDevice._device);

        if (!oneDevice._device->errorString().isEmpty()) {
            qCWarning(lcPutMultiFileJob) << "oneDevice has error:" << oneDevice._device->errorString();
        }

        if (oneDevice._device->isOpen()) {
            oneDevice._device->close();
        } else {
            qCWarning(lcPutMultiFileJob) << "Did not close device" << oneDevice._device.get()
                                         << "as it was not open";
        }
    }

    emit finishedSignal();
    return true;
}

QString PutMultiFileJob::errorString() const
{
    return _errorString.isEmpty() ? AbstractNetworkJob::errorString() : _errorString;
}

std::chrono::milliseconds PutMultiFileJob::msSinceStart() const
{
    return std::chrono::milliseconds(_requestTimer.elapsed());
}

}
