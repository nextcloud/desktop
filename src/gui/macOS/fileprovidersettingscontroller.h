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
    [[nodiscard]] Q_INVOKABLE bool isFileProviderEnabledForAccount(const QString &userIdAtHost) const;

public slots:
    void setFileProviderEnabledForAccount(const QString &userIdAtHost, const bool setEnabled, const bool showInformationDialog = true);

signals:
    void enabledAccountsChanged();

private:
    enum class FileProviderAction {
        NoAction,
        EnabledChanged,
    };

    explicit FileProviderSettingsController(QObject *parent = nullptr);

    [[nodiscard]] void *accountsInUserDefaultsEnabledForFileProviders() const;
    [[nodiscard]] FileProviderAction setFileProviderEnabledForAccountImpl(const QString &userIdAtHost, const bool setEnabled);
    void setupPersistentMainAppService();

    void *_userDefaults; // NSUserDefaults* - stored as void* to avoid Objective-C in header
    void *_accountsKey;  // NSString* - stored as void* to avoid Objective-C in header
};

} // Mac

} // OCC
