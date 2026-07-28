/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QPointer>

#include "accountfwd.h"

namespace OCC::Gui::Sharing
{

class CreateShareJob;
class DestroyShareJob;
class SearchRecipientsJob;
class Share;
class UpdateShareJob;

/**
 * @brief Creates typed jobs for the Unified Sharing API.
 */
class UnifiedSharingApi : public QObject
{
    Q_OBJECT

public:
    explicit UnifiedSharingApi(AccountPtr account, QObject *parent = nullptr);

    [[nodiscard]] CreateShareJob *createShare() const;
    [[nodiscard]] DestroyShareJob *destroyShare(const QString &shareId) const;
    [[nodiscard]] UpdateShareJob *addSource(QPointer<Share> share, const QString &fileId) const;
    [[nodiscard]] UpdateShareJob *addRecipient(QPointer<Share> share, const QString &recipientType, const QString &recipientValue) const;
    [[nodiscard]] UpdateShareJob *removeRecipient(QPointer<Share> share, const QString &recipientType, const QString &recipientValue) const;
    [[nodiscard]] SearchRecipientsJob *searchRecipients(const QString &query, qint64 offset, qint64 limit) const;
    [[nodiscard]] UpdateShareJob *setPermission(QPointer<Share> share, const QString &permissionClass, bool enabled) const;
    [[nodiscard]] UpdateShareJob *setPermissionPreset(QPointer<Share> share, const QString &permissionPreset) const;

private:
    AccountPtr _account;
};

}
