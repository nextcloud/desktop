/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

class Share;

/**
 * @brief Creates a new server-side share container.
 *
 * This operation creates the share itself without selecting a source or
 * recipient. Those are attached by separate update jobs. The returned JSON is
 * parsed into a new Share object.
 */
class CreateShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

public:
    /** @brief Creates a request to create a share for account. */
    explicit CreateShareJob(AccountPtr account);

Q_SIGNALS:
    /** @brief Emitted with the newly created share after a successful request. */
    void shareCreated(QPointer<Share> share);
};

}
