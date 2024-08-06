/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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
