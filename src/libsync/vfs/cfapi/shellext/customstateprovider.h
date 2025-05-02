/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "Generated/CfApiShellExtensions/customstateprovider.g.h"
#include "config.h"
#include <winrt/windows.foundation.collections.h>
#include <windows.storage.provider.h>
#include <QString>
#include <QMap>

namespace winrt::CfApiShellExtensions::implementation {
class __declspec(uuid(CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID)) CustomStateProvider
    : public CustomStateProviderT<CustomStateProvider>
{
public:
    CustomStateProvider();
    virtual ~CustomStateProvider();
    Windows::Foundation::Collections::IIterable<Windows::Storage::Provider::StorageProviderItemProperty>
    GetItemProperties(_In_ hstring const &itemPath);

    static void setDllFilePath(LPCTSTR dllFilePath);

private:
    static QString _dllFilePath;
    static HINSTANCE _dllhInstance;
    QMap<int, bool> _stateIconsAvailibility;
};
}

namespace winrt::CfApiShellExtensions::factory_implementation {
struct CustomStateProvider : CustomStateProviderT<CustomStateProvider, implementation::CustomStateProvider>
{
};
}
