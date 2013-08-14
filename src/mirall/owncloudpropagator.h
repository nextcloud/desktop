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

#include <neon/ne_request.h>
#include <QHash>
#include <QObject>
#include <qelapsedtimer.h>

#include "syncfileitem.h"
#include "progressdispatcher.h"

struct ne_session_s;
struct ne_decompress_s;

namespace Mirall {

class ProgressDatabase;

class OwncloudPropagator : public QObject {
    Q_OBJECT

    ne_session_s *_session;
    QString _localDir; // absolute path to the local directory. ends with '/'
    QString _remoteDir; // path to the root of the remote. ends with '/'
    ProgressDatabase *_progressDb;

    QString          _errorString;
    CSYNC_ERROR_CODE _errorCode;
    int              _httpStatusCode;
    csync_instructions_e _instruction;

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
    bool updateErrorFromSession(int neon_code = 0, ne_request *req = NULL);


    QElapsedTimer _lastTime;
    quint64 _lastProgress;
    quint64 _chunked_total_size;
    quint64 _chunked_done;
    QString _currentFile;

    static void notify_status_cb (void *userdata, ne_session_status status,
                                  const ne_session_status_info *info);

public:
    OwncloudPropagator(ne_session_s *session, const QString &localDir, const QString &remoteDir,
                       ProgressDatabase *progressDb)
            : _session(session)
            , _localDir(localDir)
            , _remoteDir(remoteDir)
            , _progressDb(progressDb)
            , _errorCode(CSYNC_ERR_NONE)
            , _httpStatusCode(0)
            , _hasFatalError(false)
    {
        if (!localDir.endsWith(QChar('/'))) _localDir+='/';
        if (!remoteDir.endsWith(QChar('/'))) _remoteDir+='/';
    }
    void  propagate(const SyncFileItem &);
    QByteArray _etag;
    bool             _hasFatalError;

    int _downloadLimit;
    int _uploadLimit;

signals:
    void completed(const SyncFileItem &, CSYNC_ERROR_CODE);
    void progress(Progress::Kind, const QString &filename, quint64 bytes, quint64 total);

};

}

#endif
