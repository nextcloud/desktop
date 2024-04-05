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
#include "owncloudpropagator.h"
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

/**
 * The code will update the database in case of error.
 * If everything goes well (no error, returns true), the caller is responsible for removing the entries
 * in the database.  But in case of error, we need to remove the entries from the database of the files
 * that were deleted.
 *
 * \a path is relative to propagator()->_localDir + _item->_file and should start with a slash
 */
bool PropagateLocalRemove::removeRecursively(const QString &absolute)
{
    FileSystem::RemoveEntryList removed;
    FileSystem::RemoveEntryList locked;
    FileSystem::RemoveErrorList errors;
    const bool success = FileSystem::removeRecursively(
        absolute,
        &removed,
        &locked,
        &errors);

    if (!success) {
        // We need to delete the entries from the database now from the deleted vector.
        // Do it while avoiding redundant delete calls to the journal.
        QString deletedDir;
        for (const auto &it : qAsConst(removed)) {
            if (!it.path.startsWith(propagator()->localPath()))
                continue;
            if (!deletedDir.isEmpty() && it.path.startsWith(deletedDir))
                continue;
            if (it.isDir) {
                deletedDir = it.path;
            }
            propagator()->_journal->deleteFileRecord(it.path.mid(propagator()->localPath().size()), it.isDir);
        }
        if (!errors.empty()) {
            QStringList errorList;
            errorList.reserve(errors.size());
            for (const auto &err : errors) {
                errorList.append(tr("%1 failed with: %2").arg(QDir::toNativeSeparators(err.entry.path), err.error));
            }
            done(SyncFileItem::NormalError, errorList.join(QStringLiteral(", ")));
            return false;
        } else if (!locked.empty()) {
            QStringList errorList;
            errorList.reserve(errors.size());
            for (const auto &l : locked) {
                // unlock is handled in hack in `void Folder::slotWatchedPathChanged`
                Q_EMIT propagator()->seenLockedFile(l.path, FileSystem::LockMode::Exclusive);
                errorList.append(tr("%1 the file is currently in use").arg(QDir::toNativeSeparators(l.path)));
            }
            done(SyncFileItem::SoftError, errorList.join(QStringLiteral(", ")));
            return false;
        }
    }
    return success;
}

void PropagateLocalRemove::start()
{
    _moveToTrash = propagator()->syncOptions()._moveFilesToTrash;

    if (propagator()->_abortRequested)
        return;

    const QString filename = propagator()->fullLocalPath(_item->_file);
    qCDebug(lcPropagateLocalRemove) << filename;

    if (auto clash = propagator()->localFileNameClash(_item->_file)) {
        done(SyncFileItem::NormalError, tr("Could not remove %1 because of a local file name clash with %2!").arg(QDir::toNativeSeparators(filename), QDir::toNativeSeparators(clash.get())));
        return;
    }

    if (FileSystem::fileExists(filename)) {
        if (FileSystem::isFileLocked(filename, FileSystem::LockMode::Exclusive)) {
            Q_EMIT propagator()->seenLockedFile(filename, FileSystem::LockMode::Exclusive);
            done(SyncFileItem::SoftError, tr("%1 the file is currently in use").arg(QDir::toNativeSeparators(filename)));
            return;
        }

        bool ok = false;
        QString removeError;
        if (_moveToTrash) {
            ok = QFile(filename).moveToTrash();
            if (!ok) {
                removeError = tr("Could not move '%1' to the trash bin").arg(filename);
            }
        } else {
            if (_item->isDirectory()) {
                // removeRecursively will call done on error
                if (!(ok = removeRecursively(filename))) {
                    return;
                }
            } else {
                ok = FileSystem::remove(filename, &removeError);
            }
        }
        if (!ok) {
            done(SyncFileItem::NormalError, removeError);
            return;
        }
    }
    propagator()->reportProgress(*_item, 0);
    propagator()->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory());
    propagator()->_journal->commit(QStringLiteral("Local remove"));
    done(SyncFileItem::Success);
}

