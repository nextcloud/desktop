/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

#include <optional>

namespace OCC::Gui::Sharing
{

/**
 * @brief Revokes one recipient's access to an existing share.
 *
 * The recipient is selected by the same type, value, and optional instance
 * tuple used when it was added. Other recipients remain attached to the share.
 * The returned representation is applied to the supplied Share object.
 */
class RemoveRecipientJob : public UpdateShareJob
{
public:
    /**
     * @brief Creates a request to remove a recipient from share.
     *
     * @param recipientTypeClass Registered server class describing the recipient type
     * @param recipientValue Identifier understood by the recipient type
     * @param instance Absolute URL of the recipient's remote instance, or no value for a local recipient
     */
    explicit RemoveRecipientJob(AccountPtr account,
                                Share &share,
                                const QString &recipientTypeClass,
                                const QString &recipientValue,
                                const std::optional<QString> &instance = std::nullopt);
};

}
