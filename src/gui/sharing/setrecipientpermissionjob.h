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
 * @brief Enables or disables one capability for one recipient of a share.
 */
class SetRecipientPermissionJob : public UpdateShareJob
{
public:
    /** @brief Creates a request for a recipient-specific permission change. */
    explicit SetRecipientPermissionJob(AccountPtr account,
                                       Share &share,
                                       const QString &recipientType,
                                       const QString &recipientValue,
                                       const std::optional<QString> &recipientInstance,
                                       const QString &permissionClass,
                                       bool enabled);
};

}
