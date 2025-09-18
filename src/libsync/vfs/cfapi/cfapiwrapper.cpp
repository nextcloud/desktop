/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cfapiwrapper.h"

#include "common/utility.h"
#include "common/filesystembase.h"
#include "hydrationjob.h"
#include "theme.h"
#include "vfs_cfapi.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QUuid>

#include <sddl.h>
#include <ntstatus.h>
#include <cfapi.h>
#include <comdef.h>

#include "config.h"

Q_LOGGING_CATEGORY(lcCfApiWrapper, "nextcloud.sync.vfs.cfapi.wrapper", QtInfoMsg)
using namespace Qt::Literals::StringLiterals;

#define FIELD_SIZE( type, field ) ( sizeof( ( (type*)0 )->field ) )
#define CF_SIZE_OF_OP_PARAM( field )                                           \
    ( FIELD_OFFSET( CF_OPERATION_PARAMETERS, field ) +                         \
      FIELD_SIZE( CF_OPERATION_PARAMETERS, field ) )

namespace {

constexpr auto syncRootFlagsFull = 34;
constexpr auto syncRootFlagsNoCfApiContextMenu = 2;

constexpr auto syncRootManagerRegKey = R"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager)";

constexpr auto forbiddenLeadingCharacterInPath = "#";

}


QString createErrorMessageForPlaceholderUpdateAndCreate(const QString &path, const QString &originalErrorMessage)
{
    const auto pathFromNativeSeparators = QDir::fromNativeSeparators(path);
    if (!pathFromNativeSeparators.contains(QStringLiteral("/%1").arg(forbiddenLeadingCharacterInPath))) {
        return originalErrorMessage;
    }
    const auto fileComponents = pathFromNativeSeparators.split("/");
    for (const auto &fileComponent : fileComponents) {
        if (fileComponent.startsWith(forbiddenLeadingCharacterInPath)) {
            qCInfo(lcCfApiWrapper) << "Failed to create/update a placeholder for path \"" << pathFromNativeSeparators << "\" that has a leading '#'.";
            return QString{originalErrorMessage + QStringLiteral(": ") + QObject::tr("Paths beginning with '#' character are not supported in VFS mode.")};
        }
    }
    return originalErrorMessage;
}

DWORD sizeToDWORD(size_t size)
{
    return OCC::Utility::convertSizeToDWORD(size);
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
    case CF_PIN_STATE_EXCLUDED:
        return OCC::PinState::Excluded;
    default:
        Q_UNREACHABLE();
        return OCC::PinState::Inherited;
    }
}

void cfApiSendTransferInfo(const CF_CONNECTION_KEY &connectionKey, const CF_TRANSFER_KEY &transferKey, NTSTATUS status, void *buffer, qint64 offset, qint64 currentBlockLength, qint64 totalLength)
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
    opParams.TransferData.Length.QuadPart = currentBlockLength;

    const qint64 cfExecuteresult = CfExecute(&opInfo, &opParams);
    if (cfExecuteresult != S_OK) {
        qCCritical(lcCfApiWrapper) << "Couldn't send transfer info" << QString::number(transferKey.QuadPart, 16) << ":" << cfExecuteresult << QString::fromWCharArray(_com_error(cfExecuteresult).ErrorMessage());
    }

    const auto isDownloadFinished = ((offset + currentBlockLength) == totalLength);
    if (isDownloadFinished) {
        return;
    }

    // refresh Windows Copy Dialog progress
    LARGE_INTEGER progressTotal;
    progressTotal.QuadPart = totalLength;

    LARGE_INTEGER progressCompleted;
    progressCompleted.QuadPart = offset;

    const qint64 cfReportProgressresult =  CfReportProviderProgress(connectionKey, transferKey, progressTotal, progressCompleted);

    if (cfReportProgressresult != S_OK) {
        qCCritical(lcCfApiWrapper) << "Couldn't report provider progress" << QString::number(transferKey.QuadPart, 16) << ":" << cfReportProgressresult << QString::fromWCharArray(_com_error(cfReportProgressresult).ErrorMessage());
    }
}

