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

#ifndef OWNCLOUDPROPAGATOR_H
#define OWNCLOUDPROPAGATOR_H

#include "syncfileitem.h"

struct ne_session_s;
struct ne_decompress_s;

namespace Mirall {

class OwncloudPropagator {
    QString _localDir; // absolute path to the local directory. ends with '/'
    QString _remoteDir; // path to the root of the remote. ends with '/'
    ne_session_s *_session;

    bool check_neon_session();


    csync_instructions_e localRemove(const SyncFileItem &);
    csync_instructions_e localMkdir(const SyncFileItem &);
    csync_instructions_e remoteRemove(const SyncFileItem &);
    csync_instructions_e remoteMkdir(const SyncFileItem &);
    csync_instructions_e downloadFile(const SyncFileItem &, bool isConflict = false);
    csync_instructions_e uploadFile(const SyncFileItem &);
    csync_instructions_e remoteRename(const SyncFileItem &);

    void updateMTimeAndETag(const char *uri, time_t);

    /* fetch the error code and string from the session */
    bool updateErrorFromSession(int neon_code = 0);


public:
    OwncloudPropagator(ne_session_s *session, const QString &localDir, const QString &remoteDir)
            : _session(session)
            , _errorCode(CSYNC_ERR_NONE)
            , _localDir(localDir)
            , _remoteDir(remoteDir) {
        if (!localDir.endsWith(QChar('/'))) _localDir+='/';
        if (!remoteDir.endsWith(QChar('/'))) _remoteDir+='/';
    }
    csync_instructions_e  propagate(const SyncFileItem &);
    QString _errorString;

    CSYNC_ERROR_CODE _errorCode;
    int              _httpStatusCode;
    QByteArray _etag;

};

}

#endif
