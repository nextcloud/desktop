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

    [[nodiscard]] QQuickWidget *settingsViewWidget(const QString &accountUserIdAtHost,
                                                   QWidget *const parent = nullptr,
                                                   const QQuickWidget::ResizeMode resizeMode = QQuickWidget::SizeRootObjectToView);

    [[nodiscard]] QStringList vfsEnabledAccounts() const;
    [[nodiscard]] Q_INVOKABLE bool vfsEnabledForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] Q_INVOKABLE bool trashDeletionEnabledForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] Q_INVOKABLE bool trashDeletionSetForAccount(const QString &userIdAtHost) const;

public slots:
    void setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled, const bool showInformationDialog = true);
    void setTrashDeletionEnabledForAccount(const QString &userIdAtHost, const bool setEnabled);

signals:
    void vfsEnabledAccountsChanged();
    void trashDeletionEnabledForAccountChanged(const QString &userIdAtHost);
    void trashDeletionSetForAccountChanged(const QString &userIdAtHost);

private:
    explicit FileProviderSettingsController(QObject *parent = nullptr);

    class MacImplementation;
    MacImplementation *d;

    QHash<QString, UserInfo*> _userInfos;
};

} // Mac

} // OCC