void CALLBACK cfApiFetchDataCallback(const CF_CALLBACK_INFO *callbackInfo, const CF_CALLBACK_PARAMETERS *callbackParameters)
{
    qCDebug(lcCfApiWrapper) << "Fetch data callback called. File size:" << callbackInfo->FileSize.QuadPart;
    qCInfo(lcCfApiWrapper) << "Desktop client process id:" << QCoreApplication::applicationPid();

    const auto sendTransferError = [=] {
        cfApiSendTransferInfo(callbackInfo->ConnectionKey,
                              callbackInfo->TransferKey,
                              STATUS_UNSUCCESSFUL,
                              nullptr,
                              callbackParameters->FetchData.RequiredFileOffset.QuadPart,
                              callbackParameters->FetchData.RequiredLength.QuadPart,
                              callbackInfo->FileSize.QuadPart);
    };

    const auto sendTransferInfo = [=](QByteArray &data, qint64 offset) {
        cfApiSendTransferInfo(callbackInfo->ConnectionKey,
                              callbackInfo->TransferKey,
                              STATUS_SUCCESS,
                              data.data(),
                              offset,
                              data.length(),
                              callbackInfo->FileSize.QuadPart);
    };

    auto vfs = reinterpret_cast<OCC::VfsCfApi *>(callbackInfo->CallbackContext);
    Q_ASSERT(vfs->metaObject()->className() == QByteArrayLiteral("OCC::VfsCfApi"));
    const auto path = QString(QString::fromWCharArray(callbackInfo->VolumeDosName) + QString::fromWCharArray(callbackInfo->NormalizedPath));
    const auto requestId = QString::number(callbackInfo->TransferKey.QuadPart, 16);

    if (!callbackInfo->ProcessInfo) {
        qCCritical(lcCfApiWrapper) << "Callback parameters did not contain required process info required for the implicit hydration check, aborting" << path << requestId;
        sendTransferError();
        return;
    }

    qCInfo(lcCfApiWrapper) << "Fetch data requested by process id:" << callbackInfo->ProcessInfo->ProcessId;
    qCInfo(lcCfApiWrapper) << "Fetch data requested by application id:" << QString(QString::fromWCharArray(callbackInfo->ProcessInfo->ApplicationId));

    if (QCoreApplication::applicationPid() == callbackInfo->ProcessInfo->ProcessId) {
        qCCritical(lcCfApiWrapper) << "implicit hydration triggered by the client itself. Will lead to a deadlock. Cancel" << path << requestId;
        sendTransferError();
        return;
    }

    // Set up connections before requesting hydration as it could happen that
    // the hydration request finishes before the connections were set up.
    // The data transfer wouldn't start at all in this case otherwise.
    bool hydrationRequestResult = false;
    QEventLoop loop;
    QObject::connect(vfs, &OCC::VfsCfApi::hydrationRequestReady, &loop, [&](const QString &id) {
        if (requestId == id) {
            hydrationRequestResult = true;
            qCDebug(lcCfApiWrapper) << "Hydration request ready for" << path << requestId;
            loop.quit();
        }
    });
    QObject::connect(vfs, &OCC::VfsCfApi::hydrationRequestFailed, &loop, [&](const QString &id) {
        if (requestId == id) {
            hydrationRequestResult = false;
            qCWarning(lcCfApiWrapper) << "Hydration request failed for" << path << requestId;
            loop.quit();
        }
    });

    qCDebug(lcCfApiWrapper) << "Request hydration for" << path << requestId;

    const auto invokeResult = QMetaObject::invokeMethod(vfs, [=] { vfs->requestHydration(requestId, path); }, Qt::QueuedConnection);
    if (!invokeResult) {
        qCCritical(lcCfApiWrapper) << "Failed to trigger hydration for" << path << requestId;
        QObject::disconnect(vfs, nullptr, &loop, nullptr); // Ensure we properly cancel hydration on server errors
        sendTransferError();
        return;
    }

    qCDebug(lcCfApiWrapper) << "Successfully triggered hydration for" << path << requestId;

    // Block and wait for vfs to signal back the hydration is ready
    qCDebug(lcCfApiWrapper) << "Starting event loop 1";
    loop.exec();
    QObject::disconnect(vfs, nullptr, &loop, nullptr); // Ensure we properly cancel hydration on server errors

    qCInfo(lcCfApiWrapper) << "VFS replied for hydration of" << path << requestId << "status was:" << hydrationRequestResult;
    if (!hydrationRequestResult) {
        qCCritical(lcCfApiWrapper) << "Failed to trigger hydration for" << path << requestId;
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

    QLocalSocket signalSocket;
    const QString signalSocketName = requestId + ":cancellation";
    signalSocket.connectToServer(signalSocketName);
    const auto cancellationSocketConnectResult = signalSocket.waitForConnected();
    if (!cancellationSocketConnectResult) {
        qCWarning(lcCfApiWrapper) << "Couldn't connect the socket" << signalSocketName
                                  << signalSocket.error() << signalSocket.errorString();
        sendTransferError();
        return;
    }

    auto hydrationRequestCancelled = false;
    QObject::connect(&signalSocket, &QLocalSocket::readyRead, &loop, [&] {
        hydrationRequestCancelled = true;
        qCCritical(lcCfApiWrapper) << "Hydration canceled for " << path << requestId;
    });

    // CFAPI expects sent blocks to be of a multiple of a block size.
    // Only the last sent block is allowed to be of a different size than
    // a multiple of a block size
    constexpr auto cfapiBlockSize = 4096;
    qint64 dataOffset = 0;
    QByteArray protrudingData;

    const auto alignAndSendData = [&](const QByteArray &receivedData) {
        QByteArray data = protrudingData + receivedData;
        protrudingData.clear();
        if (data.size() < cfapiBlockSize) {
            protrudingData = data;
            return;
        }
        const auto protudingSize = data.size() % cfapiBlockSize;
        protrudingData = data.right(protudingSize);
        data.chop(protudingSize);
        sendTransferInfo(data, dataOffset);
        dataOffset += data.size();
    };

    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, [&] {
        if (hydrationRequestCancelled) {
            qCDebug(lcCfApiWrapper) << "Don't transfer data because request" << requestId << "was cancelled";
            return;
        }

        const auto receivedData = socket.readAll();
        if (receivedData.isEmpty()) {
            qCWarning(lcCfApiWrapper) << "Unexpected empty data received" << requestId;
            sendTransferError();
            protrudingData.clear();
            loop.quit();
            return;
        }
        alignAndSendData(receivedData);
    });

    QObject::connect(vfs, &OCC::VfsCfApi::hydrationRequestFinished, &loop, [&](const QString &id) {
        qDebug(lcCfApiWrapper) << "Hydration finished for request" << id;
        if (requestId == id) {
            loop.quit();
        }
    });

    qCDebug(lcCfApiWrapper) << "Starting event loop 2";
    loop.exec();

    if (!hydrationRequestCancelled && !protrudingData.isEmpty()) {
        qDebug(lcCfApiWrapper) << "Send remaining protruding data. Size:" << protrudingData.size();
        sendTransferInfo(protrudingData, dataOffset);
    }

    int hydrationJobResult = OCC::HydrationJob::Status::Error;
    const auto invokeFinalizeResult = QMetaObject::invokeMethod(
        vfs, [&hydrationJobResult, vfs, requestId] { return vfs->finalizeHydrationJob(requestId); }, Qt::BlockingQueuedConnection,
        &hydrationJobResult);
    if (!invokeFinalizeResult) {
        qCritical(lcCfApiWrapper) << "Failed to finalize hydration job for" << path << requestId;
    }

    if (static_cast<OCC::HydrationJob::Status>(hydrationJobResult) != OCC::HydrationJob::Success) {
        sendTransferError();
    }
}

