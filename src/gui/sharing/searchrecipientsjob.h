/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

#include <QJsonArray>
#include <QList>

#include <optional>

namespace OCC::Gui::Sharing
{

/**
 * @brief Searches for recipients that can be added to a share.
 *
 * The server searches registered recipient types, such as users, groups, or
 * other sharing backends. The optional recipient type list restricts which
 * backends are searched. This job only returns candidates; it does not add a
 * recipient to a share.
 */
class SearchRecipientsJob : public UnifiedSharingRequest
{
    Q_OBJECT

public:
    /**
     * @brief Creates a paginated recipient search request.
     *
     * @param query Text used by the server's recipient search backends
     * @param offset Number of matching entries to skip
     * @param limit Maximum number of matching entries to return
     * @param recipientTypeClasses Registered recipient type classes to search, or empty for all
     * @param shareId Share whose existing recipients should be excluded, or no value to search without that filter
     */
    explicit SearchRecipientsJob(AccountPtr account,
                                 const QString &query,
                                 qint64 offset,
                                 qint64 limit,
                                 const QList<QString> &recipientTypeClasses = {},
                                 const std::optional<QString> &shareId = std::nullopt);

Q_SIGNALS:
    /** @brief Emitted with the matching recipient descriptions after a successful request. */
    void recipientsFound(const QJsonArray &recipients);
};

}
