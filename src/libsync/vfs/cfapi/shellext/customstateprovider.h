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
#include "Generated/CfApiShellExtensions/customstateprovider.g.h"
#include "config.h"
#include <winrt/windows.foundation.collections.h>
#include <windows.storage.provider.h>

namespace winrt::CfApiShellExtensions::implementation {
struct __declspec(uuid(CFAPI_SHELLEXT_CUSTOM_STATE_HANDLER_CLASS_ID)) CustomStateProvider
    : CustomStateProviderT<CustomStateProvider>
{
public:
    CustomStateProvider() = default;
    Windows::Foundation::Collections::IIterable<Windows::Storage::Provider::StorageProviderItemProperty>
    GetItemProperties(_In_ hstring const &itemPath);
};
}

namespace winrt::CfApiShellExtensions::factory_implementation {
struct CustomStateProvider : CustomStateProviderT<CustomStateProvider, implementation::CustomStateProvider>
{
};
}
