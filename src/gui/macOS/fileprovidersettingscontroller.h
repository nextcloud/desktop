/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QAbstractListModel;

namespace OCC {

class UserInfo;

namespace Mac {

/**
 * @brief Dedicated type to manage account configuration related to macOS file provider domains.
 *
 * The File Provider integration is an app-level mode: as soon as any file provider domain
 * exists, macOS activates the File Provider extension, which prevents the FinderSync
 * extension (used by classic sync folders) from running. The mode is therefore a single
 * switch for the whole client; when it is on, every configured account gets a domain.
 */
class FileProviderSettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isOperationInProgress READ isOperationInProgress NOTIFY operationInProgressChanged)
    Q_PROPERTY(QString operationMessage READ operationMessage NOTIFY operationMessageChanged)

public:
    static FileProviderSettingsController *instance();

    /// App-level File Provider mode (single switch for all accounts).
    [[nodiscard]] bool fileProviderModeEnabled() const;

    [[nodiscard]] QStringList vfsEnabledAccounts() const;
    [[nodiscard]] bool vfsEnabledForAccount(const QString &userIdAtHost) const;
    [[nodiscard]] bool isOperationInProgress() const;
    [[nodiscard]] QString operationMessage() const;

    /**
     * @brief Resolves conflicts between the app-level File Provider mode and configured
     * classic sync folders by prompting the user; without a conflict, reconciles the
     * file provider domains with the mode (creating or removing domains as needed).
     * Invoked deferred at startup and from the account settings conflict banner.
     */
    void performStartupReconciliation();

signals:
    void operationInProgressChanged();
    void operationMessageChanged();
    void vfsEnabledForAccountChanged(const QString &userIdAtHost);
    void fileProviderModeEnabledChanged(bool enabled);
    void fileProviderModeApplyFinished(bool enabled, const QStringList &failedAccounts);

public slots:
    /**
     * @brief Switches the app-level File Provider mode. The caller is responsible for
     * having obtained explicit user confirmation. Enabling discards all classic sync
     * folder configurations (files stay on disk) once every account's domain has been
     * created successfully; disabling removes all domains without recreating any
     * classic sync folders.
     */
    void setFileProviderModeEnabled(const bool enabled);

    /**
     * @brief Removes this account's File Provider domain (preserving unsynchronized
     * local data, exactly like the disable path) and immediately re-creates a fresh
     * domain with a clean state. Meant as a recovery action for a corrupted local
     * state. No-op when the app-level File Provider mode is off.
     */
    void resetVfsForAccount(const QString &userIdAtHost);

private:
    explicit FileProviderSettingsController(QObject *parent = nullptr);

    void setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled);
    void applyFileProviderModeToAllAccounts(const bool enabled);
    void removeAllClassicSyncFolders();
    void showReconciliationDialog();

    [[nodiscard]] QString fileProviderDomainIdentifierForAccount(const QString &userIdAtHost) const;
    void setOperationInProgress(bool inProgress, const QString &message = QString());

    class MacImplementation;
    friend class MacImplementation;
    MacImplementation *d;

    QHash<QString, UserInfo*> _userInfos;
    bool _isOperationInProgress = false;
    bool _reconciliationDialogShowing = false;
    QString _operationMessage;
};

} // Mac

} // OCC
