/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

/**
 * @brief Base for operations that mutate one existing share.
 *
 * Successful Unified Sharing mutation endpoints return the complete updated
 * share. The controller applies the response to the current Share identified
 * by the request after the job completes.
 */
class UpdateShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

protected:
    explicit UpdateShareJob(AccountPtr account, const QString &path, const QByteArray &verb, const UnifiedSharingRequest::Options &options = {});

Q_SIGNALS:
    /** @brief Emitted with the complete updated share response after success. */
    void shareUpdated(const QJsonDocument &json);
};

}
