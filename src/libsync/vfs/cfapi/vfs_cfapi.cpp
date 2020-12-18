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

#include "syncfileitem.h"
#include "filesystem.h"
#include "common/syncjournaldb.h"

#include <cfapi.h>
#include <comdef.h>

Q_LOGGING_CATEGORY(lcCfApi, "nextcloud.sync.vfs.cfapi", QtInfoMsg)

namespace {
void CALLBACK cfApiFetchDataCallback(_In_ CONST CF_CALLBACK_INFO* callbackInfo, _In_ CONST CF_CALLBACK_PARAMETERS* callbackParameters)
{
    qCCritical(lcCfApi) << "Got in!";
    Q_ASSERT(false);
}

CF_CALLBACK_REGISTRATION cfApiCallbacks[] = {
    { CF_CALLBACK_TYPE_FETCH_DATA, cfApiFetchDataCallback },
    CF_CALLBACK_REGISTRATION_END
};

std::unique_ptr<void, void(*)(HANDLE)> handleForPath(const QString &path)
{
    if (QFileInfo(path).isDir()) {
        HANDLE handle = nullptr;
        const qint64 openResult = CfOpenFileWithOplock(path.toStdWString().data(), CF_OPEN_FILE_FLAG_NONE, &handle);
        if (openResult == S_OK) {
            return {handle, CfCloseHandle};
        }
    } else {
        const auto handle = CreateFile(path.toStdWString().data(), 0, 0, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            return {handle, [](HANDLE h) { CloseHandle(h); }};
        }
    }

    return {nullptr, [](HANDLE){}};
}

DWORD sizeToDWORD(size_t size)
{
    return OCC::Utility::convertSizeToDWORD(size);
}

void deletePlaceholderInfo(CF_PLACEHOLDER_BASIC_INFO *info)
{
    auto byte = reinterpret_cast<char *>(info);
    delete[] byte;
}

std::unique_ptr<CF_PLACEHOLDER_BASIC_INFO, decltype(&deletePlaceholderInfo)> findPlaceholderInfo(const QString &path)
{
    auto handle = handleForPath(path);
    if (!handle) {
        return {nullptr, deletePlaceholderInfo};
    }

    constexpr auto fileIdMaxLength = 128;
    const auto infoSize = sizeof(CF_PLACEHOLDER_BASIC_INFO) + fileIdMaxLength;
    std::unique_ptr<CF_PLACEHOLDER_BASIC_INFO, decltype(&deletePlaceholderInfo)> info(reinterpret_cast<CF_PLACEHOLDER_BASIC_INFO *>(new char[infoSize]), deletePlaceholderInfo);
    const qint64 result = CfGetPlaceholderInfo(handle.get(), CF_PLACEHOLDER_INFO_BASIC, info.get(), sizeToDWORD(infoSize), nullptr);

    if (result == S_OK) {
        return info;
    } else {
        return {nullptr, deletePlaceholderInfo};
    }
}

bool setPinState(const QString &path, CF_PIN_STATE state, CF_SET_PIN_FLAGS flags)
{
    if (!findPlaceholderInfo(path)) {
        return false;
    }

    const auto handle = handleForPath(path);
    if (!handle) {
        return false;
    }

    const qint64 result = CfSetPinState(handle.get(), state, flags, nullptr);
    if (result != S_OK) {
        qCWarning(lcCfApi) << "Couldn't set pin state for" << path << ":" << _com_error(result).ErrorMessage();
    }
    return result == S_OK;
}

OCC::Result<void, QString> createPlaceholderInfo(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId)
{
    const auto fileInfo = QFileInfo(path);
    const auto localBasePath = QDir::toNativeSeparators(fileInfo.path()).toStdWString();
    const auto relativePath = fileInfo.fileName().toStdWString();

    const auto fileIdentity = QString::fromUtf8(fileId).toStdWString();

    CF_PLACEHOLDER_CREATE_INFO cloudEntry;
    cloudEntry.FileIdentity = fileIdentity.data();
    const auto fileIdentitySize = (fileIdentity.length() + 1) * sizeof(wchar_t);
    cloudEntry.FileIdentityLength = sizeToDWORD(fileIdentitySize);

    cloudEntry.RelativeFileName = relativePath.data();
    cloudEntry.Flags = CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC;
    cloudEntry.FsMetadata.FileSize.QuadPart = size;
    cloudEntry.FsMetadata.BasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &cloudEntry.FsMetadata.BasicInfo.CreationTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &cloudEntry.FsMetadata.BasicInfo.LastWriteTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &cloudEntry.FsMetadata.BasicInfo.LastAccessTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &cloudEntry.FsMetadata.BasicInfo.ChangeTime);