enum class CfApiUpdateMetadataType {
    OnlyBasicMetadata,
    AllMetadata,
};

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderState(const QString &path,
                                                                                  time_t modtime,
                                                                                  qint64 size,
                                                                                  const QByteArray &fileId,
                                                                                  const QString &replacesPath,
                                                                                  CfApiUpdateMetadataType updateType)
{
    if (updateType == CfApiUpdateMetadataType::AllMetadata && modtime <= 0) {
        return {QString{"Could not update metadata due to invalid modification time for %1: %2"}.arg(path).arg(modtime)};
    }

    const auto info = replacesPath.isEmpty() ? OCC::CfApiWrapper::findPlaceholderInfo(path)
                                             : OCC::CfApiWrapper::findPlaceholderInfo(replacesPath);
    if (!info) {
        return { "Can't update non existing placeholder info" };
    }

    const auto previousPinState = cfPinStateToPinState(info->PinState);

    CF_FS_METADATA metadata = {};
    metadata.FileSize.QuadPart = size;
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.CreationTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.LastWriteTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.LastAccessTime);
    OCC::Utility::UnixTimeToLargeIntegerFiletime(modtime, &metadata.BasicInfo.ChangeTime);


    qCInfo(lcCfApiWrapper) << "updatePlaceholderState" << path << modtime;
    const qint64 result =
        CfUpdatePlaceholder(OCC::CfApiWrapper::handleForPath(path).get(), updateType == CfApiUpdateMetadataType::AllMetadata ? &metadata : nullptr,
                            fileId.data(), static_cast<DWORD>(fileId.size()), nullptr, 0, CF_UPDATE_FLAG_MARK_IN_SYNC, nullptr, nullptr);

    if (result != S_OK) {
        const auto errorMessage = createErrorMessageForPlaceholderUpdateAndCreate(path, "Couldn't update placeholder info");
        qCWarning(lcCfApiWrapper) << errorMessage << path << ":" << QString::fromWCharArray(_com_error(result).ErrorMessage()) << replacesPath;
        return errorMessage;
    }

           // Pin state tends to be lost on updates, so restore it every time
    if (!setPinState(path, previousPinState, OCC::CfApiWrapper::NoRecurse)) {
        return { "Couldn't restore pin state" };
    }

    return OCC::Vfs::ConvertToPlaceholderResult::Ok;
}

void CALLBACK cfApiCancelFetchData(const CF_CALLBACK_INFO *callbackInfo, const CF_CALLBACK_PARAMETERS * /*callbackParameters*/)
{
    const auto path = QString(QString::fromWCharArray(callbackInfo->VolumeDosName) + QString::fromWCharArray(callbackInfo->NormalizedPath));

    qCInfo(lcCfApiWrapper) << "Cancel fetch data of" << path;
    qCInfo(lcCfApiWrapper) << "Desktop client process id:" << QCoreApplication::applicationPid();
    if (callbackInfo->ProcessInfo) {
        qCInfo(lcCfApiWrapper) << "Cancel fetch data requested by process id:" << callbackInfo->ProcessInfo->ProcessId;
        qCInfo(lcCfApiWrapper) << "Cancel fetch data requested by application id:" << QString(QString::fromWCharArray(callbackInfo->ProcessInfo->ApplicationId));
    }

    auto vfs = reinterpret_cast<OCC::VfsCfApi *>(callbackInfo->CallbackContext);
    Q_ASSERT(vfs->metaObject()->className() == QByteArrayLiteral("OCC::VfsCfApi"));
    const auto requestId = QString::number(callbackInfo->TransferKey.QuadPart, 16);

    const auto invokeResult = QMetaObject::invokeMethod(
        vfs, [=] { vfs->cancelHydration(requestId, path); }, Qt::QueuedConnection);
    if (!invokeResult) {
        qCritical(lcCfApiWrapper) << "Failed to cancel hydration for" << path << requestId;
    }
}

void CALLBACK cfApiNotifyFileOpenCompletion(const CF_CALLBACK_INFO *callbackInfo, const CF_CALLBACK_PARAMETERS * /*callbackParameters*/)
{
    const auto path = QString(QString::fromWCharArray(callbackInfo->VolumeDosName) + QString::fromWCharArray(callbackInfo->NormalizedPath));

    auto vfs = reinterpret_cast<OCC::VfsCfApi *>(callbackInfo->CallbackContext);
    Q_ASSERT(vfs->metaObject()->className() == QByteArrayLiteral("OCC::VfsCfApi"));
    const auto requestId = QString::number(callbackInfo->TransferKey.QuadPart, 16);

    qCDebug(lcCfApiWrapper) << "Open file completion:" << path << requestId;
    qCInfo(lcCfApiWrapper) << "Desktop client process id:" << QCoreApplication::applicationPid();
    if (callbackInfo->ProcessInfo) {
        qCInfo(lcCfApiWrapper) << "Open file completed by process id:" << callbackInfo->ProcessInfo->ProcessId;
        qCInfo(lcCfApiWrapper) << "Open file completed by application id:" << QString(QString::fromWCharArray(callbackInfo->ProcessInfo->ApplicationId));
    }
}

void CALLBACK cfApiValidateData(const CF_CALLBACK_INFO *callbackInfo, const CF_CALLBACK_PARAMETERS * /*callbackParameters*/)
{
    const auto path = QString(QString::fromWCharArray(callbackInfo->VolumeDosName) + QString::fromWCharArray(callbackInfo->NormalizedPath));

    auto vfs = reinterpret_cast<OCC::VfsCfApi *>(callbackInfo->CallbackContext);
    Q_ASSERT(vfs->metaObject()->className() == QByteArrayLiteral("OCC::VfsCfApi"));
    const auto requestId = QString::number(callbackInfo->TransferKey.QuadPart, 16);

    qCDebug(lcCfApiWrapper) << "Validate data:" << path << requestId;
    qCInfo(lcCfApiWrapper) << "Desktop client process id:" << QCoreApplication::applicationPid();
    if (callbackInfo->ProcessInfo) {
        qCInfo(lcCfApiWrapper) << "Validate data requested by process id:" << callbackInfo->ProcessInfo->ProcessId;
        qCInfo(lcCfApiWrapper) << "Validate data requested by application id:" << QString(QString::fromWCharArray(callbackInfo->ProcessInfo->ApplicationId));
    }
}

