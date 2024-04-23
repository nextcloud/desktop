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

#include "account.h"
#include "propagatedownloadencrypted.h"
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

#include <ctime>


namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateLocalRemove, "nextcloud.sync.propagator.localremove", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropagateLocalMkdir, "nextcloud.sync.propagator.localmkdir", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropagateLocalRename, "nextcloud.sync.propagator.localrename", QtInfoMsg)

QByteArray localFileIdFromFullId(const QByteArray &id)
{
    return id.left(8);
}

/**
 * The code will update the database in case of error.
 * If everything goes well (no error, returns true), the caller is responsible for removing the entries
 * in the database.  But in case of error, we need to remove the entries from the database of the files
 * that were deleted.
 *
 * \a path is relative to propagator()->_localDir + _item->_file and should start with a slash
 */
bool PropagateLocalRemove::removeRecursively(const QString &path)
{
    QString absolute = propagator()->fullLocalPath(_item->_file + path);
    QStringList errors;
    QList<QPair<QString, bool>> deleted;
    bool success = FileSystem::removeRecursively(
        absolute,
        [&deleted](const QString &path, bool isDir) {
            // by prepending, a folder deletion may be followed by content deletions
            deleted.prepend(qMakePair(path, isDir));
        },
        &errors);

    if (!success) {
        // We need to delete the entries from the database now from the deleted vector.
        // Do it while avoiding redundant delete calls to the journal.
        QString deletedDir;
        foreach (const auto &it, deleted) {
            if (!it.first.startsWith(propagator()->localPath()))
                continue;
            if (!deletedDir.isEmpty() && it.first.startsWith(deletedDir))
                continue;
            if (it.second) {
                deletedDir = it.first;
            }
            if (!propagator()->_journal->deleteFileRecord(it.first.mid(propagator()->localPath().size()), it.second)) {
                qCWarning(lcPropagateLocalRemove) << "Failed to delete file record from local DB" << it.first.mid(propagator()->localPath().size());
            }
        }

        _error = errors.join(", ");
    }
    return success;
}

void PropagateLocalRemove::start()
{
    qCInfo(lcPropagateLocalRemove) << "Start propagate local remove job";

    _moveToTrash = propagator()->syncOptions()._moveFilesToTrash;

    if (propagator()->_abortRequested)
        return;

    const QString filename = propagator()->fullLocalPath(_item->_file);
    qCInfo(lcPropagateLocalRemove) << "Going to delete:" << filename;

    if (propagator()->localFileNameClash(_item->_file)) {
        done(SyncFileItem::FileNameClash, tr("Could not remove %1 because of a local file name clash").arg(QDir::toNativeSeparators(filename)), ErrorCategory::GenericError);
        return;
    }

    QString removeError;
    if (_moveToTrash) {
        if ((QDir(filename).exists() || FileSystem::fileExists(filename))
            && !FileSystem::moveToTrash(filename, &removeError)) {
            done(SyncFileItem::NormalError, removeError, ErrorCategory::GenericError);
            return;
        }
    } else {
        if (_item->isDirectory()) {
            if (QDir(filename).exists() && !removeRecursively(QString())) {
                done(SyncFileItem::NormalError, _error, ErrorCategory::GenericError);
                return;
            }
        } else {
            if (FileSystem::fileExists(filename)
                && !FileSystem::remove(filename, &removeError)) {
                done(SyncFileItem::NormalError, removeError, ErrorCategory::GenericError);
                return;
            }
        }
    }
    propagator()->reportProgress(*_item, 0);
    if (!propagator()->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory())) {
        qCWarning(lcPropagateLocalRename) << "could not delete file from local DB" << _item->_originalFile;
        done(SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(_item->_originalFile), ErrorCategory::GenericError);
        return;
    }
    propagator()->_journal->commit("Local remove");
    done(SyncFileItem::Success, {}, ErrorCategory::NoError);
}

void PropagateLocalMkdir::start()
{
    if (propagator()->_abortRequested)
        return;

    startLocalMkdir();
}

void PropagateLocalMkdir::setDeleteExistingFile(bool enabled)
{
    _deleteExistingFile = enabled;
}

