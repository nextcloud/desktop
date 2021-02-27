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

#include "cfapiwrapper.h"

#include "common/utility.h"
#include "hydrationjob.h"
#include "vfs_cfapi.h"

#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QLoggingCategory>

#include <cfapi.h>
#include <comdef.h>
#include <ntstatus.h>

Q_LOGGING_CATEGORY(lcCfApiWrapper, "nextcloud.sync.vfs.cfapi.wrapper", QtInfoMsg)

#define FIELD_SIZE( type, field ) ( sizeof( ( (type*)0 )->field ) )
#define CF_SIZE_OF_OP_PARAM( field )                                           \
    ( FIELD_OFFSET( CF_OPERATION_PARAMETERS, field ) +                         \
      FIELD_SIZE( CF_OPERATION_PARAMETERS, field ) )

namespace {
void cfApiSendTransferInfo(const CF_CONNECTION_KEY &connectionKey, const CF_TRANSFER_KEY &transferKey, NTSTATUS status, void *buffer, qint64 offset, qint64 length)
{

    CF_OPERATION_INFO opInfo = { 0 };
    CF_OPERATION_PARAMETERS opParams = { 0 };

    opInfo.StructSize = sizeof(opInfo);
    opInfo.Type = CF_OPERATION_TYPE_TRANSFER_DATA;
    opInfo.ConnectionKey = connectionKey;
    opInfo.TransferKey = transferKey;
    opParams.ParamSize = CF_SIZE_OF_OP_PARAM(TransferData);
    opParams.TransferData.CompletionStatus = status;
    opParams.TransferData.Buffer = buffer;
    opParams.TransferData.Offset.QuadPart = offset;
    opParams.TransferData.Length.QuadPart = length;

    const qint64 result = CfExecute(&opInfo, &opParams);
    if (result != S_OK) {
        qCCritical(lcCfApiWrapper) << "Couldn't send transfer info" << QString::number(transferKey.QuadPart, 16) << ":" << result << QString::fromWCharArray(_com_error(result).ErrorMessage());
    }
}


void CALLBACK cfApiFetchDataCallback(const CF_CALLBACK_INFO *callbackInfo, const CF_CALLBACK_PARAMETERS *callbackParameters)
{
    const auto sendTransferError = [=] {
        cfApiSendTransferInfo(callbackInfo->ConnectionKey,
                              callbackInfo->TransferKey,
                              STATUS_UNSUCCESSFUL,
                              nullptr,
                              callbackParameters->FetchData.RequiredFileOffset.QuadPart,
                              callbackParameters->FetchData.RequiredLength.QuadPart);
    };

    const auto sendTransferInfo = [=](QByteArray &data, qint64 offset) {
        cfApiSendTransferInfo(callbackInfo->ConnectionKey,
                              callbackInfo->TransferKey,
                              STATUS_SUCCESS,
                              data.data(),
                              offset,
                              data.length());
    };

    auto vfs = reinterpret_cast<OCC::VfsCfApi *>(callbackInfo->CallbackContext);
    Q_ASSERT(vfs->metaObject()->className() == QByteArrayLiteral("OCC::VfsCfApi"));
    const auto path = QString(QString::fromWCharArray(callbackInfo->VolumeDosName) + QString::fromWCharArray(callbackInfo->NormalizedPath));
    const auto requestId = QString::number(callbackInfo->TransferKey.QuadPart, 16);

    const auto invokeResult = QMetaObject::invokeMethod(vfs, [=] { vfs->requestHydration(requestId, path); }, Qt::QueuedConnection);
    if (!invokeResult) {
        qCCritical(lcCfApiWrapper) << "Failed to trigger hydration for" << path << requestId;
        sendTransferError();
        return;
    }

    // Block and wait for vfs to signal back the hydration is ready
    bool hydrationRequestResult = false;
    QEventLoop loop;
    QObject::connect(vfs, &OCC::VfsCfApi::hydrationRequestReady, &loop, [&](const QString &id) {
        if (requestId == id) {
            hydrationRequestResult = true;
            loop.quit();
        }
    });
    QObject::connect(vfs, &OCC::VfsCfApi::hydrationRequestFailed, &loop, [&](const QString &id) {
        if (requestId == id) {
            hydrationRequestResult = false;
            loop.quit();
        }
    });
    loop.exec();
    QObject::disconnect(vfs, nullptr, &loop, nullptr);
    qCInfo(lcCfApiWrapper) << "VFS replied for hydration of" << path << requestId << "status was:" << hydrationRequestResult;

    if (!hydrationRequestResult) {
        sendTransferError();
        return;
    }

    QLocalSocket socket;
    socket.connectToServer(requestId);
    const auto connectResult = socket.waitForConnected();
    if (!connectResult) {
        qCWarning(lcCfApiWrapper) << "Couldn't connect the socket" << requestId << socket.error() << socket.errorString();
        sendTransferError();
        return;
    }

    qint64 offset = 0;

    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, [&] {
        auto data = socket.readAll();
        if (data.isEmpty()) {
            qCWarning(lcCfApiWrapper) << "Unexpected empty data received" << requestId;
            sendTransferError();
            loop.quit();
            return;
        }
        sendTransferInfo(data, offset);
        offset += data.length();
    });

