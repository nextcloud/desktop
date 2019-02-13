/*
 * Copyright (C) by Ahmed Ammar <ahmed.a.ammar@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

extern "C" {
#include "libzsync/zsync.h"
}

#include "config.h"
#include "owncloudpropagator_p.h"
#include "propagatedownload.h"
#include "networkjobs.h"
#include "account.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"
#include "propagateremotedelete.h"
#include "common/checksums.h"
#include "common/asserts.h"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryFile>
#include <cmath>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace OCC {

// DOES NOT take ownership of the device.
GETFileZsyncJob::GETFileZsyncJob(OwncloudPropagator *propagator, SyncFileItemPtr &item,
    const QString &path, QFile *device, const QMap<QByteArray, QByteArray> &headers,
    const QByteArray &expectedEtagForResume, const QByteArray &zsyncData, QObject *parent)
    : GETJob(propagator->account(), path, parent)
    , _device(device)
    , _item(item)
    , _propagator(propagator)
    , _headers(headers)
    , _expectedEtagForResume(expectedEtagForResume)
    , _hasEmittedFinishedSignal(false)
    , _zsyncData(zsyncData)
{
}

void GETFileZsyncJob::startCurrentRange(qint64 start, qint64 end)
{
    // The end of the range might exceed the file size.
    // It's size-1 because the Range header is end-inclusive.
    end = qMin(end, _item->_size - 1);

    _headers["Range"] = "bytes=" + QByteArray::number(start) + '-' + QByteArray::number(end);

    qCDebug(lcZsyncGet) << path() << "HTTP GET with range" << _headers["Range"];

    QNetworkRequest req;
    for (QMap<QByteArray, QByteArray>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }
    req.setPriority(QNetworkRequest::LowPriority); // Long downloads must not block non-propagation jobs.

    sendRequest("GET", makeDavUrl(path()), req);

    reply()->setReadBufferSize(16 * 1024); // keep low so we can easier limit the bandwidth
    qCDebug(lcZsyncGet) << _bandwidthManager << _bandwidthChoked << _bandwidthLimited;
    if (_bandwidthManager) {
        _bandwidthManager->registerDownloadJob(this);
    }

    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcZsyncGet) << " Network error: " << errorString();
    }

    _pos = 0;

    connect(reply(), &QNetworkReply::downloadProgress, this, &GETFileZsyncJob::slotOverallDownloadProgress);
    connect(reply(), &QIODevice::readyRead, this, &GETFileZsyncJob::slotReadyRead);
    connect(reply(), &QNetworkReply::metaDataChanged, this, &GETFileZsyncJob::slotMetaDataChanged);
    connect(this, &AbstractNetworkJob::networkActivity, account().data(), &Account::propagatorNetworkActivity);

    AbstractNetworkJob::start();
}

bool GETFileZsyncJob::finished()
{
    if (reply()->bytesAvailable()) {
        return false;
    }

    // zsync_receive_data will only complete once we have sent block aligned data
    off_t range_size = _zbyterange.get()[(2 * _current) + 1] - _zbyterange.get()[(2 * _current)] + 1;
    if (_pos < range_size) {
        QByteArray fill(range_size - _pos, 0);
        qCDebug(lcZsyncGet) << "About to zsync" << range_size - _pos << "filler bytes @" << _zbyterange.get()[2 * _current] << "pos:" << _pos << "of" << path();
        if (zsync_receive_data(_zr.get(), (const unsigned char *)fill.constData(), _zbyterange.get()[2 * _current] + _pos, range_size - _pos) != 0) {
            _errorString = "Failed to receive data for: " + _propagator->getFilePath(_item->_file);
            _errorStatus = SyncFileItem::NormalError;
            qCWarning(lcZsyncGet) << "Error while writing to file:" << _errorString;
            reply()->abort();
            emit finishedSignal();
            return true;
        }
    }

    // chain the next range if we still have some
    if (_current < _nrange - 1) {
        _current++;
        startCurrentRange(_zbyterange.get()[2 * _current], _zbyterange.get()[(2 * _current) + 1]);
        return false;
    }

    if (!_hasEmittedFinishedSignal) {
        _zr.reset();
        _zs.reset(); // ensure the file is closed.
        emit finishedSignal();
    }

    _hasEmittedFinishedSignal = true;

    return true; // discard
}

void GETFileZsyncJob::seedFinished(void *zs)
{
    _zs = zsync_unique_ptr<struct zsync_state>(static_cast<struct zsync_state *>(zs), [](struct zsync_state *zs) {
        zsync_complete(zs);
        zsync_end(zs);
    });
    if (!_zs) {
        _errorString = tr("Unable to parse zsync.");
        _errorStatus = SyncFileItem::NormalError;
        qCDebug(lcZsyncGet) << _errorString;
        emit finishedSignal();
        return;
    }

    { /* And print how far we've progressed towards the target file */
        long long done, total;

        zsync_progress(_zs.get(), &done, &total);
        qCInfo(lcZsyncGet).nospace() << "Done reading: "
                                     << _propagator->getFilePath(_item->_file)
                                     << " " << fixed << qSetRealNumberPrecision(1) << (100.0f * done) / total
                                     << "% of target seeded.";
    }

    /* Get a set of byte ranges that we need to complete the target */
    _zbyterange = zsync_unique_ptr<off_t>(zsync_needed_byte_ranges(_zs.get(), &_nrange, 0), [](off_t *zbr) {
        free(zbr);
    });
    if (!_zbyterange) {
        _errorString = tr("Failed to get zsync byte ranges.");
        _errorStatus = SyncFileItem::NormalError;
        qCDebug(lcZsyncGet) << _errorString;
        emit finishedSignal();
        return;
    }

    qCDebug(lcZsyncGet) << "Number of ranges:" << _nrange;

    /* If we have no ranges then we have equal files and we are done */
    if (_nrange == 0 && _item->_size == qint64(zsync_file_length(_zs.get()))) {
        _propagator->reportFileTotal(*_item, 0);
        _errorStatus = SyncFileItem::Success;
        _zr.reset();
        _zs.reset(); // ensure the file is closed.
        emit finishedSignal();
        return;
    }

    _zr = zsync_unique_ptr<struct zsync_receiver>(zsync_begin_receive(_zs.get(), 0), [](struct zsync_receiver *zr) {
        zsync_end_receive(zr);
    });
    if (!_zr) {
        _errorString = tr("Failed to initialize zsync receive structure.");
        _errorStatus = SyncFileItem::NormalError;
        qCDebug(lcZsyncGet) << _errorString;
        emit finishedSignal();
        return;
    }

    quint64 totalBytes = 0;
    for (int i = 0; i < _nrange; i++) {
        totalBytes += _zbyterange.get()[(2 * i) + 1] - _zbyterange.get()[(2 * i)] + 1;
    }

    qCDebug(lcZsyncGet) << "Total bytes:" << totalBytes;
    _propagator->reportFileTotal(*_item, totalBytes);

    /* start getting bytes for first zsync byte range */
    startCurrentRange(_zbyterange.get()[0], _zbyterange.get()[1]);
}