void PropagateLocalMkdir::start()
{
    if (propagator()->_abortRequested)
        return;

    QDir newDir(propagator()->fullLocalPath(_item->_file));
    QString newDirStr = QDir::toNativeSeparators(newDir.path());

    // When turning something that used to be a file into a directory
    // we need to delete the file first.
    QFileInfo fi(newDirStr);
    if (fi.exists() && fi.isFile()) {
        if (_deleteExistingFile) {
            qDebug(lcPropagateLocalMkdir) << "deleting existing file" << newDirStr << "to replace it with a dir";
            QString removeError;
            if (!FileSystem::remove(newDirStr, &removeError)) {
                done(SyncFileItem::NormalError,
                    tr("could not delete file %1, error: %2")
                        .arg(newDirStr, removeError));
                return;
            }
        } else if (_item->instruction() == CSYNC_INSTRUCTION_CONFLICT) {
            QString error;
            if (!propagator()->createConflict(_item, _associatedComposite, &error)) {
                done(SyncFileItem::SoftError, error);
                return;
            }
        }
    }

    if (auto clash = propagator()->localFileNameClash(_item->_file)) {
        qCWarning(lcPropagateLocalMkdir) << "New folder to create locally already exists with different case:" << _item->_file;
        done(SyncFileItem::NormalError, tr("Can not create local folder %1 because of a local file name clash with %2").arg(newDirStr, QDir::toNativeSeparators(clash.get())));
        return;
    }
    QDir localDir(propagator()->localPath());
    if (!localDir.mkpath(_item->_file)) {
        done(SyncFileItem::NormalError, tr("could not create folder %1").arg(newDirStr));
        return;
    }

    // Insert the directory into the database. The correct etag will be set later,
    // once all contents have been propagated, because should_update_metadata is true.
    // Adding an entry with a dummy etag to the database still makes sense here
    // so the database is aware that this folder exists even if the sync is aborted
    // before the correct etag is stored.
    SyncFileItem newItem(*_item);
    newItem._etag = QStringLiteral("_invalid_");
    const auto result = propagator()->updateMetadata(newItem);
    if (!result) {
        done(SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()));
        return;
    } else if (result.get() == Vfs::ConvertToPlaceholderResult::Locked) {
        done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(newItem._file));
        return;
    }
    propagator()->_journal->commit(QStringLiteral("localMkdir"));

    auto resultStatus = _item->instruction() == CSYNC_INSTRUCTION_CONFLICT ? SyncFileItem::Conflict : SyncFileItem::Success;
    done(resultStatus);
}

void PropagateLocalMkdir::setDeleteExistingFile(bool enabled)
{
    _deleteExistingFile = enabled;
}

void PropagateLocalRename::start()
{
    if (propagator()->_abortRequested)
        return;

    QString existingFile = propagator()->fullLocalPath(propagator()->adjustRenamedPath(_item->_file));
    QString targetFile = propagator()->fullLocalPath(_item->_renameTarget);

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
                    .arg(QDir::toNativeSeparators(_item->_file),
                        QDir::toNativeSeparators(_item->_renameTarget)));
            return;
        }
        if (FileSystem::isFileLocked(existingFile, FileSystem::LockMode::Exclusive)) {
            Q_EMIT propagator()->seenLockedFile(existingFile, FileSystem::LockMode::Exclusive);
            done(SyncFileItem::SoftError, tr("Could not rename %1 to %2, the file is currently in use").arg(existingFile, targetFile));
            return;
        }
        QString renameError;
        if (!FileSystem::rename(existingFile, targetFile, &renameError)) {
            done(SyncFileItem::NormalError, renameError);
            return;
        }
    }

    SyncJournalFileRecord oldRecord;
    propagator()->_journal->getFileRecord(_item->_originalFile, &oldRecord);
    propagator()->_journal->deleteFileRecord(_item->_originalFile);

    auto &vfs = propagator()->syncOptions()._vfs;
    auto pinState = vfs->pinState(_item->_originalFile);
    std::ignore = vfs->setPinState(_item->_originalFile, PinState::Inherited);

    const auto oldFile = _item->_file;

    if (!_item->isDirectory()) { // Directories are saved at the end
        SyncFileItem newItem(*_item);
        if (oldRecord.isValid()) {
            newItem._checksumHeader = oldRecord._checksumHeader;
        }
        const auto result = propagator()->updateMetadata(newItem);
        if (!result) {
            done(SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()));
            return;
        } else if (result.get() == Vfs::ConvertToPlaceholderResult::Locked) {
            done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(newItem._file));
            return;
        }
    } else {
        propagator()->_renamedDirectories.insert(oldFile, _item->_renameTarget);
        if (!PropagateRemoteMove::adjustSelectiveSync(propagator()->_journal, oldFile, _item->_renameTarget)) {
            done(SyncFileItem::FatalError, tr("Failed to rename file"));
            return;
        }
    }
    if (pinState && *pinState != PinState::Inherited
        && !vfs->setPinState(_item->_renameTarget, *pinState)) {
        done(SyncFileItem::NormalError, tr("Error setting pin state"));
        return;
    }

    propagator()->_journal->commit(QStringLiteral("localRename"));

    done(SyncFileItem::Success);
}
}
