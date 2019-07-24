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

#include <QFile>

#include "syncfileitem.h"
#include "filesystem.h"
#include "common/syncjournaldb.h"

namespace OCC {

VfsSuffix::VfsSuffix(QObject *parent)
    : Vfs(parent)
{
}

VfsSuffix::~VfsSuffix()
{
}

Vfs::Mode VfsSuffix::mode() const
{
    return WithSuffix;
}

QString VfsSuffix::fileSuffix() const
{
    return QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX);
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

bool VfsSuffix::updateMetadata(const QString &filePath, time_t modtime, qint64, const QByteArray &, QString *)
{
    FileSystem::setModTime(filePath, modtime);
    return true;
}

void VfsSuffix::createPlaceholder(const SyncFileItem &item)
{
    // The concrete shape of the placeholder is also used in isDehydratedPlaceholder() below
    QString fn = _setupParams.filesystemPath + item._file;
    if (!fn.endsWith(fileSuffix())) {
        ASSERT(false, "vfs file isn't ending with suffix");
        return;
    }

    QFile file(fn);
    file.open(QFile::ReadWrite | QFile::Truncate);
    file.write(" ");
    file.close();
    FileSystem::setModTime(fn, item._modtime);
}

void VfsSuffix::dehydratePlaceholder(const SyncFileItem &item)
{
    QFile::remove(_setupParams.filesystemPath + item._file);
    SyncFileItem virtualItem(item);
    virtualItem._file = item._renameTarget;
    createPlaceholder(virtualItem);

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
}

void VfsSuffix::convertToPlaceholder(const QString &, const SyncFileItem &, const QString &)
{
    // Nothing necessary
}

bool VfsSuffix::isDehydratedPlaceholder(const QString &filePath)
{
    if (!filePath.endsWith(fileSuffix()))
        return false;
    QFileInfo fi(filePath);
    return fi.exists() && fi.size() == 1;
}

bool VfsSuffix::statTypeVirtualFile(csync_file_stat_t *stat, void *)
{
    if (stat->path.endsWith(fileSuffix().toUtf8())) {
        stat->type = ItemTypeVirtualFile;
        return true;
    }
    return false;
}

Vfs::AvailabilityResult VfsSuffix::availability(const QString &folderPath)
{
    return availabilityInDb(folderPath);
}

} // namespace OCC
