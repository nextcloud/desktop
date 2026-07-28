/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/** @brief Adds a recipient to a share. */
class AddRecipientJob : public UpdateShareJob
{
public:
    explicit AddRecipientJob(AccountPtr account, Share &share, const QString &recipientType, const QString &recipientValue);
};

}
