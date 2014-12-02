/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "propagator_legacy.h"
#include "owncloudpropagator_p.h"

#include "utility.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "filesystem.h"
#include <httpbf.h>
#include <qfile.h>
#include <qdir.h>
#include <qdiriterator.h>
#include <qtemporaryfile.h>
#include <QDebug>
#include <QDateTime>
#include <qstack.h>
#include <QCoreApplication>

#include <neon/ne_basic.h>
#include <neon/ne_socket.h>
#include <neon/ne_session.h>
#include <neon/ne_props.h>
#include <neon/ne_auth.h>
#include <neon/ne_dates.h>
#include <neon/ne_compress.h>
#include <neon/ne_redirect.h>

#include <time.h>


namespace OCC {

static QByteArray get_etag_from_reply(ne_request *req)
{
    QByteArray ret = parseEtag(ne_get_response_header(req, "OC-ETag"));
    if (ret.isEmpty()) {
        ret = parseEtag(ne_get_response_header(req, "ETag"));
    }
    if (ret.isEmpty()) {
        ret = parseEtag(ne_get_response_header(req, "etag"));
    }
    return ret;
}

bool PropagateNeonJob::updateErrorFromSession(int neon_code, ne_request* req, int ignoreHttpCode)
{
    if( neon_code != NE_OK ) {
        qDebug("Neon error code was %d", neon_code);
    }

    QString errorString;
    int httpStatusCode = 0;

    switch(neon_code) {
        case NE_OK:     /* Success, but still the possiblity of problems */
            if( req ) {
                const ne_status *status = ne_get_status(req);

                if (status) {
                    if ( status->klass == 2 || status->code == ignoreHttpCode) {
                        // Everything is ok, no error.
                        return false;
                    }
                    errorString = QString::fromUtf8( status->reason_phrase );
                    httpStatusCode = status->code;
                    _item._httpErrorCode = httpStatusCode;
                }
            } else {
                errorString = QString::fromUtf8(ne_get_error(_propagator->_session));
                httpStatusCode = errorString.mid(0, errorString.indexOf(QChar(' '))).toInt();
                _item._httpErrorCode = httpStatusCode;
                if ((httpStatusCode >= 200 && httpStatusCode < 300)
                    || (httpStatusCode != 0 && httpStatusCode == ignoreHttpCode)) {
                    // No error
                    return false;
                    }
            }
            // FIXME: classify the error
            done (SyncFileItem::NormalError, errorString);
            return true;
        case NE_ERROR:  /* Generic error; use ne_get_error(session) for message */
            errorString = QString::fromUtf8(ne_get_error(_propagator->_session));
            // Check if we don't need to ignore that error.
            httpStatusCode = errorString.mid(0, errorString.indexOf(QChar(' '))).toInt();
            _item._httpErrorCode = httpStatusCode;
            qDebug() << Q_FUNC_INFO << "NE_ERROR" << errorString << httpStatusCode << ignoreHttpCode;
            if (ignoreHttpCode && httpStatusCode == ignoreHttpCode)
                return false;

            done(SyncFileItem::NormalError, errorString);
            return true;
        case NE_LOOKUP:  /* Server or proxy hostname lookup failed */
        case NE_AUTH:     /* User authentication failed on server */
        case NE_PROXYAUTH:  /* User authentication failed on proxy */
        case NE_CONNECT:  /* Could not connect to server */
        case NE_TIMEOUT:  /* Connection timed out */
            done(SyncFileItem::FatalError, QString::fromUtf8(ne_get_error(_propagator->_session)));
            return true;
        case NE_FAILED:   /* The precondition failed */
        case NE_RETRY:    /* Retry request (ne_end_request ONLY) */
        case NE_REDIRECT: /* See ne_redirect.h */
        default:
            done(SyncFileItem::SoftError, QString::fromUtf8(ne_get_error(_propagator->_session)));
            return true;
    }
    return false;
}

void UpdateMTimeAndETagJob::start()
{
    QScopedPointer<char, QScopedPointerPodDeleter> uri(
        ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));
    if (!updateMTimeAndETag(uri.data(), _item._modtime))
        return;
    done(SyncFileItem::Success);
}

