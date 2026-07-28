/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QObject>
#include <qqmlintegration.h>

#include "accountfwd.h"

namespace OCC::Gui::Sharing
{

class Share;

class SharingController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AccountPtr account READ account WRITE setAccount NOTIFY accountChanged)
    Q_PROPERTY(QList<Share *> shares READ shares NOTIFY sharesChanged)

public:
    SharingController(QObject *parent = nullptr);
    ~SharingController() override;

    [[nodiscard]] AccountPtr account() const;
    void setAccount(AccountPtr account);

    /** @brief Returns all shares associated with the initialized file. */
    [[nodiscard]] const QList<Share *> &shares() const;

    /**
     * @brief Loads all shares associated with a file without creating a share.
     *
     * @param fileId Server file ID used to filter the shares request
    */
    Q_INVOKABLE void initialize(const QString &fileId);
    Q_INVOKABLE void createShare(const QString &fileId);
    Q_INVOKABLE void destroyShare(Share *share);
    Q_INVOKABLE void addRecipient(Share *share, const QString &recipientType, const QString &recipientValue);
    Q_INVOKABLE void removeRecipient(Share *share, const QString &recipientType, const QString &recipientValue);

    Q_INVOKABLE void setPermission(Share *share, const QString &permissionClass, bool enabled);
    Q_INVOKABLE void setPermissionPreset(Share *share, const QString &permissionPreset);

Q_SIGNALS:
    void accountChanged();
    void sharesChanged();

private:
    AccountPtr _account;
    QList<Share *> _shares;

    [[nodiscard]] bool containsShare(const Share *share) const;
    void addSourceAfterCreation(Share &share, const QString &fileId);
    void replaceShares(const QList<Share *> &shares);
};

}
