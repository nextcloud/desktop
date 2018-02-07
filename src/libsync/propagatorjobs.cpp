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
#include "propagateremotemove.h"
#include "common/utility.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "filesystem.h"
#include <qfile.h>
#include <qdir.h>
#include <qdiriterator.h>
#include <qtemporaryfile.h>
#include <qsavefile.h>
#include <QDateTime>
#include <qstack.h>
#include <QCoreApplication>

#include <time.h>


namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateLocalRemove, "sync.propagator.localremove", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropagateLocalMkdir, "sync.propagator.localmkdir", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropagateLocalRename, "sync.propagator.localrename", QtInfoMsg)

QByteArray localFileIdFromFullId(const QByteArray &id)
{
    return id.left(8);
}

/**
 * Code inspired from Qt5's QDir::removeRecursively
 * The code will update the database in case of error.
 * If everything goes well (no error, returns true), the caller is responsible for removing the entries
 * in the database.  But in case of error, we need to remove the entries from the database of the files
 * that were deleted.
 *
 * \a path is relative to propagator()->_localDir + _item->_file and should start with a slash
 */
bool PropagateLocalRemove::removeRecursively(const QString &path)
{
    bool success = true;
    QString absolute = propagator()->_localDir + _item->_file + path;
    QDirIterator di(absolute, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);

    QVector<QPair<QString, bool>> deleted;

    while (di.hasNext()) {
        di.next();
        const QFileInfo &fi = di.fileInfo();
        bool ok;
        // The use of isSymLink here is okay:
        // we never want to go into this branch for .lnk files
        bool isDir = fi.isDir() && !fi.isSymLink() && !FileSystem::isJunction(fi.absoluteFilePath());
        if (isDir) {
            ok = removeRecursively(path + QLatin1Char('/') + di.fileName()); // recursive
        } else {
            QString removeError;
            ok = FileSystem::remove(di.filePath(), &removeError);
            if (!ok) {
                _error += PropagateLocalRemove::tr("Error removing '%1': %2;").arg(QDir::toNativeSeparators(di.filePath()), removeError) + " ";
                qCWarning(lcPropagateLocalRemove) << "Error removing " << di.filePath() << ':' << removeError;
            }
        }
        if (success && !ok) {
            // We need to delete the entries from the database now from the deleted vector
            foreach (const auto &it, deleted) {
                propagator()->_journal->deleteFileRecord(_item->_originalFile + path + QLatin1Char('/') + it.first,
                    it.second);
            }
            success = false;
            deleted.clear();
        }
        if (success) {
            deleted.append(qMakePair(di.fileName(), isDir));
        }
        if (!success && ok) {
            // This succeeded, so we need to delete it from the database now because the caller won't
            propagator()->_journal->deleteFileRecord(_item->_originalFile + path + QLatin1Char('/') + di.fileName(),
                isDir);
        }
    }
    if (success) {
        success = QDir().rmdir(absolute);
        if (!success) {
            _error += PropagateLocalRemove::tr("Could not remove folder '%1'")
                          .arg(QDir::toNativeSeparators(absolute))
                + " ";
            qCWarning(lcPropagateLocalRemove) << "Error removing folder" << absolute;
        }
    }
    return success;
}

void PropagateLocalRemove::start()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QString filename = propagator()->_localDir + _item->_file;

    qCDebug(lcPropagateLocalRemove) << filename;

    if (propagator()->localFileNameClash(_item->_file)) {
        done(SyncFileItem::NormalError, tr("Could not remove %1 because of a local file name clash").arg(QDir::toNativeSeparators(filename)));
        return;
    }

    if (_item->isDirectory()) {
        if (QDir(filename).exists() && !removeRecursively(QString())) {
            done(SyncFileItem::NormalError, _error);
            return;
        }
    } else {
        QString removeError;
        if (FileSystem::fileExists(filename)
            && !FileSystem::remove(filename, &removeError)) {
            done(SyncFileItem::NormalError, removeError);
            return;
        }
    }
    propagator()->reportProgress(*_item, 0);
    propagator()->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory());
    propagator()->_journal->commit("Local remove");
    done(SyncFileItem::Success);
}