void PropagateUploadFileLegacy::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QFile file(_propagator->getFilePath(_item._file));
    if (!file.open(QIODevice::ReadOnly)) {
        done(SyncFileItem::NormalError, file.errorString());
        return;
    }
    QScopedPointer<char, QScopedPointerPodDeleter> uri(
        ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));

    int attempts = 0;

    /*
     * do ten tries to upload the file chunked. Check the file size and mtime
     * before submitting a chunk and after having submitted the last one.
     * If the file has changed, retry.
     */
    qDebug() << "** PUT request to" << uri.data();
    const SyncJournalDb::UploadInfo progressInfo = _propagator->_journal->getUploadInfo(_item._file);

    do {
        Hbf_State state = HBF_SUCCESS;
        QScopedPointer<hbf_transfer_t, ScopedPointerHelpers> trans(hbf_init_transfer(uri.data()));
        Q_ASSERT(trans);
        trans->user_data = this;
        hbf_set_log_callback(trans.data(), _log_callback);
        hbf_set_abort_callback(trans.data(), _user_want_abort);
        trans.data()->chunk_finished_cb = chunk_finished_cb;
        static uint chunkSize = qgetenv("OWNCLOUD_CHUNK_SIZE").toUInt();
        if (chunkSize > 0)  {
            trans->block_size = trans->threshold = chunkSize;
        }

        state = hbf_splitlist(trans.data(), file.handle());

        // This is the modtime hbf will announce to the server.
        // We don't trust the modtime hbf computes itself via _fstat64
        // on windows - hbf may only use it to detect file changes during
        // upload.
        trans->oc_header_modtime = FileSystem::getModTime(file.fileName());

        // If the source file has changed during upload, it is detected and the
        // variable _previousFileSize is set accordingly. The propagator waits a
        // couple of seconds and retries.
        if(_previousFileSize > 0) {
            qDebug() << "File size changed underway: " << trans->stat_size - _previousFileSize;
            // Report the change of the overall transmission size to the propagator (queued connection because we are in a thread)
            QMetaObject::invokeMethod(_propagator, "adjustTotalTransmissionSize", Qt::QueuedConnection,
                                      Q_ARG(qint64, trans->stat_size - _previousFileSize));
            // update the item's values to the current from trans. hbf_splitlist does a stat
            _item._size = trans->stat_size;
            _item._modtime = trans->oc_header_modtime;

        }
        emit progress(_item, 0);

        if (progressInfo._valid) {
            if (Utility::qDateTimeToTime_t(progressInfo._modtime) == _item._modtime) {
                trans->start_id = progressInfo._chunk;
                trans->transfer_id = progressInfo._transferid;
            }
        }

        ne_set_notifier(_propagator->_session, notify_status_cb, this);
        _lastTime.restart();
        _lastProgress = 0;
        _chunked_done = 0;
        _chunked_total_size = _item._size;

        if( state == HBF_SUCCESS ) {
            QByteArray previousEtag;
            if (!_item._etag.isEmpty() && _item._etag != "empty_etag") {
                // We add quotes because the owncloud server always add quotes around the etag, and
                //  csync_owncloud.c's owncloud_file_id always strip the quotes.
                previousEtag = '"' + _item._etag + '"';
                trans->previous_etag = previousEtag.data();
            }
            _chunked_total_size = trans->stat_size;
            qDebug() << "About to upload " << _item._file << "  (" << previousEtag << _item._size << " bytes )";
            /* Transfer all the chunks through the HTTP session using PUT. */
            state = hbf_transfer( _propagator->_session, trans.data(), "PUT" );
        }

        // the file id should only be empty for new files up- or downloaded
        QByteArray fid = hbf_transfer_file_id( trans.data() );
        if( !fid.isEmpty() ) {
            if( !_item._fileId.isEmpty() && _item._fileId != fid ) {
                qDebug() << "WARN: File ID changed!" << _item._fileId << fid;
            }
            _item._fileId = fid;
        }

        /* Handle errors. */
        if ( state != HBF_SUCCESS ) {

            /* If the source file changed during submission, lets try again */
            if( state == HBF_SOURCE_FILE_CHANGE ) {
                if( attempts++ < 5 ) { /* FIXME: How often do we want to try? */
                    qDebug("SOURCE file has changed during upload, retry #%d in %d seconds!", attempts, 2*attempts);
                    Utility::sleep(2*attempts);
                    if( _previousFileSize == 0 ) {
                        _previousFileSize = _item._size;
                    } else {
                        _previousFileSize = trans->stat_size;
                    }
                    continue;
                }

                const QString errMsg = tr("Local file changed during sync, syncing once it arrived completely");
                done( SyncFileItem::SoftError, errMsg );
            } else if( state == HBF_USER_ABORTED ) {
                const QString errMsg = tr("Sync was aborted by user.");
                done( SyncFileItem::SoftError, errMsg );
            } else {
                // Other HBF error conditions.
                _item._httpErrorCode = hbf_fail_http_code(trans.data());
                if(checkForProblemsWithShared(_item._httpErrorCode,
                        tr("The file was edited locally but is part of a read only share. "
                           "It is restored and your edit is in the conflict file.")))
                    return;

                done(SyncFileItem::NormalError, hbf_error_string(trans.data(), state));
            }
            return;
        }

        ne_set_notifier(_propagator->_session, 0, 0);

        if( trans->modtime_accepted ) {
            _item._etag = parseEtag(hbf_transfer_etag( trans.data() ));
        } else {
            if (!updateMTimeAndETag(uri.data(), trans->oc_header_modtime))
                return;
        }

        _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, _propagator->getFilePath(_item._file)));
        // Remove from the progress database:
        _propagator->_journal->setUploadInfo(_item._file, SyncJournalDb::UploadInfo());
        _propagator->_journal->commit("upload file start");

        if (hbf_validate_source_file(trans.data()) == HBF_SOURCE_FILE_CHANGE) {
            /* Did the source file changed since the upload ?
             *               This is different from the previous check because the previous check happens between
             *               chunks while this one happens when the whole file has been uploaded.
             *
             *               The new etag is already stored in the database in the previous lines so in case of
             *               crash, we won't have a conflict but we will properly do a new upload
             */

            if( attempts++ < 5 ) { /* FIXME: How often do we want to try? */
                qDebug("SOURCE file has changed after upload, retry #%d in %d seconds!", attempts, 2*attempts);
                Utility::sleep(2*attempts);
                continue;
            }

            // Still the file change error, but we tried a couple of times.
            // Ignore this file for now.
            // Lets remove the file from the server (at least if it is new) as it is different
            // from our file here.
            if( _item._instruction == CSYNC_INSTRUCTION_NEW ) {
                QScopedPointer<char, QScopedPointerPodDeleter> uri(
                    ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));

                int rc = ne_delete(_propagator->_session, uri.data());
                qDebug() << "Remove the invalid file from server:" << rc;
            }

            const QString errMsg = tr("Local file changed during sync, syncing once it arrived completely");
            done( SyncFileItem::SoftError, errMsg );
            return;
        }

        done(SyncFileItem::Success);
        return;

    } while( true );
}

