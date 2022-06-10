/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "vfs_off.h"

#include "filesystem.h"
#include "syncfileitem.h"

using namespace OCC;

VfsOff::VfsOff(QObject *parent)
    : Vfs(parent)
{
}

VfsOff::~VfsOff() = default;

Vfs::Mode VfsOff::mode() const
{
    return Vfs::Off;
}

QString VfsOff::fileSuffix() const
{
    return QString();
}

void VfsOff::stop() { }

void VfsOff::unregisterFolder() { }

bool VfsOff::socketApiPinStateActionsShown() const
{
    return false;
}

bool OCC::VfsOff::isHydrating() const
{
    return false;
}

Result<void, QString> VfsOff::createPlaceholder(const SyncFileItem &)
{
    return {};
}

bool VfsOff::needsMetadataUpdate(const SyncFileItem &)
{
    return false;
}

bool VfsOff::isDehydratedPlaceholder(const QString &)
{
    return false;
}

bool VfsOff::statTypeVirtualFile(csync_file_stat_t *, void *)
{
    return false;
}

bool VfsOff::setPinState(const QString &, PinState)
{
    return true;
}

Optional<PinState> VfsOff::pinState(const QString &)
{
    return PinState::AlwaysLocal;
}

Vfs::AvailabilityResult VfsOff::availability(const QString &)
{
    return VfsItemAvailability::AlwaysLocal;
}

void VfsOff::startImpl(const VfsSetupParams &)
{
    Q_EMIT started();
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsOff::updateMetadata(const SyncFileItem &item, const QString &filePath, const QString &replacesFile)
{
    Q_UNUSED(replacesFile)

    if (!item.isDirectory()) {
        const bool isReadOnly = !item._remotePerm.isNull() && !item._remotePerm.hasPermission(RemotePermissions::CanWrite);
        FileSystem::setFileReadOnlyWeak(filePath, isReadOnly);
    }
    return { ConvertToPlaceholderResult::Ok };
}

void VfsOff::fileStatusChanged(const QString &, SyncFileStatus)
{
}
