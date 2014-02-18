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

#ifdef Q_OS_WIN
#include <windef.h>
#include <winbase.h>
#endif

#include <time.h>


namespace Mirall {

/**
 * For delete or remove, check that we are not removing from a shared directory.
 * If we are, try to restore the file
 *
 * Return true if the problem is handled.
 */
bool PropagateNeonJob::checkForProblemsWithShared()
{
    QString errorString = QString::fromUtf8(ne_get_error(_propagator->_session));
    int httpStatusCode = errorString.mid(0, errorString.indexOf(QChar(' '))).toInt();

    if( httpStatusCode == 403 && _propagator->isInSharedDirectory(_item._file )) {
        if( _item._type != SyncFileItem::Directory ) {
            // the file was removed locally from a read only Shared sync
            // the file is gone locally and it should be recovered.
            SyncFileItem downloadItem(_item);
            downloadItem._instruction = CSYNC_INSTRUCTION_SYNC;
            downloadItem._dir = SyncFileItem::Down;
            _restoreJob.reset(new PropagateDownloadFileLegacy(_propagator, downloadItem));
        } else {
            // Directories are harder to recover.
            // But just re-create the directory, next sync will be able to recover the files
            SyncFileItem mkdirItem(_item);
            mkdirItem._instruction = CSYNC_INSTRUCTION_SYNC;
            mkdirItem._dir = SyncFileItem::Down;
            _restoreJob.reset(new PropagateLocalMkdir(_propagator, mkdirItem));
            // Also remove the inodes and fileid from the db so no further renames are tried for
            // this item.
            _propagator->_journal->avoidRenamesOnNextSync(_item._file);
        }
        connect(_restoreJob.data(), SIGNAL(completed(SyncFileItem)),
                this, SLOT(slotRestoreJobCompleted(SyncFileItem)));
        QMetaObject::invokeMethod(_restoreJob.data(), "start");
        return true;
    }
    return false;
}

void PropagateNeonJob::slotRestoreJobCompleted(const SyncFileItem& item )
{
    if( item._status == SyncFileItem::Success ) {
        done( SyncFileItem::SoftError, tr("The file was removed from a read only share. The file has been restored."));
    } else {
        done( item._status, tr("A file was removed from a read only share, but restoring failed: %1").arg(item._errorString) );
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

void PropagateLocalRemove::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QString filename = _propagator->_localDir +  _item._file;
    if (_item._isDirectory) {
        if (QDir(filename).exists() && !removeRecursively(filename)) {
            done(SyncFileItem::NormalError, tr("Could not remove directory %1").arg(filename));
            return;
        }
    } else {
        QFile file(filename);
        if (file.exists() && !file.remove()) {
            done(SyncFileItem::NormalError, file.errorString());
            return;
        }
    }
    emit progress(Progress::StartDelete, _item, 0, _item._size);
    _propagator->_journal->deleteFileRecord(_item._originalFile, _item._isDirectory);
    _propagator->_journal->commit("Local remove");
    done(SyncFileItem::Success);
    emit progress(Progress::EndDelete, _item, _item._size, _item._size);
}

void PropagateLocalMkdir::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QDir d;
    if (!d.mkpath(_propagator->_localDir +  _item._file)) {
        done(SyncFileItem::NormalError, tr("could not create directory %1").arg(_propagator->_localDir +  _item._file));
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
    emit progress(Progress::StartDelete, _item, 0, _item._size);
    qDebug() << "** DELETE " << uri.data();
    int rc = ne_delete(_propagator->_session, uri.data());

    if( checkForProblemsWithShared() ) {
        return;
    }

    /* Ignore the error 404,  it means it is already deleted */
    if (updateErrorFromSession(rc, 0, 404)) {
        return;
    }

    _propagator->_journal->deleteFileRecord(_item._originalFile, _item._isDirectory);
    _propagator->_journal->commit("Remote Remove");
    done(SyncFileItem::Success);
    emit progress(Progress::EndDelete, _item, _item._size, _item._size);
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
    if( updateErrorFromSession( rc , 0, 405 ) ) {
        return;
    }
    done(SyncFileItem::Success);
}


}
