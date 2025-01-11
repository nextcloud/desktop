/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#include "vfs_suffix.h"

#include "syncfileitem.h"
#include "filesystem.h"
#include "common/syncjournaldb.h"

#include <QFile>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcVfsSuffix, "nextcloud.sync.vfs.suffix", QtInfoMsg)

namespace OCC {

VfsSuffix::VfsSuffix(QObject *parent)
    : Vfs(parent)
{
}

VfsSuffix::~VfsSuffix() = default;

Vfs::Mode VfsSuffix::mode() const
{
    return WithSuffix;
}

QString VfsSuffix::fileSuffix() const
{
    return QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX);
}

void VfsSuffix::startImpl(const VfsSetupParams &params)
{
    // It is unsafe for the database to contain any ".owncloud" file entries
    // that are not marked as a virtual file. These could be real .owncloud
    // files that were synced before vfs was enabled.
    QByteArrayList toWipe;
    if (!params.journal->getFilesBelowPath("", [&toWipe](const SyncJournalFileRecord &rec) {
        if (!rec.isVirtualFile() && rec._path.endsWith(APPLICATION_DOTVIRTUALFILE_SUFFIX))
            toWipe.append(rec._path);
    })) {
        qWarning() << "Could not get files below path \"\" from local DB";
    }
    for (const auto &path : toWipe) {
        if (!params.journal->deleteFileRecord(path)) {
            qWarning() << "Failed to delete file record from local DB" << path;
        }
    }
}

void VfsSuffix::stop()
{
}

void VfsSuffix::unregisterFolder()
{
}

bool VfsSuffix::isHydrating() const
{
    return false;
}

Result<void, QString> VfsSuffix::updateMetadata(const QString &filePath, time_t modtime, qint64, const QByteArray &)
{
    if (modtime <= 0) {
        return {tr("Error updating metadata due to invalid modification time")};
    }

    qCDebug(lcVfsSuffix()) << "setModTime" << filePath << modtime;
    FileSystem::setModTime(filePath, modtime);
    return {};
}

Result<void, QString> VfsSuffix::createPlaceholder(const SyncFileItem &item)
{
    if (item._modtime <= 0) {
        return {tr("Error updating metadata due to invalid modification time")};
    }

    // The concrete shape of the placeholder is also used in isDehydratedPlaceholder() below
    QString fn = _setupParams.filesystemPath + item._file;
    if (!fn.endsWith(fileSuffix())) {
        ASSERT(false, "vfs file isn't ending with suffix");
        return QStringLiteral("vfs file isn't ending with suffix");
    }

    QFile file(fn);
    if (file.exists() && file.size() > 1
        && !FileSystem::verifyFileUnchanged(fn, item._size, item._modtime)) {
        return QStringLiteral("Cannot create a placeholder because a file with the placeholder name already exist");
    }

    if (!file.open(QFile::ReadWrite | QFile::Truncate))
        return file.errorString();

    file.write(" ");
    file.close();
    qCDebug(lcVfsSuffix()) << "setModTime" << fn << item._modtime;
    FileSystem::setModTime(fn, item._modtime);
    return {};
}

Result<void, QString> VfsSuffix::dehydratePlaceholder(const SyncFileItem &item)
{
    SyncFileItem virtualItem(item);
    virtualItem._file = item._renameTarget;
    auto r = createPlaceholder(virtualItem);
    if (!r)
        return r;

    if (item._file != item._renameTarget) { // can be the same when renaming foo -> foo.owncloud to dehydrate
        QFile::remove(_setupParams.filesystemPath + item._file);
    }

    // Move the item's pin state
    auto pin = _setupParams.journal->internalPinStates().rawForPath(item._file.toUtf8());
    if (pin && *pin != PinState::Inherited) {
        setPinState(item._renameTarget, *pin);
        setPinState(item._file, PinState::Inherited);
    }

    // Ensure the pin state isn't contradictory
    pin = pinState(item._renameTarget);
    if (pin && *pin == PinState::AlwaysLocal)
        setPinState(item._renameTarget, PinState::Unspecified);
    return {};
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsSuffix::convertToPlaceholder(const QString &, const SyncFileItem &, const QString &, UpdateMetadataTypes)
{
    // Nothing necessary
    return Vfs::ConvertToPlaceholderResult::Ok;
}

bool VfsSuffix::isDehydratedPlaceholder(const QString &filePath)
{
    if (!filePath.endsWith(fileSuffix()))
        return false;
    return FileSystem::fileExists(filePath) && FileSystem::getSize(filePath) == 1;
}

bool VfsSuffix::statTypeVirtualFile(csync_file_stat_t *stat, void *)
{
    if (stat->path.endsWith(fileSuffix().toUtf8())) {
        stat->type = ItemTypeVirtualFile;
        return true;
    }
    return false;
}

bool VfsSuffix::setPinState(const QString &folderPath, PinState state)
{
    qCDebug(lcVfsSuffix) << "setPinState" << folderPath << state;
    return setPinStateInDb(folderPath, state);
}

Vfs::AvailabilityResult VfsSuffix::availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck)
{
    Q_UNUSED(recursiveCheck)
    return availabilityInDb(folderPath);
}

} // namespace OCC
