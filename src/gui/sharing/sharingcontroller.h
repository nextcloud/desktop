/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
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
    Q_PROPERTY(Share *share READ share NOTIFY shareChanged)

public:
    SharingController(QObject *parent = nullptr);

    [[nodiscard]] AccountPtr account() const;
    void setAccount(AccountPtr account);

    /** @brief Returns the share currently being created or edited. */
    [[nodiscard]] Share *share() const;

    /**
     * @brief Loads all shares associated with a file without creating a share.
     *
     * @param fileId Server file ID used to filter the shares request
     */
    Q_INVOKABLE void initialize(const QString &fileId);
    Q_INVOKABLE void createShare(const QString &fileId);
    Q_INVOKABLE void destroyShare();
    Q_INVOKABLE void addRecipient(const QString &recipientType, const QString &recipientValue);
    Q_INVOKABLE void removeRecipient(const QString &recipientType, const QString &recipientValue);

    Q_INVOKABLE void setPermission(const QString &permissionClass, bool enabled);
    Q_INVOKABLE void setPermissionPreset(const QString &permissionPreset);

Q_SIGNALS:
    void accountChanged();
    void shareChanged();
    void sharesChanged();

private:
    AccountPtr _account;
    QPointer<Share> _share;
    QList<QPointer<Share>> _shares;

    void addSourceAfterCreation(const QString &fileId);
    void handleSharesFetched(const QList<QPointer<Share>> &shares);
};

}
