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
#include <Shlguid.h>
#include <string>
#include <QString>
#include <QVector>
#include <QRandomGenerator>

namespace winrt::CfApiShellExtensions::implementation {

winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Storage::Provider::StorageProviderItemProperty>
CustomStateProvider::GetItemProperties(hstring const &itemPath)
{
    std::hash<std::wstring> hashFunc;
    const auto hash = hashFunc(itemPath.c_str());

    std::vector<winrt::Windows::Storage::Provider::StorageProviderItemProperty> properties;

    const auto itemPathString = winrt::to_string(itemPath);
    if (itemPathString.find(std::string(".sync_")) != std::string::npos
        || itemPathString.find(std::string(".owncloudsync.log")) != std::string::npos) {
        return winrt::single_threaded_vector(std::move(properties));
    }

    std::string iconResourceLog;

    const QVector<QPair<qint32, qint32>> listStates = {
        { 1, 77 },
        {2, -14},
        {3, 76}
    };

    int randomStateIndex = QRandomGenerator::global()->bounded(0, 3);

    if ((hash & 0x1) != 0) {
        winrt::Windows::Storage::Provider::StorageProviderItemProperty itemProperty;
        itemProperty.Id(listStates.at(randomStateIndex).first);
        itemProperty.Value(QString("Value%1").arg(listStates.at(randomStateIndex).first).toStdWString().c_str());
        // This icon is just for the sample. You should provide your own branded icon here
        itemProperty.IconResource(QString("shell32.dll,%1").arg(listStates.at(randomStateIndex).second).toStdWString().c_str());
        iconResourceLog = winrt::to_string(itemProperty.IconResource());
        properties.push_back(std::move(itemProperty));
    }

    return winrt::single_threaded_vector(std::move(properties));
}
}
