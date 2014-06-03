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

#include "propagatorjobs.h"
#include "owncloudpropagator_p.h"
#include "propagator_legacy.h"

#include "utility.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include <httpbf.h>
#include <qfile.h>
#include <qdir.h>
#include <qdiriterator.h>
#include <qtemporaryfile.h>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <qabstractfileengine.h>
#else
#include <qsavefile.h>
#endif
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


namespace Mirall {

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

void PropagateLocalRemove::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QString filename = _propagator->_localDir +  _item._file;
    if( _propagator->localFileNameClash(_item._file)) {
        done(SyncFileItem::NormalError, tr("Could not remove %1 because of a local file name clash")
             .arg(QDir::toNativeSeparators(filename)));
        return;
    }

    if (_item._isDirectory) {
        if (QDir(filename).exists() && !removeRecursively(filename)) {
            done(SyncFileItem::NormalError, tr("Could not remove directory %1")
                 .arg(QDir::toNativeSeparators(filename)));
            return;
        }
    } else {
        QFile file(filename);
        if (file.exists() && !file.remove()) {
            done(SyncFileItem::NormalError, file.errorString());
            return;
        }
    }
    emit progress(_item, 0);
    _propagator->_journal->deleteFileRecord(_item._originalFile, _item._isDirectory);
    _propagator->_journal->commit("Local remove");
    done(SyncFileItem::Success);
}

void PropagateLocalMkdir::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QDir newDir(_propagator->_localDir + _item._file);
    QString newDirStr = QDir::toNativeSeparators(newDir.path());
    if( Utility::fsCasePreserving() && newDir.exists() &&
            _propagator->localFileNameClash(_item._file ) ) {
        qDebug() << "WARN: new directory to create locally already exists!";
        done( SyncFileItem::NormalError, tr("Attention, possible case sensitivity clash with %1").arg(newDirStr) );
        return;
    }
    QDir localDir(_propagator->_localDir);
    if (!localDir.mkpath(_item._file)) {
        done( SyncFileItem::NormalError, tr("could not create directory %1").arg(newDirStr) );
        return;
    }
    done(SyncFileItem::Success);
}

void PropagateRemoteRemove::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QScopedPointer<char, QScopedPointerPodDeleter> uri(
        ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));
    emit progress(_item, 0);
    qDebug() << "** DELETE " << uri.data();
    int rc = ne_delete(_propagator->_session, uri.data());

    QString errorString = QString::fromUtf8(ne_get_error(_propagator->_session));
    int httpStatusCode = errorString.mid(0, errorString.indexOf(QChar(' '))).toInt();
    if( checkForProblemsWithShared(httpStatusCode,
            tr("The file has been removed from a read only share. It was restored.")) ) {
        return;
    }

    /* Ignore the error 404,  it means it is already deleted */
    if (updateErrorFromSession(rc, 0, 404)) {
        return;
    }

    //  Wed, 15 Nov 1995 06:25:24 GMT
    QDateTime dt = QDateTime::currentDateTimeUtc();
    _item._responseTimeStamp = dt.toString("hh:mm:ss");

    _propagator->_journal->deleteFileRecord(_item._originalFile, _item._isDirectory);
    _propagator->_journal->commit("Remote Remove");
    done(SyncFileItem::Success);
}

void PropagateRemoteMkdir::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QScopedPointer<char, QScopedPointerPodDeleter> uri(
        ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));

    int rc = ne_mkcol(_propagator->_session, uri.data());

    /* Special for mkcol: it returns 405 if the directory already exists.
     * Ignore that error */
    //  Wed, 15 Nov 1995 06:25:24 GMT
    QDateTime dt = QDateTime::currentDateTimeUtc();
    _item._responseTimeStamp = dt.toString("hh:mm:ss");

    if( updateErrorFromSession( rc , 0, 405 ) ) {
        return;
    }

    done(SyncFileItem::Success);
}


void PropagateLocalRename::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    // if the file is a file underneath a moved dir, the _item.file is equal
    // to _item.renameTarget and the file is not moved as a result.
    if (_item._file != _item._renameTarget) {
        emit progress(_item, 0);
        qDebug() << "MOVE " << _propagator->_localDir + _item._file << " => " << _propagator->_localDir + _item._renameTarget;
        QFile file(_propagator->_localDir + _item._file);

        if (_propagator->localFileNameClash(_item._renameTarget)) {
            // Fixme: the file that is the reason for the clash could be named here,
            // it would have to come out the localFileNameClash function
            done(SyncFileItem::NormalError, tr( "File %1 can not be renamed to %2 because of a local file name clash")
                 .arg(QDir::toNativeSeparators(_item._file)).arg(QDir::toNativeSeparators(_item._renameTarget)) );
            return;
        }
        if (!file.rename(_propagator->_localDir + _item._file, _propagator->_localDir + _item._renameTarget)) {
            done(SyncFileItem::NormalError, file.errorString());
            return;
        }
    }

    _propagator->_journal->deleteFileRecord(_item._originalFile);

    // store the rename file name in the item.
    _item._file = _item._renameTarget;

    SyncJournalFileRecord record(_item, _propagator->_localDir + _item._renameTarget);
    record._path = _item._renameTarget;

    if (!_item._isDirectory) { // Directory are saved at the end
        _propagator->_journal->setFileRecord(record);
    }
    _propagator->_journal->commit("localRename");


    done(SyncFileItem::Success);
}

void PropagateRemoteRename::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    if (_item._file == _item._renameTarget) {
        // The parents has been renamed already so there is nothing more to do.
    } else if (_item._file == QLatin1String("Shared") ) {
        // Check if it is the toplevel Shared folder and do not propagate it.
        if( QFile::rename(  _propagator->_localDir + _item._renameTarget, _propagator->_localDir + QLatin1String("Shared")) ) {
            done(SyncFileItem::NormalError, tr("This folder must not be renamed. It is renamed back to its original name."));
        } else {
            done(SyncFileItem::NormalError, tr("This folder must not be renamed. Please name it back to Shared."));
        }
        return;
    } else {
        emit progress(_item, 0);

        QScopedPointer<char, QScopedPointerPodDeleter> uri1(ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));
        QScopedPointer<char, QScopedPointerPodDeleter> uri2(ne_path_escape((_propagator->_remoteDir + _item._renameTarget).toUtf8()));
        qDebug() << "MOVE on Server: " << uri1.data() << "->" << uri2.data();

        int rc = ne_move(_propagator->_session, 1, uri1.data(), uri2.data());

        QString errorString = QString::fromUtf8(ne_get_error(_propagator->_session));
        int httpStatusCode = errorString.mid(0, errorString.indexOf(QChar(' '))).toInt();
        if( checkForProblemsWithShared(httpStatusCode,
                tr("The file was renamed but is part of a read only share. The original file was restored."))) {
            return;
        }

        if (updateErrorFromSession(rc)) {
            return;
        }

        if (!updateMTimeAndETag(uri2.data(), _item._modtime))
            return;
    }
    //  Wed, 15 Nov 1995 06:25:24 GMT
    QDateTime dt = QDateTime::currentDateTimeUtc();
    _item._responseTimeStamp = dt.toString("hh:mm:ss");

    _propagator->_journal->deleteFileRecord(_item._originalFile);
    SyncJournalFileRecord record(_item, _propagator->_localDir + _item._renameTarget);
    record._path = _item._renameTarget;

    _propagator->_journal->setFileRecord(record);
    _propagator->_journal->commit("Remote Rename");
    done(SyncFileItem::Success);
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



}
