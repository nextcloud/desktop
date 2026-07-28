/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

#include <QJsonObject>
#include <QPointer>

#include <optional>

namespace OCC::Gui::Sharing
{

class Share;

/**
 * @brief Fetches the complete representation of one share.
 *
 * The result includes the share's lifecycle state, sources, recipients,
 * properties, permissions, and selected permission preset. A secret and
 * recipient-type-specific access arguments can be supplied when required to
 * access the share.
 */
class GetShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

public:
    /**
     * @brief Creates a request to fetch one share.
     *
     * A missing secret and missing arguments are omitted from the request body.
     *
     * @param shareId Server ID of the share to fetch
     * @param secret Secret used to access the share, or no value when none is required
     * @param arguments Recipient- or property-type-specific access arguments, or no value
     */
    explicit GetShareJob(AccountPtr account,
                         const QString &shareId,
                         const std::optional<QString> &secret = std::nullopt,
                         const std::optional<QJsonObject> &arguments = std::nullopt);

Q_SIGNALS:
    /** @brief Emitted with the fetched share after a successful request. */
    void shareFetched(QPointer<Share> share);
};

}
