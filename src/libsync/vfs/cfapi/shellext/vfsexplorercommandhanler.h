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
#include "config.h"
#include <winrt/Windows.Foundation.h>
#include <ShlObj_core.h>

class __declspec(uuid(CFAPI_SHELLEXT_COMMAND_HANDLER_CLASS_ID)) VfsExplorerCommandHandler
    : public IExplorerCommand, IObjectWithSite
{
public:
    VfsExplorerCommandHandler();
    virtual ~VfsExplorerCommandHandler() = default;
    // IExplorerCommand
    IFACEMETHODIMP GetTitle(_In_opt_ IShellItemArray *items, _Outptr_result_nullonfailure_ PWSTR *name);
    IFACEMETHODIMP GetState(_In_opt_ IShellItemArray *, _In_ BOOL, _Out_ EXPCMDSTATE *cmdState);
    IFACEMETHODIMP Invoke(_In_opt_ IShellItemArray *, _In_opt_ IBindCtx *);
    IFACEMETHODIMP GetFlags(_Out_ EXPCMDFLAGS *flags);

    // Not implemented methods in IExplorerCommand for this sample
    IFACEMETHODIMP GetIcon(_In_opt_ IShellItemArray *, _Outptr_result_nullonfailure_ PWSTR *icon)
    {
        *icon = nullptr;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetToolTip(_In_opt_ IShellItemArray *, _Outptr_result_nullonfailure_ PWSTR *infoTip)
    {
        *infoTip = nullptr;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP GetCanonicalName(_Out_ GUID *guidCommandName)
    {
        *guidCommandName = GUID_NULL;
        return E_NOTIMPL;
    }
    IFACEMETHODIMP EnumSubCommands(_COM_Outptr_ IEnumExplorerCommand **enumCommands)
    {
        *enumCommands = nullptr;
        return E_NOTIMPL;
    }

    // IObjectWithSite
    IFACEMETHODIMP SetSite(_In_opt_ IUnknown *site);
    IFACEMETHODIMP GetSite(_In_ REFIID riid, _COM_Outptr_ void **site);

    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFACEMETHODIMP_(ULONG) AddRef();

    IFACEMETHODIMP_(ULONG) Release();

private:
    long _referenceCount;
    winrt::com_ptr<IUnknown> _site;
};
