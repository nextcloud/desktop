/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QtQuickWidgets/QtQuickWidgets>

class QAbstractListModel;

namespace OCC {

class UserInfo;

namespace Mac {

class FileProviderSettingsController : public QObject
{
    Q_OBJECT

public:
    static FileProviderSettingsController *instance();

    [[nodiscard]] QQuickWidget *settingsViewWidget(const QString &accountUserIdAtHost, QWidget *const parent = nullptr, const QQuickWidget::ResizeMode resizeMode = QQuickWidget::SizeRootObjectToView);
    [[nodiscard]] Q_INVOKABLE bool vfsEnabledForAccount(const QString &userIdAtHost) const;

public slots:
    void setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled, const bool showInformationDialog = true);

signals:
    void vfsEnabledAccountsChanged();

private:
    enum class VfsAccountsAction {
        VfsAccountsNoAction,
        VfsAccountsEnabledChanged,
    };

    explicit FileProviderSettingsController(QObject *parent = nullptr);

    [[nodiscard]] void *nsEnabledAccounts() const;
    [[nodiscard]] VfsAccountsAction setVfsEnabledForAccountImpl(const QString &userIdAtHost, const bool setEnabled);
    void setupPersistentMainAppService();

    QHash<QString, UserInfo*> _userInfos;
    void *_userDefaults; // NSUserDefaults* - stored as void* to avoid Objective-C in header
    void *_accountsKey;  // NSString* - stored as void* to avoid Objective-C in header
    QHash<QString, unsigned long long> _storageUsage;
    QVector<void *> _serviceConnections; // QVector<NSXPCConnection*> - stored as void* to avoid Objective-C in header
};

} // Mac

} // OCC
