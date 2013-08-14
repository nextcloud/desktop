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

#include "owncloudpropagator.h"
#include "progressdatabase.h"
#include <httpbf.h>
#include <qfile.h>
#include <qdir.h>
#include <qdiriterator.h>
#include <qtemporaryfile.h>
#include <qabstractfileengine.h>
#include <qdebug.h>
#include <QDateTime>

#include <neon/ne_basic.h>
#include <neon/ne_socket.h>
#include <neon/ne_session.h>
#include <neon/ne_props.h>
#include <neon/ne_auth.h>
#include <neon/ne_dates.h>
#include <neon/ne_compress.h>
#include <neon/ne_redirect.h>

// We use some internals of csync:
extern "C" int c_utimes(const char *, const struct timeval *);
extern "C" void csync_win32_set_file_hidden( const char *file, bool h );

namespace Mirall {

/* Helper for QScopedPointer<>, to be used as the deleter.
 * QScopePointer will call the right overload of cleanup for the pointer it holds
 */
struct ScopedPointerHelpers {
    static inline void cleanup(hbf_transfer_t *pointer) { if (pointer) hbf_free_transfer(pointer); }
    static inline void cleanup(ne_request *pointer) { if (pointer) ne_request_destroy(pointer); }
    static inline void cleanup(ne_decompress *pointer) { if (pointer) ne_decompress_destroy(pointer); }
//     static inline void cleanup(ne_propfind_handler *pointer) { if (pointer) ne_propfind_destroy(pointer); }
};

void OwncloudPropagator::propagate(const SyncFileItem &item)
{
    _errorCode = CSYNC_ERR_NONE;
    _errorString.clear();
    _httpStatusCode = 0;
    switch(item._instruction) {
        case CSYNC_INSTRUCTION_REMOVE:
            _instruction = item._dir == SyncFileItem::Down ? localRemove(item) : remoteRemove(item);
            break;
        case CSYNC_INSTRUCTION_NEW:
            if (item._isDirectory) {
                _instruction = item._dir == SyncFileItem::Down ? localMkdir(item) : remoteMkdir(item);
                break;
            }   //fall trough
        case CSYNC_INSTRUCTION_SYNC:
            if (item._isDirectory) {
                // Should we set the mtime?
                _instruction = CSYNC_INSTRUCTION_UPDATED;
                break;
            }
            _instruction = item._dir == SyncFileItem::Down ? downloadFile(item) : uploadFile(item);
            break;
        case CSYNC_INSTRUCTION_CONFLICT:
            if (item._isDirectory) {
                _instruction = CSYNC_INSTRUCTION_UPDATED;
                break;
            }
            _instruction = downloadFile(item, true);
            break;
        case CSYNC_INSTRUCTION_RENAME:
            _instruction = remoteRename(item);
            break;
        default:
            _instruction = item._instruction;
            break;
    }
    SyncFileItem newItem = item;
    newItem._instruction = _instruction;
    newItem._errorDetail = _errorString;
    newItem._httpCode = _httpStatusCode;
    newItem._etag = _etag;
    emit completed(newItem, _errorCode);
}

// compare two files with given filename and return true if they have the same content
static bool fileEquals(const QString &fn1, const QString &fn2) {
    QFile f1(fn1);
    QFile f2(fn2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        qDebug() << "fileEquals: Failed to open " << fn1 << "or" << fn2;
        return false;
    }

    if (f1.size() != f2.size()) {
        return false;
    }

    const int BufferSize = 16 * 1024;
    char buffer1[BufferSize];
    char buffer2[BufferSize];
    do {
        int r = f1.read(buffer1, BufferSize);
        if (f2.read(buffer2, BufferSize) != r) {
            // this should normaly not happen: the file are supposed to have the same size.
            return false;
        }
        if (r <= 0) {
            return true;
        }
        if (memcmp(buffer1, buffer2, r) != 0) {
            return false;
        }
    } while (true);
    return false;
}

// Code copied from Qt5's QDir::removeRecursively
static bool removeRecursively(const QString &path)
{
    bool success = true;
    QDirIterator di(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    while (di.hasNext()) {
        di.next();
        const QFileInfo& fi = di.fileInfo();
        bool ok;
        if (fi.isDir() && !fi.isSymLink())
            ok = removeRecursively(di.filePath()); // recursive
        else
            ok = QFile::remove(di.filePath());
        if (!ok)
            success = false;
    }
    if (success)
        success = QDir().rmdir(path);
    return success;
}

csync_instructions_e OwncloudPropagator::localRemove(const SyncFileItem& item)
{
    QString filename = _localDir +  item._file;
    if (item._isDirectory) {
        if (!QDir(filename).exists() || removeRecursively(filename))
            return CSYNC_INSTRUCTION_DELETED;
    } else {
        QFile file(filename);
        if (!file.exists() || file.remove())
            return CSYNC_INSTRUCTION_DELETED;
        _errorString = file.errorString();
    }
    return CSYNC_INSTRUCTION_NONE; // not ERROR so it is still written to the database
}

csync_instructions_e OwncloudPropagator::localMkdir(const SyncFileItem &item)
{
    QDir d;
    if (!d.mkpath(_localDir + item._file)) {
        _errorString = "could not create directory " + _localDir + item._file;
        return CSYNC_INSTRUCTION_ERROR;
    }
    return CSYNC_INSTRUCTION_UPDATED;
}

csync_instructions_e OwncloudPropagator::remoteRemove(const SyncFileItem &item)
{
    bool error = false;

    QScopedPointer<char, QScopedPointerPodDeleter> uri(ne_path_escape((_remoteDir + item._file).toUtf8()));
    int rc = ne_delete(_session, uri.data());

    error = updateErrorFromSession(rc);
    if (error) {
        return CSYNC_INSTRUCTION_ERROR;
    }
    return CSYNC_INSTRUCTION_DELETED;
}

csync_instructions_e OwncloudPropagator::remoteMkdir(const SyncFileItem &item)
{
    QScopedPointer<char, QScopedPointerPodDeleter> uri(ne_path_escape((_remoteDir + item._file).toUtf8()));
    bool error = false;

    int rc = ne_mkcol(_session, uri.data());
    error = updateErrorFromSession( rc );

    if( error ) {
        /* Special for mkcol: it returns 405 if the directory already exists.
         * Ignre that error */
        if (_httpStatusCode != 405) {
            return CSYNC_INSTRUCTION_ERROR;
        }
    }
    return CSYNC_INSTRUCTION_UPDATED;
}


csync_instructions_e OwncloudPropagator::uploadFile(const SyncFileItem &item)
{
    QFile file(_localDir + item._file);
    if (!file.open(QIODevice::ReadOnly)) {
        _errorString = file.errorString();
        return CSYNC_INSTRUCTION_ERROR;
    }
    QScopedPointer<char, QScopedPointerPodDeleter> uri(ne_path_escape((_remoteDir + item._file).toUtf8()));

    bool finished = true;
    int  attempts = 0;
    /*
     * do ten tries to upload the file chunked. Check the file size and mtime
     * before submitting a chunk and after having submitted the last one.
     * If the file has changed, retry.
    */
    do {
        Hbf_State state = HBF_SUCCESS;
        QScopedPointer<hbf_transfer_t, ScopedPointerHelpers> trans(hbf_init_transfer(uri.data()));
        finished = true;
        Q_ASSERT(trans);
        state = hbf_splitlist(trans.data(), file.handle());

        if (const ProgressDatabase::UploadInfo* progressInfo = _progressDb->getUploadInfo(item._file)) {
            if (progressInfo->mtime == item._modtime) {
                trans->start_id = progressInfo->chunk;
                trans->transfer_id = progressInfo->transferid;
            }
            _progressDb->remove(item._file);
        }

        ne_set_notifier(_session, notify_status_cb, this);
        _lastTime.restart();
        _lastProgress = 0;
        _chunked_done = 0;
        _chunked_total_size = item._size;
        _currentFile = item._file;

        if( state == HBF_SUCCESS ) {
            _chunked_total_size = trans->stat_size;
            /* Transfer all the chunks through the HTTP session using PUT. */
            state = hbf_transfer( _session, trans.data(), "PUT" );
        }

        /* Handle errors. */
        if ( state != HBF_SUCCESS ) {

            /* If the source file changed during submission, lets try again */
            if( state == HBF_SOURCE_FILE_CHANGE ) {
              if( attempts++ < 30 ) { /* FIXME: How often do we want to try? */
                finished = false; /* make it try again from scratch. */
                qDebug("SOURCE file has changed during upload, retry #%d in two seconds!", attempts);
                sleep(2);
              }
            }

            if( finished ) {
                _errorString = hbf_error_string(trans.data(), state);
                _httpStatusCode = hbf_fail_http_code(trans.data());

                if (trans->start_id > 0) {
                    ProgressDatabase::UploadInfo pi;
                    pi.chunk = trans->start_id;
                    pi.transferid = trans->transfer_id;
                    pi.mtime = item._modtime;
                    _progressDb->setUploadInfo(item._file, pi);
                }
                return CSYNC_INSTRUCTION_ERROR;
            }
        }
    } while( !finished );

    ne_set_notifier(_session, 0, 0);

    updateMTimeAndETag(uri.data(), item._modtime);

    return CSYNC_INSTRUCTION_UPDATED;
}

static QByteArray parseEtag(ne_request *req) {
    const char *header = ne_get_response_header(req, "etag");
    if(header && header [0] == '"' && header[ strlen(header)-1] == '"') {
        return QByteArray(header + 1, strlen(header)-2);
    } else {
        return header;
    }
}

void OwncloudPropagator::updateMTimeAndETag(const char* uri, time_t mtime)
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

    int rc = ne_proppatch( _session, uri, ops );
    bool error = updateErrorFromSession( rc );
    if( error ) {
        // FIXME: We could not set the mtime. Error or not?
        qDebug() << "PROP-Patching of modified date failed.";
    }

    // get the etag
    QScopedPointer<ne_request, ScopedPointerHelpers> req(ne_request_create(_session, "HEAD", uri));
    int neon_stat = ne_request_dispatch(req.data());

    if( updateErrorFromSession(neon_stat, req.data()) ) {
        // error happend
        qDebug() << "Could not issue HEAD request for ETag.";
    } else {
        _etag = parseEtag(req.data());
    }
}

class DownloadContext {

public:
    QFile *_file;
    QScopedPointer<ne_decompress, ScopedPointerHelpers> _decompress;

