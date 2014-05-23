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

/*
 * Abstract class for neon job.  Lives in the neon thread
 */
class PropagateNeonJob : public PropagateItemJob {
    Q_OBJECT
protected:

    /* Issue a PROPPATCH and PROPFIND to update the mtime, and fetch the etag
     * Return true in case of success, and false if the PROPFIND failed and the
     * error has been reported
     */
    bool updateMTimeAndETag(const char *uri, time_t);

    /* fetch the error code and string from the session
       in case of error, calls done with the error and returns true.

       If the HTTP error code is ignoreHTTPError,  the error is ignored
     */
    bool updateErrorFromSession(int neon_code = 0, ne_request *req = 0, int ignoreHTTPError = 0);

    /*
     * to be called by the progress callback and will wait the amount of time needed.
     */
    void limitBandwidth(qint64 progress, qint64 limit);

    QElapsedTimer _lastTime;
    qint64        _lastProgress;
    int           _httpStatusCode;

public:
    PropagateNeonJob(OwncloudPropagator* propagator, const SyncFileItem &item)
        : PropagateItemJob(propagator, item), _lastProgress(0), _httpStatusCode(0) {
            moveToThread(propagator->_neonThread);
        }

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
class PropagateRemoteRemove : public PropagateNeonJob {
    Q_OBJECT
public:
    PropagateRemoteRemove (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateNeonJob(propagator, item) {}
    void start();
};
class PropagateRemoteMkdir : public PropagateNeonJob {
    Q_OBJECT
public:
    PropagateRemoteMkdir (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateNeonJob(propagator, item) {}
    void start();
};
class PropagateLocalRename : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateLocalRename (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
};
class PropagateRemoteRename : public PropagateNeonJob {
    Q_OBJECT
public:
    PropagateRemoteRename (OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateNeonJob(propagator, item) {}
    void start();
};


// To support older owncloud in the
class UpdateMTimeAndETagJob : public PropagateNeonJob{
    Q_OBJECT
public:
    UpdateMTimeAndETagJob (OwncloudPropagator* propagator, const SyncFileItem& item)  : PropagateNeonJob(propagator, item) {}
    void start();
};


}
