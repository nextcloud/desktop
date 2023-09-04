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
constexpr auto fpSettingsControllerProp = "FileProviderSettingsController";

// NSUserDefaults entries
constexpr auto enabledAccountsSettingsKey = "enabledAccounts";
} // namespace

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSettingsController, "nextcloud.gui.mac.fileprovider.settingscontroller")

class FileProviderSettingsController::MacImplementation
{
public:
    MacImplementation(FileProviderSettingsController *const parent)
    {
        q = parent;
        _userDefaults = NSUserDefaults.standardUserDefaults;
    };

    ~MacImplementation()
    {
        [_userDefaults release];
    };

    QStringList enabledAccounts() const
    {
        QStringList qEnabledAccounts;
        NSArray<NSString *> *const enabledAccounts = nsEnabledAccounts();
        for (NSString *const userIdAtHostString in enabledAccounts) {
            qEnabledAccounts.append(QString::fromNSString(userIdAtHostString));
        }
        return qEnabledAccounts;
    }

    bool vfsEnabledForAccount(const QString &userIdAtHost) const
    {
        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();
        return [vfsEnabledAccounts containsObject:userIdAtHost.toNSString()];
    }

private:
    NSArray<NSString *> *nsEnabledAccounts() const
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

QQuickWidget *FileProviderSettingsController::settingsViewWidget(QWidget *const parent, const QQuickWidget::ResizeMode resizeMode)
{
    const auto settingsViewWidget = new QQuickWidget(Systray::instance()->trayEngine(), parent);
    settingsViewWidget->setResizeMode(resizeMode);
    settingsViewWidget->setSource(QUrl(fpSettingsQmlPath));
    settingsViewWidget->rootContext()->setContextProperty(fpSettingsControllerProp, this);
    return settingsViewWidget;
}

bool FileProviderSettingsController::vfsEnabledForAccount(const QString &userIdAtHost)
{
    return d->vfsEnabledForAccount(userIdAtHost);
}

} // namespace Mac

} // namespace OCC