void PropagateUploadFileLegacy::chunk_finished_cb(hbf_transfer_s *trans, int chunk, void* userdata)
{
    PropagateUploadFileLegacy *that = static_cast<PropagateUploadFileLegacy *>(userdata);
    Q_ASSERT(that);
    that->_chunked_done += trans->block_arr[chunk]->size;
    if (trans->block_cnt > 1) {
        SyncJournalDb::UploadInfo pi;
        pi._valid = true;
        pi._chunk = chunk + 1; // next chunk to start with
        pi._transferid = trans->transfer_id;
        pi._modtime =  Utility::qDateTimeFromTime_t(trans->oc_header_modtime);
        that->_propagator->_journal->setUploadInfo(that->_item._file, pi);
        that->_propagator->_journal->commit("Upload info");
    }
}

void PropagateUploadFileLegacy::notify_status_cb(void* userdata, ne_session_status status,
                                           const ne_session_status_info* info)
{
    PropagateUploadFileLegacy* that = reinterpret_cast<PropagateUploadFileLegacy*>(userdata);

    if (status == ne_status_sending && info->sr.total > 0) {
        emit that->progress(that->_item, that->_chunked_done + info->sr.progress);

        that->limitBandwidth(that->_chunked_done + info->sr.progress,  that->_propagator->_uploadLimit.fetchAndAddAcquire(0));
    }
}



static QByteArray parseFileId(ne_request *req) {
    QByteArray fileId;

    const char *header = ne_get_response_header(req, "OC-FileId");
    if( header ) {
        fileId = header;
    }
    return fileId;
}