    if (fileInfo.isDir()) {
        cloudEntry.Flags |= CF_PLACEHOLDER_CREATE_FLAG_DISABLE_ON_DEMAND_POPULATION;
        cloudEntry.FsMetadata.BasicInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        cloudEntry.FsMetadata.FileSize.QuadPart = 0;
    }

    const qint64 result = CfCreatePlaceholders(localBasePath.data(), &cloudEntry, 1, CF_CREATE_FLAG_NONE, nullptr);
    if (result != S_OK) {
        qCWarning(lcCfApi) << "Couldn't create placeholder info for" << path << ":" << _com_error(result).ErrorMessage();
        return "Couldn't create placeholder info";
    }

    const auto parentInfo = findPlaceholderInfo(QDir::toNativeSeparators(QFileInfo(path).absolutePath()));
    const auto state = parentInfo && parentInfo->PinState == CF_PIN_STATE_UNPINNED ? CF_PIN_STATE_UNPINNED : CF_PIN_STATE_INHERIT;

    if (!setPinState(path, state, CF_SET_PIN_FLAG_NONE)) {
        return "Couldn't set the default inherit pin state";
    }

    return {};
}

OCC::Result<void, QString> updatePlaceholderInfo(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath = QString())
{
    auto info = findPlaceholderInfo(replacesPath.isEmpty() ? path : replacesPath);
    if (!info) {
        return "Can't update non existing placeholder info";
    }

    const auto previousPinState = info->PinState;

    auto handle = handleForPath(path);
    if (!handle) {
        return "Can't update placeholder info for non existing file";
    }

    const auto fileIdentity = QString::fromUtf8(fileId).toStdWString();
    const auto fileIdentitySize = (fileIdentity.length() + 1) * sizeof(wchar_t);

    CF_FS_METADATA metadata;
    metadata.FileSize.QuadPart = size;
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.CreationTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.LastWriteTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.LastAccessTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.ChangeTime);

    const qint64 result = CfUpdatePlaceholder(handle.get(), &metadata,
                                              fileIdentity.data(), sizeToDWORD(fileIdentitySize),
                                              nullptr, 0, CF_UPDATE_FLAG_NONE, nullptr, nullptr);

    if (result != S_OK) {
        qCWarning(lcCfApi) << "Couldn't update placeholder info for" << path << ":" << _com_error(result).ErrorMessage();
        return "Couldn't update placeholder info";
    }

    // Pin state tends to be lost on updates, so restore it every time
    if (!setPinState(path, previousPinState, CF_SET_PIN_FLAG_NONE)) {
        return "Couldn't restore pin state";
    }

    return {};
}

void convertToPlaceholder(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath)
{
    auto handle = handleForPath(path);
    Q_ASSERT(handle);

    const auto fileIdentity = QString::fromUtf8(fileId).toStdWString();
    const auto fileIdentitySize = (fileIdentity.length() + 1) * sizeof(wchar_t);
    const qint64 result = CfConvertToPlaceholder(handle.get(), fileIdentity.data(), sizeToDWORD(fileIdentitySize), CF_CONVERT_FLAG_NONE, nullptr, nullptr);
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        qCCritical(lcCfApi) << "Couldn't convert to placeholder" << path << ":" << _com_error(result).ErrorMessage();
        return;
    }

    const auto originalInfo = findPlaceholderInfo(replacesPath);
    if (!originalInfo) {
        const auto stateResult = setPinState(path, CF_PIN_STATE_INHERIT, CF_SET_PIN_FLAG_NONE);
        Q_ASSERT(stateResult);
    } else {
        const auto stateResult = setPinState(path, originalInfo->PinState, CF_SET_PIN_FLAG_NONE);
        Q_ASSERT(stateResult);
    }
}