void GETFileZsyncJob::seedFailed(const QString &errorString)
{
    _errorString = errorString;
    _errorStatus = SyncFileItem::NormalError;

    qCCritical(lcZsyncGet) << _errorString;

    /* delete remote zsync file */
    QUrl zsyncUrl = zsyncMetadataUrl(_propagator, _item->_file);
    (new DeleteJob(_propagator->account(), zsyncUrl, this))->start();

    emit finishedSignal();
}

void GETFileZsyncJob::start()
{
    ZsyncSeedRunnable *run = new ZsyncSeedRunnable(_zsyncData, _propagator->getFilePath(_item->_file),
        ZsyncMode::download, _device->fileName());
    connect(run, &ZsyncSeedRunnable::finishedSignal, this, &GETFileZsyncJob::seedFinished);
    connect(run, &ZsyncSeedRunnable::failedSignal, this, &GETFileZsyncJob::seedFailed);

    // Starts in a seperate thread
    QThreadPool::globalInstance()->start(run);
}

qint64 GETFileZsyncJob::currentDownloadPosition()
{
    return _received;
}

void GETFileZsyncJob::slotReadyRead()
{
    if (!reply())
        return;

    int bufferSize = qMin(1024 * 8ll, reply()->bytesAvailable());
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    while (reply()->bytesAvailable() > 0) {
        if (_bandwidthChoked) {
            qCWarning(lcZsyncGet) << "Download choked";
            return;
        }
        qint64 toRead = bufferSize;
        if (_bandwidthLimited) {
            toRead = qMin(qint64(bufferSize), _bandwidthQuota);
            if (toRead == 0) {
                qCWarning(lcZsyncGet) << "Out of quota";
                return;
            }
            _bandwidthQuota -= toRead;
        }

        qint64 r = reply()->read(buffer.data(), toRead);
        if (r < 0) {
            _errorString = networkReplyErrorString(*reply());
            _errorStatus = SyncFileItem::NormalError;
            qCWarning(lcZsyncGet) << "Error while reading from device: " << _errorString;
            reply()->abort();
            return;
        }

        if (!_nrange) {
            qCWarning(lcZsyncGet) << "No ranges to fetch.";
            _received += r;
            _pos += r;
            return;
        }

        qCDebug(lcZsyncGet) << "About to zsync" << r << "bytes @" << _zbyterange.get()[2 * _current] << "pos:" << _pos << "of" << path();

        if (zsync_receive_data(_zr.get(), (const unsigned char *)buffer.constData(), _zbyterange.get()[2 * _current] + _pos, r) != 0) {
            _errorString = "Failed to receive data for: " + _propagator->getFilePath(_item->_file);
            _errorStatus = SyncFileItem::NormalError;
            qCWarning(lcZsyncGet) << "Error while writing to file:" << _errorString;
            reply()->abort();
            return;
        }

        _received += r;
        _pos += r;
    }
}

void GETFileZsyncJob::slotMetaDataChanged()
{
    // For some reason setting the read buffer in GETFileJob::start doesn't seem to go
    // through the HTTP layer thread(?)
    reply()->setReadBufferSize(16 * 1024);

    int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // If the status code isn't 2xx, don't write the reply body to the file.
    // For any error: handle it when the job is finished, not here.
    if (httpStatus / 100 != 2) {
        _device->close();
        return;
    }
    if (reply()->error() != QNetworkReply::NoError) {
        return;
    }
    _etag = getEtagFromReply(reply());

    if (!_expectedEtagForResume.isEmpty() && _expectedEtagForResume != _etag) {
        qCWarning(lcZsyncGet) << "We received a different E-Tag for delta!"
                              << _expectedEtagForResume << "vs" << _etag;
        _errorString = tr("We received a different E-Tag for delta. Retrying next time.");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    }

    auto lastModified = reply()->header(QNetworkRequest::LastModifiedHeader);
    if (!lastModified.isNull()) {
        _lastModified = Utility::qDateTimeToTime_t(lastModified.toDateTime());
    }
}

void GETFileZsyncJob::slotOverallDownloadProgress(qint64, qint64)
{
    emit overallDownloadProgress(_received, 0);
}
}
