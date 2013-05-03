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
#include <httpbf.h>
#include <qfile.h>
#include <qdir.h>
#include <qdiriterator.h>
#include <qtemporaryfile.h>
#include <qabstractfileengine.h>

#include <neon/ne_basic.h>
#include <neon/ne_socket.h>
#include <neon/ne_session.h>
#include <neon/ne_request.h>
#include <neon/ne_props.h>
#include <neon/ne_auth.h>
#include <neon/ne_dates.h>
#include <neon/ne_compress.h>
#include <neon/ne_redirect.h>

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

csync_instructions_e  OwncloudPropagator::propagate(const SyncFileItem &item)
{
    switch(item._instruction) {
        case CSYNC_INSTRUCTION_REMOVE:
            return item._dir == SyncFileItem::Down ? localRemove(item) : remoteRemove(item);
        case CSYNC_INSTRUCTION_NEW:
            if (item._isDirectory) {
                return item._dir == SyncFileItem::Down ? localMkdir(item) : remoteMkdir(item);
            }   //fall trough
        case CSYNC_INSTRUCTION_SYNC:
            if (item._isDirectory) {
                return CSYNC_INSTRUCTION_UPDATED; //FIXME
            } else {
                return item._dir == SyncFileItem::Down ? downloadFile(item) : uploadFile(item);
            }
        case CSYNC_INSTRUCTION_CONFLICT:
            return downloadFile(item);
        case CSYNC_INSTRUCTION_RENAME:
            return remoteRename(item);
        default:
            return item._instruction;
    }
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
        errorString = file.errorString();
    }
    //FIXME: we should update the md5
    etag.clear();
    //FIXME: we should update the mtime
    return CSYNC_INSTRUCTION_NONE; // not ERROR so it is still written to the database
}

csync_instructions_e OwncloudPropagator::localMkdir(const SyncFileItem &item)
{
    QDir d;
    if (!d.mkpath(_localDir + item._file)) {
        //FIXME: errorString
        return CSYNC_INSTRUCTION_ERROR;
    }
    return CSYNC_INSTRUCTION_UPDATED;
}

csync_instructions_e OwncloudPropagator::remoteRemove(const SyncFileItem &item)
{
    QScopedPointer<char, QScopedPointerPodDeleter> uri(ne_path_escape((_remoteDir + item._file).toUtf8()));
    int rc = ne_delete(_session, uri.data());
    if (rc != NE_OK) {
        updateErrorFromSession();
        return CSYNC_INSTRUCTION_ERROR;
    }
    return CSYNC_INSTRUCTION_DELETED;
}

csync_instructions_e OwncloudPropagator::remoteMkdir(const SyncFileItem &item)
{
    QScopedPointer<char, QScopedPointerPodDeleter> uri(ne_path_escape((_remoteDir + item._file).toUtf8()));
    int rc = ne_mkcol(_session, uri.data());
    if (rc != NE_OK) {
        updateErrorFromSession();
        /* Special for mkcol: it returns 405 if the directory already exists.
         * Ignre that error */
        if (errorCode != 405)
            return CSYNC_INSTRUCTION_ERROR;
    }
    return CSYNC_INSTRUCTION_UPDATED;
}


csync_instructions_e OwncloudPropagator::uploadFile(const SyncFileItem &item)
{
    QFile file(_localDir + item._file);
    if (!file.open(QIODevice::ReadOnly)) {
        errorString = file.errorString();
        return CSYNC_INSTRUCTION_ERROR;
    }
    QScopedPointer<char, QScopedPointerPodDeleter> uri(ne_path_escape((_remoteDir + item._file).toUtf8()));

    //TODO: FIXME: check directory

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

        //FIXME TODO
#if 0
        /* Reuse chunk info that was stored in database if existing. */
        if (dav_session.chunk_info && dav_session.chunk_info->transfer_id) {
            DEBUG_WEBDAV("Existing chunk info %d %d ", dav_session.chunk_info->start_id, dav_session.chunk_info->transfer_id);
            trans->start_id = dav_session.chunk_info->start_id;
            trans->transfer_id = dav_session.chunk_info->transfer_id;
        }

        if (state == HBF_SUCCESS && _progresscb) {
            ne_set_notifier(dav_session.ctx, ne_notify_status_cb, write_ctx);
            _progresscb(write_ctx->url, CSYNC_NOTIFY_START_UPLOAD, 0 , 0, dav_session.userdata);
        }
#endif
        if( state == HBF_SUCCESS ) {
            //chunked_total_size = trans->stat_size;
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
                errorString = hbf_error_string(state);
//                 errorCode = hbf_fail_http_code(trans);
//                 if (dav_session.chunk_info) {
//                     dav_session.chunk_info->start_id = trans->start_id;
//                     dav_session.chunk_info->transfer_id = trans->transfer_id;
//                 }
                return CSYNC_INSTRUCTION_ERROR;
            }
        }
    } while( !finished );

