/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> VfsSuffix::updateMetadata(const SyncFileItem &syncItem, const QString &filePath, const QString &replacesFile)
{
    Q_UNUSED(replacesFile)

    if (syncItem._modtime <= 0) {
        return {tr("Error updating metadata due to invalid modification time")};
    }

    qCDebug(lcVfsSuffix()) << "setModTime" << filePath << syncItem._modtime;
    FileSystem::setModTime(filePath, syncItem._modtime);
    return {OCC::Vfs::ConvertToPlaceholderResult::Ok};
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

Result<void, QString> VfsSuffix::createPlaceholders(const QList<SyncFileItemPtr> &items)
{
    auto result = Result<void, QString>{};

    for (const auto &oneItem : items) {
        const auto itemResult = createPlaceholder(*oneItem);
        if (!itemResult) {
            result = itemResult;
            break;
        }
    }

    return result;
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
