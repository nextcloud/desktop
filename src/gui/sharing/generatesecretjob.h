/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Requests a new opaque sharing secret from the server.
 *
 * The operation only generates and returns a secret. It does not attach the
 * secret to a recipient; SetRecipientSecretJob performs that update.
 */
class GenerateSecretJob : public UnifiedSharingRequest
{
    Q_OBJECT

public:
    /** @brief Creates a request to generate one secret. */
    explicit GenerateSecretJob(AccountPtr account);

Q_SIGNALS:
    /** @brief Emitted with the generated secret after a successful request. */
    void secretGenerated(const QString &secret);
};

}
