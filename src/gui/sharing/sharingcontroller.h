/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <qqmlintegration.h>
#include <QPointer>

#include "accountfwd.h"

namespace OCC::Gui::Sharing {

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

    [[nodiscard]] Share *share() const;

    Q_INVOKABLE void createShare(const QString &fileId);
    Q_INVOKABLE void destroyShare();
    Q_INVOKABLE void addRecipient(const QString &recipientType, const QString &recipientValue);
    Q_INVOKABLE void removeRecipient(const QString &recipientType, const QString &recipientValue);

    Q_INVOKABLE void setPermission(const QString &permissionClass, bool enabled);

Q_SIGNALS:
    void accountChanged();
    void shareChanged();

private:
    AccountPtr _account;
    QPointer<Share> _share;

    void addSourceAfterCreation(const QString &fileId);
};

}
