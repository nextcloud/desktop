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
 * @brief Applies a server response to an existing share.
 */
class UpdateShareJob : public UnifiedSharingRequest
{
    Q_OBJECT

protected:
    explicit UpdateShareJob(AccountPtr account, Share &share, const QString &path, const QByteArray &verb, const QList<QPair<QString, QString>> &parameters);

Q_SIGNALS:
    void shareUpdated(QPointer<Share> share);
};

}