    QObject::connect(vfs, &OCC::VfsCfApi::hydrationRequestFinished, &loop, [&](const QString &id, int s) {
        if (requestId == id) {
            const auto status = static_cast<OCC::HydrationJob::Status>(s);
            qCInfo(lcCfApiWrapper) << "Hydration done for" << path << requestId << status;
            if (status != OCC::HydrationJob::Success) {
                sendTransferError();
            }
            loop.quit();
        }
    });

    loop.exec();
}

CF_CALLBACK_REGISTRATION cfApiCallbacks[] = {
    { CF_CALLBACK_TYPE_FETCH_DATA, cfApiFetchDataCallback },
    CF_CALLBACK_REGISTRATION_END
};

DWORD sizeToDWORD(size_t size)
{
    return OCC::Utility::convertSizeToDWORD(size);
}

void deletePlaceholderInfo(CF_PLACEHOLDER_BASIC_INFO *info)
{
    auto byte = reinterpret_cast<char *>(info);
    delete[] byte;
}

std::wstring pathForHandle(const OCC::CfApiWrapper::FileHandle &handle)
{
    wchar_t buffer[MAX_PATH];
    const qint64 result = GetFinalPathNameByHandle(handle.get(), buffer, MAX_PATH, VOLUME_NAME_DOS);
    Q_ASSERT(result < MAX_PATH);
    return std::wstring(buffer);
}

OCC::PinState cfPinStateToPinState(CF_PIN_STATE state)
{
    switch (state) {
    case CF_PIN_STATE_UNSPECIFIED:
        return OCC::PinState::Unspecified;
    case CF_PIN_STATE_PINNED:
        return OCC::PinState::AlwaysLocal;
    case CF_PIN_STATE_UNPINNED:
        return OCC::PinState::OnlineOnly;
    case CF_PIN_STATE_INHERIT:
        return OCC::PinState::Inherited;
    default:
        Q_UNREACHABLE();
        return OCC::PinState::Inherited;
    }
}

CF_PIN_STATE pinStateToCfPinState(OCC::PinState state)
{
    switch (state) {
    case OCC::PinState::Inherited:
        return CF_PIN_STATE_INHERIT;
    case OCC::PinState::AlwaysLocal:
        return CF_PIN_STATE_PINNED;
    case OCC::PinState::OnlineOnly:
        return CF_PIN_STATE_UNPINNED;
    case OCC::PinState::Unspecified:
        return CF_PIN_STATE_UNSPECIFIED;
    default:
        Q_UNREACHABLE();
        return CF_PIN_STATE_UNSPECIFIED;
    }
}

CF_SET_PIN_FLAGS pinRecurseModeToCfSetPinFlags(OCC::CfApiWrapper::SetPinRecurseMode mode)
{
    switch (mode) {
    case OCC::CfApiWrapper::NoRecurse:
        return CF_SET_PIN_FLAG_NONE;
    case OCC::CfApiWrapper::Recurse:
        return CF_SET_PIN_FLAG_RECURSE;
    case OCC::CfApiWrapper::ChildrenOnly:
        return CF_SET_PIN_FLAG_RECURSE_ONLY;
    default:
        Q_UNREACHABLE();
        return CF_SET_PIN_FLAG_NONE;
    }
}

}