    explicit DownloadContext(QFile *file) : _file(file) {}

    static int content_reader(void *userdata, const char *buf, size_t len)
    {
        DownloadContext *writeCtx = static_cast<DownloadContext *>(userdata);
        size_t written = 0;

        if(buf) {
            written = writeCtx->_file->write(buf, len);
            if( len != written ) {
                qDebug("WRN: content_reader wrote wrong num of bytes: %zu, %zu", len, written);
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
    static void install_content_reader( ne_request *req, void *userdata, const ne_status *status )
    {
        DownloadContext *writeCtx = static_cast<DownloadContext *>(userdata);

        Q_UNUSED(status);

        if( !writeCtx ) {
            qDebug("Error: install_content_reader called without valid write context!");
            return;
        }

        const char *enc = ne_get_response_header( req, "Content-Encoding" );
        qDebug("Content encoding ist <%s> with status %d", enc ? enc : "empty",
                    status ? status->code : -1 );

        if( enc == QLatin1String("gzip") ) {
            writeCtx->_decompress.reset(ne_decompress_reader( req, ne_accept_2xx,
                                                             content_reader,     /* reader callback */
                                                             writeCtx ));  /* userdata        */
        } else {
            ne_add_response_body_reader( req, ne_accept_2xx,
                                        content_reader,
                                        (void*) writeCtx );
        }
    }
};

csync_instructions_e OwncloudPropagator::downloadFile(const SyncFileItem &item, bool isConflict)
{
    QString tmpFileName;
    const ProgressDatabase::DownloadInfo* progressInfo = _progressDb->getDownloadInfo(item._file);
    if (progressInfo) {
        if (progressInfo->etag != item._etag) {
            QFile::remove(_localDir + progressInfo->tmpfile);
        } else {
            tmpFileName = progressInfo->tmpfile;
        }
        _progressDb->remove(item._file);
    }
    if (tmpFileName.isEmpty()) {
        tmpFileName = item._file;
        //add a dot at the begining of the filename to hide the file.
        int slashPos = tmpFileName.lastIndexOf('/');
        tmpFileName.insert(slashPos+1, '.');
        //add the suffix
        tmpFileName += ".~" + QString::number(uint(qrand()), 16);
    }

    QFile tmpFile(_localDir + tmpFileName);
    if (!tmpFile.open(QIODevice::Append)) {
        _errorString = tmpFile.errorString();
        _errorCode = CSYNC_ERR_FILESYSTEM;
        return CSYNC_INSTRUCTION_ERROR;
    }

    csync_win32_set_file_hidden(tmpFileName.toUtf8().constData(), true);

    {
        ProgressDatabase::DownloadInfo pi;
        pi.etag = item._etag;
        pi.tmpfile = tmpFileName;
        _progressDb->setDownloadInfo(item._file, pi);
        _progressDb->save(_localDir);
    }

    /* actually do the request */
    int retry = 0;

    QScopedPointer<char, QScopedPointerPodDeleter> uri(ne_path_escape((_remoteDir + item._file).toUtf8()));
    DownloadContext writeCtx(&tmpFile);

    do {
        QScopedPointer<ne_request, ScopedPointerHelpers> req(ne_request_create(_session, "GET", uri.data()));

        /* Allow compressed content by setting the header */
        ne_add_request_header( req.data(), "Accept-Encoding", "gzip" );

        if (tmpFile.size() > 0) {
            char brange[64];
            ne_snprintf(brange, sizeof brange, "bytes=%lld-", (long long) tmpFile.size());
            ne_add_request_header(req.data(), "Range", brange);
            ne_add_request_header(req.data(), "Accept-Ranges", "bytes");
            qDebug("Retry with range %s", brange);
        }

        /* hook called before the content is parsed to set the correct reader,
         * either the compressed- or uncompressed reader.
         */
        ne_hook_post_headers( _session, DownloadContext::install_content_reader, &writeCtx);
        ne_set_notifier(_session, notify_status_cb, this);
        _lastProgress = 0;
        _lastTime.start();
        _chunked_done = _chunked_total_size = 0;
        _currentFile = item._file;

        int neon_stat = ne_request_dispatch(req.data());

        if (neon_stat == NE_TIMEOUT && (++retry) < 3) {
            continue;
        }

        /* delete the hook again, otherwise they get chained as they are with the session */
        ne_unhook_post_headers( _session, DownloadContext::install_content_reader, &writeCtx );
        ne_set_notifier(_session, 0, 0);
        _chunked_done = _chunked_total_size = 0;

        if( updateErrorFromSession(neon_stat, req.data() ) ) {
            qDebug("Error GET: Neon: %d", neon_stat);
            if (tmpFile.size() == 0) {
                // don't keep the temporary file if it is empty.
                tmpFile.close();
                tmpFile.remove();
                _progressDb->remove(item._file);
            }
            return CSYNC_INSTRUCTION_ERROR;
        }

        _etag = parseEtag(req.data());
        break;
    } while (1);


    tmpFile.close();
    tmpFile.flush();

    //In case of conflict, make a backup of the old file
    if (isConflict) {
        QString fn = _localDir + item._file;

        // compare the files to see if there was an actual conflict.
        if (fileEquals(fn, tmpFile.fileName())) {
            tmpFile.remove();
            _progressDb->remove(item._file);
            return CSYNC_INSTRUCTION_UPDATED;
        }

        QFile f(fn);
        // Add _conflict-XXXX  before the extention.
        int dotLocation = fn.lastIndexOf('.');
        // If no extention, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
        if (dotLocation <= fn.lastIndexOf('/') + 1) {
            dotLocation = fn.size();
        }
        fn.insert(dotLocation, "_conflict-" + QDateTime::fromTime_t(item._modtime).toString("yyyyMMdd-hhmmss"));
        if (!f.rename(fn)) {
            //If the rename fails, don't replace it.
            _errorString = f.errorString();
            return CSYNC_INSTRUCTION_ERROR;
        }
    }

    csync_win32_set_file_hidden(tmpFileName.toUtf8().constData(), false);

    // We want a rename that also overwite.  QFile::rename does not overwite.
    // Qt 5.1 has QFile::renameOverwrite we cold use.  (Or even better: QSaveFile)
#ifndef QT_OS_WIN
    if (!tmpFile.fileEngine()->rename(_localDir + item._file)) {
        _errorString = tmpFile.errorString();
        return CSYNC_INSTRUCTION_ERROR;
    }
#else //QT_OS_WIN
    if (::MoveFileEx((wchar_t*)tmpFile.fileName().utf16(),
                            (wchar_t*)QString(_localDir + item._file).utf16(),
                        MOVEFILE_REPLACE_EXISTING) != 0) {
        wchar_t *string = 0;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL, ::GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPWSTR)&string, 0, NULL);
        _errorString = QString::fromWCharArray(string);
        LocalFree((HLOCAL)string);
        return CSYNC_INSTRUCTION_ERROR;
    }
#endif

    _progressDb->remove(item._file);

    struct timeval times[2];
    times[0].tv_sec = times[1].tv_sec = item._modtime;
    times[0].tv_usec = times[1].tv_usec = 0;
    c_utimes((_localDir + item._file).toUtf8().data(), times);

    return CSYNC_INSTRUCTION_UPDATED;
}

csync_instructions_e OwncloudPropagator::remoteRename(const SyncFileItem &item)
{
    if (item._file == item._renameTarget)
        return CSYNC_INSTRUCTION_DELETED; // nothing to do;
    QScopedPointer<char, QScopedPointerPodDeleter> uri1(ne_path_escape((_remoteDir + item._file).toUtf8()));
    QScopedPointer<char, QScopedPointerPodDeleter> uri2(ne_path_escape((_remoteDir + item._renameTarget).toUtf8()));


    int rc = ne_move(_session, 1, uri1.data(), uri2.data());
    if (updateErrorFromSession(rc)) {
        // We set the instruction to UPDATED so next try we try to rename again
        return CSYNC_INSTRUCTION_UPDATED;
    }

    updateMTimeAndETag(uri2.data(), item._modtime);

    return CSYNC_INSTRUCTION_DELETED;
}

bool OwncloudPropagator::check_neon_session()
{
    bool isOk = true;
    if( !_session ) {
        _errorCode = CSYNC_ERR_PARAM;
        isOk = false;
    } else {
        const char *p = ne_get_error( _session );
        _errorString = QString::fromUtf8(p);

        if( !_errorString.isEmpty() ) {
            int firstSpace = _errorString.indexOf(QChar(' '));
            if( firstSpace > 0 ) {
                bool ok;
                QString numStr = _errorString.mid(0, firstSpace);
                _httpStatusCode = numStr.toInt(&ok);

                if( !ok ) {
                    _httpStatusCode = 0;
                }
            }
            isOk = false;
        }
    }
    return isOk;
}

// returns true in case there was an error
bool OwncloudPropagator::updateErrorFromSession(int neon_code, ne_request *req)
{
    bool re = false;

    if( neon_code != NE_OK ) {
        qDebug("Neon error code was %d", neon_code);
        re = true; // there was an error.
    }

    switch(neon_code) {
    case NE_OK:     /* Success, but still the possiblity of problems */
        if( req != NULL ) {
            const ne_status *status = ne_get_status(req);
            if( status ) {
                if( status->klass != 2 ) {
                    _httpStatusCode = status->code;
                    _errorCode = CSYNC_ERR_HTTP;
                    _errorString = QString::fromUtf8( status->reason_phrase );
                    re = true;
                }
            } else {
                re = true; // can not get the status
            }
        } else {
            // no neon request available.
            re = check_neon_session();
        }
        break;
    case NE_ERROR:  /* Generic error; use ne_get_error(session) for message */
        _errorString = QString::fromUtf8( ne_get_error(_session) );
        _errorCode = CSYNC_ERR_HTTP;
        break;
    case NE_LOOKUP:  /* Server or proxy hostname lookup failed */
        _errorString = QString::fromUtf8( ne_get_error(_session) );
        _errorCode = CSYNC_ERR_LOOKUP;
        _hasFatalError = true;
        break;
    case NE_AUTH:     /* User authentication failed on server */
        _errorString = QString::fromUtf8( ne_get_error(_session) );
        _errorCode = CSYNC_ERR_AUTH_SERVER;
        _hasFatalError = true;
        break;
    case NE_PROXYAUTH:  /* User authentication failed on proxy */
        _errorString = QString::fromUtf8( ne_get_error(_session) );
        _errorCode = CSYNC_ERR_AUTH_PROXY;
        _hasFatalError = true;
        break;
    case NE_CONNECT:  /* Could not connect to server */
        _errorString = QString::fromUtf8( ne_get_error(_session) );
        _errorCode = CSYNC_ERR_CONNECT;
        _hasFatalError = true;
        break;
    case NE_TIMEOUT:  /* Connection timed out */
        _errorString = QString::fromUtf8( ne_get_error(_session) );
        _errorCode = CSYNC_ERR_TIMEOUT;
        _hasFatalError = true;
        break;
    case NE_FAILED:   /* The precondition failed */
    case NE_RETRY:    /* Retry request (ne_end_request ONLY) */
    case NE_REDIRECT: /* See ne_redirect.h */
    default:
        _errorString = QString::fromUtf8( ne_get_error(_session) );
        _errorCode = CSYNC_ERR_HTTP;
        break;
    }
    return re;
}

void OwncloudPropagator::notify_status_cb(void* userdata, ne_session_status status,
                                          const ne_session_status_info* info)
{
    OwncloudPropagator* this_ = reinterpret_cast<OwncloudPropagator *>(userdata);

    if ((status == ne_status_sending || status == ne_status_recving)) {
        if (info->sr.total > 0) {
            emit this_->progress(Progress::Context, this_->_currentFile,
                                 this_->_chunked_done + info->sr.progress,
                                 this_->_chunked_total_size ? this_->_chunked_total_size : info->sr.total );
        }
        if (this_->_chunked_total_size && info->sr.total > 0 && info->sr.total == info->sr.progress) {
            this_->_chunked_done += info->sr.total;
        }
    }

    /* throttle connection */
    int bandwidth_limit = 0;
    if (status == ne_status_sending) bandwidth_limit = this_->_uploadLimit;
    if (status == ne_status_recving) bandwidth_limit = this_->_downloadLimit;
    if (bandwidth_limit > 0) {
        int64_t diff = this_->_lastTime.nsecsElapsed() / 1000;
        int64_t len = info->sr.progress - this_->_lastProgress;
        if (len > 0 && diff > 0 && (1000000 * len / diff) > (int64_t)bandwidth_limit) {
            int64_t wait_time = (1000000 * len / bandwidth_limit) - diff;
            if (wait_time > 0) {
                usleep(wait_time);
            }
        }
        this_->_lastProgress = info->sr.progress;
        this_->_lastTime.start();
    } else if (bandwidth_limit < 0 && bandwidth_limit > -100) {
        int64_t diff = this_->_lastTime.nsecsElapsed() / 1000;
        if (diff > 0) {
            // -bandwidth_limit is the % of bandwidth
            int64_t wait_time = -diff * (1 + 100.0 / bandwidth_limit);
            if (wait_time > 0) {
                usleep(wait_time);
            }
        }
        this_->_lastTime.start();
    }
}


}
