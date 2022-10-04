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

//  global compilation flag configuring windows sdk headers
//  preventing inclusion of min and max macros clashing with <limits>
#define NOMINMAX 1

//  override byte to prevent clashes with <cstddef>
#define byte win_byte_override

#include <Windows.h> // gdi plus requires Windows.h
// ...includes for other windows header that may use byte...

//  Define min max macros required by GDI+ headers.
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#else
#error max macro is already defined
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#else
#error min macro is already defined
#endif

#include <gdiplus.h>

//  Undefine min max macros so they won't collide with <limits> header content.
#undef min
#undef max

//  Undefine byte macros so it won't collide with <cstddef> header content.
#undef byte

#include "thumbnailprovider.h"
#include <vector>
#include <shlwapi.h>
#include <QSize>

extern long dllObjectsCount;

namespace VfsShellExtensions {

std::pair<HBITMAP, WTS_ALPHATYPE> hBitmapAndAlphaTypeFromData(const QByteArray &thumbnailData)
{
    if (thumbnailData.isEmpty()) {
        return {NULL, WTSAT_UNKNOWN};
    }

    Gdiplus::Bitmap *gdiPlusBitmap = nullptr;
    ULONG_PTR gdiPlusToken;
    Gdiplus::GdiplusStartupInput gdiPlusStartupInput;
    if (Gdiplus::GdiplusStartup(&gdiPlusToken, &gdiPlusStartupInput, nullptr) != Gdiplus::Status::Ok) {
        return {NULL, WTSAT_UNKNOWN};
    }

    const auto handleFailure = [gdiPlusToken]() -> std::pair<HBITMAP, WTS_ALPHATYPE> {
        Gdiplus::GdiplusShutdown(gdiPlusToken);
        return {NULL, WTSAT_UNKNOWN};
    };

    const std::vector<unsigned char> bitmapData(thumbnailData.begin(), thumbnailData.end());
    auto const stream{::SHCreateMemStream(&bitmapData[0], static_cast<UINT>(bitmapData.size()))};

    if (!stream) {
        return handleFailure();
    }
    gdiPlusBitmap = Gdiplus::Bitmap::FromStream(stream);

    auto hasAlpha = false;
    HBITMAP hBitmap = NULL;
    if (gdiPlusBitmap) {
        hasAlpha = Gdiplus::IsAlphaPixelFormat(gdiPlusBitmap->GetPixelFormat());
        if (gdiPlusBitmap->GetHBITMAP(Gdiplus::Color(0, 0, 0), &hBitmap) != Gdiplus::Status::Ok) {
            return handleFailure();
        }
    }

    Gdiplus::GdiplusShutdown(gdiPlusToken);

    return {hBitmap, hasAlpha ? WTSAT_ARGB : WTSAT_RGB};
}

ThumbnailProvider::ThumbnailProvider()
    : _referenceCount(1)
{
    InterlockedIncrement(&dllObjectsCount);
}

ThumbnailProvider::~ThumbnailProvider()
{
    InterlockedDecrement(&dllObjectsCount);
}
IFACEMETHODIMP ThumbnailProvider::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] = {
        QITABENT(ThumbnailProvider, IInitializeWithItem),
        QITABENT(ThumbnailProvider, IThumbnailProvider),
        {0},
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) ThumbnailProvider::AddRef()
{
    return InterlockedIncrement(&_referenceCount);
}

IFACEMETHODIMP_(ULONG) ThumbnailProvider::Release()
{
    const auto refCount = InterlockedDecrement(&_referenceCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

IFACEMETHODIMP ThumbnailProvider::Initialize(_In_ IShellItem *item, _In_ DWORD mode)
{
    HRESULT hresult = item->QueryInterface(__uuidof(_shellItem), reinterpret_cast<void **>(&_shellItem));
    if (FAILED(hresult)) {
        return hresult;
    }

    LPWSTR pszName = NULL;
    hresult = _shellItem->GetDisplayName(SIGDN_FILESYSPATH, &pszName);
    if (FAILED(hresult)) {
        return hresult;
    }

    _shellItemPath = QString::fromWCharArray(pszName);

    return S_OK;
}

IFACEMETHODIMP ThumbnailProvider::GetThumbnail(_In_ UINT cx, _Out_ HBITMAP *bitmap, _Out_ WTS_ALPHATYPE *alphaType)
{
    *bitmap = nullptr;
    *alphaType = WTSAT_UNKNOWN;

    const auto thumbnailDataReceived = _thumbnailProviderIpc.fetchThumbnailForFile(_shellItemPath, QSize(cx, cx));

    if (thumbnailDataReceived.isEmpty()) {
        return E_FAIL;
    }

    const auto bitmapAndAlphaType = hBitmapAndAlphaTypeFromData(thumbnailDataReceived);
    if (!bitmapAndAlphaType.first) {
        return E_FAIL;
    }
    *bitmap = bitmapAndAlphaType.first;
    *alphaType = bitmapAndAlphaType.second;

    return S_OK;
}
}
