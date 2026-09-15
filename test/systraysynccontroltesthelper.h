/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#pragma once

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include "account.h"
#include "accountmanager.h"
#include "configfile.h"
#include "folder.h"
#include "folderman.h"
#include "systray.h"

#include "foldermantestutils.h"
#include "testhelper.h"

using namespace OCC;

/** @brief Provides the shared account and folder setup for systray sync-control tests. */
class SystraySyncControlTestHelper
{
public:
    /** @brief Initializes the test configuration and first account. */
    [[nodiscard]] bool initialize()
    {
        if (!_configDir.isValid() || !_firstFolderDir.isValid() || !_secondFolderDir.isValid()) {
            return false;
        }

        QStandardPaths::setTestModeEnabled(true);
        ConfigFile::setConfDir(_configDir.path());

        // User construction references the Systray singleton, so initialize the tray before adding accounts.
        const auto systray = Systray::instance();
        systray->create();

        _firstAccountState = addTestAccount(QStringLiteral("https://one.example.com"), QStringLiteral("alice"));
        return _firstAccountState != nullptr;
    }

    /** @brief Removes the folders and accounts created by the helper. */
    void cleanup()
    {
        const auto folderMan = FolderMan::instance();
        if (_firstFolder) {
            folderMan->removeFolder(_firstFolder);
        }
        if (_secondFolder) {
            folderMan->removeFolder(_secondFolder);
        }
        _firstFolder = nullptr;
        _secondFolder = nullptr;

        if (_firstAccountState) {
            AccountManager::instance()->removeAccountState(_firstAccountState);
        }
        if (_secondAccountState) {
            AccountManager::instance()->removeAccountState(_secondAccountState);
        }
        _firstAccountState = nullptr;
        _secondAccountState = nullptr;
    }

    /** @brief Adds one classic sync folder to each of two accounts. */
    [[nodiscard]] bool addClassicFolders()
    {
        _firstFolder = FolderMan::instance()->addFolder(_firstAccountState, folderDefinition(_firstFolderDir.path()));
        if (!_firstFolder) {
            return false;
        }

        _secondAccountState = addTestAccount(QStringLiteral("https://two.example.com"), QStringLiteral("bob"));
        if (!_secondAccountState) {
            return false;
        }

        _secondFolder = FolderMan::instance()->addFolder(_secondAccountState, folderDefinition(_secondFolderDir.path()));
        return _secondFolder != nullptr;
    }

    /** @brief Returns the first classic sync folder, or null before it is added. */
    [[nodiscard]] Folder *firstFolder() const
    {
        return _firstFolder;
    }

    /** @brief Returns the second classic sync folder, or null before it is added. */
    [[nodiscard]] Folder *secondFolder() const
    {
        return _secondFolder;
    }

private:
    /** @brief Adds an account configured with test credentials. */
    static AccountState *addTestAccount(const QString &url, const QString &user)
    {
        auto account = Account::create();
        account->setUrl(QUrl(url));
        account->setDavUser(user);
        account->setCredentials(new HttpCredentialsTest(user, QStringLiteral("secret")));
        return AccountManager::instance()->addAccount(account);
    }

    QTemporaryDir _configDir;
    QTemporaryDir _firstFolderDir;
    QTemporaryDir _secondFolderDir;
    FolderManTestHelper _folderManHelper;
    AccountState *_firstAccountState = nullptr;
    AccountState *_secondAccountState = nullptr;
    Folder *_firstFolder = nullptr;
    Folder *_secondFolder = nullptr;
};
