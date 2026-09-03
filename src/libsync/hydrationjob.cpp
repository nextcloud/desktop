// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 Hannah von Reth <h.vonreth@opencloud.eu>

#include "hydrationjob.h"

#include "common/syncjournaldb.h"
#include "common/vfs.h"
#include "propagatedownload.h"

using namespace OCC;

HydrationJob::HydrationJob(Vfs *vfs, const QByteArray &fileId, std::unique_ptr<QIODevice> &&device, QObject *parent)
    : QObject(parent)
    , _vfs(vfs)
    , _fileId(fileId)
    , _device(std::move(device))
{
}

void HydrationJob::setTargetFile(const QString &fileName)
{
    _fileName = fileName;
}

QString HydrationJob::targetFileName() const
{
    return _fileName;
}

void HydrationJob::start()
{
    const auto dbQueryResult = _vfs->params().journal->getFileRecordsByFileId(_fileId, [this](const SyncJournalFileRecord &record) {
        Q_ASSERT(!_record.isValid());
        _record = record;
    });
    if (!dbQueryResult || !_record.isValid()) {
        Q_EMIT error(tr("Failed to find fileId: %1 in db").arg(QString::fromUtf8(_fileId)));
        Q_EMIT _vfs->needSync();
        return;
    }
    if (!_device->open(QIODevice::WriteOnly)) {
        Q_EMIT error(_device->errorString());
        return;
    }

    Q_ASSERT(!_job);
    // propagator()->account(),
    // propagator()->fullRemotePath(isEncrypted() ? _item->_encryptedFileName : _item->_file),
    //    &_tmpFile, headers, expectedEtagForResume, _resumeStart, this
    _job = new GETFileJob(_vfs->params().account, _vfs->params().remotePath + _record.path(), _device.get(), {}, {}, 0, this);
    _job->setExpectedContentLength(_record._fileSize);
    //_job->setPriority(QNetworkRequest::HighPriority);
    connect(_job, &GETFileJob::finishedSignal, this, [this] {
        QString errorMsg;
        if (_job->reply()->error() != 0 /*|| (_job->httpStatusCode() != 200 && _job->httpStatusCode() != 204)*/) {
            errorMsg = _job->reply()->errorString();
        }

        if (_job->contentLength() != -1) {
            const auto size = _job->resumeStart() + _job->contentLength();
            if (size != _record._fileSize) {
                errorMsg = tr("Unexpected file size transferred. Expected %1 received %2").arg(QString::number(_record._fileSize), QString::number(size));
                // assume that the local and the remote metadata are out of sync
                Q_EMIT _vfs->needSync();
            }
        }
        // if (_job->aborted()) {
        //     errorMsg = tr("Aborted.");
        // }

        if (!errorMsg.isEmpty()) {
            Q_EMIT error(errorMsg);
            return;
        }

        Q_EMIT finished();
    });
    _job->start();
}

void HydrationJob::abort()
{
    // if (_job) {
    //     _job->abort();
    // }
}

Vfs *HydrationJob::vfs() const
{
    return _vfs;
}

SyncJournalFileRecord HydrationJob::record() const
{
    return _record;
}
