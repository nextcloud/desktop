/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Enables or disables one capability for a share's recipients.
 *
 * The permission class identifies a server-registered capability compatible
 * with the share's sources, such as viewing or editing node content. The
 * server returns the complete updated share.
 */
class SetPermissionJob : public UpdateShareJob
{
public:
    /**
     * @brief Creates a request to change one permission on share.
     *
     * @param permissionClass Registered permission type class to update
     * @param enabled Whether recipients should receive that capability
     */
    explicit SetPermissionJob(AccountPtr account, Share &share, const QString &permissionClass, bool enabled);
};

}