namespace OCC {

class VfsCfApiPrivate
{
public:
    CF_CONNECTION_KEY callbackConnectionKey = {};
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
    const auto localPath = QDir::toNativeSeparators(params.filesystemPath);

    const auto providerName = params.providerName.toStdWString();
    const auto providerVersion = params.providerVersion.toStdWString();

    CF_SYNC_REGISTRATION info;
    info.ProviderName = providerName.data();
    info.ProviderVersion = providerVersion.data();
    info.SyncRootIdentity = nullptr;
    info.SyncRootIdentityLength = 0;
    info.FileIdentity = nullptr;
    info.FileIdentityLength = 0;

    CF_SYNC_POLICIES policies;
    policies.Hydration.Primary = CF_HYDRATION_POLICY_FULL;
    policies.Hydration.Modifier = CF_HYDRATION_POLICY_MODIFIER_NONE;
    policies.Population.Primary = CF_POPULATION_POLICY_ALWAYS_FULL;
    policies.Population.Modifier = CF_POPULATION_POLICY_MODIFIER_NONE;
    policies.InSync = CF_INSYNC_POLICY_PRESERVE_INSYNC_FOR_SYNC_ENGINE;
    policies.HardLink = CF_HARDLINK_POLICY_NONE;

    const qint64 registerResult = CfRegisterSyncRoot(localPath.toStdWString().data(), &info, &policies, CF_REGISTER_FLAG_UPDATE);
    Q_ASSERT(registerResult == S_OK);
    if (registerResult != S_OK) {
        qCCritical(lcCfApi) << "Initialization failed, couldn't register sync root:" << _com_error(registerResult).ErrorMessage();
        return;
    }


