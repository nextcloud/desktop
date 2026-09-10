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
 * @brief Replaces the access secret associated with one share recipient.
 *
 * The recipient is selected by its type, value, and optional instance. This
 * changes only that recipient's secret and applies the returned complete share
 * representation to the supplied Share object.
 */
class SetRecipientSecretJob : public UpdateShareJob
{
public:
    /**
     * @brief Creates a request to replace a recipient's secret.
     *
     * @param recipientTypeClass Registered server class describing the recipient type
     * @param recipientValue Identifier understood by the recipient type
     * @param secret New access secret for the recipient
     * @param instance Absolute URL of the recipient's remote instance, or no value for a local recipient
     */
    explicit SetRecipientSecretJob(AccountPtr account,
                                   Share &share,
                                   const QString &recipientTypeClass,
                                   const QString &recipientValue,
                                   const QString &secret,
                                   const std::optional<QString> &instance = std::nullopt);
};

}
