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

#pragma once

#include <QObject>
#include <QtQuickWidgets/QtQuickWidgets>

#include "gui/macOS/fileproviderdomainsyncstatus.h"

class QAbstractListModel;

namespace OCC {

class UserInfo;

namespace Mac {

class FileProviderSettingsController : public QObject
{
    Q_OBJECT

public:
    static FileProviderSettingsController *instance();

    [[nodiscard]] QQuickWidget *settingsViewWidget(const QString &accountUserIdAtHost,
                                                   QWidget *const parent = nullptr,
                                                   const QQuickWidget::ResizeMode resizeMode = QQuickWidget::SizeRootObjectToView);

    [[nodiscard]] QStringList vfsEnabledAccounts() const;
    [[nodiscard]] Q_INVOKABLE bool vfsEnabledForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] unsigned long long localStorageUsageForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] Q_INVOKABLE float localStorageUsageGbForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] unsigned long long remoteStorageUsageForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] Q_INVOKABLE float remoteStorageUsageGbForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] Q_INVOKABLE bool fastEnumerationEnabledForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] Q_INVOKABLE bool fastEnumerationSetForAccount(const QString &userIdAtHost) const;

    [[nodiscard]] Q_INVOKABLE QAbstractListModel *materialisedItemsModelForAccount(const QString &userIdAtHost);
    [[nodiscard]] Q_INVOKABLE FileProviderDomainSyncStatus *domainSyncStatusForAccount(const QString &userIdAtHost) const;

public slots:
    void setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled);
    void setFastEnumerationEnabledForAccount(const QString &userIdAtHost, const bool setEnabled);

    void createEvictionWindowForAccount(const QString &userIdAtHost);
    void refreshMaterialisedItemsForAccount(const QString &userIdAtHost);
    void signalFileProviderDomain(const QString &userIdAtHost);
    void createDebugArchive(const QString &userIdAtHost);

signals:
    void vfsEnabledAccountsChanged();
    void localStorageUsageForAccountChanged(const QString &userIdAtHost);
    void remoteStorageUsageForAccountChanged(const QString &userIdAtHost);
    void materialisedItemsForAccountChanged(const QString &userIdAtHost);
    void fastEnumerationEnabledForAccountChanged(const QString &userIdAtHost);
    void fastEnumerationSetForAccountChanged(const QString &userIdAtHost);

private:
    explicit FileProviderSettingsController(QObject *parent = nullptr);

    class MacImplementation;
    MacImplementation *d;

    QHash<QString, UserInfo*> _userInfos;
};

} // Mac

} // OCC
