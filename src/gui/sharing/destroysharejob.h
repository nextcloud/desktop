/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Destroys a share.
 */
class DestroyShareJob : public UnifiedSharingRequest
{
public:
    explicit DestroyShareJob(AccountPtr account, const QString &shareId);
};

}
