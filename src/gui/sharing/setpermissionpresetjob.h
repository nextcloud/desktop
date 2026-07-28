/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/** @brief Applies a permission preset to a share. */
class SetPermissionPresetJob : public UpdateShareJob
{
public:
    explicit SetPermissionPresetJob(AccountPtr account, Share &share, const QString &permissionPreset);
};

}