bool PropagateNeonJob::updateMTimeAndETag(const char* uri, time_t mtime)
{
    QByteArray modtime = QByteArray::number(qlonglong(mtime));
    ne_propname pname;
    pname.nspace = "DAV:";
    pname.name = "lastmodified";
    ne_proppatch_operation ops[2];
    ops[0].name = &pname;
    ops[0].type = ne_propset;
    ops[0].value = modtime.constData();
    ops[1].name = NULL;

    int rc = ne_proppatch( _propagator->_session, uri, ops );
    Q_UNUSED(rc);
    /* FIXME: error handling
     *    bool error = updateErrorFromSession( rc );
     *    if( error ) {
     *        // FIXME: We could not set the mtime. Error or not?
     *        qDebug() << "PROP-Patching of modified date failed.";
}*/

    // get the etag
    QScopedPointer<ne_request, ScopedPointerHelpers> req(ne_request_create(_propagator->_session, "HEAD", uri));
    int neon_stat = ne_request_dispatch(req.data());
    if (updateErrorFromSession(neon_stat, req.data())) {
        return false;
    } else {
        _item._etag = get_etag_from_reply(req.data());

        QByteArray fid = parseFileId(req.data());
        if( _item._fileId.isEmpty() ) {
            _item._fileId = fid;
            qDebug() << "FileID was empty, set it to " << _item._fileId;
        } else {
            if( !fid.isEmpty() && fid != _item._fileId ) {
                qDebug() << "WARN: FileID seems to have changed: "<< fid << _item._fileId;
            } else {
                qDebug() << "FileID is " << _item._fileId;
            }
        }
        return true;
    }
}

void PropagateNeonJob::limitBandwidth(qint64 progress, qint64 bandwidth_limit)
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0)) {
        // Do not limit bandwidth when aborting to speed up the current transfer
        return;
    }

    if (bandwidth_limit > 0) {
        int64_t diff = _lastTime.nsecsElapsed() / 1000;
        int64_t len = progress - _lastProgress;
        if (len > 0 && diff > 0 && (1000000 * len / diff) > bandwidth_limit) {
            int64_t wait_time = (1000000 * len / bandwidth_limit) - diff;
            if (wait_time > 0) {
                //qDebug() << "Limiting bandwidth to " << bandwidth_limit << "KB/s by waiting " << wait_time << " µs; ";
                OCC::Utility::usleep(wait_time);
            }
        }
        _lastProgress = progress;
        _lastTime.start();
    } else if (bandwidth_limit < 0 && bandwidth_limit > -100) {
        int64_t diff = _lastTime.nsecsElapsed() / 1000;
        if (diff > 0) {
            // -bandwidth_limit is the % of bandwidth
            int64_t wait_time = -diff * (1 + 100.0 / bandwidth_limit);
            if (wait_time > 0) {
                OCC::Utility::usleep(qMin(wait_time, int64_t(1000000*10)));
            }
        }
        _lastTime.start();
    }
}

int PropagateDownloadFileLegacy::content_reader(void *userdata, const char *buf, size_t len)
{
    PropagateDownloadFileLegacy *that = static_cast<PropagateDownloadFileLegacy *>(userdata);
    size_t written = 0;

    if (that->_propagator->_abortRequested.fetchAndAddRelaxed(0)) {
        ne_set_error(that->_propagator->_session, "%s", tr("Sync was aborted by user.").toUtf8().data());
        return NE_ERROR;
    }

    if(buf) {
        written = that->_file->write(buf, len);
        if( len != written || that->_file->error() != QFile::NoError) {
            qDebug() << "WRN: content_reader wrote wrong num of bytes:" << len << "," << written;
            return NE_ERROR;
        }
        return NE_OK;
    }

    return NE_ERROR;
}

/*
 * This hook is called after the response is here from the server, but before
 * the response body is parsed. It decides if the response is compressed and
 * if it is it installs the compression reader accordingly.
 * If the response is not compressed, the normal response body reader is installed.
 */
