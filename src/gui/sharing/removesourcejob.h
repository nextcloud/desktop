/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Removes a local filesystem node from an existing share.
 *
 * This stops the node identified by fileId from being content of the share. It
 * does not delete the node or the share itself. The returned representation is
 * applied to the supplied Share object.
 */
class RemoveSourceJob : public UpdateShareJob
{
public:
    /** @brief Creates a request to remove the node identified by fileId. */
    explicit RemoveSourceJob(AccountPtr account, Share &share, const QString &fileId);
};

}