void PropagateLocalMkdir::start()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QDir newDir(propagator()->getFilePath(_item->_file));
    QString newDirStr = QDir::toNativeSeparators(newDir.path());

    // When turning something that used to be a file into a directory
    // we need to delete the file first.
    QFileInfo fi(newDirStr);
    if (_deleteExistingFile && fi.exists() && fi.isFile()) {
        QString removeError;
        if (!FileSystem::remove(newDirStr, &removeError)) {
            done(SyncFileItem::NormalError,
                tr("could not delete file %1, error: %2")
                    .arg(newDirStr, removeError));
            return;
        }
    }

    if (Utility::fsCasePreserving() && propagator()->localFileNameClash(_item->_file)) {
        qCWarning(lcPropagateLocalMkdir) << "New folder to create locally already exists with different case:" << _item->_file;
        done(SyncFileItem::NormalError, tr("Attention, possible case sensitivity clash with %1").arg(newDirStr));
        return;
    }
    emit propagator()->touchedFile(newDirStr);
    QDir localDir(propagator()->_localDir);
    if (!localDir.mkpath(_item->_file)) {
        done(SyncFileItem::NormalError, tr("could not create folder %1").arg(newDirStr));
        return;
    }

    // Insert the directory into the database. The correct etag will be set later,
    // once all contents have been propagated, because should_update_metadata is true.
    // Adding an entry with a dummy etag to the database still makes sense here
    // so the database is aware that this folder exists even if the sync is aborted
    // before the correct etag is stored.
    SyncJournalFileRecord record = _item->toSyncJournalFileRecordWithInode(newDirStr);
    record._etag = "_invalid_";
    if (!propagator()->_journal->setFileRecord(record)) {
        done(SyncFileItem::FatalError, tr("Error writing metadata to the database"));
        return;
    }
    propagator()->_journal->commit("localMkdir");

    done(SyncFileItem::Success);
}

void PropagateLocalMkdir::setDeleteExistingFile(bool enabled)
{
    _deleteExistingFile = enabled;
}

void PropagateLocalRename::start()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    QString existingFile = propagator()->getFilePath(_item->_file);
    QString targetFile = propagator()->getFilePath(_item->_renameTarget);

    // if the file is a file underneath a moved dir, the _item->file is equal
    // to _item->renameTarget and the file is not moved as a result.
    if (_item->_file != _item->_renameTarget) {
        propagator()->reportProgress(*_item, 0);
        qCDebug(lcPropagateLocalRename) << "MOVE " << existingFile << " => " << targetFile;

        if (QString::compare(_item->_file, _item->_renameTarget, Qt::CaseInsensitive) != 0
            && propagator()->localFileNameClash(_item->_renameTarget)) {
            // Only use localFileNameClash for the destination if we know that the source was not
            // the one conflicting  (renaming  A.txt -> a.txt is OK)

            // Fixme: the file that is the reason for the clash could be named here,
            // it would have to come out the localFileNameClash function
            done(SyncFileItem::NormalError,
                tr("File %1 can not be renamed to %2 because of a local file name clash")
                    .arg(QDir::toNativeSeparators(_item->_file))
                    .arg(QDir::toNativeSeparators(_item->_renameTarget)));
            return;
        }

        emit propagator()->touchedFile(existingFile);
        emit propagator()->touchedFile(targetFile);
        QString renameError;
        if (!FileSystem::rename(existingFile, targetFile, &renameError)) {
            done(SyncFileItem::NormalError, renameError);
            return;
        }
    }

    SyncJournalFileRecord oldRecord;
    propagator()->_journal->getFileRecord(_item->_originalFile, &oldRecord);
    propagator()->_journal->deleteFileRecord(_item->_originalFile);

    // store the rename file name in the item.
    const auto oldFile = _item->_file;
    _item->_file = _item->_renameTarget;

    SyncJournalFileRecord record = _item->toSyncJournalFileRecordWithInode(targetFile);
    record._path = _item->_renameTarget.toUtf8();
    if (oldRecord.isValid()) {
        record._checksumHeader = oldRecord._checksumHeader;
    }

    if (!_item->isDirectory()) { // Directories are saved at the end
        if (!propagator()->_journal->setFileRecord(record)) {
            done(SyncFileItem::FatalError, tr("Error writing metadata to the database"));
            return;
        }
    } else {
        if (!PropagateRemoteMove::adjustSelectiveSync(propagator()->_journal, oldFile, _item->_renameTarget)) {
            done(SyncFileItem::FatalError, tr("Error writing metadata to the database"));
            return;
        }
    }

    propagator()->_journal->commit("localRename");

    done(SyncFileItem::Success);
}
}
