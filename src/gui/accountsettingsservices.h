/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include <QList>
#include <functional>
namespace OCC
{
class Folder;
class FolderStatusModel;
struct AccountSettingsServices {
    // Ownership transfers to AccountSettings.
    FolderStatusModel *model = nullptr;
    std::function<QList<Folder *>()> folders;
    std::function<bool()> fileProviderEnabled;
    std::function<void(bool)> setUserInfoActive;
    std::function<void()> initializeEncryption;
};
}
