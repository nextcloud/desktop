/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/** @brief Removes a recipient from a share. */
class RemoveRecipientJob : public UpdateShareJob
{
public:
    explicit RemoveRecipientJob(AccountPtr account, Share &share, const QString &recipientType, const QString &recipientValue);
};

}
