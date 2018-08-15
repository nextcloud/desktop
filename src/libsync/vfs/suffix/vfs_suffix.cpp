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

namespace OCC {

class VfsSuffixPrivate
{
};

VfsSuffix::VfsSuffix(QObject *parent)
    : Vfs(parent)
    , d_ptr(new VfsSuffixPrivate)
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

void VfsSuffix::registerFolder(const VfsSetupParams &)
{
}

void VfsSuffix::start(const VfsSetupParams &)
{
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

bool VfsSuffix::updateMetadata(const QString &filePath, time_t modtime, quint64, const QByteArray &, QString *)
{
    FileSystem::setModTime(filePath, modtime);
    return true;
}

void VfsSuffix::createPlaceholder(const QString &syncFolder, const SyncFileItemPtr &item)
{
    // NOTE: Other places might depend on contents of placeholder files (like csync_update)
    QString fn = syncFolder + item->_file;
    QFile file(fn);
    file.open(QFile::ReadWrite | QFile::Truncate);
    file.write(" ");
    file.close();
    FileSystem::setModTime(fn, item->_modtime);
}

void VfsSuffix::convertToPlaceholder(const QString &, const SyncFileItemPtr &)
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

} // namespace OCC
