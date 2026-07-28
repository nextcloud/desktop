/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Permanently removes a share from the server.
 *
 * This deletes the share identified by shareId rather than changing its
 * lifecycle state or removing only one source or recipient.
 */
class DestroyShareJob : public UnifiedSharingRequest
{
public:
    /** @brief Creates a request to delete the share identified by shareId. */
    explicit DestroyShareJob(AccountPtr account, const QString &shareId);
};

}
