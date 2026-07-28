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
 * @brief Grants a recipient access to an existing share.
 *
 * A recipient is identified by its registered recipient type, value, and
 * optional remote instance. The server returns the complete updated share
 */
class AddRecipientJob : public UpdateShareJob
{
public:
    /**
     * @brief Creates a request to add a recipient to share.
     *
     * @param recipientTypeClass Registered server class describing the recipient type
     * @param recipientValue Identifier understood by the recipient type
     * @param instance Absolute URL of the recipient's remote instance, or no value for a local recipient
     */
    explicit AddRecipientJob(AccountPtr account,
                             Share &share,
                             const QString &recipientTypeClass,
                             const QString &recipientValue,
                             const std::optional<QString> &instance = std::nullopt);
};

}
