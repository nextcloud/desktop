/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#include "owncloudpropagator_qnam.h"
#include "networkjobs.h"
#include "account.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include <QNetworkAccessManager>

namespace Mirall {

void PUTFileJob::start() {
    QNetworkRequest req;
    qDebug() << _headers;
    for(QMap<QByteArray, QByteArray>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        req.setRawHeader(it.key(), it.value());
        qDebug() << it.key() << it.value();
    }

    setReply(davRequest("PUT", path(), req, _device));
    _device->setParent(reply());
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qDebug() << "getting etag: request network error: " << reply()->errorString();
    }
    AbstractNetworkJob::start();

}


void PropagateUploadFileQNAM::start()
{
    QFile *file = new QFile(_propagator->_localDir + _item._file);
    if (!file->open(QIODevice::ReadOnly)) {
        done(SyncFileItem::NormalError, file->errorString());
        delete file;
        return;
    }

    //TODO
    //const SyncJournalDb::UploadInfo progressInfo = _propagator->_journal->getUploadInfo(_item._file);
    QMap<QByteArray, QByteArray> headers;
    headers["OC-Total-Length"] = QByteArray::number(file->size());
    headers["Content-Type"] = "application/octet-stream";
    headers["X-OC-Mtime"] = QByteArray::number(qint64(_item._modtime));
    if (!_item._etag.isEmpty() && _item._etag != "empty_etag") {
        // We add quotes because the owncloud server always add quotes around the etag, and
        //  csync_owncloud.c's owncloud_file_id always strip the quotes.
        headers["If-Match"] = '"' + _item._etag + '"';
    }
    PUTFileJob *job = new PUTFileJob(AccountManager::instance()->account(), _item._file, file, headers);
    connect(job, SIGNAL(finishedSignal()), this, SLOT(slotPutFinished()));

//     if( transfer->block_cnt > 1 ) {
//         ne_add_request_header(req, "OC-Chunked", "1");
//     }


        /*
        // If the source file has changed during upload, it is detected and the
        // variable _previousFileSize is set accordingly. The propagator waits a
        // couple of seconds and retries.
        if(_previousFileSize > 0) {
            qDebug() << "File size changed underway: " << trans->stat_size - _previousFileSize;
            // Report the change of the overall transmission size to the propagator
            _propagator->overallTransmissionSizeChanged(qint64(trans->stat_size - _previousFileSize));
            // update the item's values to the current from trans. hbf_splitlist does a stat
            _item._size = trans->stat_size;
            _item._modtime = trans->modtime;

        }
        if (progressInfo._valid) {
            if (Utility::qDateTimeToTime_t(progressInfo._modtime) == _item._modtime) {
                trans->start_id = progressInfo._chunk;
                trans->transfer_id = progressInfo._transferid;
            }
        }
        */

    emit progress(Progress::StartUpload, _item, 0, file->size());
    job->start();
}

void PropagateUploadFileQNAM::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    Q_ASSERT(job);

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {
//             /* If the source file changed during submission, lets try again */
//             if( state == HBF_SOURCE_FILE_CHANGE ) {
//                 if( attempts++ < 5 ) { /* FIXME: How often do we want to try? */
//                     qDebug("SOURCE file has changed during upload, retry #%d in %d seconds!", attempts, 2*attempts);
//                     sleep(2*attempts);
//                     if( _previousFileSize == 0 ) {
//                         _previousFileSize = _item._size;
//                     } else {
//                         _previousFileSize = trans->stat_size;
//                     }
//                     continue;
//                 }
//
//                 const QString errMsg = tr("Local file changed during sync, syncing once it arrived completely");
//                 done( SyncFileItem::SoftError, errMsg );
//             } else if( state == HBF_USER_ABORTED ) {
//                 const QString errMsg = tr("Sync was aborted by user.");
//                 done( SyncFileItem::SoftError, errMsg );
//             } else {
//                 // Other HBF error conditions.
//                 // FIXME: find out the error class.
//                 _item._httpErrorCode = hbf_fail_http_code(trans.data());
        _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(SyncFileItem::NormalError, job->reply()->errorString());
        return;
    }


    // the file id should only be empty for new files up- or downloaded
    QString fid = QString::fromUtf8(job->reply()->rawHeader("OC-FileID"));
    if( !fid.isEmpty() ) {
        if( !_item._fileId.isEmpty() && _item._fileId != fid ) {
            qDebug() << "WARN: File ID changed!" << _item._fileId << fid;
        }
        _item._fileId = fid;
    }

    _item._etag = parseEtag(job->reply()->rawHeader("ETag"));

    if (job->reply()->rawHeader("X-OC-MTime") != "accepted") {
        //FIXME
//             updateMTimeAndETag(uri.data(), _item._modtime);
        done(SyncFileItem::NormalError, tr("No X-OC-MTime extension,  ownCloud 5 is required"));
        return;
    }

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, _propagator->_localDir + _item._file));
    // Remove from the progress database:
    _propagator->_journal->setUploadInfo(_item._file, SyncJournalDb::UploadInfo());
    _propagator->_journal->commit("upload file start");

//     if (hbf_validate_source_file(trans.data()) == HBF_SOURCE_FILE_CHANGE) {
//             /* Did the source file changed since the upload ?
//              *               This is different from the previous check because the previous check happens between
//              *               chunks while this one happens when the whole file has been uploaded.
//              *
//              *               The new etag is already stored in the database in the previous lines so in case of
//              *               crash, we won't have a conflict but we will properly do a new upload
//              */
//
//             if( attempts++ < 5 ) { /* FIXME: How often do we want to try? */
//                 qDebug("SOURCE file has changed after upload, retry #%d in %d seconds!", attempts, 2*attempts);
//                 sleep(2*attempts);
//                 continue;
//             }
//
//             // Still the file change error, but we tried a couple of times.
//             // Ignore this file for now.
//             // Lets remove the file from the server (at least if it is new) as it is different
//             // from our file here.
//             if( _item._instruction == CSYNC_INSTRUCTION_NEW ) {
//                 QScopedPointer<char, QScopedPointerPodDeleter> uri(
//                     ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));
//
//                 int rc = ne_delete(_propagator->_session, uri.data());
//                 qDebug() << "Remove the invalid file from server:" << rc;
//             }
//
//             const QString errMsg = tr("Local file changed during sync, syncing once it arrived completely");
//             done( SyncFileItem::SoftError, errMsg );
//             return;
//         }
//
    emit progress(Progress::EndUpload, _item, _item._size, _item._size);
    done(SyncFileItem::Success);
}

}
