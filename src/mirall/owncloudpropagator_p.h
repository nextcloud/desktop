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

#pragma once

#include "owncloudpropagator.h"
#include <httpbf.h>
#include <neon/ne_compress.h>
#include <QFile>
#include <qdebug.h>

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


class PropagateLocalRemove : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateLocalRemove (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
};
class PropagateLocalMkdir : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateLocalMkdir (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
};
class PropagateRemoteRemove : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateRemoteRemove (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
};
class PropagateRemoteMkdir : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateRemoteMkdir (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
};
class PropagateLocalRename : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateLocalRename (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
};
class PropagateRemoteRename : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateRemoteRename (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
};

class PropagateUploadFile: public PropagateItemJob {
    Q_OBJECT
public:
    explicit PropagateUploadFile(OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item), _previousFileSize(0) {}
    void start();
private:
    // Log callback for httpbf
    static void _log_callback(const char *func, const char *text, void*)
    {
        qDebug() << "  " << func << text;
    }

    // abort callback for httpbf
    static int _user_want_abort(void *userData)
    {
        return  static_cast<PropagateUploadFile *>(userData)->_propagator->_abortRequested->fetchAndAddRelaxed(0);
    }

    // callback from httpbf when a chunk is finished
    static void chunk_finished_cb(hbf_transfer_s *trans, int chunk, void* userdata);
    static void notify_status_cb(void* userdata, ne_session_status status,
                                 const ne_session_status_info* info);

    qint64 _chunked_done; // amount of bytes already sent with the previous chunks
    qint64 _chunked_total_size; // total size of the whole file
    qint64 _previousFileSize;   // In case the file size has changed during upload, this is the previous one.
};

class PropagateDownloadFile: public PropagateItemJob {
    Q_OBJECT
public:
    explicit PropagateDownloadFile(OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item), _file(0) {}
    void start();
private:
    QFile *_file;
    QScopedPointer<ne_decompress, ScopedPointerHelpers> _decompress;
    QString errorString;
    QByteArray _expectedEtagForResume;

    static int do_not_accept (void *userdata, ne_request *req, const ne_status *st)
    {
        Q_UNUSED(userdata); Q_UNUSED(req); Q_UNUSED(st);
        return 0; // ignore this response
    }

    static int do_not_download_content_reader(void *userdata, const char *buf, size_t len)
    {
        Q_UNUSED(userdata); Q_UNUSED(buf); Q_UNUSED(len);
        return NE_ERROR;
    }

    // neon hooks:
    static int content_reader(void *userdata, const char *buf, size_t len);
    static void install_content_reader( ne_request *req, void *userdata, const ne_status *status );
    static void notify_status_cb(void* userdata, ne_session_status status,
                               const ne_session_status_info* info);
};

}