OCC::CfApiWrapper::ConnectionKey::ConnectionKey()
    : _data(new CF_CONNECTION_KEY, [](void *p) { delete reinterpret_cast<CF_CONNECTION_KEY *>(p); })
{
}

OCC::CfApiWrapper::FileHandle::FileHandle()
    : _data(nullptr, [](void *) {})
{
}

OCC::CfApiWrapper::FileHandle::FileHandle(void *data, Deleter deleter)
    : _data(data, deleter)
{
}

OCC::CfApiWrapper::PlaceHolderInfo::PlaceHolderInfo()
    : _data(nullptr, [](CF_PLACEHOLDER_BASIC_INFO *) {})
{
}

OCC::CfApiWrapper::PlaceHolderInfo::PlaceHolderInfo(CF_PLACEHOLDER_BASIC_INFO *data, Deleter deleter)
    : _data(data, deleter)
{
}

OCC::Optional<OCC::PinStateEnums::PinState> OCC::CfApiWrapper::PlaceHolderInfo::pinState() const
{
    Q_ASSERT(_data);
    if (!_data) {
        return {};
    }

    return cfPinStateToPinState(_data->PinState);
}

OCC::Result<void, QString> OCC::CfApiWrapper::registerSyncRoot(const QString &path, const QString &providerName, const QString &providerVersion)
{
    const auto p = path.toStdWString();
    const auto name = providerName.toStdWString();
    const auto version = providerVersion.toStdWString();

    CF_SYNC_REGISTRATION info;
    info.ProviderName = name.data();
    info.ProviderVersion = version.data();
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

    const qint64 result = CfRegisterSyncRoot(p.data(), &info, &policies, CF_REGISTER_FLAG_UPDATE);
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        return QString::fromWCharArray(_com_error(result).ErrorMessage());
    } else {
        return {};
    }
}

OCC::Result<void, QString> OCC::CfApiWrapper::unegisterSyncRoot(const QString &path)
{
    const auto p = path.toStdWString();
    const qint64 result = CfUnregisterSyncRoot(p.data());
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        return QString::fromWCharArray(_com_error(result).ErrorMessage());
    } else {
        return {};
    }
}

OCC::Result<OCC::CfApiWrapper::ConnectionKey, QString> OCC::CfApiWrapper::connectSyncRoot(const QString &path, OCC::VfsCfApi *context)
{
    auto key = ConnectionKey();
    const auto p = path.toStdWString();
    const qint64 result = CfConnectSyncRoot(p.data(),
                                            cfApiCallbacks,
                                            context,
                                            CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO | CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH,
                                            static_cast<CF_CONNECTION_KEY *>(key.get()));
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        return QString::fromWCharArray(_com_error(result).ErrorMessage());
    } else {
        return key;
    }
}

OCC::Result<void, QString> OCC::CfApiWrapper::disconnectSyncRoot(ConnectionKey &&key)
{
    const qint64 result = CfDisconnectSyncRoot(*static_cast<CF_CONNECTION_KEY *>(key.get()));
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        return QString::fromWCharArray(_com_error(result).ErrorMessage());
    } else {
        return {};
    }
}

