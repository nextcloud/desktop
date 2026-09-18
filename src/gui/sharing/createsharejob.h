/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

#include <QJsonDocument>

namespace OCC::Gui::Sharing
{

/**
 * @brief Creates a new server-side share container.
 *
 * This operation creates the share itself without selecting a source or
 * recipient. Those are attached by separate update jobs. The returned JSON is
 * The response is emitted as JSON for the controller to parse and own.
 */
class CreateShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

public:
    /** @brief Creates a request to create a share for account. */
    explicit CreateShareJob(AccountPtr account);

Q_SIGNALS:
    /** @brief Emitted with the newly created share representation after a successful request. */
    void shareCreated(const QJsonDocument &json);
};

}
