/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
            const auto allData = oneDevice._device->readAll();
            onePart.setBody(allData);
        }

        if (oneDevice._device->isOpen()) {
            oneDevice._device->close();
        }

        for (auto it = oneDevice._headers.begin(); it != oneDevice._headers.end(); ++it) {
            onePart.setRawHeader(it.key(), it.value());
        }

        req.setPriority(QNetworkRequest::LowPriority); // Long uploads must not block non-propagation jobs.

        _body.append(onePart);
    }

    const auto newReply = sendRequest("POST", _url, req, &_body);
    const auto &requestID = newReply->request().rawHeader("X-Request-ID");

    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcPutMultiFileJob) << " Network error: " << reply()->errorString();
    }

    connect(reply(), &QNetworkReply::uploadProgress, this, &PutMultiFileJob::uploadProgress);
    connect(reply(), &QNetworkReply::uploadProgress, this, [requestID] (qint64 bytesSent, qint64 bytesTotal) {
        qCDebug(lcPutMultiFileJob()) << requestID << "upload progress" << bytesSent << bytesTotal;
    });
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

        if (oneDevice._device->isOpen()) {
            if (!oneDevice._device->errorString().isEmpty()) {
                qCWarning(lcPutMultiFileJob) << "oneDevice has error:" << oneDevice._device->errorString();
            }

            oneDevice._device->close();
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