void CALLBACK cfApiCancelFetchPlaceHolders(const CF_CALLBACK_INFO *callbackInfo, const CF_CALLBACK_PARAMETERS * /*callbackParameters*/)
{
    const auto path = QString(QString::fromWCharArray(callbackInfo->VolumeDosName) + QString::fromWCharArray(callbackInfo->NormalizedPath));

    auto vfs = reinterpret_cast<OCC::VfsCfApi *>(callbackInfo->CallbackContext);
    Q_ASSERT(vfs->metaObject()->className() == QByteArrayLiteral("OCC::VfsCfApi"));
    const auto requestId = QString::number(callbackInfo->TransferKey.QuadPart, 16);

    qCDebug(lcCfApiWrapper) << "Cancel fetch placeholder:" << path << requestId;
    qCInfo(lcCfApiWrapper) << "Desktop client process id:" << QCoreApplication::applicationPid();
    if (callbackInfo->ProcessInfo) {
        qCInfo(lcCfApiWrapper) << "Cancel fetch placeholder requested by process id:" << callbackInfo->ProcessInfo->ProcessId;
        qCInfo(lcCfApiWrapper) << "Cancel fetch placeholder requested by application id:" << QString(QString::fromWCharArray(callbackInfo->ProcessInfo->ApplicationId));
    }
}

void CALLBACK cfApiNotifyFileCloseCompletion(const CF_CALLBACK_INFO *callbackInfo, const CF_CALLBACK_PARAMETERS * /*callbackParameters*/)
{
    const auto path = QString(QString::fromWCharArray(callbackInfo->VolumeDosName) + QString::fromWCharArray(callbackInfo->NormalizedPath));

    auto vfs = reinterpret_cast<OCC::VfsCfApi *>(callbackInfo->CallbackContext);
    Q_ASSERT(vfs->metaObject()->className() == QByteArrayLiteral("OCC::VfsCfApi"));
    const auto requestId = QString::number(callbackInfo->TransferKey.QuadPart, 16);

    qCDebug(lcCfApiWrapper) << "Close file completion:" << path << requestId;
    qCInfo(lcCfApiWrapper) << "Desktop client process id:" << QCoreApplication::applicationPid();
    if (callbackInfo->ProcessInfo) {
        qCInfo(lcCfApiWrapper) << "Close file completion by process id:" << callbackInfo->ProcessInfo->ProcessId;
        qCInfo(lcCfApiWrapper) << "Close file completion by application id:" << QString(QString::fromWCharArray(callbackInfo->ProcessInfo->ApplicationId));
    }
}

CF_CALLBACK_REGISTRATION cfApiCallbacks[] = {
    { CF_CALLBACK_TYPE_FETCH_DATA, cfApiFetchDataCallback },
    { CF_CALLBACK_TYPE_CANCEL_FETCH_DATA, cfApiCancelFetchData },
    { CF_CALLBACK_TYPE_NOTIFY_FILE_OPEN_COMPLETION, cfApiNotifyFileOpenCompletion },
    { CF_CALLBACK_TYPE_NOTIFY_FILE_CLOSE_COMPLETION, cfApiNotifyFileCloseCompletion },
    { CF_CALLBACK_TYPE_VALIDATE_DATA, cfApiValidateData },
    { CF_CALLBACK_TYPE_CANCEL_FETCH_PLACEHOLDERS, cfApiCancelFetchPlaceHolders },
    CF_CALLBACK_REGISTRATION_END
};


