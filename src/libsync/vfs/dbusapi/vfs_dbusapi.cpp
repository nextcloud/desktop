/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "vfs_dbusapi.h"

#include <QDir>
#include <QFile>

#include "syncfileitem.h"
#include "filesystem.h"
#include "common/filesystembase.h"
#include "common/syncjournaldb.h"
#include "config.h"

#include <QCoreApplication>

Q_LOGGING_CATEGORY(lcDBusApi, "nextcloud.sync.vfs.dbusapi", QtInfoMsg)

namespace OCC {

class VfsDBusApiPrivate
{
public:
    QList<HydrationJob *> hydrationJobs;
};

VfsDBusApi::VfsDBusApi(QObject *parent)
    : Vfs(parent)
    , d(new VfsDBusApiPrivate)
{
}

VfsDBusApi::~VfsDBusApi() = default;

Vfs::Mode VfsDBusApi::mode() const
{
    return DBusApi;
}

QString VfsDBusApi::fileSuffix() const
{
    return {};
}

void VfsDBusApi::startImpl(const VfsSetupParams &params)
{
}

void VfsDBusApi::stop()
{
}

void VfsDBusApi::unregisterFolder()
{
}

bool VfsDBusApi::socketApiPinStateActionsShown() const
{
    return true;
}

bool VfsDBusApi::isHydrating() const
{
    return !d->hydrationJobs.isEmpty();
}

Result<void, QString> VfsDBusApi::updateMetadata(const QString &filePath, time_t modtime, qint64 size, const QByteArray &fileId)
{
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsDBusApi::updatePlaceholderMarkInSync(const QString &filePath, const QByteArray &fileId)
{
    return ConvertToPlaceholderResult::Error;
}

bool VfsDBusApi::isPlaceHolderInSync(const QString &filePath) const
{
    return false;
}

Result<void, QString> VfsDBusApi::createPlaceholder(const SyncFileItem &item)
{
    return {};
}

Result<void, QString> VfsDBusApi::dehydratePlaceholder(const SyncFileItem &item)
{
    return {};
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsDBusApi::convertToPlaceholder(const QString &filename, const SyncFileItem &item, const QString &replacesFile, UpdateMetadataTypes updateType)
{
    return ConvertToPlaceholderResult::Error;
}

bool VfsDBusApi::needsMetadataUpdate(const SyncFileItem &item)
{
    return false;
}

bool VfsDBusApi::isDehydratedPlaceholder(const QString &filePath)
{
    return false;
}

bool VfsDBusApi::statTypeVirtualFile(csync_file_stat_t *stat, void *statData)
{
    return false;
}

bool VfsDBusApi::setPinState(const QString &folderPath, PinState state)
{
    qCDebug(lcDBusApi()) << "setPinState" << folderPath << state;
    return setPinStateInDb(folderPath, state);
}

Optional<PinState> VfsDBusApi::pinState(const QString &folderPath)
{
    return pinStateInDb(folderPath);
}

Vfs::AvailabilityResult VfsDBusApi::availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck)
{
    return AvailabilityError::NoSuchItem;
}

void VfsDBusApi::cancelHydration(const QString &requestId, const QString & /*path*/)
{
}

void VfsDBusApi::requestHydration(const QString &requestId, const QString &path)
{
    qCInfo(lcDBusApi) << "Received request to hydrate" << path << requestId;
    const auto root = QDir::toNativeSeparators(params().filesystemPath);
    Q_ASSERT(path.startsWith(root));

    const auto relativePath = QDir::fromNativeSeparators(path.mid(root.length()));
    const auto journal = params().journal;

    // Set in the database that we should download the file
    SyncJournalFileRecord record;
    if (!journal->getFileRecord(relativePath, &record) || !record.isValid()) {
        qCInfo(lcDBusApi) << "Couldn't hydrate, did not find file in db";
        emit hydrationRequestFailed(requestId);
        return;
    }

    bool isNotVirtualFileFailure = false;
    if (!record.isVirtualFile()) {
        if (isDehydratedPlaceholder(path)) {
            qCWarning(lcDBusApi) << "Hydration requested for a placeholder file not marked as virtual in local DB. Attempting to fix it...";
            record._type = ItemTypeVirtualFileDownload;
            isNotVirtualFileFailure = !journal->setFileRecord(record);
        } else {
            isNotVirtualFileFailure = true;
        }
    }

    if (isNotVirtualFileFailure) {
        qCWarning(lcDBusApi) << "Couldn't hydrate, the file is not virtual";
        emit hydrationRequestFailed(requestId);
        return;
    }
}

void VfsDBusApi::fileStatusChanged(const QString &systemFileName, SyncFileStatus fileStatus)
{
    Q_UNUSED(systemFileName);
    Q_UNUSED(fileStatus);
}

int VfsDBusApi::finalizeHydrationJob(const QString &requestId)
{
    return -1;
}

VfsDBusApi::HydratationAndPinStates VfsDBusApi::computeRecursiveHydrationAndPinStates(const QString &folderPath, const Optional<PinState> &basePinState)
{
    Q_ASSERT(!folderPath.endsWith('/'));
    const auto fullPath = QString{params().filesystemPath + folderPath};
    QFileInfo info(params().filesystemPath + folderPath);

    if (!FileSystem::fileExists(fullPath)) {
        return {};
    }
    const auto effectivePin = pinState(folderPath);
    const auto pinResult = (!effectivePin && !basePinState) ? Optional<PinState>()
                         : (!effectivePin || !basePinState) ? PinState::Inherited
                         : (*effectivePin == *basePinState) ? *effectivePin
                         : PinState::Inherited;

    if (FileSystem::isDir(fullPath)) {
        const auto dirState = HydratationAndPinStates {
            pinResult,
            {}
        };
        const auto dir = QDir(info.absoluteFilePath());
        Q_ASSERT(dir.exists());
        const auto children = dir.entryList();
        return std::accumulate(std::cbegin(children), std::cend(children), dirState, [=, this](const HydratationAndPinStates &currentState, const QString &name) {
            if (name == QStringLiteral("..") || name == QStringLiteral(".")) {
                return currentState;
            }

            // if the folderPath.isEmpty() we don't want to end up having path "/example.file" because this will lead to double slash later, when appending to "SyncFolder/"
            const auto path = folderPath.isEmpty() ? name : folderPath + '/' + name;
            const auto states = computeRecursiveHydrationAndPinStates(path, currentState.pinState);
            return HydratationAndPinStates {
                states.pinState,
                {
                    states.hydrationStatus.hasHydrated || currentState.hydrationStatus.hasHydrated,
                    states.hydrationStatus.hasDehydrated || currentState.hydrationStatus.hasDehydrated,
                }
            };
        });
    } else { // file case
        const auto isDehydrated = isDehydratedPlaceholder(info.absoluteFilePath());
        return {
            pinResult,
            {
                !isDehydrated,
                isDehydrated
            }
        };
    }
}

} // namespace OCC
