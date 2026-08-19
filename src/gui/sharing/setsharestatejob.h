/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedshare.h"
#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Changes a share's lifecycle state.
 *
 * Unified shares can be draft, active, or deleted. This operation changes that
 * state and applies the server's complete updated representation to the
 * supplied Share object. It does not permanently remove the share record;
 * DestroyShareJob performs that operation.
 */
class SetShareStateJob : public UpdateShareJob
{
public:
    /**
     * @brief Creates a request to change the state of share.
     *
     * @param state New lifecycle state
     */
    explicit SetShareStateJob(AccountPtr account, Share &share, Share::State state);
};

}