    const qint64 connectResult = CfConnectSyncRoot(localPath.toStdWString().data(),
                                                   cfApiCallbacks,
                                                   nullptr,
                                                   CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO | CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH,
                                                   &d->callbackConnectionKey);
    Q_ASSERT(connectResult == S_OK);
    if (connectResult != S_OK) {
        qCCritical(lcCfApi) << "Initialization failed, couldn't connect sync root:" << _com_error(connectResult).ErrorMessage();
    }
}

void VfsCfApi::stop()
{
    CfDisconnectSyncRoot(d->callbackConnectionKey);
}

void VfsCfApi::unregisterFolder()
{
    const auto localPath = QDir::toNativeSeparators(params().filesystemPath).toStdWString();
    CfUnregisterSyncRoot(localPath.data());
}

bool VfsCfApi::socketApiPinStateActionsShown() const
{
    return true;
}

bool VfsCfApi::isHydrating() const
{
    return false;
}

Result<void, QString> VfsCfApi::updateMetadata(const QString &filePath, time_t modtime, qint64 size, const QByteArray &fileId)
{
    const auto localPath = QDir::toNativeSeparators(filePath);
    return updatePlaceholderInfo(localPath, modtime, size, fileId);
}

Result<void, QString> VfsCfApi::createPlaceholder(const SyncFileItem &item)
{
    Q_ASSERT(params().filesystemPath.endsWith('/'));
    const auto localPath = QDir::toNativeSeparators(params().filesystemPath + item._file);
    const auto result = createPlaceholderInfo(localPath, item._modtime, item._size, item._fileId);
    return result;
}

Result<void, QString> VfsCfApi::dehydratePlaceholder(const SyncFileItem &item)
{
    const auto previousPin = pinState(item._file);

    if (!QFile::remove(_setupParams.filesystemPath + item._file)) {
        return QStringLiteral("Couldn't remove %1 to fulfill dehydration").arg(item._file);
    }

    const auto r = createPlaceholder(item);
    if (!r) {
        return r;
    }

    if (previousPin) {
        if (*previousPin == PinState::AlwaysLocal) {
            setPinState(item._file, PinState::Unspecified);
        } else {
            setPinState(item._file, *previousPin);
        }
    }

    return {};
}

void VfsCfApi::convertToPlaceholder(const QString &filename, const SyncFileItem &item, const QString &replacesFile)
{
    const auto localPath = QDir::toNativeSeparators(filename);
    const auto replacesPath = QDir::toNativeSeparators(replacesFile);
    if (findPlaceholderInfo(localPath)) {
        updatePlaceholderInfo(localPath, item._modtime, item._size, item._fileId, replacesPath);
    } else {
        ::convertToPlaceholder(localPath, item._modtime, item._size, item._fileId, replacesPath);
    }
}

bool VfsCfApi::needsMetadataUpdate(const SyncFileItem &item)
{
    return false;
}

bool VfsCfApi::isDehydratedPlaceholder(const QString &filePath)
{
    const auto path = QDir::toNativeSeparators(filePath).toStdWString();
    const auto attributes = GetFileAttributes(path.data());
    return (attributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
}

bool VfsCfApi::statTypeVirtualFile(csync_file_stat_t *stat, void *statData)
{
    const auto ffd = static_cast<WIN32_FIND_DATA *>(statData);

    const auto isDirectory = (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const auto isSparseFile = (ffd->dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
    const auto isPinned = (ffd->dwFileAttributes & FILE_ATTRIBUTE_PINNED) != 0;
    const auto isUnpinned = (ffd->dwFileAttributes & FILE_ATTRIBUTE_UNPINNED) != 0;
    const auto hasReparsePoint = (ffd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    const auto hasCloudTag = (ffd->dwReserved0 & IO_REPARSE_TAG_CLOUD) != 0;

    // It's a dir with a reparse point due to the placeholder info (hence the cloud tag)
    // if we don't remove the reparse point flag the discovery will end up thinking
    // it is a file... let's prevent it
    if (isDirectory && hasReparsePoint && hasCloudTag) {
        ffd->dwFileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
        return false;
    } else if (isSparseFile && isPinned) {
        stat->type = ItemTypeVirtualFileDownload;
        return true;
    } else if (!isSparseFile && isUnpinned){
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
    const auto localPath = QDir::toNativeSeparators(params().filesystemPath + folderPath);

    auto cfState = CF_PIN_STATE_UNSPECIFIED;
    switch (state) {
    case PinState::AlwaysLocal:
        cfState = CF_PIN_STATE_PINNED;
        break;
    case PinState::Inherited:
        cfState = CF_PIN_STATE_INHERIT;
        break;
    case PinState::OnlineOnly:
        cfState = CF_PIN_STATE_UNPINNED;
        break;
    case PinState::Unspecified:
        cfState = CF_PIN_STATE_UNSPECIFIED;
        break;
    default:
        Q_UNREACHABLE();
        return false;
    }

    return ::setPinState(localPath, cfState, CF_SET_PIN_FLAG_RECURSE);
}

Optional<PinState> VfsCfApi::pinState(const QString &folderPath)
{
    const auto localPath = QDir::toNativeSeparators(params().filesystemPath + folderPath);
    const auto info = findPlaceholderInfo(localPath);
    if (!info) {
        return {};
    }

    switch (info->PinState) {
    case CF_PIN_STATE_UNSPECIFIED:
        return PinState::Unspecified;
    case CF_PIN_STATE_PINNED:
        return PinState::AlwaysLocal;
    case CF_PIN_STATE_UNPINNED:
        return PinState::OnlineOnly;
    case CF_PIN_STATE_INHERIT:
        return PinState::Inherited;
    default:
        Q_UNREACHABLE();
        return {};
    }
}

Vfs::AvailabilityResult VfsCfApi::availability(const QString &folderPath)
{
    const auto basePinState = pinState(folderPath);
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

void VfsCfApi::fileStatusChanged(const QString &systemFileName, SyncFileStatus fileStatus)
{
    Q_UNUSED(systemFileName);
    Q_UNUSED(fileStatus);
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

            const auto path = folderPath + '/' + name;
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

OCC_DEFINE_VFS_FACTORY("win", OCC::VfsCfApi)
