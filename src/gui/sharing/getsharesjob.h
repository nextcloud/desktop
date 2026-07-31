/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

#include <QList>
#include <QPointer>

#include <optional>

namespace OCC::Gui::Sharing
{

class Share;

/**
 * @brief Fetches a page of shares accessible to the account.
 *
 * Results can be restricted to shares containing a particular source type and
 * source value. Pagination continues after lastShareId and returns at most
 * limit complete Share objects.
 */
class GetSharesJob : public UnifiedSharingRequest
{
    Q_OBJECT

public:
    /**
     * @brief Creates a request to fetch shares.
     *
     * Missing filters and a missing last share ID are omitted.
     *
     * @param sourceTypeClass Registered source type class to match, or no value for all types
     * @param sourceTypeValue Source identifier to match, or no value for all values
     * @param lastShareId ID after which the server continues the result set, or no value for the first page
     * @param limit Maximum number of shares to return
     */
    explicit GetSharesJob(AccountPtr account,
                          const std::optional<QString> &sourceTypeClass = std::nullopt,
                          const std::optional<QString> &sourceTypeValue = std::nullopt,
                          const std::optional<QString> &lastShareId = std::nullopt,
                          qint64 limit = 100);

Q_SIGNALS:
    /** @brief Emitted with the fetched shares after a successful request. */
    void sharesFetched(const QList<QPointer<Share>> &shares);
};

}
