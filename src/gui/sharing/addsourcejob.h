/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/** @brief Adds a node source to a share. */
class AddSourceJob : public UpdateShareJob
{
public:
    explicit AddSourceJob(AccountPtr account, Share &share, const QString &fileId);
};

}
