/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "fileprovidersettingscontroller.h"

#include <QQmlApplicationEngine>

#include "gui/systray.h"

// Objective-C imports
#import <Foundation/Foundation.h>
// End of Objective-C imports

namespace {
constexpr auto fpSettingsQmlPath = "qrc:/qml/src/gui/macOS/ui/FileProviderSettings.qml";

// FileProviderSettingsPage properties -- make sure they match up in QML file!
constexpr auto fpSettingsAccountUserIdAtHostProp = "accountUserIdAtHost";

// NSUserDefaults entries
constexpr auto enabledAccountsSettingsKey = "enabledAccounts";
} // namespace

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSettingsController, "nextcloud.gui.mac.fileprovider.settingscontroller")

class FileProviderSettingsController::MacImplementation
{
public:
    enum class VfsAccountsAction {
        VfsAccountsNoAction,
        VfsAccountsEnabledChanged,
    };

    MacImplementation(FileProviderSettingsController *const parent)
    {
        q = parent;
        _userDefaults = NSUserDefaults.standardUserDefaults;
    };

    ~MacImplementation() = default;

    [[nodiscard]] QStringList enabledAccounts() const
    {
        QStringList qEnabledAccounts;
        NSArray<NSString *> *const enabledAccounts = nsEnabledAccounts();
        for (NSString *const userIdAtHostString in enabledAccounts) {
            qEnabledAccounts.append(QString::fromNSString(userIdAtHostString));
        }
        return qEnabledAccounts;
    }

    [[nodiscard]] bool vfsEnabledForAccount(const QString &userIdAtHost) const
    {
        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();
        return [vfsEnabledAccounts containsObject:userIdAtHost.toNSString()];
    }

    [[nodiscard]] VfsAccountsAction setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled) const
    {
        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();
        NSString *const nsUserIdAtHost = userIdAtHost.toNSString();
        const BOOL accountEnabled = [vfsEnabledAccounts containsObject:nsUserIdAtHost];

        if (accountEnabled == setEnabled) {
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        NSMutableArray<NSString *> *const mutableVfsAccounts = vfsEnabledAccounts.mutableCopy;

        if (setEnabled) {
            [mutableVfsAccounts addObject:nsUserIdAtHost];
        } else {
            [mutableVfsAccounts removeObject:nsUserIdAtHost];
        }

        NSArray<NSString *> *const modifiedVfsAccounts = mutableVfsAccounts.copy;
        NSString *const accsKey = [NSString stringWithUTF8String:enabledAccountsSettingsKey];
        [_userDefaults setObject:modifiedVfsAccounts forKey:accsKey];
        [_userDefaults synchronize];

        qDebug() << userIdAtHost << setEnabled << enabledAccounts();

        return VfsAccountsAction::VfsAccountsEnabledChanged;
    }

    [[nodiscard]] VfsAccountsAction enableVfsForAllAccounts() const
    {
        const auto accManager = AccountManager::instance();
        const auto accountsList = accManager->accounts();

        if (accountsList.count() == 0) {
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        auto overallActResult = VfsAccountsAction::VfsAccountsNoAction;

        for (const auto &account : accountsList) {
            const auto qAccountUserIdAtHost = account->account()->userIdAtHostWithPort();
            const auto accountActResult = setVfsEnabledForAccount(qAccountUserIdAtHost, true);

            if (accountActResult == VfsAccountsAction::VfsAccountsEnabledChanged) {
                overallActResult = accountActResult;
            }
        }

        return overallActResult;

    }

private:
    [[nodiscard]] NSArray<NSString *> *nsEnabledAccounts() const
    {
        NSString *const accsKey = [NSString stringWithUTF8String:enabledAccountsSettingsKey];
        return (NSArray<NSString *> *)[_userDefaults objectForKey:accsKey];
    }

    FileProviderSettingsController *q = nullptr;
    NSUserDefaults *_userDefaults = nil;
};

FileProviderSettingsController *FileProviderSettingsController::instance()
{
    static FileProviderSettingsController controller;
    return &controller;
}

FileProviderSettingsController::~FileProviderSettingsController() = default;

FileProviderSettingsController::FileProviderSettingsController(QObject *parent)
    : QObject{parent}
{
    d = std::make_unique<FileProviderSettingsController::MacImplementation>(this);
}

QQuickWidget *FileProviderSettingsController::settingsViewWidget(const QString &accountUserIdAtHost,
                                                                 QWidget *const parent,
                                                                 const QQuickWidget::ResizeMode resizeMode)
{
    const auto settingsViewWidget = new QQuickWidget(Systray::instance()->trayEngine(), parent);
    settingsViewWidget->setResizeMode(resizeMode);
    settingsViewWidget->setSource(QUrl(fpSettingsQmlPath));
    settingsViewWidget->rootObject()->setProperty(fpSettingsAccountUserIdAtHostProp, accountUserIdAtHost);
    return settingsViewWidget;
}

bool FileProviderSettingsController::vfsEnabledForAccount(const QString &userIdAtHost) const
{
    return d->vfsEnabledForAccount(userIdAtHost);
}

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled)
{
    const auto enabledAccountsAction = d->setVfsEnabledForAccount(userIdAtHost, setEnabled);
    if (enabledAccountsAction == MacImplementation::VfsAccountsAction::VfsAccountsEnabledChanged) {
        emit vfsEnabledAccountsChanged();
    }
}

} // namespace Mac

} // namespace OCC