void PropagateLocalMkdir::startLocalMkdir()
{
    QDir newDir(propagator()->fullLocalPath(_item->_file));
    QString newDirStr = QDir::toNativeSeparators(newDir.path());

    // When turning something that used to be a file into a directory
    // we need to delete the file first.
    QFileInfo fi(newDirStr);
    if (fi.exists() && fi.isFile()) {
        if (_deleteExistingFile) {
            QString removeError;
            if (!FileSystem::remove(newDirStr, &removeError)) {
                done(SyncFileItem::NormalError,
                    tr("could not delete file %1, error: %2")
                        .arg(newDirStr, removeError), ErrorCategory::GenericError);
                return;
            }
        } else if (_item->_instruction == CSYNC_INSTRUCTION_CONFLICT) {
            QString error;
            if (!propagator()->createConflict(_item, _associatedComposite, &error)) {
                done(SyncFileItem::SoftError, error, ErrorCategory::GenericError);
                return;
            }
        }
    }

    if (Utility::fsCasePreserving() && propagator()->localFileNameClash(_item->_file)) {
        qCWarning(lcPropagateLocalMkdir) << "New folder to create locally already exists with different case:" << _item->_file;
        done(SyncFileItem::FileNameClash, tr("Folder %1 cannot be created because of a local file or folder name clash!").arg(newDirStr), ErrorCategory::GenericError);
        return;
    }
    emit propagator()->touchedFile(newDirStr);
    QDir localDir(propagator()->localPath());
    if (!localDir.mkpath(_item->_file)) {
        done(SyncFileItem::NormalError, tr("Could not create folder %1").arg(newDirStr), ErrorCategory::GenericError);
        return;
    }

    // Insert the directory into the database. The correct etag will be set later,
    // once all contents have been propagated, because should_update_metadata is true.
    // Adding an entry with a dummy etag to the database still makes sense here
    // so the database is aware that this folder exists even if the sync is aborted
    // before the correct etag is stored.
    SyncFileItem newItem(*_item);
    newItem._etag = "_invalid_";
    const auto result = propagator()->updateMetadata(newItem);
    if (!result) {
        done(SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()), ErrorCategory::GenericError);
        return;
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(newItem._file), ErrorCategory::GenericError);
        return;
    }
    propagator()->_journal->commit("localMkdir");

    auto resultStatus = _item->_instruction == CSYNC_INSTRUCTION_CONFLICT
        ? SyncFileItem::Conflict
        : SyncFileItem::Success;
    done(resultStatus, {}, ErrorCategory::NoError);
}