void PropagateDownloadFileLegacy::install_content_reader( ne_request *req, void *userdata, const ne_status *status )
{
    PropagateDownloadFileLegacy *that = static_cast<PropagateDownloadFileLegacy *>(userdata);

    Q_UNUSED(status);

    if( !that ) {
        qDebug("Error: install_content_reader called without valid write context!");
        return;
    }

    if( ne_get_status(req)->klass != 2 ) {
        qDebug() << "Request class != 2, aborting.";
        ne_add_response_body_reader( req, do_not_accept,
                                     do_not_download_content_reader,
                                     (void*) that );
        return;
    }

    QByteArray reason_phrase = ne_get_status(req)->reason_phrase;
    if(reason_phrase == QByteArray("Connection established")) {
        ne_add_response_body_reader( req, ne_accept_2xx,
                                     content_reader,
                                     (void*) that );
        return;
    }

    QByteArray etag = get_etag_from_reply(req);

    if (etag.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No E-Tag reply by server, considering it invalid" << ne_get_response_header(req, "etag");
        that->abortTransfer(req, tr("No E-Tag received from server, check Proxy/Gateway"));
        return;
    } else if (!that->_expectedEtagForResume.isEmpty() && that->_expectedEtagForResume != etag) {
        qDebug() << Q_FUNC_INFO <<  "We received a different E-Tag for resuming!"
        << QString::fromLatin1(that->_expectedEtagForResume.data()) << "vs"
        << QString::fromLatin1(etag.data());
        that->abortTransfer(req, tr("We received a different E-Tag for resuming. Retrying next time."));
        return;
    }

    quint64 start = 0;
    QByteArray ranges = ne_get_response_header(req, "content-range");
    if (!ranges.isEmpty()) {
        QRegExp rx("bytes (\\d+)-");
        if (rx.indexIn(ranges) >= 0) {
            start = rx.cap(1).toULongLong();
        }
    }
    if (start != that->_resumeStart) {
        qDebug() << Q_FUNC_INFO <<  "Wrong content-range: "<< ranges << " while expecting start was" << that->_resumeStart;
        if (start == 0) {
            // device don't support range, just stry again from scratch
            that->_file->close();
            if (!that->_file->open(QIODevice::WriteOnly)) {
                that->abortTransfer(req, that->_file->errorString());
                return;
            }
        } else {
            that->abortTransfer(req, tr("Server returned wrong content-range"));
            return;
        }
    }


    const char *enc = ne_get_response_header( req, "Content-Encoding" );
    qDebug("Content encoding ist <%s> with status %d", enc ? enc : "empty",
           status ? status->code : -1 );

    if( enc == QLatin1String("gzip") ) {
        that->_decompress.reset(ne_decompress_reader( req, ne_accept_2xx,
                                                      content_reader,     /* reader callback */
                                                      that ));  /* userdata        */
    } else {
        ne_add_response_body_reader( req, ne_accept_2xx,
                                     content_reader,
                                     (void*) that );
    }
}

void PropagateDownloadFileLegacy::abortTransfer(ne_request* req, const QString& error)
{
    errorString = error;
    ne_set_error(_propagator->_session, "%s", errorString.toUtf8().data());
    ne_add_response_body_reader( req, do_not_accept,
                                 do_not_download_content_reader,
                                 this);
}


void PropagateDownloadFileLegacy::notify_status_cb(void* userdata, ne_session_status status,
                                             const ne_session_status_info* info)
{
    PropagateDownloadFileLegacy* that = reinterpret_cast<PropagateDownloadFileLegacy*>(userdata);
    if (status == ne_status_recving && info->sr.total > 0) {
        emit that->progress(that->_item, info->sr.progress );

        that->limitBandwidth(info->sr.progress,  that->_propagator->_downloadLimit.fetchAndAddAcquire(0));
    }
}

extern QString makeConflictFileName(const QString &fn, const QDateTime &dt); // propagatedownload.cpp

