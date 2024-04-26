/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#include "vfs_cfapi.h"

#include <QDir>
#include <QFile>

#include "cfapiwrapper.h"
#include "hydrationjob.h"
#include "syncfileitem.h"
#include "filesystem.h"
#include "common/syncjournaldb.h"
#include "config.h"

#include <ntstatus.h>
#include <cfapi.h>
#include <comdef.h>

#include <QCoreApplication>

Q_LOGGING_CATEGORY(lcCfApi, "nextcloud.sync.vfs.cfapi", QtInfoMsg)

namespace cfapi {
using namespace OCC::CfApiWrapper;

constexpr auto appIdRegKey = R"(Software\Classes\AppID\)";
constexpr auto clsIdRegKey = R"(Software\Classes\CLSID\)";
const auto rootKey = HKEY_CURRENT_USER;

bool registerShellExtension()
{
    const QList<QPair<QString, QString>> listExtensions = {
        {CFAPI_SHELLEXT_THUMBNAIL_HANDLER_DISPLAY_NAME, CFAPI_SHELLEXT_THUMBNAIL_HANDLER_CLASS_ID_REG},
        {CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_DISPLAY_NAME, CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID_REG}
    };
    // assume CFAPI_SHELL_EXTENSIONS_LIB_NAME is always in the same folder as the main executable
    // assume CFAPI_SHELL_EXTENSIONS_LIB_NAME is always in the same folder as the main executable
    const auto shellExtensionDllPath = QDir::toNativeSeparators(QString(QCoreApplication::applicationDirPath() + QStringLiteral("/") + CFAPI_SHELL_EXTENSIONS_LIB_NAME + QStringLiteral(".dll")));
    if (!QFileInfo::exists(shellExtensionDllPath)) {
        Q_ASSERT(false);
        qCWarning(lcCfApi) << "Register CfAPI shell extensions failed. Dll does not exist in " << QCoreApplication::applicationDirPath();
        return false;
    }

    const QString appIdPath = QString() % appIdRegKey % CFAPI_SHELLEXT_APPID_REG;
    if (!OCC::Utility::registrySetKeyValue(rootKey, appIdPath, {}, REG_SZ, CFAPI_SHELLEXT_APPID_DISPLAY_NAME)) {
        return false;
    }
    if (!OCC::Utility::registrySetKeyValue(rootKey, appIdPath, QStringLiteral("DllSurrogate"), REG_SZ, {})) {
        return false;
    }

    for (const auto extension : listExtensions) {
        const QString clsidPath = QString() % clsIdRegKey % extension.second;
        const QString clsidServerPath = clsidPath % R"(\InprocServer32)";

        if (!OCC::Utility::registrySetKeyValue(rootKey, clsidPath, QStringLiteral("AppID"), REG_SZ, CFAPI_SHELLEXT_APPID_REG)) {
            return false;
        }
        if (!OCC::Utility::registrySetKeyValue(rootKey, clsidPath, {}, REG_SZ, extension.first)) {
            return false;
        }
        if (!OCC::Utility::registrySetKeyValue(rootKey, clsidServerPath, {}, REG_SZ, shellExtensionDllPath)) {
            return false;
        }
        if (!OCC::Utility::registrySetKeyValue(rootKey, clsidServerPath, QStringLiteral("ThreadingModel"), REG_SZ, QStringLiteral("Apartment"))) {
            return false;
        }
    }

    return true;
}

void unregisterShellExtensions()
{
    const QString appIdPath = QString() % appIdRegKey % CFAPI_SHELLEXT_APPID_REG;
    if (OCC::Utility::registryKeyExists(rootKey, appIdPath)) {
        OCC::Utility::registryDeleteKeyTree(rootKey, appIdPath);
    }

    const QStringList listExtensions = {
        CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID_REG,
        CFAPI_SHELLEXT_THUMBNAIL_HANDLER_CLASS_ID_REG
    };

    for (const auto extension : listExtensions) {
        const QString clsidPath = QString() % clsIdRegKey % extension;
        if (OCC::Utility::registryKeyExists(rootKey, clsidPath)) {
            OCC::Utility::registryDeleteKeyTree(rootKey, clsidPath);
        }
    }
}

}