PropagateLocalRename::PropagateLocalRename(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
    : PropagateItemJob(propagator, item)
{
    qCDebug(lcPropagateLocalRename) << _item->_file << _item->_renameTarget << _item->_originalFile;
}

void PropagateLocalRename::start()
{
    if (propagator()->_abortRequested)
        return;

    auto &vfs = propagator()->syncOptions()._vfs;
    const auto previousNameInDb = propagator()->adjustRenamedPath(_item->_file);
    const auto existingFile = propagator()->fullLocalPath(previousNameInDb);
    const auto targetFile = propagator()->fullLocalPath(_item->_renameTarget);

    const auto fileAlreadyMoved = !QFileInfo::exists(propagator()->fullLocalPath(_item->_originalFile)) && QFileInfo::exists(existingFile);
    auto pinState = OCC::PinState::Unspecified;
    if (!fileAlreadyMoved) {
        auto pinStateResult = vfs->pinState(propagator()->adjustRenamedPath(_item->_file));
        if (pinStateResult) {
            pinState = pinStateResult.get();
        }
    }

    // if the file is a file underneath a moved dir, the _item->file is equal
    // to _item->renameTarget and the file is not moved as a result.
    qCDebug(lcPropagateLocalRename) << _item->_file << _item->_renameTarget << _item->_originalFile << previousNameInDb << (fileAlreadyMoved ? "original file has already moved" : "original file is still there");
    Q_ASSERT(QFileInfo::exists(propagator()->fullLocalPath(_item->_originalFile)) || QFileInfo::exists(existingFile));
    if (_item->_file != _item->_renameTarget) {
        propagator()->reportProgress(*_item, 0);
        qCDebug(lcPropagateLocalRename) << "MOVE " << existingFile << " => " << targetFile;

        if (QString::compare(_item->_file, _item->_renameTarget, Qt::CaseInsensitive) != 0
            && propagator()->localFileNameClash(_item->_renameTarget)) {

            qCInfo(lcPropagateLocalRename) << "renaming a case clashed item" << _item->_file << _item->_renameTarget;
            if (_item->isDirectory()) {
                // #HotFix
                // fix a crash (we can not create a conflicted copy for folders)
                // right now, the conflict resolution will not even work for this scenario with folders,
                // but, let's fix it step by step, this will be a second stage
                done(SyncFileItem::FileNameClash,
                     tr("Folder %1 cannot be renamed because of a local file or folder name clash!").arg(_item->_file),
                     ErrorCategory::GenericError);
                return;
            }
            const auto caseClashConflictResult = propagator()->createCaseClashConflict(_item, existingFile);
            if (caseClashConflictResult) {
                done(SyncFileItem::SoftError, *caseClashConflictResult, ErrorCategory::GenericError);
            } else {
                done(SyncFileItem::FileNameClash, tr("File %1 downloaded but it resulted in a local file name clash!").arg(QDir::toNativeSeparators(_item->_file)), ErrorCategory::GenericError);
            }
            return;
        }

        emit propagator()->touchedFile(existingFile);
        emit propagator()->touchedFile(targetFile);
        if (QString renameError; !FileSystem::rename(existingFile, targetFile, &renameError)) {
            done(SyncFileItem::NormalError, renameError, ErrorCategory::GenericError);
            return;
        }
    }

    SyncJournalFileRecord oldRecord;
    if (!propagator()->_journal->getFileRecord(fileAlreadyMoved ? previousNameInDb : _item->_originalFile, &oldRecord)) {
        qCWarning(lcPropagateLocalRename) << "could not get file from local DB" << _item->_originalFile;
        done(SyncFileItem::NormalError, tr("could not get file %1 from local DB").arg(_item->_originalFile), ErrorCategory::GenericError);
        return;
    }

    if (fileAlreadyMoved && !deleteOldDbRecord(previousNameInDb)) {
        return;
    } else if (!deleteOldDbRecord(_item->_originalFile)) {
        qCWarning(lcPropagateLocalRename) << "could not delete file from local DB" << _item->_originalFile;
        return;
    }

    if (!vfs->setPinState(_item->_renameTarget, pinState)) {
        qCWarning(lcPropagateLocalRename) << "Could not set pin state of" << _item->_renameTarget << "to old value" << pinState;
        done(SyncFileItem::NormalError, tr("Error setting pin state"), ErrorCategory::GenericError);
        return;
    }

    const auto oldFile = _item->_file;

    if (!_item->isDirectory()) { // Directories are saved at the end
        auto newItem(*_item);
        if (oldRecord.isValid()) {
            newItem._checksumHeader = oldRecord._checksumHeader;
        }
        const auto result = propagator()->updateMetadata(newItem);
        if (!result) {
            done(SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()), ErrorCategory::GenericError);
            return;
        } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
            done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(newItem._file), ErrorCategory::GenericError);
            return;
        }
    } else {
        const auto dbQueryResult = propagator()->_journal->getFilesBelowPath(oldFile.toUtf8(), [oldFile, this] (const SyncJournalFileRecord &record) -> void {
            const auto oldFileName = record._path;
            const auto oldFileNameString = QString::fromUtf8(oldFileName);
            auto newFileNameString = oldFileNameString;
            newFileNameString.replace(0, oldFile.length(), _item->_renameTarget);

            if (oldFileNameString == newFileNameString) {
                return;
            }

            SyncJournalFileRecord oldRecord;
            if (!propagator()->_journal->getFileRecord(oldFileName, &oldRecord)) {
                qCWarning(lcPropagateLocalRename) << "could not get file from local DB" << oldFileName;
                done(SyncFileItem::NormalError, tr("could not get file %1 from local DB").arg(oldFileNameString), OCC::ErrorCategory::GenericError);
                return;
            }
            if (!propagator()->_journal->deleteFileRecord(oldFileName)) {
                qCWarning(lcPropagateLocalRename) << "could not delete file from local DB" << oldFileName;
                done(SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(oldFileNameString), OCC::ErrorCategory::GenericError);
                return;
            }

            const auto newItem = SyncFileItem::fromSyncJournalFileRecord(oldRecord);
            newItem->_file = newFileNameString;
            const auto result = propagator()->updateMetadata(*newItem);
            if (!result) {
                done(SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()), OCC::ErrorCategory::GenericError);
                return;
            }
        });
        if (!dbQueryResult) {
            done(SyncFileItem::FatalError, tr("Failed to propagate directory rename in hierarchy"), OCC::ErrorCategory::GenericError);
            return;
        }
        propagator()->_renamedDirectories.insert(oldFile, _item->_renameTarget);
        if (!PropagateRemoteMove::adjustSelectiveSync(propagator()->_journal, oldFile, _item->_renameTarget)) {
            done(SyncFileItem::FatalError, tr("Failed to rename file"), ErrorCategory::GenericError);
            return;
        }
    }
    if (pinState != PinState::Inherited && !vfs->setPinState(_item->_renameTarget, pinState)) {
        done(SyncFileItem::NormalError, tr("Error setting pin state"), ErrorCategory::GenericError);
        return;
    }

    propagator()->_journal->commit("localRename");

    done(SyncFileItem::Success, {}, ErrorCategory::NoError);
}

bool PropagateLocalRename::deleteOldDbRecord(const QString &fileName)
{
    if (SyncJournalFileRecord oldRecord; !propagator()->_journal->getFileRecord(fileName, &oldRecord)) {
        qCWarning(lcPropagateLocalRename) << "could not get file from local DB" << fileName;
        done(SyncFileItem::NormalError, tr("could not get file %1 from local DB").arg(fileName), OCC::ErrorCategory::GenericError);
        return false;
    }
    if (!propagator()->_journal->deleteFileRecord(fileName)) {
        qCWarning(lcPropagateLocalRename) << "could not delete file from local DB" << fileName;
        done(SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(fileName), OCC::ErrorCategory::GenericError);
        return false;
    }

    return true;
}
}
