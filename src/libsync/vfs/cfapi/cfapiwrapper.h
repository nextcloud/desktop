/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

// include order is important, this must be included before cfapi
#include <windows.h>
#include <winternl.h>

#include <cfapi.h>

#include "cfapiexport.h"
#include "common/pinstate.h"
#include "common/result.h"
#include "common/vfs.h"

#include <QFileInfo>

#include <memory>

struct CF_PLACEHOLDER_BASIC_INFO;

namespace OCC {

class VfsCfApi;

namespace CfApiWrapper
{

class NEXTCLOUD_CFAPI_EXPORT ConnectionKey
{
public:
    ConnectionKey();
    inline void *get() const { return _data.get(); }

private:
    std::unique_ptr<void, void(*)(void *)> _data;
};

class NEXTCLOUD_CFAPI_EXPORT FileHandle
{
public:
    using Deleter = void (*)(void *);

    FileHandle();
    FileHandle(void *data, Deleter deleter);

    inline void *get() const { return _data.get(); }
    inline explicit operator bool() const noexcept { return static_cast<bool>(_data); }

private:
    std::unique_ptr<void, void(*)(void *)> _data;
};

class NEXTCLOUD_CFAPI_EXPORT PlaceHolderInfo
{
public:
    PlaceHolderInfo(std::vector<char> &&buffer = {});

    inline auto *get() const noexcept { return reinterpret_cast<CF_PLACEHOLDER_BASIC_INFO *>(const_cast<char *>(_data.data())); }
    inline auto *operator->() const noexcept { return get(); }
    inline explicit operator bool() const noexcept { return !_data.empty(); }

    inline auto size() const { return _data.size(); }

    Optional<PinState> pinState() const;

private:
    std::vector<char> _data;
};

NEXTCLOUD_CFAPI_EXPORT Result<void, QString> registerSyncRoot(const QString &path, const QString &providerName, const QString &providerVersion, const QString &folderAlias, const QString &navigationPaneClsid, const QString &displayName, const QString &accountDisplayName);
NEXTCLOUD_CFAPI_EXPORT void unregisterSyncRootShellExtensions(const QString &providerName, const QString &folderAlias, const QString &accountDisplayName);
NEXTCLOUD_CFAPI_EXPORT Result<void, QString> unregisterSyncRoot(const QString &path, const QString &providerName, const QString &accountDisplayName);

Result<CF_CONNECTION_KEY, QString> connectSyncRoot(const QString &path, VfsCfApi *context);
Result<void, QString> disconnectSyncRoot(CF_CONNECTION_KEY &&key);
NEXTCLOUD_CFAPI_EXPORT bool isAnySyncRoot(const QString &providerName, const QString &accountDisplayName);

NEXTCLOUD_CFAPI_EXPORT bool isSparseFile(const QString &path);

NEXTCLOUD_CFAPI_EXPORT FileHandle handleForPath(const QString &path);

PlaceHolderInfo findPlaceholderInfo(const QString &path);

enum SetPinRecurseMode {
    NoRecurse = 0,
    Recurse,
    ChildrenOnly
};

NEXTCLOUD_CFAPI_EXPORT Result<OCC::Vfs::ConvertToPlaceholderResult, QString> setPinState(const QString &path, PinState state, SetPinRecurseMode mode);
NEXTCLOUD_CFAPI_EXPORT Result<void, QString> createPlaceholderInfo(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId);

struct PlaceholdersInfo {
    QFileInfo fileInfo;
    QString relativePath;
    std::wstring platformNativeRelativePath;
    QByteArray fileId;
    time_t modtime;
    qint64 size;
};

NEXTCLOUD_CFAPI_EXPORT Result<void, QString> createPlaceholdersInfo(const QString &localBasePath, const QList<PlaceholdersInfo> &itemsInfo);
NEXTCLOUD_CFAPI_EXPORT Result<OCC::Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderInfo(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath = QString());
NEXTCLOUD_CFAPI_EXPORT Result<OCC::Vfs::ConvertToPlaceholderResult, QString> convertToPlaceholder(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId, const QString &replacesPath);
NEXTCLOUD_CFAPI_EXPORT Result<OCC::Vfs::ConvertToPlaceholderResult, QString> dehydratePlaceholder(const QString &path, time_t modtime, qint64 size, const QByteArray &fileId);
NEXTCLOUD_CFAPI_EXPORT Result<OCC::Vfs::ConvertToPlaceholderResult, QString> updatePlaceholderMarkInSync(const QString &path, const QByteArray &fileId, const QString &replacesPath = QString());
NEXTCLOUD_CFAPI_EXPORT bool isPlaceHolderInSync(const QString &filePath);

}

} // namespace OCC