namespace OCC {

class VfsCfApiPrivate
{
public:
    QList<HydrationJob *> hydrationJobs;
    cfapi::ConnectionKey connectionKey;
};

VfsCfApi::VfsCfApi(QObject *parent)
    : Vfs(parent)
    , d(new VfsCfApiPrivate)
{
}

VfsCfApi::~VfsCfApi() = default;

Vfs::Mode VfsCfApi::mode() const
{
    return WindowsCfApi;
}

QString VfsCfApi::fileSuffix() const
{
    return {};
}

void VfsCfApi::startImpl(const VfsSetupParams &params)
{
    cfapi::registerShellExtension();
    const auto localPath = QDir::toNativeSeparators(params.filesystemPath);

    const auto registerResult = cfapi::registerSyncRoot(localPath, params.providerName, params.providerVersion, params.alias, params.navigationPaneClsid, params.displayName, params.account->displayName());
    if (!registerResult) {
        qCCritical(lcCfApi) << "Initialization failed, couldn't register sync root:" << registerResult.error();
        return;
    }

    auto connectResult = cfapi::connectSyncRoot(localPath, this);
    if (!connectResult) {
        qCCritical(lcCfApi) << "Initialization failed, couldn't connect sync root:" << connectResult.error();
        return;
    }

    d->connectionKey = *std::move(connectResult);
}

void VfsCfApi::stop()
{
    const auto result = cfapi::disconnectSyncRoot(std::move(d->connectionKey));
    if (!result) {
        qCCritical(lcCfApi) << "Disconnect failed for" << QDir::toNativeSeparators(params().filesystemPath) << ":" << result.error();
    }
}

void VfsCfApi::unregisterFolder()
{
    const auto localPath = QDir::toNativeSeparators(params().filesystemPath);
    const auto result = cfapi::unregisterSyncRoot(localPath, params().providerName, params().account->displayName());
    if (!result) {
        qCCritical(lcCfApi) << "Unregistration failed for" << localPath << ":" << result.error();
    }

    if (!cfapi::isAnySyncRoot(params().providerName, params().account->displayName())) {
        cfapi::unregisterShellExtensions();
    }
}

bool VfsCfApi::socketApiPinStateActionsShown() const
{
    return true;
}

bool VfsCfApi::isHydrating() const
{
    return !d->hydrationJobs.isEmpty();
}

Result<void, QString> VfsCfApi::updateMetadata(const QString &filePath, time_t modtime, qint64 size, const QByteArray &fileId)
{
    const auto localPath = QDir::toNativeSeparators(filePath);
    if (cfapi::handleForPath(localPath)) {
        auto result = cfapi::updatePlaceholderInfo(localPath, modtime, size, fileId);
        if (result) {
            return {};
        } else {
            return result.error();
        }
    } else {
        qCWarning(lcCfApi) << "Couldn't update metadata for non existing file" << localPath;
        return {QStringLiteral("Couldn't update metadata")};
    }
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsCfApi::updatePlaceholderMarkInSync(const QString &filePath, const QByteArray &fileId)
{
    return cfapi::updatePlaceholderMarkInSync(filePath, fileId, {});
}

bool VfsCfApi::isPlaceHolderInSync(const QString &filePath) const
{
    return cfapi::isPlaceHolderInSync(filePath);
}

Result<void, QString> VfsCfApi::createPlaceholder(const SyncFileItem &item)
{
    Q_ASSERT(params().filesystemPath.endsWith('/'));
    const auto localPath = QDir::toNativeSeparators(params().filesystemPath + item._file);
    const auto result = cfapi::createPlaceholderInfo(localPath, item._modtime, item._size, item._fileId);
    return result;
}

Result<void, QString> VfsCfApi::dehydratePlaceholder(const SyncFileItem &item)
{
    const auto localPath = QDir::toNativeSeparators(_setupParams.filesystemPath + item._file);
    if (cfapi::handleForPath(localPath)) {
        auto result = cfapi::dehydratePlaceholder(localPath, item._modtime, item._size, item._fileId);
        if (result) {
            return {};
        } else {
            return result.error();
        }
    } else {
        qCWarning(lcCfApi) << "Couldn't update metadata for non existing file" << localPath;
        return {QStringLiteral("Couldn't update metadata")};
    }
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsCfApi::convertToPlaceholder(const QString &filename, const SyncFileItem &item, const QString &replacesFile, UpdateMetadataTypes updateType)
{
    const auto localPath = QDir::toNativeSeparators(filename);

    if (item._type != ItemTypeDirectory && OCC::FileSystem::isLnkFile(filename)) {
        qCInfo(lcCfApi) << "File \"" << filename << "\" is a Windows shortcut. Not converting it to a placeholder.";
        const auto pinState = pinStateLocal(localPath);
        if (!pinState || *pinState != PinState::Excluded) {
            setPinStateLocal(localPath, PinState::Excluded);
        }
        return Vfs::ConvertToPlaceholderResult::Ok;
    }

    const auto replacesPath = QDir::toNativeSeparators(replacesFile);

    if (cfapi::findPlaceholderInfo(localPath)) {
        if (updateType & Vfs::UpdateMetadataType::FileMetadata) {
            return cfapi::updatePlaceholderInfo(localPath, item._modtime, item._size, item._fileId, replacesPath);
        } else {
            return cfapi::updatePlaceholderMarkInSync(localPath, item._fileId, replacesPath);
        }
    } else {
        return cfapi::convertToPlaceholder(localPath, item._modtime, item._size, item._fileId, replacesPath);
    }
}

bool VfsCfApi::needsMetadataUpdate(const SyncFileItem &item)
{
    return false;
}

bool VfsCfApi::isDehydratedPlaceholder(const QString &filePath)
{
    const auto path = QDir::toNativeSeparators(filePath);
    return cfapi::isSparseFile(path);
}

bool VfsCfApi::statTypeVirtualFile(csync_file_stat_t *stat, void *statData)
{
    const auto ffd = static_cast<WIN32_FIND_DATA *>(statData);

    const auto isDirectory = (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const auto isSparseFile = (ffd->dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
    const auto isPinned = (ffd->dwFileAttributes & FILE_ATTRIBUTE_PINNED) != 0;
    const auto isUnpinned = (ffd->dwFileAttributes & FILE_ATTRIBUTE_UNPINNED) != 0;
    const auto hasReparsePoint = (ffd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    const auto hasCloudTag = hasReparsePoint && (ffd->dwReserved0 & ~IO_REPARSE_TAG_CLOUD_MASK) == (IO_REPARSE_TAG_CLOUD & ~IO_REPARSE_TAG_CLOUD_MASK);

    const auto isWindowsShortcut = !isDirectory && FileSystem::isLnkFile(stat->path);

    const auto isExcludeFile = !isDirectory && FileSystem::isExcludeFile(stat->path);

    stat->is_metadata_missing = !hasCloudTag;

    // It's a dir with a reparse point due to the placeholder info (hence the cloud tag)
    // if we don't remove the reparse point flag the discovery will end up thinking
    // it is a file... let's prevent it
    if (isDirectory) {
        if (hasCloudTag) {
            ffd->dwFileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
        }
        return false;
    } else if (isSparseFile && isPinned) {
        stat->type = ItemTypeVirtualFileDownload;
        return true;
    } else if (!isSparseFile && isUnpinned && !isWindowsShortcut && !isExcludeFile) {
        stat->type = ItemTypeVirtualFileDehydration;
        return true;
    } else if (isSparseFile) {
        stat->type = ItemTypeVirtualFile;
        return true;
    }

    return false;
}

bool VfsCfApi::setPinState(const QString &folderPath, PinState state)
{
    qCDebug(lcCfApi) << "setPinState" << folderPath << state;

    const auto localPath = QDir::toNativeSeparators(params().filesystemPath + folderPath);

    return setPinStateLocal(localPath, state);
}

bool VfsCfApi::setPinStateLocal(const QString &localPath, PinState state)
{
    if (cfapi::setPinState(localPath, state, cfapi::Recurse)) {
        return true;
    } else {
        return false;
    }
}

Optional<PinState> VfsCfApi::pinState(const QString &folderPath)
{
    const auto localPath = QDir::toNativeSeparators(params().filesystemPath + folderPath);

    return pinStateLocal(localPath);
}

Optional<PinState> VfsCfApi::pinStateLocal(const QString &localPath) const
{
    const auto info = cfapi::findPlaceholderInfo(localPath);
    if (!info) {
        qCDebug(lcCfApi) << "Couldn't find pin state for regular non-placeholder file" << localPath;
        return {};
    }

    return info.pinState();
}

Vfs::AvailabilityResult VfsCfApi::availability(const QString &folderPath, const AvailabilityRecursivity recursiveCheck)
{
    const auto basePinState = pinState(folderPath);
    if (basePinState && recursiveCheck == Vfs::AvailabilityRecursivity::NotRecursiveAvailability) {
        switch (*basePinState)
        {
        case OCC::PinState::AlwaysLocal:
            return VfsItemAvailability::AlwaysLocal;
            break;
        case OCC::PinState::Excluded:
            break;
        case OCC::PinState::Inherited:
            break;
        case OCC::PinState::OnlineOnly:
            return VfsItemAvailability::OnlineOnly;
            break;
        case OCC::PinState::Unspecified:
            break;
        };
        return VfsItemAvailability::Mixed;
    } else {
        const auto hydrationAndPinStates = computeRecursiveHydrationAndPinStates(folderPath, basePinState);

        const auto pin = hydrationAndPinStates.pinState;
        const auto hydrationStatus = hydrationAndPinStates.hydrationStatus;

        if (hydrationStatus.hasDehydrated) {
            if (hydrationStatus.hasHydrated)
                return VfsItemAvailability::Mixed;
            if (pin && *pin == PinState::OnlineOnly)
                return VfsItemAvailability::OnlineOnly;
            else
                return VfsItemAvailability::AllDehydrated;
        } else if (hydrationStatus.hasHydrated) {
            if (pin && *pin == PinState::AlwaysLocal)
                return VfsItemAvailability::AlwaysLocal;
            else
                return VfsItemAvailability::AllHydrated;
        }
        return AvailabilityError::NoSuchItem;
    }
}

HydrationJob *VfsCfApi::findHydrationJob(const QString &requestId) const
{
    // Find matching hydration job for request id
    const auto hydrationJobsIter = std::find_if(d->hydrationJobs.cbegin(), d->hydrationJobs.cend(), [&](const HydrationJob *job) {
        return job->requestId() == requestId;
    });

    if (hydrationJobsIter != d->hydrationJobs.cend()) {
        return *hydrationJobsIter;
    }

    return nullptr;
}

void VfsCfApi::cancelHydration(const QString &requestId, const QString & /*path*/)
{
    // Find matching hydration job for request id
    const auto hydrationJob = findHydrationJob(requestId);
    // If found, cancel it
    if (hydrationJob) {
        qCInfo(lcCfApi) << "Cancel hydration";
        hydrationJob->cancel();
    }
}

void VfsCfApi::requestHydration(const QString &requestId, const QString &path)
{
    qCInfo(lcCfApi) << "Received request to hydrate" << path << requestId;
    const auto root = QDir::toNativeSeparators(params().filesystemPath);
    Q_ASSERT(path.startsWith(root));

    const auto relativePath = QDir::fromNativeSeparators(path.mid(root.length()));
    const auto journal = params().journal;

    // Set in the database that we should download the file
    SyncJournalFileRecord record;
    if (!journal->getFileRecord(relativePath, &record) || !record.isValid()) {
        qCInfo(lcCfApi) << "Couldn't hydrate, did not find file in db";
        emit hydrationRequestFailed(requestId);
        return;
    }

    bool isNotVirtualFileFailure = false;
    if (!record.isVirtualFile()) {
        if (isDehydratedPlaceholder(path)) {
            qCWarning(lcCfApi) << "Hydration requested for a placeholder file not marked as virtual in local DB. Attempting to fix it...";
            record._type = ItemTypeVirtualFileDownload;
            isNotVirtualFileFailure = !journal->setFileRecord(record);
        } else {
            isNotVirtualFileFailure = true;
        }
    }

    if (isNotVirtualFileFailure) {
        qCWarning(lcCfApi) << "Couldn't hydrate, the file is not virtual";
        emit hydrationRequestFailed(requestId);
        return;
    }

    // All good, let's hydrate now
    scheduleHydrationJob(requestId, relativePath, record);
}

void VfsCfApi::fileStatusChanged(const QString &systemFileName, SyncFileStatus fileStatus)
{
    Q_UNUSED(systemFileName);
    Q_UNUSED(fileStatus);
}

void VfsCfApi::scheduleHydrationJob(const QString &requestId, const QString &folderPath, const SyncJournalFileRecord &record)
{
    const auto jobAlreadyScheduled = std::any_of(std::cbegin(d->hydrationJobs), std::cend(d->hydrationJobs), [=](HydrationJob *job) {
        return job->requestId() == requestId || job->folderPath() == folderPath;
    });

    if (jobAlreadyScheduled) {
        qCWarning(lcCfApi) << "The OS submitted again a hydration request which is already on-going" << requestId << folderPath;
        emit hydrationRequestFailed(requestId);
        return;
    }

    if (d->hydrationJobs.isEmpty()) {
        emit beginHydrating();
    }

    auto job = new HydrationJob(this);
    job->setAccount(params().account);
    job->setRemoteSyncRootPath(params().remotePath);
    job->setLocalPath(params().filesystemPath);
    job->setJournal(params().journal);
    job->setRequestId(requestId);
    job->setFolderPath(folderPath);
    job->setIsEncryptedFile(record.isE2eEncrypted());
    job->setE2eMangledName(record._e2eMangledName);
    connect(job, &HydrationJob::finished, this, &VfsCfApi::onHydrationJobFinished);
    d->hydrationJobs << job;
    job->start();
    emit hydrationRequestReady(requestId);
}

void VfsCfApi::onHydrationJobFinished(HydrationJob *job)
{
    Q_ASSERT(d->hydrationJobs.contains(job));
    qCInfo(lcCfApi) << "Hydration job finished" << job->requestId() << job->folderPath() << job->status();
    emit hydrationRequestFinished(job->requestId());
    if (!job->errorString().isEmpty()) {
        params().account->reportClientStatus(ClientStatusReportingStatus::DownloadError_Virtual_File_Hydration_Failure);
        emit failureHydrating(job->errorCode(), job->statusCode(), job->errorString(), job->folderPath());
    }
}

int VfsCfApi::finalizeHydrationJob(const QString &requestId)
{
    qCDebug(lcCfApi) << "Finalize hydration job" << requestId;
    // Find matching hydration job for request id
    const auto hydrationJob = findHydrationJob(requestId);

    // If found, finalize it
    if (hydrationJob) {
        hydrationJob->finalize(this);
        d->hydrationJobs.removeAll(hydrationJob);
        hydrationJob->deleteLater();
        if (d->hydrationJobs.isEmpty()) {
            emit doneHydrating();
        }
        return hydrationJob->status();
    }

    return HydrationJob::Status::Error;
}

VfsCfApi::HydratationAndPinStates VfsCfApi::computeRecursiveHydrationAndPinStates(const QString &folderPath, const Optional<PinState> &basePinState)
{
    Q_ASSERT(!folderPath.endsWith('/'));
    QFileInfo info(params().filesystemPath + folderPath);

    if (!info.exists()) {
        return {};
    }
    const auto effectivePin = pinState(folderPath);
    const auto pinResult = (!effectivePin && !basePinState) ? Optional<PinState>()
                         : (!effectivePin || !basePinState) ? PinState::Inherited
                         : (*effectivePin == *basePinState) ? *effectivePin
                         : PinState::Inherited;

    if (info.isDir()) {
        const auto dirState = HydratationAndPinStates {
            pinResult,
            {}
        };
        const auto dir = QDir(info.absoluteFilePath());
        Q_ASSERT(dir.exists());
        const auto children = dir.entryList();
        return std::accumulate(std::cbegin(children), std::cend(children), dirState, [=](const HydratationAndPinStates &currentState, const QString &name) {
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