//       if (_progresscb) {
//         ne_set_notifier(dav_session.ctx, 0, 0);
//         _progresscb(write_ctx->url, rc != 0 ? CSYNC_NOTIFY_ERROR :
//                                               CSYNC_NOTIFY_FINISHED_UPLOAD, error_code,
//                     (long long)(error_string), dav_session.userdata);
//       }

    //Update mtime;


    QByteArray modtime = QByteArray::number(qlonglong(item._modtime));
    ne_propname pname;
    pname.nspace = "DAV:";
    pname.name = "lastmodified";
    ne_proppatch_operation ops[2];
    ops[0].name = &pname;
    ops[0].type = ne_propset;
    ops[0].value = modtime.constData();
    ops[1].name = NULL;

    int rc = ne_proppatch( _session, uri.data(), ops );
    if( rc != NE_OK ) {
        //FIXME
    }

    // get the etag
    QScopedPointer<ne_request, ScopedPointerHelpers> req(ne_request_create(_session, "HEAD", uri.data()));
    int neon_stat = ne_request_dispatch(req.data());
    if (neon_stat != NE_OK) {
        //FIXME
    } else {
        const char *header = ne_get_response_header(req.data(), "etag");
        if(header && header [0] == '"' && header[ strlen(header)-1] == '"') {
            etag = QByteArray(header + 1, strlen(header)-2);
        } else {
            etag = header;
        }
    }

    return CSYNC_INSTRUCTION_UPDATED;
}

struct DownloadContext {

    QFile *file;
    QScopedPointer<ne_decompress, ScopedPointerHelpers> decompress;

    explicit DownloadContext(QFile *file) : file(file) {}

    static int content_reader(void *userdata, const char *buf, size_t len)
    {
        DownloadContext *writeCtx = reinterpret_cast<DownloadContext *>(userdata);
        size_t written = 0;

        if(buf) {
            written = writeCtx->file->write(buf, len);
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
        DownloadContext *writeCtx = reinterpret_cast<DownloadContext *>(userdata);

        Q_UNUSED(status);

        if( !writeCtx ) {
            qDebug("Error: install_content_reader called without valid write context!");
            return;
        }

        const char *enc = ne_get_response_header( req, "Content-Encoding" );
        qDebug("Content encoding ist <%s> with status %d", enc ? enc : "empty",
                    status ? status->code : -1 );

        if( enc == QLatin1String("gzip") ) {
            writeCtx->decompress.reset(ne_decompress_reader( req, ne_accept_2xx,
                                                             content_reader,     /* reader callback */
                                                             writeCtx ));  /* userdata        */
        } else {
            ne_add_response_body_reader( req, ne_accept_2xx,
                                        content_reader,
                                        (void*) writeCtx );
        }

        //enc = ne_get_response_header( req, "ETag" );
        //FIXME: save ETAG
//         if (enc && *enc) {
//             SAFE_FREE(_id_cache.uri);
//             SAFE_FREE(_id_cache.id);
//             _id_cache.uri = c_strdup(writeCtx->url);
//             _id_cache.id = c_strdup(enc);
//         }
    }
};

csync_instructions_e OwncloudPropagator::downloadFile(const SyncFileItem &item)
{
    QTemporaryFile tmpFile(_localDir + item._file);
    if (!tmpFile.open()) {
        errorString = tmpFile.errorString();
        return CSYNC_INSTRUCTION_ERROR;
    }

    /* actually do the request */
    int retry = 0;
//     if (_progresscb) {
//         ne_set_notifier(dav_session.ctx, ne_notify_status_cb, write_ctx);
//         _progresscb(write_ctx->url, CSYNC_NOTIFY_START_DOWNLOAD, 0 , 0, dav_session.userdata);
//     }

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

        int neon_stat = ne_request_dispatch(req.data());

        if (neon_stat == NE_TIMEOUT && (++retry) < 3)
            continue;

        /* delete the hook again, otherwise they get chained as they are with the session */
        ne_unhook_post_headers( _session, DownloadContext::install_content_reader, &writeCtx );
//         ne_set_notifier(_session, 0, 0);

        /* possible return codes are:
         *  NE_OK, NE_AUTH, NE_CONNECT, NE_TIMEOUT, NE_ERROR (from ne_request.h)
         */
        if( neon_stat != NE_OK ) {

            updateErrorFromSession(neon_stat);
            qDebug("Error GET: Neon: %d", neon_stat);
            return CSYNC_INSTRUCTION_ERROR;
        } else {
            const ne_status *status = ne_get_status( req.data() );
            qDebug("GET http result %d (%s)", status->code, status->reason_phrase ? status->reason_phrase : "<empty");
            if( status->klass != 2 ) {
                qDebug("sendfile request failed with http status %d!", status->code);
                errorCode = status->code;
                errorString = QString::fromUtf8(status->reason_phrase);
                return CSYNC_INSTRUCTION_ERROR;
            }
        }

        break;
    } while (1);


    tmpFile.close();
    tmpFile.flush();

    if (!tmpFile.fileEngine()->rename(_localDir + item._file)) {
        errorString = tmpFile.errorString();
        return CSYNC_INSTRUCTION_ERROR;
    }

    tmpFile.setAutoRemove(false);

    return CSYNC_INSTRUCTION_UPDATED;
}



csync_instructions_e OwncloudPropagator::remoteRename(const SyncFileItem& )
{
    qFatal("unimplemented");
    return CSYNC_INSTRUCTION_ERROR;
}

void OwncloudPropagator::updateErrorFromSession(int neon_code)
{
    qFatal("unimplemented");
    //don't forget to update errorCode to the http code
}



}