std::wstring pathForHandle(const OCC::CfApiWrapper::FileHandle &handle)
{
    wchar_t buffer[MAX_PATH];
    const qint64 result = GetFinalPathNameByHandle(handle.get(), buffer, MAX_PATH, VOLUME_NAME_DOS);
    Q_ASSERT(result < MAX_PATH);
    return std::wstring(buffer);
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
    case OCC::PinState::Excluded:
        return CF_PIN_STATE_EXCLUDED;
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


OCC::CfApiWrapper::FileHandle::FileHandle()
    : _data(nullptr, [](void *) {})
{
}

OCC::CfApiWrapper::FileHandle::FileHandle(void *data, Deleter deleter)
    : _data(data, deleter)
{
}

OCC::CfApiWrapper::PlaceHolderInfo::PlaceHolderInfo(std::vector<char> &&buffer)
    : _data(buffer)
{
}

OCC::Optional<OCC::PinState> OCC::CfApiWrapper::PlaceHolderInfo::pinState() const
{
    if (!this) {
        return {};
    }

    return cfPinStateToPinState(get()->PinState);
}

QString convertSidToStringSid(void *sid)
{
    wchar_t *stringSid = nullptr;
    if (!ConvertSidToStringSid(sid, &stringSid)) {
        return {};
    }

    const auto result = QString::fromWCharArray(stringSid);
    LocalFree(stringSid);
    return result;
}

std::unique_ptr<TOKEN_USER> getCurrentTokenInformation()
{
    const auto tokenHandle = GetCurrentThreadEffectiveToken();

    auto tokenInfoSize = DWORD{0};

    const auto tokenSizeCallSucceeded = ::GetTokenInformation(tokenHandle, TokenUser, nullptr, 0, &tokenInfoSize);
    const auto lastError = GetLastError();
    Q_ASSERT(!tokenSizeCallSucceeded && lastError == ERROR_INSUFFICIENT_BUFFER);
    if (tokenSizeCallSucceeded || lastError != ERROR_INSUFFICIENT_BUFFER) {
        qCCritical(lcCfApiWrapper) << "GetTokenInformation for token size has failed with error" << lastError;
        return {};
    }

    std::unique_ptr<TOKEN_USER> tokenInfo;

    tokenInfo.reset(reinterpret_cast<TOKEN_USER*>(new char[tokenInfoSize]));
    if (!::GetTokenInformation(tokenHandle, TokenUser, tokenInfo.get(), tokenInfoSize, &tokenInfoSize)) {
        qCCritical(lcCfApiWrapper) << "GetTokenInformation failed with error" << lastError;
        return {};
    }

    return tokenInfo;
}

QString retrieveWindowsSid()
{
    if (const auto tokenInfo = getCurrentTokenInformation()) {
        return convertSidToStringSid(tokenInfo->User.Sid);
    }

    return {};
}

bool createSyncRootRegistryKeys(const QString &providerName, const QString &folderAlias, const QString &navigationPaneClsid, const QString &displayName, const QString &accountDisplayName, const QString &syncRootPath)
{
    // We must set specific Registry keys to make the progress bar refresh correctly and also add status icons into Windows Explorer
    // More about this here: https://docs.microsoft.com/en-us/windows/win32/shell/integrate-cloud-storage
    const auto windowsSid = retrieveWindowsSid();
    Q_ASSERT(!windowsSid.isEmpty());
    if (windowsSid.isEmpty()) {
        qCWarning(lcCfApiWrapper) << "Failed to set Registry keys for shell integration, as windowsSid is empty. Progress bar will not work.";
        return false;
    }

    // syncRootId should be: [storage provider ID]![Windows SID]![Account ID]![FolderAlias] (FolderAlias is a custom part added here to be able to register multiple sync folders for the same account)
    // folder registry keys go like: Nextcloud!S-1-5-21-2096452760-2617351404-2281157308-1001!user@nextcloud.lan:8080!0, Nextcloud!S-1-5-21-2096452760-2617351404-2281157308-1001!user@nextcloud.lan:8080!1, etc. for each sync folder
    const auto syncRootId = QStringLiteral("%1!%2!%3!%4").arg(providerName).arg(windowsSid).arg(accountDisplayName).arg(folderAlias);
    const QString providerSyncRootIdRegistryKey = syncRootManagerRegKey + QStringLiteral("\\") + syncRootId;
    const QString providerSyncRootIdUserSyncRootsRegistryKey = providerSyncRootIdRegistryKey + QStringLiteral(R"(\UserSyncRoots\)");

    struct RegistryKeyInfo {
        QString subKey;
        QString valueName;
        int type;
        QVariant value;
    };

    const auto flags = OCC::Theme::instance()->enforceVirtualFilesSyncFolder() ? syncRootFlagsNoCfApiContextMenu : syncRootFlagsFull;

    qCDebug(lcCfApiWrapper) << "syncRootPath" << syncRootPath << " will use DisplayNameResource" << displayName;

    const QVector<RegistryKeyInfo> registryKeysToSet = {
        { providerSyncRootIdRegistryKey, QStringLiteral("Flags"), REG_DWORD, flags },
        { providerSyncRootIdRegistryKey, QStringLiteral("DisplayNameResource"), REG_EXPAND_SZ, displayName },
        { providerSyncRootIdRegistryKey, QStringLiteral("IconResource"), REG_EXPAND_SZ, QString(QDir::toNativeSeparators(qApp->applicationFilePath()) + QStringLiteral(",0")) },
        { providerSyncRootIdUserSyncRootsRegistryKey, windowsSid, REG_SZ, syncRootPath},   
        { providerSyncRootIdRegistryKey, QStringLiteral("CustomStateHandler"), REG_SZ, CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID_REG},
        { providerSyncRootIdRegistryKey, QStringLiteral("ThumbnailProvider"), REG_SZ, CFAPI_SHELLEXT_THUMBNAIL_HANDLER_CLASS_ID_REG},
        { providerSyncRootIdRegistryKey, QStringLiteral("NamespaceCLSID"), REG_SZ, QString(navigationPaneClsid)}
    };

    for (const auto &registryKeyToSet : std::as_const(registryKeysToSet)) {
        if (!OCC::Utility::registrySetKeyValue(HKEY_LOCAL_MACHINE, registryKeyToSet.subKey, registryKeyToSet.valueName, registryKeyToSet.type, registryKeyToSet.value)) {
            qCWarning(lcCfApiWrapper) << "Failed to set Registry keys for shell integration. Progress bar will not work.";
            const auto deleteKeyResult = OCC::Utility::registryDeleteKeyTree(HKEY_LOCAL_MACHINE, providerSyncRootIdRegistryKey);
            Q_ASSERT(!deleteKeyResult);
            return false;
        }
        qCDebug(lcCfApiWrapper) << "Registering keys - subKey:" << registryKeyToSet.subKey 
                                                << " - valueName:" << registryKeyToSet.valueName
                                                << " - type:" << registryKeyToSet.type
                                                << " - value:" << registryKeyToSet.value.toString();
    }

    qCInfo(lcCfApiWrapper) << "Successfully set Registry keys for shell integration at:" << providerSyncRootIdRegistryKey << ". Progress bar will work.";

    return true;
}

bool deleteSyncRootRegistryKey(const QString &syncRootPath, const QString &providerName, const QString &accountDisplayName)
{
    if (OCC::Utility::registryKeyExists(HKEY_LOCAL_MACHINE, syncRootManagerRegKey)) {
        const auto windowsSid = retrieveWindowsSid();
        Q_ASSERT(!windowsSid.isEmpty());
        if (windowsSid.isEmpty()) {
            qCWarning(lcCfApiWrapper) << "Failed to delete Registry key for shell integration on path" << syncRootPath << ". Because windowsSid is empty.";
            return false;
        }

        const auto currentUserSyncRootIdPattern = QStringLiteral("%1!%2!%3").arg(providerName).arg(windowsSid).arg(accountDisplayName);

        bool result = true;

        // walk through each registered syncRootId
        OCC::Utility::registryWalkSubKeys(HKEY_LOCAL_MACHINE, syncRootManagerRegKey, [&](HKEY, const QString &syncRootId) {
            // make sure we have matching syncRootId(providerName!windowsSid!accountDisplayName)
            if (syncRootId.startsWith(currentUserSyncRootIdPattern)) {
                const QString syncRootIdUserSyncRootsRegistryKey = syncRootManagerRegKey + QStringLiteral("\\") + syncRootId + QStringLiteral(R"(\UserSyncRoots\)");
                // check if there is a 'windowsSid' Registry value under \UserSyncRoots and it matches the sync folder path we are removing
                if (OCC::Utility::registryGetKeyValue(HKEY_LOCAL_MACHINE, syncRootIdUserSyncRootsRegistryKey, windowsSid).toString() == syncRootPath) {
                    const QString syncRootIdToDelete = syncRootManagerRegKey + QStringLiteral("\\") + syncRootId;
                    result = OCC::Utility::registryDeleteKeyTree(HKEY_LOCAL_MACHINE, syncRootIdToDelete);
                }
            }
        });
        return result;
    }
    return true;
}

OCC::Result<void, QString> OCC::CfApiWrapper::registerSyncRoot(const QString &path, const QString &providerName, const QString &providerVersion, const QString &folderAlias, const QString &navigationPaneClsid, const QString &displayName, const QString &accountDisplayName)
{
    // even if we fail to register our sync root with shell, we can still proceed with using the VFS
    const auto createRegistryKeyResult = createSyncRootRegistryKeys(providerName, folderAlias, navigationPaneClsid, displayName, accountDisplayName, path);
    Q_ASSERT(createRegistryKeyResult);

    if (!createRegistryKeyResult) {
        qCWarning(lcCfApiWrapper) << "Failed to create the registry key for path:" << path;
    }

    // API is somehow keeping the pointers for longer than one would expect or freeing them itself
    // the internal format of QString is likely the right one for wstring on Windows so there's in fact not necessarily a need to copy
    const auto p = std::wstring(path.toStdWString().data());
    const auto name = std::wstring(providerName.toStdWString().data());
    const auto version = std::wstring(providerVersion.toStdWString().data());

    CF_SYNC_REGISTRATION info = {};
    info.StructSize = sizeof(CF_SYNC_REGISTRATION);
    info.ProviderName = name.data();
    info.ProviderVersion = version.data();

    CF_SYNC_POLICIES policies = {};
    policies.StructSize = sizeof(CF_SYNC_POLICIES);
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

void unregisterSyncRootShellExtensions(const QString &providerName, const QString &folderAlias, const QString &accountDisplayName)
{
    const auto windowsSid = retrieveWindowsSid();
    Q_ASSERT(!windowsSid.isEmpty());
    if (windowsSid.isEmpty()) {
        qCWarning(lcCfApiWrapper) << "Failed to unregister SyncRoot Shell Extensions!";
        return;
    }

    const auto syncRootId = QStringLiteral("%1!%2!%3!%4").arg(providerName).arg(windowsSid).arg(accountDisplayName).arg(folderAlias);

    const QString providerSyncRootIdRegistryKey = syncRootManagerRegKey + QStringLiteral("\\") + syncRootId;

    OCC::Utility::registryDeleteKeyValue(HKEY_LOCAL_MACHINE, providerSyncRootIdRegistryKey, QStringLiteral("ThumbnailProvider"));
    OCC::Utility::registryDeleteKeyValue(HKEY_LOCAL_MACHINE, providerSyncRootIdRegistryKey, QStringLiteral("CustomStateHandler"));

    qCInfo(lcCfApiWrapper) << "Successfully unregistered SyncRoot Shell Extensions!";
}

OCC::Result<void, QString> OCC::CfApiWrapper::unregisterSyncRoot(const QString &path, const QString &providerName, const QString &accountDisplayName)
{
    const auto deleteRegistryKeyResult = deleteSyncRootRegistryKey(path, providerName, accountDisplayName);
    Q_ASSERT(deleteRegistryKeyResult);

    if (!deleteRegistryKeyResult) {
        qCWarning(lcCfApiWrapper) << "Failed to delete the registry key for path:" << path;
    }

    const auto p = path.toStdWString();
    const qint64 result = CfUnregisterSyncRoot(p.data());
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        return QString::fromWCharArray(_com_error(result).ErrorMessage());
    } else {
        return {};
    }
}

OCC::Result<CF_CONNECTION_KEY, QString> OCC::CfApiWrapper::connectSyncRoot(const QString &path, OCC::VfsCfApi *context)
{
    CF_CONNECTION_KEY key;
    const auto p = path.toStdWString();
    const qint64 result =
        CfConnectSyncRoot(p.data(), cfApiCallbacks, context, CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO | CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH | CF_CONNECT_FLAG_BLOCK_SELF_IMPLICIT_HYDRATION, &key);
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        return QString::fromWCharArray(_com_error(result).ErrorMessage());
    } else {
        return { std::move(key) };
    }
}

