/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsharingrequest.h"

#include <QJsonArray>

namespace OCC::Gui::Sharing
{

class UnifiedSharingApi;

/**
 * @brief Searches for recipients and returns the result entries.
 */
class SearchRecipientsJob : public UnifiedSharingRequest
{
    Q_OBJECT

Q_SIGNALS:
    void recipientsFound(const QJsonArray &recipients);

private:
    friend class UnifiedSharingApi;
    explicit SearchRecipientsJob(AccountPtr account, const QString &query, qint64 offset, qint64 limit);
};

}