bool OCC::CfApiWrapper::isSparseFile(const QString &path)
{
    const auto p = path.toStdWString();
    const auto attributes = GetFileAttributes(p.data());
    return (attributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
}

OCC::CfApiWrapper::FileHandle OCC::CfApiWrapper::handleForPath(const QString &path)
{
    if (QFileInfo(path).isDir()) {
        HANDLE handle = nullptr;
        const qint64 openResult = CfOpenFileWithOplock(path.toStdWString().data(), CF_OPEN_FILE_FLAG_NONE, &handle);
        if (openResult == S_OK) {
            return {handle, [](HANDLE h) { CfCloseHandle(h); }};
        }
    } else {
        const auto handle = CreateFile(path.toStdWString().data(), 0, 0, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            return {handle, [](HANDLE h) { CloseHandle(h); }};
        }
    }

    return {};
}

OCC::CfApiWrapper::PlaceHolderInfo OCC::CfApiWrapper::findPlaceholderInfo(const FileHandle &handle)
{
    Q_ASSERT(handle);

    constexpr auto fileIdMaxLength = 128;
    const auto infoSize = sizeof(CF_PLACEHOLDER_BASIC_INFO) + fileIdMaxLength;
    auto info = PlaceHolderInfo(reinterpret_cast<CF_PLACEHOLDER_BASIC_INFO *>(new char[infoSize]), deletePlaceholderInfo);
    const qint64 result = CfGetPlaceholderInfo(handle.get(), CF_PLACEHOLDER_INFO_BASIC, info.get(), sizeToDWORD(infoSize), nullptr);

    if (result == S_OK) {
        return info;
    } else {
        return {};
    }
}

OCC::Result<void, QString> OCC::CfApiWrapper::setPinState(const FileHandle &handle, PinState state, SetPinRecurseMode mode)
{
    const auto cfState = pinStateToCfPinState(state);
    const auto flags = pinRecurseModeToCfSetPinFlags(mode);

    const qint64 result = CfSetPinState(handle.get(), cfState, flags, nullptr);
    if (result == S_OK) {
        return {};
    } else {
        qCWarning(lcCfApiWrapper) << "Couldn't set pin state" << state << "for" << pathForHandle(handle) << "with recurse mode" << mode << ":" << _com_error(result).ErrorMessage();
        return "Couldn't set pin state";
    }
}

OCC::Result<void, QString> OCC::CfApiWrapper::createPlaceholderInfo(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId)
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
        qCWarning(lcCfApiWrapper) << "Couldn't create placeholder info for" << path << ":" << _com_error(result).ErrorMessage();
        return "Couldn't create placeholder info";
    }

    const auto parentHandle = handleForPath(QDir::toNativeSeparators(QFileInfo(path).absolutePath()));
    const auto parentInfo = findPlaceholderInfo(parentHandle);
    const auto state = parentInfo && parentInfo->PinState == CF_PIN_STATE_UNPINNED ? CF_PIN_STATE_UNPINNED : CF_PIN_STATE_INHERIT;

    const auto handle = handleForPath(path);
    if (!setPinState(handle, cfPinStateToPinState(state), NoRecurse)) {
        return "Couldn't set the default inherit pin state";
    }

    return {};
}

OCC::Result<void, QString> OCC::CfApiWrapper::updatePlaceholderInfo(const FileHandle &handle, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath)
{
    Q_ASSERT(handle);

    const auto info = replacesPath.isEmpty() ? findPlaceholderInfo(handle)
                                             : findPlaceholderInfo(handleForPath(replacesPath));
    if (!info) {
        return "Can't update non existing placeholder info";
    }

    const auto previousPinState = cfPinStateToPinState(info->PinState);
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
        qCWarning(lcCfApiWrapper) << "Couldn't update placeholder info for" << pathForHandle(handle) << ":" << _com_error(result).ErrorMessage();
        return "Couldn't update placeholder info";
    }

    // Pin state tends to be lost on updates, so restore it every time
    if (!setPinState(handle, previousPinState, NoRecurse)) {
        return "Couldn't restore pin state";
    }

    return {};
}

OCC::Result<void, QString> OCC::CfApiWrapper::convertToPlaceholder(const FileHandle &handle, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath)
{
    Q_ASSERT(handle);

    const auto fileIdentity = QString::fromUtf8(fileId).toStdWString();
    const auto fileIdentitySize = (fileIdentity.length() + 1) * sizeof(wchar_t);
    const qint64 result = CfConvertToPlaceholder(handle.get(), fileIdentity.data(), sizeToDWORD(fileIdentitySize), CF_CONVERT_FLAG_NONE, nullptr, nullptr);
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        qCCritical(lcCfApiWrapper) << "Couldn't convert to placeholder" << pathForHandle(handle) << ":" << _com_error(result).ErrorMessage();
        return "Couldn't convert to placeholder";
    }

    const auto originalHandle = handleForPath(replacesPath);
    const auto originalInfo = originalHandle ? findPlaceholderInfo(originalHandle) : PlaceHolderInfo(nullptr, deletePlaceholderInfo);
    if (!originalInfo) {
        const auto stateResult = setPinState(handle, PinState::Inherited, NoRecurse);
        Q_ASSERT(stateResult);
        return stateResult;
    } else {
        const auto state = cfPinStateToPinState(originalInfo->PinState);
        const auto stateResult = setPinState(handle, state, NoRecurse);
        Q_ASSERT(stateResult);
        return stateResult;
    }
}
