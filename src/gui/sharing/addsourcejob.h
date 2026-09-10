/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Adds a local filesystem node as content of an existing share.
 *
 * In Unified Sharing, a source is an object being shared. This job registers
 * the node identified by fileId as a node source and applies the server's
 * complete updated representation to the supplied Share object.
 */
class AddSourceJob : public UpdateShareJob
{
public:
    /** @brief Creates a request to add the node identified by fileId to share. */
    explicit AddSourceJob(AccountPtr account, Share &share, const QString &fileId);
};

}
