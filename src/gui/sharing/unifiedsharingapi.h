/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "accountfwd.h"

namespace OCC::Gui::Sharing
{

class UnifiedSharingRequest;

/**
 * @brief Creates one-shot requests for the Unified Sharing API.
 */
class UnifiedSharingApi : public QObject
{
    Q_OBJECT

public:
    explicit UnifiedSharingApi(AccountPtr account, QObject *parent = nullptr);

    [[nodiscard]] UnifiedSharingRequest *createShare();
    [[nodiscard]] UnifiedSharingRequest *destroyShare(const QString &shareId);
    [[nodiscard]] UnifiedSharingRequest *addSource(const QString &shareId, const QString &fileId);
    [[nodiscard]] UnifiedSharingRequest *addRecipient(const QString &shareId, const QString &recipientType, const QString &recipientValue);
    [[nodiscard]] UnifiedSharingRequest *removeRecipient(const QString &shareId, const QString &recipientType, const QString &recipientValue);
    [[nodiscard]] UnifiedSharingRequest *searchRecipients(const QString &query, qint64 offset, qint64 limit);
    [[nodiscard]] UnifiedSharingRequest *setPermission(const QString &shareId, const QString &permissionClass, bool enabled);
    [[nodiscard]] UnifiedSharingRequest *setPermissionPreset(const QString &shareId, const QString &permissionPreset);

private:
    AccountPtr _account;
};

}
