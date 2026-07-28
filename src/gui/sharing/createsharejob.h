/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

class Share;
class UnifiedSharingApi;

/**
 * @brief Creates a share and returns its parsed representation.
 */
class CreateShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

Q_SIGNALS:
    void shareCreated(QPointer<Share> share);

private:
    friend class UnifiedSharingApi;
    explicit CreateShareJob(AccountPtr account);
};

}
