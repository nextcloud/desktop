/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

namespace OCC::Gui::Sharing
{

class Share;

/**
 * @brief Base for operations that mutate one existing share.
 *
 * Successful Unified Sharing mutation endpoints return the complete updated
 * share. This base applies that response to the same Share object supplied to
 * the concrete job and then emits shareUpdated. It does not own the Share and
 * safely handles the object being deleted while the request is running.
 */
class UpdateShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

protected:
    explicit UpdateShareJob(AccountPtr account,
                            Share &share,
                            const QString &path,
                            const QByteArray &verb,
                            const UnifiedSharingRequestOptions &options = {});

Q_SIGNALS:
    /** @brief Emitted after a successful response, or with null if the Share was deleted while the request was running. */
    void shareUpdated(QPointer<Share> share);
};

}