OCC::Result<void, QString> OCC::CfApiWrapper::disconnectSyncRoot(CF_CONNECTION_KEY &&key)
{
    const qint64 result = CfDisconnectSyncRoot(key);
    if (result != S_OK) {
        qCWarning(lcCfApiWrapper) << "Disconnecting sync root failed" << OCC::Utility::formatWinError(result);
        Q_ASSERT(result == S_OK);
        return OCC::Utility::formatWinError(result);
    } else {
        return {};
    }
}

bool OCC::CfApiWrapper::isAnySyncRoot(const QString &providerName, const QString &accountDisplayName)
{
    const auto windowsSid = retrieveWindowsSid();
    Q_ASSERT(!windowsSid.isEmpty());
    if (windowsSid.isEmpty()) {
        qCWarning(lcCfApiWrapper) << "Could not retrieve Windows Sid.";
        return false;
    }

    const auto syncRootPrefix = QStringLiteral("%1!%2!%3!").arg(providerName).arg(windowsSid).arg(accountDisplayName);

    if (Utility::registryKeyExists(HKEY_LOCAL_MACHINE, syncRootManagerRegKey)) {
        bool foundSyncRoots = false;
        Utility::registryWalkSubKeys(HKEY_LOCAL_MACHINE, syncRootManagerRegKey,
            [&foundSyncRoots, &syncRootPrefix](HKEY key, const QString &subKey) {
                if (subKey.startsWith(syncRootPrefix)) {
                    foundSyncRoots = true;
                }
            });
        return foundSyncRoots;
    }

    return false;
}

