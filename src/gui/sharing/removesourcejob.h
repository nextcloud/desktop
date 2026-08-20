/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Removes the item identified by @p fileId from an existing share.
 *
 * This removes the item's association with the share; it does not delete the
 * item or the share itself.
 */
class RemoveSourceJob : public UpdateShareJob
{
public:
    /** @brief Creates a request to remove the item identified by @p fileId from the share. */
    explicit RemoveSourceJob(AccountPtr account, Share &share, const QString &fileId);
};

}
