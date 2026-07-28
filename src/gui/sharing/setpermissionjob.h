/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/** @brief Enables or disables one permission on a share. */
class SetPermissionJob : public UpdateShareJob
{
public:
    explicit SetPermissionJob(AccountPtr account, Share &share, const QString &permissionClass, bool enabled);
};

}
