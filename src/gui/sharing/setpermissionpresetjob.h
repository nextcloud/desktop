/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Selects a server-defined permission preset for a share.
 *
 * A preset represents a named server configuration of the share's individual
 * permissions. The server applies the preset and returns the complete updated
 * share.
 */
class SetPermissionPresetJob : public UpdateShareJob
{
public:
    /**
     * @brief Creates a request to select a permission preset for share.
     *
     * @param permissionPreset Registered permission preset class to select
     */
    explicit SetPermissionPresetJob(AccountPtr account, Share &share, const QString &permissionPreset);
};

}