bool OCC::CfApiWrapper::isSparseFile(const QString &path)
{
    const auto p = path.toStdWString();
    const auto attributes = GetFileAttributes(p.data());
    return (attributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
}

OCC::CfApiWrapper::FileHandle OCC::CfApiWrapper::handleForPath(const QString &path)
{
    if (path.isEmpty()) {
        qCWarning(lcCfApiWrapper) << "empty path";
        return {};
    }

    if (!FileSystem::fileExists(path)) {
        qCWarning(lcCfApiWrapper) << "does not exist" << path;
        Q_ASSERT(false);
        return {};
    }

    if (FileSystem::isDir(path)) {
        HANDLE handle = nullptr;
        const qint64 openResult = CfOpenFileWithOplock(path.toStdWString().data(), CF_OPEN_FILE_FLAG_NONE, &handle);
        if (openResult == S_OK) {
            return {handle, [](HANDLE h) { CfCloseHandle(h); }};
        } else {
            qCWarning(lcCfApiWrapper) << "Could not open handle for " << path << " result: " << QString::fromWCharArray(_com_error(openResult).ErrorMessage());
        }
    } else if (FileSystem::isFile(path)) {
        const auto longpath = OCC::FileSystem::longWinPath(path);
        const auto handle = CreateFile(longpath.toStdWString().data(), 0, 0, nullptr,
                                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            return {handle, [](HANDLE h) { CloseHandle(h); }};
        } else {
            qCCritical(lcCfApiWrapper) << "Could not CreateFile for longpath:" << longpath << "with error:" << GetLastError();
        }
    }

    qCWarning(lcCfApiWrapper) << "no handle was created";
    return {};
}

OCC::CfApiWrapper::PlaceHolderInfo OCC::CfApiWrapper::findPlaceholderInfo(const QString &path)
{
    if (auto handle = handleForPath(path)) {
        std::vector<char> buffer(512);
        DWORD actualSize = {};
        const qint64 result = CfGetPlaceholderInfo(handle.get(), CF_PLACEHOLDER_INFO_BASIC, buffer.data(), static_cast<DWORD>(buffer.size()), &actualSize);
        if (result == S_OK) {
            buffer.resize(actualSize);
            return PlaceHolderInfo(std::move(buffer));
        } else if (result == HRESULT_FROM_WIN32(ERROR_NOT_A_CLOUD_FILE)) {
            // native file, not yet converted
            return {};
        } else {
            qCWarning(lcCfApiWrapper) << "Failed to retrieve placeholder info:" << Utility::formatWinError(result);
            Q_ASSERT(false);
        }
    }
    return {};
}

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> OCC::CfApiWrapper::setPinState(const QString &path, OCC::PinState state, SetPinRecurseMode mode)
{
    const auto cfState = pinStateToCfPinState(state);
    const auto flags = pinRecurseModeToCfSetPinFlags(mode);

    const qint64 result = CfSetPinState(handleForPath(path).get(), cfState, flags, nullptr);
    if (result == S_OK) {
        return OCC::Vfs::ConvertToPlaceholderResult::Ok;
    } else {
        qCWarning(lcCfApiWrapper) << "Couldn't set pin state" << state << "for" << path << "with recurse mode" << mode << ":" << QString::fromWCharArray(_com_error(result).ErrorMessage());
        return { "Couldn't set pin state" };
    }
}

OCC::Result<void, QString> OCC::CfApiWrapper::createPlaceholderInfo(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId)
{
    if (modtime <= 0) {
        return {QString{"Could not update metadata due to invalid modification time for %1: %2"}.arg(path).arg(modtime)};
    }

    const auto fileInfo = QFileInfo(path);
    const auto localBasePath = QDir::toNativeSeparators(fileInfo.path()).toStdWString();
    const auto relativePath = fileInfo.fileName().toStdWString();

    CF_PLACEHOLDER_CREATE_INFO cloudEntry = {};
    cloudEntry.FileIdentity = fileId.data();
    cloudEntry.FileIdentityLength = static_cast<DWORD>(fileId.length());

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

    qCDebug(lcCfApiWrapper) << "CfCreatePlaceholders" << path << modtime;
    const qint64 result = CfCreatePlaceholders(localBasePath.data(), &cloudEntry, 1, CF_CREATE_FLAG_NONE, nullptr);
    if (result != S_OK) {
        qCWarning(lcCfApiWrapper) << "Couldn't create placeholder info for" << path << ":" << QString::fromWCharArray(_com_error(result).ErrorMessage());
        return { "Couldn't create placeholder info" };
    }

    const auto parentInfo = findPlaceholderInfo(QDir::toNativeSeparators(QFileInfo(path).absolutePath()));
    const auto state = parentInfo && parentInfo->PinState == CF_PIN_STATE_UNPINNED ? CF_PIN_STATE_UNPINNED : CF_PIN_STATE_INHERIT;

    if (!setPinState(path, cfPinStateToPinState(state), NoRecurse)) {
        return { "Couldn't set the default inherit pin state" };
    }

    return {};
}

OCC::Result<void, QString> OCC::CfApiWrapper::createPlaceholdersInfo(const QString &localBasePath, const QList<PlaceholdersInfo> &itemsInfo)
{
    auto filteredItemsInfo = QList<PlaceholdersInfo>{};
    filteredItemsInfo.reserve(itemsInfo.size());

    std::copy_if(itemsInfo.begin(), itemsInfo.end(), std::back_inserter(filteredItemsInfo), [] (const auto &onePlaceholderInfo) -> bool {
        if (onePlaceholderInfo.modtime <= 0) {
            qCWarning(lcCfApiWrapper()) << "Skip invalid modtime file: " << onePlaceholderInfo.relativePath << "modtime:" << onePlaceholderInfo.modtime;
            return false;
        }

        return true;
    });
    const auto stdWStringBasePath = localBasePath.toStdWString();
    auto cloudEntry = std::make_unique<CF_PLACEHOLDER_CREATE_INFO[]>(filteredItemsInfo.size());

    for(auto itemIndice = 0; itemIndice < filteredItemsInfo.size(); ++itemIndice) {
        const auto &placeholderInfo = filteredItemsInfo[itemIndice];

        cloudEntry[itemIndice].FileIdentity = placeholderInfo.fileId.data();
        cloudEntry[itemIndice].FileIdentityLength = static_cast<DWORD>(placeholderInfo.fileId.length());

        cloudEntry[itemIndice].RelativeFileName = placeholderInfo.platformNativeRelativePath.data();
        cloudEntry[itemIndice].Flags = CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC;
        cloudEntry[itemIndice].FsMetadata.FileSize.QuadPart = placeholderInfo.size;
        cloudEntry[itemIndice].FsMetadata.BasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
        OCC::Utility::UnixTimeToLargeIntegerFiletime(placeholderInfo.modtime, &cloudEntry[itemIndice].FsMetadata.BasicInfo.CreationTime);
        OCC::Utility::UnixTimeToLargeIntegerFiletime(placeholderInfo.modtime, &cloudEntry[itemIndice].FsMetadata.BasicInfo.LastWriteTime);
        OCC::Utility::UnixTimeToLargeIntegerFiletime(placeholderInfo.modtime, &cloudEntry[itemIndice].FsMetadata.BasicInfo.LastAccessTime);
        OCC::Utility::UnixTimeToLargeIntegerFiletime(placeholderInfo.modtime, &cloudEntry[itemIndice].FsMetadata.BasicInfo.ChangeTime);

        if (placeholderInfo.fileInfo.isDir()) {
            cloudEntry[itemIndice].Flags |= CF_PLACEHOLDER_CREATE_FLAG_DISABLE_ON_DEMAND_POPULATION;
            cloudEntry[itemIndice].FsMetadata.BasicInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            cloudEntry[itemIndice].FsMetadata.FileSize.QuadPart = 0;
        }
    }

    auto numberOfCreatedPlaceholders = 0ul;
    const qint64 result = CfCreatePlaceholders(stdWStringBasePath.data(), cloudEntry.get(), filteredItemsInfo.size(), CF_CREATE_FLAG_NONE, &numberOfCreatedPlaceholders);
    if (result != S_OK) {
        qCWarning(lcCfApiWrapper) << "Couldn't create placeholders info" << ":" << QString::fromWCharArray(_com_error(result).ErrorMessage()) << "number of placeholders created:" << numberOfCreatedPlaceholders;

        for(auto itemIndice = 0; itemIndice < filteredItemsInfo.size(); ++itemIndice) {
            qCDebug(lcCfApiWrapper) << QString::fromStdWString(cloudEntry[itemIndice].RelativeFileName) << QString::fromWCharArray(_com_error(cloudEntry[itemIndice].Result).ErrorMessage());
        }

        return { "Couldn't create placeholder info" };
    }

    for(auto itemIndice = 0; itemIndice < filteredItemsInfo.size(); ++itemIndice) {
        const auto &placeholderInfo = filteredItemsInfo[itemIndice];
        const auto parentInfo = findPlaceholderInfo(QDir::toNativeSeparators(QFileInfo(localBasePath + QDir::separator() + placeholderInfo.relativePath).absolutePath()));
        const auto state = parentInfo && parentInfo->PinState == CF_PIN_STATE_UNPINNED ? CF_PIN_STATE_UNPINNED : CF_PIN_STATE_INHERIT;

        if (!setPinState(QDir::toNativeSeparators(QFileInfo(localBasePath + QDir::separator() + placeholderInfo.relativePath).absoluteFilePath()), cfPinStateToPinState(state), NoRecurse)) {
            return { "Couldn't set the default inherit pin state" };
        }
    }

    return {};
}

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> OCC::CfApiWrapper::updatePlaceholderInfo(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath)
{
    return updatePlaceholderState(path, modtime, size, fileId, replacesPath, CfApiUpdateMetadataType::AllMetadata);
}

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> OCC::CfApiWrapper::dehydratePlaceholder(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId)
{
    if (modtime <= 0) {
        return {QString{"Could not update metadata due to invalid modification time for %1: %2"}.arg(path).arg(modtime)};
    }

    const auto info = findPlaceholderInfo(path);
    if (info) {
        setPinState(path, OCC::PinState::OnlineOnly, OCC::CfApiWrapper::NoRecurse);

        CF_FILE_RANGE dehydrationRange = {};
        dehydrationRange.Length.QuadPart = size;

        const qint64 result = CfUpdatePlaceholder(handleForPath(path).get(), nullptr, fileId.data(), static_cast<DWORD>(fileId.size()), &dehydrationRange, 1,
                                                  CF_UPDATE_FLAG_MARK_IN_SYNC | CF_UPDATE_FLAG_DEHYDRATE, nullptr, nullptr);
        if (result != S_OK) {
            const auto errorMessage = createErrorMessageForPlaceholderUpdateAndCreate(path, "Couldn't update placeholder info");
            qCWarning(lcCfApiWrapper) << errorMessage << path << ":" << QString::fromWCharArray(_com_error(result).ErrorMessage());
            return errorMessage;
        }
    } else {
        const qint64 result = CfConvertToPlaceholder(handleForPath(path).get(), fileId.data(), static_cast<DWORD>(fileId.size()),
                                                     CF_CONVERT_FLAG_MARK_IN_SYNC | CF_CONVERT_FLAG_DEHYDRATE, nullptr, nullptr);

        if (result != S_OK) {
            const auto errorMessage = createErrorMessageForPlaceholderUpdateAndCreate(path, "Couldn't convert to placeholder");
            qCWarning(lcCfApiWrapper) << errorMessage << path << ":" << QString::fromWCharArray(_com_error(result).ErrorMessage());
            return errorMessage;
        }
    }

    return OCC::Vfs::ConvertToPlaceholderResult::Ok;
}

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> OCC::CfApiWrapper::convertToPlaceholder(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath)
{
    Q_UNUSED(modtime);
    Q_UNUSED(size);

    const qint64 result =
        CfConvertToPlaceholder(handleForPath(path).get(), fileId.data(), static_cast<DWORD>(fileId.size()), CF_CONVERT_FLAG_MARK_IN_SYNC, nullptr, nullptr);
    Q_ASSERT(result == S_OK);
    if (result != S_OK) {
        const auto errorMessage = createErrorMessageForPlaceholderUpdateAndCreate(path, "Couldn't convert to placeholder");
        qCWarning(lcCfApiWrapper) << errorMessage << path << ":" << QString::fromWCharArray(_com_error(result).ErrorMessage());
        return errorMessage;
    }

    if (!replacesPath.isEmpty()) {
        if (const auto originalInfo = findPlaceholderInfo(replacesPath)) {
            const auto state = cfPinStateToPinState(originalInfo->PinState);
            const auto stateResult = setPinState(path, state, NoRecurse);
            Q_ASSERT(stateResult);
            return stateResult;
        }
    }
    const auto stateResult = setPinState(path, PinState::Inherited, NoRecurse);
    Q_ASSERT(stateResult);
    return stateResult;
}

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> OCC::CfApiWrapper::updatePlaceholderMarkInSync(const QString &path, const QByteArray &fileId, const QString &replacesPath)
{
    return updatePlaceholderState(path, {}, {}, fileId, replacesPath, CfApiUpdateMetadataType::OnlyBasicMetadata);
}

bool OCC::CfApiWrapper::isPlaceHolderInSync(const QString &filePath)
{
    if (const auto originalInfo = findPlaceholderInfo(filePath)) {
        return originalInfo->InSyncState == CF_IN_SYNC_STATE_IN_SYNC;
    }

    return true;
}
