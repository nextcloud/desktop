/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "vfs_xattr.h"

#include "syncfileitem.h"
#include "filesystem.h"
#include "common/syncjournaldb.h"
#include "xattrwrapper.h"

#include <QFile>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcVfsXAttr, "nextcloud.sync.vfs.xattr", QtInfoMsg)

namespace xattr {
using namespace OCC::XAttrWrapper;
}

namespace OCC {

VfsXAttr::VfsXAttr(QObject *parent)
    : Vfs(parent)
{
}

VfsXAttr::~VfsXAttr() = default;

Vfs::Mode VfsXAttr::mode() const
{
    return XAttr;
}

QString VfsXAttr::fileSuffix() const
{
    return QString();
}

void VfsXAttr::startImpl(const VfsSetupParams &)
{
}

void VfsXAttr::stop()
{
}

void VfsXAttr::unregisterFolder()
{
}

bool VfsXAttr::socketApiPinStateActionsShown() const
{
    return true;
}

bool VfsXAttr::isHydrating() const
{
    return false;
}

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> VfsXAttr::updateMetadata(const SyncFileItem &syncItem, const QString &filePath, const QString &replacesFile)
{
    Q_UNUSED(replacesFile)

    if (syncItem._modtime <= 0) {
        return {tr("Error updating metadata due to invalid modification time")};
    }

    qCDebug(lcVfsXAttr()) << "setModTime" << filePath << syncItem._modtime;
    FileSystem::setModTime(filePath, syncItem._modtime);
    return {OCC::Vfs::ConvertToPlaceholderResult::Ok};
}

Result<void, QString> VfsXAttr::createPlaceholder(const SyncFileItem &item)
{
    if (item._modtime <= 0) {
        return {tr("Error updating metadata due to invalid modification time")};
    }

    const auto path = QString(_setupParams.filesystemPath + item._file);
    QFile file(path);
    if (file.exists() && file.size() > 1
        && !FileSystem::verifyFileUnchanged(path, item._size, item._modtime)) {
        return QStringLiteral("Cannot create a placeholder because a file with the placeholder name already exist");
    }

    if (!file.open(QFile::ReadWrite | QFile::Truncate)) {
        return file.errorString();
    }

    file.write(" ");
    file.close();
    qCDebug(lcVfsXAttr()) << "setModTime" << path << item._modtime;
    FileSystem::setModTime(path, item._modtime);
    return xattr::addNextcloudPlaceholderAttributes(path);
}

Result<void, QString> VfsXAttr::createPlaceholders(const QList<SyncFileItemPtr> &items)
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

Result<void, QString> VfsXAttr::dehydratePlaceholder(const SyncFileItem &item)
{
    const auto path = QString(_setupParams.filesystemPath + item._file);
    QFile file(path);
    if (!file.remove()) {
        return QStringLiteral("Couldn't remove the original file to dehydrate");
    }
    auto r = createPlaceholder(item);
    if (!r) {
        return r;
    }

    // Ensure the pin state isn't contradictory
    const auto pin = pinState(item._file);
    if (pin && *pin == PinState::AlwaysLocal) {
        setPinState(item._renameTarget, PinState::Unspecified);
    }
    return {};
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsXAttr::convertToPlaceholder(const QString &,
                                                                                const SyncFileItem &,
                                                                                const QString &,
                                                                                UpdateMetadataTypes)
{
    // Nothing necessary
    return {ConvertToPlaceholderResult::Ok};
}

bool VfsXAttr::needsMetadataUpdate(const SyncFileItem &)
{
    return false;
}

bool VfsXAttr::isDehydratedPlaceholder(const QString &filePath)
{
    const auto fi = QFileInfo(filePath);
    return fi.exists() &&
            xattr::hasNextcloudPlaceholderAttributes(filePath);
}

bool VfsXAttr::statTypeVirtualFile(csync_file_stat_t *stat, void *statData)
{
    if (stat->type == ItemTypeDirectory) {
        return false;
    }

    const auto parentPath = static_cast<QByteArray *>(statData);
    Q_ASSERT(!parentPath->endsWith('/'));
    Q_ASSERT(!stat->path.startsWith('/'));

    const auto path = QByteArray(*parentPath + '/' + stat->path);
    const auto pin = [=, this] {
        const auto absolutePath = QString::fromUtf8(path);
        Q_ASSERT(absolutePath.startsWith(params().filesystemPath.toUtf8()));
        const auto folderPath = absolutePath.mid(params().filesystemPath.length());
        return pinState(folderPath);
    }();

    if (xattr::hasNextcloudPlaceholderAttributes(path)) {
        const auto shouldDownload = pin && (*pin == PinState::AlwaysLocal);
        stat->type = shouldDownload ? ItemTypeVirtualFileDownload : ItemTypeVirtualFile;
        return true;
    } else {
        const auto shouldDehydrate = pin && (*pin == PinState::OnlineOnly);
        if (shouldDehydrate) {
            stat->type = ItemTypeVirtualFileDehydration;
            return true;
        }
    }
    return false;
}

bool VfsXAttr::setPinState(const QString &folderPath, PinState state)
{
    qCDebug(lcVfsXAttr) << "setPinState" << folderPath << state;
    return setPinStateInDb(folderPath, state);
}

Optional<PinState> VfsXAttr::pinState(const QString &folderPath)
{
    return pinStateInDb(folderPath);
}

Vfs::AvailabilityResult VfsXAttr::availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck)
{
    Q_UNUSED(recursiveCheck)
    return availabilityInDb(folderPath);
}

void VfsXAttr::fileStatusChanged(const QString &, SyncFileStatus)
{
}

} // namespace OCC
