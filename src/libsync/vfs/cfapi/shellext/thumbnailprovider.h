/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "thumbnailprovideripc.h"

#include "config.h"
#include "thumbcache.h"

#include <QString>

#include <ntstatus.h>
#include <comdef.h>

namespace VfsShellExtensions {
std::pair<HBITMAP, WTS_ALPHATYPE> hBitmapAndAlphaTypeFromData(const QByteArray &thumbnailData);

_COM_SMARTPTR_TYPEDEF(IShellItem2, IID_IShellItem2);

class __declspec(uuid(CFAPI_SHELLEXT_THUMBNAIL_HANDLER_CLASS_ID)) ThumbnailProvider : public IInitializeWithItem,
                                                                                      public IThumbnailProvider
{
public:
    ThumbnailProvider();

    virtual ~ThumbnailProvider();

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFACEMETHODIMP_(ULONG) AddRef();

    IFACEMETHODIMP_(ULONG) Release();

    IFACEMETHODIMP Initialize(_In_ IShellItem *item, _In_ DWORD mode);

    IFACEMETHODIMP GetThumbnail(_In_ UINT cx, _Out_ HBITMAP *bitmap, _Out_ WTS_ALPHATYPE *alphaType);

private:
    long _referenceCount;

    IShellItem2Ptr _shellItem;
    QString _shellItemPath;
    ThumbnailProviderIpc _thumbnailProviderIpc;
};
}
