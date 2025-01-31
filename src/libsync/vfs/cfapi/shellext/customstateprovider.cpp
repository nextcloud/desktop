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

#include "customstateprovider.h"
#include "customstateprovideripc.h"
#include <Shlguid.h>

extern long dllObjectsCount;

namespace winrt::CfApiShellExtensions::implementation {

CustomStateProvider::CustomStateProvider()
{
    InterlockedIncrement(&dllObjectsCount);
}

CustomStateProvider::~CustomStateProvider()
{
    InterlockedDecrement(&dllObjectsCount);
}

winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Storage::Provider::StorageProviderItemProperty>
CustomStateProvider::GetItemProperties(hstring const &itemPath)
{
    std::vector<winrt::Windows::Storage::Provider::StorageProviderItemProperty> properties;

    if (_dllFilePath.isEmpty()) {
        return winrt::single_threaded_vector(std::move(properties));
    }

    const auto itemPathString = QString::fromStdString(winrt::to_string(itemPath));

    const auto isItemPathValid = [&itemPathString]() {
        if (itemPathString.isEmpty()) {
            return false;
        }

        const auto itemPathSplit = itemPathString.split(QStringLiteral("\\"), Qt::SkipEmptyParts);

        if (itemPathSplit.size() > 0) {
            const auto itemName = itemPathSplit.last();
            return !itemName.startsWith(QStringLiteral(".sync_")) && !itemName.startsWith(QStringLiteral(".owncloudsync.log"));
        }

        return true;
    }();

    if (!isItemPathValid) {
        return winrt::single_threaded_vector(std::move(properties));
    }

    VfsShellExtensions::CustomStateProviderIpc customStateProviderIpc;

    const auto states = customStateProviderIpc.fetchCustomStatesForFile(itemPathString);

    for (const auto &state : states) {
        const auto stateValue = state.canConvert<int>() ? state.toInt() : -1;

        if (stateValue >= 0) {
            auto foundAvalability = _stateIconsAvailibility.constFind(stateValue);
            if (foundAvalability == std::cend(_stateIconsAvailibility)) {
                const auto hIcon = ExtractIcon(NULL, _dllFilePath.toStdWString().c_str(), stateValue);
                _stateIconsAvailibility[stateValue] = hIcon != NULL;
                if (hIcon) {
                    DestroyIcon(hIcon); 
                }
                foundAvalability = _stateIconsAvailibility.constFind(stateValue);
            }

            if (!foundAvalability.value()) {
                continue;
            }

            winrt::Windows::Storage::Provider::StorageProviderItemProperty itemProperty;
            itemProperty.Id(stateValue);
            itemProperty.Value(QStringLiteral("Value%1").arg(stateValue).toStdWString());
            itemProperty.IconResource(QString(_dllFilePath + QStringLiteral(",%1").arg(QString::number(stateValue))).toStdWString());
            properties.push_back(std::move(itemProperty));
        }
    }

    return winrt::single_threaded_vector(std::move(properties));
}
void CustomStateProvider::setDllFilePath(LPCTSTR dllFilePath)
{
    _dllFilePath = QString::fromWCharArray(dllFilePath);
    if (!_dllFilePath.endsWith(QStringLiteral(".dll"))) {
        _dllFilePath.clear();
    }
}

QString CustomStateProvider::_dllFilePath;
}