void PropagateDownloadFileLegacy::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    // do a case clash check.
    if( _propagator->localFileNameClash(_item._file) ) {
        done( SyncFileItem::NormalError, tr("File %1 can not be downloaded because of a local file name clash!")
              .arg(QDir::toNativeSeparators(_item._file)) );
        return;
    }

    emit progress(_item, 0);

    QString tmpFileName;
    const SyncJournalDb::DownloadInfo progressInfo = _propagator->_journal->getDownloadInfo(_item._file);
    if (progressInfo._valid) {
        // if the etag has changed meanwhile, remove the already downloaded part.
        if (progressInfo._etag != _item._etag) {
            QFile::remove(_propagator->getFilePath(progressInfo._tmpfile));
            _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
        } else {
            tmpFileName = progressInfo._tmpfile;
            _expectedEtagForResume = progressInfo._etag;
        }

    }

    if (tmpFileName.isEmpty()) {
        tmpFileName = _item._file;
        //add a dot at the begining of the filename to hide the file.
        int slashPos = tmpFileName.lastIndexOf('/');
        tmpFileName.insert(slashPos+1, '.');
        //add the suffix
        tmpFileName += ".~" + QString::number(uint(qrand()), 16);
    }

    QFile tmpFile(_propagator->getFilePath(tmpFileName));
    _file = &tmpFile;
    if (!tmpFile.open(QIODevice::Append | QIODevice::Unbuffered)) {
        done(SyncFileItem::NormalError, tmpFile.errorString());
        return;
    }

    FileSystem::setFileHidden(tmpFile.fileName(), true);

    {
        SyncJournalDb::DownloadInfo pi;
        pi._etag = _item._etag;
        pi._tmpfile = tmpFileName;
        pi._valid = true;
        _propagator->_journal->setDownloadInfo(_item._file, pi);
        _propagator->_journal->commit("download file start");
    }

    if (!_item._directDownloadUrl.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "Direct download URL" << _item._directDownloadUrl << "not supported with legacy propagator, will go via ownCloud server";
    }

    /* actually do the request */
    int retry = 0;

    QScopedPointer<char, QScopedPointerPodDeleter> uri(
        ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));

    do {
        QScopedPointer<ne_request, ScopedPointerHelpers> req(ne_request_create(_propagator->_session, "GET", uri.data()));

        /* Allow compressed content by setting the header */
        ne_add_request_header( req.data(), "Accept-Encoding", "gzip" );

        if (tmpFile.size() > 0) {
            quint64 done = tmpFile.size();
            if (done == _item._size) {
                qDebug() << "File is already complete, no need to download";
                break;
            }
            QByteArray rangeRequest = "bytes=" + QByteArray::number(done) +'-';
            ne_add_request_header(req.data(), "Range", rangeRequest.constData());
            ne_add_request_header(req.data(), "Accept-Ranges", "bytes");
            qDebug() << "Retry with range " << rangeRequest;
            _resumeStart = done;
        }

        /* hook called before the content is parsed to set the correct reader,
         * either the compressed- or uncompressed reader.
         */
        ne_hook_post_headers( _propagator->_session, install_content_reader, this);
        ne_set_notifier(_propagator->_session, notify_status_cb, this);
        _lastProgress = 0;
        _lastTime.start();

        int neon_stat = ne_request_dispatch(req.data());

        _decompress.reset(); // Destroy the decompress after the request has been dispatched.

        /* delete the hook again, otherwise they get chained as they are with the session */
        ne_unhook_post_headers( _propagator->_session, install_content_reader, this );
        ne_set_notifier(_propagator->_session, 0, 0);

        if (neon_stat == NE_TIMEOUT && (++retry) < 3) {
            continue;
        }

        // This one is set by install_content_reader if e.g. there is no E-Tag
        if (!errorString.isEmpty()) {
            // don't keep the temporary file as the file downloaded so far is invalid
            tmpFile.close();
            tmpFile.remove();
            _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
            done(SyncFileItem::SoftError, errorString);
            return;
        }

        // This one is set by neon
        if( updateErrorFromSession(neon_stat, req.data() ) ) {
            qDebug("Error GET: Neon: %d", neon_stat);
            if (tmpFile.size() == 0) {
                // don't keep the temporary file if it is empty.
                tmpFile.close();
                tmpFile.remove();
                _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
            }
            return;
        }
        _item._etag = get_etag_from_reply(req.data());
        break;
    } while (1);

    tmpFile.close();
    tmpFile.flush();
    QString fn = _propagator->getFilePath(_item._file);


    bool isConflict = _item._instruction == CSYNC_INSTRUCTION_CONFLICT
        && !FileSystem::fileEquals(fn, tmpFile.fileName()); // compare the files to see if there was an actual conflict.
    //In case of conflict, make a backup of the old file
    if (isConflict) {
        QFile f(fn);
        QString conflictFileName = makeConflictFileName(fn, Utility::qDateTimeFromTime_t(_item._modtime));
        if (!f.rename(conflictFileName)) {
            //If the rename fails, don't replace it.
            done(SyncFileItem::NormalError, f.errorString());
            return;
        }
    }

    QFileInfo existingFile(fn);
    if(existingFile.exists() && existingFile.permissions() != tmpFile.permissions()) {
        tmpFile.setPermissions(existingFile.permissions());
    }

    FileSystem::setFileHidden(tmpFile.fileName(), false);

    QString error;
    _propagator->addTouchedFile(fn);
    if (!FileSystem::renameReplace(tmpFile.fileName(), fn, &error)) {
        done(SyncFileItem::NormalError, error);
        return;
    }

    FileSystem::setModTime(fn, _item._modtime);

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, fn));
    _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
    _propagator->_journal->commit("download file start2");
    done(isConflict ? SyncFileItem::Conflict : SyncFileItem::Success);
}


}
